// SlotClaim.cpp: implementation of the CSlotClaim class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SlotClaim.h"
#include "QueryManager.h"
#include "SocketManager.h"
#include "Util.h"

CSlotClaim gSlotClaim;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSlotClaim::CSlotClaim() // OK
{

}

CSlotClaim::~CSlotClaim() // OK
{

}

void CSlotClaim::GDSlotClaimRecv(SDHP_SLOTCLAIM_RECV* lpMsg,int index) // OK
{
	SDHP_SLOTCLAIM_SEND pMsg;

	pMsg.header.set(0xD9,0x34,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	memcpy(pMsg.name,lpMsg->name,sizeof(pMsg.name));

	// ---- Held prizes: PER CHARACTER -------------------------------------
	// A prize was won by a character and is claimed back into that character's
	// inventory, so it stays keyed by name.
	//
	// 0xFF-filled means "every row empty" - ItemIndex reads as 0xFFFF, which is
	// what the GameServer treats as an empty row. That is also the right answer for
	// a character with no row yet, so a missing row and an empty box are the same
	// bytes and neither needs special-casing.
	if(gQueryManager.ExecQuery("SELECT Items FROM SlotMachineClaim WHERE Name='%s'",lpMsg->name) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		memset(pMsg.SlotClaim,0xFF,sizeof(pMsg.SlotClaim));
	}
	else
	{
		gQueryManager.GetAsBinary("Items",pMsg.SlotClaim[0],sizeof(pMsg.SlotClaim));

		gQueryManager.Close();
	}

	// ---- Machine state: PER ACCOUNT --------------------------------------
	// Beginner luck, the daily pity allowance and the losing streak are all keyed by
	// ACCOUNT, in their own table.
	//
	// Per character they were farmable: a player could roll a fresh character to get
	// another fifty beginner spins, and another pity win every day, as many times as
	// they cared to make characters.
	//
	// 0xFF-filled first, so an account with no row yet - or a NULL - reads as "no
	// state" rather than as whatever the buffer happened to hold.
	memset(pMsg.State,0xFF,sizeof(pMsg.State));

	if(gQueryManager.ExecQuery("SELECT State FROM SlotMachineAccount WHERE Account='%s'",lpMsg->account) != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		gQueryManager.GetAsBinary("State",pMsg.State,sizeof(pMsg.State));
	}

	gQueryManager.Close();

	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));
}

void CSlotClaim::GDSlotClaimSaveRecv(SDHP_SLOTCLAIM_SAVE_RECV* lpMsg) // OK
{
	// The ExecQuery results are CHECKED, as SpinClaim's are and unlike
	// EventInventory's. This blob holds winnings the player has already staked for;
	// a write that fails silently loses them, and without a log line nobody would
	// ever know why.
	int result = 0;

	// ---- Held prizes: PER CHARACTER -------------------------------------
	if(gQueryManager.ExecQuery("SELECT Name FROM SlotMachineClaim WHERE Name='%s'",lpMsg->name) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		gQueryManager.BindParameterAsBinary(1,lpMsg->SlotClaim[0],sizeof(lpMsg->SlotClaim));

		result = gQueryManager.ExecQuery("INSERT INTO SlotMachineClaim (Name,Items) VALUES ('%s',?)",lpMsg->name);

		gQueryManager.Close();
	}
	else
	{
		gQueryManager.Close();

		gQueryManager.BindParameterAsBinary(1,lpMsg->SlotClaim[0],sizeof(lpMsg->SlotClaim));

		result = gQueryManager.ExecQuery("UPDATE SlotMachineClaim SET Items=? WHERE Name='%s'",lpMsg->name);

		gQueryManager.Close();
	}

	if(result == 0)
	{
		LogAdd(LOG_RED,"[SlotClaim] FAILED to save the reward box for [%s] - held winnings may be lost",lpMsg->name);
	}

	// ---- Machine state: PER ACCOUNT --------------------------------------
	int stateResult = 0;

	if(gQueryManager.ExecQuery("SELECT Account FROM SlotMachineAccount WHERE Account='%s'",lpMsg->account) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		gQueryManager.BindParameterAsBinary(1,lpMsg->State,sizeof(lpMsg->State));

		stateResult = gQueryManager.ExecQuery("INSERT INTO SlotMachineAccount (Account,State) VALUES ('%s',?)",lpMsg->account);

		gQueryManager.Close();
	}
	else
	{
		gQueryManager.Close();

		gQueryManager.BindParameterAsBinary(1,lpMsg->State,sizeof(lpMsg->State));

		stateResult = gQueryManager.ExecQuery("UPDATE SlotMachineAccount SET State=? WHERE Account='%s'",lpMsg->account);

		gQueryManager.Close();
	}

	// Checked separately from the prize save: this one going quiet is what would let
	// beginner luck and the daily pity allowance start over on the next login.
	if(stateResult == 0)
	{
		LogAdd(LOG_RED,"[SlotClaim] FAILED to save the machine state for account [%s] - beginner luck and the daily pity limit may reset",lpMsg->account);
	}
}
