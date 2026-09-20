#include "StdAfx.h"
#include "CB_OffTrade.h"
#include "NewUISystem.h"
#include "CBInterface.h"
#include "CUIController.h"
#include "CharacterManager.h"
#include "Util.h"
#include "Protocol.h"
#include "NewUIBase.h"
#include "Other.h"
#include "ZzzInterface.h"


CB_OffTrade* gCB_OffTrade;

CB_OffTrade::CB_OffTrade()
{
	this->ShopActive = 0;
	this->TypeShop = -1;
	this->ClickTickCount = GetTickCount();

	for (int slot = 0; slot < MAX_CUSTOMITEM; slot++)
	{
		this->m_CustomItemEnabled[slot] = 0;
		this->m_CustomItemIndex[slot] = -1;
		this->m_CustomItemButton[slot][0] = '\0';
	}
}

void CB_OffTrade::CustomStoreItemInfoRecv(PMSG_CUSTOMSTORE_ITEM_INFO_RECV* Data)
{
	for (int slot = 0; slot < MAX_CUSTOMITEM; slot++)
	{
		this->m_CustomItemEnabled[slot] = Data->Slot[slot].Enabled;
		this->m_CustomItemIndex[slot] = Data->Slot[slot].ItemIndex;

		// Copied one short of the buffer and terminated by hand: the label came off
		// the wire and is about to be drawn as a C string, so nothing may assume the
		// sender terminated it.
		strncpy(this->m_CustomItemButton[slot], Data->Slot[slot].Button, sizeof(this->m_CustomItemButton[slot]) - 1);
		this->m_CustomItemButton[slot][sizeof(this->m_CustomItemButton[slot]) - 1] = '\0';
	}
}

int CB_OffTrade::GetCustomItemSlot(int type)
{
	const int slot = type - eOFF_CUSTOMITEM;

	if (slot < 0 || slot >= MAX_CUSTOMITEM)
	{
		return -1;
	}

	return slot;
}

const char* CB_OffTrade::GetCustomItemName(int slot)
{
	if (slot < 0 || slot >= MAX_CUSTOMITEM)
	{
		return NULL;
	}

	if (this->m_CustomItemEnabled[slot] == 0)
	{
		return NULL;
	}

	// The index comes off the wire, so it is range-checked before it is used as a
	// subscript. ItemAttribute is also read before the item table has loaded on
	// some paths, hence the NULL guard.
	if (this->m_CustomItemIndex[slot] < 0 || this->m_CustomItemIndex[slot] >= MAX_ITEM)
	{
		return NULL;
	}

	if (ItemAttribute == NULL)
	{
		return NULL;
	}

	const char* name = ItemAttribute[this->m_CustomItemIndex[slot]].Name;

	// An empty name means this client's item table has no such item.
	if (name[0] == '\0')
	{
		return NULL;
	}

	return name;
}

const char* CB_OffTrade::GetCustomItemButton(int slot)
{
	if (slot < 0 || slot >= MAX_CUSTOMITEM)
	{
		return NULL;
	}

	if (this->m_CustomItemEnabled[slot] == 0)
	{
		return NULL;
	}

	// An unlabelled slot draws no button rather than a blank one. The server logs a
	// warning at startup for exactly this case, so it is visible there too.
	if (this->m_CustomItemButton[slot][0] == '\0')
	{
		return NULL;
	}

	return this->m_CustomItemButton[slot];
}


CB_OffTrade::~CB_OffTrade()
{
}



