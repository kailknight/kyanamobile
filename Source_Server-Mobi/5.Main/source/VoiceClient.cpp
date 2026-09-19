// VoiceClient.cpp - see VoiceClient.h for the shape and the threading rules.

#include "stdafx.h"
#include "VoiceClient.h"
#include "UIControls.h"
#include "VoiceAudio.h"
#include "ZzzOpenglUtil.h"
#include "ZzzCharacter.h"
#include "ZzzInterface.h"
#include "./Utilities/Log/muConsoleDebug.h"


CVoiceClient gVoiceClient;

// The packet structs are hand-mirrored across three separately compiled
// programs with no version negotiation, so a silent size mismatch is the exact
// failure mode to guard against. 5 head + 1 + 2 + 8 + 64.
static_assert(sizeof(PMSG_VOICE_INFO_RECV) == 80, "PMSG_VOICE_INFO_RECV must match the GameServer's PMSG_VOICE_INFO_SEND");
static_assert(sizeof(VOICE_C2V_FRAME_MSG) == 1 + 8 + 2 + 1 + VOICE_FRAME_BYTES, "VOICE_C2V_FRAME_MSG is not packed as expected");
static_assert(sizeof(VOICE_V2C_FRAME_MSG) == 1 + 2 + 1 + 2 + 1 + VOICE_FRAME_BYTES, "VOICE_V2C_FRAME_MSG is not packed as expected");

// Winsock and POSIX disagree on exactly two things here and agree on the rest.
// Wrapping just those two keeps the body of this file identical on both.
//
// The address-length argument is the one that actually matters: Winsock takes
// `int*`, POSIX takes `socklen_t*`, and casting one to the other writes the
// wrong number of bytes rather than failing to compile.
#if defined(__ANDROID__) || defined(MU_IOS)
typedef socklen_t VOICE_SOCKLEN;
#else
typedef int VOICE_SOCKLEN;
#endif

static bool VoiceSetNonBlocking(SOCKET s)
{
#if defined(__ANDROID__) || defined(MU_IOS)
	const int flags = fcntl(s, F_GETFL, 0);

	if (flags < 0)
	{
		return false;
	}

	return (fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0);
#else
	u_long mode = 1;

	return (ioctlsocket(s, FIONBIO, &mode) == 0);
#endif
}

CVoiceClient::CVoiceClient() // OK
{
	this->m_Socket = INVALID_SOCKET;

	this->Init();
}

CVoiceClient::~CVoiceClient() // OK
{
	this->Release();
}

void CVoiceClient::Init() // OK
{
	std::lock_guard<std::mutex> lock(this->m_Lock);

	this->m_Enable = false;
	this->m_Host[0] = '\0';
	this->m_Port = 0;
	this->m_Token = 0;

	this->m_State = VOICE_CLIENT_OFF;
	this->m_LastHelloTick = 0;
	this->m_LastKeepAliveTick = 0;

	this->m_Socket = INVALID_SOCKET;
	memset(&this->m_ServerAddr, 0, sizeof(this->m_ServerAddr));

	this->m_EncodeState.Clear();
	this->m_SendSeq = 0;

	memset(this->m_Speaker, 0, sizeof(this->m_Speaker));

	this->m_FramesSent = 0;
	this->m_FramesRecv = 0;
	this->m_FramesLost = 0;

	this->m_LoopbackTest = false;
	this->m_LoopbackTick = 0;
	this->m_LoopbackPhase = 0;

	this->m_PlayerEnabled = true;
	this->m_MuteOthers = false;
	this->m_PlaybackVolume = 100;

	this->m_CaptureGain = 1.0f;
	this->m_SentLevel = 0;

	this->m_NoiseFloor = 0.0f;
	this->m_GateHoldUntil = 0;
	this->m_GateOpen = false;
}

void CVoiceClient::Release() // OK
{
	if (this->m_State == VOICE_CLIENT_READY)
	{
		this->SendBye();
	}

	std::lock_guard<std::mutex> lock(this->m_Lock);

	this->CloseSocket();

	this->m_State = VOICE_CLIENT_OFF;
}

// -------------------------------------------------------------------------
// socket
// -------------------------------------------------------------------------

void CVoiceClient::CloseSocket() // OK
{
	if (this->m_Socket != INVALID_SOCKET)
	{
		closesocket(this->m_Socket);
		this->m_Socket = INVALID_SOCKET;
	}
}

