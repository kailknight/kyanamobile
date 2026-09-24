// ShopManager.cpp: implementation of the CShopManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ShopManager.h"
#include "CastleSiegeSync.h"
#include "MemScript.h"
#include "NpcTalk.h"
#include "Path.h"
#include "Util.h"

CShopManager gShopManager;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CShopManager::CShopManager() // OK
{
	this->m_ShopManagerInfo.clear();

	memset(this->m_PlayerPage,0,sizeof(this->m_PlayerPage));

	memset(this->m_PlayerPageTick,0,sizeof(this->m_PlayerPageTick));
}

CShopManager::~CShopManager() // OK
{

}

void CShopManager::Load(char* path) // OK
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

	this->m_ShopManagerInfo.clear();

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

			SHOP_MANAGER_INFO info;

			info.Index = lpMemScript->GetNumber();

			info.MonsterClass = lpMemScript->GetAsNumber();

			info.Map = lpMemScript->GetAsNumber();

			info.X = lpMemScript->GetAsNumber();

			info.Y = lpMemScript->GetAsNumber();

			this->m_ShopManagerInfo.insert(std::pair<int,SHOP_MANAGER_INFO>(info.Index,info));
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
}

void CShopManager::LoadShop() // OK
{
	// CustomShopPage / CustomShopMaxPage (CustomConfig.ini): lets a shop that is bigger than
	// one 120-cell screen grow extra pages, so a whole catalogue can live under one NPC.
	// Off (the default) is exactly the old behaviour: one screen, the rest dropped.
	//
	// Read straight from the ini here rather than through gServerInfo, so that "Reload
	// Shop" on its own picks up an edited value - it does not re-read CustomConfig.ini.
	int maxPages = 1;

	if(GetPrivateProfileInt("CustomConfig","CustomShopPage",0,".\\Data\\CustomConfig.ini") != 0)
	{
		maxPages = GetPrivateProfileInt("CustomConfig","CustomShopMaxPage",10,".\\Data\\CustomConfig.ini");

		if(maxPages < 1)
		{
			maxPages = 1;
		}

		if(maxPages > SHOP_MAX_PAGE)
		{
			maxPages = SHOP_MAX_PAGE;
		}
	}

	LogAdd(LOG_BLUE,"[Shop] paging %s (up to %d page(s) per shop)",((maxPages > 1)?"ON":"OFF"),maxPages);

	std::map<int,int> LoadShop;

	char wildcard_path[MAX_PATH];

	wsprintf(wildcard_path,"%s*",gPath.GetFullPath("Shop\\"));

	WIN32_FIND_DATA data;

	HANDLE file = FindFirstFile(wildcard_path,&data);

	if(file == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		if((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			if(isdigit(data.cFileName[0]) != 0 && isdigit(data.cFileName[1]) != 0 && isdigit(data.cFileName[2]) != 0)
			{
				if(data.cFileName[3] == ' ' && data.cFileName[4] == '-' && data.cFileName[5] == ' ')
				{
					std::map<int,SHOP_MANAGER_INFO>::iterator it = this->m_ShopManagerInfo.find(atoi(data.cFileName));

					if(it != this->m_ShopManagerInfo.end())
					{
						if(LoadShop.find(it->first) == LoadShop.end())
						{
							char path[MAX_PATH];

							wsprintf(path,"Shop\\%s",data.cFileName);

							it->second.Shop.Load(gPath.GetFullPath(path),maxPages);

							LoadShop.insert(std::pair<int,int>(it->first,1));
						}
					}
				}
			}
		}
	}
	while(FindNextFile(file,&data) != 0);
}

void CShopManager::ReloadShopInterface() // OK
{
	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) != 0)
		{
			if(gObj[n].Interface.use != 0 && gObj[n].Interface.type == INTERFACE_SHOP)
			{
				gObj[n].Interface.state = 1;

				PMSG_NPC_TALK_SEND pMsg;

				pMsg.header.setE(0x30,sizeof(pMsg));

				pMsg.result = 0;

				DataSend(n,(BYTE*)&pMsg,pMsg.header.size);

				this->GCShopItemListSendByIndex(gObj[n].TargetShopNumber,n);

				GCTaxInfoSend(n,2,gCastleSiegeSync.GetTaxRateStore(n));
			}
		}
	}
}

long CShopManager::GetShopNumber(int MonsterClass,int Map,int X,int Y) // OK
{
	for(std::map<int,SHOP_MANAGER_INFO>::iterator it=this->m_ShopManagerInfo.begin();it != this->m_ShopManagerInfo.end();it++)
	{
		if(it->second.MonsterClass == MonsterClass && it->second.Map == Map && it->second.X == X && it->second.Y == Y)
		{
			return it->second.Index;
		}

	}
	for(std::map<int,SHOP_MANAGER_INFO>::iterator it=this->m_ShopManagerInfo.begin();it != this->m_ShopManagerInfo.end();it++)
	{
		if(it->second.MonsterClass != -1 && it->second.MonsterClass == MonsterClass)
		{
			return it->second.Index;
			
		}
	}

	return -1;
}

