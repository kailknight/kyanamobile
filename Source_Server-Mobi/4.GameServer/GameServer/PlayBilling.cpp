// PlayBilling.cpp: Google Play WCoin (C) recharge, GameServer side.
// See PlayBilling.h.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PlayBilling.h"
#include "CashShop.h"
#include "GameMain.h"
#include "Util.h"

CPlayBilling gPlayBilling;

namespace
{
	enum
	{
		PLAY_RESULT_CREDITED = 0,
		PLAY_RESULT_INVALID = 2,
		PLAY_RESULT_RETRY = 4,
	};

	// One request every couple of seconds per player is plenty: the phone only
	// sends on a finished payment or when the shop reopens with one pending.
	DWORD g_LastRequestTick[MAX_OBJECT];
}

void CPlayBilling::CGPurchaseRecv(PMSG_PLAY_PURCHASE_RECV* lpMsg,int aIndex,int size)
{
	if(OBJECT_RANGE(aIndex) == 0 || gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	if(size < (int)sizeof(PMSG_PLAY_PURCHASE_RECV))
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	lpMsg->ProductId[sizeof(lpMsg->ProductId) - 1] = 0;
	lpMsg->Token[sizeof(lpMsg->Token) - 1] = 0;

	if(GetTickCount() - g_LastRequestTick[aIndex] < 2000)
	{
		this->SendClientResult(aIndex,PLAY_RESULT_RETRY,0,lpMsg->ProductId);
		return;
	}

	g_LastRequestTick[aIndex] = GetTickCount();

	if(lpMsg->ProductId[0] == 0 || lpMsg->Token[0] == 0)
	{
		this->SendClientResult(aIndex,PLAY_RESULT_INVALID,0,lpMsg->ProductId);
		return;
	}

	SDHP_PLAY_PURCHASE_SEND pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x37,sizeof(pMsg));
	pMsg.aIndex = (WORD)aIndex;
	strncpy_s(pMsg.AccountID,sizeof(pMsg.AccountID),lpObj->Account,_TRUNCATE);
	strncpy_s(pMsg.ProductId,sizeof(pMsg.ProductId),lpMsg->ProductId,_TRUNCATE);
	strncpy_s(pMsg.Token,sizeof(pMsg.Token),lpMsg->Token,_TRUNCATE);

	gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));
}

void CPlayBilling::DGPurchaseRecv(SDHP_PLAY_PURCHASE_RECV* lpMsg)
{
	int aIndex = lpMsg->aIndex;

	lpMsg->AccountID[sizeof(lpMsg->AccountID) - 1] = 0;
	lpMsg->ProductId[sizeof(lpMsg->ProductId) - 1] = 0;

	// The player may have logged out, or the slot gone to someone else, while
	// Google was being asked. The WCoin is already on the account either way.
	if(OBJECT_RANGE(aIndex) == 0 || gObjIsAccountValid(aIndex,lpMsg->AccountID) == 0)
	{
		return;
	}

	if(lpMsg->Result == PLAY_RESULT_CREDITED)
	{
		LogAdd(LOG_GREEN,"[PlayBilling] [%s][%s] +%d WCoin (C) from Google Play (%s)",gObj[aIndex].Account,gObj[aIndex].Name,lpMsg->WCoinC,lpMsg->ProductId);

		// Re-read the balance the DataServer just changed, so the shop shows it.
		gCashShop.CGCashShopPointRecv(aIndex);
	}

	this->SendClientResult(aIndex,lpMsg->Result,lpMsg->WCoinC,lpMsg->ProductId);
}

void CPlayBilling::SendClientResult(int aIndex,BYTE result,DWORD wcoin,const char* productId)
{
	PMSG_PLAY_PURCHASE_SEND pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD3,0xF1,sizeof(pMsg));
	pMsg.Result = result;
	pMsg.WCoinC = wcoin;

	if(productId != NULL)
	{
		strncpy_s(pMsg.ProductId,sizeof(pMsg.ProductId),productId,_TRUNCATE);
	}

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}
