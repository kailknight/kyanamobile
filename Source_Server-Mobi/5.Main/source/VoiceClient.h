// VoiceClient.h - the client half of proximity voice chat.
//
// Shared by PC and Android: one UDP socket, HELLO to claim the session, an
// encoded frame every 20ms while the talk key is held, and a mix of everyone
// audible coming back the other way.
//
// WHAT THIS LAYER DOES NOT DO
//
// It never touches a microphone or a speaker. Capture hands it 160 samples at
// a time through SendCaptureFrame, playback asks for samples through
// PullPlayback, and the two backends behind those calls are the only part of
// this feature that differs between PC and Android (Phase 4). Keeping the
// split here means the transport can be finished and tested with a synthetic
// source before either backend exists - see SetLoopbackTest.
//
// THREADING
//
// Proc() runs on the game's main loop. SendCaptureFrame and PullPlayback are
// expected to be called from an audio thread, because that is how SDL's audio
// callback works on Android. Everything mutable is therefore behind one mutex.
// One rather than several on purpose: the critical sections are a memcpy of
// 160 shorts, the callers arrive 20ms apart, and a single lock cannot deadlock
// against itself. Socket reads happen outside it.

#pragma once

#include "VoiceProtocol.h"
#include "VoiceCodec.h"

#include <mutex>

// How many people can be audible at once. Past this the furthest are simply
// not mixed - well beyond anything intelligible, since more than three or four
// simultaneous voices is already noise.
#define VOICE_MAX_SPEAKERS      16

// Jitter buffer depth per speaker, in frames of 20ms. Eight frames is 160ms:
// enough to ride out ordinary network wobble, short enough that nobody is
// talking noticeably behind their own character.
#define VOICE_JITTER_FRAMES     8

#define VOICE_JITTER_SAMPLES    (VOICE_JITTER_FRAMES * VOICE_FRAME_SAMPLES)

// A speaker is considered to have stopped this long after their last frame.
// Only drives the "who is talking" readout; audio already stops when frames do.
#define VOICE_SPEAKING_HOLD_MS  400

#pragma pack(push, 1)

// Mirror of the GameServer's PMSG_VOICE_INFO_SEND, packet 0xC2:0xD3:0x7A.
//
// The 5-byte head is spelled out here rather than pulled in from WSclient.h so
// this header stays free of the game's protocol headers - it is included from
// the scene and the packet dispatcher as well as from its own .cpp. The layout
// must match PSWMSG_HEAD exactly; there is a compile-time check for the total
// size in VoiceClient.cpp.
struct PMSG_VOICE_INFO_RECV
{
	BYTE   type;      // 0xC2
	BYTE   size[2];   // big endian
	BYTE   head;      // 0xD3
	BYTE   subh;      // 0x7A

	BYTE   Enable;
	WORD   Port;
	VQWORD Token;
	char   Host[64];
};

#pragma pack(pop)

enum eVoiceClientState
{
	// No server info yet, or the server said voice is off. No socket is open.
	VOICE_CLIENT_OFF = 0,

	// Socket open, HELLO sent, waiting for HELLO_OK.
	VOICE_CLIENT_CONNECTING = 1,

	VOICE_CLIENT_READY = 2,

	// The service does not recognise our token. Retrying would just produce
	// the same answer, so we stop and wait for the GameServer to issue a new
	// one, which it does on its own every second.
	VOICE_CLIENT_REJECTED = 3,
};

// One person we can currently hear.
struct VOICE_SPEAKER
{
	bool  Used;
	VWORD PlayerIndex;

	// Distance attenuation from the service, 0-255. Applied at mix time rather
	// than on arrival so the newest value always wins, even for audio already
	// sitting in the jitter buffer.
	VBYTE Volume;

	VWORD LastSeq;
	bool  HasSeq;
	DWORD LastFrameTick;

	// Decoded PCM waiting to be played, as a ring of samples.
	short Pcm[VOICE_JITTER_SAMPLES];
	int   Read;
	int   Write;
	int   Fill;
};