bool CVoiceClient::OpenSocket() // OK
{
	this->CloseSocket();

	if (this->m_Host[0] == '\0' || this->m_Port == 0)
	{
		return false;
	}

	memset(&this->m_ServerAddr, 0, sizeof(this->m_ServerAddr));

	this->m_ServerAddr.sin_family = AF_INET;
	this->m_ServerAddr.sin_port = htons(this->m_Port);
	this->m_ServerAddr.sin_addr.s_addr = inet_addr(this->m_Host);

	// inet_addr only handles a dotted quad. A name needs resolving, and this
	// deliberately matches how the rest of the codebase does it rather than
	// reaching for getaddrinfo, which is not available in every build here.
	if (this->m_ServerAddr.sin_addr.s_addr == INADDR_NONE)
	{
		struct hostent* pHost = gethostbyname(this->m_Host);

		if (pHost == NULL || pHost->h_addr_list[0] == NULL)
		{
			g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] cannot resolve %s", this->m_Host);
			return false;
		}

		memcpy(&this->m_ServerAddr.sin_addr, pHost->h_addr_list[0], sizeof(struct in_addr));
	}

	this->m_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (this->m_Socket == INVALID_SOCKET)
	{
		g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] socket failed (%d)", WSAGetLastError());
		return false;
	}

	// Non-blocking, because everything here runs on the game's frame. A
	// blocking recvfrom with nobody talking would stall the whole client.
	if (VoiceSetNonBlocking(this->m_Socket) == false)
	{
		g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] cannot set non-blocking (%d)", WSAGetLastError());
		this->CloseSocket();
		return false;
	}

	// No bind: the first sendto picks an ephemeral port, and the service learns
	// that endpoint from our HELLO. Binding a fixed port would only create a
	// collision when two clients share a machine.

	return true;
}

// -------------------------------------------------------------------------
// server info, from packet 0xD3:0x7A
// -------------------------------------------------------------------------

void CVoiceClient::SetServerInfo(BYTE bEnable, const char* szHost, WORD wPort, VQWORD Token) // OK
{
	if (szHost == NULL)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(this->m_Lock);

	// The GameServer resends this every second while a session exists, so the
	// common case is "nothing changed" and must stay cheap - and must not
	// restart the socket, which would re-HELLO fifty times a minute.
	if (this->m_Enable == (bEnable != 0)
		&& this->m_Port == wPort
		&& this->m_Token == Token
		&& strncmp(this->m_Host, szHost, sizeof(this->m_Host)) == 0)
	{
		return;
	}

	this->m_Enable = (bEnable != 0);
	this->m_Port = wPort;
	this->m_Token = Token;

	strncpy(this->m_Host, szHost, sizeof(this->m_Host) - 1);
	this->m_Host[sizeof(this->m_Host) - 1] = '\0';

	this->CloseSocket();

	memset(this->m_Speaker, 0, sizeof(this->m_Speaker));

	this->m_EncodeState.Clear();
	this->m_SendSeq = 0;

	if (this->m_Enable == false || this->m_Token == 0)
	{
		this->m_State = VOICE_CLIENT_OFF;

		g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] disabled by server");
		return;
	}

	// A fresh token is exactly what clears a rejection, so this is also the
	// recovery path out of VOICE_CLIENT_REJECTED.
	this->m_State = VOICE_CLIENT_CONNECTING;
	this->m_LastHelloTick = 0;

	g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] server %s:%u, connecting", this->m_Host, this->m_Port);
}

// -------------------------------------------------------------------------
// sending
// -------------------------------------------------------------------------

void CVoiceClient::SendHello() // OK
{
	if (this->m_Socket == INVALID_SOCKET)
	{
		return;
	}

	VOICE_C2V_HELLO_MSG msg;
	msg.Type = VOICE_C2V_HELLO;
	msg.Token = this->m_Token;

	sendto(this->m_Socket, (const char*)&msg, sizeof(msg), 0,
		(const sockaddr*)&this->m_ServerAddr, sizeof(this->m_ServerAddr));
}

void CVoiceClient::SendBye() // OK
{
	std::lock_guard<std::mutex> lock(this->m_Lock);

	if (this->m_Socket == INVALID_SOCKET)
	{
		return;
	}

	VOICE_C2V_BYE_MSG msg;
	msg.Type = VOICE_C2V_BYE;
	msg.Token = this->m_Token;

	sendto(this->m_Socket, (const char*)&msg, sizeof(msg), 0,
		(const sockaddr*)&this->m_ServerAddr, sizeof(this->m_ServerAddr));
}

