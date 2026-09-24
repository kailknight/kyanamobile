// CustomStore.cpp: implementation of the CCustomStore class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CustomStore.h"
#include "CashShop.h"
#include "CommandManager.h"
#include "DSProtocol.h"
#include "GameMain.h"
#include "Log.h"
#include "Map.h"
#include "MasterSkillTree.h"
#include "Message.h"
#include "Notice.h"
#include "PcPoint.h"
#include "ServerInfo.h"
#include "SocketManager.h"
#include "Util.h"
#include "IpManager.h"
#if(JEWELBANKVER2)
// For the jewel-bank fallback in OnPShopBuyItemWithCustomItem: a price the
// buyer cannot cover from inventory is taken from the bank instead.
#include "BCustomItemBank.h"
#endif

CCustomStore gCustomStore;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCustomStore::CCustomStore() // OK
{

}

CCustomStore::~CCustomStore() // OK
{

}

void CCustomStore::ReadCustomStoreInfo(char* section,char* path) // OK
{
	this->m_CustomStoreMapZone = GetPrivateProfileInt(section,"CustomStoreMapZone",0,path);

	this->m_CustomStoreTime[0] = GetPrivateProfileInt(section,"CustomStoreTime_AL0",0,path);

	this->m_CustomStoreTime[1] = GetPrivateProfileInt(section,"CustomStoreTime_AL1",0,path);

	this->m_CustomStoreTime[2] = GetPrivateProfileInt(section,"CustomStoreTime_AL2",0,path);

	this->m_CustomStoreTime[3] = GetPrivateProfileInt(section,"CustomStoreTime_AL3",0,path);

	GetPrivateProfileString(section,"CustomStoreCommandJoBSyntax","",this->m_CustomStoreCommandJoBSyntax,sizeof(this->m_CustomStoreCommandJoBSyntax),path);

	GetPrivateProfileString(section,"CustomStoreCommandJoSSyntax","",this->m_CustomStoreCommandJoSSyntax,sizeof(this->m_CustomStoreCommandJoSSyntax),path);

	GetPrivateProfileString(section,"CustomStoreCommandJoCSyntax","",this->m_CustomStoreCommandJoCSyntax,sizeof(this->m_CustomStoreCommandJoCSyntax),path);

	GetPrivateProfileString(section,"CustomStoreCommandWCCSyntax","",this->m_CustomStoreCommandWCCSyntax,sizeof(this->m_CustomStoreCommandWCCSyntax),path);

	GetPrivateProfileString(section,"CustomStoreCommandWCPSyntax","",this->m_CustomStoreCommandWCPSyntax,sizeof(this->m_CustomStoreCommandWCPSyntax),path);

	GetPrivateProfileString(section,"CustomStoreCommandWCGSyntax","",this->m_CustomStoreCommandWCGSyntax,sizeof(this->m_CustomStoreCommandWCGSyntax),path);

	GetPrivateProfileString(section,"CustomStoreJoBName","",this->m_CustomStoreJoBName,sizeof(this->m_CustomStoreJoBName),path);

	GetPrivateProfileString(section,"CustomStoreJoSName","",this->m_CustomStoreJoSName,sizeof(this->m_CustomStoreJoSName),path);

	GetPrivateProfileString(section,"CustomStoreJoCName","",this->m_CustomStoreJoCName,sizeof(this->m_CustomStoreJoCName),path);

	GetPrivateProfileString(section,"CustomStoreWCCName","",this->m_CustomStoreWCCName,sizeof(this->m_CustomStoreWCCName),path);

	GetPrivateProfileString(section,"CustomStoreWCPName","",this->m_CustomStoreWCPName,sizeof(this->m_CustomStoreWCPName),path);

	GetPrivateProfileString(section,"CustomStoreWCGName","",this->m_CustomStoreWCGName,sizeof(this->m_CustomStoreWCGName),path);

	// Custom exchange items. All off by default, so a server that has not been
	// told about them behaves exactly as before.
	for(int slot=0;slot < MAX_CUSTOM_STORE_ITEM;slot++)
	{
		// Keys are numbered from 1 in the ini, because an admin counting buttons on
		// screen counts from 1. The array is indexed from 0.
		char szKey[64] = {0};

		wsprintf(szKey,"CustomStoreItem%dSwitch",slot+1);
		this->m_CustomStoreItemSwitch[slot] = GetPrivateProfileInt(section,szKey,0,path);

		wsprintf(szKey,"CustomStoreItem%dCommand",slot+1);
		GetPrivateProfileString(section,szKey,"",this->m_CustomStoreItemCommandSyntax[slot],sizeof(this->m_CustomStoreItemCommandSyntax[slot]),path);

		wsprintf(szKey,"CustomStoreItem%dName",slot+1);
		GetPrivateProfileString(section,szKey,"",this->m_CustomStoreItemName[slot],sizeof(this->m_CustomStoreItemName[slot]),path);

		wsprintf(szKey,"CustomStoreItem%dButton",slot+1);
		GetPrivateProfileString(section,szKey,"",this->m_CustomStoreItemButton[slot],sizeof(this->m_CustomStoreItemButton[slot]),path);

		// -1 rather than 0, so a missing key cannot silently mean "Kris" (section 0,
		// index 0) - the store stays off instead of trading in a weapon nobody meant.
		wsprintf(szKey,"CustomStoreItem%dCat",slot+1);
		this->m_CustomStoreItemCat[slot] = GetPrivateProfileInt(section,szKey,-1,path);

		wsprintf(szKey,"CustomStoreItem%dIndex",slot+1);
		this->m_CustomStoreItemIndex[slot] = GetPrivateProfileInt(section,szKey,-1,path);

		wsprintf(szKey,"CustomStoreItem%dLevel",slot+1);
		this->m_CustomStoreItemLevel[slot] = GetPrivateProfileInt(section,szKey,0,path);

		if(this->m_CustomStoreItemSwitch[slot] != 0 && this->IsCustomItemStore(slot) == 0)
		{
			LogAdd(LOG_RED,"[Store] CustomStoreItem%dSwitch is on but CustomStoreItem%dCat/CustomStoreItem%dIndex name no item - that custom item store is disabled",slot+1,slot+1,slot+1);
		}

		// Warned about rather than substituted: falling back to the item name would
		// draw something too wide for the button, and falling back to "Item 1" would
		// look like a bug in the client rather than a missing key.
		if(this->IsCustomItemStore(slot) != 0 && this->m_CustomStoreItemButton[slot][0] == '\0')
		{
			LogAdd(LOG_RED,"[Store] CustomStoreItem%dButton is empty - that store's button will have no label",slot+1);
		}
	}

	this->m_CustomStoreOfflineGPGain = GetPrivateProfileInt(section,"CustomStoreOfflineGPGain",0,path);

	this->m_CustomStoreOfflineMapZone = GetPrivateProfileInt(section,"CustomStoreOfflineMapZone",0,path);

}

