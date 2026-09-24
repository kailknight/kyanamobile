// Shop.cpp: implementation of the CShop class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
//#include "ItemValue.h"
#include "Shop.h"
#include "SocketItemOption.h"
#include "SocketItemType.h"
#include "MemScript.h"
#include "User.h"
#include "Util.h"

CShop gShop;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CShop::CShop() // OK
{
	this->Init();
}

CShop::~CShop() // OK
{

}

void CShop::Init() // OK
{
	// One empty page, always - page 0 must exist so every single-page path works with
	// no special case. Load() calls this again on every reload, so the pages a previous,
	// larger copy of the file grew are released here rather than left behind.
	std::vector<SHOP_PAGE>().swap(this->m_Pages);

	this->m_Pages.resize(1);
}

void CShop::Load(char* path,int maxPages) // OK
{
	CMemScript* lpMemScript = new CMemScript;

	if(lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR,path);
		return;
	}

	if(lpMemScript->SetBuffer(path) == 0)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}

	this->Init();

	if(maxPages < 1)
	{
		maxPages = 1;
	}

	if(maxPages > SHOP_MAX_PAGE)
	{
		maxPages = SHOP_MAX_PAGE;
	}

	int dropped = 0;

	try
	{
		while(true)
		{
			if(lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}

			if(strcmp("end",lpMemScript->GetString()) == 0)
			{
				break;
			}

			int ItemIndex = SafeGetItem(GET_ITEM(lpMemScript->GetNumber(),lpMemScript->GetAsNumber()));

			int ItemLevel = lpMemScript->GetAsNumber();

			int ItemDurability = lpMemScript->GetAsNumber();

			int ItemOption1 = lpMemScript->GetAsNumber();

			int ItemOption2 = lpMemScript->GetAsNumber();

			int ItemOption3 = lpMemScript->GetAsNumber();

			int ItemNewOption = lpMemScript->GetAsNumber();

			int AncOption = lpMemScript->GetAsNumber();

			int JOH	= lpMemScript->GetAsNumber();

			int OpEx = lpMemScript->GetAsNumber();

			int Socket1 = lpMemScript->GetAsNumber();

			int Socket2 = lpMemScript->GetAsNumber();

			int Socket3 = lpMemScript->GetAsNumber();

			int Socket4 = lpMemScript->GetAsNumber();

			int Socket5 = lpMemScript->GetAsNumber();

			if(this->InsertItemNew(ItemIndex,ItemLevel,ItemDurability,ItemOption1,ItemOption2,ItemOption3,ItemNewOption,AncOption,JOH,OpEx,Socket1,Socket2,Socket3,Socket4,Socket5,0,maxPages) == 0)
			{
				dropped++;
			}


			//this->InsertItem(ItemIndex,ItemLevel,ItemDurability,ItemOption1,ItemOption2,ItemOption3,ItemNewOption,0);
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;

	// Until now an item that did not fit was dropped without a word, so a shop file that had
	// grown past one screen quietly lost its tail and nothing anywhere said so.
	if(dropped > 0)
	{
		LogAdd(LOG_RED,"[Shop] %s: %d item(s) did not fit and were dropped - the shop is full at %d page(s) of 120 cells (CustomShopPage / CustomShopMaxPage in CustomConfig.ini)",path,dropped,this->GetPageCount());
	}
	else if(this->GetPageCount() > 1)
	{
		LogAdd(LOG_BLUE,"[Shop] %s: %d items over %d pages",path,(int)this->GetItemCount(),this->GetPageCount());
	}
}

void CShop::ShopItemSet(int page,int slot,BYTE type) // OK
{
	if(page < 0 || page >= (int)this->m_Pages.size())
	{
		return;
	}

	if(SHOP_INVENTORY_RANGE(slot) == 0)
	{
		return;
	}

	ITEM_INFO ItemInfo;

	if(gItemManager.GetInfo(this->m_Pages[page].Item[slot].m_Index,&ItemInfo) == 0)
	{
		return;
	}

	int x = slot%8;
	int y = slot/8;

	if((x+ItemInfo.Width) > 8 || (y+ItemInfo.Height) > 15)
	{
		return;
	}

	for(int sy=0;sy < ItemInfo.Height;sy++)
	{
		for(int sx=0;sx < ItemInfo.Width;sx++)
		{
			this->m_Pages[page].Map[(((sy+y)*8)+(sx+x))] = type;
		}
	}
}

BYTE CShop::ShopRectCheck(int page,int x,int y,int width,int height) // OK
{
	if(page < 0 || page >= (int)this->m_Pages.size())
	{
		return 0xFF;
	}

	if((x+width) > 8 || (y+height) > 15)
	{
		return 0xFF;
	}

	for(int sy=0;sy < height;sy++)
	{
		for(int sx=0;sx < width;sx++)
		{
			if(this->m_Pages[page].Map[(((sy+y)*8)+(sx+x))] != 0xFF)
			{
				return 0xFF;
			}
		}
	}

	return ((y*8)+x);
}

void CShop::InsertItem(int ItemIndex,int ItemLevel,int ItemDurability,int ItemOption1,int ItemOption2,int ItemOption3,int ItemNewOption,int ItemValue) // OK
{
	// The older 8-column loader path. Nothing calls it any more (Load uses InsertItemNew),
	// so it keeps its original single-screen behaviour on page 0.
	ITEM_INFO ItemInfo;

	if(gItemManager.GetInfo(ItemIndex,&ItemInfo) == 0)
	{
		return;
	}

	for(int y=0;y < 15;y++)
	{
		for(int x=0;x < 8;x++)
		{
			if(this->m_Pages[0].Map[((y*8)+x)] == 0xFF)
			{
				BYTE slot = this->ShopRectCheck(0,x,y,ItemInfo.Width,ItemInfo.Height);

				if(slot != 0xFF)
				{
					this->m_Pages[0].Item[slot].m_Level = ItemLevel;
					this->m_Pages[0].Item[slot].m_Durability = (float)((ItemDurability==0)?gItemManager.GetItemDurability(ItemIndex,ItemLevel,ItemNewOption,0):ItemDurability);
					this->m_Pages[0].Item[slot].Convert(ItemIndex,ItemOption1,ItemOption2,ItemOption3,ItemNewOption,0,0,0,0,0xFF);
					this->m_Pages[0].Item[slot].m_PcPointValue = ItemValue;
					this->ShopItemSet(0,slot,1);
					return;
				}
			}
		}
	}
}

bool CShop::InsertItemNew(int ItemIndex,int ItemLevel,int ItemDurability,int ItemOption1,int ItemOption2,int ItemOption3,int ItemNewOption,int Anc, int JOH, int OpEx, int Socket1, int Socket2, int Socket3, int Socket4, int Socket5, int ItemValue,int maxPages) // OK
{
	ITEM_INFO ItemInfo;

	if(gItemManager.GetInfo(ItemIndex,&ItemInfo) == 0)
	{
		// An item id the server does not know. It was always skipped; now it is said so.
		// Returns true because "does not exist" is not "did not fit" - the caller counts
		// only the second as a full shop.
		LogAdd(LOG_RED,"[Shop] item %d is not in Item.txt - skipped",ItemIndex);
		return 1;
	}

	// Two tries: the LAST page, then - if it is full and the shop is allowed another - a
	// fresh one. Never back-fills an earlier page's gaps: that would let a small item jump
	// ahead of larger ones written before it, and the order of the file is exactly what
	// the shop owner lays the pages out by.
	bool added = 0;

	for(int attempt=0;attempt < 2;attempt++)
	{
		int page = (int)this->m_Pages.size()-1;

		for(int y=0;y < 15;y++)
		{
			for(int x=0;x < 8;x++)
			{
				if(this->m_Pages[page].Map[((y*8)+x)] == 0xFF)
				{
					BYTE slot = this->ShopRectCheck(page,x,y,ItemInfo.Width,ItemInfo.Height);

					if(slot != 0xFF)
					{
						CItem* lpItem = &this->m_Pages[page].Item[slot];

						lpItem->m_Level = ItemLevel;
						lpItem->m_Durability = (float)((ItemDurability==0)?gItemManager.GetItemDurability(ItemIndex,ItemLevel,ItemNewOption,0):ItemDurability);


						BYTE ItemSocketOption[MAX_SOCKET_OPTION] = {0xFF,0xFF,0xFF,0xFF,0xFF};

						int qtd;

						if (gSocketItemType.CheckSocketItemType(ItemIndex) == 1)
						{
							qtd = gSocketItemType.GetSocketItemMaxSocket(ItemIndex);

							ItemSocketOption[0] = (BYTE)((qtd > 0)?((Socket1 != 255)?Socket1:255):255);
							ItemSocketOption[1] = (BYTE)((qtd > 1)?((Socket2 != 255)?Socket2:255):255);
							ItemSocketOption[2] = (BYTE)((qtd > 2)?((Socket3 != 255)?Socket3:255):255);
							ItemSocketOption[3] = (BYTE)((qtd > 3)?((Socket4 != 255)?Socket4:255):255);
							ItemSocketOption[4] = (BYTE)((qtd > 4)?((Socket5 != 255)?Socket5:255):255);
							//this->m_Item[slot].m_SocketOptionBonus = gSocketItemOption.GetSocketItemBonusOption(this->m_Item);
						}


						lpItem->Convert(ItemIndex,ItemOption1,ItemOption2,ItemOption3,ItemNewOption,Anc,JOH,OpEx,ItemSocketOption,0xFF);

						lpItem->m_PcPointValue = ItemValue;

						this->ShopItemSet(page,slot,1);
						return 1;
					}
				}
			}
		}

		// Full. Grow a page and go round again - unless the shop is already at its limit.
		if(attempt == 0)
		{
			if((int)this->m_Pages.size() >= maxPages)
			{
				return 0;
			}

			this->m_Pages.resize(this->m_Pages.size()+1);

			added = 1;
		}
	}

	// Did not fit even an empty page (an item wider than 8 or taller than 15). The page
	// added for it is empty and would only be an empty screen to flip to, so it goes.
	if(added != 0)
	{
		this->m_Pages.pop_back();
	}

	return 0;
}

bool CShop::GetItem(CItem* lpItem,int slot,int page) // OK
{
	if(page < 0 || page >= (int)this->m_Pages.size())
	{
		return 0;
	}

	if(SHOP_INVENTORY_RANGE(slot) != 0)
	{
		if(this->m_Pages[page].Item[slot].IsItem() != 0)
		{
			(*lpItem) = this->m_Pages[page].Item[slot];
			return 1;
		}
	}

	return 0;
}

long CShop::GetItemCount() // OK
{
	int count = 0;

	for(int page=0;page < (int)this->m_Pages.size();page++)
	{
		for(int n=0;n < SHOP_SIZE;n++)
		{
			if(this->m_Pages[page].Item[n].IsItem() != 0)
			{
				count++;
			}
		}
	}

	return count;
}

int CShop::GetPageCount() // OK
{
	return (int)this->m_Pages.size();
}

bool CShop::GCShopItemListSend(int aIndex,int page) // OK
{
	if(page < 0 || page >= (int)this->m_Pages.size())
	{
		page = 0;
	}

	// Multi-page shops announce which page this list is BEFORE the list itself, so the
	// client can empty its grid and set its "page 2/5" label first. A single-page shop
	// sends nothing extra and is byte-for-byte what it always was.
	if(this->m_Pages.size() > 1)
	{
		PMSG_SHOP_PAGE_SEND pPage;

		pPage.header.set(0xD3,0xB7,sizeof(pPage));

		pPage.page = (BYTE)page;

		pPage.pages = (BYTE)this->m_Pages.size();

		DataSend(aIndex,(BYTE*)&pPage,pPage.header.size);
	}

	BYTE send[2048];

	PMSG_SHOP_ITEM_LIST_SEND pMsg;

	pMsg.header.set(0x31,0);

	int size = sizeof(pMsg);

	pMsg.type = 0;

	pMsg.count = 0;

	PMSG_SHOP_ITEM_LIST info;

	for(int n=0;n < SHOP_SIZE;n++)
	{
		if(this->m_Pages[page].Item[n].IsItem() != 0)
		{
			info.slot = n;

			gItemManager.ItemByteConvert(info.ItemInfo,this->m_Pages[page].Item[n]);

			memcpy(&send[size],&info,sizeof(info));
			size += sizeof(info);

			pMsg.count++;
		}
	}

	pMsg.header.size[0] = SET_NUMBERHB(size);
	pMsg.header.size[1] = SET_NUMBERLB(size);

	memcpy(send,&pMsg,sizeof(pMsg));

	DataSend(aIndex,send,size);

	this->GCItemValueSend(aIndex,page);
	return 1;
}

void CShop::GCItemValueSend(int Index,int page)
{
#if (GAMESERVER_CLIENTE_UPDATE >= 9)

	if(gObjIsConnected(Index) == false)
	{
		return;
	}

	LPOBJ lpObj = &gObj[Index];

	if(page < 0 || page >= (int)this->m_Pages.size())
	{
		page = 0;
	}

	// The page whose prices this packet carries. The client keys a price by item type, level
	// and excellent bits - not by slot - so it does not matter that the slots differ per page.
	SHOP_PAGE& pg = this->m_Pages[page];

	if((GetTickCount()-lpObj->ShopValueSendDelay) < (DWORD)1200)
	{
		return;
	}

	BYTE send[8192];

	PMSG_ITEM_VALUE_SEND pMsg;

	pMsg.header.set(0xF3,0xE9,0);

	int size = sizeof(pMsg);

	pMsg.count = 0;

	ITEM_VALUE_DATA info;

	for(int n=0;n < SHOP_SIZE;n++)
	{
		if(pg.Item[n].IsItem() != 0)
		{
			info.index = pg.Item[n].m_Index;

			info.level = pg.Item[n].m_Level;

			info.newopt = pg.Item[n].m_NewOption;

			info.type = 0;

			info.value = pg.Item[n].m_BuyMoney;

			info.buysell = 0;

			if (pg.Item[n].Coin1 > 0)
			{
				info.type = 1;
				info.value = pg.Item[n].Coin1;
			}
			if (pg.Item[n].Coin2 > 0)
			{
				info.type = 2;
				info.value = pg.Item[n].Coin2;
			}
			if (pg.Item[n].Coin3 > 0)
			{
				info.type = 3;
				info.value = pg.Item[n].Coin3;
			}

			info.sellvalue = 0;

			if (pg.Item[n].Sell>0)
			{
				info.sellvalue = pg.Item[n].m_SellMoney;
			}

			pMsg.count++;

			memcpy(&send[size],&info,sizeof(info));
			size += sizeof(info);
		}
	}


	for(int n=0;n < INVENTORY_FULL_SIZE;n++)
	{
		if(lpObj->Inventory[n].IsItem() != 0)
		{
			info.index = lpObj->Inventory[n].m_Index;

			info.level = lpObj->Inventory[n].m_Level;

			info.newopt = lpObj->Inventory[n].m_NewOption;

			info.type = 0;

			if (lpObj->Inventory[n].Coin1 > 0)
			{
				info.type = 1;
			}
			if (lpObj->Inventory[n].Coin2 > 0)
			{
				info.type = 2;
			}
			if (lpObj->Inventory[n].Coin3 > 0)
			{
				info.type = 3;
			}

			info.value = lpObj->Inventory[n].m_BuyMoney;

			info.buysell = 1;

			//info.sellvalue = 0;

			if (lpObj->Inventory[n].Sell > 0)
			{
				info.sellvalue = lpObj->Inventory[n].m_SellMoney;
			}
			else
			{
				info.sellvalue = 0;
			}

			pMsg.count++;

			memcpy(&send[size],&info,sizeof(info));
			size += sizeof(info);
		}
	}
#if(GAMESERVER_UPDATE>=701)

		if(lpObj->Inventory[236].IsItem() != 0)
		{
			info.index = lpObj->Inventory[236].m_Index;

			info.level = lpObj->Inventory[236].m_Level;

			info.newopt = lpObj->Inventory[236].m_NewOption;

			info.type = 0;

			if (lpObj->Inventory[236].Coin1 > 0)
			{
				info.type = 1;
			}
			if (lpObj->Inventory[236].Coin2 > 0)
			{
				info.type = 2;
			}
			if (lpObj->Inventory[236].Coin3 > 0)
			{
				info.type = 3;
			}

			info.value = lpObj->Inventory[236].m_BuyMoney;

			info.buysell = 1;

			//info.sellvalue = 0;

			if (lpObj->Inventory[236].Sell > 0)
			{
				info.sellvalue = lpObj->Inventory[236].m_SellMoney;
			}
			else
			{
				info.sellvalue = 0;
			}

			pMsg.count++;

			memcpy(&send[size],&info,sizeof(info));
			size += sizeof(info);
		}
#endif

	pMsg.header.size[0] = SET_NUMBERHB(size);
	pMsg.header.size[1] = SET_NUMBERLB(size);
	// ---
	memcpy(send,&pMsg,sizeof(pMsg));

	DataSend(Index,send,size);

	lpObj->ShopValueSendDelay = GetTickCount();

#endif
	return;
}