// Assumes the lock is held. The capture path and the loopback test both come
// through here so there is only ever one copy of the encode-and-send sequence.
void CVoiceClient::SendFrameLocked(const short* pSamples) // OK
{
	// Frames are dropped outright until the service has acknowledged us. There
	// is nothing to gain from buffering speech nobody can route yet.
	if (this->m_State != VOICE_CLIENT_READY || this->m_Socket == INVALID_SOCKET)
	{
		return;
	}

	// The peak of this frame as the microphone gave it, before anything is done
	// to it. Both the gate and the levelling below key off this.
	int peak = 0;

	for (int n = 0; n < VOICE_FRAME_SAMPLES; n++)
	{
		const int a = (pSamples[n] < 0) ? -pSamples[n] : pSamples[n];

		if (a > peak)
		{
			peak = a;
		}
	}

	// ---- noise gate ------------------------------------------------------
	//
	// Runs BEFORE the gain, on the raw level. Gating after amplification would
	// be gating a signal the gate itself caused to be loud.
	if (VoiceSettingGetNoiseGate())
	{
		const DWORD now = GetTickCount();

		// Learned floor, not a fixed threshold: "quiet" on a headset and "quiet"
		// on a phone at arm's length differ by an order of magnitude, and a
		// constant would be wrong on most devices.
		//
		// Only updated while the gate is shut, so a long sentence cannot teach
		// the gate that speech is background.
		if (this->m_GateOpen == false)
		{
			if (this->m_NoiseFloor <= 0.0f)
			{
				this->m_NoiseFloor = (float)peak;
			}
			else
			{
				this->m_NoiseFloor += ((float)peak - this->m_NoiseFloor) * 0.05f;
			}

			if (this->m_NoiseFloor > 4000.0f)
			{
				// A ceiling, so a genuinely loud room cannot raise the floor far
				// enough to gate out speech entirely.
				this->m_NoiseFloor = 4000.0f;
			}
		}

		// Opens well clear of the floor, and the absolute term keeps a silent
		// room (floor near zero) from opening on a whisper of hiss.
		const float openAt = (this->m_NoiseFloor * 2.5f) + 300.0f;

		if ((float)peak >= openAt)
		{
			// Held open briefly after the level drops, so the quiet tail of a
			// word is not chopped off - the classic giveaway of a gate set with
			// no hold at all.
			this->m_GateHoldUntil = now + 300;
			this->m_GateOpen = true;
		}
		else if (this->m_GateOpen && (int)(now - this->m_GateHoldUntil) >= 0)
		{
			this->m_GateOpen = false;
		}

		if (this->m_GateOpen == false)
		{
			// Nothing sent at all. The frames are independent, so the listener's
			// decoder needs no closing frame, and their speaker slot simply ages
			// out. This is also where most of a session's bandwidth is saved.
			this->m_SentLevel = 0;
			return;
		}
	}
	else
	{
		this->m_GateOpen = true;
	}

	// ---- automatic input levelling --------------------------------------
	//
	// A quiet microphone at 8 kHz does not just sound faint: ADPCM has 4 bits
	// per sample to work with, and a signal using a tenth of the range throws
	// most of that away, so it arrives indistinct as well as quiet. Raising the
	// level before encoding is worth far more here than it would be at 16-bit.
	short boosted[VOICE_FRAME_SAMPLES];

	{
		// 60% of full scale. Aiming at 100% would clip on every transient;
		// leaving headroom means the loud parts of a sentence survive.
		const int target = 19600;

		// The gain is only chased on frames with actual signal in them. Below
		// this the frame is room noise or silence, and driving the gain up
		// there would amplify hiss and then slam it down the moment somebody
		// spoke - the classic pumping artefact.
		const int kSilenceFloor = 200;

		if (peak > kSilenceFloor)
		{
			float desired = (float)target / (float)peak;

			// Never below 1: attenuation belongs to playback, where distance
			// is already being applied. Capped at 10 so a dead microphone
			// cannot be amplified into pure noise.
			if (desired < 1.0f)
			{
				desired = 1.0f;
			}

			if (desired > 10.0f)
			{
				desired = 10.0f;
			}

			// Asymmetric on purpose. Coming DOWN is fast, because the cost of
			// being slow is clipping the start of a shout. Going UP is slow, so
			// the level does not visibly breathe between words.
			if (desired < this->m_CaptureGain)
			{
				this->m_CaptureGain += (desired - this->m_CaptureGain) * 0.40f;
			}
			else
			{
				this->m_CaptureGain += (desired - this->m_CaptureGain) * 0.03f;
			}
		}

		// The player's own multiplier on top, for a microphone so quiet that
		// the automatic ceiling above is not enough.
		float gain = this->m_CaptureGain * ((float)VoiceSettingGetMicBoost() / 100.0f);

		if (gain < 1.0f)
		{
			gain = 1.0f;
		}

		int sentPeak = 0;

		for (int n = 0; n < VOICE_FRAME_SAMPLES; n++)
		{
			int v = (int)((float)pSamples[n] * gain);

			// Hard clamp. A limiter would sound better, but clipping a rare
			// transient is a far smaller problem than the whole signal being
			// too quiet to understand, which is what this is fixing.
			if (v > 32767)
			{
				v = 32767;
			}

			if (v < -32768)
			{
				v = -32768;
			}

			boosted[n] = (short)v;

			const int a = (v < 0) ? -v : v;

			if (a > sentPeak)
			{
				sentPeak = a;
			}
		}

		this->m_SentLevel = sentPeak >> 7;   // 0-32767 down to 0-255
	}

	VOICE_C2V_FRAME_MSG msg;
	msg.Type = VOICE_C2V_FRAME;
	msg.Token = this->m_Token;
	msg.Seq = this->m_SendSeq++;

	const int written = VoiceEncodeFrame(&this->m_EncodeState, boosted, VOICE_FRAME_SAMPLES,
		msg.Data, VOICE_FRAME_BYTES);

	if (written != VOICE_FRAME_BYTES)
	{
		return;
	}

	msg.Length = (VBYTE)written;

	sendto(this->m_Socket, (const char*)&msg, sizeof(msg), 0,
		(const sockaddr*)&this->m_ServerAddr, sizeof(this->m_ServerAddr));

	this->m_FramesSent++;
}

