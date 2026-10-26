// DuelRanked.h: Ranked 1v1 matchmaking for the Duel Arena, GameServer side.
//
// See Working Project\MD Files\RankedDuel-PLAN.md. The DataServer owns the queue,
// the pairing and the ratings. This side validates the player, hops it to the
// host GameServer (DuelRankedHostServer, 19), runs the fight there and reports
// the result. Wire structs: section 5 of the plan, mirrored byte for byte from
// the DataServer's DuelRankedProtocol.h.
//
// Threads: DataServer replies arrive on the UI thread, client packets on the
// socket queue thread, game logic on the timer thread (gDuel.MainProc, every
// 100 ms). The DataServer replies and the events raised by other threads (a
// character closing, a score win, a spectator leaving) only go into an inbox;
// the match tables are touched by the timer thread alone. Other threads read a
// small published snapshot of the names that take part. Both locks are leaf
// locks: nothing is sent and no gObj is touched while one is held.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Protocol.h"
#include "CriticalSection.h"

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

// MSG.Code (DataServer -> GameServer 0xD9 / 0x57; the GameServer relays it as AA:13)
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

//**********************************************//
//************ Client -> GameServer ************//
//**********************************************//

// C1:AA:10 STATUS_REQ and C1:AA:13 LIST_REQ are the bare PSBMSG_HEAD. Sent encrypted.

struct PMSG_DUELRANKED_QUEUE_RECV
{
	PSBMSG_HEAD header;		// C1:AA:11
	BYTE On;				// 1 join, 0 leave
};

struct PMSG_DUELRANKED_ACCEPT_RECV
{
	PSBMSG_HEAD header;		// C1:AA:12
	BYTE Flag;				// 1 accept, 0 decline
	DWORD MatchId;			// the prompt's MatchId
};

struct PMSG_DUELRANKED_WATCH_RECV
{
	PSBMSG_HEAD header;		// C1:AA:14
	BYTE Room;
	DWORD MatchId;
};

//**********************************************//
//************ GameServer -> Client ************//
//**********************************************//

struct PMSG_DUELRANKED_STATUS_SEND
{
	PSBMSG_HEAD header;		// C1:AA:10
	WORD Mmr;
	BYTE Tier;
	BYTE Div;
	WORD StepLo;
	WORD StepNext;
	WORD Wins;
	WORD Losses;
	BYTE State;
	WORD QueuedSec;
	BYTE Accepting;
};

struct PMSG_DUELRANKED_PROMPT_SEND
{
	PSBMSG_HEAD header;		// C1:AA:11
	DWORD MatchId;
	char Opp[10];
	WORD OppMmr;
	BYTE OppTier;
	BYTE OppDiv;
	BYTE SecLeft;
};

struct PMSG_DUELRANKED_LIST_SEND
{
	PSBMSG_HEAD header;		// C1:AA:12, followed by Count entries
	BYTE Count;
};

struct PMSG_DUELRANKED_LIST_ENTRY
{
	DWORD MatchId;
	BYTE Room;
	char N1[10];
	char N2[10];
	WORD Mmr1;
	WORD Mmr2;
	BYTE Tier1;
	BYTE Div1;
	BYTE Tier2;
	BYTE Div2;
	BYTE S1;
	BYTE S2;
	WORD RemainSec;
};

struct PMSG_DUELRANKED_MSG_SEND
{
	PSBMSG_HEAD header;		// C1:AA:13
	BYTE Code;
	BYTE P1;
};

struct PMSG_DUELRANKED_RESULT_SEND
{
	PSBMSG_HEAD header;		// C1:AA:14
	BYTE Outcome;			// 0 win, 1 loss, 2 draw, 3 void
	WORD OldMmr;
	WORD NewMmr;
	short Delta;
	BYTE OldTier;
	BYTE OldDiv;
	BYTE NewTier;
	BYTE NewDiv;
};