bool CCustomStore::CommandCustomStore(LPOBJ lpObj,char* arg) // OK
{
	char mode[32] = {0};

	gCommandManager.GetString(arg,mode,sizeof(mode),0);

	if(strcmp(mode,"") != 0)
	{
		if(strcmp(mode,this->m_CustomStoreCommandJoBSyntax) == 0)
		{
			if (this->OpenCustomStore(lpObj,0) != 0)
			{
				return 1;
			}
		}
		else if(strcmp(mode,this->m_CustomStoreCommandJoSSyntax) == 0)
		{
			if (this->OpenCustomStore(lpObj,1) != 0)
			{
				return 1;
			}
		}
		else if(strcmp(mode,this->m_CustomStoreCommandJoCSyntax) == 0)
		{
			if (this->OpenCustomStore(lpObj,2) != 0)
			{
				return 1;
			}
		}
		else if(strcmp(mode,this->m_CustomStoreCommandWCCSyntax) == 0)
		{
			if (this->OpenCustomStore(lpObj,3) != 0)
			{
				return 1;
			}
		}
		else if(strcmp(mode,this->m_CustomStoreCommandWCPSyntax) == 0)
		{
			if (this->OpenCustomStore(lpObj,4) != 0)
			{
				return 1;
			}
		}
		else if(strcmp(mode,this->m_CustomStoreCommandWCGSyntax) == 0)
		{
			if (this->OpenCustomStore(lpObj,5) != 0)
			{
				return 1;
			}
		}
		else
		{
			// Guarded by IsCustomItemStore as well as by a non-empty syntax: with a
			// slot switched off its command is normally empty and would match
			// nothing anyway, but an ini that sets a command and forgets the item
			// would otherwise open a store that can never be paid into.
			for(int slot=0;slot < MAX_CUSTOM_STORE_ITEM;slot++)
			{
				if(this->IsCustomItemStore(slot) == 0)
				{
					continue;
				}

				if(strcmp(mode,this->m_CustomStoreItemCommandSyntax[slot]) != 0)
				{
					continue;
				}

				if (this->OpenCustomStore(lpObj,CUSTOM_STORE_TYPE_ITEM+slot) != 0)
				{
					return 1;
				}

				break;
			}
		}
	}
	return 0;
}

int CCustomStore::GetCustomItemSlot(int type) // OK
{
	const int slot = type - CUSTOM_STORE_TYPE_ITEM;

	if(slot < 0 || slot >= MAX_CUSTOM_STORE_ITEM)
	{
		return -1;
	}

	return slot;
}

bool CCustomStore::IsCustomItemStore(int slot) // OK
{
	// Range-checked first: the slot can come from a store type that arrived off
	// the wire, so it is not necessarily one this server knows about.
	if(slot < 0 || slot >= MAX_CUSTOM_STORE_ITEM)
	{
		return 0;
	}

	if(this->m_CustomStoreItemSwitch[slot] == 0)
	{
		return 0;
	}

	if(this->m_CustomStoreItemCat[slot] < 0 || this->m_CustomStoreItemIndex[slot] < 0)
	{
		return 0;
	}

	if(this->m_CustomStoreItemLevel[slot] < 0)
	{
		return 0;
	}

	return 1;
}

int CCustomStore::GetCustomItemIndex(int slot) // OK
{
	if(this->IsCustomItemStore(slot) == 0)
	{
		return -1;
	}

	return GET_ITEM(this->m_CustomStoreItemCat[slot],this->m_CustomStoreItemIndex[slot]);
}

bool CCustomStore::CommandCustomStoreOffline(LPOBJ lpObj,char* arg) // OK
{
	 if(lpObj->Interface.use != 0)
	 {
		return 0;
	 }	

	if(lpObj->PShopOpen == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(787));
		return 0;
	}

	if(gMap[lpObj->Map].CheckAttr(lpObj->X,lpObj->Y,1) == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(779));
		return 0;
	}

	if(CC_MAP_RANGE(lpObj->Map) != 0 || IT_MAP_RANGE(lpObj->Map) != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(779));
		return 0;
	}

	if(this->m_CustomStoreOfflineMapZone != -1 && this->m_CustomStoreOfflineMapZone != lpObj->Map)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(779));
		return 0;
	}

	lpObj->Socket = INVALID_SOCKET;

	lpObj->PShopCustomOffline = 1;

	lpObj->PShopCustomOfflineTime = 0;

	lpObj->PShopCustomTime = this->m_CustomStoreTime[lpObj->AccountLevel]*60;

	closesocket(lpObj->PerSocketContext->Socket);

	return 1;
}