void CVoiceClient::SendCaptureFrame(const short* pSamples) // OK
{
	if (pSamples == NULL)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(this->m_Lock);

	this->SendFrameLocked(pSamples);
}

// -------------------------------------------------------------------------
// receiving
// -------------------------------------------------------------------------

void CVoiceClient::PumpReceive() // OK
{
	if (this->m_Socket == INVALID_SOCKET)
	{
		return;
	}

	// Capped rather than draining to empty. A flood - hostile or just a very
	// loud siege - must cost a bounded amount of this frame, and anything left
	// is read next frame or dropped by the kernel, which for voice is correct.
	for (int loop = 0; loop < 64; loop++)
	{
		VBYTE buffer[VOICE_MAX_PACKET];

		sockaddr_in from;
		memset(&from, 0, sizeof(from));

		VOICE_SOCKLEN fromLength = (VOICE_SOCKLEN)sizeof(from);

		const int received = recvfrom(this->m_Socket, (char*)buffer, sizeof(buffer), 0,
			(sockaddr*)&from, &fromLength);

		if (received <= 0)
		{
			break;
		}

		// Only the service talks to this socket. Anything from elsewhere is
		// either stray or somebody trying their luck.
		if (from.sin_addr.s_addr != this->m_ServerAddr.sin_addr.s_addr
			|| from.sin_port != this->m_ServerAddr.sin_port)
		{
			continue;
		}

		std::lock_guard<std::mutex> lock(this->m_Lock);

		this->HandlePacket(buffer, received);
	}
}

void CVoiceClient::HandlePacket(const VBYTE* pBuffer, int iLength) // OK
{
	if (iLength < (int)sizeof(VOICE_HEADER))
	{
		return;
	}

	switch (pBuffer[0])
	{
	case VOICE_V2C_HELLO_OK:
		{
			if (this->m_State != VOICE_CLIENT_READY)
			{
				this->m_State = VOICE_CLIENT_READY;

				g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] session accepted");
			}
		}
		break;

	case VOICE_V2C_REJECT:
		{
			if (iLength < (int)sizeof(VOICE_V2C_REJECT_MSG))
			{
				break;
			}

			const VOICE_V2C_REJECT_MSG* lpMsg = (const VOICE_V2C_REJECT_MSG*)pBuffer;

			// Stop rather than retry. An unknown token stays unknown however
			// many times we ask; the way out is a new token from the
			// GameServer, which arrives on its own.
			this->m_State = VOICE_CLIENT_REJECTED;

			g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] rejected (reason %d)", lpMsg->Reason);
		}
		break;

	case VOICE_V2C_FRAME:
		{
			if (iLength < (int)(sizeof(VOICE_V2C_FRAME_MSG) - VOICE_FRAME_BYTES))
			{
				break;
			}

			this->HandleFrameIn((const VOICE_V2C_FRAME_MSG*)pBuffer);
		}
		break;
	}
}

void CVoiceClient::HandleFrameIn(const VOICE_V2C_FRAME_MSG* lpMsg) // OK
{
	if (lpMsg->Length > VOICE_FRAME_BYTES)
	{
		return;
	}

	const int slot = this->AcquireSpeaker(lpMsg->SpeakerIndex);

	if (slot < 0)
	{
		return;
	}

	VOICE_SPEAKER* pSpeaker = &this->m_Speaker[slot];

	// Out-of-order arrival. Signed 16-bit difference so the comparison keeps
	// working across the wrap at 65535, which happens every 22 minutes of
	// continuous talking.
	if (pSpeaker->HasSeq)
	{
		const short delta = (short)(lpMsg->Seq - pSpeaker->LastSeq);

		if (delta <= 0)
		{
			// Late. Playing it now would put it after audio that already went
			// out, which sounds worse than the gap it was meant to fill.
			this->m_FramesLost++;
			return;
		}

		if (delta > 1)
		{
			this->m_FramesLost += (DWORD)(delta - 1);
		}
	}

	pSpeaker->LastSeq = lpMsg->Seq;
	pSpeaker->HasSeq = true;
	pSpeaker->Volume = lpMsg->Volume;
	pSpeaker->LastFrameTick = GetTickCount();

	short pcm[VOICE_FRAME_SAMPLES];

	// No per-speaker decoder state: every frame carries its own, which is what
	// makes the gap above survivable rather than turning the rest of the
	// sentence into noise. See VoiceCodec.h.
	if (VoiceDecodeFrame(lpMsg->Data, lpMsg->Length, pcm, VOICE_FRAME_SAMPLES) != VOICE_FRAME_SAMPLES)
	{
		return;
	}

	// Full buffer means playback is not keeping up, or this speaker is ahead
	// of us. Drop the OLDEST frame, not the newest - latency stays bounded and
	// what the listener hears is the most recent thing said.
	if ((pSpeaker->Fill + VOICE_FRAME_SAMPLES) > VOICE_JITTER_SAMPLES)
	{
		pSpeaker->Read = (pSpeaker->Read + VOICE_FRAME_SAMPLES) % VOICE_JITTER_SAMPLES;
		pSpeaker->Fill -= VOICE_FRAME_SAMPLES;
	}

	for (int n = 0; n < VOICE_FRAME_SAMPLES; n++)
	{
		pSpeaker->Pcm[pSpeaker->Write] = pcm[n];
		pSpeaker->Write = (pSpeaker->Write + 1) % VOICE_JITTER_SAMPLES;
	}

	pSpeaker->Fill += VOICE_FRAME_SAMPLES;

	this->m_FramesRecv++;
}

