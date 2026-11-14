// ItemReroll.h - "Excellent Item Reroller" NPC (1.0.50).
//
// The player talks to the NPC, picks an excellent item from the bag (right click / long press, the item STAYS in the bag),
// optionally locks some of its excellent options and presses Reroll: the jewels are paid and the server rolls a new option
// set (kept in memory, nothing changes on the item yet). The client spins a reel, then shows old vs new. Confirm pays the
// WCoin / Goblin Point price and only then replaces the item's options. Everything is configured in Data\Custom\CustomItemReroll.txt.
//
// Server authoritative: ownership (slot + serial + item index + current option mask), item group, jewels and coins are all
// checked here; the pending roll is removed BEFORE it is applied, so a replayed or duplicated confirm finds nothing.
#pragma once

#include "Protocol.h"
#include "User.h"
#include <map>

#define RR_MAX_DENY 32

#pragma pack(push,1)

// C1:D3:68 - the window opens (server -> client): configuration and prices
struct PMSG_RR_OPEN_SEND
{
	PSBMSG_HEAD header;
	BYTE PayMask;      // 1 = WCoin accepted, 2 = Goblin Point accepted, 4 = jewels accepted
	BYTE Pad;
	WORD JewelItem;    // item index of the jewel (GET_ITEM)
	WORD JewelCount;   // jewels per reroll
	WORD JewelPerLock; // extra jewels per locked option
	WORD SpinMs;       // length of the spin animation
	WORD JewelHave;    // jewels in the bag right now
	DWORD PriceWC;
	DWORD PriceGP;
	DWORD LockWC;      // extra per locked option
	DWORD LockGP;
	WORD PriceJW;      // Confirm price in jewels (PayMask bit 4)
	WORD LockJW;       // extra jewels per locked option
};

// C1:D3:69 - state of the window (server -> client), sent after every action
struct PMSG_RR_STATE_SEND
{
	PSBMSG_HEAD header;
	BYTE Code;         // eItemRerollCode
	BYTE Stage;        // 0 no item, 1 item chosen, 2 rolled (waiting for Confirm)
	BYTE Slot;         // bag slot of the chosen item
	BYTE Valid;        // excellent bits this item can have
	BYTE Old;          // option bits now on the item
	BYTE New;          // rolled bits (Stage 2)
	BYTE Locks;        // locks the roll was made with (Stage 2)
	BYTE Flags;        // bit 0: options the item has now cannot come up again (AllowSameOption = 0)
	WORD ItemIndex;    // GET_ITEM index of the chosen item
	WORD JewelHave;    // jewels in the bag (+ the Jewel Bank when UseJewelBank = 1)
	BYTE Weight[6];    // config chance weight of each excellent option for this item type (0 = never)
	BYTE LevelByte;    // the held item as the client's item bytes: level / skill / luck / option
	BYTE ExtByte;      // set (ancient) option
	BYTE Mult[6];      // price multiplier of each excellent option for this item type (final price = base x sum of the options it ends up with)
};

// C1:D3:69 - the window buttons (client -> server, UNENCRYPTED like every 0xD3 packet)
struct PMSG_RR_ACTION_RECV
{
	PSBMSG_HEAD header;
	BYTE Action;       // 0 choose item (Slot), 1 roll (Locks), 2 confirm (Pay 1 WCoin / 2 Goblin / 3 jewels), 3 drop the rolled result, 4 forget the item, 5 window closed
	BYTE Slot;
	BYTE Locks;        // bit n locked
	BYTE Pay;          // 1 = WCoin, 2 = Goblin Point, 3 = jewels
};

#pragma pack(pop)

static_assert(sizeof(PMSG_RR_OPEN_SEND) == 4+2+10+16+4,"RR open size");
static_assert(sizeof(PMSG_RR_STATE_SEND) == 4+8+4+6+6+2,"RR state size");
static_assert(sizeof(PMSG_RR_ACTION_RECV) == 8,"RR action size");

enum eItemRerollCode
{
	RR_OK = 0,
	RR_ITEM_CHOSEN = 1,
	RR_ROLLED = 2,
	RR_APPLIED = 3,
	RR_CLEARED = 4,
	RR_ERR_OFF = 10,
	RR_ERR_FAR = 11,          // window not open / too far from the NPC
	RR_ERR_BUSY = 12,         // trade, shop, other window
	RR_ERR_ITEM = 13,         // not a rerollable item
	RR_ERR_NOEXC = 14,        // item has no excellent option
	RR_ERR_GROUP = 15,        // item group switched off in the config
	RR_ERR_ANCIENT = 16,
	RR_ERR_JEWEL = 17,        // not enough jewels
	RR_ERR_COIN = 18,         // not enough WCoin / Goblin Points
	RR_ERR_PAY = 19,          // this currency is not accepted
	RR_ERR_LOCKS = 20,        // invalid locks / nothing left to roll
	RR_ERR_NODIFF = 21,       // no different result could be rolled
	RR_ERR_CHANGED = 22,      // the item changed (moved / traded / modified): the roll was dropped
	RR_ERR_NOROLL = 23,       // nothing rolled to confirm
	RR_ERR_EXPIRED = 24,
	RR_ERR_NOITEM = 25,       // no item chosen
	RR_ERR_INTERNAL = 26,
	RR_ERR_NOSPACE = 27       // no room in the bag to give the item back
};

