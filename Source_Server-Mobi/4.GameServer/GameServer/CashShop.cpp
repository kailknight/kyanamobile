// CashShop.cpp: implementation of the CCashShop class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CashShop.h"
#include "BonusManager.h"
#include "CustomBuyVip.h"
#include "CSProtocol.h"
#include "DSProtocol.h"
#include "EffectManager.h"
#include "GameMain.h"
#include "ItemManager.h"
#include "JSProtocol.h"
#include "Log.h"
#include "Map.h"
#include "MemScript.h"
#include "Message.h"
#include "Notice.h"
#include "ObjectManager.h"
#include "ServerInfo.h"
#include "SocketItemOption.h"
#include "Util.h"
#include "Log.h"

CCashShop gCashShop;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCashShop::CCashShop() // OK
{
	#if(GAMESERVER_UPDATE>=501)

	this->m_CashShopPackageCount = 0;
	this->m_CashShopProductCount = 0;
	this->m_iLastCatalogVersion = 0;

	#endif
}

CCashShop::~CCashShop() // OK
{

}

void CCashShop::LoadPackage(char* path) // OK
{
	#if(GAMESERVER_UPDATE>=501)

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

	this->m_CashShopPackageCount = 0;

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

			CASH_SHOP_PACKAGE_INFO info;

			memset(&info,0,sizeof(info));

			info.Category = lpMemScript->GetNumber();

			info.BaseIndex = lpMemScript->GetAsNumber();

			info.MainIndex = lpMemScript->GetAsNumber();

			info.ItemIndex = lpMemScript->GetAsNumber();

			info.CoinIndex = lpMemScript->GetAsNumber();

			info.CoinValue = lpMemScript->GetAsNumber();

			info.BonusGP = lpMemScript->GetAsNumber();

			for(int n=0;n < MAX_CASH_SHOP_PACKAGE_PRODUCT;n++)
			{
				info.ProductBaseIndex[n] = lpMemScript->GetAsNumber();
			}

			for(int n=0;n < MAX_CASH_SHOP_PACKAGE_PRODUCT;n++)
			{
				info.ProductMainIndex[n] = lpMemScript->GetAsNumber();
			}

			this->SetPackageInfo(info);
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;

	#endif
}

void CCashShop::LoadProduct(char* path) // OK
{
	#if(GAMESERVER_UPDATE>=501)

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

	this->m_CashShopProductCount = 0;

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

			CASH_SHOP_PRODUCT_INFO info;

			memset(&info,0,sizeof(info));

			info.BaseIndex = lpMemScript->GetNumber();

			info.MainIndex = lpMemScript->GetAsNumber();

			info.CoinValue = lpMemScript->GetAsNumber();

			info.ItemIndex = lpMemScript->GetAsNumber();

			info.ItemLevel = lpMemScript->GetAsNumber();

			info.ItemOption1 = lpMemScript->GetAsNumber();

			info.ItemOption2 = lpMemScript->GetAsNumber();

			info.ItemOption3 = lpMemScript->GetAsNumber();

			info.ItemNewOption = lpMemScript->GetAsNumber();

			info.ItemSetOption = lpMemScript->GetAsNumber();

			info.ItemHarmonyOption = lpMemScript->GetAsNumber();

			info.ItemOptionEx = lpMemScript->GetAsNumber();

			info.ItemSocketOption1 = lpMemScript->GetAsNumber();

			info.ItemSocketOption2 = lpMemScript->GetAsNumber();

			info.ItemSocketOption3 = lpMemScript->GetAsNumber();

			info.ItemSocketOption4 = lpMemScript->GetAsNumber();

			info.ItemSocketOption5 = lpMemScript->GetAsNumber();

			info.ItemQuantity = lpMemScript->GetAsNumber();

			info.ItemDuration = lpMemScript->GetAsNumber();

			this->SetProductInfo(info);
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;

	#endif
}

void CCashShop::SetPackageInfo(CASH_SHOP_PACKAGE_INFO info) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(this->m_CashShopPackageCount < 0 || this->m_CashShopPackageCount >= MAX_CASH_SHOP_ITEM)
	{
		return;
	}

	this->m_CashShopPackageInfo[this->m_CashShopPackageCount++] = info;

	#endif
}

void CCashShop::SetProductInfo(CASH_SHOP_PRODUCT_INFO info) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(this->m_CashShopProductCount < 0 || this->m_CashShopProductCount >= MAX_CASH_SHOP_ITEM)
	{
		return;
	}

	this->m_CashShopProductInfo[this->m_CashShopProductCount++] = info;

	#endif
}

CASH_SHOP_PACKAGE_INFO* CCashShop::GetPackageInfo(int index) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	for(int n=0;n < MAX_CASH_SHOP_ITEM;n++)
	{
		if(this->m_CashShopPackageInfo[n].MainIndex == index)
		{
			return &this->m_CashShopPackageInfo[n];
		}
	}

	return 0;

	#else

	return 0;

	#endif
}

CASH_SHOP_PRODUCT_INFO* CCashShop::GetProductInfo(int index) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	for(int n=0;n < MAX_CASH_SHOP_ITEM;n++)
	{
		if(this->m_CashShopProductInfo[n].MainIndex == index)
		{
			return &this->m_CashShopProductInfo[n];
		}
	}

	return 0;

	#else

	return 0;

	#endif
}

CASH_SHOP_PRODUCT_INFO* CCashShop::GetProductInfoByBaseIndex(int index) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	for(int n=0;n < MAX_CASH_SHOP_ITEM;n++)
	{
		if(this->m_CashShopProductInfo[n].BaseIndex == index)
		{
			return &this->m_CashShopProductInfo[n];
		}
	}

	return 0;

	#else

	return 0;

	#endif
}

void CCashShop::MainProc() // OK
{
	#if(GAMESERVER_UPDATE>=501)

	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) == 0)
		{
			continue;
		}

		LPOBJ lpObj = &gObj[n];

		if((GetTickCount()-lpObj->CashShopGoblinPointTime) >= ((DWORD)gServerInfo.m_CashShopGoblinPointDelay*60000))
		{
			lpObj->CashShopGoblinPointTime = GetTickCount();
			this->GDCashShopAddPointSaveSend(lpObj->Index,0,0,0,gBonusManager.GetBonusValue(lpObj,BONUS_INDEX_GLOBIN_POINT,gServerInfo.m_CashShopGoblinPointValue[lpObj->AccountLevel],-1,-1,-1,-1),0);
			this->CGCashShopPointRecv(lpObj->Index);
		}

		for(int i=0;i < INVENTORY_SIZE;i++)
		{
			if(lpObj->Inventory[i].IsItem() == 0)
			{
				continue;
			}

			if(lpObj->Inventory[i].m_IsPeriodicItem == 0)
			{
				continue;
			}

			if(lpObj->Inventory[i].m_LoadPeriodicItem == 0)
			{
				continue;
			}

			if(lpObj->Inventory[i].m_PeriodicItemTime <= ((int)time(0)))
			{
				gItemManager.InventoryDelItem(lpObj->Index,i);
				gItemManager.GCItemDeleteSend(lpObj->Index,i,1);
				gItemManager.UpdateInventoryViewport(lpObj->Index,i);
			}
		}
	}

	#endif
}

int CCashShop::GetPackageProductBaseIndexCount(CASH_SHOP_PACKAGE_INFO* lpPackageInfo) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	int count = 0;

	for(int n=0;n < MAX_CASH_SHOP_PACKAGE_PRODUCT;n++)
	{
		if(lpPackageInfo->ProductBaseIndex[n] != 0)
		{
			count++;
		}
	}

	return count;

	#else

	return 0;

	#endif
}

int CCashShop::GetPackageProductMainIndexCount(CASH_SHOP_PACKAGE_INFO* lpPackageInfo) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	int count = 0;

	for(int n=0;n < MAX_CASH_SHOP_PACKAGE_PRODUCT;n++)
	{
		if(lpPackageInfo->ProductMainIndex[n] != 0)
		{
			count++;
		}
	}

	return count;

	#else

	return 0;

	#endif
}

void CCashShop::CGCashShopPointRecv(int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gServerInfo.m_CashShopSwitch == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	if(((lpObj->CashShopTransaction[0]==0)?(lpObj->CashShopTransaction[0]++):lpObj->CashShopTransaction[0]) != 0)
	{
		return;
	}

	SDHP_CASH_SHOP_POINT_SEND pMsg;

	pMsg.header.set(0x18,0x00,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);

	#else

	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	SDHP_CASH_SHOP_POINT_SEND pMsg;

	pMsg.header.set(0x18,0x00,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);

	#endif
}

void CCashShop::CGCashShopOpenRecv(PMSG_CASH_SHOP_OPEN_RECV* lpMsg,int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gServerInfo.m_CashShopSwitch == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	
	
	
	PMSG_CASH_SHOP_OPEN_SEND pMsg;

	pMsg.header.set(0xD2,0x02,sizeof(pMsg));

	if (lpObj->Interface.type == INTERFACE_CHAOS_BOX || lpObj->Interface.type == INTERFACE_TRADE || lpObj->Interface.type == INTERFACE_WAREHOUSE || lpObj->Interface.type == INTERFACE_PERSONAL_SHOP || lpObj->Interface.type == INTERFACE_PARTY)
	{
		pMsg.result = 0;
		DataSend(aIndex, (BYTE*)&pMsg, pMsg.header.size);
		return;
	}

	if (gItemManager.ChaosBoxHasItem(lpObj) || gItemManager.TradeHasItem(lpObj))
	{
		pMsg.result = 0;
		DataSend(aIndex, (BYTE*)&pMsg, pMsg.header.size);
		return;
	}
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		pMsg.result = 0;
		DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	if(gMap[lpObj->Map].CheckAttr(lpObj->X,lpObj->Y,1) == 0)
	{
		pMsg.result = 0;
		DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	if(lpMsg->OpenType == 0)
	{
		if(lpObj->Interface.use == 0)
		{
			lpObj->Interface.use = 1;
			lpObj->Interface.type = INTERFACE_CASH_SHOP;
			lpObj->Interface.state = 0;

			pMsg.result = 1;

			DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);

			// Shelf prices follow the open, so a sale that started since login is
			// visible without a relog. No-op unless CustomCashShop is on.
			this->GDCashShopPriceReqSend(aIndex);
		}
		else
		{
			pMsg.result = 8;

			DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
		}
	}
	else
	{
		lpObj->Interface.use = 0;
		lpObj->Interface.type = INTERFACE_NONE;
		lpObj->Interface.state = 0;

		pMsg.result = 0;

		DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
	}

	#endif
}