// -------------------------------------------------------------------------
// speaker table - both assume the lock is held
// -------------------------------------------------------------------------

int CVoiceClient::FindSpeaker(VWORD wPlayerIndex) // OK
{
	for (int n = 0; n < VOICE_MAX_SPEAKERS; n++)
	{
		if (this->m_Speaker[n].Used && this->m_Speaker[n].PlayerIndex == wPlayerIndex)
		{
			return n;
		}
	}

	return -1;
}

int CVoiceClient::AcquireSpeaker(VWORD wPlayerIndex) // OK
{
	const int existing = this->FindSpeaker(wPlayerIndex);

	if (existing >= 0)
	{
		return existing;
	}

	for (int n = 0; n < VOICE_MAX_SPEAKERS; n++)
	{
		if (this->m_Speaker[n].Used == false)
		{
			memset(&this->m_Speaker[n], 0, sizeof(VOICE_SPEAKER));

			this->m_Speaker[n].Used = true;
			this->m_Speaker[n].PlayerIndex = wPlayerIndex;

			return n;
		}
	}

	// Every slot busy. The new voice is simply not heard; stealing a slot
	// would cut someone off mid-word to start someone else mid-word.
	return -1;
}

void CVoiceClient::ExpireSpeakers(DWORD dwNow) // OK
{
	for (int n = 0; n < VOICE_MAX_SPEAKERS; n++)
	{
		if (this->m_Speaker[n].Used == false)
		{
			continue;
		}

		const DWORD quietFor = dwNow - this->m_Speaker[n].LastFrameTick;

		// Normally kept until the buffer has drained as well as gone quiet, so
		// the tail of the last word is not cut off.
		if (this->m_Speaker[n].Fill > 0 && quietFor < 5000)
		{
			continue;
		}

		// The 5 second ceiling is not about tidiness. Nothing drains these
		// buffers except playback, so if playback is not running - no audio
		// backend yet, a device that failed to open, a stalled callback - then
		// waiting for Fill to reach zero waits forever, all sixteen slots fill
		// up, and nobody new can ever be heard. Time has to win over drain.
		if (quietFor > 2000)
		{
			memset(&this->m_Speaker[n], 0, sizeof(VOICE_SPEAKER));
		}
	}
}

// -------------------------------------------------------------------------
// playback mix
// -------------------------------------------------------------------------

void CVoiceClient::PullPlayback(short* pOut, int iCount) // OK
{
	if (pOut == NULL || iCount <= 0)
	{
		return;
	}

	// Always writes the full count, silence included, so the audio backend
	// never has to special-case "nobody is talking".
	memset(pOut, 0, sizeof(short) * (size_t)iCount);

	std::lock_guard<std::mutex> lock(this->m_Lock);

	for (int n = 0; n < VOICE_MAX_SPEAKERS; n++)
	{
		VOICE_SPEAKER* pSpeaker = &this->m_Speaker[n];

		if (pSpeaker->Used == false || pSpeaker->Fill <= 0)
		{
			continue;
		}

		// Muted: the buffer is still drained, deliberately. Skipping the drain
		// would let it fill up and hold the slot open, and then unmuting would
		// play a backlog of things said while the player was not listening.
		if (this->m_MuteOthers)
		{
			int discard = pSpeaker->Fill;

			if (discard > iCount)
			{
				discard = iCount;
			}

			pSpeaker->Read = (pSpeaker->Read + discard) % VOICE_JITTER_SAMPLES;
			pSpeaker->Fill -= discard;
			continue;
		}

		int available = pSpeaker->Fill;

		if (available > iCount)
		{
			available = iCount;
		}

		for (int s = 0; s < available; s++)
		{
			const int sample = (int)pSpeaker->Pcm[pSpeaker->Read];

			pSpeaker->Read = (pSpeaker->Read + 1) % VOICE_JITTER_SAMPLES;

			// Distance attenuation, then the player's own volume. Integer
			// throughout, because this runs inside an audio callback where a
			// float divide per sample per speaker adds up.
			int scaled = (sample * (int)pSpeaker->Volume) >> 8;

			if (this->m_PlaybackVolume < 100)
			{
				scaled = (scaled * this->m_PlaybackVolume) / 100;
			}

			int mixed = (int)pOut[s] + scaled;

			// Summing several speakers can exceed the sample range. Clipping
			// is the wrong-sounding option but the right one here: the
			// alternative is ducking everyone whenever a second person talks.
			if (mixed > 32767)
			{
				mixed = 32767;
			}

			if (mixed < -32768)
			{
				mixed = -32768;
			}

			pOut[s] = (short)mixed;
		}

		pSpeaker->Fill -= available;
	}
}

