// VoiceAudioWin.cpp - PC capture and playback over waveIn / waveOut.
//
// See VoiceAudio.h for why winmm rather than WASAPI.
//
// WHY POLLING AND NOT CALLBACKS
//
// waveIn and waveOut can both call back on their own thread, and that is the
// obvious way to write this. It is also a documented trap: inside a waveOut
// callback you may call almost nothing, and the one function you actually want
// there - waveOutWrite - is not on the permitted list. Everybody calls it
// anyway and it usually works, which is the worst kind of bug to inherit.
//
// So the devices are opened with CALLBACK_NULL and both directions are pumped
// from the game's frame instead. At 50-60 FPS that is a poll every 16-20ms
// against 20ms buffers, with enough of them queued to ride out a slow frame.
// Nothing here can run at a moment when the rest of the client is not expecting
// it, which is worth more than the latency it costs.

#include "stdafx.h"

#if !defined(__ANDROID__) && !defined(MU_IOS)

#include "VoiceAudio.h"
#include "VoiceClient.h"
#include "./Utilities/Log/muConsoleDebug.h"

#include <stdarg.h>

#pragma comment(lib, "winmm.lib")

// 20ms of 8 kHz mono 16-bit.
#define VOICE_BUFFER_BYTES      (VOICE_FRAME_SAMPLES * sizeof(short))

// Eight buffers is 160ms of queued playback. That is the cushion against a
// slow frame: shorter and an ordinary hitch becomes an audible gap, longer and
// people hear each other noticeably behind their own characters.
#define VOICE_PLAY_BUFFERS      8

// Capture needs far less, because a missed poll there costs the speaker a few
// milliseconds of their own voice rather than interrupting everyone else's.
#define VOICE_CAP_BUFFERS       4

namespace
{
	HWAVEOUT   g_WaveOut = NULL;
	WAVEHDR    g_PlayHeader[VOICE_PLAY_BUFFERS];
	short      g_PlayBuffer[VOICE_PLAY_BUFFERS][VOICE_FRAME_SAMPLES];

	HWAVEIN    g_WaveIn = NULL;
	WAVEHDR    g_CapHeader[VOICE_CAP_BUFFERS];
	short      g_CapBuffer[VOICE_CAP_BUFFERS][VOICE_FRAME_SAMPLES];

	void MakeFormat(WAVEFORMATEX* pFormat)
	{
		memset(pFormat, 0, sizeof(WAVEFORMATEX));

		pFormat->wFormatTag      = WAVE_FORMAT_PCM;
		pFormat->nChannels       = 1;
		pFormat->nSamplesPerSec  = VOICE_SAMPLE_RATE;
		pFormat->wBitsPerSample  = 16;
		pFormat->nBlockAlign     = (WORD)(pFormat->nChannels * pFormat->wBitsPerSample / 8);
		pFormat->nAvgBytesPerSec = pFormat->nSamplesPerSec * pFormat->nBlockAlign;
		pFormat->cbSize          = 0;
	}
}

// -------------------------------------------------------------------------
// playback
// -------------------------------------------------------------------------

bool CVoiceAudio::OpenPlayback() // OK
{
	if (this->m_PlaybackOpen)
	{
		return true;
	}

	if (waveOutGetNumDevs() == 0)
	{
		this->SetError("no playback device");
		return false;
	}

	WAVEFORMATEX format;
	MakeFormat(&format);

	const MMRESULT result = waveOutOpen(&g_WaveOut, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);

	if (result != MMSYSERR_NOERROR)
	{
		this->SetError("waveOutOpen failed (%d)", (int)result);
		g_WaveOut = NULL;
		return false;
	}

	memset(g_PlayHeader, 0, sizeof(g_PlayHeader));
	memset(g_PlayBuffer, 0, sizeof(g_PlayBuffer));

	for (int n = 0; n < VOICE_PLAY_BUFFERS; n++)
	{
		g_PlayHeader[n].lpData         = (LPSTR)g_PlayBuffer[n];
		g_PlayHeader[n].dwBufferLength = VOICE_BUFFER_BYTES;

		waveOutPrepareHeader(g_WaveOut, &g_PlayHeader[n], sizeof(WAVEHDR));

		// Queued as silence, so the device is already running by the time
		// anybody speaks. Starting it on the first frame of real audio would
		// swallow the start of the first word.
		waveOutWrite(g_WaveOut, &g_PlayHeader[n], sizeof(WAVEHDR));
	}

	this->m_PlaybackOpen = true;

	g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] playback open");

	return true;
}