bool CCustomStore::OpenCustomStore(LPOBJ lpObj,int type) // OK
{
	if(gServerInfo.m_PersonalShopSwitch == 0)
	{
		return 0;
	}

	// The type arrives straight off the wire in CGOffTradeRecv, so an old or
	// hand-built client could ask for a custom-item store this server does not
	// have. Refused here rather than there, so the command path is covered too.
	const int customSlot = this->GetCustomItemSlot(type);

	if(customSlot != -1 && this->IsCustomItemStore(customSlot) == 0)
	{
		return 0;
	}

	 if(lpObj->Interface.use != 0)
	 {
		return 0;
	 }	

	if(lpObj->PShopOpen != 0)
	{
		gPersonalShop.GCPShopOpenSend(lpObj->Index,0);
		return 0;
	}

	if(gMap[lpObj->Map].CheckAttr(lpObj->X,lpObj->Y,1) == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(779));
		return 0;
	}

	if(CC_MAP_RANGE(lpObj->Map) != 0 || IT_MAP_RANGE(lpObj->Map) != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(779));
		return 0;
	}

	if(this->m_CustomStoreMapZone != -1 && this->m_CustomStoreMapZone != lpObj->Map)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(779));
		return 0;
	}

	if(gPersonalShop.CheckPersonalShopOpen(lpObj->Index) == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(780));
		return 0;
	}

	if(lpObj->Level <= 5)
	{
		gPersonalShop.GCPShopOpenSend(lpObj->Index,2);
		return 0;
	}

	lpObj->PShopOpen = 1;

	lpObj->PShopCustom = 1;

	lpObj->PShopCustomType = type;

	lpObj->PShopCustomOffline = 0;

	lpObj->PShopCustomOfflineTime = 0;

	lpObj->PShopCustomTime = this->m_CustomStoreTime[lpObj->AccountLevel]*60;

	if (this->m_CustomStoreTime[lpObj->AccountLevel] > 0) 
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(788),this->m_CustomStoreTime[lpObj->AccountLevel]);
	}

	switch(type)
	{
		case 0:
			memcpy(lpObj->PShopText,this->m_CustomStoreJoBName,sizeof(lpObj->PShopText));
			break;
		case 1:
			memcpy(lpObj->PShopText,this->m_CustomStoreJoSName,sizeof(lpObj->PShopText));
			break;
		case 2:
			memcpy(lpObj->PShopText,this->m_CustomStoreJoCName,sizeof(lpObj->PShopText));
			break;
		case 3:
			memcpy(lpObj->PShopText,this->m_CustomStoreWCCName,sizeof(lpObj->PShopText));
			break;
		case 4:
			memcpy(lpObj->PShopText,this->m_CustomStoreWCPName,sizeof(lpObj->PShopText));
			break;
		case 5:
			memcpy(lpObj->PShopText,this->m_CustomStoreWCGName,sizeof(lpObj->PShopText));
			break;
		default:
			// The custom-item slots. Reached through customSlot rather than a case
			// per slot, so raising MAX_CUSTOM_STORE_ITEM does not need this switch
			// touched.
			if(customSlot != -1)
			{
				memcpy(lpObj->PShopText,this->m_CustomStoreItemName[customSlot],sizeof(lpObj->PShopText));
			}
			break;
	}

	#if(GAMESERVER_UPDATE != 603)
		gPersonalShop.GCPShopOpenSend(lpObj->Index,1);
	#endif

	PMSG_PSHOP_TEXT_CHANGE_SEND pMsg;

	pMsg.header.set(0x3F,0x10,sizeof(pMsg));

	pMsg.index[0] = SET_NUMBERHB(lpObj->Index);

	pMsg.index[1] = SET_NUMBERLB(lpObj->Index);

	memcpy(pMsg.text,lpObj->PShopText,sizeof(pMsg.text));

	memcpy(pMsg.name,lpObj->Name,sizeof(pMsg.name));

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);

	this->GCOffActiveSend(lpObj->Index,1);

	return 1;
}

bool CCustomStore::OnPShopOpen(LPOBJ lpObj) // OK
{
	return ((lpObj->PShopCustom==0)?0:1);
}

void CCustomStore::OnPShopClose(LPOBJ lpObj) // OK
{
	if(lpObj->PShopCustom != 0)
	{
		lpObj->PShopCustom = 0;
		lpObj->PShopCustomType = 0;
	}

	if(lpObj->PShopCustomOffline == 1)
	{
		lpObj->PShopCustomOffline = 2;
		lpObj->PShopCustomOfflineTime = 5;
	}
}

void CCustomStore::OnPShopSecondProc(LPOBJ lpObj) // OK
{
	if(lpObj->PShopCustomOffline != 0)
	{
		if(lpObj->PShopCustomOffline == 2)
		{
			if((--lpObj->PShopCustomOfflineTime) == 0)
			{
				gObjDel(lpObj->Index);
				lpObj->PShopCustomOffline = 0;
				lpObj->PShopCustomOfflineTime = 0;
			}
		}

		lpObj->CheckSumTime = GetTickCount();
		lpObj->ConnectTickCount = GetTickCount();
		lpObj->PcPointPointTime = ((this->m_CustomStoreOfflineGPGain==0)?GetTickCount():lpObj->PcPointPointTime);
		lpObj->CashShopGoblinPointTime = ((this->m_CustomStoreOfflineGPGain==0)?GetTickCount():lpObj->CashShopGoblinPointTime);
	}

	if (lpObj->PShopCustomTime > 0) 
	{
		if((--lpObj->PShopCustomTime) == 0)
		{
			LogAdd(LOG_BLACK,"[Store][%s][%s] Timeout",lpObj->Account,lpObj->Name);
			gPersonalShop.GCPShopCloseSend(lpObj->Index,1);
			
			lpObj->PShopOpen = 0;
			memset(lpObj->PShopText,0,sizeof(lpObj->PShopText));
			this->OnPShopClose(lpObj);

			gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(789));
			return;
		}
	}
}

void CCustomStore::OnPShopAlreadyConnected(LPOBJ lpObj) // OK
{
	if(lpObj->PShopCustomOffline != 0)
	{
		gObjDel(lpObj->Index);
		lpObj->PShopCustomOffline = 0;
		lpObj->PShopCustomOfflineTime = 0;
	}
}