void CCashShop::CGCashShopItemBuyRecv(PMSG_CASH_SHOP_ITEM_BUY_RECV* lpMsg,int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gServerInfo.m_CashShopSwitch == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	//itemLock
	if(lpObj->Lock > 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,2,0,gMessage.GetMessage(778));
		return;
	}

	if(((lpObj->CashShopTransaction[1]==0)?(lpObj->CashShopTransaction[1]++):lpObj->CashShopTransaction[1]) != 0)
	{
		return;
	}

	SDHP_CASH_SHOP_ITEM_BUY_SEND pMsg;

	pMsg.header.set(0x18,0x01,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	pMsg.PackageMainIndex = lpMsg->PackageMainIndex;

	pMsg.Category = lpMsg->Category;

	pMsg.ProductMainIndex = lpMsg->ProductMainIndex;

	pMsg.ItemIndex = lpMsg->ItemIndex;

	pMsg.CoinIndex = lpMsg->CoinIndex;

	pMsg.MileageFlag = lpMsg->MileageFlag;

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);

	#endif
}

void CCashShop::CGCashShopItemGifRecv(PMSG_CASH_SHOP_ITEM_GIF_RECV* lpMsg,int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gServerInfo.m_CashShopSwitch == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	if(((lpObj->CashShopTransaction[1]==0)?(lpObj->CashShopTransaction[1]++):lpObj->CashShopTransaction[1]) != 0)
	{
		return;
	}

	SDHP_CASH_SHOP_ITEM_GIF_SEND pMsg;

	pMsg.header.set(0x18,0x02,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	pMsg.PackageMainIndex = lpMsg->PackageMainIndex;

	pMsg.Category = lpMsg->Category;

	pMsg.ProductMainIndex = lpMsg->ProductMainIndex;

	pMsg.SaleZone = lpMsg->SaleZone;

	pMsg.ItemIndex = lpMsg->ItemIndex;

	pMsg.CoinIndex = lpMsg->CoinIndex;

	pMsg.MileageFlag = lpMsg->MileageFlag;

	memcpy(pMsg.GiftName,lpMsg->GiftName,sizeof(pMsg.GiftName));

	memcpy(pMsg.GiftText,lpMsg->GiftText,sizeof(pMsg.GiftText));

	gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

void CCashShop::CGCashShopItemNumRecv(PMSG_CASH_SHOP_ITEM_NUM_RECV* lpMsg,int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gServerInfo.m_CashShopSwitch == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	if(((lpObj->CashShopTransaction[2]==0)?(lpObj->CashShopTransaction[2]++):lpObj->CashShopTransaction[2]) != 0)
	{
		return;
	}

	SDHP_CASH_SHOP_ITEM_NUM_SEND pMsg;

	pMsg.header.set(0x18,0x03,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	pMsg.InventoryPage = lpMsg->InventoryPage;

	pMsg.InventoryType = lpMsg->InventoryType;

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);

	#endif
}

void CCashShop::CGCashShopItemUseRecv(PMSG_CASH_SHOP_ITEM_USE_RECV* lpMsg,int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gServerInfo.m_CashShopSwitch == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	if(((lpObj->CashShopTransaction[3]==0)?(lpObj->CashShopTransaction[3]++):lpObj->CashShopTransaction[3]) != 0)
	{
		return;
	}

	SDHP_CASH_SHOP_ITEM_USE_SEND pMsg;

	pMsg.header.set(0x18,0x04,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	pMsg.BaseItemCode = lpMsg->BaseItemCode;

	pMsg.MainItemCode = lpMsg->MainItemCode;

	pMsg.ItemIndex = lpMsg->ItemIndex;

	pMsg.ProductType = lpMsg->ProductType;

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);

	#endif
}

void CCashShop::GCCashShopInitSend(LPOBJ lpObj) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gServerInfo.m_CashShopSwitch == 0)
	{
		return;
	}

	PSBMSG_HEAD pMsg;

	pMsg.set(0xD2,0x00,sizeof(pMsg));

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.size);

	this->GCCashShopScriptVersionSend(lpObj);

	this->GCCashShopBannerVersionSend(lpObj);

	#endif
}

void CCashShop::GCCashShopScriptVersionSend(LPOBJ lpObj) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	PMSG_CASH_SHOP_VERSION_SEND pMsg;

	pMsg.header.set(0xD2,0x0C,sizeof(pMsg));

	pMsg.version[0] = gServerInfo.m_CashShopScriptVersion1;

	pMsg.version[1] = gServerInfo.m_CashShopScriptVersion2;

	pMsg.version[2] = gServerInfo.m_CashShopScriptVersion3;

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);

	#endif
}

void CCashShop::GCCashShopBannerVersionSend(LPOBJ lpObj) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	PMSG_CASH_SHOP_VERSION_SEND pMsg;

	pMsg.header.set(0xD2,0x15,sizeof(pMsg));

	pMsg.version[0] = gServerInfo.m_CashShopBannerVersion1;

	pMsg.version[1] = gServerInfo.m_CashShopBannerVersion2;

	pMsg.version[2] = gServerInfo.m_CashShopBannerVersion3;

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);

	#endif
}

void CCashShop::GCCashShopItemSend(LPOBJ lpObj,int PageCount,CASH_SHOP_INVENTORY* CashInventory) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	PMSG_CASH_SHOP_ITEM_SEND pMsg;

	pMsg.header.set(0xD2,0x0D,sizeof(pMsg));

	for(int n=0;n < PageCount;n++)
	{
		pMsg.BaseItemCode = CashInventory[n].BaseItemCode;

		pMsg.MainItemCode = CashInventory[n].MainItemCode;

		pMsg.PackageMainIndex = CashInventory[n].PackageMainIndex;

		pMsg.ProductBaseIndex = CashInventory[n].ProductBaseIndex;

		pMsg.ProductMainIndex = CashInventory[n].ProductMainIndex;

		pMsg.CoinValue = CashInventory[n].CoinValue;

		pMsg.ProductType = CashInventory[n].ProductType;

		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
	}

	#endif
}

void CCashShop::GCCashShopGiftSend(LPOBJ lpObj,int PageCount,CASH_SHOP_INVENTORY* CashInventory) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	PMSG_CASH_SHOP_GIFT_SEND pMsg;

	pMsg.header.set(0xD2,0x0E,sizeof(pMsg));

	for(int n=0;n < PageCount;n++)
	{
		pMsg.BaseItemCode = CashInventory[n].BaseItemCode;

		pMsg.MainItemCode = CashInventory[n].MainItemCode;

		pMsg.PackageMainIndex = CashInventory[n].PackageMainIndex;

		pMsg.ProductBaseIndex = CashInventory[n].ProductBaseIndex;

		pMsg.ProductMainIndex = CashInventory[n].ProductMainIndex;

		pMsg.CoinValue = CashInventory[n].CoinValue;

		pMsg.ProductType = CashInventory[n].ProductType;

		memcpy(pMsg.GiftName,CashInventory[n].GiftName,sizeof(pMsg.GiftName));

		memcpy(pMsg.GiftText,CashInventory[n].GiftText,sizeof(pMsg.GiftText));

		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
	}

	#endif
}

void CCashShop::GCCashShopPeriodicItemSend(int aIndex,int index,int slot,int time) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	PMSG_CASH_SHOP_PERIODIC_ITEM_SEND pMsg;

	pMsg.header.set(0xD2,0x12,sizeof(pMsg));

	pMsg.index = index;

	pMsg.slot = slot;

	pMsg.time = time;

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);

	#endif
}

