// CustomStore.h: interface for the CCustomStore class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "PersonalShop.h"
#include "User.h"

// The currency a custom store trades in, as carried in PShopCustomType and in
// the Type field of the off-trade packets. 0-5 are the vanilla six and cannot
// move - the client hardcodes them, and so does CGOffTradeRecv.
//
// 6 and 7 are NOT store types: on the wire they are the "go offline" and "close
// store" commands, which share the same Type field. That is why the custom-item
// stores start at 8 and not 6.
#define CUSTOM_STORE_TYPE_JOB			0
#define CUSTOM_STORE_TYPE_JOS			1
#define CUSTOM_STORE_TYPE_JOC			2
#define CUSTOM_STORE_TYPE_WCC			3
#define CUSTOM_STORE_TYPE_WCP			4
#define CUSTOM_STORE_TYPE_WCG			5

// Custom exchange items, one store type each: 8, 9, 10.
//
// MAX_CUSTOM_STORE_ITEM is 3, and lives in Protocol.h because the info packet
// needs it. Three is what fits: the shop window draws its currency buttons three
// to a row, and the vanilla six already fill two rows. A fourth would need a
// fourth row, and there is no room for one above the Exit/Open/Close buttons.
// Raising it without redrawing CB_OffTrade::DrawButton would give the extra store
// a working command and no button.
#define CUSTOM_STORE_TYPE_ITEM			8
#define CUSTOM_STORE_TYPE_ITEM_LAST		(CUSTOM_STORE_TYPE_ITEM + MAX_CUSTOM_STORE_ITEM - 1)

class CCustomStore
{
public:
	CCustomStore();
	virtual ~CCustomStore();
	void ReadCustomStoreInfo(char* section,char* path);
	bool CommandCustomStore(LPOBJ lpObj,char* arg);
	bool CommandCustomStoreOffline(LPOBJ lpObj,char* arg);
	bool OpenCustomStore(LPOBJ lpObj,int type);
	bool OnPShopOpen(LPOBJ lpObj);
	void OnPShopClose(LPOBJ lpObj);
	void OnPShopSecondProc(LPOBJ lpObj);
	void OnPShopAlreadyConnected(LPOBJ lpObj);
	void OnPShopItemList(LPOBJ lpObj,LPOBJ lpTarget);
	bool OnPShopBuyItemRecv(PMSG_PSHOP_BUY_ITEM_RECV* lpMsg,int aIndex);
	static void OnPShopBuyItemCallbackRecv(LPOBJ lpObj,LPOBJ lpTarget,DWORD slot,DWORD WCoinC,DWORD WCoinP,DWORD GoblinPoint);
	void CGOffTradeRecv(PMSG_OFFTRADE_RECV* lpMsg, int aIndex);
	void GCOffTradeSend(int aIndex,int bIndex);
	void GCOffActiveSend(int aIndex, int active);

	// Tells one client which items the custom-item stores trade in, so the shop
	// window can draw and label their buttons. Every slot is sent, enabled or not,
	// so a slot being switched off is stated rather than left to be inferred.
	//
	// Sent once as the character enters the game, because the buttons have to be
	// labelled before any shop is opened.
	void GCCustomStoreItemInfoSend(int aIndex);

	// Whether a custom-item store exists. Off unless the ini both switches that
	// slot on and names a real item. Out-of-range slots answer "off" rather than
	// reading past the array - the slot can come from a store type off the wire.
	bool IsCustomItemStore(int slot);

	// The client-side item index (GET_ITEM) of a slot's exchange item, or -1.
	int GetCustomItemIndex(int slot);

	// Which custom-item slot a store type refers to, or -1 if it is not one of
	// them. The one place the type-to-slot offset is written down.
	int GetCustomItemSlot(int type);