bool CShopManager::GetItemByIndex(int index,CItem* lpItem,int slot,int page) // OK
{
	std::map<int,SHOP_MANAGER_INFO>::iterator it = this->m_ShopManagerInfo.find(index);

	if(it == this->m_ShopManagerInfo.end())
	{
		return 0;
	}
	else
	{
		return it->second.Shop.GetItem(lpItem,slot,page);
	}
}

bool CShopManager::GetItemByMonsterClass(int MonsterClass,CItem* lpItem,int slot) // OK
{
	for(std::map<int,SHOP_MANAGER_INFO>::iterator it=this->m_ShopManagerInfo.begin();it != this->m_ShopManagerInfo.end();it++)
	{
		if(it->second.MonsterClass != -1 && it->second.MonsterClass == MonsterClass)
		{
			return it->second.Shop.GetItem(lpItem,slot);
		}
	}

	return 0;
}

long CShopManager::GetItemCountByIndex(int index) // OK
{
	std::map<int,SHOP_MANAGER_INFO>::iterator it = this->m_ShopManagerInfo.find(index);

	if(it == this->m_ShopManagerInfo.end())
	{
		return 0;
	}
	else
	{
		return it->second.Shop.GetItemCount();
	}
}

long CShopManager::GetItemCountByMonsterClass(int MonsterClass) // OK
{
	for(std::map<int,SHOP_MANAGER_INFO>::iterator it=this->m_ShopManagerInfo.begin();it != this->m_ShopManagerInfo.end();it++)
	{
		if(it->second.MonsterClass != -1 && it->second.MonsterClass == MonsterClass)
		{
			return it->second.Shop.GetItemCount();
		}
	}

	return 0;
}

bool CShopManager::GCShopItemListSendByIndex(int index,int aIndex) // OK
{
	std::map<int,SHOP_MANAGER_INFO>::iterator it = this->m_ShopManagerInfo.find(index);

	if(it == this->m_ShopManagerInfo.end())
	{
		return 0;
	}
	else
	{
		// The player's page, clamped to what the shop has NOW - a Reload Shop can shrink a
		// shop while someone is on its last page, and the stored page must not outlive it.
		int page = this->GetPage(aIndex);

		if(page >= it->second.Shop.GetPageCount())
		{
			page = it->second.Shop.GetPageCount()-1;

			if(OBJECT_RANGE(aIndex) != 0)
			{
				this->m_PlayerPage[aIndex] = (BYTE)page;
			}
		}

		return it->second.Shop.GCShopItemListSend(aIndex,page);
	}
}

bool CShopManager::GCShopItemListSendByMonsterClass(int MonsterClass,int Map,int X,int Y,int aIndex) // OK
{
	for(std::map<int,SHOP_MANAGER_INFO>::iterator it=this->m_ShopManagerInfo.begin();it != this->m_ShopManagerInfo.end();it++)
	{
		if(it->second.MonsterClass == MonsterClass && it->second.Map == Map && it->second.X == X && it->second.Y == Y)
		{
			return it->second.Shop.GCShopItemListSend(aIndex,this->GetPage(aIndex));
		}
	}

	return 0;
}

void CShopManager::OpenShop(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	// Every open starts on page 1. Without this a player who left a shop on page 4 would find
	// the NEXT shop they opened - possibly a different NPC - already on page 4.
	this->m_PlayerPage[aIndex] = 0;

	this->m_PlayerPageTick[aIndex] = 0;
}

int CShopManager::GetPage(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return 0;
	}

	return this->m_PlayerPage[aIndex];
}

void CShopManager::CGShopPageRecv(PMSG_SHOP_PAGE_RECV* lpMsg,int aIndex) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Interface.use == 0 || lpObj->Interface.type != INTERFACE_SHOP)
	{
		return;
	}

	if(SHOP_RANGE(lpObj->TargetShopNumber) == 0)
	{
		return;
	}

	std::map<int,SHOP_MANAGER_INFO>::iterator it = this->m_ShopManagerInfo.find(lpObj->TargetShopNumber);

	if(it == this->m_ShopManagerInfo.end())
	{
		return;
	}

	// A one-page shop has nothing to turn to. Only a client that is out of step would ask.
	if(it->second.Shop.GetPageCount() <= 1)
	{
		return;
	}

	// Ignored, not answered, if it comes too fast: every accepted request costs a full item
	// list plus a price packet, and a hand cannot turn a page faster than this anyway.
	DWORD now = GetTickCount();

	if((now-this->m_PlayerPageTick[aIndex]) < 100)
	{
		return;
	}

	this->m_PlayerPageTick[aIndex] = now;

	// A page that does not exist is treated as "stay where you are" - the reply below still
	// goes out, so the client's grid is redrawn to match what the server is really on.
	if((int)lpMsg->page < it->second.Shop.GetPageCount())
	{
		this->m_PlayerPage[aIndex] = lpMsg->page;
	}

	// The price packet is throttled to one per 1200 ms per player (GCItemValueSend). Turning
	// a page inside that window would leave the new page's items with no prices at all.
	lpObj->ShopValueSendDelay = 0;

	this->GCShopItemListSendByIndex(lpObj->TargetShopNumber,aIndex);
}