void CCustomStore::OnPShopItemList(LPOBJ lpObj,LPOBJ lpTarget) // OK
{
	if(lpTarget->PShopCustom != 0)
	{
		switch(lpTarget->PShopCustomType)
		{
			case 0:
				gNotice.GCNoticeSend(lpObj->Index,0,0,0,0,0,0,gMessage.GetMessage(781));
				break;
			case 1:
				gNotice.GCNoticeSend(lpObj->Index,0,0,0,0,0,0,gMessage.GetMessage(782));
				break;
			case 2:
				gNotice.GCNoticeSend(lpObj->Index,0,0,0,0,0,0,gMessage.GetMessage(783));
				break;
			case 3:
				gNotice.GCNoticeSend(lpObj->Index,0,0,0,0,0,0,gMessage.GetMessage(784));
				break;
			case 4:
				gNotice.GCNoticeSend(lpObj->Index,0,0,0,0,0,0,gMessage.GetMessage(785));
				break;
			case 5:
				gNotice.GCNoticeSend(lpObj->Index,0,0,0,0,0,0,gMessage.GetMessage(786));
				break;
			default:
			{
				// Composed rather than taken from Message.txt, because the item is
				// configurable: a fixed message line would go stale the moment the
				// ini named a different item.
				const int customSlot = this->GetCustomItemSlot(lpTarget->PShopCustomType);

				if(customSlot != -1 && this->IsCustomItemStore(customSlot) != 0)
				{
					gNotice.GCNoticeSend(lpObj->Index,0,0,0,0,0,0,"This store only trades in %s",gItemManager.GetItemName(this->GetCustomItemIndex(customSlot)));
				}
				break;
			}
		}
	}
}

bool CCustomStore::OnPShopBuyItemRecv(PMSG_PSHOP_BUY_ITEM_RECV* lpMsg,int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return 1;
	}

	int bIndex = MAKE_NUMBERW(lpMsg->index[0],lpMsg->index[1]);

	if(gObjIsConnectedGP(bIndex) == 0)
	{
		return 1;
	}

	/*
		Refuse buying from your own shop. The target index comes straight off the
		wire, so nothing stopped a client sending its own, and the whole flow then
		"worked": InventoryInsertItem duplicated the item into a second slot, the
		seller was credited and the buyer charged - the same person - and
		GDCashShopSubPointSaveSend and the credit ran as two separate DataServer
		round trips. Whether that nets to zero depends entirely on the order they
		land in, so it was an item-duplication bug waiting on a race, not just a
		pointless transaction.
	*/
	if(aIndex == bIndex)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,3);
		return 1;
	}

	LPOBJ lpTarget = &gObj[bIndex];

	if(lpTarget->PShopOpen == 0)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,3);
		return 1;
	}

	if(lpTarget->PShopCustom == 0)
	{
		return 0;
	}

	if(lpTarget->PShopTransaction != 0)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,4);
		return 1;
	}

	if(INVENTORY_SHOP_RANGE(lpMsg->slot) == 0)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,5);
		return 1;
	}

	gObjFixInventoryPointer(aIndex);

	if(lpObj->Transaction == 1)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,6);
		return 1;
	}

	char name[11] = {0};

	memcpy(name,lpMsg->name,sizeof(lpMsg->name));
	
	if(strcmp(name,lpTarget->Name) != 0)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,6);
		return 1;
	}

	if(lpTarget->Inventory[lpMsg->slot].IsItem() == 0)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,6);
		return 1;
	}

	if(lpTarget->Inventory[lpMsg->slot].m_PShopValue <= 0)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,6);
		return 1;
	}

	// Taken before the cash-currency block below: that block reads point balances
	// that mean nothing here, and its throttle would otherwise be the only thing
	// standing between two inventory writes.
	if(this->GetCustomItemSlot(lpTarget->PShopCustomType) != -1)
	{
		return this->OnPShopBuyItemWithCustomItem(lpObj,lpTarget,lpMsg->slot);
	}
#if (FIXBUGCOIN)
	DWORD gPShopWCCValue = ((lpTarget->PShopCustomType == 3) ? lpTarget->Inventory[lpMsg->slot].m_PShopValue : 0);

	DWORD gPShopWCPValue = ((lpTarget->PShopCustomType == 4) ? lpTarget->Inventory[lpMsg->slot].m_PShopValue : 0);

	DWORD gPShopWGPValue = ((lpTarget->PShopCustomType == 5) ? lpTarget->Inventory[lpMsg->slot].m_PShopValue : 0);

	if (GetTickCount() < (lpObj->ClickClientSend + 1000))
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GetMessage(44));
		return 1;
	}

	lpObj->ClickClientSend = GetTickCount();

	if (gPShopWCCValue > lpObj->Coin1 || gPShopWCPValue > lpObj->Coin2 || gPShopWGPValue > lpObj->Coin3)
	{

		//gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, "[PSShop] Khong du %d de mua !", lpTarget->Inventory[lpMsg->slot].m_PShopValue);
		gPersonalShop.GCPShopBuyItemSend(aIndex, bIndex, 0, 7);
		return 1;
	}
	//=====
