// VoiceAudio.cpp - the platform-independent half of the audio layer.
//
// Decides WHEN devices should be open and when frames should move. The two
// backend files decide HOW, and neither of them contains any policy.

#include "stdafx.h"
#include "VoiceAudio.h"
#include "VoiceClient.h"
#include "./Utilities/Log/muConsoleDebug.h"

#if defined(__ANDROID__)
#include "Platform/MobilePlatform.h"
#endif

#include <stdarg.h>

CVoiceAudio gVoiceAudio;

// How long the microphone stays open after the talk key is released. Long
// enough that normal back-and-forth speech does not reopen the device several
// times a second, short enough that "the microphone is only on while you are
// talking" stays true in any sense a player would care about.
#define VOICE_MIC_LINGER_MS     1000

CVoiceAudio::CVoiceAudio() // OK
{
	this->m_PlaybackOpen = false;
	this->m_CaptureOpen = false;
	this->m_Talking = false;
	this->m_StopTalkingTick = 0;
	this->m_CaptureLevel = 0;
	this->m_LastError[0] = '\0';
	this->m_CaptureFailed = false;
	this->m_PlaybackFailed = false;
}

CVoiceAudio::~CVoiceAudio() // OK
{
	this->Stop();
}

void CVoiceAudio::SetError(const char* szFormat, ...) // OK
{
	va_list args;
	va_start(args, szFormat);
	vsnprintf(this->m_LastError, sizeof(this->m_LastError), szFormat, args);
	va_end(args);

	this->m_LastError[sizeof(this->m_LastError) - 1] = '\0';

	g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] %s", this->m_LastError);
}

void CVoiceAudio::RequestCaptureReopen() // OK
{
	if (this->m_CaptureOpen == false)
	{
		return;
	}

	this->CloseCapture();

	// The failure flag is cleared too: a different device deserves its own
	// attempt rather than inheriting the last one's verdict.
	this->m_CaptureFailed = false;
	this->m_LastError[0] = '\0';
}

void CVoiceAudio::Stop() // OK
{
	this->CloseCapture();
	this->ClosePlayback();

	this->m_Talking = false;
	this->m_CaptureFailed = false;
	this->m_PlaybackFailed = false;
	this->m_CaptureLevel = 0;

	// Cleared with the session, so a device that was busy earlier gets another
	// chance next time rather than staying broken until the client restarts.
	this->m_LastError[0] = '\0';
}

void CVoiceAudio::SetTalking(bool bTalking) // OK
{
	if (bTalking == this->m_Talking)
	{
		return;
	}

	this->m_Talking = bTalking;

	if (bTalking == false)
	{
		this->m_StopTalkingTick = GetTickCount();

		// The tone only runs as a stand-in for a microphone that would not
		// open, so it stops with the key like real speech would.
		if (this->m_CaptureFailed)
		{
			gVoiceClient.SetLoopbackTest(false);
		}

		return;
	}

#if defined(__ANDROID__)
	// Asked for here, the first time somebody tries to talk, rather than at
	// startup. A game that demands the microphone before the player has seen a
	// reason for it mostly gets told no, and a denial is far harder to walk
	// back than a prompt that arrives with obvious context.
	//
	// The request is asynchronous, so this attempt still fails and falls
	// through to the tone below. The next press picks up the answer.
	if (MU_MobileHasMicPermission() == false)
	{
		MU_MobileRequestMicPermission();

		this->SetError("waiting for microphone permission");
		this->m_CaptureFailed = true;

		gVoiceClient.SetLoopbackTest(true);
		return;
	}
#endif

	if (this->OpenCapture())
	{
		this->m_CaptureFailed = false;
		return;
	}

	// No microphone, or something else already holds it. Rather than failing
	// silently, fall back to the test tone: the transport is still worth
	// proving, and hearing the tone at the other end answers "is it my
	// microphone or is it the network" on the spot.
	this->m_CaptureFailed = true;

	gVoiceClient.SetLoopbackTest(true);
}

void CVoiceAudio::Proc() // OK
{
	// Devices follow the session. Nothing is opened until the service has
	// accepted us, and everything closes when it has not - which also covers
	// logout, map change and a server that has voice switched off.
	if (gVoiceClient.IsReady() == false)
	{
		if (this->m_PlaybackOpen || this->m_CaptureOpen)
		{
			this->Stop();
		}

		return;
	}

	// One attempt per session. A device that refuses once will refuse every
	// frame, and retrying at 60Hz would fill the log and stutter the client
	// for no benefit.
	if (this->m_PlaybackOpen == false && this->m_PlaybackFailed == false)
	{
		if (this->OpenPlayback() == false)
		{
			this->m_PlaybackFailed = true;
		}
	}

	this->PumpPlayback();

	if (this->m_CaptureOpen)
	{
		this->PumpCapture();

		// Released a while ago. Close the microphone rather than holding it
		// open for the rest of the session.
		if (this->m_Talking == false
			&& (GetTickCount() - this->m_StopTalkingTick) > VOICE_MIC_LINGER_MS)
		{
			this->CloseCapture();
		}
	}
}