// C1:D3:C4, the public rank icon of the players around a client (and its own). Count
// entries follow the header; at most DUELRANKED_VIEW_MAX per packet. Tier is the
// ranked tier + 1 (0 = no icon), Div 3 = III, 2 = II, 1 = I, 0 = none.
struct PMSG_DUELRANKED_VIEW_ENTRY
{
	WORD Index;
	BYTE Tier;
	BYTE Div;
};

struct PMSG_DUELRANKED_VIEW_SEND
{
	PSBMSG_HEAD header;
	BYTE Count;
};

#pragma pack(pop)

#define DUELRANKED_VIEW_MAX 40	// 5 + 40 * 4 = 165 bytes, under the one byte C1 size

#define DUELRANKED_LIST_BASE 18

static_assert(sizeof(SDHP_DUELRANKED_REQ) == 4 + 2 + 11 + 11 + 1 + 2 + 4 + 1,"SDHP_DUELRANKED_REQ must match the DataServer's copy (36)");
static_assert(sizeof(DUELRANKED_ROOM) == 1 + 1 + 4 + 11 + 11 + 1 + 1 + 2,"DUELRANKED_ROOM must match the DataServer's copy (32)");
static_assert(sizeof(SDHP_DUELRANKED_HOSTREPORT) == 4 + 2 + 2 + 1 + 1 + 4 * 32,"SDHP_DUELRANKED_HOSTREPORT must match the DataServer's copy (138)");
static_assert(sizeof(SDHP_DUELRANKED_DEPART) == 4 + 11 + 11 + 4 + 1 + 2 + 1 + 1 + 1,"SDHP_DUELRANKED_DEPART must match the DataServer's copy (36)");
static_assert(sizeof(SDHP_DUELRANKED_HOSTEVENT) == 4 + 4 + 1 + 1 + 11,"SDHP_DUELRANKED_HOSTEVENT must match the DataServer's copy (21)");
static_assert(sizeof(SDHP_DUELRANKED_RESULT) == 4 + 4 + 11 + 11 + 1 + 1 + 1 + 1,"SDHP_DUELRANKED_RESULT must match the DataServer's copy (34)");
static_assert(sizeof(SDHP_DUELRANKED_STATUS) == 4 + 2 + 11 + 2 + 1 + 1 + 2 + 2 + 2 + 2 + 1 + 2 + 1,"SDHP_DUELRANKED_STATUS must match the DataServer's copy (33)");
static_assert(sizeof(SDHP_DUELRANKED_PROMPT) == 4 + 2 + 11 + 4 + 11 + 2 + 1 + 1 + 1,"SDHP_DUELRANKED_PROMPT must match the DataServer's copy (37)");
static_assert(sizeof(SDHP_DUELRANKED_GO) == 4 + 11 + 11 + 4 + 1 + 2 + 1,"SDHP_DUELRANKED_GO must match the DataServer's copy (34)");
static_assert(sizeof(SDHP_DUELRANKED_EXPECT) == 4 + 4 + 1 + 1 + 11 + 11 + 11 + 2 + 1 + 1 + 2 + 1 + 1 + 1 + 1,"SDHP_DUELRANKED_EXPECT must match the DataServer's copy (53)");
static_assert(sizeof(SDHP_DUELRANKED_APPLIED) == 4 + 4 + 11 + 11 + 1 + 1 + 2 + 2 + 2 + 2 + 2 + 2 + 4 + 4,"SDHP_DUELRANKED_APPLIED must match the DataServer's copy (52)");
static_assert(sizeof(DUELRANKED_LISTENTRY) == 4 + 1 + 2 + 11 + 11 + 2 + 2 + 4 + 2 + 2,"DUELRANKED_LISTENTRY must match the DataServer's copy (41)");
static_assert(sizeof(SDHP_DUELRANKED_LIST) == DUELRANKED_LIST_BASE + 4 * 41,"SDHP_DUELRANKED_LIST must match the DataServer's copy (182 with 4 entries)");
static_assert(sizeof(SDHP_DUELRANKED_MSG) == 4 + 2 + 11 + 4 + 1 + 1,"SDHP_DUELRANKED_MSG must match the DataServer's copy (23)");
static_assert(sizeof(SDHP_DUELRANKED_MATCHCANCEL) == 4 + 4 + 1,"SDHP_DUELRANKED_MATCHCANCEL must match the DataServer's copy (9)");
static_assert(sizeof(SDHP_DUELRANKED_RANK) == 4 + 11 + 11 + 1 + 1 + 1,"SDHP_DUELRANKED_RANK must match the other side's copy (29)");
static_assert(sizeof(SDHP_DUELRANKED_LIST) <= 255,"PSBMSG_HEAD stores the packet size in a single BYTE");

