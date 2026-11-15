// ItemReroll.cpp - see ItemReroll.h. Config: Data\Custom\CustomItemReroll.txt.
#include "stdafx.h"
#include "ItemReroll.h"
#include "CashShop.h"
#include "BCustomItemBank.h"
#include "DSProtocol.h"
#include "ItemManager.h"
#include "ItemOptionRate.h"
#include "Log.h"
#include "MapServerManager.h"
#include "Monster.h"
#include "Notice.h"
#include "Path.h"
#include "ObjectManager.h"
#include "Protocol.h"
#include "ServerInfo.h"
#include "User.h"
#include "Util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

CItemReroll gItemReroll;

namespace
{
	struct RRLock
	{
		CRITICAL_SECTION* m_cs;
		RRLock(CRITICAL_SECTION* cs) : m_cs(cs) { EnterCriticalSection(m_cs); }
		~RRLock() { LeaveCriticalSection(m_cs); }
	};

	int BitCount(BYTE v)
	{
		int c = 0;

		for(int n=0;n < 8;n++)
		{
			if((v & (1 << n)) != 0)
			{
				c++;
			}
		}

		return c;
	}

	// "12,15" or "12 15" -> 2 numbers
	bool ParsePair(const char* text,int* a,int* b)
	{
		return (sscanf(text,"%d%*[ ,;\t]%d",a,b) == 2);
	}

}

CItemReroll::CItemReroll()
{
	InitializeCriticalSection(&this->m_cs);
	this->m_SecTick = 0;
	this->m_NpcIndex = -1;
	this->m_Switch = 0;
	this->m_NpcClass = 243;
	this->m_NpcMap = 0;
	this->m_NpcX = 130;
	this->m_NpcY = 229;
	this->m_NpcDir = 3;
	this->m_MaxDistance = 10;
	this->m_AllowWeapon = 1;
	this->m_AllowShield = 1;
	this->m_AllowArmor = 1;
	this->m_AllowWing = 1;
	this->m_AllowAccessory = 1;
	this->m_AllowPet = 0;
	this->m_AllowAncient = 0;
	this->m_JewelItem = GET_ITEM(12,15);
	this->m_JewelCount = 1;
	this->m_JewelPerLock = 1;
	this->m_PriceWC = 100;
	this->m_PriceGP = 200;
	this->m_LockWC = 50;
	this->m_LockGP = 100;
	this->m_PriceJW = -1;
	this->m_PayWith = 1;
	this->m_PriceOv = -1;
	this->m_LockOv = -1;
	for(int n=0;n < 6;n++){this->m_MultWeapon[n] = 1;this->m_MultArmor[n] = 1;this->m_MultAccessory[n] = 1;this->m_MultWing[n] = 1;}
	this->m_LockJW = 0;
	this->m_MinOptions = 1;
	this->m_MaxOptions = 4;
	this->m_PendingSeconds = 300;
	this->m_SpinMs = 1800;
	this->m_UseJewelBank = 1;
	this->m_AllowSame = 0;
	this->m_KeepCount = 1;
	for(int n=0;n < 6;n++){this->m_RateWeapon[n] = 10;this->m_RateArmor[n] = 10;this->m_RateAccessory[n] = 10;this->m_RateWing[n] = 10;}
	this->m_DenyCount = 0;
}

CItemReroll::~CItemReroll()
{
	DeleteCriticalSection(&this->m_cs);
}

//////////////////////////////////////////////////////////////////////
// Config
//////////////////////////////////////////////////////////////////////

