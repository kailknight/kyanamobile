// SlotMachine.h: interface for the CSlotMachine class.
//
// A 5x3 video slot machine reached by talking to a configurable NPC. The stake and
// the payout are both ITEMS, so no new currency exists.
//
// THE SERVER ROLLS; THE CLIENT ONLY ANIMATES. A spin request carries nothing but
// which accept-list entry to stake. The reply carries the finished 15-cell grid,
// the winning lines and the amount won. The client spins its reels and lands them
// on the symbols it was given. Nothing about the outcome is ever decided
// client-side.
//
// Winnings are PARKED in a reward box and claimed by hand, never pushed into the
// inventory. See SLOT_CLAIM_ROW below for why this box is separate from the spin
// wheel's.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "User.h"
#include "Protocol.h"
#include "ItemManager.h"
#include "MonsterSetBase.h"

// ---- Geometry ---------------------------------------------------------------
// Fixed, not configurable. The client's reel layout and the wire format below
// both assume 5x3; making it variable would mean a variable-length grid in every
// packet for no gain.
#define SLOT_REELS					5
#define SLOT_ROWS					3
#define SLOT_CELLS					(SLOT_REELS * SLOT_ROWS)

// ---- Table limits ----------------------------------------------------------
// Each is a hard cap on how much of that section the config may declare. Rows
// past the cap are refused with a log line rather than silently dropped, because
// a paytable that quietly lost its last row would misprice every spin.
#define SLOT_MAX_SYMBOL				16
#define SLOT_MAX_LINE				16
#define SLOT_MAX_STAKE				16
#define SLOT_MAX_ANNOUNCE			8

// SLOT_CLAIM_SIZE and SLOT_CLAIM_ROW live in User.h, because LPOBJ holds the box
// inline and User.h cannot include this file. 8 rows: enough in practice, and
// small enough that "the box is full" is a real pressure to claim rather than a
// theoretical limit.

// Symbol kinds.
//   NORMAL     - pays on a payline
//   SCATTER    - ignores paylines entirely; 3+ anywhere triggers free spins
//   MULTIPLIER - a WILD carrying a value. It substitutes for whatever a line is
//                paying, and the values of every multiplier on a winning line ADD
//                together (not multiply), clamped to m_MultiCap.
#define SLOT_SYMBOL_NORMAL			0
#define SLOT_SYMBOL_SCATTER			1
#define SLOT_SYMBOL_MULTIPLIER		2

// Free spins are locked to the stake that triggered them. -1 means "not in a
// free-spin round", which is distinct from stake index 0.
#define SLOT_NO_STAKE				(-1)

// Hard ceiling on the bet, whatever the config says. The bet travels as a BYTE and
// multiplies every payout, so this is also what stops a malformed packet asking for
// a 255x payout.
#define SLOT_BET_LIMIT				100

// Spin request refusal codes, echoed to the client so it can say why rather than
// just failing silently.
#define SLOT_RESULT_OK				0
#define SLOT_RESULT_DISABLED		1
#define SLOT_RESULT_NOT_OPEN		2
#define SLOT_RESULT_TOO_FAST		3
#define SLOT_RESULT_BAD_STAKE		4
#define SLOT_RESULT_NO_FUNDS		5
#define SLOT_RESULT_BOX_FULL		6
#define SLOT_RESULT_BUSY			7
#define SLOT_RESULT_NO_ROOM			8
#define SLOT_RESULT_NO_BANK			9

// ---- Config ----------------------------------------------------------------

struct SLOT_SYMBOL_INFO
{
	int Kind;
	int ItemIndex;		// GET_ITEM(cat,index) - what the client draws it as
	int Level;
	int Value;			// multiplier value; 0 for every other kind
	char Name[32];
};

struct SLOT_LINE_INFO
{
	int Row[SLOT_REELS];	// row index per reel, 0 = top
};

// Payout for 3, 4 and 5 of a kind, expressed DIRECTLY IN ITEMS WON.
//
// Not in "credits", and the difference is worth stating: the chosen model pays in
// the same item that was staked, and under that rule credits cancel out
// completely - stake 30 credits, win x5 = 150, paid 150/30 = 5 items, which is
// just the multiplier. Writing the table in items removes a division, removes all
// rounding loss, and is what an operator can actually reason about.
struct SLOT_PAY_INFO
{
	int Pay[3];			// [0] = 3 of a kind, [1] = 4, [2] = 5
};