static_assert(sizeof(PMSG_DUELRANKED_QUEUE_RECV) == 5,"PMSG_DUELRANKED_QUEUE_RECV must match the client's copy");
static_assert(sizeof(PMSG_DUELRANKED_ACCEPT_RECV) == 9,"PMSG_DUELRANKED_ACCEPT_RECV must match the client's copy");
static_assert(sizeof(PMSG_DUELRANKED_WATCH_RECV) == 9,"PMSG_DUELRANKED_WATCH_RECV must match the client's copy");
static_assert(sizeof(PMSG_DUELRANKED_STATUS_SEND) == 4 + 2 + 1 + 1 + 2 + 2 + 2 + 2 + 1 + 2 + 1,"PMSG_DUELRANKED_STATUS_SEND must match the client's copy (20)");
static_assert(sizeof(PMSG_DUELRANKED_PROMPT_SEND) == 4 + 4 + 10 + 2 + 1 + 1 + 1,"PMSG_DUELRANKED_PROMPT_SEND must match the client's copy (23)");
static_assert(sizeof(PMSG_DUELRANKED_LIST_SEND) == 5,"PMSG_DUELRANKED_LIST_SEND must match the client's copy");
static_assert(sizeof(PMSG_DUELRANKED_LIST_ENTRY) == 4 + 1 + 10 + 10 + 2 + 2 + 1 + 1 + 1 + 1 + 1 + 1 + 2,"PMSG_DUELRANKED_LIST_ENTRY must match the client's copy (37)");
static_assert(sizeof(PMSG_DUELRANKED_VIEW_ENTRY) == 4,"PMSG_DUELRANKED_VIEW_ENTRY must match the client's copy");
static_assert(sizeof(PMSG_DUELRANKED_VIEW_SEND) == 5,"PMSG_DUELRANKED_VIEW_SEND must match the client's copy");
static_assert(sizeof(PMSG_DUELRANKED_MSG_SEND) == 6,"PMSG_DUELRANKED_MSG_SEND must match the client's copy");
static_assert(sizeof(PMSG_DUELRANKED_RESULT_SEND) == 4 + 1 + 2 + 2 + 2 + 4,"PMSG_DUELRANKED_RESULT_SEND must match the client's copy (15)");

//**********************************************//
//************** Host match tables **************//
//**********************************************//

#define DR_MAX_MATCH 8
#define DR_MAX_SPEC 40
#define DR_MAX_RESULT 32
#define DR_MAX_GOSEEN 32
#define DR_MAX_HOMEWAIT 32
#define DR_MAX_SNAP 64
#define DR_MAX_INBOX 256

// State of a match on the host.
enum eDuelRankedHostState
{
	DRH_STAGING = 1,	// waiting for the fighters to be ready
	DRH_BEGIN = 2,		// room reserved, fighters being placed on their spawn cells
	DRH_FIGHT = 3,
	DRH_PAUSED = 4,		// a fighter dropped, waiting for the reconnect
	DRH_CLEAN = 5,		// settled, the room is being cleaned
};

