// FriendMail.h - modern friend window data and item mail, GameServer side.
//
// Client <-> GameServer: head 0xD3, subcodes 0xE0-0xE5 (the client copies of
// these structs live in the client's FriendMailWindow.h - keep them in step).
// GameServer <-> DataServer: FriendMailProtocol.h, head 0xD9, 0x40-0x46.
//
// Sending: the client names up to MailItemMaxPerLetter inventory slots. They
// are checked (tradeable, not equipped, fee payable), the DataServer confirms
// the recipient is a friend, then - and only then - the items and the fee are
// taken, the character is saved, and the items go to the DataServer. Every
// check is repeated after that round trip, against the item serials captured
// at the start, so nothing the player does in between can be exploited.
//
// Claiming: the letter's waiting items are listed, inventory room for all of
// them is checked, then the DataServer claims them. Whatever the claim returned
// is placed; anything that no longer fits, or a player who left meanwhile,
// gets the claim undone.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Protocol.h"
#include "DSProtocol.h"
#include "FriendMailProtocol.h"
#include "ItemManager.h"
#include "User.h"

#pragma pack(push,1)

// ---- client -> GameServer ----------------------------------------------------

struct PMSG_FRIENDMAIL_SEND_RECV	// C2:D3:E1
{
	PSWMSG_HEAD header;
	char ToName[10];
	char Subject[32];
	BYTE ItemCount;
	BYTE Slot[FRIENDMAIL_MAX_ITEMS];
	BYTE CoinType;		// eFriendMailCoin, 0 = no coins
	DWORD CoinAmount;
	WORD MemoSize;
	char Memo[FRIENDMAIL_MEMO_MAX];
};

struct PMSG_FRIENDMAIL_MEMO_RECV	// C1:D3:E2 list items, C1:D3:E3 claim
{
	PSBMSG_HEAD header;
	WORD MemoIndex;
};

// ---- GameServer -> client ----------------------------------------------------

struct PMSG_FRIENDMAIL_DETAIL
{
	char Name[10];
	BYTE Class;		// 0xFF = character no longer exists
	WORD Level;
	WORD MasterLevel;
	WORD Resets;
	char Guild[8];
	BYTE Pending;	// 1 = they have not accepted the friend request yet
};

struct PMSG_FRIENDMAIL_DETAIL_SEND	// C2:D3:E0
{
	PSWMSG_HEAD header;
	BYTE Count;
	PMSG_FRIENDMAIL_DETAIL List[FRIENDMAIL_MAX_DETAILS];
};

struct PMSG_FRIENDMAIL_RESULT_SEND	// C1:D3:E1
{
	PSBMSG_HEAD header;
	BYTE Result;	// eFriendMailClientResult
};

struct PMSG_FRIENDMAIL_ITEMS_SEND	// C1:D3:E2
{
	PSBMSG_HEAD header;
	WORD MemoIndex;
	BYTE Count;
	BYTE Items[FRIENDMAIL_MAX_ITEMS][MAX_ITEM_INFO];
	DWORD Coins[3];		// waiting WCoinC, WCoinP, Goblin Points (eFriendMailCoin - 1)
};

struct PMSG_FRIENDMAIL_CLAIM_SEND	// C1:D3:E3
{
	PSBMSG_HEAD header;
	WORD MemoIndex;
	BYTE Result;	// eFriendMailClientResult
	BYTE Claimed;
};

struct PMSG_FRIENDMAIL_CONFIG_SEND	// C1:D3:E4
{
	PSBMSG_HEAD header;
	BYTE Modern;
	BYTE FeeType;		// 0 zen, 1 item, 2 WCoinC, 3 WCoinP, 4 Goblin Points
	DWORD Fee;			// per attached item
	WORD FeeItem;		// GET_ITEM(section,index), type 1 only
	BYTE FeeItemLevel;
	BYTE MaxPerLetter;
	BYTE ExpireDays;
	DWORD SendZen;		// postage: zen for every letter (MailSendZen)
	BYTE CoinTypes;		// which coins can be mailed: 1 WCoinC, 2 WCoinP, 4 Goblin Points
	DWORD CoinMax;		// most coins per letter, 0 = no limit
};

struct PMSG_FRIENDMAIL_COUNT
{
	WORD MemoIndex;
	BYTE Count;
};

struct PMSG_FRIENDMAIL_COUNTS_SEND	// C2:D3:E5
{
	PSWMSG_HEAD header;
	BYTE Count;
	PMSG_FRIENDMAIL_COUNT List[FRIENDMAIL_MAX_COUNTS];
};

#pragma pack(pop)

