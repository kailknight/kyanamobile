// VoiceAudioNull.cpp - iOS placeholder backend for proximity voice.
//
// The first iOS build ships without audio. Neither device opens, so the
// platform-independent half in VoiceAudio.cpp reports the reason through
// GetLastError() instead of the build failing to link.

#include "stdafx.h"

#if defined(MU_IOS)

#include "VoiceAudio.h"

bool CVoiceAudio::OpenPlayback()
{
	this->SetError("voice audio is not available on iOS yet");
	return false;
}

void CVoiceAudio::ClosePlayback()
{
}

bool CVoiceAudio::OpenCapture()
{
	this->SetError("voice audio is not available on iOS yet");
	return false;
}

void CVoiceAudio::CloseCapture()
{
}

void CVoiceAudio::PumpPlayback()
{
}

void CVoiceAudio::PumpCapture()
{
}

int VoiceAudioGetCaptureDeviceCount()
{
	return 0;
}

bool VoiceAudioGetCaptureDeviceName(int, char*, int)
{
	return false;
}

void VoiceAudioGetSelectedCaptureDeviceName(char* szOut, int iMax)
{
	if (szOut == NULL || iMax <= 0)
	{
		return;
	}

	strncpy(szOut, "System default", (size_t)(iMax - 1));
	szOut[iMax - 1] = '\0';
}

#endif
