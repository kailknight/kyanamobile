// VoiceChat.cpp - see VoiceChat.h for why this is driven from one tick.

#include "stdafx.h"
#include "VoiceChat.h"
#include "ServerInfo.h"
#include "User.h"
#include "Util.h"
#include "Log.h"
#include "Protocol.h"

CVoiceChat gVoiceChat;

CVoiceChat::CVoiceChat() // OK
{
	this->m_Socket = INVALID_SOCKET;
	this->m_Ready = false;
	this->m_TokenCounter = 0;

	memset(&this->m_ServiceAddr, 0, sizeof(this->m_ServiceAddr));
	memset(this->m_LastToken, 0, sizeof(this->m_LastToken));
}

CVoiceChat::~CVoiceChat() // OK
{
	this->Clean();
}

void CVoiceChat::Init() // OK
{
	this->Clean();

	if (gServerInfo.m_VoiceChatEnable == 0)
	{
		LogAdd(LOG_BLUE, "[VoiceChat] disabled");
		return;
	}

	// Send-only. The service never replies to the GameServer, so there is
	// nothing to bind and no receive path to run - which is also why this
	// needs no thread.
	this->m_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (this->m_Socket == INVALID_SOCKET)
	{
		LogAdd(LOG_RED, "[VoiceChat] socket failed (%d)", WSAGetLastError());
		return;
	}

	memset(&this->m_ServiceAddr, 0, sizeof(this->m_ServiceAddr));
	this->m_ServiceAddr.sin_family = AF_INET;
	this->m_ServiceAddr.sin_port = htons((u_short)gServerInfo.m_VoiceChatPort);

	// Same resolve the other UDP link uses (CSocketManagerUdp::Connect) -
	// literal address first, hostname as the fallback.
	this->m_ServiceAddr.sin_addr.s_addr = inet_addr(gServerInfo.m_VoiceChatHost);

	if (this->m_ServiceAddr.sin_addr.s_addr == INADDR_NONE)
	{
		hostent* host = gethostbyname(gServerInfo.m_VoiceChatHost);

		if (host == 0)
		{
			LogAdd(LOG_RED, "[VoiceChat] could not resolve %s (%d)", gServerInfo.m_VoiceChatHost, WSAGetLastError());
			closesocket(this->m_Socket);
			this->m_Socket = INVALID_SOCKET;
			return;
		}

		memcpy(&this->m_ServiceAddr.sin_addr.s_addr, (*host->h_addr_list), host->h_length);
	}

	this->m_Ready = true;

	LogAdd(LOG_BLUE, "[VoiceChat] relaying to %s:%d", gServerInfo.m_VoiceChatHost, gServerInfo.m_VoiceChatPort);
}

void CVoiceChat::Clean() // OK
{
	if (this->m_Socket != INVALID_SOCKET)
	{
		closesocket(this->m_Socket);
		this->m_Socket = INVALID_SOCKET;
	}

	this->m_Ready = false;

	memset(this->m_LastToken, 0, sizeof(this->m_LastToken));
}

bool CVoiceChat::SendToService(const void* pData, int iLength) // OK
{
	if (this->m_Ready == false)
	{
		return false;
	}

	// UDP and send-only: a failure here is not worth retrying or logging per
	// packet. The next tick sends a fresh position anyway, and the service
	// expires sessions it stops hearing about.
	return (sendto(this->m_Socket, (const char*)pData, iLength, 0,
		(const sockaddr*)&this->m_ServiceAddr, sizeof(this->m_ServiceAddr)) == iLength);
}

VQWORD CVoiceChat::MakeToken(int aIndex) // OK
{
	// Not cryptographic, and it does not need to be: a token is only useful
	// to someone who can also reach the voice port, and the service binds it
	// to the first endpoint that presents it. What it DOES need is to be
	// unguessable enough that a player cannot derive a neighbour's token, and
	// unique enough that a re-login into the same object slot never collides
	// with the session it replaced.
	this->m_TokenCounter++;

	VQWORD token = 0;

	token ^= ((VQWORD)GetTickCount() << 32);
	token ^= ((VQWORD)GetLargeRand() << 16);
	token ^= (VQWORD)GetLargeRand();
	token ^= ((VQWORD)this->m_TokenCounter << 24);
	token ^= (VQWORD)aIndex;

	// Zero is the service's "no session" sentinel, so it must never be issued.
	if (token == 0)
	{
		token = 1;
	}

	return token;
}