#endif
	#if(GAMESERVER_UPDATE>=501)

	if(lpTarget->PShopCustomType == 3 || lpTarget->PShopCustomType == 4 || lpTarget->PShopCustomType == 5)
	{
		gCashShop.GDCashShopRecievePointSend(aIndex,(DWORD)&CCustomStore::OnPShopBuyItemCallbackRecv,(DWORD)&gObj[bIndex],lpMsg->slot);
		return 1;
	}

	#else

	if(lpTarget->PShopCustomType == 3 || lpTarget->PShopCustomType == 4 || lpTarget->PShopCustomType == 5)
	{
		gPcPoint.GDPcPointRecievePointSend(aIndex,(DWORD)&CCustomStore::OnPShopBuyItemCallbackRecv,(DWORD)&gObj[bIndex],lpMsg->slot);
		return 1;
	}

	#endif

	int PShopJoBValue = ((lpTarget->PShopCustomType==0)?lpTarget->Inventory[lpMsg->slot].m_PShopValue:0);

	int PShopJoSValue = ((lpTarget->PShopCustomType==1)?lpTarget->Inventory[lpMsg->slot].m_PShopValue:0);

	int PShopJoCValue = ((lpTarget->PShopCustomType==2)?lpTarget->Inventory[lpMsg->slot].m_PShopValue:0);

	int RequireJewelCount[3] = {0};

	int PaymentJewelCount[3] = {0};

	int RequireJewelTable[3][PSHOP_REQUIRE_TABLE_SIZE] = {0};

	int PaymentJewelTable[3][4] = {0};

	gPersonalShop.GetRequireJewelCount(lpObj,&RequireJewelCount[0],RequireJewelTable[0],0,PShopJoBValue);

	gPersonalShop.GetRequireJewelCount(lpObj,&RequireJewelCount[1],RequireJewelTable[1],1,PShopJoSValue);

	gPersonalShop.GetRequireJewelCount(lpObj,&RequireJewelCount[2],RequireJewelTable[2],2,PShopJoCValue);

	gPersonalShop.GetPaymentJewelCount(lpTarget,&PaymentJewelCount[0],PaymentJewelTable[0],0,PShopJoBValue);

	gPersonalShop.GetPaymentJewelCount(lpTarget,&PaymentJewelCount[1],PaymentJewelTable[1],1,PShopJoSValue);

	gPersonalShop.GetPaymentJewelCount(lpTarget,&PaymentJewelCount[2],PaymentJewelTable[2],2,PShopJoCValue);

	if(RequireJewelCount[0] < PShopJoBValue)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
		return 1;
	}

	if(RequireJewelCount[1] < PShopJoSValue)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
		return 1;
	}

	if(RequireJewelCount[2] < PShopJoCValue)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
		return 1;
	}

	if(PShopJoBValue > 0 && RequireJewelTable[0][0] == 0 && RequireJewelTable[0][1] == 0 && RequireJewelTable[0][2] == 0 && RequireJewelTable[0][3] == 0 && RequireJewelTable[0][4] == 0)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
		return 1;
	}

	if(PShopJoSValue > 0 && RequireJewelTable[1][0] == 0 && RequireJewelTable[1][1] == 0 && RequireJewelTable[1][2] == 0 && RequireJewelTable[1][3] == 0 && RequireJewelTable[1][4] == 0)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
		return 1;
	}

	if(PShopJoCValue > 0 && RequireJewelTable[2][0] == 0 && RequireJewelTable[2][1] == 0 && RequireJewelTable[2][2] == 0 && RequireJewelTable[2][3] == 0 && RequireJewelTable[2][4] == 0)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
		return 1;
	}

	// No room for the jewels: into the seller's jewel bank if it can take
	// them, otherwise the sale is refused (see CPersonalShop::CanPayJewelsToBank).
	bool payToSellerBank = false;

	if(gItemManager.GetInventoryEmptySlotCount(lpTarget) < (PaymentJewelCount[0]+PaymentJewelCount[1]+PaymentJewelCount[2]))
	{
		payToSellerBank = gPersonalShop.CanPayJewelsToBank(lpTarget,PaymentJewelTable);

		if(payToSellerBank == false)
		{
			gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
			return 1;
		}
	}

	lpTarget->PShopTransaction = 1;

	BYTE result = gItemManager.InventoryInsertItem(aIndex,lpTarget->Inventory[lpMsg->slot]);

	if(result == 0xFF)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,8);
		return 1;
	}

	gLog.Output(LOG_TRADE,"[SellPesonalShopItem][%s][%s] - (Account: %s, Name: %s, Value: %d, JoBValue: %d, JoSValue: %d, JoCValue: %d, Index: %04d, Level: %02d, Serial: %08X, Option1: %01d, Option2: %01d, Option3: %01d, NewOption: %03d, JewelOfHarmonyOption: %03d, ItemOptionEx: %03d, SocketOption: %03d, %03d, %03d, %03d, %03d)",lpTarget->Account,lpTarget->Name,lpObj->Account,0,PShopJoBValue,PShopJoSValue,PShopJoCValue,lpTarget->Inventory[lpMsg->slot].m_Index,lpTarget->Inventory[lpMsg->slot].m_Level,lpTarget->Inventory[lpMsg->slot].m_Serial,lpTarget->Inventory[lpMsg->slot].m_Option1,lpTarget->Inventory[lpMsg->slot].m_Option2,lpTarget->Inventory[lpMsg->slot].m_Option3,lpTarget->Inventory[lpMsg->slot].m_NewOption,lpTarget->Inventory[lpMsg->slot].m_JewelOfHarmonyOption,lpTarget->Inventory[lpMsg->slot].m_ItemOptionEx,lpTarget->Inventory[lpMsg->slot].m_SocketOption[0],lpTarget->Inventory[lpMsg->slot].m_SocketOption[1],lpTarget->Inventory[lpMsg->slot].m_SocketOption[2],lpTarget->Inventory[lpMsg->slot].m_SocketOption[3],lpTarget->Inventory[lpMsg->slot].m_SocketOption[4]);

	gPersonalShop.SetRequireJewelCount(lpObj,RequireJewelTable[0],0);

	gPersonalShop.SetRequireJewelCount(lpObj,RequireJewelTable[1],1);

	gPersonalShop.SetRequireJewelCount(lpObj,RequireJewelTable[2],2);

	gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,result,1);

	GDCharacterInfoSaveSend(aIndex);

	gPersonalShop.PaySellerJewels(lpTarget,PaymentJewelTable[0],0,payToSellerBank);

	gPersonalShop.PaySellerJewels(lpTarget,PaymentJewelTable[1],1,payToSellerBank);

	gPersonalShop.PaySellerJewels(lpTarget,PaymentJewelTable[2],2,payToSellerBank);

	gPersonalShop.GCPShopSellItemSend(bIndex,aIndex,lpMsg->slot);

	gItemManager.InventoryDelItem(bIndex,lpMsg->slot);
	gItemManager.GCItemDeleteSend(bIndex,lpMsg->slot,1);

	GDCharacterInfoSaveSend(bIndex);

	if(gPersonalShop.CheckPersonalShop(bIndex) == 0)
	{
		lpTarget->PShopItemChange = 1;
	}
	else
	{
		lpTarget->PShopOpen = 0;
		memset(lpTarget->PShopText,0,sizeof(lpTarget->PShopText));
		gPersonalShop.GCPShopCloseSend(bIndex,1);
		this->OnPShopClose(lpTarget);
	}

	lpTarget->PShopTransaction = 0;

	return 1;
}