class CVoiceClient
{
public:
	CVoiceClient();
	virtual ~CVoiceClient();

	void Init();
	void Release();

	// From the game connection, packet 0xD3:0x7A. Safe to call repeatedly with
	// the same values - it only acts when something actually changed.
	void SetServerInfo(BYTE bEnable, const char* szHost, WORD wPort, VQWORD Token);

	// Called once a frame from MoveMainScene. Pumps the socket, re-sends HELLO
	// when needed, and retires speakers who have gone quiet.
	void Proc();

	// -- the seam the Phase 4 audio backends plug into --------------------

	// Capture side: exactly VOICE_FRAME_SAMPLES samples, 8kHz mono.
	void SendCaptureFrame(const short* pSamples);

	// Playback side: always writes iCount samples, silence included, so the
	// caller never has to care whether anyone is talking.
	void PullPlayback(short* pOut, int iCount);

	// -- state and diagnostics --------------------------------------------

	// The SERVER's switch. Whether this player wants to take part is a separate
	// question - see m_PlayerEnabled - because the two fail differently: a
	// server with voice off should show no voice UI at all, while a player who
	// opted out should still be told the feature exists.
	bool IsEnabled() const { return this->m_Enable; }

	// This player's own opt-out, mirrored from GameConfig once a frame.
	bool IsPlayerEnabled() const { return this->m_PlayerEnabled; }
	bool IsMutingOthers() const { return this->m_MuteOthers; }
	bool IsReady() const { return this->m_State == VOICE_CLIENT_READY; }
	int  GetState() const { return this->m_State; }

	// True while the given game index is talking. Phase 5 puts an indicator
	// over their head with this.
	bool IsSpeaking(int iPlayerIndex);

	int  GetSpeakingCount();

	// Peak of the last frame actually SENT, 0-255 - after gain, not before.
	// This is what the meter shows: the raw device level says what the
	// microphone heard, but what matters to the player is what went out, and
	// with auto-levelling the two are far apart on a quiet microphone.
	int GetSentLevel() const { return this->m_SentLevel; }

	// The gain auto-levelling has settled on, as a percentage, for the readout.
	int GetAutoGainPercent() const { return (int)(this->m_CaptureGain * 100.0f); }

	// Whether the noise gate is currently passing audio. Shown in the readout so
	// a player whose gate is set too tight can see it closing on them instead of
	// wondering why nobody hears the start of their sentences.
	bool IsGateOpen() const { return this->m_GateOpen; }

	DWORD GetFramesSent() const { return this->m_FramesSent; }
	DWORD GetFramesRecv() const { return this->m_FramesRecv; }
	DWORD GetFramesLost() const { return this->m_FramesLost; }

	// -- Phase 3 verification ---------------------------------------------
	//
	// Feeds a tone into SendCaptureFrame on the main loop's timing, so the
	// transport can be proved end-to-end between two clients before any
	// microphone code exists. Not shipped behaviour; there is no UI for it.
	void SetLoopbackTest(bool bOn);
	bool GetLoopbackTest() const { return this->m_LoopbackTest; }

private:
	bool OpenSocket();
	void CloseSocket();
	void SendHello();
	void SendBye();
	void PumpReceive();
	void HandlePacket(const VBYTE* pBuffer, int iLength);
	void HandleFrameIn(const VOICE_V2C_FRAME_MSG* lpMsg);
	void SendFrameLocked(const short* pSamples);
	void ExpireSpeakers(DWORD dwNow);
	void ProcLoopbackTest(DWORD dwNow);

	// Both assume the lock is already held.
	int  FindSpeaker(VWORD wPlayerIndex);
	int  AcquireSpeaker(VWORD wPlayerIndex);

	mutable std::mutex m_Lock;

	bool    m_Enable;
	char    m_Host[64];
	WORD    m_Port;
	VQWORD  m_Token;

	int     m_State;
	DWORD   m_LastHelloTick;

