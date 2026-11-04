// ItemTracker.cpp - see ItemTracker.h.
#include "stdafx.h"
#include "ItemTracker.h"
#include "ItemManager.h"
#include "MapManager.h"
#include "ServerInfo.h"
#include "GameMain.h"
#include "User.h"
#include "Util.h"

CItemTracker gItemTracker;

CItemTracker::CItemTracker() // OK
{
	this->m_enabled = 1;
	this->m_minLevel = 7;
	this->m_logAll = 0;
}

void CItemTracker::Load() // OK
{
	const char* path = ".\Data\GameServerInfo - Common.ini";

	this->m_enabled = GetPrivateProfileInt("GameServerInfo","ItemTrackerEnabled",1,path);
	this->m_minLevel = GetPrivateProfileInt("GameServerInfo","ItemTrackerMinLevel",7,path);
	this->m_logAll = GetPrivateProfileInt("GameServerInfo","ItemTrackerLogAll",0,path);
}

bool CItemTracker::Notable(int itemIndex,int level,int exc,int setOpt,int harmony,int optionEx) // OK
{
	if(this->m_logAll != 0 || exc != 0 || setOpt != 0 || harmony != 0 || optionEx != 0 || level >= this->m_minLevel)
	{
		return true;
	}

	// Wings, pets, rings, orbs, tickets... (sections 12 and 13).
	if(itemIndex >= GET_ITEM(12,0) && itemIndex < GET_ITEM(14,0))
	{
		return true;
	}

	// Section 14: only the jewels and the high value specials; potions are noise.
	switch(itemIndex)
	{
		case GET_ITEM(14,13):	// Bless
		case GET_ITEM(14,14):	// Soul
		case GET_ITEM(14,16):	// Life
		case GET_ITEM(14,22):	// Creation
		case GET_ITEM(14,31):	// Guardian
		case GET_ITEM(14,42):	// Harmony
		case GET_ITEM(12,15):	// Chaos
		case GET_ITEM(14,11):	// Box of Luck
			return true;
	}

	return false;
}

static void Copy(char* out,int outSize,const char* in)
{
	strncpy_s(out,outSize,(in == 0) ? "" : in,_TRUNCATE);
}

void CItemTracker::Send(SDHP_ITEMTRACK_SEND* lpMsg) // OK
{
	lpMsg->header.set(0xD9,0x56,sizeof(*lpMsg));
	lpMsg->ServerCode = (WORD)gServerInfo.m_ServerCode;

	gDataServerConnection.DataSend((BYTE*)lpMsg,lpMsg->header.size);
}

