// CustomMining.cpp - "Tarkan Mining" event. See CustomMining.h. Config: Data\Custom\CustomMining.txt.
#include "stdafx.h"
#include "CustomMining.h"
#include "BCustomItemBank.h"
#include "DSProtocol.h"
#include "GameMaster.h"
#include "ItemManager.h"
#include "JewelMix.h"
#include "Map.h"
#include "MapServerManager.h"
#include "Monster.h"
#include "Notice.h"
#include "ServerDisplayer.h"
#include "ServerInfo.h"
#include "User.h"
#include "Util.h"
#include "Log.h"
#include <algorithm>

CCustomMining gCustomMining;

namespace
{
	struct CsLock
	{
		CRITICAL_SECTION* m_cs;
		CsLock(CRITICAL_SECTION* cs) : m_cs(cs) { EnterCriticalSection(m_cs); }
		~CsLock() { LeaveCriticalSection(m_cs); }
	};

	void CopyText(char* dest,int destSize,const char* src)
	{
		int n = 0;

		if(src != 0)
		{
			for(;n < destSize-1 && src[n] != 0;n++)
			{
				dest[n] = src[n];
			}
		}

		dest[n] = 0;
	}

	char* Trim(char* text)
	{
		while(*text == ' ' || *text == '\t')
		{
			text++;
		}

		int len = (int)strlen(text);

		while(len > 0 && (text[len-1] == ' ' || text[len-1] == '\t' || text[len-1] == '\r' || text[len-1] == '\n'))
		{
			text[--len] = 0;
		}

		return text;
	}

	// "47.5, 49.16" -> {4750, 4916}; returns the count
	int ParsePercents(const char* text,int* out,int max)
	{
		int count = 0;

		while(*text != 0 && count < max)
		{
			while(*text == ' ' || *text == ',' || *text == '\t')
			{
				text++;
			}

			if(*text == 0)
			{
				break;
			}

			char* end = 0;
			double value = strtod(text,&end);

			if(end == text)
			{
				break;
			}

			out[count++] = (int)(value*100.0+0.5);
			text = end;
		}

		return count;
	}

	const char* TIER_NAMES[CM_MAX_TIERS] = {"Blue","Purple","Gold"};
}

CCustomMining::CCustomMining() // OK
{
	InitializeCriticalSection(&this->m_cs);

	this->m_SecTick = 0;
	this->m_State = CM_STATE_OFF;
	this->m_NextStart = 0;
	this->m_EventStart = 0;
	this->m_EventEnd = 0;
	this->m_LastPhase = 0;
	this->m_OneMinDone = false;

	for(int n=0;n < CM_MAX_TIERS;n++)
	{
		this->m_Tier[n].NextSpawn = 0;
	}

	this->ResetStats();
	this->SetDefaults();
}

CCustomMining::~CCustomMining() // OK
{
	DeleteCriticalSection(&this->m_cs);
}

//////////////////////////////////////////////////////////////////////
// Config
//////////////////////////////////////////////////////////////////////

void CCustomMining::SetDefaults() // OK
{
	this->m_Switch = 0;
	this->m_ServerFilter = -1;
	this->m_Map = 8;
	this->m_Duration = 300;
	this->m_StartCount = 0;
	this->m_AreaX = 199;
	this->m_AreaY = 58;
	this->m_AreaRange = 18;
	this->m_MinGap = 4;
	this->m_MaxStones = 20;
	this->m_MaxDistance = 6;
	this->m_AttemptMs = 3600;
	this->m_PickAxe = GET_ITEM(1,10);
	this->m_UseJewelBank = 1;
	this->m_IdleCashoutSec = 120;

	this->m_JewelCount = 5;
	this->m_JewelItem[0] = GET_ITEM(14,13); this->m_JewelWeight[0] = 20; // Bless
	this->m_JewelItem[1] = GET_ITEM(14,14); this->m_JewelWeight[1] = 20; // Soul
	this->m_JewelItem[2] = GET_ITEM(14,22); this->m_JewelWeight[2] = 20; // Creation
	this->m_JewelItem[3] = GET_ITEM(12,15); this->m_JewelWeight[3] = 20; // Chaos
	this->m_JewelItem[4] = GET_ITEM(14,41); this->m_JewelWeight[4] = 20; // Gemstone

	static const int classes[CM_MAX_TIERS] = {616,617,618};
	static const int alive[CM_MAX_TIERS] = {20,20,20};
	static const int count[CM_MAX_TIERS][3] = {{5,1,1},{0,3,1},{0,0,1}};
	static const int interval[CM_MAX_TIERS] = {15,20,30};
	static const int dur[CM_MAX_TIERS] = {2,4,8};
	static const int from[CM_MAX_TIERS] = {0,120,240};
	static const int to[CM_MAX_TIERS] = {300,300,300};
	static const int chance[CM_MAX_TIERS][5] = {{4750,4916,4964,4672,4751},{4750,4916,4633,4691,4766},{4750,4916,4793,4754,4753}};
	static const int total[CM_MAX_TIERS][5] = {{1,3,7,16,33},{2,6,15,34,67},{4,12,29,65,125}};

	for(int n=0;n < CM_MAX_TIERS;n++)
	{
		CM_TIER* t = &this->m_Tier[n];

		t->Class = classes[n];
		t->MaxAlive = alive[n];
		t->SpawnIntervalSec = interval[n];
		for(int k=0;k < 3;k++){t->SpawnCount[k] = count[n][k];}
		t->DurCost = dur[n];
		t->PhaseFrom = from[n];
		t->PhaseTo = to[n];
		t->Steps = 5;

		memset(t->Chance,0,sizeof(t->Chance));
		memset(t->Total,0,sizeof(t->Total));

		for(int i=0;i < 5;i++)
		{
			t->Chance[i] = chance[n][i];
			t->Total[i] = total[n][i];
		}
	}

	CopyText(this->m_TextStart,sizeof(this->m_TextStart),"[Tarkan Mining] The Tarkan Mining event is starting! Equip a Pick Axe and mine the stones on Tarkan.");
	CopyText(this->m_TextBlue,sizeof(this->m_TextBlue),"[Tarkan Mining] Blue Mining Stones are appearing on Tarkan!");
	CopyText(this->m_TextPurple,sizeof(this->m_TextPurple),"[Tarkan Mining] Purple Mining Stones are appearing on Tarkan!");
	CopyText(this->m_TextGold,sizeof(this->m_TextGold),"[Tarkan Mining] Gold Mining Stones are appearing on Tarkan! Last minute!");
	CopyText(this->m_TextOneMin,sizeof(this->m_TextOneMin),"[Tarkan Mining] 1 minute left!");
	CopyText(this->m_TextEnd,sizeof(this->m_TextEnd),"[Tarkan Mining] The Tarkan Mining event has ended.");
}

