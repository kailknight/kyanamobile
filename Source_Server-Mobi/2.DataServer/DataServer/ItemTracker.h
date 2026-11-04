// ItemTracker.h - writes the public Item Tracker feed (ItemActionLog).
//
// The GameServer sends one 0xD9:0x56 per notable item action (GameServer
// ItemTracker.h). Fire and forget, nothing is answered. Rows older than
// DataServer.ini [DataServerInfo] ItemTrackRetentionDays (default 30, 0 = keep
// forever) are deleted in small batches, at most once an hour, while rows are
// arriving. ItemTrackerEnabled = 0 drops the packets.
//
// Table and read-only login: Database\ItemTracker_Schema.sql.
#pragma once

#include "DataServerProtocol.h"

#pragma pack(push,1)

struct SDHP_ITEMTRACK_RECV
{
	PSBMSG_HEAD header; // C1:D9:56
	WORD ServerCode;
	WORD ItemIndex;
	DWORD Serial;
	WORD Count;
	BYTE Level;
	BYTE Skill;
	BYTE Luck;
	BYTE Option;
	BYTE Excellent;
	BYTE SetOption;
	BYTE Harmony;
	BYTE OptionEx;
	char Action[16];
	char Character[11];
	char Source[24];
	char Map[24];
	char ItemName[40];
	char Details[56];
};

#pragma pack(pop)

static_assert(sizeof(SDHP_ITEMTRACK_RECV) == 4 + 2 + 2 + 4 + 2 + 8 + 16 + 11 + 24 + 24 + 40 + 56,"SDHP_ITEMTRACK_RECV layout");

class CItemTracker
{
public:
	CItemTracker();
	void Load();
	void OnItem(SDHP_ITEMTRACK_RECV* lpMsg,int size);
private:
	void Purge();
	int m_enabled;
	int m_retentionDays;
	DWORD m_lastPurge;
};

extern CItemTracker gItemTracker;
