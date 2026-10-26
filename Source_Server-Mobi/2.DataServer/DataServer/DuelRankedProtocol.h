// DuelRankedProtocol.h: wire structs between the GameServer and the DataServer
// for ranked 1v1 matchmaking in the Duel Arena (head 0xD9, sub-codes 0x51-0x58).
//
// See Working Project\MD Files\RankedDuel-PLAN.md section 5. Mirrored byte for
// byte in the GameServer's DuelRanked.h. Needs BYTE/WORD/DWORD and PSBMSG_HEAD
// (C1: size, head, subhead) in scope, like MonthlyCard.h.
//
// Names are char[11] (10 + NUL). Every struct is packed; sizes are asserted.
//
//////////////////////////////////////////////////////////////////////

#pragma once

// REQ.Op
enum eDuelRankedOp
{
	DUELRANKED_OP_STATUS = 0,
	DUELRANKED_OP_JOIN = 1,
	DUELRANKED_OP_LEAVE = 2,
	DUELRANKED_OP_ACCEPT = 3,
	DUELRANKED_OP_DECLINE = 4,
	DUELRANKED_OP_LIST = 5,
	DUELRANKED_OP_WATCH = 6,
	DUELRANKED_OP_CANTGO = 7,
};

// STATUS.State (the DataServer's state of a character)
enum eDuelRankedState
{
	DUELRANKED_STATE_NONE = 0,
	DUELRANKED_STATE_QUEUED = 1,
	DUELRANKED_STATE_PROMPT = 2,
	DUELRANKED_STATE_STAGING = 3,
	DUELRANKED_STATE_FIGHTING = 4,
	DUELRANKED_STATE_WATCHING = 5,
};

// GO.Kind, EXPECT.Kind, DEPART.Kind (RESUME is GO only)
enum eDuelRankedKind
{
	DUELRANKED_KIND_FIGHTER = 0,
	DUELRANKED_KIND_SPECTATOR = 1,
	DUELRANKED_KIND_RESUME = 2,
};

// HOSTEVENT.Event
enum eDuelRankedHostEvent
{
	DUELRANKED_EVENT_ARRIVED = 1,
	DUELRANKED_EVENT_STARTED = 2,
	DUELRANKED_EVENT_ABORT = 3,
	DUELRANKED_EVENT_PAUSED = 4,
	DUELRANKED_EVENT_RESUMED = 5,
	DUELRANKED_EVENT_SPECTATOR_LEFT = 6,
	DUELRANKED_EVENT_SPECTATOR_ARRIVED = 7,
};

// RESULT.Reason
enum eDuelRankedReason
{
	DUELRANKED_REASON_SCORE = 1,
	DUELRANKED_REASON_FORFEIT = 2,
	DUELRANKED_REASON_DISCONNECT = 3,
	DUELRANKED_REASON_TIMEOUT = 4,
	DUELRANKED_REASON_IDLE = 5,
};

// MATCHCANCEL.Reason
enum eDuelRankedCancel
{
	DUELRANKED_CANCEL_VOID = 1,
	DUELRANKED_CANCEL_NOSHOW = 2,
	DUELRANKED_CANCEL_WATCHDOG = 3,
};