int CCustomMining::ParseInts(const char* text,int* out,int max) // OK
{
	int count = 0;

	while(*text != 0 && count < max)
	{
		while(*text == ' ' || *text == ',' || *text == '\t')
		{
			text++;
		}

		if(*text == 0)
		{
			break;
		}

		char* end = 0;
		long value = strtol(text,&end,10);

		if(end == text)
		{
			break;
		}

		out[count++] = (int)value;
		text = end;
	}

	return count;
}

void CCustomMining::Load(char* path) // OK
{
	CsLock lock(&this->m_cs);

	DWORD nextSpawn[CM_MAX_TIERS];

	for(int n=0;n < CM_MAX_TIERS;n++)
	{
		nextSpawn[n] = this->m_Tier[n].NextSpawn;
	}

	// defaults first, so a key removed from the file goes back to its default
	this->SetDefaults();

	FILE* file = 0;

	if(fopen_s(&file,path,"r") != 0 || file == 0)
	{
		LogAdd(LOG_RED,"[CustomMining] %s not found: event off.",path);
		this->m_Switch = 0;
		return;
	}

	int jewelLines = 0;
	char line[512];

	while(fgets(line,sizeof(line),file) != 0)
	{
		char* comment = strstr(line,"//");

		if(comment != 0)
		{
			*comment = 0;
		}

		char* hash = strchr(line,'#');

		if(hash != 0)
		{
			*hash = 0;
		}

		char* eq = strchr(line,'=');

		if(eq == 0)
		{
			continue;
		}

		*eq = 0;

		char* key = Trim(line);
		char* value = Trim(eq+1);

		int v[16];
		int count = 0;

		if(_stricmp(key,"Switch") == 0){this->m_Switch = atoi(value);}
		else if(_stricmp(key,"ServerCode") == 0){this->m_ServerFilter = atoi(value);}
		else if(_stricmp(key,"Map") == 0){this->m_Map = atoi(value);}
		else if(_stricmp(key,"DurationSec") == 0){this->m_Duration = max(atoi(value),30);}
		else if(_stricmp(key,"MinGap") == 0){this->m_MinGap = max(atoi(value),0);}
		else if(_stricmp(key,"MaxStones") == 0){this->m_MaxStones = max(atoi(value),1);}
		else if(_stricmp(key,"MaxDistance") == 0){this->m_MaxDistance = max(atoi(value),1);}
		else if(_stricmp(key,"AttemptMs") == 0){this->m_AttemptMs = min(max(atoi(value),500),30000);}
		else if(_stricmp(key,"UseJewelBank") == 0){this->m_UseJewelBank = atoi(value);}
		else if(_stricmp(key,"IdleCashoutSec") == 0){this->m_IdleCashoutSec = max(atoi(value),0);}
		else if(_stricmp(key,"Start") == 0)
		{
			// Start = Day,Hour,Minute   (Day 0 = Sunday .. 6 = Saturday, * = every day)
			char text[64];
			CopyText(text,sizeof(text),value);

			int day = -1;
			char* p = text;

			if(*p == '*')
			{
				p++;
			}
			else
			{
				day = atoi(p);
			}

			char* comma = strchr(p,',');

			if(comma != 0 && this->m_StartCount < CM_MAX_START)
			{
				count = this->ParseInts(comma+1,v,2);

				if(count == 2 && v[0] >= 0 && v[0] < 24 && v[1] >= 0 && v[1] < 60 && day >= -1 && day <= 6)
				{
					this->m_StartDay[this->m_StartCount] = day;
					this->m_StartHour[this->m_StartCount] = v[0];
					this->m_StartMinute[this->m_StartCount] = v[1];
					this->m_StartCount++;
				}
			}
		}
		else if(_stricmp(key,"SpawnArea") == 0)
		{
			if(this->ParseInts(value,v,3) == 3){this->m_AreaX = v[0];this->m_AreaY = v[1];this->m_AreaRange = max(v[2],1);}
		}
		else if(_stricmp(key,"PickAxe") == 0)
		{
			if(this->ParseInts(value,v,2) == 2){this->m_PickAxe = GET_ITEM(v[0],v[1]);}
		}
		else if(_stricmp(key,"JewelWeight") == 0)
		{
			if(jewelLines == 0)
			{
				this->m_JewelCount = 0; // the file replaces the default list
			}

			jewelLines++;

			if(this->ParseInts(value,v,3) == 3 && this->m_JewelCount < CM_MAX_JEWELS)
			{
				this->m_JewelItem[this->m_JewelCount] = GET_ITEM(v[0],v[1]);
				this->m_JewelWeight[this->m_JewelCount] = max(v[2],0);
				this->m_JewelCount++;
			}
		}
		else if(_stricmp(key,"TextStart") == 0){CopyText(this->m_TextStart,sizeof(this->m_TextStart),value);}
		else if(_stricmp(key,"TextBlue") == 0){CopyText(this->m_TextBlue,sizeof(this->m_TextBlue),value);}
		else if(_stricmp(key,"TextPurple") == 0){CopyText(this->m_TextPurple,sizeof(this->m_TextPurple),value);}
		else if(_stricmp(key,"TextGold") == 0){CopyText(this->m_TextGold,sizeof(this->m_TextGold),value);}
		else if(_stricmp(key,"TextOneMinute") == 0){CopyText(this->m_TextOneMin,sizeof(this->m_TextOneMin),value);}
		else if(_stricmp(key,"TextEnd") == 0){CopyText(this->m_TextEnd,sizeof(this->m_TextEnd),value);}
		else
		{
			// per colour keys: BlueClass, PurpleChance, GoldJewels ...
			for(int n=0;n < CM_MAX_TIERS;n++)
			{
				size_t len = strlen(TIER_NAMES[n]);

				if(_strnicmp(key,TIER_NAMES[n],len) != 0)
				{
					continue;
				}

				const char* sub = key+len;
				CM_TIER* t = &this->m_Tier[n];

				if(_stricmp(sub,"Class") == 0){t->Class = atoi(value);}
				else if(_stricmp(sub,"MaxAlive") == 0){t->MaxAlive = max(atoi(value),0);}
				else if(_stricmp(sub,"SpawnIntervalSec") == 0){t->SpawnIntervalSec = max(atoi(value),1);}
				else if(_stricmp(sub,"SpawnCount") == 0){memset(t->SpawnCount,0,sizeof(t->SpawnCount));int c = this->ParseInts(value,v,3);for(int k=0;k < c;k++){t->SpawnCount[k] = max(v[k],0);}}
				else if(_stricmp(sub,"DurabilityCost") == 0){t->DurCost = min(max(atoi(value),0),255);}
				else if(_stricmp(sub,"Phase") == 0){if(this->ParseInts(value,v,2) == 2){t->PhaseFrom = v[0];t->PhaseTo = v[1];}}
				else if(_stricmp(sub,"Chance") == 0)
				{
					memset(t->Chance,0,sizeof(t->Chance));
					ParsePercents(value,t->Chance,CM_MAX_STEPS); // the step count is worked out below
				}
				else if(_stricmp(sub,"Jewels") == 0)
				{
					memset(t->Total,0,sizeof(t->Total));
					this->ParseInts(value,t->Total,CM_MAX_STEPS);
				}

				break;
			}
		}
	}

	fclose(file);

	// the number of steps = the shorter of the Chance / Jewels lists
	for(int n=0;n < CM_MAX_TIERS;n++)
	{
		CM_TIER* t = &this->m_Tier[n];

		int steps = 0;

		while(steps < CM_MAX_STEPS && t->Chance[steps] > 0 && t->Total[steps] > 0)
		{
			steps++;
		}

		t->Steps = steps;
		t->NextSpawn = nextSpawn[n];
	}

	if(this->m_State == CM_STATE_IDLE || this->m_State == CM_STATE_OFF)
	{
		this->SetNextStart();
	}

	LogAdd(LOG_BLUE,"[CustomMining] loaded: switch %d, map %d, %d start time(s), %d s, blue %d/%d/%d steps, jewel bank %d",this->m_Switch,this->m_Map,this->m_StartCount,this->m_Duration,this->m_Tier[0].Steps,this->m_Tier[1].Steps,this->m_Tier[2].Steps,this->m_UseJewelBank);
}