void CItemReroll::Load(char* path) // OK
{
	RRLock lock(&this->m_cs);

	// defaults first, so a key removed from the file goes back to its default
	int switchOld = this->m_Switch;
	this->m_Switch = 0;
	this->m_NpcClass = 243;
	this->m_NpcMap = 0;
	this->m_NpcX = 130;
	this->m_NpcY = 229;
	this->m_NpcDir = 3;
	this->m_MaxDistance = 10;
	this->m_AllowWeapon = 1;
	this->m_AllowShield = 1;
	this->m_AllowArmor = 1;
	this->m_AllowWing = 1;
	this->m_AllowAccessory = 1;
	this->m_AllowPet = 0;
	this->m_AllowAncient = 0;
	this->m_JewelItem = GET_ITEM(12,15);
	this->m_JewelCount = 1;
	this->m_JewelPerLock = 1;
	this->m_PriceWC = 100;
	this->m_PriceGP = 200;
	this->m_LockWC = 50;
	this->m_LockGP = 100;
	this->m_PriceJW = -1;
	this->m_PayWith = 1;
	this->m_PriceOv = -1;
	this->m_LockOv = -1;
	for(int n=0;n < 6;n++){this->m_MultWeapon[n] = 1;this->m_MultArmor[n] = 1;this->m_MultAccessory[n] = 1;this->m_MultWing[n] = 1;}
	this->m_LockJW = 0;
	this->m_MinOptions = 1;
	this->m_MaxOptions = 4;
	this->m_PendingSeconds = 300;
	this->m_SpinMs = 1800;
	this->m_UseJewelBank = 1;
	this->m_AllowSame = 0;
	this->m_KeepCount = 1;
	for(int n=0;n < 6;n++){this->m_RateWeapon[n] = 10;this->m_RateArmor[n] = 10;this->m_RateAccessory[n] = 10;this->m_RateWing[n] = 10;}
	this->m_DenyCount = 0;

	FILE* f = fopen(path,"r");

	if(f == 0)
	{
		LogAdd(LOG_RED,"[ItemReroll] %s not found - the Excellent Item Reroller stays OFF",path);
		(void)switchOld;
		return;
	}

	char line[512];

	while(fgets(line,sizeof(line),f) != 0)
	{
		// strip comments
		char* c = strstr(line,"//");

		if(c != 0){*c = 0;}

		c = strchr(line,'#');

		if(c != 0){*c = 0;}

		char* eq = strchr(line,'=');

		if(eq == 0)
		{
			continue;
		}

		*eq = 0;

		char* key = line;
		char* val = eq+1;

		while(*key != 0 && isspace((unsigned char)*key)){key++;}

		int kl = (int)strlen(key);

		while(kl > 0 && isspace((unsigned char)key[kl-1])){key[--kl] = 0;}

		while(*val != 0 && isspace((unsigned char)*val)){val++;}

		int vl = (int)strlen(val);

		while(vl > 0 && isspace((unsigned char)val[vl-1])){val[--vl] = 0;}

		if(*key == 0 || *val == 0)
		{
			continue;
		}

		int v = atoi(val);

		#define RR_KEY(name) (_stricmp(key,name) == 0)

		if(RR_KEY("Switch")){this->m_Switch = (v != 0);}
		else if(RR_KEY("NpcClass")){this->m_NpcClass = v;}
		else if(RR_KEY("NpcMap")){this->m_NpcMap = v;}
		else if(RR_KEY("NpcX")){this->m_NpcX = v;}
		else if(RR_KEY("NpcY")){this->m_NpcY = v;}
		else if(RR_KEY("NpcDir")){this->m_NpcDir = v;}
		else if(RR_KEY("MaxDistance")){this->m_MaxDistance = v;}
		else if(RR_KEY("AllowWeapons")){this->m_AllowWeapon = (v != 0);}
		else if(RR_KEY("AllowShields")){this->m_AllowShield = (v != 0);}
		else if(RR_KEY("AllowArmors")){this->m_AllowArmor = (v != 0);}
		else if(RR_KEY("AllowWings")){this->m_AllowWing = (v != 0);}
		else if(RR_KEY("AllowAccessories")){this->m_AllowAccessory = (v != 0);}
		else if(RR_KEY("AllowPets")){this->m_AllowPet = (v != 0);}
		else if(RR_KEY("AllowAncient")){this->m_AllowAncient = (v != 0);}
		else if(RR_KEY("Jewel"))
		{
			int g = 0,i = 0;

			if(ParsePair(val,&g,&i) != false && g >= 0 && g < 16 && i >= 0 && i < MAX_ITEM_TYPE)
			{
				this->m_JewelItem = GET_ITEM(g,i);
			}
		}
		else if(RR_KEY("JewelCount")){this->m_JewelCount = v;}
		else if(RR_KEY("JewelPerLock")){this->m_JewelPerLock = v;}
		else if(RR_KEY("PriceWCoin")){this->m_PriceWC = v;}
		else if(RR_KEY("PriceGoblinPoint")){this->m_PriceGP = v;}
		else if(RR_KEY("PriceJewel")){this->m_PriceJW = v;}
		else if(RR_KEY("PayWith")){this->m_PayWith = v;}
		else if(RR_KEY("Price")){this->m_PriceOv = v;}
		else if(RR_KEY("LockPrice")){this->m_LockOv = v;}
		else if(RR_KEY("PriceMultiWeapon") || RR_KEY("PriceMultiArmor") || RR_KEY("PriceMultiAccessory") || RR_KEY("PriceMultiWing"))
		{
			int w[6] = {1,1,1,1,1,1};
			sscanf(val,"%d%*[ ,;\t]%d%*[ ,;\t]%d%*[ ,;\t]%d%*[ ,;\t]%d%*[ ,;\t]%d",&w[0],&w[1],&w[2],&w[3],&w[4],&w[5]);
			BYTE* dst = (_stricmp(key,"PriceMultiWeapon") == 0)?this->m_MultWeapon:((_stricmp(key,"PriceMultiArmor") == 0)?this->m_MultArmor:((_stricmp(key,"PriceMultiAccessory") == 0)?this->m_MultAccessory:this->m_MultWing));
			for(int n=0;n < 6;n++){dst[n] = (BYTE)((w[n] < 0)?0:((w[n] > 100)?100:w[n]));}
		}
		else if(RR_KEY("LockPriceJewel")){this->m_LockJW = v;}
		else if(RR_KEY("LockPriceWCoin")){this->m_LockWC = v;}
		else if(RR_KEY("LockPriceGoblinPoint")){this->m_LockGP = v;}
		else if(RR_KEY("MinOptions")){this->m_MinOptions = v;}
		else if(RR_KEY("MaxOptions")){this->m_MaxOptions = v;}
		else if(RR_KEY("PendingSeconds")){this->m_PendingSeconds = v;}
		else if(RR_KEY("SpinMs")){this->m_SpinMs = v;}
		else if(RR_KEY("UseJewelBank")){this->m_UseJewelBank = (v != 0);}
		else if(RR_KEY("AllowSameOption")){this->m_AllowSame = (v != 0);}
		else if(RR_KEY("KeepOptionCount")){this->m_KeepCount = (v != 0);}
		else if(RR_KEY("RateWeapon") || RR_KEY("RateArmor") || RR_KEY("RateAccessory") || RR_KEY("RateWing"))
		{
			int w[6] = {10,10,10,10,10,10};
			sscanf(val,"%d%*[ ,;	]%d%*[ ,;	]%d%*[ ,;	]%d%*[ ,;	]%d%*[ ,;	]%d",&w[0],&w[1],&w[2],&w[3],&w[4],&w[5]);
			BYTE* dst = (_stricmp(key,"RateWeapon") == 0)?this->m_RateWeapon:((_stricmp(key,"RateArmor") == 0)?this->m_RateArmor:((_stricmp(key,"RateAccessory") == 0)?this->m_RateAccessory:this->m_RateWing));
			for(int n=0;n < 6;n++){dst[n] = (BYTE)((w[n] < 0)?0:((w[n] > 100)?100:w[n]));}
		}
		else if(RR_KEY("DenyItem"))
		{
			int g = 0,i = 0;

			if(ParsePair(val,&g,&i) != false && g >= 0 && g < 16 && i >= 0 && i < MAX_ITEM_TYPE && this->m_DenyCount < RR_MAX_DENY)
			{
				this->m_Deny[this->m_DenyCount++] = GET_ITEM(g,i);
			}
		}

		#undef RR_KEY
	}

	fclose(f);

	// sanity
	if(this->m_MaxDistance < 1){this->m_MaxDistance = 1;}
	if(this->m_MaxDistance > 30){this->m_MaxDistance = 30;}
	if(this->m_JewelCount < 0){this->m_JewelCount = 0;}
	if(this->m_JewelCount > 200){this->m_JewelCount = 200;}
	if(this->m_JewelPerLock < 0){this->m_JewelPerLock = 0;}
	if(this->m_JewelPerLock > 50){this->m_JewelPerLock = 50;}
	if(this->m_PriceWC < 0){this->m_PriceWC = -1;} // -1 = WCoin not accepted
	if(this->m_PriceGP < 0){this->m_PriceGP = -1;} // -1 = Goblin Points not accepted
	if(this->m_LockWC < 0){this->m_LockWC = 0;}
	if(this->m_LockGP < 0){this->m_LockGP = 0;}
	if(this->m_PriceJW < 0){this->m_PriceJW = -1;} // -1 = paying the Confirm with jewels is not accepted
	if(this->m_PriceJW > 1000){this->m_PriceJW = 1000;}
	if(this->m_LockJW < 0){this->m_LockJW = 0;}
	if(this->m_LockJW > 100){this->m_LockJW = 100;}

	// ONE payment method for the Confirm: PayWith = 1 WCoin, 2 Goblin Points, 3 jewels. 'Price' / 'LockPrice' (when present) set its prices.
	if(this->m_PayWith < 1 || this->m_PayWith > 3){this->m_PayWith = 1;}

	if(this->m_PayWith == 1)
	{
		if(this->m_PriceOv >= 0){this->m_PriceWC = this->m_PriceOv;}
		if(this->m_LockOv >= 0){this->m_LockWC = this->m_LockOv;}
		if(this->m_PriceWC < 0){this->m_PriceWC = 100;}
	}
	else if(this->m_PayWith == 2)
	{
		if(this->m_PriceOv >= 0){this->m_PriceGP = this->m_PriceOv;}
		if(this->m_LockOv >= 0){this->m_LockGP = this->m_LockOv;}
		if(this->m_PriceGP < 0){this->m_PriceGP = 100;}
	}
	else
	{
		if(this->m_PriceOv >= 0){this->m_PriceJW = min(this->m_PriceOv,1000);}
		if(this->m_LockOv >= 0){this->m_LockJW = min(this->m_LockOv,100);}
		if(this->m_PriceJW < 0){this->m_PriceJW = 1;}
	}
	if(this->m_MaxOptions < 1){this->m_MaxOptions = 1;}
	if(this->m_MaxOptions > MAX_EXC_OPTION){this->m_MaxOptions = MAX_EXC_OPTION;}
	if(this->m_MinOptions < 1){this->m_MinOptions = 1;}
	if(this->m_MinOptions > this->m_MaxOptions){this->m_MinOptions = this->m_MaxOptions;}
	if(this->m_PendingSeconds < 10){this->m_PendingSeconds = 10;}
	if(this->m_SpinMs < 300){this->m_SpinMs = 300;}
	if(this->m_SpinMs > 10000){this->m_SpinMs = 10000;}


	LogAdd(LOG_BLUE,"[ItemReroll] loaded: %s, NPC %d at map %d (%d,%d), jewel %d x%d (+%d/lock), WCoin %d (+%d/lock), Goblin %d (+%d/lock), options %d-%d",
		(this->m_Switch != 0)?"ON":"OFF",this->m_NpcClass,this->m_NpcMap,this->m_NpcX,this->m_NpcY,this->m_JewelItem,this->m_JewelCount,this->m_JewelPerLock,this->m_PriceWC,this->m_LockWC,this->m_PriceGP,this->m_LockGP,this->m_MinOptions,this->m_MaxOptions);
}