void CB_OffTrade::DrawButton(float X, float Y)
{
	//gInterface.DrawMessage(1, "Shop Active %d, Type %d", this->ShopActive, this->TypeShop);
	const BYTE state[3] = { 0, 1, 2 };
	X = X + 10;
	float bStartX = X;
	float bStartY = Y;
	if (this->ShopActive == 1 && this->TypeShop != -1)
	{
		if (g_pBCustomMenuInfo->DrawButton(bStartX + 30, bStartY + 320, 140, 12, "Close Store", 120))
		{

			CGSendOffTrade(7);
		}
		if (g_pBCustomMenuInfo->DrawButton(bStartX + 30, bStartY + 320 + 40, 140, 12, "Offline Store", 120))
		{
			gInterface.m_Disconnect = true;
			CGSendOffTrade(6);
			
		}
	}
	else
	{
		// Rows sit 8px higher than they used to, and 26px apart rather than 27.
		// Three rows now have to fit between the text block above (which ends
		// around Y+310) and the Exit/Open/Close buttons at Y+391: at the old Y+320
		// start with a 27px step the third row would have run into them.
		const float rowTop = 312.0f;
		const float rowStep = 26.0f;

		if (UIController.BButton(gInterface.Data[IMG_31617].ModelID, bStartX + 30, bStartY + rowTop, 38, 23, 38, 23, 3, state))
		{
			CGSendOffTrade(0);
		}
		TextDraw((HFONT)g_hFontBold, bStartX + 30, bStartY + rowTop + 5, 0xFFC738FF, 0x0, 39, 0, 3, "Bless");

		bStartX += 40;
		if (UIController.BButton(gInterface.Data[IMG_31617].ModelID, bStartX + 30, bStartY + rowTop, 38, 23, 38, 23, 3, state))
		{
			CGSendOffTrade(1);
		}
		TextDraw((HFONT)g_hFontBold, bStartX + 30, bStartY + rowTop + 5, 0xFFC738FF, 0x0, 39, 0, 3, "Soul");

		bStartX += 40;
		if (UIController.BButton(gInterface.Data[IMG_31617].ModelID, bStartX + 30, bStartY + rowTop, 38, 23, 38, 23, 3, state))
		{
			CGSendOffTrade(2);
		}
		TextDraw((HFONT)g_hFontBold, bStartX + 30, bStartY + rowTop + 5, 0xFFC738FF, 0x0, 39, 0, 3, "Chaos");

		//=======================
		bStartX = X;
		bStartY += rowStep;
		if (UIController.BButton(gInterface.Data[IMG_31617].ModelID, bStartX + 30, bStartY + rowTop, 38, 23, 38, 23, 3, state))
		{
			CGSendOffTrade(3);
		}
		TextDraw((HFONT)g_hFontBold, bStartX + 30, bStartY + rowTop + 5, 0xFFC738FF, 0x0, 39, 0, 3, "WC");


		bStartX += 40;
		if (UIController.BButton(gInterface.Data[IMG_31617].ModelID, bStartX + 30, bStartY + rowTop, 38, 23, 38, 23, 3, state))
		{
			CGSendOffTrade(4);
		}
		TextDraw((HFONT)g_hFontBold, bStartX + 30, bStartY + rowTop + 5, 0xFFC738FF, 0x0, 39, 0, 3, "WP");


		bStartX += 40;
		if (UIController.BButton(gInterface.Data[IMG_31617].ModelID, bStartX + 30, bStartY + rowTop, 38, 23, 38, 23, 3, state))
		{
			CGSendOffTrade(5);
		}
		TextDraw((HFONT)g_hFontBold, bStartX + 30, bStartY + rowTop + 5, 0xFFC738FF, 0x0, 39, 0, 3, "GP");

		//=======================
		// A third row for the server's custom exchange items - same grid, same
		// button size, up to three of them.
		//
		// They are PACKED left to right rather than drawn at the column matching
		// their slot: with only slot 2 configured, a lone button in the middle of
		// the row reads as a layout bug rather than as a deliberate single option.
		bStartX = X;
		bStartY += rowStep;

		int column = 0;

		for (int slot = 0; slot < MAX_CUSTOMITEM; slot++)
		{
			const char* customButton = GetCustomItemButton(slot);

			// No label means the slot is off, or the ini forgot its button text. Either
			// way there is nothing to draw, and drawing an unlabelled button would give
			// the player something to press with no idea what it costs.
			if (customButton == NULL)
			{
				continue;
			}

			const float buttonX = bStartX + 30 + (column * 40);

			if (UIController.BButton(gInterface.Data[IMG_31617].ModelID, buttonX, bStartY + rowTop, 38, 23, 38, 23, 3, state))
			{
				CGSendOffTrade(eOFF_CUSTOMITEM + slot);
			}
			TextDraw((HFONT)g_hFontBold, buttonX, bStartY + rowTop + 5, 0xFFC738FF, 0x0, 39, 0, 3, (char*)customButton);

			column++;
		}
	}
}

void CB_OffTrade::CGSendOffTrade(int Type)
{
	if (GetTickCount() > this->ClickTickCount)
	{
		PMSG_OFFTRADE_SEND pMsg;
		pMsg.header.set(0xF3, 0xEB, sizeof(pMsg));
		pMsg.Type = Type;
		DataSend((BYTE*)& pMsg, pMsg.header.size);

		this->ClickTickCount = GetTickCount() + 1000;
		//gInterface.DrawMessage(1, "CGSendOffTrade %d", Type);
	}
}

void CB_OffTrade::RecvPShop(PMSG_OFFTRADE_RECV* Data)
{
	this->ShopList = 1;
	this->TypeShop = Data->Type;
}

void CB_OffTrade::PShopActiveRecv(PMSG_SHOPACTIVE_RECV* Data)
{
	this->ShopActive = Data->Active;
	this->TypeShop = Data->Type;
}