bool CCustomMining::Served() // OK
{
	if(this->m_ServerFilter != -1 && this->m_ServerFilter != gServerInfo.m_ServerCode)
	{
		return false;
	}

	return (gMapServerManager.CheckMapServer(this->m_Map) != 0);
}

//////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////

void CCustomMining::Announce(const char* format,...) // OK
{
	char buff[256] = {0};

	va_list arg;
	va_start(arg,format);
	vsprintf_s(buff,format,arg);
	va_end(arg);

	gNotice.GCNoticeSendToAll(0,0,0,0,0,0,"%s",buff);
}

void CCustomMining::Notify(LPOBJ lpObj,const char* format,...) // OK
{
	char buff[256] = {0};

	va_list arg;
	va_start(arg,format);
	vsprintf_s(buff,format,arg);
	va_end(arg);

	gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"%s",buff);
}

int CCustomMining::TierOfClass(int cls) // OK
{
	for(int n=0;n < CM_MAX_TIERS;n++)
	{
		if(this->m_Tier[n].Class == cls)
		{
			return n;
		}
	}

	return -1;
}

int CCustomMining::TierByName(const char* name) // OK
{
	for(int n=0;n < CM_MAX_TIERS;n++)
	{
		if(_stricmp(name,TIER_NAMES[n]) == 0)
		{
			return n;
		}
	}

	return -1;
}

const char* CCustomMining::JewelName(int item) // OK
{
	if(item == GET_ITEM(14,13)){return "Jewel of Bless";}
	if(item == GET_ITEM(14,14)){return "Jewel of Soul";}
	if(item == GET_ITEM(14,22)){return "Jewel of Creation";}
	if(item == GET_ITEM(12,15)){return "Jewel of Chaos";}
	if(item == GET_ITEM(14,41)){return "Gemstone";}

	return "jewel";
}

int CCustomMining::RollJewel() // OK
{
	int total = 0;

	for(int n=0;n < this->m_JewelCount;n++)
	{
		total += this->m_JewelWeight[n];
	}

	if(total <= 0)
	{
		return GET_ITEM(14,13);
	}

	int roll = GetLargeRand()%total;

	for(int n=0;n < this->m_JewelCount;n++)
	{
		if(roll < this->m_JewelWeight[n])
		{
			return this->m_JewelItem[n];
		}

		roll -= this->m_JewelWeight[n];
	}

	return this->m_JewelItem[0];
}

void CCustomMining::SetNextStart() // OK
{
	this->m_NextStart = 0;

	if(this->m_Switch == 0 || this->m_StartCount == 0)
	{
		return;
	}

	time_t now = time(0);
	time_t best = 0;

	for(int n=0;n < this->m_StartCount;n++)
	{
		struct tm t;
		localtime_s(&t,&now);

		t.tm_hour = this->m_StartHour[n];
		t.tm_min = this->m_StartMinute[n];
		t.tm_sec = 0;
		t.tm_isdst = -1;

		if(this->m_StartDay[n] >= 0)
		{
			t.tm_mday += (this->m_StartDay[n]-t.tm_wday+7)%7;
		}

		time_t next = mktime(&t);

		if(next <= now)
		{
			t.tm_mday += ((this->m_StartDay[n] >= 0)?7:1);
			t.tm_isdst = -1;
			next = mktime(&t);
		}

		if(best == 0 || next < best)
		{
			best = next;
		}
	}

	this->m_NextStart = best;
}

//////////////////////////////////////////////////////////////////////
// Stones
//////////////////////////////////////////////////////////////////////

bool CCustomMining::IsStoneValid(CM_STONE* s) // OK
{
	return (gObjIsConnected(s->Index) != 0 && gObj[s->Index].Class == this->m_Tier[s->Tier].Class && gObj[s->Index].Map == this->m_Map);
}

CM_STONE* CCustomMining::FindStone(int index) // OK
{
	for(size_t n=0;n < this->m_Stones.size();n++)
	{
		if(this->m_Stones[n].Index == index)
		{
			return &this->m_Stones[n];
		}
	}

	return 0;
}

int CCustomMining::AliveCount(int tier) // OK
{
	int count = 0;

	for(size_t n=0;n < this->m_Stones.size();)
	{
		CM_STONE* s = &this->m_Stones[n];

		if(this->IsStoneValid(s) == false)
		{
			this->m_Stones.erase(this->m_Stones.begin()+n); // gone (deleted by its timer, or the slot was reused)
			continue;
		}

		if(s->Tier == tier)
		{
			count++;
		}

		n++;
	}

	return count;
}