//////////////////////////////////////////////////////////////////////
// NPC
//////////////////////////////////////////////////////////////////////

bool CItemReroll::Served() // OK
{
	return (gMapServerManager.CheckMapServer(this->m_NpcMap) != 0);
}

void CItemReroll::MainProc() // OK
{
	if((GetTickCount()-this->m_SecTick) < 1000)
	{
		return;
	}

	this->m_SecTick = GetTickCount();

	RRLock lock(&this->m_cs);

	bool alive = (this->m_NpcIndex != -1 && gObjIsConnected(this->m_NpcIndex) != 0 && gObj[this->m_NpcIndex].Class == this->m_NpcClass && gObj[this->m_NpcIndex].Map == this->m_NpcMap && gObj[this->m_NpcIndex].X == this->m_NpcX && gObj[this->m_NpcIndex].Y == this->m_NpcY);

	if(this->m_Switch == 0 || this->Served() == false)
	{
		if(alive != false)
		{
			gObjDel(this->m_NpcIndex);
			LogAdd(LOG_BLUE,"[ItemReroll] NPC removed (switched off)");
		}

		this->m_NpcIndex = -1;
		return;
	}

	if(alive != false)
	{
		// the config may have moved it
		if(gObj[this->m_NpcIndex].Map != this->m_NpcMap || gObj[this->m_NpcIndex].X != this->m_NpcX || gObj[this->m_NpcIndex].Y != this->m_NpcY)
		{
			gObjDel(this->m_NpcIndex);
			this->m_NpcIndex = -1;
		}
		else
		{
			return;
		}
	}

	int index = gObjAddMonster(this->m_NpcMap);

	if(OBJECT_RANGE(index) == 0)
	{
		this->m_NpcIndex = -1;
		return;
	}

	if(CBCustomAddMob(index,this->m_NpcClass,this->m_NpcMap,this->m_NpcX,this->m_NpcY,this->m_NpcDir) == 0)
	{
		this->m_NpcIndex = -1;
		return;
	}

	this->m_NpcIndex = index;
	LogAdd(LOG_BLUE,"[ItemReroll] NPC class %d spawned at map %d (%d,%d), index %d",this->m_NpcClass,this->m_NpcMap,this->m_NpcX,this->m_NpcY,index);
}

bool CItemReroll::Dialog(LPOBJ lpObj,LPOBJ lpNpc) // OK
{
	if(this->m_Switch == 0 || lpNpc->Class != this->m_NpcClass || lpNpc->Map != this->m_NpcMap || lpNpc->X != this->m_NpcX || lpNpc->Y != this->m_NpcY)
	{
		return false;
	}

	if(lpObj->Type != OBJECT_USER || lpObj->Interface.use != 0 || lpObj->PShopOpen != 0)
	{
		return true;
	}

	RRLock lock(&this->m_cs);

	RR_SESSION* s = this->GetSession(lpObj);

	// an item saved here from an earlier visit (or earlier session) is still on the NPC: load it, the player can reroll or claim it
	if(this->m_Held.find(lpObj->Index) == this->m_Held.end())
	{
		this->LoadHeld(lpObj);
	}

	// a fresh window: nothing chosen, nothing rolled
	memset(s,0,sizeof(RR_SESSION));
	strncpy(s->Name,lpObj->Name,sizeof(s->Name)-1);
	s->Open = true;
	s->OpenTick = GetTickCount();
	s->NpcIndex = lpNpc->Index;

	std::map<int,RR_HELD>::iterator hd = this->m_Held.find(lpObj->Index);

	if(hd != this->m_Held.end())
	{
		s->HasItem = true;
		s->Slot = hd->second.Slot;
		s->Serial = hd->second.Item.m_Serial;
		s->ItemIndex = hd->second.Item.m_Index;
		s->Old = (BYTE)(hd->second.Item.m_NewOption & 0x3F);

		if(hd->second.HasPend != false)
		{
			s->HasRoll = true; // the roll paid for earlier is still there
			s->New = hd->second.PendNew;
			s->Locks = hd->second.PendLocks;
			s->RollTick = GetTickCount();
		}
	}

	this->SendOpen(lpObj);
	this->SendState(lpObj,s,RR_OK);
	return true;
}

//////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////

RR_SESSION* CItemReroll::GetSession(LPOBJ lpObj) // OK
{
	return &this->m_Sessions[lpObj->Index];
}

