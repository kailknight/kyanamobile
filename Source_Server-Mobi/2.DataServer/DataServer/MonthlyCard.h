// MonthlyCard.h: Monthly WCoin (Kira Pass), DataServer side.
//
// See Working Project\MD Files\MonthlyWCoin-PLAN.md and MonthlyCard_Schema.sql.
// Settings: PlayBilling.ini [MonthlyCard] (Daily, Instant, Days, MailExpireDays,
// Sender).
//
//////////////////////////////////////////////////////////////////////

#pragma once

// Mirrored byte for byte in the GameServer's MonthlyCard.h.
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

static_assert(sizeof(SDHP_MONTHLYCARD_REQ) == 4 + 2 + 11 + 11 + 1, "SDHP_MONTHLYCARD_REQ must match the GameServer's copy");
static_assert(sizeof(SDHP_MONTHLYCARD_RESULT) == 4 + 2 + 11 + 1 + 4 + 4 + 1 + 6, "SDHP_MONTHLYCARD_RESULT must match the GameServer's copy");

class CMonthlyCard
{
public:
	void OnRequest(SDHP_MONTHLYCARD_REQ* lpMsg,int index);

	// One purchase (Google / Apple / website / admin). Result 0 = granted now,
	// 1 = this purchase was granted before, -1 = database error.
	int Grant(const char* account,const char* source,const char* ref);
};

extern CMonthlyCard gMonthlyCard;