void CVoiceAudio::ClosePlayback() // OK
{
	if (g_WaveOut == NULL)
	{
		this->m_PlaybackOpen = false;
		return;
	}

	// Reset first: it marks every queued buffer done, without which
	// waveOutUnprepareHeader refuses and the device never closes.
	waveOutReset(g_WaveOut);

	for (int n = 0; n < VOICE_PLAY_BUFFERS; n++)
	{
		waveOutUnprepareHeader(g_WaveOut, &g_PlayHeader[n], sizeof(WAVEHDR));
	}

	waveOutClose(g_WaveOut);

	g_WaveOut = NULL;
	this->m_PlaybackOpen = false;

	g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] playback closed");
}

void CVoiceAudio::PumpPlayback() // OK
{
	if (g_WaveOut == NULL)
	{
		return;
	}

	for (int n = 0; n < VOICE_PLAY_BUFFERS; n++)
	{
		if ((g_PlayHeader[n].dwFlags & WHDR_DONE) == 0)
		{
			continue;
		}

		// PullPlayback always fills the whole buffer, silence included, so
		// there is no "is anyone talking" check to make here.
		gVoiceClient.PullPlayback(g_PlayBuffer[n], VOICE_FRAME_SAMPLES);

		g_PlayHeader[n].dwFlags &= ~WHDR_DONE;
		g_PlayHeader[n].dwBufferLength = VOICE_BUFFER_BYTES;

		waveOutWrite(g_WaveOut, &g_PlayHeader[n], sizeof(WAVEHDR));
	}
}

// -------------------------------------------------------------------------
// capture
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// device selection
// -------------------------------------------------------------------------

int VoiceAudioGetCaptureDeviceCount() // OK
{
	return (int)waveInGetNumDevs();
}