void CCashShop::DGCashShopPointRecv(SDHP_CASH_SHOP_POINT_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if (!gObjIsAccountValid(lpMsg->index, lpMsg->account))
	{
		LogAdd(LOG_RED,"[DGCashShopPointRecv] Invalid Account [%d](%s)",lpMsg->index,lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

#if (FIXBUGCOIN)
	lpObj->Coin1 = CheckCoinBB(lpMsg->WCoinC);
	lpObj->Coin2 = CheckCoinBB(lpMsg->WCoinP);
	lpObj->Coin3 = CheckCoinBB(lpMsg->GoblinPoint);
	lpObj->Ruud = CheckCoinBB(lpMsg->Ruud);
#else
	lpObj->Coin1 = lpMsg->WCoinC;
	lpObj->Coin2 = lpMsg->WCoinP;
	lpObj->Coin3 = lpMsg->GoblinPoint;
	lpObj->Ruud = lpMsg->Ruud;
#endif

	lpObj->CashShopTransaction[0] = 0;

	PMSG_CASH_SHOP_POINT_SEND pMsg;

	pMsg.header.set(0xD2,0x01,sizeof(pMsg));

	pMsg.result = 0;

	pMsg.WCoinC[0] = lpObj->Coin1;

	pMsg.WCoinC[1] = lpObj->Coin1;

	pMsg.WCoinP[0] = lpObj->Coin2;

	pMsg.WCoinP[1] = lpObj->Coin2;

	pMsg.GoblinPoint = lpObj->Coin3;

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);


	if (lpObj->BuyVip > 0)
	{
		gCustomBuyVip.BuyVipDone(lpObj);
	}

	#else

	if(gObjIsAccountValid(lpMsg->index,lpMsg->account) == 0)
	{
		LogAdd(LOG_RED,"[DGCashShopPointRecv] Invalid Account [%d](%s)",lpMsg->index,lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	lpObj->Coin1 = lpMsg->WCoinC;
	lpObj->Coin2 = lpMsg->WCoinP;
	lpObj->Coin3 = lpMsg->GoblinPoint;

	if (lpObj->BuyVip > 0)
	{
		gCustomBuyVip.BuyVipDone(lpObj);
	}

	#endif

	this->GCSendCoin(lpObj, lpObj->Coin1, lpObj->Coin2, lpObj->Coin3);
}

void CCashShop::GDCashShopCatalogReqSend() // OK
{
	if(gServerInfo.m_CustomCashShop == 0)
	{
		return;
	}

	SDHP_CASH_SHOP_CATALOG_REQ_SEND pMsg;

	pMsg.header.set(0x18,0x08,sizeof(pMsg));

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);

	LogAddCat(LOG_CAT_CASHSHOP,LOG_BLUE,"[CashShop] custom catalog requested from the DataServer");
}

void CCashShop::DGCashShopCatalogBeginRecv(SDHP_CASH_SHOP_CATALOG_BEGIN_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	// Clearing happens here rather than when the request goes out, so a
	// DataServer that never answers leaves the shop on whatever the .txt files
	// last gave it instead of empty.
	this->m_CashShopProductCount = 0;

	this->m_CashShopPackageCount = 0;

	LogAddCat(LOG_CAT_CASHSHOP,LOG_BLUE,"[CashShop] custom catalog incoming: %d products, %d packages",lpMsg->ProductCount,lpMsg->PackageCount);

	if(lpMsg->ProductCount > MAX_CASH_SHOP_ITEM || lpMsg->PackageCount > MAX_CASH_SHOP_ITEM)
	{
		// SetProductInfo/SetPackageInfo drop silently past the cap. Say so once
		// here rather than let the shop quietly come up short.
		LogAddCat(LOG_CAT_CASHSHOP,LOG_RED,"[CashShop] catalog exceeds MAX_CASH_SHOP_ITEM (%d) - the tail will be dropped",MAX_CASH_SHOP_ITEM);
	}

	#endif
}

void CCashShop::DGCashShopCatalogProductRecv(SDHP_CASH_SHOP_CATALOG_PRODUCT_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	CASH_SHOP_PRODUCT_INFO info;

	memset(&info,0,sizeof(info));

	info.BaseIndex = lpMsg->BaseIndex;
	info.MainIndex = lpMsg->MainIndex;
	info.CoinValue = lpMsg->CoinValue;
	info.ItemIndex = lpMsg->ItemIndex;
	info.ItemLevel = lpMsg->ItemLevel;
	info.ItemOption1 = lpMsg->ItemOption1;
	info.ItemOption2 = lpMsg->ItemOption2;
	info.ItemOption3 = lpMsg->ItemOption3;
	info.ItemNewOption = lpMsg->ItemNewOption;
	info.ItemSetOption = lpMsg->ItemSetOption;
	info.ItemHarmonyOption = lpMsg->ItemHarmonyOption;
	info.ItemOptionEx = lpMsg->ItemOptionEx;
	info.ItemSocketOption1 = lpMsg->ItemSocketOption1;
	info.ItemSocketOption2 = lpMsg->ItemSocketOption2;
	info.ItemSocketOption3 = lpMsg->ItemSocketOption3;
	info.ItemSocketOption4 = lpMsg->ItemSocketOption4;
	info.ItemSocketOption5 = lpMsg->ItemSocketOption5;
	info.ItemQuantity = lpMsg->ItemQuantity;
	info.ItemDuration = lpMsg->ItemDuration;

	// Same setter the .txt loader uses, so everything downstream is unchanged.
	this->SetProductInfo(info);

	#endif
}

void CCashShop::DGCashShopCatalogPackageRecv(SDHP_CASH_SHOP_CATALOG_PACKAGE_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	CASH_SHOP_PACKAGE_INFO info;

	memset(&info,0,sizeof(info));

	info.Category = lpMsg->Category;
	info.BaseIndex = lpMsg->BaseIndex;
	info.MainIndex = lpMsg->MainIndex;
	info.ItemIndex = lpMsg->ItemIndex;
	info.CoinIndex = lpMsg->CoinIndex;
	info.CoinValue = lpMsg->CoinValue;
	info.BonusGP = lpMsg->BonusGP;

	for(int n=0;n < MAX_CASH_SHOP_PACKAGE_PRODUCT;n++)
	{
		info.ProductBaseIndex[n] = lpMsg->ProductBaseIndex[n];
		info.ProductMainIndex[n] = lpMsg->ProductMainIndex[n];
	}

	this->SetPackageInfo(info);

	if(lpMsg->IsLast != 0)
	{
		/*
			The stream is complete, so anything past the new counts is the tail
			of an older, longer catalog. Clear it.

			GetPackageInfo and GetProductInfo scan all MAX_CASH_SHOP_ITEM slots
			rather than stopping at the count, so a reload returning fewer rows
			would otherwise leave those still findable - an item deleted in the
			admin tool would stay sellable for the life of the process.

			Done here and not in DGCashShopCatalogBeginRecv on purpose: clearing
			up front would throw away the CashShopPackage.txt catalog the moment
			a push started, so a stream that died half way through would leave
			the server with nothing to sell instead of what it had before.
		*/
		for(int n=this->m_CashShopProductCount;n < MAX_CASH_SHOP_ITEM;n++)
		{
			memset(&this->m_CashShopProductInfo[n],0,sizeof(CASH_SHOP_PRODUCT_INFO));
		}

		for(int n=this->m_CashShopPackageCount;n < MAX_CASH_SHOP_ITEM;n++)
		{
			memset(&this->m_CashShopPackageInfo[n],0,sizeof(CASH_SHOP_PACKAGE_INFO));
		}

		LogAddCat(LOG_CAT_CASHSHOP,LOG_BLUE,"[CashShop] custom catalog loaded: %d products, %d packages",this->m_CashShopProductCount,this->m_CashShopPackageCount);
	}

	#endif
}

/*
	Ask the DataServer what the shelf should say for this player. Fire and forget
	from here: the reply carries the player index back, and DGCashShopPriceRecv
	forwards it on. If the DataServer link is down nothing arrives and the client
	keeps drawing its IBSPackage.txt prices, which is the correct fallback.
*/
void CCashShop::GDCashShopPriceReqSend(int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gServerInfo.m_CustomCashShop == 0)
	{
		return;
	}

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	SDHP_CASH_SHOP_PRICE_REQ_SEND pMsg;

	pMsg.header.set(0x18,0x0B,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);

	#endif
}

/*
	One batch of prices back from the DataServer, forwarded to the player who
	opened the shop. The account is re-checked because the index alone is not
	enough - between the request and the reply the slot can be recycled by a new
	login, and prices are per-server, not per-player, so a stale send would just
	be noise in somebody else's shop.
*/
void CCashShop::DGCashShopPriceRecv(SDHP_CASH_SHOP_PRICE_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	int aIndex = lpMsg->index;

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(memcmp(lpObj->Account,lpMsg->account,sizeof(lpMsg->account)) != 0)
	{
		return;
	}

	int count = lpMsg->Count;

	if(count < 0 || count > MAX_CASH_SHOP_PRICE_BATCH)
	{
		return;
	}

	PMSG_CASH_SHOP_PRICE_SEND pMsg;

	// Sized to the rows actually present, not to the array - an empty tail packet
	// is a bare header rather than 192 bytes of zeroes. Derived from sizeof rather
	// than written out, so the explicit padding above stays accounted for.
	pMsg.header.set(0xD2,0x20,(sizeof(pMsg)-sizeof(pMsg.Row))+(count*sizeof(CASH_SHOP_PRICE_ROW)));

	pMsg.Count = (BYTE)count;

	pMsg.IsLast = lpMsg->IsLast;

	if(count > 0)
	{
		memcpy(pMsg.Row,lpMsg->Row,count*sizeof(CASH_SHOP_PRICE_ROW));
	}

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);

	#endif
}

/*
	One batch of package display-name overrides back from the DataServer,
	forwarded to the player who opened the shop. Mirrors DGCashShopPriceRecv
	exactly, including the stale-slot account re-check - this rides the same
	request/reply round trip as prices, just a separate sub-code.
*/
/*
	"Send me the display catalog, unless it is still version N." Client 0xD2:0x23.

	Asked once per session by the client, and again only when it is told the
	version moved - not on every shop open. Price still goes every open, because
	discounts are time-bounded and have to stay live, but a hundred catalog rows
	on every keypress would be waste.
*/
void CCashShop::CGCashShopCatalogReqRecv(PMSG_CASH_SHOP_CATALOG_REQ_RECV* lpMsg,int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	if(gServerInfo.m_CustomCashShop == 0)
	{
		return;
	}

	this->GDCashShopCatalogLineReqSend(aIndex,lpMsg->CachedVersion);

	#endif
}

void CCashShop::GDCashShopCatalogLineReqSend(int aIndex,int cachedVersion) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gServerInfo.m_CustomCashShop == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	SDHP_CASH_SHOP_CATALOG_LINE_REQ_SEND pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0x18,0x0F,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	pMsg.CachedVersion = cachedVersion;

	gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

