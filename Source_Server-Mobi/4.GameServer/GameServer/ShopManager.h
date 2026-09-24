// ShopManager.h: interface for the CShopManager class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Shop.h"

#define MAX_SHOP 100

#define SHOP_RANGE(x) (((x)<0)?0:((x)>=MAX_SHOP)?0:1)

struct SHOP_MANAGER_INFO
{
	int Index;
	int MonsterClass;
	int Map;
	int X;
	int Y;
	CShop Shop;
};

class CShopManager
{
public:
	CShopManager();
	virtual ~CShopManager();
	void Load(char* path);
	void LoadShop();
	void ReloadShopInterface();
	long GetShopNumber(int MonsterClass,int Map,int X,int Y);
	bool GetItemByIndex(int index,CItem* lpItem,int slot,int page = 0);
	bool GetItemByMonsterClass(int MonsterClass,CItem* lpItem,int slot);
	long GetItemCountByIndex(int index);
	long GetItemCountByMonsterClass(int MonsterClass);
	bool GCShopItemListSendByIndex(int index,int aIndex);
	bool GCShopItemListSendByMonsterClass(int MonsterClass,int Map,int X,int Y,int aIndex);

	// ---- Shop pages ----------------------------------------------------------
	// Which page each player is looking at is kept HERE, indexed by object slot, rather
	// than as a new OBJECTSTRUCT field: User.h is included by nearly every file in the
	// GameServer, and a field there would mean rebuilding all of it for a shop feature.
	//
	// The server, not the client, decides which page a purchase comes from - the buy
	// packet only carries a slot number - so this is the authoritative copy.
	void OpenShop(int aIndex);			// a player has just opened a shop: back to page 0
	int GetPage(int aIndex);
	void CGShopPageRecv(PMSG_SHOP_PAGE_RECV* lpMsg,int aIndex);
private:
	std::map<int,SHOP_MANAGER_INFO> m_ShopManagerInfo;
	BYTE m_PlayerPage[MAX_OBJECT];
	DWORD m_PlayerPageTick[MAX_OBJECT];
};

extern CShopManager gShopManager;