// -------------------------------------------------------------------------
// per-frame
// -------------------------------------------------------------------------

void CVoiceClient::Proc() // OK
{
	const DWORD now = GetTickCount();

	// recvfrom stays outside the lock; HandlePacket takes it per packet.
	this->PumpReceive();

	std::lock_guard<std::mutex> lock(this->m_Lock);

	// Copied here, on the game thread, so the audio callback never has to
	// reach into GameConfig.
	const bool wasPlayerEnabled = this->m_PlayerEnabled;

	this->m_PlayerEnabled = VoiceSettingGetEnabled();
	this->m_MuteOthers = VoiceSettingGetMuteOthers();
	this->m_PlaybackVolume = VoiceSettingGetVolume();

	if (this->m_Enable == false || this->m_State == VOICE_CLIENT_OFF || this->m_State == VOICE_CLIENT_REJECTED)
	{
		return;
	}

	// Opted out. The socket closes and the speakers are dropped, but the token
	// and address are kept, so switching back on reconnects without waiting for
	// the GameServer to notice and reissue.
	if (this->m_PlayerEnabled == false)
	{
		if (this->m_Socket != INVALID_SOCKET)
		{
			this->CloseSocket();

			memset(this->m_Speaker, 0, sizeof(this->m_Speaker));

			this->m_State = VOICE_CLIENT_CONNECTING;
			this->m_LastHelloTick = 0;

			g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] off (player setting)");
		}

		return;
	}

	if (wasPlayerEnabled == false)
	{
		g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] on (player setting)");
	}

	if (this->m_Socket == INVALID_SOCKET)
	{
		if (this->OpenSocket() == false)
		{
			// Back off rather than hammering a name that will not resolve.
			this->m_LastHelloTick = now;
			return;
		}
	}

	if (this->m_State == VOICE_CLIENT_CONNECTING)
	{
		if (this->m_LastHelloTick == 0 || (now - this->m_LastHelloTick) > 1000)
		{
			this->SendHello();
			this->m_LastHelloTick = now;

			// Counts as a keepalive too. Without this the first tick after
			// HELLO_OK sees a zero timestamp and fires a redundant HELLO.
			this->m_LastKeepAliveTick = now;
		}

		return;
	}

	// See m_LastKeepAliveTick: this is about the NAT mapping, not the session.
	if ((now - this->m_LastKeepAliveTick) > 15000)
	{
		this->SendHello();
		this->m_LastKeepAliveTick = now;
	}

	this->ExpireSpeakers(now);

	this->ProcLoopbackTest(now);
}

bool CVoiceClient::IsSpeaking(int iPlayerIndex) // OK
{
	std::lock_guard<std::mutex> lock(this->m_Lock);

	const int slot = this->FindSpeaker((VWORD)iPlayerIndex);

	if (slot < 0)
	{
		return false;
	}

	return ((GetTickCount() - this->m_Speaker[slot].LastFrameTick) < VOICE_SPEAKING_HOLD_MS);
}

int CVoiceClient::GetSpeakingCount() // OK
{
	std::lock_guard<std::mutex> lock(this->m_Lock);

	const DWORD now = GetTickCount();

	int count = 0;

	for (int n = 0; n < VOICE_MAX_SPEAKERS; n++)
	{
		if (this->m_Speaker[n].Used == false)
		{
			continue;
		}

		if ((now - this->m_Speaker[n].LastFrameTick) < VOICE_SPEAKING_HOLD_MS)
		{
			count++;
		}
	}

	return count;
}

// -------------------------------------------------------------------------
// Phase 3 verification - a synthetic speaker
// -------------------------------------------------------------------------