bool CCustomMining::SpawnStone(int tier) // OK
{
	CM_TIER* t = &this->m_Tier[tier];

	int x = 0;
	int y = 0;
	bool found = false;

	for(int attempt=0;attempt < 100 && found == false;attempt++)
	{
		x = this->m_AreaX+(int)(GetLargeRand()%(this->m_AreaRange*2+1))-this->m_AreaRange;
		y = this->m_AreaY+(int)(GetLargeRand()%(this->m_AreaRange*2+1))-this->m_AreaRange;

		// the stone must stand on walkable ground: attribute bits 4 (blocked) and 8 (no stand) clear on the tile and its neighbours.
		// The safe zone bit (1) is fine - Tarkan's spawn area is inside one.
		bool walkable = true;

		for(int dy=-1;dy <= 1 && walkable;dy++)
		{
			for(int dx=-1;dx <= 1;dx++)
			{
				if(gMap[this->m_Map].CheckAttr(x+dx,y+dy,12) != 0)
				{
					walkable = false;
					break;
				}
			}
		}

		if(walkable == false)
		{
			continue;
		}

		bool ok = true;

		for(size_t n=0;n < this->m_Stones.size();n++)
		{
			if(max(abs(this->m_Stones[n].X-x),abs(this->m_Stones[n].Y-y)) < this->m_MinGap)
			{
				ok = false;
				break;
			}
		}

		found = ok;
	}

	if(found == false)
	{
		LogAdd(LOG_RED,"[CustomMining] spawn failed: no free tile around %d,%d range %d on map %d",this->m_AreaX,this->m_AreaY,this->m_AreaRange,this->m_Map);
		return false;
	}

	int index = gObjAddMonster(this->m_Map);

	if(OBJECT_RANGE(index) == 0)
	{
		LogAdd(LOG_RED,"[CustomMining] spawn failed: gObjAddMonster(map %d) gave no slot (is this GameServer serving the map?)",this->m_Map);
		return false;
	}

	int remain = max((int)(this->m_EventEnd-time(0))+2,2);

	if(CBCustomAddMob(index,t->Class,this->m_Map,x,y,(int)(GetLargeRand()%8),(DWORD)remain) == 0)
	{
		LogAdd(LOG_RED,"[CustomMining] spawn failed: class %d is not in Monster.txt of this GameServer (add rows 615-618)",t->Class);
		return false;
	}

	LogAdd(LOG_BLUE,"[CustomMining] %s stone spawned at %d,%d (index %d)",TIER_NAMES[tier],x,y,index);

	CM_STONE stone;
	stone.Index = index;
	stone.Tier = tier;
	stone.Owner = -1;
	stone.OwnerName[0] = 0;
	stone.X = x;
	stone.Y = y;

	this->m_Stones.push_back(stone);

	return true;
}

void CCustomMining::RemoveStone(int index) // OK
{
	for(size_t n=0;n < this->m_Stones.size();n++)
	{
		if(this->m_Stones[n].Index == index)
		{
			if(this->IsStoneValid(&this->m_Stones[n]) != false)
			{
				gObjDel(index);
			}

			this->m_Stones.erase(this->m_Stones.begin()+n);
			return;
		}
	}
}

int CCustomMining::CurrentPhase() // OK
{
	int elapsed = (int)(time(0)-this->m_EventStart);
	int phase = 0;

	for(int n=0;n < CM_MAX_TIERS;n++)
	{
		if(elapsed >= this->m_Tier[n].PhaseFrom && elapsed < this->m_Tier[n].PhaseTo)
		{
			phase = n+1;
		}
	}

	return phase;
}

void CCustomMining::SpawnTick() // OK
{
	int elapsed = (int)(time(0)-this->m_EventStart);

	for(int n=0;n < CM_MAX_TIERS;n++)
	{
		CM_TIER* t = &this->m_Tier[n];

		if(t->Steps <= 0 || t->Class <= 0 || elapsed < t->PhaseFrom || elapsed >= t->PhaseTo)
		{
			continue;
		}

		if((int)(GetTickCount()-t->NextSpawn) < 0 && t->NextSpawn != 0)
		{
			continue;
		}

		// every interval this colour adds SpawnCount stones, while the colour and the whole event stay under their caps
		bool failed = false;

		int phase = this->CurrentPhase();
		int amount = t->SpawnCount[(phase >= 1 && phase <= 3)?phase-1:0];

		for(int i=0;i < amount;i++)
		{
			if(this->AliveCount(n) >= t->MaxAlive || (int)this->m_Stones.size() >= this->m_MaxStones)
			{
				break;
			}

			if(this->SpawnStone(n) == false)
			{
				failed = true;
				break;
			}
		}

		t->NextSpawn = GetTickCount()+((failed != false)?2000:(DWORD)t->SpawnIntervalSec*1000); // failed: no free tile / slot, try again soon
	}
}

void CCustomMining::SendHud() // OK
{
	PMSG_CM_HUD_SEND pMsg;

	pMsg.header.set(0xD3,0x6E,sizeof(pMsg));

	pMsg.Phase = (this->m_State == CM_STATE_RUNNING)?(BYTE)this->CurrentPhase():0;
	pMsg.Pad = 0;

	time_t now = time(0);
	pMsg.RemainSec = (WORD)((this->m_State == CM_STATE_RUNNING && this->m_EventEnd > now)?min((long long)(this->m_EventEnd-now),(long long)65535):0);

	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) == 0 || gObj[n].Map != this->m_Map)
		{
			continue;
		}

		DataSend(n,(BYTE*)&pMsg,pMsg.header.size);
	}
}

//////////////////////////////////////////////////////////////////////
// State machine
//////////////////////////////////////////////////////////////////////

void CCustomMining::MainProc() // OK
{
	if((GetTickCount()-this->m_SecTick) < 1000)
	{
		return;
	}

	this->m_SecTick = GetTickCount();

	if(this->m_Switch == 0 || this->Served() == false)
	{
		if(this->m_State == CM_STATE_RUNNING)
		{
			CsLock lock(&this->m_cs);
			this->EndEvent("switched off");
		}

		this->m_State = CM_STATE_OFF;
		gServerDisplayer.EventTarkanMining = -1;
		return;
	}

	CsLock lock(&this->m_cs);

	if(this->m_State == CM_STATE_OFF)
	{
		this->m_State = CM_STATE_IDLE;
		this->SetNextStart();
	}

	gServerDisplayer.EventTarkanMining = this->GetDisplayTime();

	time_t now = time(0);

	if(this->m_State == CM_STATE_IDLE)
	{
		if(this->m_StartCount > 0 && this->m_NextStart != 0 && now >= this->m_NextStart)
		{
			this->StartEvent();
		}

		return;
	}

	// running
	if(now >= this->m_EventEnd)
	{
		this->EndEvent("time is up");
		return;
	}

	int phase = this->CurrentPhase();

	if(phase != this->m_LastPhase)
	{
		this->m_LastPhase = phase;

		if(phase == 1){this->Announce("%s",this->m_TextBlue);}
		else if(phase == 2){this->Announce("%s",this->m_TextPurple);}
		else if(phase == 3){this->Announce("%s",this->m_TextGold);}
	}

	if(this->m_OneMinDone == false && (this->m_EventEnd-now) <= 60 && this->m_Duration > 90)
	{
		this->m_OneMinDone = true;
		this->Announce("%s",this->m_TextOneMin);
	}

	this->SpawnTick();
	this->SessionTick();
	this->SendHud();
}

void CCustomMining::StartEvent() // OK
{
	this->m_State = CM_STATE_RUNNING;
	this->m_EventStart = time(0);
	this->m_EventEnd = this->m_EventStart+this->m_Duration;
	this->m_LastPhase = 0;
	this->m_OneMinDone = false;

	for(int n=0;n < CM_MAX_TIERS;n++)
	{
		this->m_Tier[n].NextSpawn = 0;
	}

	this->m_Stones.clear();

	this->ResetStats();

	LogAdd(LOG_BLUE,"[CustomMining] event started (%d s) on map %d",this->m_Duration,this->m_Map);

	this->Announce("%s",this->m_TextStart);
}