void CB_OffTrade::ShowPrice(int TextNum, char* Price)
{
	if (this->ShopActive != 0 || this->ShopList != 0)
	{
		if (this->TypeShop == 0)
		{
			sprintf(TextList[TextNum], gCustomMessage.GetMessage(61), Price);
		}
		else if (this->TypeShop == 1)
		{
			sprintf(TextList[TextNum], gCustomMessage.GetMessage(62), Price);
		}
		else if (this->TypeShop == 2)
		{
			sprintf(TextList[TextNum], gCustomMessage.GetMessage(63), Price);
		}
		else if (this->TypeShop == 3)
		{
			sprintf(TextList[TextNum], gCustomMessage.GetMessage(55), Price);
		}
		else if (this->TypeShop == 4)
		{
			sprintf(TextList[TextNum], gCustomMessage.GetMessage(56), Price);
		}
		else if (this->TypeShop == 5)
		{
			sprintf(TextList[TextNum], gCustomMessage.GetMessage(57), Price);
		}
		else if (GetCustomItemSlot(this->TypeShop) != -1)
		{
			// Built here rather than from a message file entry, because the currency
			// is whatever item the server names - a fixed line would go stale. The
			// full item name is used, not the short button label: there is room for
			// it here, and it is what tells the buyer exactly what to go and find.
			const char* customName = GetCustomItemName(GetCustomItemSlot(this->TypeShop));

			sprintf(TextList[TextNum], "%s %s", Price, ((customName!=NULL)?customName:"Item"));
		}
		else
		{
			sprintf(TextList[TextNum], gCustomMessage.GetMessage(49), Price);
		}
	}
	else
	{
		sprintf(TextList[TextNum], GlobalText[63], Price);
	}
	
}

void CB_OffTrade::ShowMessNotice()
{
	if (this->ShopActive != 0 || this->ShopList != 0)
	{
		if (this->TypeShop == 0)
		{
			g_pChatListBox->AddText("", "Not enough Bless", SEASON3B::TYPE_ERROR_MESSAGE);
		}
		else if (this->TypeShop == 1)
		{
			g_pChatListBox->AddText("", "Not enough Soul", SEASON3B::TYPE_ERROR_MESSAGE);
		}
		else if (this->TypeShop == 2)
		{
			g_pChatListBox->AddText("", "Not enough WCoin", SEASON3B::TYPE_ERROR_MESSAGE);
		}
		else if (this->TypeShop == 3)
		{
			g_pChatListBox->AddText("", "Not enough WCoinP", SEASON3B::TYPE_ERROR_MESSAGE);
		}
		else if (this->TypeShop == 4)
		{
			g_pChatListBox->AddText("", "Not enough GobinPoint", SEASON3B::TYPE_ERROR_MESSAGE);
		}
		else if (this->TypeShop == 5)
		{
			g_pChatListBox->AddText("", "Not enough Zen", SEASON3B::TYPE_ERROR_MESSAGE);
		}
		else if (GetCustomItemSlot(this->TypeShop) != -1)
		{
			const char* customName = GetCustomItemName(GetCustomItemSlot(this->TypeShop));

			char szNotEnough[128];

			// A buffer rather than a ternary of two literals: on Android a ternary
			// whose arms are both string literals is const char*, and passing that
			// where char* is wanted is a hard error rather than a warning.
			sprintf(szNotEnough, "Not enough %s", ((customName!=NULL)?customName:"of that item"));

			g_pChatListBox->AddText("", szNotEnough, SEASON3B::TYPE_ERROR_MESSAGE);
		}
		else
		{
			g_pChatListBox->AddText("", GlobalText[423], SEASON3B::TYPE_ERROR_MESSAGE);
		}
	}
	else
	{
		g_pChatListBox->AddText("", GlobalText[423], SEASON3B::TYPE_ERROR_MESSAGE);
	}
}

void CB_OffTrade::RenderTextNotice(float X, float Y)
{
	unicode::t_char Text[100];
	memset(&Text, 0, sizeof(unicode::t_char) * 100);

	if (this->ShopActive != 0 || this->ShopList != 0)
	{
		if (this->TypeShop == 0)
		{
			sprintf(Text, "Can only trade using Bless");
		}
		else if (this->TypeShop == 1)
		{
			sprintf(Text, "Can only trade using Soul");
		}
		else if (this->TypeShop == 2)
		{
			sprintf(Text, "Can only trade using Chaos");
		}
		else if (this->TypeShop == 3)
		{
			sprintf(Text, "Can only trade using WCoin");
		}
		else if (this->TypeShop == 4)
		{
			sprintf(Text, "Can only trade using WCoinP");
		}
		else if (this->TypeShop == 5)
		{
			sprintf(Text, "Can only trade using GobinP");
		}
		else if (GetCustomItemSlot(this->TypeShop) != -1)
		{
			const char* customName = GetCustomItemName(GetCustomItemSlot(this->TypeShop));

			sprintf(Text, "Can only trade using %s", ((customName!=NULL)?customName:"a special item"));
		}
		else
		{
			sprintf(Text, GlobalText[1135]);
		}
	}
	else
	{
		sprintf(Text, GlobalText[1135]);
	}

	//sprintf(Text, GlobalText[1135]); 


	TextDraw((HFONT)g_hFontBold, X + 30, Y + 332, RGBA(255, 45, 47, 255), 0x0, 0, 0, 3, Text);
}