void CVoiceChat::SendSession(LPOBJ lpObj) // OK
{
	VOICE_G2V_SESSION_MSG msg;
	memset(&msg, 0, sizeof(msg));

	msg.Type = VOICE_G2V_SESSION;
	msg.Token = lpObj->m_VoiceToken;
	msg.PlayerIndex = (VWORD)lpObj->Index;

	memcpy(msg.Name, lpObj->Name, VOICE_NAME_LENGTH - 1);
	msg.Name[VOICE_NAME_LENGTH - 1] = '\0';

	this->SendToService(&msg, sizeof(msg));
}

void CVoiceChat::SendPosition(LPOBJ lpObj) // OK
{
	VOICE_G2V_POS_MSG msg;
	memset(&msg, 0, sizeof(msg));

	msg.Type = VOICE_G2V_POS;
	msg.PlayerIndex = (VWORD)lpObj->Index;
	msg.Map = (VBYTE)lpObj->Map;
	msg.X = (VBYTE)lpObj->X;
	msg.Y = (VBYTE)lpObj->Y;

	this->SendToService(&msg, sizeof(msg));
}

void CVoiceChat::SendDrop(int aIndex, VQWORD Token) // OK
{
	VOICE_G2V_DROP_MSG msg;
	memset(&msg, 0, sizeof(msg));

	msg.Type = VOICE_G2V_DROP;
	msg.PlayerIndex = (VWORD)aIndex;

	this->SendToService(&msg, sizeof(msg));
}

void CVoiceChat::GCVoiceInfoSend(LPOBJ lpObj) // OK
{
	PMSG_VOICE_INFO_SEND pMsg;
	memset(&pMsg, 0, sizeof(pMsg));

	pMsg.header.set(0xD3, 0x7A, sizeof(pMsg));

	pMsg.Enable = 1;
	pMsg.Port = (WORD)gServerInfo.m_VoiceChatPort;
	pMsg.Token = lpObj->m_VoiceToken;

	// The address the CLIENT should reach the service on, which is not
	// necessarily the one this server uses. A voice service on the same box as
	// the GameServer is 127.0.0.1 from here and the public address from a
	// player's machine, so it is configured separately.
	memcpy(pMsg.Host, gServerInfo.m_VoiceChatClientHost, sizeof(pMsg.Host) - 1);
	pMsg.Host[sizeof(pMsg.Host) - 1] = '\0';

	// sizeof, not header.size - PSWMSG_HEAD::size is a two-byte big-endian
	// field inside the packet, not a length DataSend can take.
	DataSend(lpObj->Index, (BYTE*)&pMsg, sizeof(pMsg));
}

void CVoiceChat::ClearSession(int aIndex) // OK
{
	if (OBJMAX_RANGE(aIndex) == 0)
	{
		return;
	}

	if (this->m_LastToken[aIndex] != 0)
	{
		this->SendDrop(aIndex, this->m_LastToken[aIndex]);
		this->m_LastToken[aIndex] = 0;
	}

	gObj[aIndex].m_VoiceToken = 0;
}

void CVoiceChat::Proc() // OK
{
	if (this->m_Ready == false)
	{
		return;
	}

	for (int n = OBJECT_START_USER; n < MAX_OBJECT; n++)
	{
		LPOBJ lpObj = &gObj[n];

		// Gone, but we were tracking one. Covers every way out at once -
		// logout, timeout, kick, crash - without knowing which it was.
		if (gObjIsConnectedGP(n) == 0)
		{
			if (this->m_LastToken[n] != 0)
			{
				this->SendDrop(n, this->m_LastToken[n]);
				this->m_LastToken[n] = 0;
			}

			continue;
		}

		// Connected but not in the world yet - no position worth sending.
		if (lpObj->Type != OBJECT_USER || lpObj->State != OBJECT_PLAYING)
		{
			continue;
		}

		// A token that no longer matches means this slot was reused by a
		// different player, so the old session has to go before the new one
		// opens - otherwise the service would keep the previous name against
		// this index.
		if (this->m_LastToken[n] != 0 && this->m_LastToken[n] != lpObj->m_VoiceToken)
		{
			this->SendDrop(n, this->m_LastToken[n]);
			this->m_LastToken[n] = 0;
		}

		if (lpObj->m_VoiceToken == 0)
		{
			lpObj->m_VoiceToken = this->MakeToken(n);

			this->SendSession(lpObj);
			this->GCVoiceInfoSend(lpObj);

			this->m_LastToken[n] = lpObj->m_VoiceToken;

			// No position yet this tick. The session exists from the service's
			// point of view, and the next tick places it - one second before
			// anyone can hear this player, which is imperceptible next to the
			// time it takes a client to open its own socket and say HELLO.
			continue;
		}

		this->SendPosition(lpObj);
	}
}