void CVoiceClient::SetLoopbackTest(bool bOn) // OK
{
	std::lock_guard<std::mutex> lock(this->m_Lock);

	this->m_LoopbackTest = bOn;
	this->m_LoopbackTick = 0;
	this->m_LoopbackPhase = 0;

	g_ConsoleDebug->Write(MCD_RECEIVE, "[Voice] loopback test %s", bOn ? "ON" : "OFF");
}

void CVoiceClient::ProcLoopbackTest(DWORD dwNow) // OK
{
	if (this->m_LoopbackTest == false)
	{
		return;
	}

	if (this->m_LoopbackTick == 0)
	{
		this->m_LoopbackTick = dwNow;
	}

	// Catch up in whole frames, so the rate is 50/s regardless of frame rate,
	// and cap the catch-up so a long stall does not produce a burst.
	int frames = (int)((dwNow - this->m_LoopbackTick) / 20);

	if (frames <= 0)
	{
		return;
	}

	if (frames > 5)
	{
		frames = 5;
	}

	this->m_LoopbackTick = dwNow;

	for (int f = 0; f < frames; f++)
	{
		short samples[VOICE_FRAME_SAMPLES];

		for (int n = 0; n < VOICE_FRAME_SAMPLES; n++)
		{
			// 400 Hz at 8 kHz is 20 samples per cycle. A square rather than a
			// sine keeps this free of any math dependency and is unmistakable
			// on the other end.
			samples[n] = ((this->m_LoopbackPhase % 20) < 10) ? 8000 : -8000;

			this->m_LoopbackPhase++;
		}

		// Not SendCaptureFrame: this runs from Proc, which already holds the
		// lock, and std::mutex is not recursive.
		this->SendFrameLocked(samples);
	}
}

// -------------------------------------------------------------------------
// speaking indicator
// -------------------------------------------------------------------------

