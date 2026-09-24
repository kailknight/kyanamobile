// Shop.h: interface for the CShop class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Item.h"
#include "ItemManager.h"
#include "Protocol.h"

#define SHOP_SIZE 120

// Most pages one shop may have. The page number travels in a BYTE, and each page is a
// full 120-item CItem array (~100 KB), so this is a memory ceiling as much as a wire
// one. The real limit is CustomShopMaxPage in CustomConfig.ini, clamped to this.
#define SHOP_MAX_PAGE 99

#define SHOP_INVENTORY_RANGE(x) (((x)<0)?0:((x)>=SHOP_SIZE)?0:1)

//**********************************************//
//************ GameServer -> Client ************//
//**********************************************//

struct PMSG_SHOP_ITEM_LIST_SEND
{
	PWMSG_HEAD header; // C2:31
	BYTE type;
	BYTE count;
};

struct PMSG_SHOP_ITEM_LIST
{
	BYTE slot;
	BYTE ItemInfo[MAX_ITEM_INFO];
};

struct PMSG_ITEM_VALUE_SEND
{
	PSWMSG_HEAD header;
	int count;
};

struct ITEM_VALUE_DATA
{
	int index;
	int level;
	int newopt;
	int type;
	int value;
	int buysell;
	int sellvalue;
};

// Which page of a multi-page shop the item list that follows belongs to. Sent only when
// the shop HAS more than one page, so a single-page shop looks exactly as it always did.
// The client clears its item grid on receipt and then takes the ordinary C2:31 list.
struct PMSG_SHOP_PAGE_SEND
{
	PSBMSG_HEAD header; // C1:D3:B7
	BYTE page;			// 0-based
	BYTE pages;			// how many the shop has
};

//**********************************************//
//************ Client -> GameServer ************//
//**********************************************//

struct PMSG_SHOP_PAGE_RECV
{
	PSBMSG_HEAD header; // C1:D3:B8
	BYTE page;			// the page the client wants, 0-based
};

// All BYTE fields after a 4-byte C1 header, so there is no padding to disagree about.
// The client mirrors both in NewUINPCShop.h with the same asserts.
static_assert(sizeof(PMSG_SHOP_PAGE_SEND) == 6,"PMSG_SHOP_PAGE_SEND must stay 6 bytes");
static_assert(sizeof(PMSG_SHOP_PAGE_RECV) == 5,"PMSG_SHOP_PAGE_RECV must stay 5 bytes");

//**********************************************//
//**********************************************//
//**********************************************//

// One 8x15 screen of a shop.
//
// Held BY VALUE in a std::vector, not by pointer: SHOP_MANAGER_INFO (and so its CShop) is
// copied into the manager's std::map, and a raw-pointer page list would be shared between
// the copy and the original, then freed twice.
struct SHOP_PAGE
{
	SHOP_PAGE()
	{
		for(int n=0;n < SHOP_SIZE;n++)
		{
			this->Item[n].Clear();
			this->Map[n] = 0xFF;
		}
	}

	CItem Item[SHOP_SIZE];
	BYTE Map[SHOP_SIZE];
};

class CShop
{
public:
	CShop();
	virtual ~CShop();
	void Init();

	// maxPages = 1 is the original behaviour: one 120-cell screen, and anything that does
	// not fit on it is dropped. More than 1 lets the shop grow extra pages instead.
	void Load(char* path,int maxPages = 1);

	void ShopItemSet(int page,int slot,BYTE type);
	BYTE ShopRectCheck(int page,int x,int y,int width,int height);
	void InsertItem(int ItemIndex,int ItemLevel,int ItemDurability,int ItemOption1,int ItemOption2,int ItemOption3,int ItemNewOption,int ItemValue);

	// false = the item did not fit and was dropped.
	bool InsertItemNew(int ItemIndex,int ItemLevel,int ItemDurability,int ItemOption1,int ItemOption2,int ItemOption3,int ItemNewOption,int Anc, int JOH, int OpEx, int Socket1, int Socket2, int Socket3, int Socket4, int Socket5, int ItemValue,int maxPages = 1);

	bool GetItem(CItem* lpItem,int slot,int page = 0);
	long GetItemCount();			// across every page
	int GetPageCount();
	bool GCShopItemListSend(int aIndex,int page = 0);
	void GCItemValueSend(int Index,int page = 0);
private:
	// Never empty: page 0 always exists, so the single-page paths need no special case.
	std::vector<SHOP_PAGE> m_Pages;
};

extern CShop gShop;