	// Doubles as the NAT keepalive. The service's sessions are kept alive by
	// the GameServer's position feed, not by us, but a client that says nothing
	// for minutes loses its NAT mapping and goes deaf the moment somebody
	// nearby starts talking. A HELLO every so often keeps the hole open, and
	// the service rebinds the endpoint from it if the port moved.
	DWORD   m_LastKeepAliveTick;

	SOCKET  m_Socket;
	sockaddr_in m_ServerAddr;

	VOICE_CODEC_STATE m_EncodeState;
	VWORD   m_SendSeq;

	// Automatic input levelling. Microphone sensitivity varies by an order of
	// magnitude between a headset and a phone's built-in mic, and at 8 kHz
	// telephone quality a quiet signal is not merely quiet - it loses most of
	// what little resolution ADPCM has, so it arrives as mush as well as faint.
	//
	// Only ever amplifies. Bringing loud speech DOWN is playback's job, and
	// doing it here would fight the distance attenuation the service applies.
	float   m_CaptureGain;
	int     m_SentLevel;

	// Noise gate. Complements the platform echo canceller rather than replacing
	// it: the canceller removes what the speaker is playing, this stops steady
	// background - a fan, traffic, a room's hum - from being transmitted at all
	// between words. It also means nothing goes out while nobody is speaking,
	// which is where most of a session's bandwidth would otherwise go.
	//
	// The floor is learned rather than fixed, because "quiet" on a headset and
	// "quiet" on a phone held at arm's length are an order of magnitude apart.
	float   m_NoiseFloor;
	DWORD   m_GateHoldUntil;
	bool    m_GateOpen;

	VOICE_SPEAKER m_Speaker[VOICE_MAX_SPEAKERS];

	DWORD   m_FramesSent;
	DWORD   m_FramesRecv;
	DWORD   m_FramesLost;

	bool    m_LoopbackTest;
	DWORD   m_LoopbackTick;
	int     m_LoopbackPhase;

	// Copied out of GameConfig once a frame by Proc, on the game thread, so
	// that PullPlayback can read them from an audio callback without reaching
	// into a singleton from a thread that has no business being there.
	bool    m_PlayerEnabled;
	bool    m_MuteOthers;
	int     m_PlaybackVolume;   // 0-100
};

extern CVoiceClient gVoiceClient;

// Drawn by the scene's information pass. Shows nothing unless voice is on.
void RenderVoiceChatDebug();

// The player's own voice preferences. Implemented in VoiceSettings.cpp, which
// is the one place that knows the client keeps PC settings in the registry and
// Android settings in GameConfig.
bool VoiceSettingGetEnabled();
void VoiceSettingSetEnabled(bool bEnabled);
bool VoiceSettingGetMuteOthers();
void VoiceSettingSetMuteOthers(bool bMute);
int  VoiceSettingGetVolume();
void VoiceSettingSetVolume(int iVolume);
int  VoiceSettingGetPttKey();
void VoiceSettingSetPttKey(int iVirtualKey);

// Which microphone to record from: -1 for the system default, otherwise a
// waveIn device id. PC only - see CfgDefaultVoiceMicDevice.
int  VoiceSettingGetMicDevice();
void VoiceSettingSetMicDevice(int iDeviceId);

// Extra gain on top of the automatic levelling, as a percentage (100 = none).
// Both platforms: a quiet phone microphone needs this as much as a PC one.
int  VoiceSettingGetMicBoost();
void VoiceSettingSetMicBoost(int iPercent);

// The next value in the cycle the options row steps through. Kept beside the
// setting so the list of stops lives in one place rather than in the UI.
int  VoiceSettingNextMicBoost(int iCurrentPercent);

// Noise gate on/off. Works on both platforms; on Android it sits on top of the
// platform's own echo cancellation rather than replacing it.
bool VoiceSettingGetNoiseGate();
void VoiceSettingSetNoiseGate(bool bEnabled);

// Hold to talk, or tap to toggle. CfgVoiceTalkModeHold / CfgVoiceTalkModeToggle.
int  VoiceSettingGetTalkMode();
void VoiceSettingSetTalkMode(int iMode);