// A small marker over the head of everyone currently talking.
//
// Worth having for a reason beyond decoration: proximity voice with no visual
// cue means a disembodied voice with no idea who it belongs to, and on a busy
// screen that is worse than no voice at all.
//
// The speaker's game index travels in every audio frame precisely so this can
// be drawn without a lookup on the wire - only the local one from index to
// character slot, below.
void RenderVoiceSpeakingMarks() // OK
{
	if (gVoiceClient.IsEnabled() == false || gVoiceClient.IsPlayerEnabled() == false)
	{
		return;
	}

	if (gVoiceClient.GetSpeakingCount() <= 0)
	{
		return;
	}

	for (int i = 0; i < MAX_CHARACTERS_CLIENT; i++)
	{
		CHARACTER* c = &CharactersClient[i];
		OBJECT* o = &c->Object;

		if (o->Live == false || o->Kind != KIND_PLAYER)
		{
			continue;
		}

		if (gVoiceClient.IsSpeaking(c->Key) == false)
		{
			continue;
		}

		// Above the name plate rather than beside it, so it does not collide
		// with whatever else is already over that character's head.
		vec3_t Position;
		int ScreenX = 0;
		int ScreenY = 0;

		Vector(o->Position[0], o->Position[1], o->Position[2] + o->BoundingBoxMax[2] + 140.f, Position);
		Projection(Position, &ScreenX, &ScreenY);

		EnableAlphaTest();

		// Three stacked bars, brightest at the bottom - a speaker glyph drawn
		// out of the primitives this client already has, rather than a new
		// texture that would have to be authored, shipped and UV-mapped.
		glColor4f(0.1f, 0.1f, 0.1f, 0.6f);
		RenderColor((float)(ScreenX - 7), (float)(ScreenY - 1), 14.f, 12.f);

		glColor4f(0.4f, 1.0f, 0.4f, 1.0f);
		RenderColor((float)(ScreenX - 5), (float)(ScreenY + 6), 3.f, 4.f);

		glColor4f(0.4f, 1.0f, 0.4f, 0.85f);
		RenderColor((float)(ScreenX - 1), (float)(ScreenY + 3), 3.f, 7.f);

		glColor4f(0.4f, 1.0f, 0.4f, 0.7f);
		RenderColor((float)(ScreenX + 3), (float)(ScreenY), 3.f, 10.f);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

// -------------------------------------------------------------------------
// on-screen readout
// -------------------------------------------------------------------------

// Shows only what a player can act on, and only when there is something to
// say. A permanent frame counter in the corner of a live client is clutter;
// "your microphone is not working" is not.
void RenderVoiceChatDebug() // OK
{
	const bool bKeyHeld = g_VoicePttKeyHeld;

	// Silent unless the server has voice, OR the player is actually holding the
	// talk key. That second half is the whole point of this function: pressing
	// the key must always produce SOME feedback, especially when voice cannot
	// work at all. Without it, "voice is off on this server" and "my keybind is
	// dead" look exactly the same - nothing happens - and they lead to
	// completely different places.
	if (gVoiceClient.IsEnabled() == false && bKeyHeld == false && g_VoicePttBlockedByChat == false)
	{
		return;
	}

	const int x = 10;
	int y = 10;

	const bool bLive = gVoiceAudio.IsTalking();

	// While the key is down, something unmistakable. A red block reading MIC
	// LIVE, because "am I currently broadcasting" is the one thing here that
	// must never be ambiguous.
	if (bKeyHeld || bLive)
	{
		EnableAlphaTest();

		if (bLive)
		{
			glColor4f(0.85f, 0.15f, 0.15f, 0.9f);
		}
		else
		{
			// Key down but not transmitting - grey, so it is visibly not the
			// same state as live.
			glColor4f(0.35f, 0.35f, 0.38f, 0.85f);
		}

		RenderColor((float)x, (float)y, 12.f, 12.f);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		g_pRenderText->SetTextColor(255, 235, 225, 255);
		g_pRenderText->SetBgColor(0, 0, 0, 170);

		if (bLive)
		{
			g_pRenderText->RenderText(x + 16, y,
				gVoiceClient.GetLoopbackTest() ? "MIC LIVE (test tone)" : "MIC LIVE");
		}
		else
		{
			// The key registered. Whatever is wrong is below, not in the
			// keyboard.
			g_pRenderText->RenderText(x + 16, y, "talk key held");
		}

		y += 16;
	}

	// The level meter, only while actually transmitting. This is the single
	// most useful instrument here: it separates a microphone that is muted or
	// unplugged from a network that is not carrying anything, which look
	// identical from the player's side.
	if (bLive)
	{
		// The level SENT, not the level heard. Auto-levelling means those are
		// far apart on a quiet microphone, and what the player needs to know is
		// whether anything usable is going out.
		const int level = gVoiceClient.GetSentLevel();

		// The raw device level still matters for one specific case: zero here
		// means the microphone itself is giving nothing, which no amount of
		// gain will fix and which reads completely differently from "quiet".
		const int rawLevel = gVoiceAudio.GetCaptureLevel();

		EnableAlphaTest();

		glColor4f(0.1f, 0.1f, 0.1f, 0.6f);
		RenderColor((float)x, (float)y, 104.f, 12.f);

		if (level > 0)
		{
			// Green until it is close to clipping, then red - a meter that is
			// always one colour says nothing about whether the level is good.
			if (level > 220)
			{
				glColor4f(1.0f, 0.3f, 0.2f, 1.0f);
			}
			else
			{
				glColor4f(0.3f, 1.0f, 0.3f, 1.0f);
			}

			RenderColor((float)(x + 2), (float)(y + 2), (float)((level * 100) / 255), 8.f);
		}

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		if (rawLevel <= 0)
		{
			// Nothing at all from the device. Distinct from "quiet" - this is
			// the muted, unplugged or permission-denied case.
			g_pRenderText->SetTextColor(255, 200, 160, 255);
			g_pRenderText->SetBgColor(0, 0, 0, 170);
			g_pRenderText->RenderText(x + 110, y, "no input");
		}
		else
		{
			// What the levelling had to do to get there. A player being told
			// they are at 600% knows to move closer or raise the boost, rather
			// than guessing.
			char szGain[48];
			sprintf(szGain, "x%d%%", gVoiceClient.GetAutoGainPercent());

			g_pRenderText->SetTextColor(190, 190, 190, 255);
			g_pRenderText->SetBgColor(0, 0, 0, 140);
			g_pRenderText->RenderText(x + 110, y, szGain);
		}

		y += 16;
	}

	// Why nothing is happening. Ordered so the most actionable cause wins:
	// there is no point saying "others are muted" to somebody whose server has
	// voice switched off.
	char Text[192];
	Text[0] = '\0';

	if (g_VoicePttBlockedByChat)
	{
		strcpy(Text, "Voice: close the chat box to talk");
	}
	else if (gVoiceClient.IsEnabled() == false)
	{
		// Only reachable while the key is held, per the guard at the top.
		strcpy(Text, "Voice: not enabled on this server");
	}
	else if (gVoiceClient.IsPlayerEnabled() == false)
	{
		strcpy(Text, "Voice: off - turn on Voice Chat in Options");
	}
	else if (gVoiceClient.GetState() == VOICE_CLIENT_REJECTED)
	{
		strcpy(Text, "Voice: server refused the session");
	}
	else if (gVoiceClient.GetState() != VOICE_CLIENT_READY)
	{
		strcpy(Text, "Voice: connecting...");
	}
	else if (gVoiceAudio.GetLastError()[0] != '\0')
	{
		sprintf(Text, "Voice: %s", gVoiceAudio.GetLastError());
	}
	else if (gVoiceClient.IsMutingOthers())
	{
		strcpy(Text, "Voice: others muted");
	}

	if (Text[0] != '\0')
	{
		g_pRenderText->SetTextColor(255, 200, 160, 255);
		g_pRenderText->SetBgColor(0, 0, 0, 170);
		g_pRenderText->RenderText(x, y, Text);
	}
}