void CItemReroll::Log(LPOBJ lpObj,const char* format,...) // OK
{
	char buff[256] = {0};

	va_list arg;
	va_start(arg,format);
	vsprintf_s(buff,format,arg);
	va_end(arg);

	LogAdd(LOG_BLUE,"[ItemReroll][%s][%s] %s",lpObj->Account,lpObj->Name,buff);
	gLog.Output(LOG_WC,"[ItemReroll][%s][%s] %s",lpObj->Account,lpObj->Name,buff);
}

const BYTE* CItemReroll::GetWeights(int itemIndex) // OK
{
	int slot = gItemManager.GetItemSlotByIndex(itemIndex);

	if(slot == 7 || slot == 8){return this->m_RateWing;}
	if(slot == 9 || slot == 10 || slot == 11){return this->m_RateAccessory;}
	if(slot >= 0 && slot <= 1){return this->m_RateWeapon;}

	return this->m_RateArmor;
}

// Jewels in the bag plus (UseJewelBank = 1) the Jewel Bank.
int CItemReroll::CountJewels(LPOBJ lpObj) // OK
{
	int count = gItemManager.GetInventoryItemCount(lpObj,this->m_JewelItem,0);

#if(JEWELBANKVER2)
	if(this->m_UseJewelBank != 0)
	{
		count += gBCustomItemBank.CheckCountItemBank(lpObj->Index,this->m_JewelItem,0);
	}
#endif

	return count;
}

// Takes 'need' jewels: the bag first, the rest from the Jewel Bank. The bank part is taken first (it can be given back), then the bag
// part is verified; if that fails the bank part is returned. *ok = false means nothing was taken.
void CItemReroll::PayJewels(LPOBJ lpObj,int need,bool* ok) // OK
{
	*ok = false;

	if(need <= 0)
	{
		*ok = true;
		return;
	}

	int inBag = gItemManager.GetInventoryItemCount(lpObj,this->m_JewelItem,0);
	int fromBag = min(inBag,need);
	int fromBank = need-fromBag;

#if(JEWELBANKVER2)
	if(fromBank > 0)
	{
		if(this->m_UseJewelBank == 0 || gBCustomItemBank.CheckCountItemBank(lpObj->Index,this->m_JewelItem,0) < fromBank || gBCustomItemBank.CongTruBank(lpObj->Index,this->m_JewelItem,0,-fromBank,0) == 0)
		{
			return;
		}
	}
#else
	if(fromBank > 0)
	{
		return;
	}
#endif

	if(fromBag > 0)
	{
		gItemManager.DeleteInventoryItemCount(lpObj,this->m_JewelItem,0,fromBag);

		if(inBag-gItemManager.GetInventoryItemCount(lpObj,this->m_JewelItem,0) != fromBag)
		{
#if(JEWELBANKVER2)
			if(fromBank > 0){gBCustomItemBank.CongTruBank(lpObj->Index,this->m_JewelItem,0,fromBank,0);}
#endif
			return;
		}
	}

	*ok = true;
}

int CItemReroll::JewelCost(int locks) // OK
{
	int cost = this->m_JewelCount+this->m_JewelPerLock*locks;
	return ((cost > 1000)?1000:cost);
}

const BYTE* CItemReroll::GetMults(int itemIndex) // OK
{
	int slot = gItemManager.GetItemSlotByIndex(itemIndex);

	if(slot == 7 || slot == 8){return this->m_MultWing;}
	if(slot == 9 || slot == 10 || slot == 11){return this->m_MultAccessory;}
	if(slot >= 0 && slot <= 1){return this->m_MultWeapon;}

	return this->m_MultArmor;
}

// The price of the result: the base price times the sum of the multipliers of the options the new item gets.
int CItemReroll::Units(int itemIndex,BYTE result) // OK
{
	const BYTE* mult = this->GetMults(itemIndex);
	int units = 0;

	for(int b=0;b < 6;b++)
	{
		if((result & (1 << b)) != 0){units += mult[b];}
	}

	return units;
}

DWORD CItemReroll::PriceCoin(int pay,int locks,int units) // OK
{
	int base = (pay == 1)?this->m_PriceWC:this->m_PriceGP;
	int add = (pay == 1)?this->m_LockWC:this->m_LockGP;

	long long total = (long long)base*units+(long long)add*locks;

	return (DWORD)((total > 0x7FFFFFFF)?0x7FFFFFFF:total);
}

void CItemReroll::SendOpen(LPOBJ lpObj) // OK
{
	PMSG_RR_OPEN_SEND pMsg;

	pMsg.header.set(0xD3,0x68,sizeof(pMsg));

	pMsg.PayMask = (BYTE)(1 << (this->m_PayWith-1));
	pMsg.PriceJW = (WORD)((this->m_PriceJW < 0)?0:this->m_PriceJW);
	pMsg.LockJW = (WORD)this->m_LockJW;
	pMsg.Pad = 0;
	pMsg.JewelItem = (WORD)this->m_JewelItem;
	pMsg.JewelCount = (WORD)this->m_JewelCount;
	pMsg.JewelPerLock = (WORD)this->m_JewelPerLock;
	pMsg.SpinMs = (WORD)this->m_SpinMs;
	pMsg.JewelHave = (WORD)min(this->CountJewels(lpObj),65535);
	pMsg.PriceWC = (DWORD)this->m_PriceWC;
	pMsg.PriceGP = (DWORD)this->m_PriceGP;
	pMsg.LockWC = (DWORD)this->m_LockWC;
	pMsg.LockGP = (DWORD)this->m_LockGP;

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
}

void CItemReroll::SendState(LPOBJ lpObj,RR_SESSION* s,BYTE code) // OK
{
	PMSG_RR_STATE_SEND pMsg;

	pMsg.header.set(0xD3,0x69,sizeof(pMsg));

	pMsg.Code = code;
	pMsg.Stage = 0;
	pMsg.Slot = 0;
	pMsg.Valid = 0;
	pMsg.Old = 0;
	pMsg.New = 0;
	pMsg.Locks = 0;
	pMsg.Flags = (BYTE)((this->m_AllowSame == 0)?1:0);
	memset(pMsg.Weight,0,sizeof(pMsg.Weight));
	memset(pMsg.Mult,0,sizeof(pMsg.Mult));
	pMsg.LevelByte = 0;
	pMsg.ExtByte = 0;
	pMsg.ItemIndex = 0;
	pMsg.JewelHave = (WORD)min(this->CountJewels(lpObj),65535);

	if(s != 0 && s->HasItem != false)
	{
		BYTE valid = 0;
		BYTE old = 0;

		gItemOptionRate.MakeNewOption(s->ItemIndex,MAX_EXC_OPTION,&valid);

		pMsg.Stage = 1;
		pMsg.Slot = (BYTE)s->Slot;
		pMsg.Valid = (BYTE)(valid & 0x3F);
		pMsg.Old = s->Old;
		pMsg.ItemIndex = (WORD)s->ItemIndex;
		memcpy(pMsg.Weight,this->GetWeights(s->ItemIndex),6);
		memcpy(pMsg.Mult,this->GetMults(s->ItemIndex),6);

		std::map<int,RR_HELD>::iterator hd = this->m_Held.find(lpObj->Index);

		if(hd != this->m_Held.end())
		{
			CItem* it = &hd->second.Item;
			pMsg.LevelByte = (BYTE)((it->m_Level*8) | (it->m_Option1*128) | (it->m_Option2*4) | (it->m_Option3 & 3));
			pMsg.ExtByte = (BYTE)(it->m_SetOption);
		}
		(void)old;

		if(s->HasRoll != false)
		{
			pMsg.Stage = 2;
			pMsg.New = s->New;
			pMsg.Locks = s->Locks;
		}
	}

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
}