bool CCustomStore::OnPShopBuyItemWithCustomItem(LPOBJ lpObj,LPOBJ lpTarget,int invenSlot) // OK
{
	const int aIndex = lpObj->Index;

	const int bIndex = lpTarget->Index;

	// Which of the three custom currencies this shop takes. Read from the shop
	// rather than passed in, so there is one source of truth for it.
	const int customSlot = this->GetCustomItemSlot(lpTarget->PShopCustomType);

	const int itemIndex = this->GetCustomItemIndex(customSlot);

	// Every caller has already checked this, but the price below is paid out of
	// inventory and there is no way to un-pay it, so the guard is repeated rather
	// than assumed.
	if(itemIndex < 0)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
		return 1;
	}

	// The same one-second throttle the cash-currency path uses. It matters more
	// here, not less: this path completes synchronously, so two buy packets
	// arriving back to back would both pass their checks against the same
	// inventory before either had written to it.
	if(GetTickCount() < (lpObj->ClickClientSend + 1000))
	{
		gNotice.GCNoticeSend(aIndex,1,0,0,0,0,0,gMessage.GetMessage(44));
		return 1;
	}

	lpObj->ClickClientSend = GetTickCount();

	const int price = lpTarget->Inventory[invenSlot].m_PShopValue;

	const int level = this->m_CustomStoreItemLevel[customSlot];

	// Inventory first, then the jewel bank for whatever is short.
	//
	// Inventory is spent first on purpose: it is the stock the player can see, and
	// draining the bank while items sat in the inventory would look like the bank
	// had been raided. It also keeps the common case - enough in hand - free of any
	// bank involvement at all.
	const int invenCount = gItemManager.GetInventoryItemCount(lpObj,itemIndex,level);

	const int payFromInven = ((invenCount > price)?price:invenCount);

	int payFromBank = price - payFromInven;

#if(JEWELBANKVER2)

	// CheckCountItemBank answers 0 for every reason the bank cannot be used -
	// disabled, a bot or offline-mode character, or an item that is not one of its
	// configured slots - so an unbankable currency item simply behaves as before.
	const int bankCount = gBCustomItemBank.CheckCountItemBank(aIndex,itemIndex,level);

	// Checked here rather than left to CongTruBank, which refuses on these two maps
	// but would do so AFTER the item had been handed over. Everything that can say
	// no has to say it before anything moves.
	const bool bankUsable = (lpObj->Map != 107 && lpObj->Map != 109);

	if(payFromBank > 0 && (bankUsable == false || bankCount < payFromBank))
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
		return 1;
	}

#else

	// No bank in this build, so inventory has to cover the whole price.
	if(payFromBank > 0)
	{
		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
		return 1;
	}

#endif

	// The seller is paid in loose items, one per slot - the exchange item has no
	// bundle form the way the jewels do. A price of 20 therefore needs 20 free
	// slots in the seller's inventory, and the sale is refused rather than
	// silently paying part of it.
	//
	// The slot the sold item vacates is deliberately NOT counted: it is freed
	// after this point, and counting it would make the last item in a full
	// inventory sellable for exactly one more unit than the seller can hold.
	//
	// When the seller cannot hold it, the whole payment goes into their jewel
	// bank instead, if the bank takes this item and has room under the
	// account's cap. Only if it cannot is the sale refused. Decided here, before
	// anything moves.
	bool payToSellerBank = false;

	if(gItemManager.GetInventoryEmptySlotCount(lpTarget) < price)
	{
#if(JEWELBANKVER2)
		payToSellerBank = gBCustomItemBank.CanAddBank(bIndex,itemIndex,level,price);
#endif

		if(payToSellerBank == false)
		{
			gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
			return 1;
		}
	}

	lpTarget->PShopTransaction = 1;

#if(JEWELBANKVER2)

	// The bank is charged BEFORE the item is handed over, because this is the only
	// step left that can still refuse: CongTruBank has guards of its own, and a
	// refusal after the insert would have given the item away for nothing. If the
	// insert then fails the charge is put back, which cannot itself fail - the
	// ceiling it checks was satisfied a moment ago by the larger balance.
	if(payFromBank > 0 && gBCustomItemBank.CongTruBank(aIndex,itemIndex,level,-(payFromBank),0) == 0)
	{
		lpTarget->PShopTransaction = 0;

		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,7);
		return 1;
	}

#endif

	BYTE result = gItemManager.InventoryInsertItem(aIndex,lpTarget->Inventory[invenSlot]);

	if(result == 0xFF)
	{
#if(JEWELBANKVER2)
		if(payFromBank > 0)
		{
			gBCustomItemBank.CongTruBank(aIndex,itemIndex,level,+(payFromBank),0);
		}
#endif

		// Cleared before returning. The jewel and cash paths both leak
		// PShopTransaction = 1 here, which leaves the seller's shop refusing every
		// later buyer with "transaction in progress" until they close it.
		lpTarget->PShopTransaction = 0;

		gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,0,8);
		return 1;
	}

	gLog.Output(LOG_TRADE,"[SellPesonalShopItem-CustomItem][%s][%s] - (Account: %s, Name: %s, Pay: %d x %s (%d inventory, %d bank), Index: %04d, Level: %02d, Serial: %08X)",lpTarget->Account,lpTarget->Name,lpObj->Account,lpObj->Name,price,gItemManager.GetItemName(itemIndex),payFromInven,payFromBank,lpTarget->Inventory[invenSlot].m_Index,lpTarget->Inventory[invenSlot].m_Level,lpTarget->Inventory[invenSlot].m_Serial);

	// payFromInven, not price: the rest has already come out of the bank, and
	// asking for the full price here would try to take it twice.
	if(payFromInven > 0)
	{
		gItemManager.DeleteInventoryItemCount(lpObj,itemIndex,level,payFromInven);
	}

	gPersonalShop.GCPShopBuyItemSend(aIndex,bIndex,result,1);

	GDCharacterInfoSaveSend(aIndex);

