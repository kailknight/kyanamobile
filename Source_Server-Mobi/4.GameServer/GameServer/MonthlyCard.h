// MonthlyCard.h: Monthly WCoin (Kira Pass), GameServer side.
//
// On character enter the DataServer pays what is owed by mail (instant reward
// + today's daily reward) and returns the card status. See
// Working Project\MD Files\MonthlyWCoin-PLAN.md.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Protocol.h"

// Mirrored byte for byte in the DataServer's MonthlyCard.h.
#pragma pack(push,1)

// GameServer -> DataServer, 0xD9 / 0x50.
struct SDHP_MONTHLYCARD_REQ
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	char Name[11];
	BYTE Mode;          // 0 = character entered: pay what is owed, then status. 1 = status only.
};

// DataServer -> GameServer, 0xD9 / 0x50.
struct SDHP_MONTHLYCARD_RESULT
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char Account[11];
	BYTE Mode;
	DWORD Amount;       // WCoin (C) mailed just now (0 = nothing)
	DWORD DaysLeft;     // 0 = no card / ended
	BYTE PaidToday;     // today's daily reward has gone out
	WORD Days;          // PlayBilling.ini [MonthlyCard], for the client panel
	WORD Instant;
	WORD Daily;
};

#pragma pack(pop)

static_assert(sizeof(SDHP_MONTHLYCARD_REQ) == 4 + 2 + 11 + 11 + 1, "SDHP_MONTHLYCARD_REQ must match the DataServer's copy");
static_assert(sizeof(SDHP_MONTHLYCARD_RESULT) == 4 + 2 + 11 + 1 + 4 + 4 + 1 + 6, "SDHP_MONTHLYCARD_RESULT must match the DataServer's copy");

// GameServer -> client, C1:D3:F4: Kira Pass status for the Menu panel. Mirrored
// in the client's CB_KiraPass.h.
#pragma pack(push,1)
struct PMSG_MONTHLYCARD_STATUS_SEND
{
	PSBMSG_HEAD header;
	DWORD DaysLeft;
	BYTE PaidToday;
	WORD Days;
	WORD Instant;
	WORD Daily;
	BYTE OpenMailbox;   // 1 = a real login (not a server switch) with an active pass: show the mailbox
};
#pragma pack(pop)

static_assert(sizeof(PMSG_MONTHLYCARD_STATUS_SEND) == 4 + 4 + 1 + 6 + 1, "PMSG_MONTHLYCARD_STATUS_SEND must match the client's copy");

class CMonthlyCard
{
public:
	// Character entered (friend data loading): collect what is owed.
	void OnCharacterEnter(int aIndex);

	// Status only (the client's Kira Pass panel asks).
	void RequestStatus(int aIndex);

	void DGResultRecv(SDHP_MONTHLYCARD_RESULT* lpMsg);

private:
	void SendRequest(int aIndex,BYTE mode);
};

extern CMonthlyCard gMonthlyCard;