void CCustomMining::EndEvent(const char* reason) // OK
{
	LogAdd(LOG_BLUE,"[CustomMining] event ended (%s)",reason);

	this->ForceCashAll();

	for(size_t n=0;n < this->m_Stones.size();n++)
	{
		if(this->IsStoneValid(&this->m_Stones[n]) != false)
		{
			gObjDel(this->m_Stones[n].Index);
		}
	}

	this->m_Stones.clear();
	this->m_Sessions.clear();

	this->m_State = CM_STATE_IDLE;
	this->m_LastPhase = 0;

	this->SendHud(); // phase 0, 0 seconds: the clients hide the line

	this->Announce("%s",this->m_TextEnd);

	this->AnnounceStats();

	this->SetNextStart();

	gServerDisplayer.EventTarkanMining = this->GetDisplayTime();
}

int CCustomMining::GetDisplayTime() // OK
{
	if(this->m_Switch == 0 || this->Served() == false)
	{
		return -1;
	}

	if(this->m_State == CM_STATE_RUNNING)
	{
		return 0;
	}

	if(this->m_State == CM_STATE_IDLE && this->m_NextStart != 0)
	{
		return max((int)(this->m_NextStart-time(0)),1);
	}

	return -1;
}

//////////////////////////////////////////////////////////////////////
// Player flow
//////////////////////////////////////////////////////////////////////

CM_SESSION* CCustomMining::FindSession(LPOBJ lpObj) // OK
{
	std::map<int,CM_SESSION>::iterator it = this->m_Sessions.find(lpObj->Index);

	if(it == this->m_Sessions.end())
	{
		return 0;
	}

	if(strcmp(it->second.Name,lpObj->Name) != 0)
	{
		this->m_Sessions.erase(it); // stale index: a different character owns it now
		return 0;
	}

	return &it->second;
}

void CCustomMining::SendState(LPOBJ lpObj,CM_SESSION* s,int code) // OK
{
	PMSG_CM_STATE_SEND pMsg;

	pMsg.header.set(0xD3,0x6D,sizeof(pMsg));

	CM_TIER* t = &this->m_Tier[s->Tier];

	pMsg.Code = (BYTE)code;
	pMsg.Class = (BYTE)s->Tier; // the window picks the colour from it
	pMsg.Step = (BYTE)s->Step;
	pMsg.Steps = (BYTE)t->Steps;
	pMsg.Err = 0;
	pMsg.JewelItem = (WORD)s->JewelItem;
	pMsg.Pending = (WORD)min(s->Pending,65535);
	pMsg.NextTotal = (WORD)((s->Step < t->Steps)?t->Total[s->Step]:0);
	pMsg.Chance = (WORD)((s->Step < t->Steps)?t->Chance[s->Step]:0);
	pMsg.AttemptMs = (WORD)this->m_AttemptMs;
	pMsg.DurCost = (BYTE)t->DurCost;
	pMsg.PickDur = (BYTE)((lpObj->Inventory[0].IsItem() != 0 && lpObj->Inventory[0].m_Index == this->m_PickAxe)?min((int)lpObj->Inventory[0].m_Durability,255):0);

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
}

bool CCustomMining::CheckRequirements(LPOBJ lpObj,CM_STONE* stone,int tier,bool checkDur) // OK
{
	if(lpObj->Map != this->m_Map || abs(lpObj->X-gObj[stone->Index].X) > this->m_MaxDistance || abs(lpObj->Y-gObj[stone->Index].Y) > this->m_MaxDistance)
	{
		this->Notify(lpObj,"Mining: you are too far from the stone.");
		return false;
	}

	if(lpObj->Inventory[0].IsItem() == 0 || lpObj->Inventory[0].m_Index != this->m_PickAxe)
	{
		this->Notify(lpObj,"Mining: equip the Pick Axe in your hand first.");
		return false;
	}

	if(checkDur != false && (int)lpObj->Inventory[0].m_Durability < this->m_Tier[tier].DurCost)
	{
		this->Notify(lpObj,"Mining: your Pick Axe needs at least %d durability for this stone (repair it with a Jewel of Bless).",this->m_Tier[tier].DurCost);
		return false;
	}

	return true;
}

bool CCustomMining::Dialog(LPOBJ lpObj,LPOBJ lpNpc) // OK
{
	int tier = this->TierOfClass(lpNpc->Class);

	if(tier < 0 || lpNpc->Map != this->m_Map)
	{
		return false;
	}

	if(lpObj->Interface.use != 0 || lpObj->PShopOpen != 0)
	{
		return true;
	}

	CsLock lock(&this->m_cs);

	CM_STONE* stone = this->FindStone(lpNpc->Index);

	if(this->m_Switch == 0 || this->m_State != CM_STATE_RUNNING || stone == 0 || this->IsStoneValid(stone) == false)
	{
		this->Notify(lpObj,"Mining: this stone is quiet. Wait for the next Tarkan Mining event.");
		return true;
	}

	CM_SESSION* s = this->FindSession(lpObj);

	if(s != 0 && s->Step > 0)
	{
		// the player is in the middle of a stone: the click brings the window back
		if(s->Busy == false)
		{
			this->SendState(lpObj,s,CM_CODE_CONTINUE);
		}

		return true;
	}

	// locked by somebody else?
	if(stone->Owner != -1 && stone->Owner != lpObj->Index)
	{
		if(gObjIsConnectedGP(stone->Owner) != 0 && strcmp(gObj[stone->Owner].Name,stone->OwnerName) == 0 && this->m_Sessions.find(stone->Owner) != this->m_Sessions.end())
		{
			this->Notify(lpObj,"Mining: somebody is already mining this stone.");
			return true;
		}

		stone->Owner = -1; // stale lock
	}

	if(this->CheckRequirements(lpObj,stone,tier,true) == false)
	{
		return true;
	}

	CM_SESSION session;
	memset(&session,0,sizeof(session));
	CopyText(session.Name,sizeof(session.Name),lpObj->Name);
	session.Stone = stone->Index;
	session.Tier = tier;
	session.IdleTick = GetTickCount();

	this->m_Sessions[lpObj->Index] = session;

	this->SendState(lpObj,&this->m_Sessions[lpObj->Index],CM_CODE_ASK);

	return true;
}

