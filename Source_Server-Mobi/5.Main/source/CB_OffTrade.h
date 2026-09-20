#pragma once
#include "Protocol.h"


class CB_OffTrade
{
public:
	enum eOffTradeType
	{
		eOFF_ZEN,
		eOFF_WCOINP,
		eOFF_WCOINC,
		eOFF_CHAOS,
		eOFF_BLESS,
		eOFF_SOUL,
	};

	// The store types for the server's custom exchange items, matching
	// CUSTOM_STORE_TYPE_ITEM and MAX_CUSTOM_STORE_ITEM in the GameServer's
	// CustomStore.h / Protocol.h. Slot n is type eOFF_CUSTOMITEM + n.
	//
	// 8 rather than 6, because the Type field of the off-trade packet carries
	// commands as well as store types: 6 means "go offline" and 7 means "close
	// store". Changing this without changing the server would open the wrong
	// thing, or nothing.
	enum
	{
		eOFF_CUSTOMITEM = 8,
		MAX_CUSTOMITEM = 3,
	};

	struct PMSG_OFFTRADE_SEND
	{
		PSBMSG_HEAD header;
		int Type;
	};

	struct PMSG_OFFTRADE_RECV
	{
		PSBMSG_HEAD header;
		int Type;
	};

	struct PMSG_SHOPACTIVE_RECV
	{
		PSBMSG_HEAD header;
		int Active;
		int Type;
	};

	// C1:F3:F5 - which items the server's custom-item stores trade in.
	//
	// The item NAME is not carried: it is looked up in this client's own item
	// table, so the price line and the warnings always agree with the item the
	// server actually takes. The short BUTTON label is carried, because it is not
	// the item name - a 38px button cannot hold one - and there is nowhere else
	// this client could learn it.
	struct PMSG_CUSTOMSTORE_ITEM_INFO_SLOT
	{
		int Enabled;
		int ItemIndex;
		char Button[12];
	};

	struct PMSG_CUSTOMSTORE_ITEM_INFO_RECV
	{
		PSBMSG_HEAD header;
		PMSG_CUSTOMSTORE_ITEM_INFO_SLOT Slot[MAX_CUSTOMITEM];
	};


	CB_OffTrade();
	~CB_OffTrade();
	void DrawButton(float X, float Y);
	void RenderTextNotice(float X, float Y);
	void RecvPShop(PMSG_OFFTRADE_RECV* Data);
	void CGSendOffTrade(int Type);
	void PShopActiveRecv(PMSG_SHOPACTIVE_RECV* Data);

	void ShowPrice(int TextNum, char* Price);
	void ShowMessNotice();

	void CustomStoreItemInfoRecv(PMSG_CUSTOMSTORE_ITEM_INFO_RECV* Data);

	// A slot's exchange item name out of this client's item table, or NULL when
	// the server has not offered that store (or named an item this client does not
	// have). Callers must handle NULL - that is what says "do not draw the
	// button". Takes a SLOT (0-based), not a store type.
	const char* GetCustomItemName(int slot);

	// A slot's short button label, or NULL when the slot is off or unlabelled.
	const char* GetCustomItemButton(int slot);

	// Slot for a store type, or -1. The one place the offset is written down on
	// this side; the GameServer has its own GetCustomItemSlot.
	int GetCustomItemSlot(int type);

	int m_OfftradeType;

	// Set from the server as the character enters the game, so every slot defaults
	// to "no custom store" until told otherwise. A server without the feature
	// never sends the packet and no button appears.
	int m_CustomItemEnabled[MAX_CUSTOMITEM];
	int m_CustomItemIndex[MAX_CUSTOMITEM];
	char m_CustomItemButton[MAX_CUSTOMITEM][12];
	int TypeShop;
	int ShopActive;
	int ShopList;

	DWORD ClickTickCount;
};

extern CB_OffTrade* gCB_OffTrade;