// The window must be open, belong to this player and the player must still stand next to the NPC.
bool CItemReroll::CheckWindow(LPOBJ lpObj,RR_SESSION* s,BYTE* code) // OK
{
	if(this->m_Switch == 0)
	{
		*code = RR_ERR_OFF;
		return false;
	}

	if(gObjIsConnectedGP(lpObj->Index) == 0 || lpObj->Type != OBJECT_USER)
	{
		*code = RR_ERR_FAR;
		return false;
	}

	if(s->Open == false || strcmp(s->Name,lpObj->Name) != 0)
	{
		*code = RR_ERR_FAR;
		return false;
	}

	if(lpObj->Map != this->m_NpcMap || abs(lpObj->X-this->m_NpcX) > this->m_MaxDistance || abs(lpObj->Y-this->m_NpcY) > this->m_MaxDistance)
	{
		s->Open = false;
		*code = RR_ERR_FAR;
		return false;
	}

	if(lpObj->Interface.use != 0 || lpObj->PShopOpen != 0 || lpObj->State != OBJECT_PLAYING || lpObj->Teleport != 0)
	{
		*code = RR_ERR_BUSY;
		return false;
	}

	return true;
}

// Is the item in this bag slot a rerollable excellent item? Returns RR_OK or the error code; fills the valid / current bits.
int CItemReroll::CheckItem(LPOBJ lpObj,int slot,BYTE* valid,BYTE* old) // OK
{
	if(INVENTORY_FULL_RANGE(slot) == 0 || slot < INVENTORY_WEAR_SIZE)
	{
		return RR_ERR_ITEM;
	}

	return this->CheckItemPtr(&lpObj->Inventory[slot],valid,old);
}

int CItemReroll::CheckItemPtr(CItem* lpItem,BYTE* valid,BYTE* old) // OK
{
	if(lpItem->IsItem() == 0)
	{
		return RR_ERR_ITEM;
	}

	for(int n=0;n < this->m_DenyCount;n++)
	{
		if(this->m_Deny[n] == lpItem->m_Index)
		{
			return RR_ERR_GROUP;
		}
	}

	int group = lpItem->m_Index/MAX_ITEM_TYPE;
	int itemSlot = lpItem->m_Slot;

	bool allowed = false;

	if(itemSlot == 7){allowed = (this->m_AllowWing != 0);}
	else if(itemSlot == 9 || itemSlot == 10 || itemSlot == 11){allowed = (this->m_AllowAccessory != 0);}
	else if(itemSlot == 8){allowed = (this->m_AllowPet != 0);}
	else if(group >= 0 && group <= 5){allowed = (this->m_AllowWeapon != 0);}
	else if(group == 6){allowed = (this->m_AllowShield != 0);}
	else if(group >= 7 && group <= 11){allowed = (this->m_AllowArmor != 0);}

	if(allowed == false)
	{
		return RR_ERR_GROUP;
	}

	if(lpItem->m_SetOption != 0 && this->m_AllowAncient == 0)
	{
		return RR_ERR_ANCIENT;
	}

	BYTE mask = (BYTE)(lpItem->m_NewOption & 0x3F);

	if(mask == 0)
	{
		return RR_ERR_NOEXC;
	}

	BYTE all = 0;

	// the bits this item type can carry (wings 3-4, Dinorant/Fenrir 3, the rest 6): same rule as the normal exc roll
	gItemOptionRate.MakeNewOption(lpItem->m_Index,MAX_EXC_OPTION,&all);
	all &= 0x3F;

	if(all == 0 || (mask & ~all) != 0)
	{
		return RR_ERR_ITEM;
	}

	*valid = all;
	*old = mask;
	return RR_OK;
}

