// VoiceAudioIos.cpp - iOS capture and playback for proximity voice.
//
// The AVAudioEngine work lives in ios/src/IosVoice.mm; this file only adapts
// it to CVoiceAudio, the same role VoiceAudioOpenSL.cpp plays on Android.
// Both callbacks below run on audio threads and go straight into the
// transport, whose mutex makes that safe - as on Android.
//
// Engine start and stop are asynchronous, so opening a device always reports
// success here. A device that then fails logs why to the console instead.

#include "stdafx.h"

#if defined(MU_IOS)

#include "VoiceAudio.h"
#include "VoiceClient.h"
#include "IosVoice.h"
#include "./Utilities/Log/muConsoleDebug.h"

#include <cmath>

namespace
{
	// Capture state last reported through SetError, so each change is said once.
	int g_ReportedCaptureState = IOS_VOICE_CAPTURE_IDLE;

	// The talk button's bar, 0-255. On a dB scale spanning 48 dB rather than
	// linear: voice-processed speech on an iPhone peaks around 1500-4500 of
	// 32767, which a linear bar shows as a sliver that barely moves.
	int MeterLevel(int iPeak)
	{
		if (iPeak <= 0)
		{
			return 0;
		}
		const double db = 20.0 * std::log10((double)iPeak / 32767.0);
		const double level = 255.0 * (db + 48.0) / 48.0;
		return (level < 0.0) ? 0 : ((level > 255.0) ? 255 : (int)level);
	}

	void PullVoicePlayback(short* pOut, int iCount)
	{
		gVoiceClient.PullPlayback(pOut, iCount);
	}

	void OnVoiceCaptureFrame(const short* pSamples, int /*iCount*/, int iPeak)
	{
		gVoiceAudio.NotifyCaptureLevel(MeterLevel(iPeak));

		static int s_frames = 0;
		if (++s_frames <= 3 || (s_frames % 100) == 0)
		{
			IosVoice_Log("game: capture frame #%d peak=%d talking=%d sent=%lu gateOpen=%d sentLevel=%d gain=%d%%",
				s_frames, iPeak, gVoiceAudio.IsTalking() ? 1 : 0, (unsigned long)gVoiceClient.GetFramesSent(),
				gVoiceClient.IsGateOpen() ? 1 : 0, gVoiceClient.GetSentLevel(), gVoiceClient.GetAutoGainPercent());
		}

		// The microphone stays open through the linger after the button is
		// released; those frames are captured but deliberately not sent.
		if (gVoiceAudio.IsTalking())
		{
			gVoiceClient.SendCaptureFrame(pSamples);
		}
	}
}

bool CVoiceAudio::OpenPlayback() // OK
{
	if (this->m_PlaybackOpen)
	{
		return true;
	}

	IosVoice_Log("game: OpenPlayback");
	IosVoice_OpenPlayback(PullVoicePlayback);
	this->m_PlaybackOpen = true;
	g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] playback open");
	return true;
}

void CVoiceAudio::ClosePlayback() // OK
{
	if (this->m_PlaybackOpen)
	{
		IosVoice_ClosePlayback();
		g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] playback closed");
	}
	this->m_PlaybackOpen = false;
}

bool CVoiceAudio::OpenCapture() // OK
{
	if (this->m_CaptureOpen)
	{
		return true;
	}

	const int permission = IosVoice_MicPermission();
	IosVoice_Log("game: OpenCapture permission=%d talking=%d", permission, this->m_Talking ? 1 : 0);
	if (permission == 0)
	{
		this->SetError("microphone denied - allow it in Settings");
		return false;
	}

	IosVoice_OpenCapture(OnVoiceCaptureFrame, VOICE_FRAME_SAMPLES, VOICE_SAMPLE_RATE);
	g_ReportedCaptureState = IOS_VOICE_CAPTURE_STARTING;
	this->m_CaptureOpen = true;
	g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] microphone open");
	return true;
}

void CVoiceAudio::CloseCapture() // OK
{
	if (this->m_CaptureOpen)
	{
		IosVoice_Log("game: CloseCapture");
		IosVoice_CloseCapture();
		g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] microphone closed");
	}
	this->m_CaptureOpen = false;
	this->m_CaptureLevel = 0;
}

void CVoiceAudio::PumpPlayback() // OK
{
	// The engine is pumped from Proc every frame (IosVoice_Pump), not only
	// while playback is open.
}

void CVoiceAudio::PumpCapture() // OK
{
	// The capture tap delivers frames itself (OnVoiceCaptureFrame). What is
	// left here is saying why nobody hears the player when iOS will not give
	// the app the microphone - otherwise that looks exactly like a bug.
	const int state = IosVoice_CaptureState();
	if (state == g_ReportedCaptureState)
	{
		return;
	}
	g_ReportedCaptureState = state;

	if (state == IOS_VOICE_CAPTURE_SILENT)
	{
		this->SetError("mic silent - another app (a call?) is using it");
	}
	else if (state == IOS_VOICE_CAPTURE_UNAVAILABLE)
	{
		this->SetError("iOS refused the mic - end any call and retry");
	}
	else if (state == IOS_VOICE_CAPTURE_LIVE)
	{
		this->m_LastError[0] = '\0';
	}
}

// AVAudioSession routes to whatever input is active (built-in, headset,
// Bluetooth) and offers no picker worth showing, so like Android this reports
// no devices and the options row hides itself.
int VoiceAudioGetCaptureDeviceCount() // OK
{
	return 0;
}

bool VoiceAudioGetCaptureDeviceName(int, char*, int) // OK
{
	return false;
}

void VoiceAudioGetSelectedCaptureDeviceName(char* szOut, int iMax) // OK
{
	if (szOut == NULL || iMax <= 0)
	{
		return;
	}

	strncpy(szOut, "System default", (size_t)(iMax - 1));
	szOut[iMax - 1] = '\0';
}

#endif