#if(JEWELBANKVER2)
	// Checked by CanAddBank above and nothing has touched the seller's bank
	// since, so this does not fail - but if it ever did, pay in items rather
	// than not at all.
	if(payToSellerBank && gBCustomItemBank.CongTruBank(bIndex,itemIndex,level,+(price),0) == 0)
	{
		gLog.Output(LOG_TRADE,"[SellPesonalShopItem-CustomItem][%s][%s] - bank deposit of %d refused, paid as items",lpTarget->Account,lpTarget->Name,price);
		payToSellerBank = false;
	}
#endif

	for(int n=0;payToSellerBank == false && n < price;n++)
	{
		GDCreateItemSend(bIndex,0xEB,0,0,itemIndex,(BYTE)level,0,0,0,0,-1,0,0,0,0,0,0xFF,0);
	}

	gPersonalShop.GCPShopSellItemSend(bIndex,aIndex,invenSlot);

	gItemManager.InventoryDelItem(bIndex,(BYTE)invenSlot);
	gItemManager.GCItemDeleteSend(bIndex,(BYTE)invenSlot,1);

	GDCharacterInfoSaveSend(bIndex);

	if(gPersonalShop.CheckPersonalShop(bIndex) == 0)
	{
		lpTarget->PShopItemChange = 1;
	}
	else
	{
		lpTarget->PShopOpen = 0;
		memset(lpTarget->PShopText,0,sizeof(lpTarget->PShopText));
		gPersonalShop.GCPShopCloseSend(bIndex,1);
		this->OnPShopClose(lpTarget);
	}

	lpTarget->PShopTransaction = 0;

	return 1;
}

void CCustomStore::OnPShopBuyItemCallbackRecv(LPOBJ lpObj,LPOBJ lpTarget,DWORD slot,DWORD WCoinC,DWORD WCoinP,DWORD GoblinPoint) // OK
{
	if(gObjIsConnectedGP(lpTarget->Index) == 0)
	{
		gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,0,3);
		return;
	}

	// Re-checked here as well as in OnPShopBuyItemRecv, because this runs as a
	// DataServer callback: the point balance arrives asynchronously, so this is
	// the function that actually moves the item and it must not depend on the
	// request-side guard still being the only way in.
	if(lpObj->Index == lpTarget->Index)
	{
		gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,0,3);
		return;
	}

	if(lpTarget->PShopOpen == 0)
	{
		gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,0,3);
		return;
	}

	if(lpTarget->PShopTransaction != 0)
	{
		gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,0,4);
		return;
	}

	if(lpTarget->Inventory[slot].IsItem() == 0)
	{
		gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,0,6);
		return;
	}

	DWORD PShopWCCValue = ((lpTarget->PShopCustomType==3)?lpTarget->Inventory[slot].m_PShopValue:0);

	DWORD PShopWCPValue = ((lpTarget->PShopCustomType==4)?lpTarget->Inventory[slot].m_PShopValue:0);

	DWORD PShopWGPValue = ((lpTarget->PShopCustomType==5)?lpTarget->Inventory[slot].m_PShopValue:0);

	#if(GAMESERVER_UPDATE>=501)
	if(WCoinC < PShopWCCValue)
	{
		gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,0,7);
		return;
	}

	if(WCoinP < PShopWCPValue)
	{
		gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,0,7);
		return;
	}
	#else
	if(lpObj->Coin1 < PShopWCCValue)
	{
		gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,0,7);
		return;
	}

	if(lpObj->Coin2 < PShopWCPValue)
	{
		gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,0,7);
		return;
	}
	#endif

	if(GoblinPoint < PShopWGPValue)
	{
		gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,0,7);
		return;
	}

	lpTarget->PShopTransaction = 1;

	BYTE result = gItemManager.InventoryInsertItem(lpObj->Index,lpTarget->Inventory[slot]);

	if(result == 0xFF)
	{
		gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,0,8);
		return;
	}
	gLog.Output(LOG_TRADE, "[ShopItem]|  %s/%s | Có: %d WCC |Nhận Thêm Từ | %s/%s)|, WC: %d, WP: %d, WG: %d, Index: %04d Item %s+ %02d, Serial: %08X, Skil: %01d, Luck: %01d, op: %01d, EX: %03d,Than:%d GC: %03d, 380: %03d, SK: %03d, %03d, %03d, %03d, %03d)", lpTarget->Account, lpTarget->Name, lpTarget->Coin1, lpObj->Account, lpObj->Name, PShopWCCValue, PShopWCPValue, PShopWGPValue, lpTarget->Inventory[slot].m_Index, gItemManager.GetItemName(lpTarget->Inventory[slot].m_Index), lpTarget->Inventory[slot].m_Level, lpTarget->Inventory[slot].m_Serial, lpTarget->Inventory[slot].m_Option1, lpTarget->Inventory[slot].m_Option2,
		lpTarget->Inventory[slot].m_Option3, lpTarget->Inventory[slot].m_NewOption, lpTarget->Inventory[slot].m_SetOption, lpTarget->Inventory[slot].m_JewelOfHarmonyOption, lpTarget->Inventory[slot].m_ItemOptionEx, lpTarget->Inventory[slot].m_SocketOption[0], lpTarget->Inventory[slot].m_SocketOption[1], lpTarget->Inventory[slot].m_SocketOption[2], lpTarget->Inventory[slot].m_SocketOption[3], lpTarget->Inventory[slot].m_SocketOption[4]);

	gLog.Output(LOG_WC, "[ShopItem] - %s/%s |Cũ: %d||Mới: %d||Nhận Từ|%s/%s)| Giá: %d WC, WCP: %d, WCG: %d| Có: %d WC|Còn: %d WC", lpTarget->Account, lpTarget->Name, lpTarget->Coin1, lpTarget->Coin1 + PShopWCCValue, lpObj->Account, lpObj->Name, PShopWCCValue, PShopWCPValue, PShopWGPValue, lpObj->Coin1, lpObj->Coin1 - PShopWCCValue);
	gNotice.NewNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, gMessage.GetMessage(3111), gItemManager.GetItemName(lpTarget->Inventory[slot].m_Index), lpTarget->Name, PShopWCCValue); //"Bạn Mua %s của %s Giá %d WC "
	#if(GAMESERVER_UPDATE>=501)

	gCashShop.GDCashShopSubPointSaveSend(lpObj->Index,0,PShopWCCValue,PShopWCPValue,PShopWGPValue,0 , "CustomStoreBuy");
	gCashShop.CGCashShopPointRecv(lpObj->Index);

	#else

	if (PShopWCCValue > 0)
	{
		GDSetCoinSend(lpObj->Index, -(PShopWCCValue), 0, 0,"CustomStoreBuy");
	}
	if (PShopWCPValue > 0)
	{
		GDSetCoinSend(lpObj->Index, 0, -(PShopWCPValue), 0,"CustomStoreBuy");
	}

	gPcPoint.GDPcPointSubPointSaveSend(lpObj->Index,PShopWGPValue);

	gPcPoint.GDPcPointPointSend(lpObj->Index);

	#endif

	gPersonalShop.GCPShopBuyItemSend(lpObj->Index,lpTarget->Index,result,1);

	GDCharacterInfoSaveSend(lpObj->Index);

	#if(GAMESERVER_UPDATE>=501)

	gCashShop.GDCashShopAddPointSaveSend(lpTarget->Index,0,PShopWCCValue,PShopWCPValue,PShopWGPValue,0,"CustomStoreAddCoin");
	gCashShop.CGCashShopPointRecv(lpTarget->Index);

	#else

	if (PShopWCCValue > 0)
	{
		GDSetCoinSend(lpTarget->Index, PShopWCCValue, 0, 0,"CustomStoreSell");
	}
	if (PShopWCPValue > 0)
	{
		GDSetCoinSend(lpTarget->Index, 0, PShopWCPValue, 0,"CustomStoreSell");
	}

	gPcPoint.GDPcPointAddPointSaveSend(lpTarget->Index,PShopWGPValue);

	gPcPoint.GDPcPointPointSend(lpTarget->Index);

	#endif

	gPersonalShop.GCPShopSellItemSend(lpTarget->Index,lpObj->Index,slot);

	gItemManager.InventoryDelItem(lpTarget->Index,(BYTE)slot);
	gItemManager.GCItemDeleteSend(lpTarget->Index,(BYTE)slot,1);

	GDCharacterInfoSaveSend(lpTarget->Index);

	if(gPersonalShop.CheckPersonalShop(lpTarget->Index) == 0)
	{
		lpTarget->PShopItemChange = 1;
	}
	else
	{
		lpTarget->PShopOpen = 0;
		memset(lpTarget->PShopText,0,sizeof(lpTarget->PShopText));
		gPersonalShop.GCPShopCloseSend(lpTarget->Index,1);
		gCustomStore.OnPShopClose(lpTarget);
	}

	lpTarget->PShopTransaction = 0;
}