struct RR_SESSION
{
	char Name[11];
	bool Open;
	DWORD OpenTick;
	int NpcIndex;
	// chosen item
	bool HasItem;
	int Slot;
	DWORD Serial;
	int ItemIndex;
	BYTE Old;
	// rolled result
	bool HasRoll;
	BYTE New;
	BYTE Locks;
	DWORD RollTick;
};

struct RR_HELD
{
	CItem Item;   // the item taken out of the bag while it sits on the NPC
	int Slot;     // the bag cell it came from
	bool HasPend; // a rolled result waits for Confirm (saved with the item, so closing the window does not lose the paid roll)
	BYTE PendNew;
	BYTE PendLocks;
	char Name[11];
};

class CItemReroll
{
public:
	CItemReroll();
	~CItemReroll();

	void Load(char* path);   // Data\Custom\CustomItemReroll.txt
	void MainProc();         // 1 second user loop: keeps the NPC spawned (or removes it when switched off)

	bool Dialog(LPOBJ lpObj,LPOBJ lpNpc);                          // NpcTalk.cpp
	void CGActionRecv(PMSG_RR_ACTION_RECV* lpMsg,int aIndex);      // Protocol.cpp, D3:69
	void OnUserLogout(LPOBJ lpObj);                                // User.cpp gObjDel: the held item goes back (or to the ground) before the character is saved

private:
	bool Served();
	RR_SESSION* GetSession(LPOBJ lpObj);
	bool CheckWindow(LPOBJ lpObj,RR_SESSION* s,BYTE* code);
	int  CheckItem(LPOBJ lpObj,int slot,BYTE* valid,BYTE* old);
	int  CheckItemPtr(CItem* lpItem,BYTE* valid,BYTE* old);
	bool ReturnHeld(LPOBJ lpObj,bool dropIfFull);
	bool SavedPath(LPOBJ lpObj,char* out,int outSize);
	bool SaveHeld(LPOBJ lpObj,RR_HELD* h);
	bool LoadHeld(LPOBJ lpObj);
	void DeleteSaved(LPOBJ lpObj);
	bool Roll(const BYTE* weight,BYTE valid,BYTE old,BYTE locks,BYTE* out);
	const BYTE* GetWeights(int itemIndex);
	void PayJewels(LPOBJ lpObj,int need,bool* ok);
	int  CountJewels(LPOBJ lpObj);
	DWORD PriceCoin(int pay,int locks,int units);
	const BYTE* GetMults(int itemIndex);
	int  Units(int itemIndex,BYTE result);
	int  JewelCost(int locks);
	void SendOpen(LPOBJ lpObj);
	void SendState(LPOBJ lpObj,RR_SESSION* s,BYTE code);
	void Log(LPOBJ lpObj,const char* format,...);

	void DoChoose(LPOBJ lpObj,RR_SESSION* s,int slot);
	void DoRoll(LPOBJ lpObj,RR_SESSION* s,int slot,BYTE locks);
	void DoConfirm(LPOBJ lpObj,RR_SESSION* s,int pay);

	CRITICAL_SECTION m_cs;
	std::map<int,RR_SESSION> m_Sessions;
	std::map<int,RR_HELD> m_Held;

	DWORD m_SecTick;
	int m_NpcIndex;

	// config
	int m_Switch;
	int m_NpcClass,m_NpcMap,m_NpcX,m_NpcY,m_NpcDir;
	int m_MaxDistance;
	int m_AllowWeapon,m_AllowShield,m_AllowArmor,m_AllowWing,m_AllowAccessory,m_AllowPet;
	int m_AllowAncient;
	int m_JewelItem;      // GET_ITEM(group,index)
	int m_JewelCount;
	int m_JewelPerLock;
	int m_PriceWC,m_PriceGP,m_LockWC,m_LockGP,m_PriceJW,m_LockJW;
	int m_MinOptions,m_MaxOptions;
	int m_PendingSeconds;
	int m_SpinMs;
	int m_PayWith,m_PriceOv,m_LockOv;
	BYTE m_MultWeapon[6],m_MultArmor[6],m_MultAccessory[6],m_MultWing[6];
	int m_UseJewelBank;
	int m_AllowSame;
	int m_KeepCount;
	BYTE m_RateWeapon[6],m_RateArmor[6],m_RateAccessory[6],m_RateWing[6];
	int m_Deny[RR_MAX_DENY];
	int m_DenyCount;
};

extern CItemReroll gItemReroll;
