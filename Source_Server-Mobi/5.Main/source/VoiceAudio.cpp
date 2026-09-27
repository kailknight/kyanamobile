// VoiceAudio.cpp - the platform-independent half of the audio layer.
//
// Decides WHEN devices should be open and when frames should move. The two
// backend files decide HOW, and neither of them contains any policy.

#include "stdafx.h"
#include "VoiceAudio.h"
#include "VoiceClient.h"
#include "./Utilities/Log/muConsoleDebug.h"

#if defined(__ANDROID__) || defined(MU_IOS)
#include "Platform/MobilePlatform.h"
#endif
#if defined(MU_IOS)
#include "IosVoice.h"
#define VOICE_TRACE(...) IosVoice_Log(__VA_ARGS__)
#else
#define VOICE_TRACE(...) ((void)0)
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
	this->m_WaitingForPermission = false;
	this->m_CaptureRetryTick = 0;
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
	this->m_WaitingForPermission = false;
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

	VOICE_TRACE("game: SetTalking(%d) ready=%d playbackOpen=%d captureOpen=%d", bTalking ? 1 : 0,
		gVoiceClient.IsReady() ? 1 : 0, this->m_PlaybackOpen ? 1 : 0, this->m_CaptureOpen ? 1 : 0);

	if (bTalking == false)
	{
		this->m_StopTalkingTick = GetTickCount();

		// The tone only runs as a stand-in for a microphone that would not
		// open, so it stops with the key like real speech would. Keyed off the
		// tone itself: Stop() and RequestCaptureReopen() clear m_CaptureFailed
		// without switching it off.
		if (gVoiceClient.GetLoopbackTest())
		{
			gVoiceClient.SetLoopbackTest(false);
		}

		return;
	}

	// Not yet: Proc opens the microphone once the service has accepted us.
	// Opening it here would have Proc close it again on the same frame, and
	// the talk key being held would reopen it on the next - every frame, and
	// on iOS each of those rebuilds the audio engine.
	if (gVoiceClient.IsReady() == false)
	{
		return;
	}

	this->StartCapture();
}

void CVoiceAudio::StartCapture() // OK
{
#if defined(__ANDROID__) || defined(MU_IOS)
	// Asked for here, the first time somebody tries to talk, rather than at
	// startup. A game that demands the microphone before the player has seen a
	// reason for it mostly gets told no, and a denial is far harder to walk
	// back than a prompt that arrives with obvious context.
	//
	// The request is asynchronous, so this attempt falls through to the tone
	// below. Proc retries once the answer is in, without the player having to
	// press again - the talk button is a toggle on mobile and stays on.
#if defined(MU_IOS)
	// Only while the prompt is unanswered. A denial falls through to
	// OpenCapture, which says to allow it in Settings.
	if (IosVoice_MicPermission() == -1)
#else
	if (MU_MobileHasMicPermission() == false)
#endif
	{
		VOICE_TRACE("game: no mic permission yet - asking, sending the test tone meanwhile");
		MU_MobileRequestMicPermission();

		this->SetError("waiting for microphone permission");
		this->m_CaptureFailed = true;
		this->m_WaitingForPermission = true;

		gVoiceClient.SetLoopbackTest(true);
		return;
	}
#endif

	// Past the permission check: a device failure from here on is not retried.
	this->m_WaitingForPermission = false;

	if (this->OpenCapture())
	{
		if (gVoiceClient.GetLoopbackTest())
		{
			gVoiceClient.SetLoopbackTest(false);
		}
		this->m_CaptureFailed = false;
		this->m_LastError[0] = '\0';

		// A gate floor learned from the last press - or from speech already
		// under way as the device came up - would shut out this one.
		gVoiceClient.ResetCaptureGate();
		return;
	}

	VOICE_TRACE("game: OpenCapture failed (%s) - sending the test tone", this->m_LastError);

	// No microphone, or something else already holds it. Rather than failing
	// silently, fall back to the test tone: the transport is still worth
	// proving, and hearing the tone at the other end answers "is it my
	// microphone or is it the network" on the spot.
	this->m_CaptureFailed = true;

	gVoiceClient.SetLoopbackTest(true);
}

void CVoiceAudio::Proc() // OK
{
#if defined(MU_IOS)
	// Every frame, voice session or not: it is also what recovers game audio
	// from an interruption iOS never reported the end of.
	IosVoice_Pump();
#endif

	// Devices follow the session. Nothing is opened until the service has
	// accepted us, and everything closes when it has not - which also covers
	// logout, map change and a server that has voice switched off.
	if (gVoiceClient.IsReady() == false)
	{
		if (this->m_PlaybackOpen || this->m_CaptureOpen)
		{
			if (gVoiceClient.GetLoopbackTest())
			{
				gVoiceClient.SetLoopbackTest(false);
			}
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

	// Talking but no microphone yet: the key went down before the session was
	// ready, or the permission prompt has since been answered. A device that
	// failed for any other reason is not retried - it would fail every time.
	if (this->m_Talking && this->m_CaptureOpen == false)
	{
		if (this->m_CaptureFailed == false)
		{
			this->StartCapture();
		}
#if defined(__ANDROID__) || defined(MU_IOS)
		else if (this->m_WaitingForPermission && (GetTickCount() - this->m_CaptureRetryTick) > 500)
		{
			this->m_CaptureRetryTick = GetTickCount();

#if defined(MU_IOS)
			if (IosVoice_MicPermission() != -1)
#else
			if (MU_MobileHasMicPermission())
#endif
			{
				this->StartCapture();
			}
		}
#endif
	}

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