// MSG.Code (DataServer -> GameServer 0x57; the GameServer relays it as AA:13)
enum eDuelRankedMsg
{
	DUELRANKED_MSG_QUEUED = 1,
	DUELRANKED_MSG_LEFT_QUEUE = 2,
	DUELRANKED_MSG_RANKED_DISABLED = 3,
	DUELRANKED_MSG_ARENA_UNAVAILABLE = 4,
	DUELRANKED_MSG_ALREADY_QUEUED = 5,
	DUELRANKED_MSG_IN_MATCH = 6,
	DUELRANKED_MSG_COOLDOWN = 7,				// P1 = seconds, capped 255
	DUELRANKED_MSG_LEVEL_TOO_LOW = 8,			// P1 = min level (GameServer only)
	DUELRANKED_MSG_NOT_ALLOWED_NOW = 9,		// (GameServer only)
	DUELRANKED_MSG_OPEN_TITUS_FIRST = 10,		// (GameServer only)
	DUELRANKED_MSG_OPPONENT_DECLINED = 11,	// you stay queued
	DUELRANKED_MSG_MATCH_CANCELLED = 12,		// you stay queued
	DUELRANKED_MSG_REMOVED_FROM_QUEUE = 13,	// you declined / timed out (cooldown)
	DUELRANKED_MSG_MATCH_VOID = 14,			// no MMR changed
	DUELRANKED_MSG_OPPONENT_DISCONNECTED = 15,	// P1 = seconds left (host)
	DUELRANKED_MSG_OPPONENT_RETURNED = 16,	// (host)
	DUELRANKED_MSG_OPPONENT_TIMED_OUT = 17,	// you win (host)
	DUELRANKED_MSG_YOU_TIMED_OUT = 18,		// you lose (host)
	DUELRANKED_MSG_WATCH_NOT_FOUND = 19,
	DUELRANKED_MSG_WATCH_FULL = 20,
	DUELRANKED_MSG_PARTY_BLOCK = 21,			// (GameServer only)
	DUELRANKED_MSG_HELPER_BLOCK = 22,			// (GameServer only)
	DUELRANKED_MSG_MOVING_TO_ARENA = 23,		// (GameServer only)
	DUELRANKED_MSG_FIGHT_STARTING = 24,		// (host)
	DUELRANKED_MSG_RESUMING = 25,				// (host)
	DUELRANKED_MSG_SPECTATING = 26,			// (host)
	DUELRANKED_MSG_GOING_HOME = 27,			// (host)
	DUELRANKED_MSG_RANK_REWARD = 28,			// first time at a higher rank: reward added to the account (P1 = tier)
};

#pragma pack(push,1)

//**********************************************//
//********** GameServer -> DataServer **********//
//**********************************************//

// 0xD9 / 0x51. Op 0..7 (eDuelRankedOp). Level only matters for JOIN. MatchId is
// the PROMPT token for ACCEPT/DECLINE, the real MatchId for WATCH and CANTGO.
struct SDHP_DUELRANKED_REQ
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	char Name[11];
	BYTE Op;
	WORD Level;
	DWORD MatchId;
	BYTE Room;
};

// One duel room in HOSTREPORT. State: 0 empty, 1 fighting, 2 paused, 3 clean.
struct DUELRANKED_ROOM
{
	BYTE State;
	BYTE Ranked;
	DWORD MatchId;
	char Name1[11];
	char Name2[11];
	BYTE Score1;
	BYTE Score2;
	WORD RemainSec;
};

// 0xD9 / 0x52, from the host every few seconds.
struct SDHP_DUELRANKED_HOSTREPORT
{
	PSBMSG_HEAD header;
	WORD UserCount;
	WORD UserMax;
	BYTE Accepting;
	BYTE FreeRooms;
	DUELRANKED_ROOM Rooms[4];
};

// 0xD9 / 0x53, from the player's origin GameServer after GO.
struct SDHP_DUELRANKED_DEPART
{
	PSBMSG_HEAD header;
	char Name[11];
	char Account[11];
	DWORD MatchId;
	BYTE Kind;				// 0 fighter, 1 spectator
	WORD OriginServer;
	BYTE OriginMap;
	BYTE OriginX;
	BYTE OriginY;
};

// 0xD9 / 0x54, from the host (eDuelRankedHostEvent).
struct SDHP_DUELRANKED_HOSTEVENT
{
	PSBMSG_HEAD header;
	DWORD MatchId;
	BYTE Event;
	BYTE Room;
	char Name[11];
};

// 0xD9 / 0x55, from the host; resent until APPLIED arrives.
struct SDHP_DUELRANKED_RESULT
{
	PSBMSG_HEAD header;
	DWORD MatchId;
	char Winner[11];
	char Loser[11];
	BYTE Reason;			// eDuelRankedReason
	BYTE ScoreW;
	BYTE ScoreL;
	BYTE IsDraw;
};

//**********************************************//
//********** DataServer -> GameServer **********//
//**********************************************//