enum eDuelRankedRole
{
	DR_ROLE_FIGHTER = 1,
	DR_ROLE_SPECTATOR = 2,
};

enum eDuelRankedInbox
{
	DR_IN_DS = 1,			// a packet from the DataServer
	DR_IN_CLOSE = 2,		// a ranked fighter's character is closing
	DR_IN_SCOREWIN = 3,		// a fighter reached the winning score
	DR_IN_SPECLEAVE = 4,	// a spectator asked to leave (AA:09)
};

struct DR_INBOX
{
	BYTE Kind;
	BYTE Sub;
	WORD Size;
	BYTE Data[264];
};

struct DR_EVT_CLOSE
{
	DWORD MatchId;
	char Name[11];
	BYTE HaveScores;
	BYTE ScoreSelf;
	BYTE ScoreOther;
};

struct DR_EVT_SCOREWIN
{
	DWORD MatchId;
	char Winner[11];
	char Loser[11];
	BYTE ScoreW;
	BYTE ScoreL;
};

struct DR_EVT_SPEC
{
	char Name[11];
};

// What other threads may read: the names that take part and in what state.
struct DR_SNAP
{
	char Name[11];
	DWORD MatchId;
	BYTE Role;
	BYTE State;			// eDuelRankedHostState
	BYTE Absent;
};

struct DR_FIGHTER
{
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
	BYTE HaveExpect;
	BYTE Arrived;		// ARRIVED event sent
	BYTE ReadyCount;	// consecutive seconds the arrival criteria held
	BYTE Placed;		// on the spawn cell
	BYTE Absent;
	BYTE Reconnects;
	BYTE Score;
	int Index;			// last object index seen; always re-validated by name
	DWORD AbsentSince;
};

struct DR_HOSTMATCH
{
	BYTE Used;
	BYTE State;
	BYTE Room;			// 0xFF = none
	BYTE Settled;
	BYTE Voided;
	BYTE Resuming;
	BYTE ResumeSlot;	// the returning fighter while Resuming
	BYTE StartedSent;
	BYTE EffectsDone;	// the win messages were already shown
	BYTE EndSent;		// AA:03 already sent to the fighters
	BYTE ResultShown;	// AA:14 (or the void message) already sent
	DWORD MatchId;
	DR_FIGHTER F[2];
	BYTE Gone;			// everyone was sent home; the record lingers a few seconds for the PK guard
	int RemainSec;		// ranked clock, frozen while paused
	DWORD ClockTick;	// the clock was last advanced at
	DWORD GoneTick;		// sent home at
	DWORD StateTick;	// the match was created (staging) or entered this state
	DWORD BeginTick;	// the placement started
	DWORD CleanEnd;		// settled at
	DWORD NoticeTick;	// last reconnect message
};

struct DR_SPEC
{
	BYTE Used;
	BYTE State;			// 1 waiting to arrive, 2 in the room
	BYTE Arrived;
	BYTE ReadyCount;
	BYTE Room;
	DWORD MatchId;
	char Name[11];
	char Account[11];
	WORD OriginServer;
	BYTE OriginMap;
	BYTE OriginX;
	BYTE OriginY;
	int Index;
	DWORD AddedTick;
};

// A result the DataServer has not acknowledged yet.
struct DR_RESULT
{
	BYTE Used;
	SDHP_DUELRANKED_RESULT Msg;
	DWORD FirstTick;
	DWORD LastTick;
};

struct DR_GOSEEN
{
	BYTE Used;
	BYTE Kind;
	DWORD MatchId;
	char Name[11];
	DWORD Tick;
};

// Someone who was ordered to this host for a match (or a fight to watch) that ended
// before he landed. The match record is gone, so this remembers where he came from:
// when he appears on the arena map he is sent back there.
struct DR_HOMEWAIT
{
	BYTE Used;
	char Name[11];
	char Account[11];
	WORD OriginServer;
	BYTE OriginMap;
	BYTE OriginX;
	BYTE OriginY;
	DWORD Since;
};