void CCustomMining::CGActionRecv(PMSG_CM_ACTION_RECV* lpMsg,int aIndex) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0 || this->m_Switch == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	CsLock lock(&this->m_cs);

	CM_SESSION* s = this->FindSession(lpObj);

	if(s == 0)
	{
		PMSG_CM_STATE_SEND pMsg;
		memset(&pMsg,0,sizeof(pMsg));
		pMsg.header.set(0xD3,0x6D,sizeof(pMsg));
		pMsg.Code = CM_CODE_CLOSE;
		DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	s->IdleTick = GetTickCount();

	if(lpMsg->Action == 2)
	{
		if(s->Busy == false && s->Step == 0)
		{
			this->EndSession(aIndex,false);
		}

		return;
	}

	if(lpMsg->Action == 3)
	{
		if(s->Busy != false && (GetTickCount()-s->StartTick) >= (DWORD)(this->m_AttemptMs*8/10))
		{
			this->Resolve(lpObj,s);
		}

		return;
	}

	if(s->Busy != false)
	{
		return;
	}

	CM_STONE* stone = this->FindStone(s->Stone);

	if(this->m_State != CM_STATE_RUNNING || stone == 0 || this->IsStoneValid(stone) == false)
	{
		this->Notify(lpObj,"Mining: the stone is gone.");

		if(s->Step > 0)
		{
			this->CashOut(lpObj,s,true);
		}
		else
		{
			this->SendState(lpObj,s,CM_CODE_CLOSE);
			this->EndSession(aIndex,false);
		}

		return;
	}

	if(lpMsg->Action == 1)
	{
		if(s->Step > 0)
		{
			this->CashOut(lpObj,s,false);
		}

		return;
	}

	if(lpMsg->Action != 0)
	{
		return;
	}

	if(s->Step == 0 && stone->Owner != -1 && stone->Owner != aIndex)
	{
		if(gObjIsConnectedGP(stone->Owner) != 0 && strcmp(gObj[stone->Owner].Name,stone->OwnerName) == 0 && this->m_Sessions.find(stone->Owner) != this->m_Sessions.end())
		{
			this->Notify(lpObj,"Mining: somebody is already mining this stone.");
			this->SendState(lpObj,s,CM_CODE_ERROR);
			this->EndSession(aIndex,false);
			return;
		}
	}

	if(this->CheckRequirements(lpObj,stone,s->Tier,true) == false)
	{
		this->SendState(lpObj,s,CM_CODE_ERROR);

		if(s->Step == 0)
		{
			this->EndSession(aIndex,false);
		}

		return;
	}

	if(s->Step == 0)
	{
		stone->Owner = aIndex;
		CopyText(stone->OwnerName,sizeof(stone->OwnerName),lpObj->Name);
	}

	this->StartAttempt(lpObj,s);
}

void CCustomMining::StartAttempt(LPOBJ lpObj,CM_SESSION* s) // OK
{
	CM_TIER* t = &this->m_Tier[s->Tier];

	// the durability is paid whether the attempt wins or loses
	float dur = lpObj->Inventory[0].m_Durability-(float)t->DurCost;
	lpObj->Inventory[0].m_Durability = (dur < 0)?0:dur;
	gItemManager.GCItemDurSend(lpObj->Index,0,(BYTE)min((int)lpObj->Inventory[0].m_Durability,255),0);

	s->Won = ((int)(GetLargeRand()%10000) < t->Chance[s->Step]);
	s->Busy = true;
	s->StartTick = GetTickCount();
	s->DueTick = s->StartTick+(DWORD)this->m_AttemptMs+5000;
	s->IdleTick = s->StartTick;

	this->SendState(lpObj,s,CM_CODE_MINING);
}

// Applies the hidden outcome of the running attempt to the session. true = success.
bool CCustomMining::ApplyOutcome(CM_SESSION* s) // OK
{
	s->Busy = false;
	s->IdleTick = GetTickCount();

	CM_TIER* t = &this->m_Tier[s->Tier];

	if(s->Won == false)
	{
		return false;
	}

	s->Step++;

	if(s->Step == 1)
	{
		s->JewelItem = this->RollJewel(); // rolled once, kept for the rest of the stone
	}

	s->Pending = t->Total[s->Step-1];

	return true;
}

void CCustomMining::Resolve(LPOBJ lpObj,CM_SESSION* s) // OK
{
	CM_TIER* t = &this->m_Tier[s->Tier];

	int lost = s->Pending;

	if(this->ApplyOutcome(s) == false)
	{
		s->Pending = lost;
		this->SendState(lpObj,s,CM_CODE_FAIL);
		this->Notify(lpObj,"Mining: the stone crumbled%s.",(lost > 0)?" and your jewels are lost":"");
		LogAdd(LOG_BLUE,"[CustomMining] %s failed at step %d (lost %d %s)",lpObj->Name,s->Step+1,lost,this->JewelName(s->JewelItem));

		this->m_StatStones++;
		this->m_StatLostStones++;
		this->m_StatLostJewels += lost;
		this->EndSession(lpObj->Index,true);
		return;
	}

	if(s->Step >= t->Steps)
	{
		this->CashOut(lpObj,s,false); // the last step pays by itself
		return;
	}

	this->SendState(lpObj,s,CM_CODE_CONTINUE);
}

bool CCustomMining::GrantJewels(LPOBJ lpObj,int item,int count,bool forced) // OK
{
	if(count <= 0)
	{
		return true;
	}

#if(JEWELBANKVER2)
	if(this->m_UseJewelBank != 0 && gBCustomItemBank.CanAddBank(lpObj->Index,item,0,count) != 0 && gBCustomItemBank.CongTruBank(lpObj->Index,item,0,count,0) != 0)
	{
		return true;
	}
#endif

	// bundles in the bag (the same split as the vanilla mining reward: 30 / 20 / 10 / singles)
	int pack = -1;

	for(int n=0;n < MAX_JEWEL_MIX_BUNDLE;n++)
	{
		if(gJewelMix.GetJewelSimpleIndex(n) == item)
		{
			pack = gJewelMix.GetJewelBundleIndex(n);
			break;
		}
	}

	int table[4] = {0,0,0,0};
	int value = count;

	if(pack == -1)
	{
		table[0] = count;
	}
	else
	{
		table[3] = value/30; value %= 30;
		table[2] = value/20; value %= 20;
		table[1] = value/10; value %= 10;
		table[0] = value;
	}

	if(gItemManager.GetInventoryEmptySlotCount(lpObj) < (table[0]+table[1]+table[2]+table[3]))
	{
		return false;
	}

	for(int n=0;n < table[3];n++){GDCreateItemSend(lpObj->Index,0xEB,0,0,pack,2,0,0,0,0,lpObj->Index,0,0,0,0,0,0xFF,0);}
	for(int n=0;n < table[2];n++){GDCreateItemSend(lpObj->Index,0xEB,0,0,pack,1,0,0,0,0,lpObj->Index,0,0,0,0,0,0xFF,0);}
	for(int n=0;n < table[1];n++){GDCreateItemSend(lpObj->Index,0xEB,0,0,pack,0,0,0,0,0,lpObj->Index,0,0,0,0,0,0xFF,0);}
	for(int n=0;n < table[0];n++){GDCreateItemSend(lpObj->Index,0xEB,0,0,item,0,0,0,0,0,lpObj->Index,0,0,0,0,0,0xFF,0);}

	this->Notify(lpObj,"Mining: %d %s added to your bag.",count,this->JewelName(item));

	return true;
}