// What a stake is made of. Kind 0 is an inventory item; 1-3 are the cash
// currencies, which have no inventory footprint and so are paid straight out
// instead of being parked in the reward box.
#define SLOT_STAKE_ITEM				0
#define SLOT_STAKE_WCOINC			1
#define SLOT_STAKE_WCOINP			2
#define SLOT_STAKE_GOBLIN			3

struct SLOT_STAKE_INFO
{
	int Kind;
	int ItemIndex;		// Kind 0 only
	int Level;			// Kind 0 only
	int Amount;			// items, or coins, per unit of bet
	int Credits;		// only for the per-spin payout cap and the audit log
	char Name[32];
};

// ---- Bonus, pity and beginner luck -----------------------------------------
// All three exist to shape how the machine FEELS rather than what it pays, and
// all three are off unless the config switches them on.
struct SLOT_BONUS_INFO
{
	// Awarded on top of free spins whenever a spin lands 3+ scatters. Count 0
	// disables it without disturbing the free spins.
	int ScatterItemIndex;
	int ScatterLevel;
	int ScatterCount;

	// After this many losing spins in a row, the next spin is FORCED to win.
	// 0 disables it. The forced spin is built as a real grid and then evaluated
	// by the ordinary code, so it cannot pay something the paytable would not.
	int LossStreak;
	int LossStreakPerDay;		// how many forced wins a character may have per day

	// Relative weights for what the forced win looks like. The user's numbers -
	// 90 / 5 / 2.5 / 2.5 percent - are expressible as 900 / 50 / 25 / 25.
	int PityWeightMin;			// smallest paying combination
	int PityWeightX2;			// a x2 wild in the run
	int PityWeightX4;			// a x4 wild in the run
	int PityWeightScatter;		// three scatters, so free spins instead

	// For a character's first BeginnerSpins spins, a LOSING spin is converted
	// into a minimum win BeginnerPercent of the time. 0 for either disables it.
	int BeginnerSpins;
	int BeginnerPercent;

	// Win sizes that are worth telling the whole server about. A win is announced
	// at the HIGHEST threshold it reaches, so one spin never posts twice.
	int AnnounceCount;
	int Announce[SLOT_MAX_ANNOUNCE];
};

struct SLOT_MESSAGE_INFO
{
	int Index;
	char Message[256];
};

// ---- Reward box ------------------------------------------------------------
// SLOT_CLAIM_ROW (in User.h) holds what and HOW MANY.
//
// That count is why this box cannot be the spin wheel's. SpinClaim holds up to 8
// individual CItems, one per slot; a slot machine pays N of a single item, so a
// win of 40 jewels would need 40 of its slots and simply would not fit. Here one
// win is one row whatever its size. Widening the wheel's row to carry a count was
// the alternative and was rejected - it would have misread every prize players are
// already holding on the wheel.

// ---- Wire ------------------------------------------------------------------
// Head 0xD3, joining the spin wheel's family (0x8A-0x8E). 0x8F-0x93 are free -
// grep the WHOLE case 0xD3 switch in Protocol.cpp before adding more; it runs to
// roughly line 1950 and 0x7B was once claimed several hundred lines below where it
// looked free.
//
// 0xD3 MUST be sent unencrypted: HackPacketCheck.txt lists head 211 with
// Encrypt = 0, and CheckPacketHack calls CloseClient on a mismatch BEFORE
// dispatch. DataSend ends in a plain send, so it is correct; never spe.Send(TRUE).
//
// Packed explicitly and pinned with static_asserts, because the client declares
// the same structs separately and nothing but these numbers keeps the two in step.
#pragma pack(push,1)

struct PMSG_SLOT_OPEN_SYMBOL
{
	BYTE Kind;
	WORD ItemIndex;
	BYTE Level;
	BYTE Value;
};

struct PMSG_SLOT_OPEN_LINE
{
	BYTE Row[SLOT_REELS];
};

struct PMSG_SLOT_OPEN_STAKE
{
	WORD ItemIndex;
	BYTE Level;
	WORD Credits;
	BYTE Kind;			// SLOT_STAKE_*, so the client can word the cost line
	DWORD Amount;		// items or coins per unit of bet
	char Name[32];
};

