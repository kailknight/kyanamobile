// VoiceAudioOpenSL.cpp - Android capture and playback over OpenSL ES.
//
// WHY NOT SDL, WHICH IS RIGHT THERE
//
// SDL2 is linked into this client and SDL_INIT_AUDIO appears in the source, so
// SDL audio looks available. It is not, for three reasons that all have to be
// true at once and all are:
//
//   - sokol_app is the entry point. SDL_main is compiled out entirely, inside
//     an #if 0, so the SDL_Init call in it has never run.
//   - The launched activity is PreloadActivity -> MuMainNativeActivity, not
//     the SDLActivity subclass. SDL.java's SDLAudioManager.nativeSetupJNI()
//     therefore never runs.
//   - SDL's Android audio driver is a shim over Java AudioTrack/AudioRecord
//     reached through exactly that class. Without it there is nothing to open.
//
// This is the same reason the game's own music goes through a Java MuAudio
// class over JNI rather than SDL_mixer - see the comment at the top of
// MuAudio.java, which records the first time this was discovered.
//
// OpenSL ES is part of the NDK, needs no Java bootstrap, and covers the whole
// minSdk 21 range. AAudio is a nicer API but starts at 26, which would have
// meant shipping two backends.
//
// THREADING
//
// OpenSL calls back on its own thread, like SDL would have. Both callbacks go
// straight into the transport, whose mutex is what makes that safe - the
// reason it was written to be callable from an audio thread rather than from
// the game loop.

#include "stdafx.h"

#if defined(__ANDROID__)

#include "VoiceAudio.h"
#include "VoiceClient.h"
#include "./Utilities/Log/muConsoleDebug.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <stdarg.h>

namespace
{
	// OpenSL's buffer queues hand back one buffer at a time and need another
	// enqueued immediately, so each direction keeps a small rotation. Two is
	// the minimum that works (one playing, one being filled); three gives a
	// little slack on a phone that is busy elsewhere.
	const int kQueueBuffers = 3;

	SLObjectItf g_Engine = NULL;
	SLEngineItf g_EngineItf = NULL;

	// playback
	SLObjectItf g_OutputMix = NULL;
	SLObjectItf g_Player = NULL;
	SLPlayItf   g_PlayItf = NULL;
	SLAndroidSimpleBufferQueueItf g_PlayQueue = NULL;

	short g_PlayBuffer[kQueueBuffers][VOICE_FRAME_SAMPLES];
	int   g_PlayIndex = 0;

	// capture
	SLObjectItf g_Recorder = NULL;
	SLRecordItf g_RecordItf = NULL;
	SLAndroidSimpleBufferQueueItf g_RecordQueue = NULL;

	short g_RecordBuffer[kQueueBuffers][VOICE_FRAME_SAMPLES];
	int   g_RecordIndex = 0;