bool VoiceAudioGetCaptureDeviceName(int iIndex, char* szOut, int iMax) // OK
{
	if (szOut == NULL || iMax <= 0)
	{
		return false;
	}

	if (iIndex < 0 || iIndex >= (int)waveInGetNumDevs())
	{
		return false;
	}

	WAVEINCAPS caps;
	memset(&caps, 0, sizeof(caps));

	if (waveInGetDevCaps((UINT_PTR)iIndex, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
	{
		return false;
	}

	// szPname is 32 chars including the terminator and is NOT guaranteed to be
	// terminated when the name fills it, so the copy is bounded and terminated
	// here rather than trusted.
	strncpy(szOut, caps.szPname, (size_t)(iMax - 1));
	szOut[iMax - 1] = '\0';

	return true;
}

void VoiceAudioGetSelectedCaptureDeviceName(char* szOut, int iMax) // OK
{
	if (szOut == NULL || iMax <= 0)
	{
		return;
	}

	const int selected = VoiceSettingGetMicDevice();

	if (selected < 0)
	{
		strncpy(szOut, "System default", (size_t)(iMax - 1));
		szOut[iMax - 1] = '\0';
		return;
	}

	if (VoiceAudioGetCaptureDeviceName(selected, szOut, iMax))
	{
		return;
	}

	// Saved id that no longer exists - a headset unplugged since last session.
	// Said out loud rather than silently showing the default, because the
	// player chose that device and it is worth knowing it is gone.
	strncpy(szOut, "(missing - using default)", (size_t)(iMax - 1));
	szOut[iMax - 1] = '\0';
}

bool CVoiceAudio::OpenCapture() // OK
{
	if (this->m_CaptureOpen)
	{
		return true;
	}

	const UINT deviceCount = waveInGetNumDevs();

	if (deviceCount == 0)
	{
		this->SetError("no microphone");
		return false;
	}

	// A saved device id can outlive the device itself, so it is validated here
	// and quietly falls back rather than failing. WAVE_MAPPER means "let
	// Windows pick", which also follows the player's own OS default when they
	// change it.
	int selected = VoiceSettingGetMicDevice();

	if (selected < 0 || selected >= (int)deviceCount)
	{
		selected = -1;
	}

	const UINT_PTR deviceId = (selected < 0) ? (UINT_PTR)WAVE_MAPPER : (UINT_PTR)selected;

	WAVEFORMATEX format;
	MakeFormat(&format);

	const MMRESULT result = waveInOpen(&g_WaveIn, deviceId, &format, 0, 0, CALLBACK_NULL);

	if (result != MMSYSERR_NOERROR)
	{
		// The common one is MMSYSERR_ALLOCATED - something else already has
		// the microphone - which is worth telling the player rather than
		// leaving them talking into nothing.
		this->SetError("waveInOpen failed (%d)", (int)result);
		g_WaveIn = NULL;
		return false;
	}

	memset(g_CapHeader, 0, sizeof(g_CapHeader));
	memset(g_CapBuffer, 0, sizeof(g_CapBuffer));

	for (int n = 0; n < VOICE_CAP_BUFFERS; n++)
	{
		g_CapHeader[n].lpData         = (LPSTR)g_CapBuffer[n];
		g_CapHeader[n].dwBufferLength = VOICE_BUFFER_BYTES;

		waveInPrepareHeader(g_WaveIn, &g_CapHeader[n], sizeof(WAVEHDR));
		waveInAddBuffer(g_WaveIn, &g_CapHeader[n], sizeof(WAVEHDR));
	}

	waveInStart(g_WaveIn);

	this->m_CaptureOpen = true;

	g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] microphone open");

	return true;
}

void CVoiceAudio::CloseCapture() // OK
{
	if (g_WaveIn == NULL)
	{
		this->m_CaptureOpen = false;
		return;
	}

	waveInStop(g_WaveIn);
	waveInReset(g_WaveIn);

	for (int n = 0; n < VOICE_CAP_BUFFERS; n++)
	{
		waveInUnprepareHeader(g_WaveIn, &g_CapHeader[n], sizeof(WAVEHDR));
	}

	waveInClose(g_WaveIn);

	g_WaveIn = NULL;
	this->m_CaptureOpen = false;
	this->m_CaptureLevel = 0;

	g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] microphone closed");
}

void CVoiceAudio::PumpCapture() // OK
{
	if (g_WaveIn == NULL)
	{
		return;
	}

	for (int n = 0; n < VOICE_CAP_BUFFERS; n++)
	{
		if ((g_CapHeader[n].dwFlags & WHDR_DONE) == 0)
		{
			continue;
		}

		// Only a full frame is worth sending. A short one would have to be
		// padded, and padding a differential codec with silence puts a click
		// at the join.
		if (g_CapHeader[n].dwBytesRecorded >= VOICE_BUFFER_BYTES)
		{
			int peak = 0;

			for (int s = 0; s < VOICE_FRAME_SAMPLES; s++)
			{
				const int a = (g_CapBuffer[n][s] < 0) ? -g_CapBuffer[n][s] : g_CapBuffer[n][s];

				if (a > peak)
				{
					peak = a;
				}
			}

			this->m_CaptureLevel = peak >> 7;   // 0-32767 down to 0-255

			// The device stays open through the linger after the key is
			// released; these frames are captured but deliberately not sent.
			if (this->m_Talking)
			{
				gVoiceClient.SendCaptureFrame(g_CapBuffer[n]);
			}
		}

		g_CapHeader[n].dwFlags &= ~WHDR_DONE;
		g_CapHeader[n].dwBytesRecorded = 0;

		waveInAddBuffer(g_WaveIn, &g_CapHeader[n], sizeof(WAVEHDR));
	}
}

#endif  // PC only
