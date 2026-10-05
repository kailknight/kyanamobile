// FriendMailWindow.h - the modern Friends window: friend list with class,
// level, resets, guild and server; a mailbox whose letters can carry items;
// writing letters with attachments. PC and mobile share it.
//
// The server decides which window players get (CustomConfig.ini
// FriendWindowModern, sent at login as C1:D3:E4). With it off - or against a
// server too old to send it - everything opens the legacy window as before.
//
// Packets (server side: GameServer FriendMail.h - keep the layouts in step):
//   E0 friend details, E1 send a letter with items / its result, E2 a
//   letter's items, E3 claim them, E4 config, E5 waiting-item count per letter.
// Plain letters, friends and whispers use the legacy packets unchanged.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "WSclient.h"

#define FRIENDMAIL_MAX_ITEMS		5
#define FRIENDMAIL_MAX_DETAILS		60
#define FRIENDMAIL_MAX_COUNTS		100
#define FRIENDMAIL_MEMO_MAX			1000

// Letters sent from this window carry this as their window id, so the legacy
// send-result handler (WSclient.cpp ReceiveLetterSendResult) can hand the
// result here instead of looking for a legacy write window.
#define FRIENDMAIL_LETTER_WINDOW_GUID	0x7FFFFF01

#pragma pack(push,1)

struct PMSG_FRIENDMAIL_SEND				// C2:D3:E1
{
	PSWMSG_HEAD header;
	char ToName[10];
	char Subject[32];
	BYTE ItemCount;
	BYTE Slot[FRIENDMAIL_MAX_ITEMS];
	BYTE CoinType;				// 0 none, 1 WCoinC, 2 WCoinP, 3 Goblin Points
	DWORD CoinAmount;
	WORD MemoSize;
	char Memo[FRIENDMAIL_MEMO_MAX];
};

struct PMSG_FRIENDMAIL_MEMO				// C1:D3:E2 / E3
{
	PSBMSG_HEAD header;
	WORD MemoIndex;
};

struct PMSG_FRIENDMAIL_EMPTY			// C1:D3:E0
{
	PSBMSG_HEAD header;
};

struct PMSG_FRIENDMAIL_DETAIL
{
	char Name[10];
	BYTE Class;
	WORD Level;
	WORD MasterLevel;
	WORD Resets;
	char Guild[8];
	BYTE Pending;				// 1 = they have not accepted the friend request yet
};

struct PMSG_FRIENDMAIL_DETAIL_RECV		// C2:D3:E0
{
	PSWMSG_HEAD header;
	BYTE Count;
	PMSG_FRIENDMAIL_DETAIL List[FRIENDMAIL_MAX_DETAILS];
};

struct PMSG_FRIENDMAIL_RESULT_RECV		// C1:D3:E1
{
	PSBMSG_HEAD header;
	BYTE Result;
};

struct PMSG_FRIENDMAIL_ITEMS_RECV		// C1:D3:E2
{
	PSBMSG_HEAD header;
	WORD MemoIndex;
	BYTE Count;
	BYTE Items[FRIENDMAIL_MAX_ITEMS][PACKET_ITEM_LENGTH];
	DWORD Coins[3];				// waiting WCoinC, WCoinP, Goblin Points
};

struct PMSG_FRIENDMAIL_CLAIM_RECV		// C1:D3:E3
{
	PSBMSG_HEAD header;
	WORD MemoIndex;
	BYTE Result;
	BYTE Claimed;
};

struct PMSG_FRIENDMAIL_CONFIG_RECV		// C1:D3:E4
{
	PSBMSG_HEAD header;
	BYTE Modern;
	BYTE FeeType;
	DWORD Fee;
	WORD FeeItem;
	BYTE FeeItemLevel;
	BYTE MaxPerLetter;
	BYTE ExpireDays;
	DWORD SendZen;				// postage, zen per letter
	BYTE CoinTypes;				// mailable coins: 1 WCoinC, 2 WCoinP, 4 Goblin Points
	DWORD CoinMax;				// most coins per letter, 0 = no limit
};

struct PMSG_FRIENDMAIL_COUNT
{
	WORD MemoIndex;
	BYTE Count;
};

struct PMSG_FRIENDMAIL_COUNTS_RECV		// C2:D3:E5
{
	PSWMSG_HEAD header;
	BYTE Count;
	PMSG_FRIENDMAIL_COUNT List[FRIENDMAIL_MAX_COUNTS];
};

#pragma pack(pop)

class CFriendMailWindow
{
public:
	CFriendMailWindow();
	~CFriendMailWindow();

	// True when the server asked for the modern window.
	bool IsModern() const { return this->m_ConfigReceived && this->m_Modern; }
	bool IsOpen() const;
	void Toggle();
	void Close();

	void DrawWindow();

