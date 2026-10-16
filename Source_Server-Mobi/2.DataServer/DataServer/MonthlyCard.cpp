// MonthlyCard.cpp: Monthly WCoin (Kira Pass), DataServer side.
//
// Account and character names reach SQL as bound parameters, never through
// the query text (ExecQuery formats with plain sprintf, QueryManager.cpp).
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "DataServerProtocol.h"
#include "MonthlyCard.h"
#include "FriendMail.h"
#include "FriendMailProtocol.h"
#include "QueryManager.h"
#include "SocketManager.h"
#include "Util.h"

CMonthlyCard gMonthlyCard;

namespace
{
	const char* kMonthlyCardConfig = ".\\PlayBilling.ini";

	void CopyText(char* out,int outSize,const char* in,int inSize)
	{
		memset(out,0,outSize);
		int n = 0;
		while(n < inSize && n < outSize - 1 && in[n] != 0)
		{
			out[n] = in[n];
			n++;
		}
	}
}

void CMonthlyCard::OnRequest(SDHP_MONTHLYCARD_REQ* lpMsg,int index)
{
	char account[11];
	char name[11];
	CopyText(account,sizeof(account),lpMsg->Account,10);
	CopyText(name,sizeof(name),lpMsg->Name,10);

	SDHP_MONTHLYCARD_RESULT pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x50,sizeof(pMsg));
	pMsg.aIndex = lpMsg->aIndex;
	memcpy(pMsg.Account,account,sizeof(pMsg.Account));
	pMsg.Mode = lpMsg->Mode;
	pMsg.Days = (WORD)GetPrivateProfileIntA("MonthlyCard","Days",30,kMonthlyCardConfig);
	pMsg.Instant = (WORD)GetPrivateProfileIntA("MonthlyCard","Instant",50,kMonthlyCardConfig);
	pMsg.Daily = (WORD)GetPrivateProfileIntA("MonthlyCard","Daily",25,kMonthlyCardConfig);

	if(account[0] == 0)
	{
		gSocketManager.DataSend(index,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	// Character entered: take what is owed (instant reward + today's daily)
	// and mail it to this character. TakeToday marks it taken in the same
	// transaction, so two logins at once can't both collect.
	if(lpMsg->Mode == 0 && name[0] != 0)
	{
		const int daily = GetPrivateProfileIntA("MonthlyCard","Daily",25,kMonthlyCardConfig);
		const int expireDays = GetPrivateProfileIntA("MonthlyCard","MailExpireDays",14,kMonthlyCardConfig);

		char sender[11] = {0};
		GetPrivateProfileStringA("MonthlyCard","Sender","Kira Pass",sender,sizeof(sender),kMonthlyCardConfig);

		int amount = 0, instant = 0, paidDaily = 0, daysLeft = 0;

		gQueryManager.BindParameterAsString(1,account,sizeof(account));

		if(gQueryManager.ExecQuery("EXEC WZ_MonthlyCard_TakeToday ?,%d",daily) != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
		{
			amount = gQueryManager.GetAsInteger("Amount");
			instant = gQueryManager.GetAsInteger("Instant");
			paidDaily = gQueryManager.GetAsInteger("Daily");
			daysLeft = gQueryManager.GetAsInteger("DaysLeft");
		}

		gQueryManager.Close();

		if(amount > 0)
		{
			char memo[256];

			if(instant > 0 && paidDaily > 0)
				_snprintf_s(memo,sizeof(memo),_TRUNCATE,"Thank you for your Kira Pass! %d WCoin (C) for your purchase and %d for today. %d day(s) left. Claim it within %d days.",instant,paidDaily,daysLeft,expireDays);
			else if(instant > 0)
				_snprintf_s(memo,sizeof(memo),_TRUNCATE,"Thank you for your Kira Pass! %d WCoin (C) for your purchase. %d day(s) left. Claim it within %d days.",instant,daysLeft,expireDays);
			else
				_snprintf_s(memo,sizeof(memo),_TRUNCATE,"Your daily Kira Pass reward: %d WCoin (C). %d day(s) left. Log in every day to collect. Claim it within %d days.",paidDaily,daysLeft,expireDays);

			if(gFriendMail.MailCoins(name,sender,"Kira Pass reward",memo,FRIENDMAIL_COIN_WCOINC,amount,expireDays) != 0)
			{
				pMsg.Amount = (DWORD)amount;
				LogAdd(LOG_GREEN,"[MonthlyCard] %s (%s): mailed %d WCoin (C) (instant %d, daily %d), %d day(s) left",account,name,amount,instant,paidDaily,daysLeft);
			}
			else
			{
				// Mailbox full or no friend list yet: keep it for the next login.
				gQueryManager.BindParameterAsString(1,account,sizeof(account));
				gQueryManager.ExecQuery("EXEC WZ_MonthlyCard_Refund ?,%d",amount);
				gQueryManager.Close();
				LogAdd(LOG_RED,"[MonthlyCard] %s (%s): could not mail %d WCoin (C), kept for the next login",account,name,amount);
			}
		}
	}

	gQueryManager.BindParameterAsString(1,account,sizeof(account));

	if(gQueryManager.ExecQuery("EXEC WZ_MonthlyCard_Status ?") != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		int daysLeft = gQueryManager.GetAsInteger("DaysLeft");
		pMsg.DaysLeft = (daysLeft > 0) ? (DWORD)daysLeft : 0;
		pMsg.PaidToday = (gQueryManager.GetAsInteger("PaidToday") == 1) ? 1 : 0;
	}

	gQueryManager.Close();

	gSocketManager.DataSend(index,(BYTE*)&pMsg,pMsg.header.size);
}

int CMonthlyCard::Grant(const char* account,const char* source,const char* ref)
{
	const int days = GetPrivateProfileIntA("MonthlyCard","Days",30,kMonthlyCardConfig);
	const int instant = GetPrivateProfileIntA("MonthlyCard","Instant",50,kMonthlyCardConfig);

	char acc[11];
	char src[17];
	char reference[201];
	CopyText(acc,sizeof(acc),account,10);
	CopyText(src,sizeof(src),source,16);
	CopyText(reference,sizeof(reference),ref,200);

	int result = -1;

	gQueryManager.BindParameterAsString(1,acc,sizeof(acc));
	gQueryManager.BindParameterAsString(2,src,sizeof(src));
	gQueryManager.BindParameterAsString(3,reference,sizeof(reference));

	if(gQueryManager.ExecQuery("EXEC WZ_MonthlyCard_Grant ?,?,?,%d,%d",days,instant) != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		result = gQueryManager.GetAsInteger("Result");
	}

	gQueryManager.Close();

	LogAdd((result == 0) ? LOG_GREEN : LOG_BLACK,"[MonthlyCard] %s: %s purchase %s - %s",acc,src,
		(result == 0) ? "granted (+30 days)" : ((result == 1) ? "already granted" : "FAILED"),reference);

	return result;
}
