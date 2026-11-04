// ItemTracker.h - public "Item Tracker" feed: one database row per notable item action.
//
// Every call here sends one 0xD9:0x56 packet to the DataServer (ItemTracker.cpp
// there), which writes the row to ItemActionLog (Database\ItemTracker_Schema.sql).
// Fire and forget, like DeviceBan: a database outage loses tracker rows, never
// gameplay. Only CHARACTER names are sent - never the account or the IP, so
// nothing in the table can leak them to the public page.
//
// Which items are logged is decided here, so the table stays small:
//   always   excellent, set, harmony, socket items, jewels and other tracked
//            specials, wings/pets/rings (sections 12 and 13), +ItemTrackerMinLevel
//   never    plain potions/arrows/common stuff, unless ItemTrackerLogAll = 1
//
// Keys, GameServerInfo - Common.ini [GameServerInfo]:
//   ItemTrackerEnabled   = 1   0 switches the whole feature off
//   ItemTrackerMinLevel  = 7   +N and above is always logged
//   ItemTrackerLogAll    = 0   1 = log every item, potions included
#pragma once

#include "Item.h"
#include "Protocol.h"

#pragma pack(push,1)

struct SDHP_ITEMTRACK_SEND
{
	PSBMSG_HEAD header; // C1:D9:56
	WORD ServerCode;
	WORD ItemIndex;
	DWORD Serial;
	WORD Count;			// jewel bank movements; 1 otherwise
	BYTE Level;
	BYTE Skill;			// Option1
	BYTE Luck;			// Option2
	BYTE Option;		// Option3 (0-7, x4 in game)
	BYTE Excellent;		// NewOption bit mask
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

static_assert(sizeof(SDHP_ITEMTRACK_SEND) == 4 + 2 + 2 + 4 + 2 + 8 + 16 + 11 + 24 + 24 + 40 + 56,"SDHP_ITEMTRACK_SEND layout");

class CItemTracker
{
public:
	CItemTracker();
	void Load();
	// An item that exists as a CItem (inventory, trade, shop...).
	void Log(const char* action,int aIndex,CItem* lpItem,const char* source,const char* details = "");
	// An item known only by its fields (a freshly issued serial).
	void LogFields(const char* action,int aIndex,int itemIndex,DWORD serial,int level,int skill,int luck,int option,int exc,int setOpt,int harmony,int optionEx,const char* source,const char* details = "",bool force = false);
	// Jewel bank deposit (count > 0) or withdrawal (count < 0): no serial.
	void LogBank(int aIndex,int itemIndex,int level,int count,const char* details = "");
	// Cash shop purchase / use: logged whatever the item is (no serial, no notable filter).
	void LogShop(const char* action,int aIndex,int itemIndex,int level,int count,int price);
	// Something that is not an item: a coin balance change or a slot machine spin.
	// Chaos machine: MixBegin snapshots the box when the player presses Mix, MixEnd (from GCChaosMixSend) logs the result and the ingredients.
	void MixBegin(int aIndex,int mixId);
	void MixEnd(int aIndex,int result,CItem* lpItem);
	void LogNote(const char* action,int aIndex,const char* itemName,const char* source,const char* details,int count = 1);
	void LogCoin(int aIndex,const char* coinName,int change,int balance,const char* reason);
private:
	bool Notable(int itemIndex,int level,int exc,int setOpt,int harmony,int optionEx);
	void Send(SDHP_ITEMTRACK_SEND* lpMsg);
	int m_enabled;
	int m_minLevel;
	int m_logAll;
};

extern CItemTracker gItemTracker;
