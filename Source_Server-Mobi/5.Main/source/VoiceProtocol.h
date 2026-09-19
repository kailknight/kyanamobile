// VoiceProtocol.h - the wire format for proximity voice chat.
//
// Three parties speak this, all over ONE UDP port on the voice service:
//
//   client        -> service : HELLO to claim a session, FRAME to talk, BYE to leave
//   GameServer    -> service : SESSION / POS / DROP, the control feed
//   service       -> client  : HELLO_OK, and FRAME for every speaker in range
//
// Deliberately kept off the GameServer's TCP connection. Voice is 20-50 small
// packets a second per speaker; putting that on the game socket means TCP
// head-of-line blocking stalls combat behind a lost audio packet, and the
// IOCP workers that run the world end up scheduling audio.
//
// THIS FILE IS DUPLICATED, like the game's other packet structs: the canonical
// copy lives here, mirrored into the GameServer and the client. All three must
// be rebuilt together when it changes - there is no version negotiation.

#pragma once

// Fixed-width types without dragging in the game headers, so the voice service
// stays a standalone console app.
//
// `unsigned long long` rather than MSVC's `__int64`, because the client copy of
// this file is compiled by the NDK's clang too, where `__int64` only exists via
// a typedef in PlatformDefs.h - and a wire struct that depends on include order
// to have the right size is exactly the bug this codebase keeps paying for.
// `long long` is 8 bytes on MSVC x86 and on every Android ABI. See
// mu-android-long-abi-bug: bare `long` is 4 bytes on PC and 8 on Android.
typedef unsigned char       VBYTE;
typedef unsigned short      VWORD;
typedef unsigned int        VDWORD;
typedef unsigned long long  VQWORD;

// One frame of audio. 8000 Hz mono, 20ms per frame = 160 samples.
//
// IMA ADPCM packs 2 samples per byte, so a frame is 80 bytes plus the 4-byte
// codec state = 84 on the wire, about 34 kbit/s while a player is actually
// talking. Push-to-talk means that is only while the key is held.
//
// The codec sits behind VoiceCodec.h; swapping ADPCM for Opus later changes
// VOICE_FRAME_BYTES and nothing else in this file.
#define VOICE_SAMPLE_RATE       8000
#define VOICE_FRAME_SAMPLES     160
#define VOICE_FRAME_BYTES       84

// Generous ceiling for one datagram. Nothing here approaches it; it exists so
// a malformed length field cannot make the service read past its buffer.
#define VOICE_MAX_PACKET        512

// Room for a name plus terminator, matching the game's own MAX_ACCOUNT_NAME.
#define VOICE_NAME_LENGTH       11

enum eVoicePacketType
{
    // client -> service
    VOICE_C2V_HELLO     = 1,
    VOICE_C2V_FRAME     = 2,
    VOICE_C2V_BYE       = 3,

    // service -> client
    VOICE_V2C_HELLO_OK  = 10,
    VOICE_V2C_FRAME     = 11,
    VOICE_V2C_REJECT    = 12,

    // GameServer -> service
    VOICE_G2V_SESSION   = 20,
    VOICE_G2V_POS       = 21,
    VOICE_G2V_DROP      = 22,
};

#pragma pack(push, 1)

// Every packet starts with this, so a receiver can switch before trusting
// anything else about the buffer.
struct VOICE_HEADER
{
    VBYTE Type;
};

// -------------------------------------------------------------------------
// client -> service
// -------------------------------------------------------------------------

// Claims a session and, just as importantly, tells the service which
// UDP endpoint this player is speaking from. The service cannot know that
// until the client sends something - the GameServer only knows the player's
// TCP address, which is not necessarily the same source port.
struct VOICE_C2V_HELLO_MSG
{
    VBYTE  Type;
    VQWORD Token;
};

struct VOICE_C2V_FRAME_MSG
{
    VBYTE  Type;
    VQWORD Token;

    // Wraps freely. Used only to drop frames that arrive out of order; there
    // is no retransmission and no reordering buffer, because a late voice
    // frame is worth less than the silence it would fill.
    VWORD  Seq;

    VBYTE  Length;
    VBYTE  Data[VOICE_FRAME_BYTES];
};

struct VOICE_C2V_BYE_MSG
{
    VBYTE  Type;
    VQWORD Token;
};

// -------------------------------------------------------------------------
// service -> client
// -------------------------------------------------------------------------

struct VOICE_V2C_HELLO_OK_MSG
{
    VBYTE Type;
};

// Why the session was refused, so the client can say something useful instead
// of failing silently.
enum eVoiceRejectReason
{
    VOICE_REJECT_UNKNOWN_TOKEN = 0,   // no session, or it expired
    VOICE_REJECT_SERVER_FULL   = 1,
};

struct VOICE_V2C_REJECT_MSG
{
    VBYTE Type;
    VBYTE Reason;
};

struct VOICE_V2C_FRAME_MSG
{
    VBYTE  Type;

    // Who is talking. The game index, so the client can put a speaking
    // indicator over the right character without another lookup.
    VWORD  SpeakerIndex;

    // Distance attenuation, 0-255, computed by the service.
    //
    // Done server-side on purpose: the listener may not have the speaker in
    // their viewport at all - behind a wall, just out of render range - and a
    // client that cannot see the speaker has no position to attenuate from.
    VBYTE  Volume;

    VWORD  Seq;
    VBYTE  Length;
    VBYTE  Data[VOICE_FRAME_BYTES];
};

// -------------------------------------------------------------------------
// GameServer -> service
// -------------------------------------------------------------------------

// Issues or refreshes a session. Sent when a player enters the world; the
// same token goes to the client down the game connection, and the client
// presents it in HELLO.
struct VOICE_G2V_SESSION_MSG
{
    VBYTE  Type;
    VQWORD Token;
    VWORD  PlayerIndex;
    char   Name[VOICE_NAME_LENGTH];
};

// Where everyone is. The service needs this to answer "who is in range", and
// it is the only thing keeping a session alive - a player who stops appearing
// in this feed has left, so their session times out on its own and no
// disconnect packet can be missed.
struct VOICE_G2V_POS_MSG
{
    VBYTE  Type;
    VWORD  PlayerIndex;
    VBYTE  Map;
    VBYTE  X;
    VBYTE  Y;
};

struct VOICE_G2V_DROP_MSG
{
    VBYTE  Type;
    VWORD  PlayerIndex;
};

#pragma pack(pop)
