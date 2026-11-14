// KingOfKiraProtocol.h - wire structs for King of Kira (1.0.50).
// IDENTICAL copy in 2.DataServer\DataServer and 4.GameServer\GameServer: keep them byte-for-byte equal.
// All structs packed, fixed-width types only, sizes asserted.
#pragma once

#pragma pack(push,1)

#define KOK_HEAD 0xD9
#define KOK_SUB_RELAY  0x60 // GS -> DS -> GS(s)
#define KOK_SUB_SAVE   0x61 // GS(host) -> DS: a king was crowned
#define KOK_SUB_LOAD   0x62 // GS -> DS: send me the current king
#define KOK_SUB_WINNER 0x63 // DS -> GS: the current king (reply to LOAD, broadcast after SAVE)
#define KOK_TARGET_ALL 0xFFFF
#define KOK_RELAY_DATA 160

enum eKokRelayKind
{
	KOK_RELAY_REGISTER = 1, // GS(any) -> host: Data = KOK_REG_DATA
	KOK_RELAY_REGREPLY = 2, // host -> GS: Data = KOK_REGREPLY_DATA
	KOK_RELAY_STATE    = 3, // host -> all: Data = KOK_STATE_DATA (about once a second while active, and on every change)
	KOK_RELAY_LIST     = 4, // host -> all: Data = KOK_ENTRY_DATA, one packet per registered guild (Total = 0 when none)
};

struct SDHP_KOK_RELAY
{
	BYTE type;      // 0xC1
	BYTE size;
	BYTE head;      // 0xD9
	BYTE subh;      // 0x60
	WORD Target;    // KOK_TARGET_ALL or a server code
	WORD FromServer;
	BYTE Kind;
	BYTE Len;
	BYTE Data[KOK_RELAY_DATA];
};

#define KOK_MAX_ALLY 5
#define KOK_MAX_ENTRIES 64

// One registered main guild with its alliance (names snapshotted when the master registers).
struct KOK_ENTRY_DATA
{
	WORD Index;       // position in the list (KOK_RELAY_LIST)
	WORD Total;       // how many guilds are registered (0 = the list is empty)
	int Key;          // alliance key: Guild->GuildUnion, or the guild's own number when it has no alliance
	char Guild[9];    // main guild name, NUL terminated
	BYTE AllyCount;
	char Allies[KOK_MAX_ALLY][9]; // allied guild names, NUL terminated ("" never counted)
};

struct KOK_REG_DATA
{
	WORD aIndex;      // requester's index on his own server
	BYTE Cancel;      // 1 = withdraw the registration
	BYTE Pad;
	char Name[11];    // character name (guild master)
	KOK_ENTRY_DATA Entry;
};

struct KOK_REGREPLY_DATA
{
	WORD aIndex;
	BYTE Result;      // see eKokRegResult
	BYTE Pad;
	char Name[11];
	char Guild[9];
};

enum eKokRegResult
{
	KOK_REG_OK = 0,
	KOK_REG_CLOSED = 1,       // not in a registration window
	KOK_REG_FULL = 2,         // two guilds already registered
	KOK_REG_ALREADY = 3,      // this guild is registered
	KOK_REG_ALLIED = 4,       // this guild is allied to a registered guild
	KOK_REG_NOTMASTER = 5,
	KOK_REG_NOGUILD = 6,
	KOK_REG_CANCELLED = 7,    // registration withdrawn
	KOK_REG_DISABLED = 8,
	KOK_REG_LIMIT = 9,        // the list is full (64)
};

struct KOK_STATE_DATA
{
	BYTE State;        // eKokState
	BYTE Contested;    // 1 = two masters on the hill, counting paused
	WORD RemainSec;    // seconds left in this state (0 when not timed)
	char Guild1[9];    // registered / fighting main guilds, "" when empty
	char Guild2[9];
	WORD Points1;
	WORD Points2;
	char OnHill[9];    // guild whose master stands on the hill, "" none
	int Key1;          // alliance keys of the two guilds (see KOK_REG_DATA.Key); a player is a participant when his own key matches
	int Key2;
};

enum eKokState
{
	KOK_STATE_BLANK = 0,    // off
	KOK_STATE_IDLE = 1,     // waiting for the next start time
	KOK_STATE_REGISTER = 2, // registration open, countdown running
	KOK_STATE_FIGHT = 3,    // event running
	KOK_STATE_END = 4,      // finished, players being sent out
	KOK_STATE_LOCKED = 5,   // registration closed, waiting for the start
};

struct SDHP_KOK_SAVE
{
	BYTE type; BYTE size; BYTE head; BYTE subh; // C1 D9 61
	char Winner[9];
	char Loser[9];
	WORD WinnerPoints;
	WORD LoserPoints;
	int WinnerKey;
};

struct SDHP_KOK_LOAD
{
	BYTE type; BYTE size; BYTE head; BYTE subh; // C1 D9 62
	WORD ServerCode;
};

struct SDHP_KOK_WINNER
{
	BYTE type; BYTE size; BYTE head; BYTE subh; // C1 D9 63
	BYTE Has;          // 0 = no king yet
	char Guild[9];
	BYTE Mark[32];     // Guild.G_Mark
	int Key;           // alliance key of the winner when it was crowned
};

static_assert(sizeof(SDHP_KOK_RELAY) == 4+2+2+1+1+KOK_RELAY_DATA,"KOK relay size");
static_assert(sizeof(KOK_REG_DATA) <= KOK_RELAY_DATA,"KOK reg size");
static_assert(sizeof(KOK_ENTRY_DATA) <= KOK_RELAY_DATA,"KOK entry size");
static_assert(sizeof(KOK_REGREPLY_DATA) <= KOK_RELAY_DATA,"KOK regreply size");
static_assert(sizeof(KOK_STATE_DATA) <= KOK_RELAY_DATA,"KOK state size");
static_assert(sizeof(SDHP_KOK_SAVE) == 4+9+9+2+2+4,"KOK save size");
static_assert(sizeof(SDHP_KOK_WINNER) == 4+1+9+32+4,"KOK winner size");

#pragma pack(pop)