// 0xD9 / 0x51
struct SDHP_DUELRANKED_STATUS
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	WORD Mmr;
	BYTE Tier;
	BYTE Div;				// 3 = III, 2 = II, 1 = I, 0 = none (top tier)
	WORD StepLo;
	WORD StepNext;			// 0 = top step
	WORD Wins;
	WORD Losses;
	BYTE State;				// eDuelRankedState
	WORD QueuedSec;
	BYTE Accepting;
};

// 0xD9 / 0x52. MatchId here is a PROMPT TOKEN (high bit set), not the final
// MatchId: it only identifies the prompt for ACCEPT/DECLINE. The real MatchId
// comes with GO and EXPECT.
struct SDHP_DUELRANKED_PROMPT
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	DWORD MatchId;
	char Opp[11];
	WORD OppMmr;
	BYTE OppTier;
	BYTE OppDiv;
	BYTE Sec;
};

// 0xD9 / 0x53, addressed by NAME (the GameServer finds the player).
struct SDHP_DUELRANKED_GO
{
	PSBMSG_HEAD header;
	char Name[11];
	char Account[11];
	DWORD MatchId;
	BYTE Kind;				// eDuelRankedKind
	WORD HostServer;
	BYTE Room;
};

// 0xD9 / 0x54, to the host only.
struct SDHP_DUELRANKED_EXPECT
{
	PSBMSG_HEAD header;
	DWORD MatchId;
	BYTE Kind;				// 0 fighter, 1 spectator
	BYTE Slot;				// fighter 0/1
	char Name[11];
	char Account[11];
	char Opp[11];
	WORD Mmr;
	BYTE Tier;
	BYTE Div;
	WORD OriginServer;
	BYTE OriginMap;
	BYTE OriginX;
	BYTE OriginY;
	BYTE Room;
};

// 0xD9 / 0x55, to the host. Applied 0 = already settled before (nothing changed
// now). Applied 0 with an empty Winner = the match was voided in the database.
struct SDHP_DUELRANKED_APPLIED
{
	PSBMSG_HEAD header;
	DWORD MatchId;
	char Winner[11];
	char Loser[11];
	BYTE IsDraw;
	BYTE Applied;
	short DeltaW;
	short DeltaL;
	WORD OldMmrW;
	WORD OldMmrL;
	WORD NewMmrW;
	WORD NewMmrL;
	BYTE OldTierW;
	BYTE OldDivW;
	BYTE NewTierW;
	BYTE NewDivW;
	BYTE OldTierL;
	BYTE OldDivL;
	BYTE NewTierL;
	BYTE NewDivL;
};

struct DUELRANKED_LISTENTRY
{
	DWORD MatchId;
	BYTE Room;
	WORD HostServer;
	char Name1[11];
	char Name2[11];
	WORD Mmr1;
	WORD Mmr2;
	BYTE Tier1;
	BYTE Div1;
	BYTE Tier2;
	BYTE Div2;
	BYTE Score1;
	BYTE Score2;
	WORD RemainSec;
};

// 0xD9 / 0x56. Only Count entries are on the wire: header.size is
// DUELRANKED_LIST_BASE + Count * sizeof(DUELRANKED_LISTENTRY). Count <= 4.
struct SDHP_DUELRANKED_LIST
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	BYTE Count;
	DUELRANKED_LISTENTRY e[4];
};

// 0xD9 / 0x57. Code = eDuelRankedMsg.
struct SDHP_DUELRANKED_MSG
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	DWORD MatchId;
	BYTE Code;
	BYTE P1;
};

// 0xD9 / 0x58, to the host: drop the pending or running match and send the
// arrived players home. Reason = eDuelRankedCancel.
struct SDHP_DUELRANKED_MATCHCANCEL
{
	PSBMSG_HEAD header;
	DWORD MatchId;
	BYTE Reason;
};

// 0xD9 / 0x59, addressed by NAME (the GameServer finds the player and checks the account).
// Pushed when a character registers on a GameServer and again when a ranked result
// is applied. Played = 0: never played a rated match (no icon). Tier/Div as in STATUS.
struct SDHP_DUELRANKED_RANK
{
	PSBMSG_HEAD header;
	char Name[11];
	char Account[11];
	BYTE Tier;
	BYTE Div;
	BYTE Played;
};