void CCustomStore::CGOffTradeRecv(PMSG_OFFTRADE_RECV* lpMsg, int aIndex)
{
	if (lpMsg->Type >= 0 && lpMsg->Type <= 5)
	{
		gCustomStore.OpenCustomStore(&gObj[aIndex], lpMsg->Type);
	}
	else if (lpMsg->Type == 6)
	{
		gCustomStore.CommandCustomStoreOffline(&gObj[aIndex], NULL);
	}
	else if (lpMsg->Type == 7)
	{
		LPOBJ lpObj = &gObj[aIndex];
		lpObj->PShopOpen = 0;
		memset(lpObj->PShopText,0,sizeof(lpObj->PShopText));
		gPersonalShop.GCPShopCloseSend(aIndex,1);
		gCustomStore.OnPShopClose(lpObj);
	}
	// 8 upwards, and not 6, because 6 and 7 above are commands rather than store
	// types. OpenCustomStore does the real validation - this only decides that the
	// number is in the custom-item band at all.
	else if (lpMsg->Type >= CUSTOM_STORE_TYPE_ITEM && lpMsg->Type <= CUSTOM_STORE_TYPE_ITEM_LAST)
	{
		gCustomStore.OpenCustomStore(&gObj[aIndex], lpMsg->Type);
	}
}

void CCustomStore::GCCustomStoreItemInfoSend(int aIndex)
{
	PMSG_CUSTOMSTORE_ITEM_INFO_SEND pMsg;

	pMsg.header.set(0xF3, 0xF5, sizeof(pMsg));

	for(int slot=0;slot < MAX_CUSTOM_STORE_ITEM;slot++)
	{
		pMsg.Slot[slot].Enabled = ((this->IsCustomItemStore(slot)==0)?0:1);

		pMsg.Slot[slot].ItemIndex = this->GetCustomItemIndex(slot);

		// Zeroed first, then copied one short of the buffer, so the label is always
		// terminated no matter what the ini held - it is about to be handed to the
		// client as a C string.
		memset(pMsg.Slot[slot].Button,0,sizeof(pMsg.Slot[slot].Button));

		strncpy(pMsg.Slot[slot].Button,this->m_CustomStoreItemButton[slot],sizeof(pMsg.Slot[slot].Button)-1);
	}

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CCustomStore::GCOffTradeSend(int aIndex,int bIndex)
{
	PMSG_OFFTRADE_SEND pMsg;
	pMsg.header.set(0xF3, 0xEB, sizeof(pMsg));

	pMsg.Type = -1;

	if(gObj[bIndex].PShopCustom == 1)
	{
		pMsg.Type = gObj[bIndex].PShopCustomType;
	}
	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CCustomStore::GCOffActiveSend(int aIndex, int active)
{
	PMSG_SHOPACTIVE_SEND pMsg;
	pMsg.header.set(0xF3, 0xEC, sizeof(pMsg));
	pMsg.Active = active;

	pMsg.Type = -1;
	if(gObj[aIndex].PShopCustom == 1)
	{
		pMsg.Type = gObj[aIndex].PShopCustomType;
	}

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}