	// Server -> client (Protocol.cpp, 0xD3 / 0xE0-0xE5).
	void RecvDetails(PMSG_FRIENDMAIL_DETAIL_RECV* lpMsg);
	void RecvSendResult(PMSG_FRIENDMAIL_RESULT_RECV* lpMsg);
	void RecvItems(PMSG_FRIENDMAIL_ITEMS_RECV* lpMsg);
	void RecvClaimResult(PMSG_FRIENDMAIL_CLAIM_RECV* lpMsg);
	void RecvConfig(PMSG_FRIENDMAIL_CONFIG_RECV* lpMsg);
	void RecvCounts(PMSG_FRIENDMAIL_COUNTS_RECV* lpMsg);

	// Legacy packet hooks (WSclient.cpp). Each returns true when the modern
	// window took the event, so the legacy pop-up is skipped.
	bool OnLetterText(DWORD letterId,const char* text);
	bool OnLetterSendResult(DWORD windowGuid,BYTE result);
	bool OnFriendRequest(const char* name);
	bool OnLegacyMessage(const char* text);
	void OnLetterDeleted(DWORD letterId);

private:
	enum View
	{
		VIEW_FRIENDS,
		VIEW_MAIL,
		VIEW_READ,
		VIEW_COMPOSE,
		VIEW_PICK,
		VIEW_ADD_FRIEND,
	};

	struct DETAIL
	{
		BYTE Class;
		WORD Level;
		WORD MasterLevel;
		WORD Resets;
		char Guild[9];
		bool Pending;
	};

	struct ATTACHMENT
	{
		int Slot;			// server inventory slot, -1 = empty
		short Type;			// to notice the item moving away before Send
	};

	void SetView(View view);
	void SetNotice(const char* text,DWORD color);
	void RequestDetails();
	void ClearReadItems();
	void OpenLetter(DWORD letterId);
	void BeginCompose(const char* toName,const char* subject);
	void SendCompose();
	int AttachmentCount() const;
	int ReadCoinCount() const;
	void HideInputs();
	void ShowInputs(bool to,bool subject,bool text,bool friendName,bool coin = false);

	// Rendering helpers.
	bool Button(float x,float y,float w,const char* text,bool enabled = true);
	bool RowClicked(float x,float y,float w,float h) const;
	void RenderAvatar(float x,float y,float size,BYTE cls) const;
	void RenderItemTile(float x,float y,float size,const ITEM* pItem,bool hovered) const;
	void RenderWrappedText(float x,float y,float w,int maxLines,const char* text,DWORD color) const;
	void ItemFeeText(char* out,int outSize,int itemCount) const;
	void NextCoinType();
	DWORD ReadCoinAmount() const;

	void RenderFriends(float x,float y,float w,float h);
	void RenderMail(float x,float y,float w,float h);
	void RenderRead(float x,float y,float w,float h);
	void RenderCompose(float x,float y,float w,float h);
	void RenderPick(float x,float y,float w,float h);
	void RenderAddFriend(float x,float y,float w,float h);
	void RenderRequestBanner(float x,float y,float w);

	// Server config.
	bool m_ConfigReceived;
	bool m_Modern;
	BYTE m_FeeType;
	DWORD m_Fee;
	WORD m_FeeItem;
	BYTE m_FeeItemLevel;
	BYTE m_MaxPerLetter;
	BYTE m_ExpireDays;
	DWORD m_SendZen;
	BYTE m_CoinTypes;
	DWORD m_CoinMax;

	// Data.
	std::map<std::string,DETAIL> m_Details;
	std::map<DWORD,BYTE> m_WaitingItems;	// letter -> waiting item count
	std::vector<std::string> m_FriendRequests;

	// UI state.
	View m_View;
	int m_Page;
	std::string m_SelectedFriend;
	DWORD m_SelectedLetter;
	char m_Notice[128];
	DWORD m_NoticeColor;
	DWORD m_NoticeTick;
	bool m_Click;
	bool m_WasDown;
	bool m_InputsShown;
	DWORD m_DetailRequestTick;

	// Read view.
	DWORD m_ReadLetter;
	bool m_ReadTextLoaded;
	char m_ReadText[FRIENDMAIL_MEMO_MAX + 1];
	ITEM* m_ReadItems[FRIENDMAIL_MAX_ITEMS];
	int m_ReadItemCount;
	bool m_ClaimPending;
	DWORD m_ReadCoins[3];

	// Compose view.
	ATTACHMENT m_Attach[FRIENDMAIL_MAX_ITEMS];
	int m_PickTarget;
	BYTE m_CoinType;			// coin picked to send, 0 = none available
	bool m_SendPending;
	DWORD m_SendTick;

	// Hovered item, drawn after everything else so nothing paints over it.
	const ITEM* m_HoverItem;
	float m_HoverX;
	float m_HoverY;
};

extern CFriendMailWindow* gFriendMailWindow;

// Every place that used to toggle INTERFACE_FRIEND calls this instead: the
// modern window when the server enabled it, the legacy one otherwise.
void ToggleFriendWindowRouted();