// GameServer -> Client, C2:D3:8F.
// Followed by SymbolCount PMSG_SLOT_OPEN_SYMBOL, then LineCount
// PMSG_SLOT_OPEN_LINE, then StakeCount PMSG_SLOT_OPEN_STAKE.
struct PMSG_SLOT_OPEN_SEND
{
	PSWMSG_HEAD header;
	BYTE SymbolCount;
	BYTE LineCount;
	BYTE StakeCount;
	BYTE MultiCap;
	BYTE Reels;			// sent so a client built against a different geometry
	BYTE Rows;			// can refuse rather than misdraw
	BYTE MaxBet;		// how many items per spin the machine allows
	WORD FreeSpinsLeft;
};

// Client -> GameServer, C1:D3:90.
struct PMSG_SLOT_SPIN_RECV
{
	PSBMSG_HEAD header;
	BYTE stake;			// accept-list index; ignored during free spins
	BYTE bet;			// how many of that item to stake, 1..MaxBet
};

struct PMSG_SLOT_RESULT_LINE
{
	BYTE line;			// payline index
	BYTE count;			// how many in the run, 3-5
	BYTE multi;			// the additive multiplier applied, already clamped
	DWORD pay;			// items won on this line, multiplier included
};

// GameServer -> Client, C2:D3:91. Followed by WinLineCount PMSG_SLOT_RESULT_LINE.
struct PMSG_SLOT_RESULT_SEND
{
	PSWMSG_HEAD header;
	BYTE result;					// SLOT_RESULT_*; nothing else is valid unless OK
	BYTE Cell[SLOT_CELLS];			// symbol id per cell, reel-major
	BYTE ScatterCount;
	BYTE WinLineCount;
	BYTE FreeSpinAwarded;			// free spins granted by THIS spin, 0 if none
	WORD FreeSpinsLeft;
	DWORD TotalPay;					// items won, bet multiplier already applied
	WORD StakeItemIndex;
	BYTE StakeLevel;
	BYTE WasFreeSpin;				// 1 when this spin cost nothing
	BYTE BetCount;					// echoed, so the client shows what was actually
									// staked rather than what it asked for
};

// Client -> GameServer, C1:D3:92.
struct PMSG_SLOT_CLAIM_RECV
{
	PSBMSG_HEAD header;
	BYTE slot;			// 0xFF = claim every row
};

struct PMSG_SLOT_CLAIM_ROW_SEND
{
	BYTE slot;
	WORD ItemIndex;
	BYTE Level;
	DWORD Count;
};

// GameServer -> Client, C2:D3:93. Followed by count PMSG_SLOT_CLAIM_ROW_SEND.
struct PMSG_SLOT_CLAIM_LIST_SEND
{
	PSWMSG_HEAD header;
	BYTE count;
	BYTE free;			// free rows left, so the client can warn before a spin
	BYTE result;		// SLOT_RESULT_* from the claim that prompted this, or OK
};

// ---- Reward box persistence (GameServer <-> DataServer) --------------------
// BYTE-FOR-BYTE duplicates of the same structs in the DataServer's SlotClaim.h.
// This codebase deliberately keeps GameServer<->DataServer wire structs
// duplicated rather than shared, so any field change must be mirrored on BOTH
// sides in the same commit and both binaries deployed together - BUILD.bat does
// not collect DataServer.exe, and a one-sided deploy misaligns the wire silently,
// surfacing as vanished winnings rather than a clean failure.

// One persisted row. 16 bytes to match the stride every other per-character item
// blob uses, so a future field costs no migration.
//
// ItemIndex == 0xFFFF is an empty row. That is what a 0xFF-filled blob decodes to,
// which is also what the DataServer returns for a character with no row yet - so
// "no data" and "empty box" are the same bytes and neither needs special-casing.
struct SLOTCLAIM_ROW_DATA
{
	WORD  ItemIndex;
	BYTE  Level;
	BYTE  Reserved;
	DWORD Count;
	BYTE  Padding[8];
};

// GameServer -> DataServer, C1:D9:34 - load request
struct SDHP_SLOTCLAIM_RECV
{
	PSBMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
};

