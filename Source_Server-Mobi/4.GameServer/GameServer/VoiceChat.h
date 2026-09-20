// VoiceChat.h - the GameServer's half of proximity voice chat.
//
// The GameServer owns identity and position; the voice service owns routing.
// This module is the bridge: it hands every player a token, tells the service
// where everyone is standing, and tells each client where to send its audio.
//
// It never carries audio. Not one sample passes through here - that is the
// whole point of running voice on its own service.
//
// DRIVEN ENTIRELY FROM ONE 1Hz TICK
//
// There is no login hook and no logout hook. Proc() walks the object table
// once a second and reconciles: a playing user with no token gets one, a user
// with a token gets a position update, and a token belonging to somebody no
// longer connected is dropped.
//
// That is deliberate. Threading hooks through the login and disconnect paths
// would mean finding every route a player can leave by - timeout, kick, crash,
// server shutdown, character change - and missing one leaves a ghost that
// other players keep hearing. Reconciling from the table cannot miss a case
// because it never asks what happened, only what is true now. It also matches
// the service, which treats the position feed as the session keepalive.

#pragma once

#include "Protocol.h"
#include "User.h"
#include "VoiceProtocol.h"

class CVoiceChat
{
public:
	CVoiceChat();
	virtual ~CVoiceChat();

	// Opens the UDP socket and resolves the service address. Safe to call when
	// voice is disabled - it simply does nothing and stays inert.
	void Init();

	void Clean();

	// The 1Hz reconcile. See the header comment.
	void Proc();

	// Clears one player's token, for the paths that already know a player is
	// leaving. Purely an optimisation: without it the next Proc() notices
	// anyway, one second later.
	void ClearSession(int aIndex);

private:
	bool SendToService(const void* pData, int iLength);

	void SendSession(LPOBJ lpObj);
	void SendPosition(LPOBJ lpObj);
	void SendDrop(int aIndex, VQWORD Token);

	// Tells the client where to send its audio, and with which token.
	void GCVoiceInfoSend(LPOBJ lpObj);

	VQWORD MakeToken(int aIndex);

	SOCKET m_Socket;
	sockaddr_in m_ServiceAddr;
	bool m_Ready;

	// Mirrors gObj[].m_VoiceToken, but for slots that have gone away. Proc()
	// compares the two to spot a player who left without the object being
	// available to read a token from any more.
	VQWORD m_LastToken[MAX_OBJECT];

	// Rolls into every token so two players issued in the same millisecond,
	// or a re-login into the same object slot, cannot collide.
	VDWORD m_TokenCounter;
};

extern CVoiceChat gVoiceChat;

//**********************************************//
//************ GameServer -> Client ************//
//**********************************************//

#pragma pack(push, 1)

// Sent once when a player is given a voice session. The client needs all
// three: where to send, and what to identify itself as.
struct PMSG_VOICE_INFO_SEND
{
	PSWMSG_HEAD header; // C2:D3:7A

	// 0 means voice is off for this server; the client should not open a
	// socket at all. Sent rather than staying silent so the client can tell
	// "disabled" from "server too old to answer".
	BYTE   Enable;

	WORD   Port;
	VQWORD Token;
	char   Host[64];
};

#pragma pack(pop)