#pragma pack(pop)

#define DUELRANKED_LIST_BASE 18

static_assert(sizeof(SDHP_DUELRANKED_REQ) == 4 + 2 + 11 + 11 + 1 + 2 + 4 + 1,"SDHP_DUELRANKED_REQ must match the GameServer's copy (36)");
static_assert(sizeof(DUELRANKED_ROOM) == 1 + 1 + 4 + 11 + 11 + 1 + 1 + 2,"DUELRANKED_ROOM must match the GameServer's copy (32)");
static_assert(sizeof(SDHP_DUELRANKED_HOSTREPORT) == 4 + 2 + 2 + 1 + 1 + 4 * 32,"SDHP_DUELRANKED_HOSTREPORT must match the GameServer's copy (138)");
static_assert(sizeof(SDHP_DUELRANKED_DEPART) == 4 + 11 + 11 + 4 + 1 + 2 + 1 + 1 + 1,"SDHP_DUELRANKED_DEPART must match the GameServer's copy (36)");
static_assert(sizeof(SDHP_DUELRANKED_HOSTEVENT) == 4 + 4 + 1 + 1 + 11,"SDHP_DUELRANKED_HOSTEVENT must match the GameServer's copy (21)");
static_assert(sizeof(SDHP_DUELRANKED_RESULT) == 4 + 4 + 11 + 11 + 1 + 1 + 1 + 1,"SDHP_DUELRANKED_RESULT must match the GameServer's copy (34)");
static_assert(sizeof(SDHP_DUELRANKED_STATUS) == 4 + 2 + 11 + 2 + 1 + 1 + 2 + 2 + 2 + 2 + 1 + 2 + 1,"SDHP_DUELRANKED_STATUS must match the GameServer's copy (33)");
static_assert(sizeof(SDHP_DUELRANKED_PROMPT) == 4 + 2 + 11 + 4 + 11 + 2 + 1 + 1 + 1,"SDHP_DUELRANKED_PROMPT must match the GameServer's copy (37)");
static_assert(sizeof(SDHP_DUELRANKED_GO) == 4 + 11 + 11 + 4 + 1 + 2 + 1,"SDHP_DUELRANKED_GO must match the GameServer's copy (34)");
static_assert(sizeof(SDHP_DUELRANKED_EXPECT) == 4 + 4 + 1 + 1 + 11 + 11 + 11 + 2 + 1 + 1 + 2 + 1 + 1 + 1 + 1,"SDHP_DUELRANKED_EXPECT must match the GameServer's copy (53)");
static_assert(sizeof(SDHP_DUELRANKED_APPLIED) == 4 + 4 + 11 + 11 + 1 + 1 + 2 + 2 + 2 + 2 + 2 + 2 + 4 + 4,"SDHP_DUELRANKED_APPLIED must match the GameServer's copy (52)");
static_assert(sizeof(DUELRANKED_LISTENTRY) == 4 + 1 + 2 + 11 + 11 + 2 + 2 + 4 + 2 + 2,"DUELRANKED_LISTENTRY must match the GameServer's copy (41)");
static_assert(sizeof(SDHP_DUELRANKED_LIST) == DUELRANKED_LIST_BASE + 4 * 41,"SDHP_DUELRANKED_LIST must match the GameServer's copy (182 with 4 entries)");
static_assert(sizeof(SDHP_DUELRANKED_MSG) == 4 + 2 + 11 + 4 + 1 + 1,"SDHP_DUELRANKED_MSG must match the GameServer's copy (23)");
static_assert(sizeof(SDHP_DUELRANKED_MATCHCANCEL) == 4 + 4 + 1,"SDHP_DUELRANKED_MATCHCANCEL must match the GameServer's copy (9)");
static_assert(sizeof(SDHP_DUELRANKED_RANK) == 4 + 11 + 11 + 1 + 1 + 1,"SDHP_DUELRANKED_RANK must match the other side's copy (29)");
static_assert(sizeof(SDHP_DUELRANKED_LIST) <= 255,"PSBMSG_HEAD stores the packet size in a single BYTE");