// Per-character machine state that is NOT a held prize.
//
// This has to persist or two of its fields are exploitable: a daily pity limit
// that reset on relog would give unlimited forced wins, and a beginner-spin
// counter that reset would make beginner luck permanent. The losing streak is
// here for the opposite reason - losing it on relog only delays a player's pity
// win, but there is no reason to make them start again.
struct SLOTCLAIM_STATE_DATA
{
	DWORD LossStreak;
	DWORD PityDate;			// YYYYMMDD of the last forced win, 0 if never
	DWORD PityUsedToday;
	DWORD SpinCount;		// lifetime, for beginner luck
};

// GameServer -> DataServer, C2:D9:35 - save
struct SDHP_SLOTCLAIM_SAVE_RECV
{
	PSWMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
	BYTE SlotClaim[SLOT_CLAIM_SIZE][16];
	BYTE State[16];
};

// DataServer -> GameServer, C2:D9:34 - load reply
struct SDHP_SLOTCLAIM_SEND
{
	PSWMSG_HEAD header;
	WORD index;
	char account[11];
	char name[11];
	BYTE SlotClaim[SLOT_CLAIM_SIZE][16];
	BYTE State[16];
};

#pragma pack(pop)

// Stride pins. If one of these fires the wire has drifted - fix the struct, never
// the number.
static_assert(sizeof(PMSG_SLOT_OPEN_SYMBOL) == 5, "PMSG_SLOT_OPEN_SYMBOL must stay 5 bytes");
static_assert(sizeof(PMSG_SLOT_OPEN_LINE) == 5, "PMSG_SLOT_OPEN_LINE must stay 5 bytes");
static_assert(sizeof(PMSG_SLOT_OPEN_STAKE) == 42, "PMSG_SLOT_OPEN_STAKE must stay 42 bytes");
static_assert(sizeof(PMSG_SLOT_RESULT_LINE) == 7, "PMSG_SLOT_RESULT_LINE must stay 7 bytes");
static_assert(sizeof(PMSG_SLOT_CLAIM_ROW_SEND) == 8, "PMSG_SLOT_CLAIM_ROW_SEND must stay 8 bytes");
static_assert(sizeof(SLOTCLAIM_ROW_DATA) == 16, "SLOTCLAIM_ROW_DATA must stay 16 bytes");
static_assert(sizeof(SDHP_SLOTCLAIM_RECV) == 28, "SDHP_SLOTCLAIM_RECV must stay 28 bytes");
static_assert(sizeof(SLOTCLAIM_STATE_DATA) == 16, "SLOTCLAIM_STATE_DATA must stay 16 bytes");
static_assert(sizeof(SDHP_SLOTCLAIM_SAVE_RECV) == 173, "SDHP_SLOTCLAIM_SAVE_RECV must stay 173 bytes");
static_assert(sizeof(SDHP_SLOTCLAIM_SEND) == 173, "SDHP_SLOTCLAIM_SEND must stay 173 bytes");

//**********************************************//
//**********************************************//
//**********************************************//

class CSlotMachine
{
public:
	CSlotMachine();
	virtual ~CSlotMachine();

	void Load(char* path);

	// Pushes the configured NPC into the spawn table, so its position lives in
	// CustomSlotMachine.txt and not in MonsterSetBase. Must run after
	// gMonsterSetBase.LoadFolder and before SetMonsterData - see the call beside
	// gSauChangeItem.SetNPC() in CServerInfo::ReadMonsterInfo.
	void SetNPC();

	// NPC talk pre-hook. Returns 1 when this was our NPC and the click is
	// consumed; 0 falls through to the generic shop window.
	bool Dialog(LPOBJ lpObj,LPOBJ lpNpc);

	// Called when the player closes the machine, and on disconnect / map move.
	void CloseMachine(int aIndex);

	// ---- Spin ------------------------------------------------------------
	void CGSpinRecv(int aIndex,BYTE* lpRecv);

	// ---- Reward box ------------------------------------------------------
	int  GetClaimFreeCount(int aIndex);
	// Adds count of an item, merging into an existing row for the same item so a
	// run of small wins cannot fill the box. Returns false when there is no room.
	bool PushClaim(int aIndex,int itemIndex,int level,int count);
	void CGClaimRecv(int aIndex,BYTE* lpRecv);
	void SendClaimList(int aIndex,BYTE result);

	// Persistence. Load is requested once per character login and guarded by
	// SlotClaimLoad so it cannot run twice; save bails when the load never
	// happened, so an unloaded box can never overwrite a real one with blanks.
	void GDSlotClaimSend(int aIndex);
	void DGSlotClaimRecv(SDHP_SLOTCLAIM_SEND* lpMsg);
	void GDSlotClaimSaveSend(int aIndex);