bool CCustomMining::CashOut(LPOBJ lpObj,CM_SESSION* s,bool forced) // OK
{
	int item = s->JewelItem;
	int count = s->Pending;

	if(count > 0 && item != 0)
	{
		if(this->GrantJewels(lpObj,item,count,forced) == false)
		{
			if(forced == false)
			{
				this->Notify(lpObj,"Mining: your bag is full - free some slots and cash out again.");
				s->Busy = false;
				this->SendState(lpObj,s,CM_CODE_CONTINUE);
				return false;
			}

			LogAdd(LOG_RED,"[CustomMining] FORFEITED: %s [%s] could not receive %d x %s (bank refused, bag full)",lpObj->Name,lpObj->Account,count,this->JewelName(item));
		}
		else
		{
			LogAdd(LOG_BLUE,"[CustomMining] %s cashed out %d x %s at step %d",lpObj->Name,count,this->JewelName(item),s->Step);

			// statistics: what was mined, and by whom
			bool listed = false;

			for(int n=0;n < this->m_JewelCount;n++)
			{
				if(this->m_JewelItem[n] == item)
				{
					this->m_StatJewels[n] += count;
					listed = true;
					break;
				}
			}

			if(listed == false)
			{
				this->m_StatOtherJewels += count;
			}

			this->m_StatMiner[std::string(s->Name)] += count;
		}

		this->m_StatStones++;
	}

	if(forced == false || (gObjIsConnectedGP(lpObj->Index) != 0 && strcmp(lpObj->Name,s->Name) == 0))
	{
		this->SendState(lpObj,s,CM_CODE_CASHED);
	}

	this->EndSession(lpObj->Index,true);

	return true;
}

void CCustomMining::ResetStats() // OK
{
	memset(this->m_StatJewels,0,sizeof(this->m_StatJewels));
	this->m_StatOtherJewels = 0;
	this->m_StatStones = 0;
	this->m_StatLostStones = 0;
	this->m_StatLostJewels = 0;
	this->m_StatMiner.clear();
}

// After the event: the golden lines with what was mined (total and per jewel), the stones, and the best miner.
void CCustomMining::AnnounceStats() // OK
{
	int total = this->m_StatOtherJewels;

	for(int n=0;n < this->m_JewelCount;n++)
	{
		total += this->m_StatJewels[n];
	}

	if(this->m_StatStones == 0)
	{
		this->Announce("[Tarkan Mining] Results: no stone was mined this time.");
		return;
	}

	char detail[200] = {0};

	for(int n=0;n < this->m_JewelCount;n++)
	{
		if(this->m_StatJewels[n] <= 0)
		{
			continue;
		}

		char part[48];
		sprintf_s(part,"%s%d %s",(detail[0] != 0)?", ":"",this->m_StatJewels[n],this->JewelName(this->m_JewelItem[n]));

		if(strlen(detail)+strlen(part) < sizeof(detail)-1)
		{
			strcat_s(detail,part);
		}
	}

	if(total > 0)
	{
		this->Announce("[Tarkan Mining] Results: %d jewels mined (%s).",total,detail);
	}
	else
	{
		this->Announce("[Tarkan Mining] Results: no jewels were cashed out this time.");
	}

	this->Announce("[Tarkan Mining] %d stone%s mined, %d crumbled (%d jewel%s lost).",this->m_StatStones,(this->m_StatStones == 1)?"":"s",this->m_StatLostStones,this->m_StatLostJewels,(this->m_StatLostJewels == 1)?"":"s");

	std::string best;
	int bestCount = 0;

	for(std::map<std::string,int>::iterator it = this->m_StatMiner.begin();it != this->m_StatMiner.end();++it)
	{
		if(it->second > bestCount)
		{
			bestCount = it->second;
			best = it->first;
		}
	}

	if(bestCount > 0)
	{
		this->Announce("[Tarkan Mining] Top miner: %s with %d jewels!",best.c_str(),bestCount);
	}

	LogAdd(LOG_BLUE,"[CustomMining] results: %d jewels (%s), %d stones, %d crumbled, %d lost, top %s %d",total,detail,this->m_StatStones,this->m_StatLostStones,this->m_StatLostJewels,best.c_str(),bestCount);
}

void CCustomMining::EndSession(int aIndex,bool removeStone) // OK
{
	std::map<int,CM_SESSION>::iterator it = this->m_Sessions.find(aIndex);

	if(it == this->m_Sessions.end())
	{
		return;
	}

	int stoneIndex = it->second.Stone;

	this->m_Sessions.erase(it);

	if(removeStone != false)
	{
		this->RemoveStone(stoneIndex);
	}
	else
	{
		CM_STONE* stone = this->FindStone(stoneIndex);

		if(stone != 0 && stone->Owner == aIndex)
		{
			stone->Owner = -1;
			stone->OwnerName[0] = 0;
		}
	}
}

void CCustomMining::ForceCashAll() // OK
{
	std::vector<int> list;

	for(std::map<int,CM_SESSION>::iterator it = this->m_Sessions.begin();it != this->m_Sessions.end();++it)
	{
		list.push_back(it->first);
	}

	for(size_t n=0;n < list.size();n++)
	{
		std::map<int,CM_SESSION>::iterator it = this->m_Sessions.find(list[n]);

		if(it == this->m_Sessions.end())
		{
			continue;
		}

		CM_SESSION* s = &it->second;

		if(gObjIsConnectedGP(list[n]) == 0 || strcmp(gObj[list[n]].Name,s->Name) != 0)
		{
			LogAdd(LOG_RED,"[CustomMining] session of %s dropped (player gone), %d x %s lost",s->Name,s->Pending,this->JewelName(s->JewelItem));
			this->m_Sessions.erase(it);
			continue;
		}

		LPOBJ lpObj = &gObj[list[n]];

		if(s->Busy != false)
		{
			this->ApplyOutcome(s);

			if(s->Won == false)
			{
				s->Pending = 0;
			}
		}

		if(s->Step > 0 && s->Pending > 0)
		{
			this->CashOut(lpObj,s,true);
		}
		else
		{
			this->SendState(lpObj,s,CM_CODE_CLOSE);
			this->m_Sessions.erase(list[n]);
		}
	}
}

