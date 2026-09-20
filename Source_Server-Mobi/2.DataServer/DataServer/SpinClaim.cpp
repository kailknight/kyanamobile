// SpinClaim.cpp: implementation of the CSpinClaim class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SpinClaim.h"
#include "QueryManager.h"
#include "SocketManager.h"
#include "Util.h"

CSpinClaim gSpinClaim;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSpinClaim::CSpinClaim() // OK
{

}

CSpinClaim::~CSpinClaim() // OK
{

}

void CSpinClaim::GDSpinClaimRecv(SDHP_SPINCLAIM_RECV* lpMsg,int index) // OK
{
	SDHP_SPINCLAIM_SEND pMsg;

	pMsg.header.set(0xD9,0x32,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	memcpy(pMsg.name,lpMsg->name,sizeof(pMsg.name));

	// 0xFF-filled means "no items" - that is what ConvertItemByte on the
	// GameServer side reads as an empty slot, and it is also the correct answer
	// for a character with no row yet.
	if(gQueryManager.ExecQuery("SELECT Items FROM SpinWheelClaim WHERE Name='%s'",lpMsg->name) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		memset(pMsg.SpinClaim,0xFF,sizeof(pMsg.SpinClaim));
	}
	else
	{
		gQueryManager.GetAsBinary("Items",pMsg.SpinClaim[0],sizeof(pMsg.SpinClaim));

		gQueryManager.Close();
	}

	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));
}

void CSpinClaim::GDSpinClaimSaveRecv(SDHP_SPINCLAIM_SAVE_RECV* lpMsg) // OK
{
	// Unlike EventInventory's equivalent, the ExecQuery results here are CHECKED.
	// This blob holds prizes the player has already paid for; a write that fails
	// silently loses them, and without a log line nobody would ever know why.
	int result = 0;

	if(gQueryManager.ExecQuery("SELECT Name FROM SpinWheelClaim WHERE Name='%s'",lpMsg->name) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		gQueryManager.BindParameterAsBinary(1,lpMsg->SpinClaim[0],sizeof(lpMsg->SpinClaim));

		result = gQueryManager.ExecQuery("INSERT INTO SpinWheelClaim (Name,Items) VALUES ('%s',?)",lpMsg->name);

		gQueryManager.Close();
	}
	else
	{
		gQueryManager.Close();

		gQueryManager.BindParameterAsBinary(1,lpMsg->SpinClaim[0],sizeof(lpMsg->SpinClaim));

		result = gQueryManager.ExecQuery("UPDATE SpinWheelClaim SET Items=? WHERE Name='%s'",lpMsg->name);

		gQueryManager.Close();
	}

	if(result == 0)
	{
		LogAdd(LOG_RED,"[SpinClaim] FAILED to save the claim box for [%s] - held prizes may be lost",lpMsg->name);
	}
}