class CDuelRanked
{
public:
	CDuelRanked();
	virtual ~CDuelRanked();

	// Every 100 ms from CDuel::MainProc (timer thread): drains the inbox, and
	// once a second runs the host's tables.
	void MainProc();

	// Client -> GameServer, C1:AA:10..14 (socket thread).
	void CGStatusRecv(int aIndex);
	void CGQueueRecv(PMSG_DUELRANKED_QUEUE_RECV* lpMsg,int size,int aIndex);
	void CGAcceptRecv(PMSG_DUELRANKED_ACCEPT_RECV* lpMsg,int size,int aIndex);
	void CGListRecv(int aIndex);
	void CGWatchRecv(PMSG_DUELRANKED_WATCH_RECV* lpMsg,int size,int aIndex);

	// DataServer -> GameServer, C1:D9:51..58 (any thread): inbox only.
	void DGRecv(BYTE* lpMsg,int size);

	// Hooks (any thread).
	bool OnCharacterClose(LPOBJ lpObj);
	bool IsPausedFighter(char* name);
	bool IsParticipant(char* name);
	bool CheckProtectedTarget(LPOBJ lpObj,LPOBJ lpTarget);
	void OnScoreWin(DWORD matchId,char* winner,char* loser,BYTE scoreW,BYTE scoreL);
	void OnSpectatorLeave(int aIndex);

	// Public rank icon (DataServer 0x59, client C1:D3:C4).
	void SendRanksToViewer(int aIndex);	// the players in aIndex's viewport and itself, to aIndex
	void SendRankToAll(int aIndex,bool force = 0);	// aIndex to itself and to the players that see it (force: also to clear an icon)

	// Timer thread: a player leaves the arena for good (origin, or gate 17).
	void SendHome(int aIndex);