	int GetEnable() { return this->m_Enable; }

private:
	void SendOpen(int aIndex);
	void SendResult(int aIndex,BYTE result);

	// Fills cell[] with one symbol id per cell from the per-reel weighted strips.
	void DrawGrid(BYTE* cell);

	// Evaluates cell[] against the active paylines. Returns total items won and
	// fills the win-line list; scatter count comes back separately because
	// scatters ignore paylines.
	int Evaluate(const BYTE* cell,PMSG_SLOT_RESULT_LINE* lines,int* lineCount,int* scatterCount);

	// Overwrites cell[] with a grid that is GUARANTEED to pay, used by the pity
	// and beginner-luck paths. Builds a real grid and lets Evaluate price it, so a
	// forced win can never pay something the paytable would not.
	//   shape 0 = smallest paying combination
	//   shape 1 = same, with a x2 wild in the run
	//   shape 2 = same, with a x4 wild in the run
	//   shape 3 = three scatters, so free spins rather than a line win
	void ForceWinGrid(BYTE* cell,int shape);

	// Picks a shape from the configured pity weights.
	int  RollPityShape();

	// The lowest-paying normal symbol, and the wild carrying a given value.
	int  LowestPayingSymbol();
	int  FindWildSymbol(int value);
	int  FindScatterSymbol();

	// Today as YYYYMMDD, for the once-a-day pity limit.
	DWORD TodayStamp();

	// Broadcasts the gold centre-screen notice every client shows for a big drop.
	// Built here rather than through PostMessageItemNotice, which formats the text
	// with the player name as its ONLY substitution and so cannot carry an amount.
	void AnnounceWin(LPOBJ lpObj,int amount,const char* what);

	// True when this symbol id is a wild, i.e. a multiplier.
	bool IsWild(int symbol);
	int  MultiplierValue(int symbol);

	bool TakeStake(LPOBJ lpObj,const SLOT_STAKE_INFO& stake,int count);
	bool CanAffordStake(LPOBJ lpObj,const SLOT_STAKE_INFO& stake,int count);

	// Moves one row into the inventory, then the bank for the remainder. Returns
	// a SLOT_RESULT_* - the row is left untouched unless the whole of it fits.
	// How many of an item will actually fit in the inventory right now. Binary
	// search, because CheckItemInventorySpaceForBundle only answers yes/no for a
	// given quantity and an item occupies a RECTANGLE, so no cell count can
	// substitute for asking it.
	int  InventoryRoomFor(LPOBJ lpObj,int itemIndex,int want);

	BYTE ClaimRow(int aIndex,int slot);

	char* GetMessage(int index);

public:
	int m_Enable;

private:
	int m_NpcClass;
	int m_NpcMap;
	int m_NpcX;
	int m_NpcY;
	int m_NpcDir;
	int m_Lines;				// active paylines; <= m_LineCount
	int m_FreeSpins;
	int m_MaxRetrigger;
	int m_MultiCap;
	int m_MaxPayoutCredits;
	int m_MaxBet;

	int m_SymbolCount;
	SLOT_SYMBOL_INFO m_Symbol[SLOT_MAX_SYMBOL];

	// Weight per symbol id, per reel. The only RTP dial that matters, and it is
	// per-reel on purpose: that is what lets scatters and high-value symbols be
	// confined to reels 1/3/5, which is how real machines control near-misses.
	int m_ReelWeight[SLOT_REELS][SLOT_MAX_SYMBOL];

	int m_LineCount;
	SLOT_LINE_INFO m_Line[SLOT_MAX_LINE];

	SLOT_PAY_INFO m_Pay[SLOT_MAX_SYMBOL];

	int m_StakeCount;
	SLOT_STAKE_INFO m_Stake[SLOT_MAX_STAKE];

	// Scatter bonus, losing-streak pity and beginner luck.
	SLOT_BONUS_INFO m_Bonus;

	std::map<int,SLOT_MESSAGE_INFO> m_MessageInfo;

	// Backing store for GetMessage's not-found text. Must not be a local: the
	// caller formats through the returned pointer.
	char m_DefaultMessage[256];
};

extern CSlotMachine gSlotMachine;