/*
	One catalog fragment on its way through. Head 0x18 sub 0x10 in, 0xD2 sub 0x24
	out.

	A straight relay - the fragment is copied across untouched, because the
	GameServer has no reason to understand a script line and every reason not to
	start parsing one. The account is re-checked the same way the price and name
	relays check it, so a stale index cannot deliver another player's answer.
*/
void CCashShop::DGCashShopCatalogLineRecv(SDHP_CASH_SHOP_CATALOG_LINE_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	/*
		The catalog version moved, so this server's SELLABLE catalog is stale.

		Two different catalogs are in play and only one of them was being kept
		current. The display catalog (what the shelf draws) is pulled per player
		by the request this fragment is answering, so a new item appeared on the
		shelf the moment it was added. The sellable catalog - m_CashShopPackageInfo,
		what GetPackageInfo searches - was only ever loaded on the DataServer
		handshake, which is GameServer start or a DataServer reconnect.

		So a freshly added item drew fine and then failed to buy: GetPackageInfo
		returned 0 and DGCashShopItemBuyRecv answered result 3, which the client
		shows as "not in stock". The only cure was restarting the GameServer.

		Every reply carries the true current version, including the "up to date"
		one, so this fires at most once per actual change. Recording the version
		before sending means the rest of the fragments in this stream do not
		queue a second reload.
	*/
	if(lpMsg->Version != 0 && lpMsg->Version != this->m_iLastCatalogVersion)
	{
		this->m_iLastCatalogVersion = lpMsg->Version;

		LogAddCat(LOG_CAT_CASHSHOP,LOG_BLUE,"[CashShop] catalog version is now %d - reloading the sellable catalog",lpMsg->Version);

		this->GDCashShopCatalogReqSend();
	}

	int aIndex = lpMsg->index;

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(memcmp(lpObj->Account,lpMsg->account,sizeof(lpMsg->account)) != 0)
	{
		return;
	}

	PMSG_CASH_SHOP_CATALOG_SEND pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0xD2,0x24,sizeof(pMsg));

	pMsg.RowKind = lpMsg->RowKind;
	pMsg.Flags = lpMsg->Flags;
	pMsg.Version = lpMsg->Version;

	memcpy(pMsg.Fragment,lpMsg->Fragment,sizeof(pMsg.Fragment));

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);

	#endif
}

void CCashShop::DGCashShopNameRecv(SDHP_CASH_SHOP_NAME_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	int aIndex = lpMsg->index;

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(memcmp(lpObj->Account,lpMsg->account,sizeof(lpMsg->account)) != 0)
	{
		return;
	}

	int count = lpMsg->Count;

	if(count < 0 || count > MAX_CASH_SHOP_NAME_BATCH)
	{
		return;
	}

	PMSG_CASH_SHOP_NAME_SEND pMsg;

	pMsg.header.set(0xD2,0x21,(sizeof(pMsg)-sizeof(pMsg.Row))+(count*sizeof(CASH_SHOP_NAME_ROW)));

	pMsg.Count = (BYTE)count;

	pMsg.IsLast = lpMsg->IsLast;

	if(count > 0)
	{
		memcpy(pMsg.Row,lpMsg->Row,count*sizeof(CASH_SHOP_NAME_ROW));
	}

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);

	#endif
}

/*
	The active banner back from the DataServer, forwarded to the player who
	opened the shop. Same stale-slot account re-check as prices/names, same
	request/reply round trip, separate sub-code. Single row, not a batch.
*/
void CCashShop::DGCashShopBannerRecv(SDHP_CASH_SHOP_BANNER_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	int aIndex = lpMsg->index;

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(memcmp(lpObj->Account,lpMsg->account,sizeof(lpMsg->account)) != 0)
	{
		return;
	}

	PMSG_CASH_SHOP_BANNER_SEND pMsg;

	pMsg.header.set(0xD2,0x22,sizeof(pMsg));

	pMsg.BannerId = lpMsg->BannerId;

	memcpy(pMsg.ImagePath,lpMsg->ImagePath,sizeof(pMsg.ImagePath));

	pMsg.TargetPackageId = lpMsg->TargetPackageId;

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);

	#endif
}

