// VoiceAudio.h - the microphone and the speaker.
//
// The only part of proximity voice where the two platforms genuinely differ,
// which is why it is the last layer rather than the first. Everything below it
// - the codec, the wire format, the transport, the jitter buffers - is shared
// and was finished and tested before any of this existed.
//
// Two implementations, one interface:
//
//   VoiceAudioWin.cpp   PC       waveIn / waveOut (winmm)
//   VoiceAudioSDL.cpp   Android  SDL_OpenAudioDevice
//
// Neither is chosen for elegance; both are chosen because they add no new
// dependency. winmm.lib is already linked into the PC client and SDL_INIT_AUDIO
// is already up on Android with no device open (music there goes through a Java
// MuAudio class over JNI, not through SDL). WASAPI would be roughly three times
// the code for no audible difference at 8 kHz mono, and putting SDL2 into the
// PC build would mean a new DLL beside a client that MUGUARD checksums.
//
// PUSH TO TALK AND THE MICROPHONE
//
// The capture device is opened when talking starts and closed shortly after it
// stops, rather than being held open for the session. That costs a few
// milliseconds off the first syllable, and buys the plain claim that the
// microphone is not open unless somebody is holding the key - which was the
// point of choosing push-to-talk in the first place.
//
// The "shortly after" is a one second linger, so that ordinary conversation -
// key down, key up, key down again - does not churn the device open and closed
// several times a second.

#pragma once

#include "VoiceProtocol.h"

class CVoiceAudio
{
public:
	CVoiceAudio();
	virtual ~CVoiceAudio();

	// Called once a frame from MoveMainScene, straight after the transport's
	// own Proc. Opens and closes devices as the session and the talk key
	// require, and moves audio in both directions.
	void Proc();

	void Stop();

	// Push to talk. Safe to call every frame with the same value.
	void SetTalking(bool bTalking);

	bool IsTalking() const { return this->m_Talking; }
	bool IsPlaybackOpen() const { return this->m_PlaybackOpen; }
	bool IsCaptureOpen() const { return this->m_CaptureOpen; }

	// Peak level of the last captured frame, 0-255. Phase 5 draws a meter with
	// this; it is also the quickest way to tell a muted microphone from a
	// broken network.
	int GetCaptureLevel() const { return this->m_CaptureLevel; }

	// Empty unless something failed. Shown in the debug readout so a device
	// that refuses to open says why instead of just being silent.
	const char* GetLastError() const { return this->m_LastError; }

	// Closes the microphone if it is open, so the next press picks up a changed
	// device. Cheap and safe to call when nothing is open.
	void RequestCaptureReopen();

	// Public only because the Android backend reports this from SDL's audio
	// callback, which is a free function and cannot reach a private member.
	// Written from the audio thread and read from the game loop: a plain int
	// is fine, because a torn read of a level meter is not a thing that can
	// happen on either target and would not matter if it did.
	void NotifyCaptureLevel(int iLevel) { this->m_CaptureLevel = iLevel; }

private:
	// Implemented per platform.
	bool OpenPlayback();
	void ClosePlayback();
	bool OpenCapture();
	void CloseCapture();
	void PumpPlayback();
	void PumpCapture();

	void SetError(const char* szFormat, ...);

	bool  m_PlaybackOpen;
	bool  m_CaptureOpen;
	bool  m_Talking;
	DWORD m_StopTalkingTick;
	int   m_CaptureLevel;
	char  m_LastError[128];

	// Set when the capture device will not open. The transport is still worth
	// proving in that case, so talking falls back to the test tone rather than
	// doing nothing - which also answers "is it my microphone or the network"
	// without any further work.
	bool  m_CaptureFailed;

	// Tracked separately from m_CaptureFailed and from m_LastError, so that a
	// missing microphone does not also stop the player hearing anybody. These
	// are two devices and they fail independently.
	bool  m_PlaybackFailed;
};

extern CVoiceAudio gVoiceAudio;

// Reads the talk key (or, on Android, the on-screen button) and drives
// SetTalking. Lives in CBInterface.cpp because that is where the client's key
// handling already is.
void VoiceChatProcInput();

// Set by the Android on-screen push-to-talk button, read by VoiceChatProcInput.
// A phone has no key to hold, so the touch handler holds this instead.
extern bool g_VoiceTouchTalking;

// The RAW talk-key state, set every frame by VoiceChatProcInput before it
// checks whether voice is usable at all. The readout uses this to answer "did
// my key even register", which is a different question from "am I talking" and
// the one a player actually needs answered when nothing happens.
extern bool g_VoicePttKeyHeld;

// True while the chat input box has the keyboard, so the readout can say why
// the talk key is being ignored instead of looking broken.
extern bool g_VoicePttBlockedByChat;

// -------------------------------------------------------------------------
// Microphone selection
//
// Implemented per platform. Android returns 0 devices: OpenSL ES offers no way
// to choose an input, so there is nothing to list and the UI hides itself
// rather than showing a picker that cannot pick.
// -------------------------------------------------------------------------

// Number of selectable capture devices, 0 if the platform cannot choose.
int VoiceAudioGetCaptureDeviceCount();

// Human-readable name of a device, for the options row. Returns false and
// leaves the buffer untouched for an index that does not exist.
bool VoiceAudioGetCaptureDeviceName(int iIndex, char* szOut, int iMax);

// The name to show for whatever is currently selected, including the
// "system default" case. Always writes something printable.
void VoiceAudioGetSelectedCaptureDeviceName(char* szOut, int iMax);