	// GM test commands.
	bool TestHop(LPOBJ lpObj);
	bool TestHome(LPOBJ lpObj,char* arg);

private:
	// config
	bool IsHost();
	bool HostAccepting();
	// origin side
	int CheckOrigin(LPOBJ lpObj,int mode,BYTE* p1);
	bool Throttle(int aIndex,int kind,DWORD ms);
	bool SendReq(int aIndex,BYTE op,DWORD matchId,BYTE room,WORD level);
	void SendCantGo(char* name,char* account,DWORD matchId);
	void SendDepart(LPOBJ lpObj,DWORD matchId,BYTE kind);
	void SendClientMsg(int aIndex,BYTE code,BYTE p1);
	bool HopToHost(LPOBJ lpObj,WORD hostServer);
	DWORD GoLast(char* name,DWORD matchId,BYTE kind);
	void GoMark(char* name,DWORD matchId,BYTE kind);
	// inbox
	void Push(const DR_INBOX* item);
	void DrainInbox();
	void HandleDS(BYTE sub,BYTE* data,int size);
	void ForwardStatus(SDHP_DUELRANKED_STATUS* p);
	void ForwardPrompt(SDHP_DUELRANKED_PROMPT* p);
	void ForwardList(SDHP_DUELRANKED_LIST* p);
	void ForwardMsg(SDHP_DUELRANKED_MSG* p);
	void HandleGo(SDHP_DUELRANKED_GO* p);
	void HandleExpect(SDHP_DUELRANKED_EXPECT* p);
	void HandleApplied(SDHP_DUELRANKED_APPLIED* p);
	void HandleCancel(SDHP_DUELRANKED_MATCHCANCEL* p);
	void HandleRank(SDHP_DUELRANKED_RANK* p);
	void HandleClose(DR_EVT_CLOSE* e);
	void HandleScoreWin(DR_EVT_SCOREWIN* e);
	void HandleSpecLeave(DR_EVT_SPEC* e);
	// host
	void Tick1s();
	DR_HOSTMATCH* FindMatch(DWORD matchId);
	DR_HOSTMATCH* NewMatch(DWORD matchId);
	int SlotOf(DR_HOSTMATCH* rec,char* name);
	int FindUser(char* name,char* account,bool closing = 0);
	bool IsReady(LPOBJ lpObj);
	void ProcStaging(DR_HOSTMATCH* rec);
	void ProcBegin(DR_HOSTMATCH* rec);
	void ProcFight(DR_HOSTMATCH* rec);
	void ProcClean(DR_HOSTMATCH* rec);
	bool TryBegin(DR_HOSTMATCH* rec);
	bool PlaceFighters(DR_HOSTMATCH* rec);
	void FinishBegin(DR_HOSTMATCH* rec);
	void StartResume(DR_HOSTMATCH* rec,int slot);
	void AbortBegin(DR_HOSTMATCH* rec,int slot);
	void PauseFighter(DR_HOSTMATCH* rec,int slot);
	void SyncScores(DR_HOSTMATCH* rec);
	void Settle(DR_HOSTMATCH* rec,int winner,BYTE reason,bool draw);
	void SettleByScore(DR_HOSTMATCH* rec,BYTE reason);
	void EndVoid(DR_HOSTMATCH* rec,char* why,bool sendAbort,char* blame);
	void AbortStaging(DR_HOSTMATCH* rec,bool blame);
	void DropSpectatorsOf(DWORD matchId);
	bool IsPendingSpectator(char* name);
	void LeaveArena(DR_HOSTMATCH* rec);
	void SendFightersHome(DR_HOSTMATCH* rec);
	void AddHomeWait(char* name,char* account,WORD originServer,BYTE originMap,BYTE x,BYTE y);
	void DropHomeWait(char* name);
	void ProcHomeWait();
	void SendHomeRecord(int aIndex,WORD originServer,BYTE originMap,BYTE x,BYTE y);
	bool InRoomArea(int room,int x,int y);
	bool PlacedAt(LPOBJ lpObj,int gate);
	void NoticeRoom(DR_HOSTMATCH* rec,int toSlot,BYTE code,BYTE p1);
	void NoticeSpectators(DR_HOSTMATCH* rec,BYTE code,BYTE p1);
	void QueueResult(DR_HOSTMATCH* rec,int w,int l,BYTE reason,bool draw);
	void ProcResults();
	void ProcSpectators();
	void AddSpectator(DR_SPEC* e,DR_HOSTMATCH* rec);
	void RemoveSpectator(DR_SPEC* e,bool sendHome);
	void SendHostEvent(BYTE ev,DWORD matchId,BYTE room,char* name);
	void SendHostReport();
	void Sweep();
	void PublishSnap();

private:
	CCriticalSection m_InLock;
	std::vector<DR_INBOX> m_Inbox;

	CCriticalSection m_SnapLock;
	DR_SNAP m_Snap[DR_MAX_SNAP];
	int m_SnapCount;

	// timer thread only from here
	DWORD m_SecondTick;
	DWORD m_SecCount;
	bool m_SnapDirty;
	DR_HOSTMATCH m_Match[DR_MAX_MATCH];
	DR_SPEC m_Spec[DR_MAX_SPEC];
	DR_RESULT m_Result[DR_MAX_RESULT];
	DR_GOSEEN m_GoSeen[DR_MAX_GOSEEN];
	DR_HOMEWAIT m_HomeWait[DR_MAX_HOMEWAIT];
	DWORD m_LastReportTick;
	SDHP_DUELRANKED_HOSTREPORT m_LastReport;
	DWORD m_LastReq[4][MAX_OBJECT];	// status, queue, list, watch
	DWORD m_StrandSince[MAX_OBJECT];

	// rank icon cache by object index: tier + 1 (0 = none) and division
	BYTE m_ViewTier[MAX_OBJECT];
	BYTE m_ViewDiv[MAX_OBJECT];
};

extern CDuelRanked gDuelRanked;