// New option set: the locked bits stay, the rest is drawn (by the config weights) from the unlocked valid bits. With AllowSameOption = 0
// the options the item has now cannot come up again. Never the same set as before.
bool CItemReroll::Roll(const BYTE* weight,BYTE valid,BYTE old,BYTE locks,BYTE* out) // OK
{
	int validCount = BitCount(valid);
	int lockedCount = BitCount(locks);

	BYTE pool = (BYTE)(valid & ~locks);

	if(this->m_AllowSame == 0)
	{
		pool &= (BYTE)~old;
	}

	for(int b=0;b < 6;b++)
	{
		if(weight[b] == 0){pool &= (BYTE)~(1 << b);}
	}

	int poolCount = BitCount(pool);

	if(poolCount == 0)
	{
		return false;
	}

	int minC = max(this->m_MinOptions,lockedCount);
	int maxC = min(this->m_MaxOptions,validCount);

	if(this->m_KeepCount != 0)
	{
		// the item keeps its number of excellent options: 2 options in, 2 options out (locked ones count)
		minC = maxC = max(BitCount(old),lockedCount);
	}

	if(maxC < minC){maxC = minC;}
	if(maxC > validCount){maxC = validCount;}
	if(minC > maxC){minC = maxC;}

	for(int attempt=0;attempt < 60;attempt++)
	{
		int total = minC+(GetLargeRand()%(maxC-minC+1));
		int need = total-lockedCount;

		if(need > poolCount){need = poolCount;}
		if(need < 1 && lockedCount == 0){need = 1;}

		BYTE result = locks;
		BYTE left = pool;

		for(int k=0;k < need && left != 0;k++)
		{
			int sum = 0;

			for(int b=0;b < 6;b++)
			{
				if((left & (1 << b)) != 0){sum += weight[b];}
			}

			if(sum <= 0){break;}

			int pick = GetLargeRand()%sum;

			for(int b=0;b < 6;b++)
			{
				if((left & (1 << b)) != 0)
				{
					if(pick < weight[b])
					{
						result |= (BYTE)(1 << b);
						left &= (BYTE)~(1 << b);
						break;
					}

					pick -= weight[b];
				}
			}
		}

		if(result != 0 && result != old)
		{
			*out = result;
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////
// Client packets
//////////////////////////////////////////////////////////////////////

void CItemReroll::CGActionRecv(PMSG_RR_ACTION_RECV* lpMsg,int aIndex) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	RRLock lock(&this->m_cs);

	RR_SESSION* s = this->GetSession(lpObj);

	if(lpMsg->Action == 5) // window closed: the item STAYS saved on the NPC (claim it next time); the jewels of an unconfirmed roll are gone, it was told on the Reroll button
	{
		s->Open = false;
		s->HasRoll = false;
		return;
	}

	BYTE code = RR_OK;

	if(this->CheckWindow(lpObj,s,&code) == false)
	{
		s->HasRoll = false;
		this->SendState(lpObj,(s->Open != false)?s:0,code);
		return;
	}

	switch(lpMsg->Action)
	{
		case 0:
			this->DoChoose(lpObj,s,lpMsg->Slot);
			break;
		case 1:
			this->DoRoll(lpObj,s,lpMsg->Slot,lpMsg->Locks);
			break;
		case 2:
			this->DoConfirm(lpObj,s,lpMsg->Pay);
			break;
		case 3:
			s->HasRoll = false;
			{
				std::map<int,RR_HELD>::iterator dh = this->m_Held.find(lpObj->Index);

				if(dh != this->m_Held.end() && dh->second.HasPend != false)
				{
					dh->second.HasPend = false;
					this->SaveHeld(lpObj,&dh->second);
				}
			}
			this->SendState(lpObj,s,RR_OK);
			break;
		case 4:
			s->HasRoll = false;

			if(this->ReturnHeld(lpObj,false) == false)
			{
				this->SendState(lpObj,s,RR_ERR_NOSPACE); // the item stays on the NPC until there is room
				break;
			}

			s->HasItem = false;
			this->SendState(lpObj,s,RR_CLEARED);
			break;
		default:
			break;
	}
}

void CItemReroll::DoChoose(LPOBJ lpObj,RR_SESSION* s,int slot) // OK
{
	BYTE valid = 0;
	BYTE old = 0;

	s->HasRoll = false;

	int err = this->CheckItem(lpObj,slot,&valid,&old);

	if(err != RR_OK)
	{
		this->SendState(lpObj,s,(BYTE)err); // whatever was on the NPC before stays there
		return;
	}

	// a previous item goes back to the bag first (into the cell it came from when that is still free)
	if(this->ReturnHeld(lpObj,false) == false)
	{
		this->SendState(lpObj,s,RR_ERR_NOSPACE);
		return;
	}

	// take the item OUT of the bag and keep it here (the server owns it until it is given back)
	RR_HELD& h = this->m_Held[lpObj->Index];

	h.Item = lpObj->Inventory[slot];
	h.Slot = slot;
	h.HasPend = false;
	h.PendNew = 0;
	h.PendLocks = 0;
	strncpy(h.Name,lpObj->Name,sizeof(h.Name)-1);
	h.Name[sizeof(h.Name)-1] = 0;

	// the saved copy must be on disk BEFORE the item leaves the bag
	if(this->SaveHeld(lpObj,&h) == false)
	{
		this->m_Held.erase(lpObj->Index);
		this->SendState(lpObj,s,RR_ERR_INTERNAL);
		return;
	}

	s->HasItem = true;
	s->Slot = slot;
	s->Serial = h.Item.m_Serial;
	s->ItemIndex = h.Item.m_Index;
	s->Old = old;

	gItemManager.InventoryDelItem(lpObj->Index,slot);
	gItemManager.GCItemDeleteSend(lpObj->Index,slot,1);

	this->Log(lpObj,"placed %s slot %d serial %u (removed from the bag, held by the NPC)",gItemManager.GetItemName(s->ItemIndex),slot,s->Serial);

	this->SendState(lpObj,s,RR_ITEM_CHOSEN);
}

// ---- the held item is also kept in a file (Data\Custom\ItemRerollSaved\<account>_<name>.dat, the same 16-byte item format the
// inventory is saved in), so it survives logout, disconnects and restarts: the player claims it at the NPC.
#define RR_SAVE_MAGIC 0x32305252   // 'RR02' (24-byte 'RR01' records, without the pending roll, are still read)
#define RR_SAVE_MAGIC_OLD 0x31305252

bool CItemReroll::SavedPath(LPOBJ lpObj,char* out,int outSize) // OK
{
	char dir[MAX_PATH] = {0};

	strcpy_s(dir,gPath.GetFullPath("Custom\\ItemRerollSaved"));
	CreateDirectoryA(dir,0);

	char name[64] = {0};

	for(int n=0;n < 63;n++)
	{
		char c = lpObj->Account[n];

		if(c == 0){break;}

		name[n] = (isalnum((unsigned char)c) != 0)?c:'_';
	}

	strcat_s(name,"_");

	int len = (int)strlen(name);

	for(int n=0;n < 10 && len < 63;n++,len++)
	{
		char c = lpObj->Name[n];

		if(c == 0){break;}

		name[len] = (isalnum((unsigned char)c) != 0)?c:'_';
	}

	name[len] = 0;

	sprintf_s(out,outSize,"%s\\%s.dat",dir,name);
	return true;
}

bool CItemReroll::SaveHeld(LPOBJ lpObj,RR_HELD* h) // OK
{
	char path[MAX_PATH] = {0};

	this->SavedPath(lpObj,path,sizeof(path));

	BYTE rec[16+4+4+4];
	DWORD magic = RR_SAVE_MAGIC;

	memcpy(rec,&magic,4);
	gItemManager.DBItemByteConvert(rec+4,&h->Item);
	memcpy(rec+20,&h->Slot,4);
	rec[24] = (BYTE)((h->HasPend != false)?1:0);
	rec[25] = h->PendNew;
	rec[26] = h->PendLocks;
	rec[27] = 0;

	FILE* f = fopen(path,"wb");

	if(f == 0)
	{
		LogAdd(LOG_RED,"[ItemReroll] cannot write %s",path);
		return false;
	}

	bool ok = (fwrite(rec,1,sizeof(rec),f) == sizeof(rec));
	ok = (fflush(f) == 0) && ok;
	fclose(f);

	return ok;
}

bool CItemReroll::LoadHeld(LPOBJ lpObj) // OK
{
	char path[MAX_PATH] = {0};

	this->SavedPath(lpObj,path,sizeof(path));

	FILE* f = fopen(path,"rb");

	if(f == 0)
	{
		return false;
	}

	BYTE rec[16+4+4+4];
	memset(rec,0,sizeof(rec));
	size_t got = fread(rec,1,sizeof(rec),f);
	fclose(f);

	DWORD magic = 0;

	memcpy(&magic,rec,4);

	if(!((got == sizeof(rec) && magic == RR_SAVE_MAGIC) || (got == 24 && magic == RR_SAVE_MAGIC_OLD)))
	{
		LogAdd(LOG_RED,"[ItemReroll] %s is not a valid saved item",path);
		return false;
	}

	RR_HELD h;

	if(gItemManager.ConvertItemByte(&h.Item,rec+4) == 0)
	{
		return false;
	}

	memcpy(&h.Slot,rec+20,4);
	h.HasPend = (magic == RR_SAVE_MAGIC && rec[24] == 1);
	h.PendNew = (h.HasPend != false)?rec[25]:0;
	h.PendLocks = (h.HasPend != false)?rec[26]:0;
	strncpy(h.Name,lpObj->Name,sizeof(h.Name)-1);
	h.Name[sizeof(h.Name)-1] = 0;

	this->m_Held[lpObj->Index] = h;

	this->Log(lpObj,"saved item loaded: %s serial %u",gItemManager.GetItemName(h.Item.m_Index),h.Item.m_Serial);
	return true;
}

void CItemReroll::DeleteSaved(LPOBJ lpObj) // OK
{
	char path[MAX_PATH] = {0};

	this->SavedPath(lpObj,path,sizeof(path));
	DeleteFileA(path);
}

// Gives the held item back to the player: the cell it came from if free, else the first free place in the bag.
// false = no room (the item stays held). dropIfFull: last resort on logout, the item is dropped at the player's feet.
bool CItemReroll::ReturnHeld(LPOBJ lpObj,bool dropIfFull) // OK
{
	std::map<int,RR_HELD>::iterator it = this->m_Held.find(lpObj->Index);

	if(it == this->m_Held.end())
	{
		return true;
	}

	if(strcmp(it->second.Name,lpObj->Name) != 0)
	{
		LogAdd(LOG_RED,"[ItemReroll] held item of %s found on the object of %s - dropped from the list",it->second.Name,lpObj->Name);
		this->m_Held.erase(it);
		return true;
	}

	CItem item = it->second.Item;

	// Room check first: the cell it came from only if that cell is inside the bag the player has open (the extended pages count only when
	// they are unlocked), otherwise the first free place of the bag + opened extensions. Nothing free = it cannot be claimed.
	BYTE slot = 0xFF;

	if(it->second.Slot >= INVENTORY_WEAR_SIZE && it->second.Slot < gItemManager.GetInventoryMaxValue(lpObj))
	{
		slot = gItemManager.InventoryAddItem(lpObj->Index,item,it->second.Slot);
	}

	if(slot == 0xFF && gItemManager.CheckItemInventorySpace(lpObj,item.m_Index) != 0)
	{
		slot = gItemManager.InventoryInsertItem(lpObj->Index,item);
	}

	if(slot != 0xFF)
	{
		gItemManager.GCItemModifySend(lpObj->Index,slot);
		this->Log(lpObj,"returned %s serial %u to bag slot %d",gItemManager.GetItemName(item.m_Index),item.m_Serial,(int)slot);
		this->DeleteSaved(lpObj); // the item is in the bag now: the saved copy goes (after the insert, so a crash can only duplicate, never lose)
		this->m_Held.erase(it);
		return true;
	}

	if(dropIfFull != false)
	{
		BYTE socket[MAX_SOCKET_OPTION];
		memcpy(socket,item.m_SocketOption,sizeof(socket));
		GDCreateItemSend(lpObj->Index,(BYTE)lpObj->Map,(BYTE)lpObj->X,(BYTE)lpObj->Y,item.m_Index,(BYTE)item.m_Level,(BYTE)item.m_Durability,item.m_Option1,item.m_Option2,item.m_Option3,lpObj->Index,item.m_NewOption,item.m_SetOption,item.m_JewelOfHarmonyOption,item.m_ItemOptionEx,socket,item.m_SocketOptionBonus,0);
		this->Log(lpObj,"bag full at logout: dropped %s (was serial %u) on the ground",gItemManager.GetItemName(item.m_Index),item.m_Serial);
		this->DeleteSaved(lpObj);
		this->m_Held.erase(it);
		return true;
	}

	return false;
}

void CItemReroll::OnUserLogout(LPOBJ lpObj) // OK
{
	RRLock lock(&this->m_cs);

	this->m_Held.erase(lpObj->Index); // the item stays saved in its file; the player claims it at the NPC

	std::map<int,RR_SESSION>::iterator it = this->m_Sessions.find(lpObj->Index);

	if(it != this->m_Sessions.end())
	{
		this->m_Sessions.erase(it);
	}
}

void CItemReroll::DoRoll(LPOBJ lpObj,RR_SESSION* s,int slot,BYTE locks) // OK
{
	if(s->HasItem == false)
	{
		this->SendState(lpObj,s,RR_ERR_NOITEM);
		return;
	}

	s->HasRoll = false; // a new roll replaces the old one in every case

	BYTE valid = 0;
	BYTE old = 0;

	// the item must be the one that was placed, unchanged (the NPC holds it)
	std::map<int,RR_HELD>::iterator hit = this->m_Held.find(lpObj->Index);

	int err = (slot == s->Slot && hit != this->m_Held.end())?this->CheckItemPtr(&hit->second.Item,&valid,&old):RR_ERR_CHANGED;

	if(err == RR_OK && (hit->second.Item.m_Serial != s->Serial || hit->second.Item.m_Index != s->ItemIndex || old != s->Old))
	{
		err = RR_ERR_CHANGED;
	}

	if(err != RR_OK)
	{
		this->ReturnHeld(lpObj,false);
		s->HasItem = false;
		this->SendState(lpObj,s,(BYTE)err);
		return;
	}

	// locks: only options the item has now
	if((locks & ~old) != 0 || (locks & ~valid) != 0)
	{
		this->SendState(lpObj,s,RR_ERR_LOCKS);
		return;
	}

	int lockCount = BitCount(locks);
	int need = this->JewelCost(lockCount);

	if(this->CountJewels(lpObj) < need)
	{
		this->SendState(lpObj,s,RR_ERR_JEWEL);
		return;
	}

	BYTE rolled = 0;

	if(this->Roll(this->GetWeights(s->ItemIndex),valid,old,locks,&rolled) == false)
	{
		this->SendState(lpObj,s,(valid == locks)?RR_ERR_LOCKS:RR_ERR_NODIFF);
		return;
	}

	// pay the jewels (bag first, then the Jewel Bank); nothing else is touched. Verified by the count before / after.
	int before = this->CountJewels(lpObj);

	bool paid = false;

	this->PayJewels(lpObj,need,&paid);

	int after = this->CountJewels(lpObj);

	if(paid == false || before-after != need)
	{
		this->Log(lpObj,"ERROR jewel payment %d -> %d (expected -%d, paid %d)",before,after,need,(int)paid);
		s->HasRoll = false;
		this->SendState(lpObj,s,RR_ERR_INTERNAL);
		return;
	}

	s->HasRoll = true;
	s->New = rolled;
	s->Locks = locks;
	s->RollTick = GetTickCount();

	// the paid roll is saved with the item: closing the window or logging out does not lose it
	hit->second.HasPend = true;
	hit->second.PendNew = rolled;
	hit->second.PendLocks = locks;
	this->SaveHeld(lpObj,&hit->second);

	this->Log(lpObj,"rolled %s slot %d serial %u: %02X -> %02X (locks %02X), jewels paid %d",gItemManager.GetItemName(s->ItemIndex),s->Slot,s->Serial,old,rolled,locks,need);

	this->SendState(lpObj,s,RR_ROLLED);
}

void CItemReroll::DoConfirm(LPOBJ lpObj,RR_SESSION* s,int pay) // OK
{
	if(s->HasItem == false)
	{
		this->SendState(lpObj,s,RR_ERR_NOITEM);
		return;
	}

	if(s->HasRoll == false)
	{
		this->SendState(lpObj,s,RR_ERR_NOROLL);
		return;
	}

	// take the roll out first: a second confirm (replayed packet, double click) finds nothing
	RR_SESSION r = *s;
	s->HasRoll = false;

	if((GetTickCount()-r.RollTick) > (DWORD)this->m_PendingSeconds*1000)
	{
		std::map<int,RR_HELD>::iterator eh = this->m_Held.find(lpObj->Index);

		if(eh != this->m_Held.end() && eh->second.HasPend != false)
		{
			eh->second.HasPend = false;
			this->SaveHeld(lpObj,&eh->second);
		}

		this->SendState(lpObj,s,RR_ERR_EXPIRED);
		return;
	}

	BYTE valid = 0;
	BYTE old = 0;

	std::map<int,RR_HELD>::iterator hit = this->m_Held.find(lpObj->Index);

	int err = (hit != this->m_Held.end())?this->CheckItemPtr(&hit->second.Item,&valid,&old):RR_ERR_CHANGED;

	if(err == RR_OK && (hit->second.Item.m_Serial != r.Serial || hit->second.Item.m_Index != r.ItemIndex || old != r.Old))
	{
		err = RR_ERR_CHANGED;
	}

	if(err == RR_OK && ((r.New & ~valid) != 0 || (r.Locks & ~old) != 0 || (r.New & r.Locks) != r.Locks || r.New == old || r.New == 0))
	{
		err = RR_ERR_CHANGED;
	}

	if(err != RR_OK)
	{
		this->ReturnHeld(lpObj,false);
		s->HasItem = false;
		this->Log(lpObj,"confirm refused (%d): item changed after the roll",err);
		this->SendState(lpObj,s,(BYTE)err);
		return;
	}

	// payment method: 1 WCoin, 2 Goblin Points, 3 jewels - each only when the config accepts it
	pay = this->m_PayWith; // the config has ONE payment method; whatever the client sent is ignored

	int units = this->Units(r.ItemIndex,r.New);

	int lockCount = BitCount(r.Locks);
	DWORD price = 0;

	if(pay == 3)
	{
		price = (DWORD)min(this->m_PriceJW*units+this->m_LockJW*lockCount,100000);

		if(this->CountJewels(lpObj) < (int)price)
		{
			s->HasRoll = true;
			this->SendState(lpObj,s,RR_ERR_JEWEL);
			return;
		}

		int before = this->CountJewels(lpObj);
		bool paid = false;

		this->PayJewels(lpObj,(int)price,&paid);

		if(paid == false || before-this->CountJewels(lpObj) != (int)price)
		{
			this->Log(lpObj,"ERROR jewel payment for the confirm (price %u, paid %d)",price,(int)paid);
			this->SendState(lpObj,s,RR_ERR_INTERNAL);
			return;
		}
	}
	else
	{
		price = this->PriceCoin(pay,lockCount,units);

		int have = (pay == 1)?lpObj->Coin1:lpObj->Coin3;

		if(have < 0 || (DWORD)have < price)
		{
			s->HasRoll = true;
			this->SendState(lpObj,s,RR_ERR_COIN);
			return;
		}

		int c1 = lpObj->Coin1;
		int c2 = lpObj->Coin2;
		int c3 = lpObj->Coin3;

		if(price > 0)
		{
			GDSetCoinSend(lpObj->Index,(pay == 1)?-(int)price:0,0,(pay == 2)?-(int)price:0,"ItemReroll");

			// GDSetCoinSend silently does nothing on an overflow; verify the balance really dropped by the price
			int d1 = c1-lpObj->Coin1;
			int d2 = c2-lpObj->Coin2;
			int d3 = c3-lpObj->Coin3;

			bool ok = (d2 == 0 && d1 == ((pay == 1)?(int)price:0) && d3 == ((pay == 2)?(int)price:0));

			if(ok == false)
			{
				this->Log(lpObj,"ERROR coin payment: delta %d/%d/%d for price %u (pay %d)",d1,d2,d3,price,pay);

				if(d1 != 0 || d2 != 0 || d3 != 0)
				{
					GDSetCoinSend(lpObj->Index,d1,d2,d3,"ItemRerollRefund");
				}

				gCashShop.CGCashShopPointRecv(lpObj->Index);
				this->SendState(lpObj,s,RR_ERR_INTERNAL);
				return;
			}
		}
	}

	// apply: the same item, the new option bits; everything else (level, luck, skill, durability, sockets) is kept
	CItem* lpItem = &hit->second.Item;

	BYTE newOption = (BYTE)((lpItem->m_NewOption & ~0x3F) | r.New);

	lpItem->Convert(lpItem->m_Index,lpItem->m_Option1,lpItem->m_Option2,lpItem->m_Option3,newOption,lpItem->m_SetOption,lpItem->m_JewelOfHarmonyOption,lpItem->m_ItemOptionEx,lpItem->m_SocketOption,lpItem->m_SocketOptionBonus);

	gCashShop.CGCashShopPointRecv(lpObj->Index);

	this->Log(lpObj,"APPLIED %s slot %d serial %u: %02X -> %02X (locks %02X), paid %u %s",gItemManager.GetItemName(r.ItemIndex),r.Slot,r.Serial,r.Old,r.New,r.Locks,price,(pay == 1)?"WCoin":((pay == 2)?"Goblin Points":"jewels"));

	hit->second.HasPend = false; // the roll is used up

	// the item (new options) stays saved on the NPC: written to its file now, the player claims it (Claim button) - now or any time later
	if(this->SaveHeld(lpObj,&hit->second) == false)
	{
		this->Log(lpObj,"ERROR could not save the rerolled item of serial %u (it is still held in memory)",r.Serial);
	}

	GDCharacterInfoSaveSend(lpObj->Index);

	s->Old = r.New;
	s->HasRoll = false;
	this->SendState(lpObj,s,RR_APPLIED);
}