void CCashShop::GDCashShopLogSend(int aIndex,int BaseIndex,int MainIndex,int ItemIndex,int CoinIndex,int ListPrice,int PaidPrice,int DiscountPercent,BYTE Result) // OK
{
	if(gServerInfo.m_CustomCashShop == 0)
	{
		return;
	}

	if(gObjIsConnected(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	SDHP_CASH_SHOP_LOG_SEND pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0x18,0x07,sizeof(pMsg));

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	memcpy(pMsg.character,lpObj->Name,sizeof(pMsg.character));

	memcpy(pMsg.IpAddress,lpObj->IpAddr,sizeof(pMsg.IpAddress));

	pMsg.BaseIndex = BaseIndex;

	pMsg.MainIndex = MainIndex;

	pMsg.ItemIndex = ItemIndex;

	pMsg.CoinIndex = CoinIndex;

	pMsg.ListPrice = ListPrice;

	pMsg.PaidPrice = PaidPrice;

	pMsg.DiscountPercent = (short)DiscountPercent;

	pMsg.Result = Result;

	pMsg.ServerCode = (short)gServerInfo.m_ServerCode;

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CCashShop::DGCashShopItemBuyRecv(SDHP_CASH_SHOP_ITEM_BUY_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gObjIsAccountValid(lpMsg->index,lpMsg->account) == 0)
	{
		LogAdd(LOG_RED,"[DGCashShopItemBuyRecv] Invalid Account [%d](%s)",lpMsg->index,lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	lpObj->CashShopTransaction[1] = 0;

	PMSG_CASH_SHOP_ITEM_BUY_SEND pMsg;

	pMsg.header.set(0xD2,0x03,sizeof(pMsg));

	CASH_SHOP_PACKAGE_INFO* lpPackageInfo = this->GetPackageInfo(lpMsg->PackageMainIndex);

	if(lpPackageInfo == 0)
	{
		pMsg.result = 3;
		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	if(lpMsg->result != 0)
	{
		pMsg.result = 3;
		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	if(lpPackageInfo->CoinIndex != lpMsg->CoinIndex)
	{
		pMsg.result = 3;
		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	if(lpMsg->ProductMainIndex == 0)
	{
		// Charge what the DataServer resolved. Falling back to the catalog price when
		// it could not resolve (CustomCashShop off, tables absent, no matching row)
		// means a broken lookup degrades to the old behaviour, never to a free item.
		int ChargePrice = ((gServerInfo.m_CustomCashShop != 0 && lpMsg->EffectivePrice >= 0) ? lpMsg->EffectivePrice : lpPackageInfo->CoinValue);

		int ShelfPrice = ((lpMsg->ListPrice >= 0) ? lpMsg->ListPrice : lpPackageInfo->CoinValue);

		DWORD WCoinC = ((lpPackageInfo->CoinIndex==508)?ChargePrice:0);

		DWORD WCoinP = ((lpPackageInfo->CoinIndex==509)?ChargePrice:0);

		DWORD GoblinPoint = ((lpPackageInfo->CoinIndex==0)?ChargePrice:0);

		if(WCoinC > lpMsg->WCoinC)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if(WCoinP > lpMsg->WCoinP)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if(GoblinPoint > lpMsg->GoblinPoint)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if((lpMsg->ItemCount+this->GetPackageProductBaseIndexCount(lpPackageInfo)) > (MAX_CASH_SHOP_PAGE*MAX_CASH_SHOP_PAGE_ITEM))
		{
			pMsg.result = 2;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

	//	gLog.Output(LOG_CASH_SHOP,"[CashShopItemBuy][PackageInfo][%s][%s] - (Category: %d, BaseIndex: %d, MainIndex: %d, ItemIndex: %d, CoinIndex: %d, CoinValue: %d, BonusGP: %d)",lpObj->Account,lpObj->Name,lpPackageInfo->Category,lpPackageInfo->BaseIndex,lpPackageInfo->MainIndex,lpPackageInfo->ItemIndex,lpPackageInfo->CoinIndex,lpPackageInfo->CoinValue,lpPackageInfo->BonusGP);

		for(int n=0;n < MAX_CASH_SHOP_PACKAGE_PRODUCT;n++)
		{
			if(lpPackageInfo->ProductBaseIndex[n] != 0)
			{
				CASH_SHOP_PRODUCT_INFO* lpProductInfo = ((lpPackageInfo->ProductMainIndex[n]==0)?this->GetProductInfoByBaseIndex(lpPackageInfo->ProductBaseIndex[n]):this->GetProductInfo(lpPackageInfo->ProductMainIndex[n]));

				/*
					A package can outlive the product it lists - delete the product
					in the admin tool and leave the package, and the lookup above
					returns 0. Every field below dereferences it, so without this
					the purchase takes the GameServer down rather than failing.
				*/
				if(lpProductInfo == 0)
				{
					LogAddCat(LOG_CAT_CASHSHOP,LOG_RED,"[CashShop] package %d lists product %d/%d which is not in the catalog - skipped",lpPackageInfo->MainIndex,lpPackageInfo->ProductBaseIndex[n],lpPackageInfo->ProductMainIndex[n]);
					continue;
				}

				this->GDCashShopInsertItemSaveSend(lpObj->Index,0,83,lpPackageInfo->MainIndex,lpProductInfo->BaseIndex,lpProductInfo->MainIndex,lpProductInfo->CoinValue,80,0,0);
				gLog.Output(LOG_CASH_SHOP, "[Mua-XShop][%s/%s] - (Base: %d/%d, Coin: %d, Item: %s, Cấp độ: +%d, ItemOption1: %d, ItemOption2: %d, ItemOption3: %d, ItemNewOption: %d, ItemSetOption: %d, ItemHarmonyOption: %d, ItemOptionEx: %d, ItemSocketOption: %d, %d, %d, %d, %d, ItemQuantity: %d, ItemDuration: %d)", lpObj->Account, lpObj->Name, lpProductInfo->BaseIndex, lpProductInfo->MainIndex, lpProductInfo->CoinValue, gItemManager.GetItemName(lpProductInfo->ItemIndex), lpProductInfo->ItemLevel, lpProductInfo->ItemOption1, lpProductInfo->ItemOption2, lpProductInfo->ItemOption3, lpProductInfo->ItemNewOption, lpProductInfo->ItemSetOption, lpProductInfo->ItemHarmonyOption, lpProductInfo->ItemOptionEx, lpProductInfo->ItemSocketOption1, lpProductInfo->ItemSocketOption2, lpProductInfo->ItemSocketOption3, lpProductInfo->ItemSocketOption4, lpProductInfo->ItemSocketOption5, lpProductInfo->ItemQuantity, lpProductInfo->ItemDuration);
				gLog.Output(LOG_WC, "[CashShop][%s/%s]Co San: %d WC- (Coin:- %d, Item: %s, ItemDuration: %d)", lpObj->Account, lpObj->Name, lpObj->Coin1, lpProductInfo->CoinValue, gItemManager.GetItemName(lpProductInfo->ItemIndex), lpProductInfo->ItemDuration);

			}
		}

		pMsg.result = 0;

		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);

		this->GDCashShopSubPointSaveSend(lpObj->Index,0,WCoinC,WCoinP,GoblinPoint, 0,"DGCashShopItemBuyRecv");

		if(lpPackageInfo->BonusGP > 0){this->GDCashShopAddPointSaveSend(lpObj->Index,0,0,0,lpPackageInfo->BonusGP,0);}

		// Bundle path. ListPrice == PaidPrice until the discount engine lands.
		this->GDCashShopLogSend(lpObj->Index,lpPackageInfo->BaseIndex,lpPackageInfo->MainIndex,lpPackageInfo->ItemIndex,lpPackageInfo->CoinIndex,ShelfPrice,ChargePrice,lpMsg->DiscountPercent,0);
	}
	else
	{
		CASH_SHOP_PRODUCT_INFO* lpProductInfo = this->GetProductInfo(lpMsg->ProductMainIndex);

		if(lpProductInfo == 0)
		{
			return;
		}

		// Charge what the DataServer resolved. Falling back to the catalog price when
		// it could not resolve (CustomCashShop off, tables absent, no matching row)
		// means a broken lookup degrades to the old behaviour, never to a free item.
		int ChargePrice = ((gServerInfo.m_CustomCashShop != 0 && lpMsg->EffectivePrice >= 0) ? lpMsg->EffectivePrice : lpProductInfo->CoinValue);

		int ShelfPrice = ((lpMsg->ListPrice >= 0) ? lpMsg->ListPrice : lpProductInfo->CoinValue);

		DWORD WCoinC = ((lpPackageInfo->CoinIndex==508)?ChargePrice:0);

		DWORD WCoinP = ((lpPackageInfo->CoinIndex==509)?ChargePrice:0);

		DWORD GoblinPoint = ((lpPackageInfo->CoinIndex==0)?ChargePrice:0);

		if(WCoinC > lpMsg->WCoinC)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if(WCoinP > lpMsg->WCoinP)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if(GoblinPoint > lpMsg->GoblinPoint)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if((lpMsg->ItemCount+1) > (MAX_CASH_SHOP_PAGE*MAX_CASH_SHOP_PAGE_ITEM))
		{
			pMsg.result = 2;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		//gLog.Output(LOG_CASH_SHOP,"[CashShopItemBuy][PackageInfo][%s][%s] - (Category: %d, BaseIndex: %d, MainIndex: %d, ItemIndex: %d, CoinIndex: %d, CoinValue: %d, BonusGP: %d)",lpObj->Account,lpObj->Name,lpPackageInfo->Category,lpPackageInfo->BaseIndex,lpPackageInfo->MainIndex,lpPackageInfo->ItemIndex,lpPackageInfo->CoinIndex,lpPackageInfo->CoinValue,lpPackageInfo->BonusGP);

		this->GDCashShopInsertItemSaveSend(lpObj->Index,0,83,lpPackageInfo->MainIndex,lpProductInfo->BaseIndex,lpProductInfo->MainIndex,lpProductInfo->CoinValue,80,0,0);

		gLog.Output(LOG_CASH_SHOP, "[CashShopItemBuy][ProductInfo][%s][%s] - (Base: %d, Main: %d, CoinValue: %d, Item: %s, Cấp độ: %d, ItemOption1: %d, ItemOption2: %d, ItemOption3: %d, ItemNewOption: %d, ItemSetOption: %d, ItemHarmonyOption: %d, ItemOptionEx: %d, ItemSocketOption: %d, %d, %d, %d, %d, ItemQuantity: %d, ItemDuration: %d)", lpObj->Account, lpObj->Name, lpProductInfo->BaseIndex, lpProductInfo->MainIndex, lpProductInfo->CoinValue, gItemManager.GetItemName(lpProductInfo->ItemIndex), lpProductInfo->ItemLevel, lpProductInfo->ItemOption1, lpProductInfo->ItemOption2, lpProductInfo->ItemOption3, lpProductInfo->ItemNewOption, lpProductInfo->ItemSetOption, lpProductInfo->ItemHarmonyOption, lpProductInfo->ItemOptionEx, lpProductInfo->ItemSocketOption1, lpProductInfo->ItemSocketOption2, lpProductInfo->ItemSocketOption3, lpProductInfo->ItemSocketOption4, lpProductInfo->ItemSocketOption5, lpProductInfo->ItemQuantity, lpProductInfo->ItemDuration);
		gLog.Output(LOG_WC, "[Cahsop][%s/%s]Co San: %d WC- (Coin:- %d, Item: %s, ItemDuration: %d)", lpObj->Account, lpObj->Name, lpObj->Coin1, lpProductInfo->CoinValue, gItemManager.GetItemName(lpProductInfo->ItemIndex), lpProductInfo->ItemDuration);

		pMsg.result = 0;

		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);

		this->GDCashShopSubPointSaveSend(lpObj->Index,0,WCoinC,WCoinP,GoblinPoint, 0,"DGCashShopItemBuyRecv");

		if(lpPackageInfo->BonusGP > 0){this->GDCashShopAddPointSaveSend(lpObj->Index,0,0,0,lpPackageInfo->BonusGP,0);}

		// Variant path - charged the product price, not the package price.
		this->GDCashShopLogSend(lpObj->Index,lpProductInfo->BaseIndex,lpProductInfo->MainIndex,lpProductInfo->ItemIndex,lpPackageInfo->CoinIndex,ShelfPrice,ChargePrice,lpMsg->DiscountPercent,0);
	}

	#endif
}

void CCashShop::DGCashShopItemGifRecv(SDHP_CASH_SHOP_ITEM_GIF_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gObjIsAccountValid(lpMsg->index,lpMsg->account) == 0)
	{
		LogAdd(LOG_RED,"[DGCashShopItemGifRecv] Invalid Account [%d](%s)",lpMsg->index,lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	lpObj->CashShopTransaction[1] = 0;

	PMSG_CASH_SHOP_ITEM_GIF_SEND pMsg;

	pMsg.header.set(0xD2,0x04,sizeof(pMsg));

	CASH_SHOP_PACKAGE_INFO* lpPackageInfo = this->GetPackageInfo(lpMsg->PackageMainIndex);

	if(lpPackageInfo == 0)
	{
		pMsg.result = 3;
		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	if(lpMsg->result != 0)
	{
		pMsg.result = 3;
		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	if(lpPackageInfo->CoinIndex != lpMsg->CoinIndex)
	{
		pMsg.result = 3;
		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	if(lpMsg->ProductMainIndex == 0)
	{
		DWORD WCoinC = ((lpPackageInfo->CoinIndex==508)?lpPackageInfo->CoinValue:0);

		DWORD WCoinP = ((lpPackageInfo->CoinIndex==509)?lpPackageInfo->CoinValue:0);

		DWORD GoblinPoint = ((lpPackageInfo->CoinIndex==0)?lpPackageInfo->CoinValue:0);

		if(WCoinC > lpMsg->WCoinC)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if(WCoinP > lpMsg->WCoinP)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if(GoblinPoint > lpMsg->GoblinPoint)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if((lpMsg->ItemCount+this->GetPackageProductBaseIndexCount(lpPackageInfo)) > (MAX_CASH_SHOP_PAGE*MAX_CASH_SHOP_PAGE_ITEM))
		{
			pMsg.result = 2;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		gLog.Output(LOG_CASH_SHOP,"[CashShopItemGif][PackageInfo][%s][%s] - (GiftAccount: %s, GiftName: %s, Category: %d, BaseIndex: %d, MainIndex: %d, ItemIndex: %d, CoinIndex: %d, CoinValue: %d, BonusGP: %d)",lpObj->Account,lpObj->Name,lpMsg->GiftAccount,lpMsg->GiftName,lpPackageInfo->Category,lpPackageInfo->BaseIndex,lpPackageInfo->MainIndex,lpPackageInfo->ItemIndex,lpPackageInfo->CoinIndex,lpPackageInfo->CoinValue,lpPackageInfo->BonusGP);

		for(int n=0;n < MAX_CASH_SHOP_PACKAGE_PRODUCT;n++)
		{
			if(lpPackageInfo->ProductBaseIndex[n] != 0)
			{
				CASH_SHOP_PRODUCT_INFO* lpProductInfo = ((lpPackageInfo->ProductMainIndex[n]==0)?this->GetProductInfoByBaseIndex(lpPackageInfo->ProductBaseIndex[n]):this->GetProductInfo(lpPackageInfo->ProductMainIndex[n]));

				/*
					A package can outlive the product it lists - delete the product
					in the admin tool and leave the package, and the lookup above
					returns 0. Every field below dereferences it, so without this
					the gift takes the GameServer down rather than failing.
				*/
				if(lpProductInfo == 0)
				{
					LogAddCat(LOG_CAT_CASHSHOP,LOG_RED,"[CashShop] package %d lists product %d/%d which is not in the catalog - skipped",lpPackageInfo->MainIndex,lpPackageInfo->ProductBaseIndex[n],lpPackageInfo->ProductMainIndex[n]);
					continue;
				}

				this->GDCashShopInsertItemSaveSend(lpObj->Index,lpMsg->GiftAccount,71,lpPackageInfo->MainIndex,lpProductInfo->BaseIndex,lpProductInfo->MainIndex,lpProductInfo->CoinValue,80,lpObj->Name,lpMsg->GiftText);
				gLog.Output(LOG_CASH_SHOP,"[CashShopItemGif][ProductInfo][%s][%s] - (BaseIndex: %d, MainIndex: %d, CoinValue: %d, ItemIndex: %d, ItemLevel: %d, ItemOption1: %d, ItemOption2: %d, ItemOption3: %d, ItemNewOption: %d, ItemSetOption: %d, ItemHarmonyOption: %d, ItemOptionEx: %d, ItemSocketOption: %d, %d, %d, %d, %d, ItemQuantity: %d, ItemDuration: %d)",lpObj->Account,lpObj->Name,lpProductInfo->BaseIndex,lpProductInfo->MainIndex,lpProductInfo->CoinValue,lpProductInfo->ItemIndex,lpProductInfo->ItemLevel,lpProductInfo->ItemOption1,lpProductInfo->ItemOption2,lpProductInfo->ItemOption3,lpProductInfo->ItemNewOption,lpProductInfo->ItemSetOption,lpProductInfo->ItemHarmonyOption,lpProductInfo->ItemOptionEx,lpProductInfo->ItemSocketOption1,lpProductInfo->ItemSocketOption2,lpProductInfo->ItemSocketOption3,lpProductInfo->ItemSocketOption4,lpProductInfo->ItemSocketOption5,lpProductInfo->ItemQuantity,lpProductInfo->ItemDuration);
			}
		}

		pMsg.result = 0;

		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);

		this->GDCashShopSubPointSaveSend(lpObj->Index,0,WCoinC,WCoinP,GoblinPoint,0,"DGCashShopItemGifRecv");

		if(lpPackageInfo->BonusGP > 0){this->GDCashShopAddPointSaveSend(lpObj->Index,0,0,0,lpPackageInfo->BonusGP,0);}

		GEMailMessageSend(lpObj->Name,lpMsg->GiftName,gMessage.GetMessage(276),2,ACTION_COLD1,lpObj->CharSet,lpMsg->GiftText);
	}
	else
	{
		CASH_SHOP_PRODUCT_INFO* lpProductInfo = this->GetProductInfo(lpMsg->ProductMainIndex);

		if(lpProductInfo == 0)
		{
			return;
		}

		DWORD WCoinC = ((lpPackageInfo->CoinIndex==508)?lpProductInfo->CoinValue:0);

		DWORD WCoinP = ((lpPackageInfo->CoinIndex==509)?lpProductInfo->CoinValue:0);

		DWORD GoblinPoint = ((lpPackageInfo->CoinIndex==0)?lpProductInfo->CoinValue:0);

		if(WCoinC > lpMsg->WCoinC)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if(WCoinP > lpMsg->WCoinP)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if(GoblinPoint > lpMsg->GoblinPoint)
		{
			pMsg.result = 1;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		if((lpMsg->ItemCount+1) > (MAX_CASH_SHOP_PAGE*MAX_CASH_SHOP_PAGE_ITEM))
		{
			pMsg.result = 2;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		gLog.Output(LOG_CASH_SHOP,"[CashShopItemGif][PackageInfo][%s][%s] - (TargetAccount: %s, TargetName: %s, Category: %d, BaseIndex: %d, MainIndex: %d, ItemIndex: %d, CoinIndex: %d, CoinValue: %d, BonusGP: %d)",lpObj->Account,lpObj->Name,lpMsg->GiftAccount,lpMsg->GiftName,lpPackageInfo->Category,lpPackageInfo->BaseIndex,lpPackageInfo->MainIndex,lpPackageInfo->ItemIndex,lpPackageInfo->CoinIndex,lpPackageInfo->CoinValue,lpPackageInfo->BonusGP);

		this->GDCashShopInsertItemSaveSend(lpObj->Index,lpMsg->GiftAccount,71,lpPackageInfo->MainIndex,lpProductInfo->BaseIndex,lpProductInfo->MainIndex,lpProductInfo->CoinValue,80,lpObj->Name,lpMsg->GiftText);

		gLog.Output(LOG_CASH_SHOP,"[CashShopItemGif][ProductInfo][%s][%s] - (BaseIndex: %d, MainIndex: %d, CoinValue: %d, ItemIndex: %d, ItemLevel: %d, ItemOption1: %d, ItemOption2: %d, ItemOption3: %d, ItemNewOption: %d, ItemSetOption: %d, ItemHarmonyOption: %d, ItemOptionEx: %d, ItemSocketOption: %d, %d, %d, %d, %d, ItemQuantity: %d, ItemDuration: %d)",lpObj->Account,lpObj->Name,lpProductInfo->BaseIndex,lpProductInfo->MainIndex,lpProductInfo->CoinValue,lpProductInfo->ItemIndex,lpProductInfo->ItemLevel,lpProductInfo->ItemOption1,lpProductInfo->ItemOption2,lpProductInfo->ItemOption3,lpProductInfo->ItemNewOption,lpProductInfo->ItemSetOption,lpProductInfo->ItemHarmonyOption,lpProductInfo->ItemOptionEx,lpProductInfo->ItemSocketOption1,lpProductInfo->ItemSocketOption2,lpProductInfo->ItemSocketOption3,lpProductInfo->ItemSocketOption4,lpProductInfo->ItemSocketOption5,lpProductInfo->ItemQuantity,lpProductInfo->ItemDuration);

		pMsg.result = 0;

		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);

		this->GDCashShopSubPointSaveSend(lpObj->Index,0,WCoinC,WCoinP,GoblinPoint,0,"DGCashShopItemGifRecv");

		if(lpPackageInfo->BonusGP > 0){this->GDCashShopAddPointSaveSend(lpObj->Index,0,0,0,lpPackageInfo->BonusGP,0);}

		GEMailMessageSend(lpObj->Name,lpMsg->GiftName,gMessage.GetMessage(276),2,ACTION_COLD1,lpObj->CharSet,lpMsg->GiftText);
	}

	#endif
}

void CCashShop::DGCashShopItemNumRecv(SDHP_CASH_SHOP_ITEM_NUM_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gObjIsAccountValid(lpMsg->index,lpMsg->account) == 0)
	{
		LogAdd(LOG_RED,"[DGCashShopItemNumRecv] Invalid Account [%d](%s)",lpMsg->index,lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	lpObj->CashShopTransaction[2] = 0;

	if(lpMsg->result == 0 && lpMsg->InventoryType == 71)
	{
		DWORD CashShopInventoryCurPage = lpMsg->InventoryPage;

		DWORD CashShopInventoryMaxPage = ((lpMsg->ItemCount==0)?0:(((lpMsg->ItemCount-1)/MAX_CASH_SHOP_PAGE_ITEM)+1));

		CashShopInventoryCurPage = ((CashShopInventoryCurPage==0&&CashShopInventoryMaxPage>0)?1:CashShopInventoryCurPage);

		CashShopInventoryCurPage = ((CashShopInventoryCurPage>CashShopInventoryMaxPage)?CashShopInventoryMaxPage:CashShopInventoryCurPage);

		PMSG_CASH_SHOP_ITEM_NUM_SEND pMsg;

		pMsg.header.set(0xD2,0x06,sizeof(pMsg));

		pMsg.ItemCount = (WORD)lpMsg->ItemCount;

		pMsg.PageCount = (WORD)lpMsg->PageCount;

		pMsg.CurPage = (WORD)CashShopInventoryCurPage;

		pMsg.MaxPage = (WORD)CashShopInventoryMaxPage;

		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);

		this->GCCashShopGiftSend(lpObj,lpMsg->PageCount,(CASH_SHOP_INVENTORY*)lpMsg->ProductInfo);
	}

	if(lpMsg->result == 0 && lpMsg->InventoryType == 83)
	{
		DWORD CashShopInventoryCurPage = lpMsg->InventoryPage;

		DWORD CashShopInventoryMaxPage = ((lpMsg->ItemCount==0)?0:(((lpMsg->ItemCount-1)/MAX_CASH_SHOP_PAGE_ITEM)+1));

		CashShopInventoryCurPage = ((CashShopInventoryCurPage==0&&CashShopInventoryMaxPage>0)?1:CashShopInventoryCurPage);

		CashShopInventoryCurPage = ((CashShopInventoryCurPage>CashShopInventoryMaxPage)?CashShopInventoryMaxPage:CashShopInventoryCurPage);

		PMSG_CASH_SHOP_ITEM_NUM_SEND pMsg;

		pMsg.header.set(0xD2,0x06,sizeof(pMsg));

		pMsg.ItemCount = (WORD)lpMsg->ItemCount;

		pMsg.PageCount = (WORD)lpMsg->PageCount;

		pMsg.CurPage = (WORD)CashShopInventoryCurPage;

		pMsg.MaxPage = (WORD)CashShopInventoryMaxPage;

		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);

		this->GCCashShopItemSend(lpObj,lpMsg->PageCount,(CASH_SHOP_INVENTORY*)lpMsg->ProductInfo);
	}

	#endif
}

void CCashShop::DGCashShopItemUseRecv(SDHP_CASH_SHOP_ITEM_USE_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gObjIsAccountValid(lpMsg->index,lpMsg->account) == 0)
	{
		LogAdd(LOG_RED,"[DGCashShopItemUseRecv] Invalid Account [%d](%s)",lpMsg->index,lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	lpObj->CashShopTransaction[3] = 0;

	PMSG_CASH_SHOP_ITEM_USE_SEND pMsg;

	pMsg.header.set(0xD2,0x0B,sizeof(pMsg));

	if(lpMsg->result != 0)
	{
		pMsg.result = 1;
		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	CASH_SHOP_PRODUCT_INFO* lpProductInfo = this->GetProductInfo(lpMsg->ProductInfo.ProductMainIndex);

	if(lpProductInfo == 0)
	{
		pMsg.result = 1;
		DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	EFFECT_INFO* lpInfo = gEffectManager.GetInfoByItem(lpProductInfo->ItemIndex);

	if(lpInfo == 0)
	{
		if(lpProductInfo->ItemDuration == 0)
		{
			if(lpProductInfo->ItemIndex == GET_ITEM(14,91) || lpProductInfo->ItemIndex == GET_ITEM(14,97) || lpProductInfo->ItemIndex == GET_ITEM(14,98) || lpProductInfo->ItemIndex == GET_ITEM(14,114) || lpProductInfo->ItemIndex == GET_ITEM(14,117) || lpProductInfo->ItemIndex == GET_ITEM(14,162) || lpProductInfo->ItemIndex == GET_ITEM(14,163) || lpProductInfo->ItemIndex == GET_ITEM(14,169))
			{
				CItem item;

				item.m_Index = lpProductInfo->ItemIndex;

				switch(item.m_Index)
				{
					case GET_ITEM(14,91):
						if(gObjectManager.CharacterUseCreationCard(lpObj,&item) == 0)
						{
							pMsg.result = 1;
							DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
							return;
						}
						break;
					case GET_ITEM(14,97):
						if(gObjectManager.CharacterUseCreationCard(lpObj,&item) == 0)
						{
							pMsg.result = 1;
							DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
							return;
						}
						break;
					case GET_ITEM(14,98):
						if(gObjectManager.CharacterUseCreationCard(lpObj,&item) == 0)
						{
							pMsg.result = 1;
							DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
							return;
						}
						break;
					case GET_ITEM(14,114):
						if(gObjRebuildMasterSkillTree(lpObj) == 0)
						{
							pMsg.result = 1;
							DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
							return;
						}
						break;
					case GET_ITEM(14,117):
							lpObj->RenameEnable = 1;
						break;
					case GET_ITEM(14,162):
						if(gObjectManager.CharacterUseInventoryExpansion(lpObj,&item) == 0)
						{
							pMsg.result = 1;
							DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
							return;
						}
						break;
					case GET_ITEM(14,163):
						if(gObjectManager.CharacterUseWarehouseExpansion(lpObj,&item) == 0)
						{
							pMsg.result = 1;
							DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
							return;
						}
						break;
					case GET_ITEM(14,169):
						if(gObjectManager.CharacterUseCreationCard(lpObj,&item) == 0)
						{
							pMsg.result = 1;
							DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
							return;
						}
						break;
				}
			}
			else
			{
				if(gItemManager.CheckItemInventorySpace(lpObj,lpProductInfo->ItemIndex) == 0)
				{
					pMsg.result = 1;
					DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
					return;
				}

				CItem item;

				item.m_Index = lpProductInfo->ItemIndex;

				item.m_SocketOption[0] = lpProductInfo->ItemSocketOption1;

				item.m_SocketOption[1] = lpProductInfo->ItemSocketOption2;

				item.m_SocketOption[2] = lpProductInfo->ItemSocketOption3;

				item.m_SocketOption[3] = lpProductInfo->ItemSocketOption4;

				item.m_SocketOption[4] = lpProductInfo->ItemSocketOption5;

				item.m_SocketOptionBonus = gSocketItemOption.GetSocketItemBonusOption(&item);

				GDCreateItemSend(lpObj->Index,0xEB,0,0,lpProductInfo->ItemIndex,lpProductInfo->ItemLevel,lpProductInfo->ItemQuantity,lpProductInfo->ItemOption1,lpProductInfo->ItemOption2,lpProductInfo->ItemOption3,-1,lpProductInfo->ItemNewOption,lpProductInfo->ItemSetOption,lpProductInfo->ItemHarmonyOption,lpProductInfo->ItemOptionEx,item.m_SocketOption,item.m_SocketOptionBonus,((lpProductInfo->ItemDuration>0)?((DWORD)time(0)+lpProductInfo->ItemDuration):0));
			}
		}
		else
		{
			if(lpProductInfo->ItemIndex == GET_ITEM(13,124))
			{
				switch(lpProductInfo->ItemIndex)
				{
					case GET_ITEM(13,124):
						GJAccountLevelSaveSend(lpObj->Index,lpProductInfo->ItemLevel,lpProductInfo->ItemDuration);
						GJAccountLevelSend(lpObj->Index);
						break;
				}
			}
			else
			{
				if(gItemManager.CheckItemInventorySpace(lpObj,lpProductInfo->ItemIndex) == 0)
				{
					pMsg.result = 1;
					DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
					return;
				}

				CItem item;

				item.m_Index = lpProductInfo->ItemIndex;

				item.m_SocketOption[0] = lpProductInfo->ItemSocketOption1;

				item.m_SocketOption[1] = lpProductInfo->ItemSocketOption2;

				item.m_SocketOption[2] = lpProductInfo->ItemSocketOption3;

				item.m_SocketOption[3] = lpProductInfo->ItemSocketOption4;

				item.m_SocketOption[4] = lpProductInfo->ItemSocketOption5;

				item.m_SocketOptionBonus = gSocketItemOption.GetSocketItemBonusOption(&item);

				GDCreateItemSend(lpObj->Index,0xEB,0,0,lpProductInfo->ItemIndex,lpProductInfo->ItemLevel,lpProductInfo->ItemQuantity,lpProductInfo->ItemOption1,lpProductInfo->ItemOption2,lpProductInfo->ItemOption3,-1,lpProductInfo->ItemNewOption,lpProductInfo->ItemSetOption,lpProductInfo->ItemHarmonyOption,lpProductInfo->ItemOptionEx,item.m_SocketOption,item.m_SocketOptionBonus,((lpProductInfo->ItemDuration>0)?((DWORD)time(0)+lpProductInfo->ItemDuration):0));
			}
		}
	}
	else
	{
		if(gEffectManager.GetEffect(lpObj,lpInfo->Index) != 0)
		{
			pMsg.result = 3;
			DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}

		gEffectManager.AddEffect(lpObj,1,lpInfo->Index,(int)(time(0)+lpProductInfo->ItemDuration),0,0,0,0);
	}

	gLog.Output(LOG_CASH_SHOP,"[CashShopItemUse][ProductInfo][%s][%s] - (BaseIndex: %d, MainIndex: %d, CoinValue: %d, ItemIndex: %d, ItemLevel: %d, ItemOption1: %d, ItemOption2: %d, ItemOption3: %d, ItemNewOption: %d, ItemSetOption: %d, ItemHarmonyOption: %d, ItemOptionEx: %d, ItemSocketOption: %d, %d, %d, %d, %d, ItemQuantity: %d, ItemDuration: %d)",lpObj->Account,lpObj->Name,lpProductInfo->BaseIndex,lpProductInfo->MainIndex,lpProductInfo->CoinValue,lpProductInfo->ItemIndex,lpProductInfo->ItemLevel,lpProductInfo->ItemOption1,lpProductInfo->ItemOption2,lpProductInfo->ItemOption3,lpProductInfo->ItemNewOption,lpProductInfo->ItemSetOption,lpProductInfo->ItemHarmonyOption,lpProductInfo->ItemOptionEx,lpProductInfo->ItemSocketOption1,lpProductInfo->ItemSocketOption2,lpProductInfo->ItemSocketOption3,lpProductInfo->ItemSocketOption4,lpProductInfo->ItemSocketOption5,lpProductInfo->ItemQuantity,lpProductInfo->ItemDuration);

	pMsg.result = 0;

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);

	this->GDCashShopDeleteItemSaveSend(lpObj->Index,0,lpMsg->BaseItemCode,lpMsg->MainItemCode);

	#endif
}

void CCashShop::DGCashShopPeriodicItemRecv(SDHP_CASH_SHOP_PERIODIC_ITEM_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gObjIsAccountValid(lpMsg->index,lpMsg->account) == 0)
	{
		LogAdd(LOG_RED,"[DGCashShopPeriodicItemRecv] Invalid Account [%d](%s)",lpMsg->index,lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	for(int n=0;n < lpMsg->count;n++)
	{
		SDHP_CASH_SHOP_PERIODIC_ITEM1* lpInfo = (SDHP_CASH_SHOP_PERIODIC_ITEM1*)(((BYTE*)lpMsg)+sizeof(SDHP_CASH_SHOP_PERIODIC_ITEM_RECV)+(sizeof(SDHP_CASH_SHOP_PERIODIC_ITEM1)*n));

		if(INVENTORY_RANGE(lpInfo->slot) == 0)
		{
			continue;
		}

		if(lpObj->Inventory[lpInfo->slot].IsItem() == 0)
		{
			continue;
		}

		if(lpObj->Inventory[lpInfo->slot].m_Serial != lpInfo->serial)
		{
			continue;
		}

		if(lpObj->Inventory[lpInfo->slot].m_IsPeriodicItem == 0)
		{
			continue;
		}

		if(lpObj->Inventory[lpInfo->slot].m_LoadPeriodicItem != 0)
		{
			continue;
		}

		lpObj->Inventory[lpInfo->slot].m_LoadPeriodicItem = 1;

		lpObj->Inventory[lpInfo->slot].m_PeriodicItemTime = lpInfo->time;

		this->GCCashShopPeriodicItemSend(lpObj->Index,lpObj->Inventory[lpInfo->slot].m_Index,lpInfo->slot,lpObj->Inventory[lpInfo->slot].m_PeriodicItemTime);
	}

	#endif
}

void CCashShop::DGCashShopRecievePointRecv(SDHP_CASH_SHOP_RECIEVE_POINT_RECV* lpMsg) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gObjIsAccountValid(lpMsg->index,lpMsg->account) == 0)
	{
		LogAdd(LOG_RED,"[DGCashShopRecievePointRecv] Invalid Account [%d](%s)",lpMsg->index,lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];
#if (FIXBUGCOIN)
	lpObj->CashShopTransaction[0] = 0;
#endif
	((void(*)(LPOBJ,DWORD,DWORD,DWORD,DWORD,DWORD))lpMsg->CallbackFunc)(lpObj,lpMsg->CallbackArg1,lpMsg->CallbackArg2,lpMsg->WCoinC,lpMsg->WCoinP,lpMsg->GoblinPoint);

	#endif
}

void CCashShop::GDCashShopPeriodicItemSend(int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gObjIsAccountValid(aIndex,gObj[aIndex].Account) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	BYTE send[4096];

	SDHP_CASH_SHOP_PERIODIC_ITEM_SEND pMsg;

	pMsg.header.set(0x18,0x05,0);

	int size = sizeof(pMsg);

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	pMsg.count = 0;

	SDHP_CASH_SHOP_PERIODIC_ITEM2 info;

	for(int n=0;n < INVENTORY_SIZE;n++)
	{
		if(lpObj->Inventory[n].IsItem() == 0)
		{
			continue;
		}

		if(lpObj->Inventory[n].m_IsPeriodicItem == 0)
		{
			continue;
		}

		info.slot = n;

		info.serial = lpObj->Inventory[n].m_Serial;

		memcpy(&send[size],&info,sizeof(info));
		size += sizeof(info);

		pMsg.count++;
	}

	if(pMsg.count > 0)
	{
		pMsg.header.size[0] = SET_NUMBERHB(size);
		pMsg.header.size[1] = SET_NUMBERLB(size);

		memcpy(send,&pMsg,sizeof(pMsg));

		gDataServerConnection.DataSend(send,size);
	}

	#endif
}

void CCashShop::GDCashShopRecievePointSend(int aIndex,DWORD CallbackFunc,DWORD CallbackArg1,DWORD CallbackArg2) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	if(gObjIsAccountValid(aIndex,gObj[aIndex].Account) == 0)
	{
		return;
	}
#if (FIXBUGCOIN)
	if (((gObj[aIndex].CashShopTransaction[0] == 0) ? (gObj[aIndex].CashShopTransaction[0]++) : gObj[aIndex].CashShopTransaction[0]) != 0)
	{
		return;
	}
#endif
	LPOBJ lpObj = &gObj[aIndex];

	SDHP_CASH_SHOP_RECIEVE_POINT_SEND pMsg;

	pMsg.header.set(0x18,0x06,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	pMsg.CallbackFunc = CallbackFunc;

	pMsg.CallbackArg1 = CallbackArg1;

	pMsg.CallbackArg2 = CallbackArg2;

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);

	#endif
}

void CCashShop::GDCashShopAddPointSaveSend(int aIndex,char* GiftAccount,DWORD AddWCoinC,DWORD AddWCoinP,DWORD AddGoblinPoint,DWORD AddRuud, char* NameLog) // OK
{

	if (CheckCoinBBT(gObj[aIndex].Coin1 + AddWCoinC) || CheckCoinBBT(gObj[aIndex].Coin2 + AddWCoinP) || CheckCoinBBT(gObj[aIndex].Coin3 + AddGoblinPoint) || CheckCoinBBT(gObj[aIndex].Ruud + AddRuud))
	{
		//	LogAdd(LOG_RED, "[MaxCoin] GDCashShopAddPointSaveSend -> %d,%d,%d,%d", AddWCoinC, AddWCoinP, AddGoblinPoint, AddRuud);

		LogAdd(LOG_RED, "[MaxCoin] [%s] GDCashShopAddPointSaveSend -> %d,%d,%d,%d Log: %s", gObj[aIndex].Name, AddWCoinC, AddWCoinP, AddGoblinPoint, AddRuud, NameLog);
		gLog.Output(LOG_DEBUG, "[MaxCoin] [%s] GDCashShopAddPointSaveSend -> %d,%d,%d,%d Log: %s", gObj[aIndex].Name, AddWCoinC, AddWCoinP, AddGoblinPoint, AddRuud, NameLog);
		return;
	}

	#if(GAMESERVER_UPDATE>=501)

	SDHP_CASH_SHOP_ADD_POINT_SAVE_SEND pMsg;

	pMsg.header.set(0x18,0x30,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,gObj[aIndex].Account,sizeof(pMsg.account));

	if(GiftAccount == 0)
	{
		memset(pMsg.GiftAccount,0,sizeof(pMsg.GiftAccount));
	}
	else
	{
		memcpy(pMsg.GiftAccount,GiftAccount,sizeof(pMsg.GiftAccount));
	}

	pMsg.AddWCoinC = AddWCoinC;

	pMsg.AddWCoinP = AddWCoinP;

	pMsg.AddGoblinPoint = AddGoblinPoint;

	pMsg.AddRuud = AddRuud;

	gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

void CCashShop::GDCashShopSubPointSaveSend(int aIndex,char* GiftAccount,DWORD SubWCoinC,DWORD SubWCoinP,DWORD SubGoblinPoint, DWORD SubRuud, char* NameLog) // OK
{
	if (CheckCoinBBT(gObj[aIndex].Coin1 - SubWCoinC) || CheckCoinBBT(gObj[aIndex].Coin2 - SubWCoinP) || CheckCoinBBT(gObj[aIndex].Coin3 - SubGoblinPoint) || CheckCoinBBT(gObj[aIndex].Ruud - SubRuud))
	{
		LogAdd(LOG_RED, "[MaxCoin] [%s] GDCashShopSubPointSaveSend -> %d,%d,%d,%d Log: %s", gObj[aIndex].Name, SubWCoinC, SubWCoinP, SubGoblinPoint, SubRuud, NameLog);
		gLog.Output(LOG_DEBUG, "[MaxCoin] [%s] GDCashShopSubPointSaveSend -> %d,%d,%d,%d Log: %s", gObj[aIndex].Name, SubWCoinC, SubWCoinP, SubGoblinPoint, SubRuud, NameLog);
		return;
	}
	#if(GAMESERVER_UPDATE>=501)

	SDHP_CASH_SHOP_SUB_POINT_SAVE_SEND pMsg;

	pMsg.header.set(0x18,0x31,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,gObj[aIndex].Account,sizeof(pMsg.account));

	if(GiftAccount == 0)
	{
		memset(pMsg.GiftAccount,0,sizeof(pMsg.GiftAccount));
	}
	else
	{
		memcpy(pMsg.GiftAccount,GiftAccount,sizeof(pMsg.GiftAccount));
	}

	pMsg.SubWCoinC = SubWCoinC;

	pMsg.SubWCoinP = SubWCoinP;

	pMsg.SubGoblinPoint = SubGoblinPoint;

	pMsg.SubRuud = SubRuud;

	gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

void CCashShop::GDCashShopInsertItemSaveSend(int aIndex,char* GiftAccount,BYTE InventoryType,DWORD PackageMainIndex,DWORD ProductBaseIndex,DWORD ProductMainIndex,double CoinValue,BYTE ProductType,char* GiftName,char* GiftText) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	SDHP_CASH_SHOP_INSERT_ITEM_SAVE_SEND pMsg;

	pMsg.header.set(0x18,0x32,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,gObj[aIndex].Account,sizeof(pMsg.account));

	if(GiftAccount == 0)
	{
		memset(pMsg.GiftAccount,0,sizeof(pMsg.GiftAccount));
	}
	else
	{
		memcpy(pMsg.GiftAccount,GiftAccount,sizeof(pMsg.GiftAccount));
	}

	pMsg.InventoryType = InventoryType;

	pMsg.PackageMainIndex = PackageMainIndex;

	pMsg.ProductBaseIndex = ProductBaseIndex;

	pMsg.ProductMainIndex = ProductMainIndex;

	pMsg.CoinValue = CoinValue;

	pMsg.ProductType = ProductType;

	if(GiftName == 0)
	{
		memset(pMsg.GiftName,0,sizeof(pMsg.GiftName));
	}
	else
	{
		memcpy(pMsg.GiftName,GiftName,sizeof(pMsg.GiftName));
	}

	if(GiftText == 0)
	{
		memset(pMsg.GiftText,0,sizeof(pMsg.GiftText));
	}
	else
	{
		memcpy(pMsg.GiftText,GiftText,sizeof(pMsg.GiftText));
	}

	gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

void CCashShop::GDCashShopDeleteItemSaveSend(int aIndex,char* GiftAccount,DWORD BaseItemCode,DWORD MainItemCode) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	SDHP_CASH_SHOP_DELETE_ITEM_SAVE_SEND pMsg;

	pMsg.header.set(0x18,0x33,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,gObj[aIndex].Account,sizeof(pMsg.account));

	if(GiftAccount == 0)
	{
		memset(pMsg.GiftAccount,0,sizeof(pMsg.GiftAccount));
	}
	else
	{
		memcpy(pMsg.GiftAccount,GiftAccount,sizeof(pMsg.GiftAccount));
	}

	pMsg.BaseItemCode = BaseItemCode;

	pMsg.MainItemCode = MainItemCode;

	gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

void CCashShop::GDCashShopPeriodicItemSaveSend(int aIndex) // OK
{
	#if(GAMESERVER_UPDATE>=501)

	LPOBJ lpObj = &gObj[aIndex];

	BYTE send[4096];

	SDHP_CASH_SHOP_PERIODIC_ITEM_SAVE_SEND pMsg;

	pMsg.header.set(0x18,0x34,0);

	int size = sizeof(pMsg);

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	pMsg.count = 0;

	SDHP_CASH_SHOP_PERIODIC_ITEM_SAVE info;

	for(int n=0;n < INVENTORY_SIZE;n++)
	{
		if(lpObj->Inventory[n].IsItem() == 0)
		{
			continue;
		}

		if(lpObj->Inventory[n].m_IsPeriodicItem == 0)
		{
			continue;
		}

		if(lpObj->Inventory[n].m_LoadPeriodicItem == 0)
		{
			continue;
		}

		info.serial = lpObj->Inventory[n].m_Serial;

		info.time = lpObj->Inventory[n].m_PeriodicItemTime;

		memcpy(&send[size],&info,sizeof(info));
		size += sizeof(info);

		pMsg.count++;
	}

	if(pMsg.count > 0)
	{
		pMsg.header.size[0] = SET_NUMBERHB(size);
		pMsg.header.size[1] = SET_NUMBERLB(size);

		memcpy(send,&pMsg,sizeof(pMsg));

		gDataServerConnection.DataSend(send,size);
	}

	#endif
}

void CCashShop::GCSendCoin(LPOBJ lpObj, int coin1, int coin2, int coin3) // OK
{
#if (GAMESERVER_CLIENTE_UPDATE >= 9)
	if (OBJECT_RANGE(lpObj->Index) == 0)
	{
		return;
	}

	PMSG_COIN_SEND pMsg;

	pMsg.header.set(0xF3,0xEA,sizeof(pMsg));

	pMsg.Coin1 = lpObj->Coin1;

	pMsg.Coin2 = lpObj->Coin2;

	pMsg.Coin3 = lpObj->Coin3;

	pMsg.Ruud = lpObj->Ruud;

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
#endif

}