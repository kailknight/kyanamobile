// FriendMailProtocol.h - GameServer <-> DataServer messages for the modern
// friend window and item mail. Head 0xD9, subcodes 0x40-0x46.
//
// IDENTICAL COPIES live in 2.DataServer/DataServer and 4.GameServer/GameServer.
// Change both or neither: the two sides read these structs straight off the
// wire, so any difference in layout corrupts every message.
//
// Item mail rules (see FriendMail_Schema.sql for the database half):
//   * The GameServer removes the items and the fee BEFORE asking the
//     DataServer to store them, and saves the character first, so an item is
//     never both in the inventory and in the mailbox.
//   * The DataServer never fails a store outright: if the letter cannot be
//     written to the recipient, it goes to the sender's own mailbox instead.
//   * A claim only succeeds for items whose row flipped from waiting to
//     claimed in that call (WZ_MailItem_Claim). A claim the GameServer cannot
//     complete is undone with 0x45.

#pragma once

#define FRIENDMAIL_MAX_ITEMS		5	// per letter (CustomConfig MailItemMaxPerLetter is capped to this)
#define FRIENDMAIL_MAX_DETAILS		60	// friends per detail list
#define FRIENDMAIL_MAX_COUNTS		100	// letters per attachment-count list
#define FRIENDMAIL_ITEM_BYTES		16	// one inventory slot (CItemManager::DBItemByteConvert)
#define FRIENDMAIL_MEMO_MAX			1000
#define FRIENDMAIL_MAX_ATTACH		(FRIENDMAIL_MAX_ITEMS + 1)	// items + one coin row

// What an attachment row carries (T_FriendMailItem.CoinType).
enum eFriendMailCoin
{
	FRIENDMAIL_COIN_NONE		= 0,	// an item (ItemData)
	FRIENDMAIL_COIN_WCOINC		= 1,	// CoinAmount WCoin (C)  - lpObj->Coin1
	FRIENDMAIL_COIN_WCOINP		= 2,	// CoinAmount WCoin (P)  - lpObj->Coin2
	FRIENDMAIL_COIN_GOBLIN		= 3,	// CoinAmount Goblin Points - lpObj->Coin3
};

// Results of 0x41 / 0x42.
enum eFriendMailResult
{
	FRIENDMAIL_ERROR		= 0,
	FRIENDMAIL_OK			= 1,	// 0x41: may send / 0x42: delivered to the recipient
	FRIENDMAIL_NOT_FRIEND	= 2,	// 0x41: recipient not on the sender's friend list (or no such character)
	FRIENDMAIL_RETURNED		= 3,	// 0x42: recipient's mailbox refused it - stored in the sender's mailbox
};

struct FRIENDMAIL_DETAIL
{
	char Name[11];
	BYTE Class;
	WORD Level;
	WORD MasterLevel;
	WORD Resets;
	char Guild[9];
	BYTE Pending;		// 1 = on the list but they have not accepted yet (T_FriendList.Del)
};

struct FRIENDMAIL_ITEM
{
	__int64 ItemID;
	BYTE ItemData[FRIENDMAIL_ITEM_BYTES];
	BYTE CoinType;		// eFriendMailCoin; ItemData is unused when not NONE
	DWORD CoinAmount;
};

struct FRIENDMAIL_COUNT
{
	DWORD MemoIndex;
	BYTE Count;
	BYTE Reserved[3];
};

// 0x40 friend details, 0x46 attachment counts (+ the expiry sweep): request.
struct SDHP_FRIENDMAIL_NAME_REQ
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	char Name[11];
};

// 0x40 reply (C2).
struct SDHP_FRIENDMAIL_DETAIL_RECV
{
	PSWMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	BYTE Count;
	FRIENDMAIL_DETAIL List[FRIENDMAIL_MAX_DETAILS];
};

// 0x41 may this letter be sent? Nothing is changed by it.
struct SDHP_FRIENDMAIL_CHECK_REQ
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	char Name[11];
	char ToName[11];
};

struct SDHP_FRIENDMAIL_CHECK_RECV
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	char ToName[11];
	BYTE Result;		// eFriendMailResult
};

// 0x42 store a letter with its items and coins (C2). Sent only after the items,
// the coins and the fees are already gone from the sender.
struct SDHP_FRIENDMAIL_STORE_REQ
{
	PSWMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	char Name[11];
	char ToName[11];
	char Subject[32];
	BYTE Dir;
	BYTE Action;
	BYTE Photo[18];
	BYTE ItemCount;
	BYTE ExpireDays;
	BYTE Items[FRIENDMAIL_MAX_ITEMS][FRIENDMAIL_ITEM_BYTES];
	BYTE CoinType;		// eFriendMailCoin - coins taken from the sender with the items
	DWORD CoinAmount;
	WORD MemoSize;
	char Memo[FRIENDMAIL_MEMO_MAX];
};

struct SDHP_FRIENDMAIL_STORE_RECV
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	char ToName[11];
	BYTE Result;		// eFriendMailResult
	BYTE ItemCount;
};

// 0x43 list a letter's waiting items, 0x44 claim them.
struct SDHP_FRIENDMAIL_MEMO_REQ
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	char Name[11];
	DWORD MemoIndex;
};

// 0x43 reply: the waiting items. 0x44 reply: the items THIS claim took.
struct SDHP_FRIENDMAIL_ITEMS_RECV
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	char Name[11];		// so a claim can still be undone for a player who left mid-claim
	BYTE Count;
	DWORD MemoIndex;
	FRIENDMAIL_ITEM Items[FRIENDMAIL_MAX_ATTACH];
};

// 0x45 undo a claim the GameServer could not complete. No reply.
struct SDHP_FRIENDMAIL_UNCLAIM_REQ
{
	PSBMSG_HEAD header;
	char Name[11];
	BYTE Count;
	__int64 ItemID[FRIENDMAIL_MAX_ATTACH];
};

// 0x46 reply (C2): waiting-item count per letter. Also pushed unprompted to a
// recipient who is online when a letter with items arrives.
struct SDHP_FRIENDMAIL_COUNTS_RECV
{
	PSWMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	BYTE Count;
	FRIENDMAIL_COUNT List[FRIENDMAIL_MAX_COUNTS];
};
