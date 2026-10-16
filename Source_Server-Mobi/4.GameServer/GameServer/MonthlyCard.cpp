// MonthlyCard.cpp: Monthly WCoin (Kira Pass), GameServer side.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MonthlyCard.h"
#include "DSProtocol.h"
#include "GameMain.h"
#include "Notice.h"
#include "User.h"
#include "Util.h"

CMonthlyCard gMonthlyCard;

void CMonthlyCard::SendRequest(int aIndex,BYTE mode)
{
	if(OBJECT_RANGE(aIndex) == 0 || gObj[aIndex].Type != OBJECT_USER)
	{
		return;
	}

	SDHP_MONTHLYCARD_REQ pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x50,sizeof(pMsg));
	pMsg.aIndex = (WORD)aIndex;
	memcpy(pMsg.Account,gObj[aIndex].Account,sizeof(pMsg.Account) - 1);
	memcpy(pMsg.Name,gObj[aIndex].Name,sizeof(pMsg.Name) - 1);
	pMsg.Mode = mode;

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CMonthlyCard::OnCharacterEnter(int aIndex)
{
	// Off-helper / fake-online characters don't collect: the reward needs a
	// real login that day.
	if(gObjIsConnectedGP(aIndex) == 0 || gObj[aIndex].IsFakeOnline != 0 || gObj[aIndex].m_OfflineMode != 0)
	{
		return;
	}

	this->SendRequest(aIndex,0);
}

void CMonthlyCard::RequestStatus(int aIndex)
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	this->SendRequest(aIndex,1);
}

void CMonthlyCard::DGResultRecv(SDHP_MONTHLYCARD_RESULT* lpMsg)
{
	int aIndex = lpMsg->aIndex;

	if(OBJECT_RANGE(aIndex) == 0 || gObjIsAccountValid(aIndex,lpMsg->Account) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	lpObj->MonthlyCardDaysLeft = (int)lpMsg->DaysLeft;
	lpObj->MonthlyCardPaidToday = lpMsg->PaidToday;

	PMSG_MONTHLYCARD_STATUS_SEND pStatus;
	pStatus.header.set(0xD3,0xF4,sizeof(pStatus));
	pStatus.DaysLeft = lpMsg->DaysLeft;
	pStatus.PaidToday = lpMsg->PaidToday;
	pStatus.Days = lpMsg->Days;
	pStatus.Instant = lpMsg->Instant;
	pStatus.Daily = lpMsg->Daily;
	// Pass holders see the mailbox at each login. LastServerCode is -1 after a
	// normal login and the previous server after a server switch (Crywolf,
	// Castle Siege), so warping back and forth doesn't pop it again.
	pStatus.OpenMailbox = (lpMsg->Mode == 0 && lpMsg->DaysLeft > 0 && lpObj->LastServerCode == -1) ? 1 : 0;
	DataSend(aIndex,(BYTE*)&pStatus,pStatus.header.size);

	if(lpMsg->Amount > 0)
	{
		gNotice.GCNoticeSend(aIndex,1,0,0,0,0,0,"[Kira Pass] %d WCoin (C) sent to your mailbox. %d day(s) left.",lpMsg->Amount,lpMsg->DaysLeft);
	}
}