void CItemTracker::LogFields(const char* action,int aIndex,int itemIndex,DWORD serial,int level,int skill,int luck,int option,int exc,int setOpt,int harmony,int optionEx,const char* source,const char* details,bool force) // OK
{
	if(this->m_enabled == 0 || OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	if(force == false && this->Notable(itemIndex,level,exc,setOpt,harmony,optionEx) == false)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	SDHP_ITEMTRACK_SEND pMsg;
	memset(&pMsg,0,sizeof(pMsg));

	pMsg.ItemIndex = (WORD)itemIndex;
	pMsg.Serial = serial;
	pMsg.Count = 1;
	pMsg.Level = (BYTE)level;
	pMsg.Skill = (BYTE)skill;
	pMsg.Luck = (BYTE)luck;
	pMsg.Option = (BYTE)option;
	pMsg.Excellent = (BYTE)exc;
	pMsg.SetOption = (BYTE)setOpt;
	pMsg.Harmony = (BYTE)harmony;
	pMsg.OptionEx = (BYTE)optionEx;

	Copy(pMsg.Action,sizeof(pMsg.Action),action);
	Copy(pMsg.Character,sizeof(pMsg.Character),lpObj->Name);
	Copy(pMsg.Source,sizeof(pMsg.Source),source);
	Copy(pMsg.Map,sizeof(pMsg.Map),gMapManager.GetMapName(lpObj->Map));
	Copy(pMsg.ItemName,sizeof(pMsg.ItemName),gItemManager.GetItemName(itemIndex));
	Copy(pMsg.Details,sizeof(pMsg.Details),details);

	this->Send(&pMsg);
}

void CItemTracker::Log(const char* action,int aIndex,CItem* lpItem,const char* source,const char* details) // OK
{
	if(lpItem == 0 || lpItem->IsItem() == 0)
	{
		return;
	}

	this->LogFields(action,aIndex,lpItem->m_Index,lpItem->m_Serial,lpItem->m_Level,lpItem->m_Option1,lpItem->m_Option2,lpItem->m_Option3,lpItem->m_NewOption,lpItem->m_SetOption,lpItem->m_JewelOfHarmonyOption,lpItem->m_ItemOptionEx,source,details);
}

void CItemTracker::LogShop(const char* action,int aIndex,int itemIndex,int level,int count,int price) // OK
{
	if(this->m_enabled == 0 || OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	SDHP_ITEMTRACK_SEND pMsg;
	memset(&pMsg,0,sizeof(pMsg));

	pMsg.ItemIndex = (WORD)itemIndex;
	pMsg.Count = (WORD)((count < 1) ? 1 : count);
	pMsg.Level = (BYTE)level;

	char details[56];
	sprintf_s(details,"%d coin",price);

	Copy(pMsg.Action,sizeof(pMsg.Action),action);
	Copy(pMsg.Character,sizeof(pMsg.Character),lpObj->Name);
	Copy(pMsg.Source,sizeof(pMsg.Source),"Cash shop");
	Copy(pMsg.Map,sizeof(pMsg.Map),gMapManager.GetMapName(lpObj->Map));
	Copy(pMsg.ItemName,sizeof(pMsg.ItemName),gItemManager.GetItemName(itemIndex));
	Copy(pMsg.Details,sizeof(pMsg.Details),details);

	this->Send(&pMsg);
}

void CItemTracker::LogNote(const char* action,int aIndex,const char* itemName,const char* source,const char* details,int count) // OK
{
	if(this->m_enabled == 0 || OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	SDHP_ITEMTRACK_SEND pMsg;
	memset(&pMsg,0,sizeof(pMsg));

	pMsg.Count = (WORD)((count < 1) ? 1 : ((count > 65535) ? 65535 : count));

	Copy(pMsg.Action,sizeof(pMsg.Action),action);
	Copy(pMsg.Character,sizeof(pMsg.Character),lpObj->Name);
	Copy(pMsg.Source,sizeof(pMsg.Source),source);
	Copy(pMsg.Map,sizeof(pMsg.Map),gMapManager.GetMapName(lpObj->Map));
	Copy(pMsg.ItemName,sizeof(pMsg.ItemName),itemName);
	Copy(pMsg.Details,sizeof(pMsg.Details),details);

	this->Send(&pMsg);
}

void CItemTracker::LogCoin(int aIndex,const char* coinName,int change,int balance,const char* reason) // OK
{
	char details[56];
	sprintf_s(details,"%+d, balance %d",change,balance);

	this->LogNote("WCoin",aIndex,coinName,reason,details,1);
}

struct MIX_SNAP_ITEM
{
	short Index;
	BYTE Level,Skill,Luck,Option,Exc,SetOpt,Harmony,OptionEx;
	DWORD Serial;
};

struct MIX_SNAP
{
	int MixId;
	int Count;
	MIX_SNAP_ITEM Item[CHAOS_BOX_SIZE];
};

static MIX_SNAP* gMixSnap[MAX_OBJECT];

void CItemTracker::MixBegin(int aIndex,int mixId) // OK
{
	if(this->m_enabled == 0 || OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	if(gMixSnap[aIndex] == 0)
	{
		gMixSnap[aIndex] = new MIX_SNAP;
	}

	MIX_SNAP* snap = gMixSnap[aIndex];

	snap->MixId = mixId;
	snap->Count = 0;

	for(int n=0;n < CHAOS_BOX_SIZE;n++)
	{
		CItem* lpItem = &gObj[aIndex].ChaosBox[n];

		if(lpItem->IsItem() == 0)
		{
			continue;
		}

		MIX_SNAP_ITEM* it = &snap->Item[snap->Count++];

		it->Index = lpItem->m_Index;
		it->Level = (BYTE)lpItem->m_Level;
		it->Skill = (BYTE)lpItem->m_Option1;
		it->Luck = (BYTE)lpItem->m_Option2;
		it->Option = (BYTE)lpItem->m_Option3;
		it->Exc = lpItem->m_NewOption;
		it->SetOpt = lpItem->m_SetOption;
		it->Harmony = lpItem->m_JewelOfHarmonyOption;
		it->OptionEx = lpItem->m_ItemOptionEx;
		it->Serial = lpItem->m_Serial;
	}
}

void CItemTracker::MixEnd(int aIndex,int result,CItem* lpItem) // OK
{
	if(this->m_enabled == 0 || OBJECT_RANGE(aIndex) == 0 || gMixSnap[aIndex] == 0 || gMixSnap[aIndex]->Count == 0)
	{
		return;
	}

	MIX_SNAP* snap = gMixSnap[aIndex];

	// 1 = a result item, 0 = the mix failed; every other code is a refusal
	// (not enough zen, wrong ingredients...) and nothing was consumed.
	if(result != 0 && result != 1)
	{
		snap->Count = 0;
		return;
	}

	char details[56];
	sprintf_s(details,"mix %d, rate %d%%, zen %d",snap->MixId,gObj[aIndex].ChaosSuccessRate,gObj[aIndex].ChaosMoney);

	if(result == 1 && lpItem != 0 && lpItem->IsItem() != 0)
	{
		this->LogFields("MixSuccess",aIndex,lpItem->m_Index,lpItem->m_Serial,lpItem->m_Level,lpItem->m_Option1,lpItem->m_Option2,lpItem->m_Option3,lpItem->m_NewOption,lpItem->m_SetOption,lpItem->m_JewelOfHarmonyOption,lpItem->m_ItemOptionEx,"Chaos Machine",details,true);
	}
	else if(result == 0)
	{
		this->LogNote("MixFail",aIndex,"Chaos mix","Chaos Machine",details,1);
	}

	for(int n=0;n < snap->Count;n++)
	{
		MIX_SNAP_ITEM* it = &snap->Item[n];

		this->LogFields("MixInput",aIndex,it->Index,it->Serial,it->Level,it->Skill,it->Luck,it->Option,it->Exc,it->SetOpt,it->Harmony,it->OptionEx,"Chaos Machine",((result == 1)?"Used, mix succeeded":"Used, mix failed"));
	}

	snap->Count = 0;
}

void CItemTracker::LogBank(int aIndex,int itemIndex,int level,int count,const char* details) // OK
{
	if(this->m_enabled == 0 || OBJECT_RANGE(aIndex) == 0 || count == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	SDHP_ITEMTRACK_SEND pMsg;
	memset(&pMsg,0,sizeof(pMsg));

	pMsg.ItemIndex = (WORD)itemIndex;
	pMsg.Count = (WORD)((count < 0) ? -count : count);
	pMsg.Level = (BYTE)level;

	Copy(pMsg.Action,sizeof(pMsg.Action),(count > 0) ? "BankDeposit" : "BankWithdraw");
	Copy(pMsg.Character,sizeof(pMsg.Character),lpObj->Name);
	Copy(pMsg.Source,sizeof(pMsg.Source),"Jewel Bank");
	Copy(pMsg.Map,sizeof(pMsg.Map),gMapManager.GetMapName(lpObj->Map));
	Copy(pMsg.ItemName,sizeof(pMsg.ItemName),gItemManager.GetItemName(itemIndex));
	Copy(pMsg.Details,sizeof(pMsg.Details),details);

	this->Send(&pMsg);
}