	bool EnsureEngine()
	{
		if (g_EngineItf != NULL)
		{
			return true;
		}

		if (slCreateEngine(&g_Engine, 0, NULL, 0, NULL, NULL) != SL_RESULT_SUCCESS)
		{
			return false;
		}

		if ((*g_Engine)->Realize(g_Engine, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
		{
			(*g_Engine)->Destroy(g_Engine);
			g_Engine = NULL;
			return false;
		}

		if ((*g_Engine)->GetInterface(g_Engine, SL_IID_ENGINE, &g_EngineItf) != SL_RESULT_SUCCESS)
		{
			(*g_Engine)->Destroy(g_Engine);
			g_Engine = NULL;
			g_EngineItf = NULL;
			return false;
		}

		return true;
	}

	// The codec's format, spelled out once. OpenSL will not resample, so this
	// is what the device is asked for and what it must give back.
	SLDataFormat_PCM MakeFormat()
	{
		SLDataFormat_PCM format;

		format.formatType    = SL_DATAFORMAT_PCM;
		format.numChannels   = 1;
		format.samplesPerSec = SL_SAMPLINGRATE_8;     // 8000 * 1000, OpenSL's milli-Hz
		format.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
		format.containerSize = 16;
		format.channelMask   = SL_SPEAKER_FRONT_CENTER;
		format.endianness    = SL_BYTEORDER_LITTLEENDIAN;

		return format;
	}

	void PlaybackCallback(SLAndroidSimpleBufferQueueItf queue, void* /*pContext*/)
	{
		short* pBuffer = g_PlayBuffer[g_PlayIndex];

		g_PlayIndex = (g_PlayIndex + 1) % kQueueBuffers;

		// Always fills the whole buffer, silence included, so there is nothing
		// to decide here.
		gVoiceClient.PullPlayback(pBuffer, VOICE_FRAME_SAMPLES);

		(*queue)->Enqueue(queue, pBuffer, VOICE_FRAME_SAMPLES * sizeof(short));
	}

	void RecordCallback(SLAndroidSimpleBufferQueueItf queue, void* pContext)
	{
		CVoiceAudio* pAudio = (CVoiceAudio*)pContext;

		short* pBuffer = g_RecordBuffer[g_RecordIndex];

		g_RecordIndex = (g_RecordIndex + 1) % kQueueBuffers;

		int peak = 0;

		for (int n = 0; n < VOICE_FRAME_SAMPLES; n++)
		{
			const int a = (pBuffer[n] < 0) ? -pBuffer[n] : pBuffer[n];

			if (a > peak)
			{
				peak = a;
			}
		}

		pAudio->NotifyCaptureLevel(peak >> 7);   // 0-32767 down to 0-255

		// The device stays open through the linger after the key is released;
		// those frames are captured but deliberately not sent.
		if (pAudio->IsTalking())
		{
			gVoiceClient.SendCaptureFrame(pBuffer);
		}

		(*queue)->Enqueue(queue, pBuffer, VOICE_FRAME_SAMPLES * sizeof(short));
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

	if (EnsureEngine() == false)
	{
		this->SetError("OpenSL engine failed");
		return false;
	}

	if ((*g_EngineItf)->CreateOutputMix(g_EngineItf, &g_OutputMix, 0, NULL, NULL) != SL_RESULT_SUCCESS)
	{
		this->SetError("CreateOutputMix failed");
		return false;
	}

	if ((*g_OutputMix)->Realize(g_OutputMix, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
	{
		this->SetError("output mix realize failed");
		this->ClosePlayback();
		return false;
	}

	SLDataLocator_AndroidSimpleBufferQueue queueLoc =
		{ SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, (SLuint32)kQueueBuffers };

	SLDataFormat_PCM format = MakeFormat();
	SLDataSource source = { &queueLoc, &format };

	SLDataLocator_OutputMix mixLoc = { SL_DATALOCATOR_OUTPUTMIX, g_OutputMix };
	SLDataSink sink = { &mixLoc, NULL };

	const SLInterfaceID ids[1] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
	const SLboolean req[1] = { SL_BOOLEAN_TRUE };

	if ((*g_EngineItf)->CreateAudioPlayer(g_EngineItf, &g_Player, &source, &sink, 1, ids, req) != SL_RESULT_SUCCESS)
	{
		this->SetError("CreateAudioPlayer failed");
		this->ClosePlayback();
		return false;
	}

	if ((*g_Player)->Realize(g_Player, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
	{
		this->SetError("player realize failed");
		this->ClosePlayback();
		return false;
	}

	(*g_Player)->GetInterface(g_Player, SL_IID_PLAY, &g_PlayItf);
	(*g_Player)->GetInterface(g_Player, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &g_PlayQueue);

	if (g_PlayItf == NULL || g_PlayQueue == NULL)
	{
		this->SetError("player interfaces missing");
		this->ClosePlayback();
		return false;
	}

	(*g_PlayQueue)->RegisterCallback(g_PlayQueue, PlaybackCallback, this);

	memset(g_PlayBuffer, 0, sizeof(g_PlayBuffer));
	g_PlayIndex = 0;

	(*g_PlayItf)->SetPlayState(g_PlayItf, SL_PLAYSTATE_PLAYING);

	// Primed with silence. A buffer queue that is empty never calls back, so
	// without this the callback chain would never start and playback would sit
	// there silently doing nothing.
	for (int n = 0; n < kQueueBuffers; n++)
	{
		(*g_PlayQueue)->Enqueue(g_PlayQueue, g_PlayBuffer[n], VOICE_FRAME_SAMPLES * sizeof(short));
	}

	g_PlayIndex = 0;

	this->m_PlaybackOpen = true;

	g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] playback open");

	return true;
}

void CVoiceAudio::ClosePlayback() // OK
{
	if (g_PlayItf != NULL)
	{
		(*g_PlayItf)->SetPlayState(g_PlayItf, SL_PLAYSTATE_STOPPED);
	}

	if (g_PlayQueue != NULL)
	{
		(*g_PlayQueue)->Clear(g_PlayQueue);
	}

	// Destroy tears the callback down with the object, so nothing can be
	// running in PlaybackCallback after this returns.
	if (g_Player != NULL)
	{
		(*g_Player)->Destroy(g_Player);
		g_Player = NULL;
	}

	if (g_OutputMix != NULL)
	{
		(*g_OutputMix)->Destroy(g_OutputMix);
		g_OutputMix = NULL;
	}

	g_PlayItf = NULL;
	g_PlayQueue = NULL;

	if (this->m_PlaybackOpen)
	{
		g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] playback closed");
	}

	this->m_PlaybackOpen = false;
}

void CVoiceAudio::PumpPlayback() // OK
{
	// Nothing to do: OpenSL drives playback from its own thread. The function
	// exists so both backends present the same shape to VoiceAudio.cpp, which
	// holds all the policy and none of the mechanism.
}

// -------------------------------------------------------------------------
// capture
// -------------------------------------------------------------------------

bool CVoiceAudio::OpenCapture() // OK
{
	if (this->m_CaptureOpen)
	{
		return true;
	}

	if (EnsureEngine() == false)
	{
		this->SetError("OpenSL engine failed");
		return false;
	}

	SLDataLocator_IODevice device =
		{ SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, SL_DEFAULTDEVICEID_AUDIOINPUT, NULL };

	SLDataSource source = { &device, NULL };

	SLDataLocator_AndroidSimpleBufferQueue queueLoc =
		{ SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, (SLuint32)kQueueBuffers };

	SLDataFormat_PCM format = MakeFormat();
	SLDataSink sink = { &queueLoc, &format };

	// SL_IID_ANDROIDCONFIGURATION is requested purely so the recording preset
	// can be set below. Without it GetInterface fails and the microphone opens
	// raw.
	const SLInterfaceID ids[2] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
	const SLboolean req[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_FALSE };

	// This is where a missing RECORD_AUDIO grant shows up, as a plain failure
	// with no detail. The manifest declares the permission; asking the player
	// for it at runtime is VoiceChatRequestMicPermission, called before we get
	// here.
	if ((*g_EngineItf)->CreateAudioRecorder(g_EngineItf, &g_Recorder, &source, &sink, 2, ids, req) != SL_RESULT_SUCCESS)
	{
		this->SetError("no microphone (permission?)");
		return false;
	}

	// ---- echo cancellation and noise suppression -------------------------
	//
	// This is the fix for the microphone picking up the game's own audio out of
	// the phone's speaker. That is not background noise, it is acoustic echo,
	// and no amount of gating or filtering on our side removes it properly -
	// cancelling echo needs to compare what is being played against what is
	// being heard, sample-aligned, which only the platform can do.
	//
	// SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION is how you ask for that:
	// it routes capture through the device's voice pipeline, which is where the
	// hardware AEC, noise suppressor and gain control live - the same ones every
	// phone call and every video chat app uses. The default preset
	// (SL_ANDROID_RECORDING_PRESET_GENERIC) deliberately bypasses all of it,
	// because it is meant for recording music, and that is what this was using.
	//
	// Must be set AFTER CreateAudioRecorder and BEFORE Realize; the
	// configuration is read as the device is brought up and ignored afterwards.
	{
		SLAndroidConfigurationItf config = NULL;

		if ((*g_Recorder)->GetInterface(g_Recorder, SL_IID_ANDROIDCONFIGURATION, &config) == SL_RESULT_SUCCESS
			&& config != NULL)
		{
			SLuint32 preset = SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION;

			const SLresult presetResult = (*config)->SetConfiguration(
				config, SL_ANDROID_KEY_RECORDING_PRESET, &preset, sizeof(SLuint32));

			// Not fatal. A device that refuses the preset still records, just
			// without the platform's help, so it is worth saying rather than
			// failing - "why do they hear my game" has a concrete answer here.
			if (presetResult != SL_RESULT_SUCCESS)
			{
				g_ConsoleDebug->Write(MCD_RECEIVE,
					"[Voice] voice-communication preset refused - no echo cancellation");
			}
		}
		else
		{
			g_ConsoleDebug->Write(MCD_RECEIVE,
				"[Voice] no android config interface - no echo cancellation");
		}
	}

	if ((*g_Recorder)->Realize(g_Recorder, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
	{
		this->SetError("microphone realize failed (permission?)");
		this->CloseCapture();
		return false;
	}

	(*g_Recorder)->GetInterface(g_Recorder, SL_IID_RECORD, &g_RecordItf);
	(*g_Recorder)->GetInterface(g_Recorder, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &g_RecordQueue);

	if (g_RecordItf == NULL || g_RecordQueue == NULL)
	{
		this->SetError("microphone interfaces missing");
		this->CloseCapture();
		return false;
	}

	(*g_RecordQueue)->RegisterCallback(g_RecordQueue, RecordCallback, this);

	memset(g_RecordBuffer, 0, sizeof(g_RecordBuffer));
	g_RecordIndex = 0;

	for (int n = 0; n < kQueueBuffers; n++)
	{
		(*g_RecordQueue)->Enqueue(g_RecordQueue, g_RecordBuffer[n], VOICE_FRAME_SAMPLES * sizeof(short));
	}

	g_RecordIndex = 0;

	(*g_RecordItf)->SetRecordState(g_RecordItf, SL_RECORDSTATE_RECORDING);

	this->m_CaptureOpen = true;

	g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] microphone open");

	return true;
}

void CVoiceAudio::CloseCapture() // OK
{
	if (g_RecordItf != NULL)
	{
		(*g_RecordItf)->SetRecordState(g_RecordItf, SL_RECORDSTATE_STOPPED);
	}

	if (g_RecordQueue != NULL)
	{
		(*g_RecordQueue)->Clear(g_RecordQueue);
	}

	if (g_Recorder != NULL)
	{
		(*g_Recorder)->Destroy(g_Recorder);
		g_Recorder = NULL;
	}

	g_RecordItf = NULL;
	g_RecordQueue = NULL;

	if (this->m_CaptureOpen)
	{
		g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] microphone closed");
	}

	this->m_CaptureOpen = false;
	this->m_CaptureLevel = 0;
}

void CVoiceAudio::PumpCapture() // OK
{
	// See PumpPlayback: OpenSL's callback does this work.
}

// -------------------------------------------------------------------------
// device selection - not available here
// -------------------------------------------------------------------------
//
// OpenSL ES records from SL_DEFAULTDEVICEID_AUDIOINPUT and offers no way to
// enumerate or choose anything else. Picking an input on Android needs AAudio
// (API 28+, against minSdk 21) or the Java AudioManager. Reporting zero
// devices makes the options row hide itself rather than showing a picker that
// cannot pick.

int VoiceAudioGetCaptureDeviceCount() // OK
{
	return 0;
}

bool VoiceAudioGetCaptureDeviceName(int /*iIndex*/, char* /*szOut*/, int /*iMax*/) // OK
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

#endif  // Android only