void CCustomMining::SessionTick() // OK
{
	std::vector<int> list;

	for(std::map<int,CM_SESSION>::iterator it = this->m_Sessions.begin();it != this->m_Sessions.end();++it)
	{
		list.push_back(it->first);
	}

	DWORD now = GetTickCount();

	for(size_t n=0;n < list.size();n++)
	{
		std::map<int,CM_SESSION>::iterator it = this->m_Sessions.find(list[n]);

		if(it == this->m_Sessions.end())
		{
			continue;
		}

		CM_SESSION* s = &it->second;

		if(gObjIsConnectedGP(list[n]) == 0 || strcmp(gObj[list[n]].Name,s->Name) != 0)
		{
			this->EndSession(list[n],true);
			continue;
		}

		LPOBJ lpObj = &gObj[list[n]];

		CM_STONE* stone = this->FindStone(s->Stone);

		if(stone == 0 || this->IsStoneValid(stone) == false)
		{
			if(s->Busy != false)
			{
				this->ApplyOutcome(s);

				if(s->Won == false)
				{
					s->Pending = 0;
				}
			}

			if(s->Step > 0 && s->Pending > 0)
			{
				this->CashOut(lpObj,s,true);
			}
			else
			{
				this->SendState(lpObj,s,CM_CODE_CLOSE);
				this->EndSession(list[n],false);
			}

			continue;
		}

		if(s->Busy != false)
		{
			if((int)(now-s->DueTick) >= 0)
			{
				this->Resolve(lpObj,s); // the client never reported the end of the bar (lag): apply it anyway
			}

			continue;
		}

		if(s->Step == 0)
		{
			if((now-s->IdleTick) > 60000)
			{
				this->SendState(lpObj,s,CM_CODE_CLOSE);
				this->EndSession(list[n],false);
			}
		}
		else if(this->m_IdleCashoutSec > 0 && (now-s->IdleTick) > (DWORD)this->m_IdleCashoutSec*1000)
		{
			this->Notify(lpObj,"Mining: you took too long - your jewels were cashed out.");

			if(this->CashOut(lpObj,s,false) == false)
			{
				s->IdleTick = now; // bag full: ask again later
			}
		}
	}
}

void CCustomMining::OnUserLogout(LPOBJ lpObj) // OK
{
	if(lpObj->Type != OBJECT_USER)
	{
		return;
	}

	CsLock lock(&this->m_cs);

	CM_SESSION* s = this->FindSession(lpObj);

	if(s == 0)
	{
		return;
	}

	bool wasBusy = (s->Busy != false);

	if(wasBusy != false)
	{
		this->ApplyOutcome(s);

		if(s->Won == false)
		{
			s->Pending = 0;
		}
	}

	if(s->Step > 0 && s->Pending > 0)
	{
		this->CashOut(lpObj,s,true);
	}
	else
	{
		this->EndSession(lpObj->Index,(s->Step > 0 || wasBusy != false));
	}
}

//////////////////////////////////////////////////////////////////////
// GM command / menu
//////////////////////////////////////////////////////////////////////

void CCustomMining::MenuCommand(const char* arg) // OK
{
	if(this->m_Switch == 0)
	{
		LogAdd(LOG_RED,"[CustomMining] Switch is 0 in CustomMining.txt: nothing started.");
		return;
	}

	CsLock lock(&this->m_cs);

	if(this->Served() == false)
	{
		LogAdd(LOG_RED,"[CustomMining] this server does not run map %d.",this->m_Map);
		return;
	}

	if(_stricmp(arg,"start") == 0)
	{
		if(this->m_State == CM_STATE_RUNNING)
		{
			LogAdd(LOG_RED,"[CustomMining] the event is already running.");
		}
		else
		{
			this->m_State = CM_STATE_IDLE;
			this->StartEvent();
		}
	}
	else if(_stricmp(arg,"stop") == 0)
	{
		if(this->m_State == CM_STATE_RUNNING)
		{
			this->EndEvent("stopped by a GM");
		}
	}
}

void CCustomMining::RunTest(LPOBJ lpObj,int tier,int sessions) // OK
{
	CM_TIER* t = &this->m_Tier[tier];

	if(t->Steps <= 0)
	{
		this->Notify(lpObj,"[Mining] %s has no steps configured.",TIER_NAMES[tier]);
		return;
	}

	this->Notify(lpObj,"[Mining] %s x%d sessions, expected jewels per stone when cashing out after step k:",TIER_NAMES[tier],sessions);

	for(int k=1;k <= t->Steps;k++)
	{
		double sum = 0;

		for(int i=0;i < sessions;i++)
		{
			bool ok = true;

			for(int a=0;a < k && ok;a++)
			{
				ok = ((int)(GetLargeRand()%10000) < t->Chance[a]);
			}

			if(ok)
			{
				sum += t->Total[k-1];
			}
		}

		double reach = 1.0;

		for(int a=0;a < k;a++)
		{
			reach *= t->Chance[a]/10000.0;
		}

		this->Notify(lpObj,"[Mining] step %d: %.2f jewels (reach chance %.1f%%, durability %d)",k,sum/sessions,reach*100.0,t->DurCost*k);
	}
}

bool CCustomMining::Command(LPOBJ lpObj,char* arg) // OK
{
	if(gGameMaster.CheckGameMasterLevel(lpObj,1) == 0)
	{
		return false;
	}

	char word[32] = {0};
	char colour[32] = {0};
	int number = 0;

	int got = sscanf_s(arg,"%31s %31s %d",word,(unsigned)sizeof(word),colour,(unsigned)sizeof(colour),&number);

	if(got < 1 || _stricmp(word,"status") == 0)
	{
		this->Notify(lpObj,"[Mining] %s. Commands: /mining start|stop|status|spawn <blue|purple|gold>|test <colour> <sessions>",(this->m_Switch == 0)?"OFF (Switch = 0)":((this->m_State == CM_STATE_RUNNING)?"RUNNING":"idle"));

		if(this->m_Switch != 0)
		{
			CsLock lock(&this->m_cs);

			this->Notify(lpObj,"[Mining] stones: blue %d, purple %d, gold %d; miners %d; next start in %d s",this->AliveCount(0),this->AliveCount(1),this->AliveCount(2),(int)this->m_Sessions.size(),this->GetDisplayTime());
		}

		return true;
	}

	if(_stricmp(word,"test") == 0)
	{
		int tier = this->TierByName(colour);

		if(tier < 0)
		{
			this->Notify(lpObj,"[Mining] /mining test <blue|purple|gold> <sessions>");
			return true;
		}

		CsLock lock(&this->m_cs);
		this->RunTest(lpObj,tier,min(max((got >= 3)?number:10000,100),1000000));
		return true;
	}

	if(this->m_Switch == 0)
	{
		this->Notify(lpObj,"[Mining] Switch is 0 in CustomMining.txt.");
		return true;
	}

	if(_stricmp(word,"start") == 0 || _stricmp(word,"stop") == 0)
	{
		this->MenuCommand(word);
		return true;
	}

	if(_stricmp(word,"spawn") == 0)
	{
		int tier = this->TierByName(colour);

		CsLock lock(&this->m_cs);

		if(tier < 0 || this->m_State != CM_STATE_RUNNING)
		{
			this->Notify(lpObj,"[Mining] /mining spawn <blue|purple|gold> needs a running event (/mining start).");
		}
		else
		{
			this->Notify(lpObj,(this->SpawnStone(tier) != false)?"[Mining] stone spawned.":"[Mining] no free tile / slot.");
		}

		return true;
	}

	return false;
}
