// CB_KiraPass.h: Kira Pass (monthly WCoin) panel, opened from the Menu.
//
// Status comes from the GameServer (C1:D3:F4) at login and after a purchase;
// the client never asks for it, so no new inbound packet is needed. Buying:
// PC opens the website store, phones use Google Play / App Store billing
// (product kira_pass_30). See Working Project\MD Files\MonthlyWCoin-PLAN.md.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Protocol.h"

#pragma pack(push,1)
// Mirrored in the GameServer's MonthlyCard.h.
struct PMSG_MONTHLYCARD_STATUS_RECV
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

static_assert(sizeof(PMSG_MONTHLYCARD_STATUS_RECV) == 4 + 4 + 1 + 6 + 1, "PMSG_MONTHLYCARD_STATUS_RECV must match the GameServer's copy");

#define KIRA_PASS_PRODUCT_ID	"kira_pass_30"
#define KIRA_PASS_STORE_URL		"https://worldofkira.com/account/?view=store"

class CB_KiraPass
{
public:
	CB_KiraPass();

	void OpenWindow();
	void DrawWindow();
	void RecvStatus(const BYTE* lpMsg,int size);

private:
	void Buy();

	bool m_Known;          // a status arrived this session
	DWORD m_DaysLeft;
	BYTE m_PaidToday;
	WORD m_Days;
	WORD m_Instant;
	WORD m_Daily;
};

extern CB_KiraPass* gCB_KiraPass;