	// The custom-item stores' buy path. Split out of OnPShopBuyItemRecv because it
	// shares none of the jewel path: one item type, no bundle tiers, and no
	// DataServer round trip - the payment is inventory on both sides, so it
	// completes synchronously.
	bool OnPShopBuyItemWithCustomItem(LPOBJ lpObj,LPOBJ lpTarget,int invenSlot);
public:
	//int m_CustomStoreSwitch;
	int m_CustomStoreMapZone;
	//int m_CustomStoreEnable[4];
	//int m_CustomStoreRequireLevel[4];
	//int m_CustomStoreRequireReset[4];
	int m_CustomStoreTime[4];
	//char m_CustomStoreCommandSyntax[32];
	char m_CustomStoreCommandJoBSyntax[32];
	char m_CustomStoreCommandJoSSyntax[32];
	char m_CustomStoreCommandJoCSyntax[32];
	char m_CustomStoreCommandWCCSyntax[32];
	char m_CustomStoreCommandWCPSyntax[32];
	char m_CustomStoreCommandWCGSyntax[32];
	char m_CustomStoreJoBName[36];
	char m_CustomStoreJoSName[36];
	char m_CustomStoreJoCName[36];
	char m_CustomStoreWCCName[36];
	char m_CustomStoreWCPName[36];
	char m_CustomStoreWCGName[36];

	// Custom exchange items. Vanilla only ever allowed the three jewels and the
	// three cash currencies; these are three further stores whose currency is an
	// ordinary inventory item named in the ini.
	//
	// They pay out of the buyer's inventory straight into the seller's, so unlike
	// the jewel types there is no bundle arithmetic (a jewel price of 30 can be
	// paid with one Bundle of 30) and unlike the cash types there is no
	// DataServer point balance involved. One item, one unit of price.
	int  m_CustomStoreItemSwitch[MAX_CUSTOM_STORE_ITEM];
	char m_CustomStoreItemCommandSyntax[MAX_CUSTOM_STORE_ITEM][32];
	char m_CustomStoreItemName[MAX_CUSTOM_STORE_ITEM][36];
	int  m_CustomStoreItemCat[MAX_CUSTOM_STORE_ITEM];
	int  m_CustomStoreItemIndex[MAX_CUSTOM_STORE_ITEM];
	int  m_CustomStoreItemLevel[MAX_CUSTOM_STORE_ITEM];

	// The short text on the button, separate from the item and from the shop
	// title. A currency button is 38px wide - about six characters of the bold
	// font - and item names do not fit: "Jewel of Bless" would draw as a smear.
	// The full item name is still what the price line and the warnings use, and
	// that comes from the client's own item table rather than from here.
	char m_CustomStoreItemButton[MAX_CUSTOM_STORE_ITEM][12];
	//char m_CustomStoreText1[128];
	//char m_CustomStoreText2[128];
	//char m_CustomStoreText3[128];
	//char m_CustomStoreText4[128];
	//char m_CustomStoreText5[128];
	//char m_CustomStoreText6[128];
	//char m_CustomStoreText7[128];
	//char m_CustomStoreText8[128];
	//char m_CustomStoreText9[128];
	//char m_CustomStoreText10[128];
	//char m_CustomStoreText11[128];
	//char m_CustomStoreText12[128];
	//char m_CustomStoreText13[128];
	//int m_CustomStoreOfflineSwitch;
	int m_CustomStoreOfflineGPGain;
	int m_CustomStoreOfflineMapZone;
	//int m_CustomStoreOfflineEnable[4];
	//int m_CustomStoreOfflineRequireLevel[4];
	//int m_CustomStoreOfflineRequireReset[4];
	//char m_CustomStoreOfflineCommandSyntax[32];
	//char m_CustomStoreOfflineText1[128];
	//char m_CustomStoreOfflineText2[128];
	//char m_CustomStoreOfflineText3[128];
	//char m_CustomStoreOfflineText4[128];
	//char m_CustomStoreOfflineText5[128];
};

extern CCustomStore gCustomStore;