enum eFriendMailClientResult
{
	FRIENDMAIL_CLIENT_ERROR			= 0,
	FRIENDMAIL_CLIENT_OK			= 1,	// sent / claimed
	FRIENDMAIL_CLIENT_NOT_FRIEND	= 2,	// recipient is not on your friend list
	FRIENDMAIL_CLIENT_RETURNED		= 3,	// recipient's mailbox was full - it is in your mailbox instead
	FRIENDMAIL_CLIENT_NO_FEE		= 4,	// not enough fee items (bag + Jewel Bank)
	FRIENDMAIL_CLIENT_BAD_ITEM		= 5,	// an item cannot be mailed (untradeable, equipped, moved)
	FRIENDMAIL_CLIENT_BUSY			= 6,	// a previous send/claim is still in progress, or another window is open
	FRIENDMAIL_CLIENT_TOO_MANY		= 7,	// more items than MailItemMaxPerLetter
	FRIENDMAIL_CLIENT_FULL			= 8,	// claim: not enough inventory room
	FRIENDMAIL_CLIENT_NOTHING		= 9,	// claim: no waiting items on this letter
	FRIENDMAIL_CLIENT_BAD_COIN		= 10,	// that coin cannot be mailed, or the amount is over the limit
	FRIENDMAIL_CLIENT_NO_ZEN		= 11,	// not enough zen for postage (+ a zen item fee)
	FRIENDMAIL_CLIENT_NO_COINS		= 12,	// not enough of the coins being sent (+ a coin item fee)
};

class CFriendMail
{
public:
	CFriendMail();

	// Login (CSProtocol.cpp FriendListRequest): config to the client, and the
	// attachment counts (which also runs the DataServer's expiry sweep).
	void OnFriendListRequested(int aIndex);

	// Client -> GameServer (Protocol.cpp, 0xD3 / 0xE0-0xE3).
	void CGDetailRequest(int aIndex);
	void CGSendRequest(PMSG_FRIENDMAIL_SEND_RECV* lpMsg,int aIndex);
	void CGItemsRequest(PMSG_FRIENDMAIL_MEMO_RECV* lpMsg,int aIndex);
	void CGClaimRequest(PMSG_FRIENDMAIL_MEMO_RECV* lpMsg,int aIndex);

	// DataServer -> GameServer (DSProtocol.cpp, 0xD9 / 0x40-0x46).
	void DGDetailRecv(SDHP_FRIENDMAIL_DETAIL_RECV* lpMsg);
	void DGCheckRecv(SDHP_FRIENDMAIL_CHECK_RECV* lpMsg);
	void DGStoreRecv(SDHP_FRIENDMAIL_STORE_RECV* lpMsg);
	void DGItemsRecv(SDHP_FRIENDMAIL_ITEMS_RECV* lpMsg);
	void DGClaimRecv(SDHP_FRIENDMAIL_ITEMS_RECV* lpMsg);
	void DGCountsRecv(SDHP_FRIENDMAIL_COUNTS_RECV* lpMsg);

private:
	enum ePendingKind { PENDING_NONE = 0, PENDING_SEND = 1, PENDING_CLAIM = 2 };

	struct PENDING
	{
		BYTE Kind;
		DWORD Tick;
		char Account[11];	// whose operation this is: a slot reused by another account is stale
		char ToName[11];
		char Subject[32];
		WORD MemoSize;
		char Memo[FRIENDMAIL_MEMO_MAX];
		BYTE ItemCount;
		BYTE Slot[FRIENDMAIL_MAX_ITEMS];
		int ItemIndex[FRIENDMAIL_MAX_ITEMS];
		DWORD ItemSerial[FRIENDMAIL_MAX_ITEMS];
		BYTE CoinType;
		DWORD CoinAmount;
		DWORD MemoIndex;
	};

	bool BeginPending(int aIndex,BYTE kind);
	void EndPending(int aIndex);
	bool CheckItems(LPOBJ lpObj,const PENDING& pending,bool afterRoundTrip);
	// What a send costs on top of the attachments: postage (zen) plus the
	// per-item fee, and the coins themselves, as totals per currency.
	struct COSTS
	{
		DWORD Zen;
		DWORD FeeItems;		// count of MailItemFeeItem
		int Coin[3];		// WCoinC, WCoinP, Goblin Points
	};

	void GetCosts(const PENDING& pending,COSTS* costs);
	BYTE CanPay(LPOBJ lpObj,const PENDING& pending,const COSTS& costs);
	bool TakeCoins(LPOBJ lpObj,const COSTS& costs);
	void TakeZenAndFeeItems(LPOBJ lpObj,const COSTS& costs,int feeFromBag);
	// Fee items: the bag (inventory + expansions, attachments excluded) first,
	// then the Jewel Bank for whatever the bag is short.
	int CountFeeItemsInBag(LPOBJ lpObj,const PENDING& pending);
	int CountFeeItemsInBank(int aIndex);
	void SendResult(int aIndex,BYTE result);
	void SendClaimResult(int aIndex,DWORD memoIndex,BYTE result,BYTE claimed);
	void SendNameRequest(int aIndex,BYTE subcode);

	PENDING m_Pending[MAX_OBJECT];
};

extern CFriendMail gFriendMail;
