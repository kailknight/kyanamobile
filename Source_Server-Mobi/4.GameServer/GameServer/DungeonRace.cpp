// DungeonRace.cpp - see DungeonRace.h and Working Project\MD Files\DungeonRace-DESIGN.md.
#include "stdafx.h"
#include "DungeonRace.h"
#include "Attack.h"
#include "CommandManager.h"
#include "DSProtocol.h"
#include "EffectManager.h"
#include "Gate.h"
#include "GameMaster.h"
#include "Guild.h"
#include "GuildClass.h"
#include "ItemBagManager.h"
#include "ItemManager.h"
#include "Map.h"
#include "MapManager.h"
#include "MapServerManager.h"
#include "Monster.h"
#include "MonsterSetBase.h"
#include "Notice.h"
#include "ObjectManager.h"
#include "Protocol.h"
#include "ServerDisplayer.h"
#include "ServerInfo.h"
#include "User.h"
#include "Util.h"
#include "Log.h"
#include <algorithm>

CDungeonRace gDungeonRace;

// defined in User.cpp (not declared in a header)
BOOL BackSpringCheck(int & x, int & y, int & dir, BYTE map);

// Set around the event's own teleports / around a player walking through a gate (same thread as the warp function).
static __declspec(thread) int t_Internal = 0;
static __declspec(thread) int t_GateStep = 0;

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
}

CDungeonRace::CDungeonRace() // OK
{
	InitializeCriticalSection(&this->m_cs);

	this->m_Switch = 0;
	this->m_ServerFilter = -1;
	this->m_Schedule = 2;
	this->m_DayOfWeek = 6;
	this->m_Hour = 21;
	this->m_Minute = 0;
	this->m_RegisterMin = 5;
	this->m_MinPlayers = 2;
	this->m_MaxPlayers = 100;
	this->m_CountdownSec = 15;
	this->m_MaxMin = 20;
	this->m_Winners = 3;
	this->m_FinishExitDelaySec = 5;
	this->m_MinLevel = 1;
	this->m_StartMap = 0;
	this->m_StartX = 123;
	this->m_StartY = 229;
	this->m_StartRadius = 6;
	this->m_DungeonMap = 1;
	this->m_FinishX1 = 0;
	this->m_FinishY1 = 78;
	this->m_FinishX2 = 12;
	this->m_FinishY2 = 92;
	this->m_NpcClass = 239;
	this->m_NpcMap = 0;
	this->m_NpcX = 127;
	this->m_NpcY = 229;
	this->m_NpcDir = 3;
	this->m_BlockWings = 1;
	this->m_BlockWeapons = 1;
	this->m_BlockRings = 1;
	this->m_BlockPendants = 1;
	this->m_BlockHornUniria = 1;
	this->m_BlockHornDinorant = 1;
	this->m_BlockFenrir = 1;
	this->m_IceChance = 100;
	this->m_IceSeconds = 4;
	this->m_LightningChance = 100;
	this->m_LightningMode = 2;
	this->m_LightningTiles = 3;
	this->m_MaxJump = 15;
	this->m_WarpDisqualify = 1;
	this->m_RespawnAtStart = 1;
	this->m_LockDungeon = 1;
	this->m_GMBypass = 1;
	this->m_ConfirmSec = 10;

	this->m_State = DR_STATE_OFF;
	this->m_NextStart = 0;
	this->m_StateEnd = 0;
	this->m_RaceStart = 0;
	this->m_RaceStartTick = 0;
	memset(this->m_WinnerName,0,sizeof(this->m_WinnerName));
	this->m_SecTick = GetTickCount();
	this->m_NpcIndex = -1;
	this->m_LastAnnounce = -1;
	this->m_FinishCount = 0;
	this->m_EndRequest = false;

	for(int n=0;n < 3;n++)
	{
		this->m_Zen[n] = 0;
		this->m_Bag[n] = 600+n;
	}
}

CDungeonRace::~CDungeonRace() // OK
{
	DeleteCriticalSection(&this->m_cs);
}

//////////////////////////////////////////////////////////////////////
// Config
//////////////////////////////////////////////////////////////////////

void CDungeonRace::ParseIntList(const char* text,std::set<int>* out) // OK
{
	out->clear();

	char buff[512];
	CopyText(buff,sizeof(buff),text);

	char* context = 0;

	for(char* token = strtok_s(buff,", ;",&context);token != 0;token = strtok_s(0,", ;",&context))
	{
		out->insert(atoi(token));
	}
}

void CDungeonRace::ParseMonsterMap(const char* text) // OK
{
	this->m_MonsterMap.clear();

	char buff[512];
	CopyText(buff,sizeof(buff),text);

	char* context = 0;

	for(char* token = strtok_s(buff,", ;",&context);token != 0;token = strtok_s(0,", ;",&context))
	{
		int from = 0;
		int to = 0;

		if(sscanf_s(token,"%d:%d",&from,&to) == 2)
		{
			this->m_MonsterMap[from] = to;
		}
	}
}

void CDungeonRace::Load(char* section,char* path) // OK
{
	CsLock lock(&this->m_cs);

	this->m_Switch = GetPrivateProfileInt(section,"DungeonRaceSwitch",0,path);
	this->m_ServerFilter = GetPrivateProfileInt(section,"DungeonRaceServer",-1,path);
	this->m_Schedule = GetPrivateProfileInt(section,"DungeonRaceSchedule",2,path);
	this->m_DayOfWeek = GetPrivateProfileInt(section,"DungeonRaceDayOfWeek",6,path);
	this->m_Hour = GetPrivateProfileInt(section,"DungeonRaceHour",21,path);
	this->m_Minute = GetPrivateProfileInt(section,"DungeonRaceMinute",0,path);
	this->m_RegisterMin = GetPrivateProfileInt(section,"DungeonRaceRegisterMin",5,path);
	this->m_MinPlayers = GetPrivateProfileInt(section,"DungeonRaceMinPlayers",2,path);
	this->m_MaxPlayers = GetPrivateProfileInt(section,"DungeonRaceMaxPlayers",100,path);
	this->m_CountdownSec = GetPrivateProfileInt(section,"DungeonRaceCountdownSec",15,path);
	this->m_MaxMin = GetPrivateProfileInt(section,"DungeonRaceMaxMin",20,path);
	this->m_Winners = GetPrivateProfileInt(section,"DungeonRaceWinners",3,path);
	this->m_FinishExitDelaySec = GetPrivateProfileInt(section,"DungeonRaceFinishExitDelaySec",5,path);
	this->m_MinLevel = GetPrivateProfileInt(section,"DungeonRaceMinLevel",1,path);
	this->m_StartMap = GetPrivateProfileInt(section,"DungeonRaceStartMap",0,path);
	this->m_StartX = GetPrivateProfileInt(section,"DungeonRaceStartX",123,path);
	this->m_StartY = GetPrivateProfileInt(section,"DungeonRaceStartY",229,path);
	this->m_StartRadius = GetPrivateProfileInt(section,"DungeonRaceStartRadius",6,path);
	this->m_DungeonMap = GetPrivateProfileInt(section,"DungeonRaceDungeonMap",1,path);
	this->m_FinishX1 = GetPrivateProfileInt(section,"DungeonRaceFinishX1",0,path);
	this->m_FinishY1 = GetPrivateProfileInt(section,"DungeonRaceFinishY1",78,path);
	this->m_FinishX2 = GetPrivateProfileInt(section,"DungeonRaceFinishX2",12,path);
	this->m_FinishY2 = GetPrivateProfileInt(section,"DungeonRaceFinishY2",92,path);
	this->m_NpcClass = GetPrivateProfileInt(section,"DungeonRaceNpcClass",239,path);
	this->m_NpcMap = GetPrivateProfileInt(section,"DungeonRaceNpcMap",0,path);
	this->m_NpcX = GetPrivateProfileInt(section,"DungeonRaceNpcX",127,path);
	this->m_NpcY = GetPrivateProfileInt(section,"DungeonRaceNpcY",229,path);
	this->m_NpcDir = GetPrivateProfileInt(section,"DungeonRaceNpcDir",3,path);
	this->m_BlockWings = GetPrivateProfileInt(section,"DungeonRaceBlockWings",1,path);
	this->m_BlockWeapons = GetPrivateProfileInt(section,"DungeonRaceBlockWeapons",1,path);
	this->m_BlockRings = GetPrivateProfileInt(section,"DungeonRaceBlockRings",1,path);
	this->m_BlockPendants = GetPrivateProfileInt(section,"DungeonRaceBlockPendants",1,path);
	this->m_BlockHornUniria = GetPrivateProfileInt(section,"DungeonRaceBlockHornUniria",1,path);
	this->m_BlockHornDinorant = GetPrivateProfileInt(section,"DungeonRaceBlockHornDinorant",1,path);
	this->m_BlockFenrir = GetPrivateProfileInt(section,"DungeonRaceBlockFenrir",1,path);
	this->m_IceChance = GetPrivateProfileInt(section,"DungeonRaceIceChance",100,path);
	this->m_IceSeconds = GetPrivateProfileInt(section,"DungeonRaceIceSeconds",4,path);
	this->m_LightningChance = GetPrivateProfileInt(section,"DungeonRaceLightningChance",100,path);
	this->m_LightningMode = GetPrivateProfileInt(section,"DungeonRaceLightningMode",2,path);
	this->m_LightningTiles = GetPrivateProfileInt(section,"DungeonRaceLightningTiles",3,path);
	this->m_MaxJump = GetPrivateProfileInt(section,"DungeonRaceMaxJump",15,path);
	this->m_WarpDisqualify = GetPrivateProfileInt(section,"DungeonRaceWarpDisqualify",1,path);
	this->m_RespawnAtStart = GetPrivateProfileInt(section,"DungeonRaceRespawnAtStart",1,path);
	this->m_LockDungeon = GetPrivateProfileInt(section,"DungeonRaceLockDungeon",1,path);
	this->m_GMBypass = GetPrivateProfileInt(section,"DungeonRaceGMBypass",1,path);
	this->m_ConfirmSec = GetPrivateProfileInt(section,"DungeonRaceConfirmSec",10,path);

	char buff[1024];

	GetPrivateProfileString(section,"DungeonRaceMonsterMap","14:22,6:9,10:12,1:8",buff,sizeof(buff),path);
	this->ParseMonsterMap(buff);

	GetPrivateProfileString(section,"DungeonRaceIceClasses","8,12,22,100,101,102,104,105,106",buff,sizeof(buff),path);
	this->ParseIntList(buff,&this->m_IceClasses);

	GetPrivateProfileString(section,"DungeonRaceLightningClasses","9",buff,sizeof(buff),path);
	this->ParseIntList(buff,&this->m_LightningClasses);

	// Rewards live in EventItemBag: DungeonRaceBagN = the SpecialValue of the bag row in EventItemBagManager.txt (items edited in
	// EventItemBag\600/601/602 - Dungeon Race ... .txt), plus optional extra zen.
	static const int bagDefaults[3] = {600,601,602};
	static const int zenDefaults[3] = {0,0,0};

	for(int n=0;n < 3;n++)
	{
		char key[40];

		sprintf_s(key,"DungeonRaceBag%d",n+1);
		this->m_Bag[n] = GetPrivateProfileInt(section,key,bagDefaults[n],path);

		sprintf_s(key,"DungeonRaceZen%d",n+1);
		this->m_Zen[n] = (DWORD)GetPrivateProfileInt(section,key,zenDefaults[n],path);
	}

	if(this->m_RegisterMin < 1){this->m_RegisterMin = 1;}
	if(this->m_MinPlayers < 1){this->m_MinPlayers = 1;}
	if(this->m_MaxPlayers < 1){this->m_MaxPlayers = 1;}
	if(this->m_CountdownSec < 3){this->m_CountdownSec = 3;}
	if(this->m_MaxMin < 1){this->m_MaxMin = 1;}
	if(this->m_Winners < 1){this->m_Winners = 1;}
	if(this->m_Winners > 3){this->m_Winners = 3;}
	if(this->m_FinishExitDelaySec < 1){this->m_FinishExitDelaySec = 1;}
	if(this->m_StartRadius < 1){this->m_StartRadius = 1;}
	if(this->m_IceSeconds < 1){this->m_IceSeconds = 1;}
	if(this->m_LightningTiles < 1){this->m_LightningTiles = 1;}
	if(this->m_MaxJump < 0){this->m_MaxJump = 0;}
	if(this->m_Schedule < 0 || this->m_Schedule > 2){this->m_Schedule = 0;}
	if(this->m_DayOfWeek < 0 || this->m_DayOfWeek > 6){this->m_DayOfWeek = 6;}
	if(this->m_FinishX1 > this->m_FinishX2){std::swap(this->m_FinishX1,this->m_FinishX2);}
	if(this->m_FinishY1 > this->m_FinishY2){std::swap(this->m_FinishY1,this->m_FinishY2);}

	// A reload never interrupts a running race: only an idle event is rescheduled.
	if(this->m_State == DR_STATE_OFF || this->m_State == DR_STATE_IDLE)
	{
		this->m_State = ((this->m_Switch != 0)?DR_STATE_IDLE:DR_STATE_OFF);
		this->SetNextStart();
	}
}

bool CDungeonRace::Served() // OK
{
	if(this->m_ServerFilter != -1 && this->m_ServerFilter != gServerInfo.m_ServerCode)
	{
		return false;
	}

	return (gMapServerManager.CheckMapServer(this->m_NpcMap) != 0 && gMapServerManager.CheckMapServer(this->m_DungeonMap) != 0);
}

int CDungeonRace::GetState() // OK
{
	if(this->m_Switch == 0)
	{
		return DR_STATE_OFF;
	}

	return this->m_State;
}

//////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////

void CDungeonRace::Announce(const char* format,...) // OK
{
	char buff[256] = {0};

	va_list arg;
	va_start(arg,format);
	vsprintf_s(buff,format,arg);
	va_end(arg);

	gNotice.GCNoticeSendToAll(0,0,0,0,0,0,"%s",buff); // type 0 = the golden server-wide line
}

void CDungeonRace::Notify(LPOBJ lpObj,const char* format,...) // OK
{
	char buff[256] = {0};

	va_list arg;
	va_start(arg,format);
	vsprintf_s(buff,format,arg);
	va_end(arg);

	gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"%s",buff);
}

DR_PLAYER* CDungeonRace::Find(LPOBJ lpObj) // OK
{
	std::map<int,DR_PLAYER>::iterator it = this->m_Players.find(lpObj->Index);

	if(it == this->m_Players.end())
	{
		return 0;
	}

	if(strcmp(it->second.Name,lpObj->Name) != 0)
	{
		this->m_Players.erase(it); // stale index: a different character now owns it
		return 0;
	}

	return &it->second;
}

bool CDungeonRace::IsActiveRacer(LPOBJ lpObj) // OK
{
	DR_PLAYER* p = this->Find(lpObj);

	return (p != 0 && p->Active == 1);
}

int CDungeonRace::BlockedCategory(CItem* lpItem) // OK
{
	if(lpItem->IsItem() == 0)
	{
		return 0;
	}

	switch(lpItem->m_Slot)
	{
		case 7:
			return (this->m_BlockWings != 0)?1:0;
		case 9:
			return (this->m_BlockPendants != 0)?2:0;
		case 10:
		case 11:
			return (this->m_BlockRings != 0)?3:0;
		case 0:
		case 1:
			if(lpItem->m_Index < GET_ITEM(6,0) && lpItem->m_Index != GET_ITEM(4,7) && lpItem->m_Index != GET_ITEM(4,15)) // swords .. staffs, not shields / arrows / bolts
			{
				return (this->m_BlockWeapons != 0)?4:0;
			}
			return 0;
		case 8:
			if(lpItem->m_Index == GET_ITEM(13,2))
			{
				return (this->m_BlockHornUniria != 0)?5:0;
			}
			if(lpItem->m_Index == GET_ITEM(13,3))
			{
				return (this->m_BlockHornDinorant != 0)?6:0;
			}
			if(lpItem->m_Index == GET_ITEM(13,37))
			{
				return (this->m_BlockFenrir != 0)?7:0;
			}
			return 0;
	}

	return 0;
}

bool CDungeonRace::IsItemBlocked(LPOBJ lpObj,CItem* lpItem) // OK
{
	if(this->m_Switch == 0 || this->m_State < DR_STATE_REGISTER || lpObj->Type != OBJECT_USER)
	{
		return false;
	}

	CsLock lock(&this->m_cs);

	if(this->IsActiveRacer(lpObj) == false)
	{
		return false;
	}

	if(this->BlockedCategory(lpItem) == 0)
	{
		return false;
	}

	this->Notify(lpObj,"Dungeon Race: this item cannot be worn during the race.");

	return true;
}

void CDungeonRace::SendToStart(LPOBJ lpObj) // OK
{
	int x = this->m_StartX+(GetLargeRand()%3)-1;
	int y = this->m_StartY+(GetLargeRand()%3)-1;

	t_Internal++;
	gObjTeleport(lpObj->Index,this->m_StartMap,x,y);
	t_Internal--;
}

void CDungeonRace::SendToExit(LPOBJ lpObj) // OK
{
	int gate,map,x,y,dir,level;

	if(gGate.GetGate(17,&gate,&map,&x,&y,&dir,&level) == 0)
	{
		return;
	}

	t_Internal++;
	gObjTeleport(lpObj->Index,map,x,y);
	t_Internal--;
}

//////////////////////////////////////////////////////////////////////
// Schedule
//////////////////////////////////////////////////////////////////////

void CDungeonRace::SetNextStart() // OK
{
	this->m_NextStart = 0;

	if(this->m_Switch == 0 || this->m_Schedule == 0)
	{
		return;
	}

	time_t now = time(0);

	struct tm t;
	localtime_s(&t,&now);

	t.tm_hour = this->m_Hour;
	t.tm_min = this->m_Minute;
	t.tm_sec = 0;
	t.tm_isdst = -1;

	if(this->m_Schedule == 2)
	{
		t.tm_mday += (this->m_DayOfWeek-t.tm_wday+7)%7;
	}

	time_t next = mktime(&t);

	if(next <= now)
	{
		t.tm_mday += ((this->m_Schedule == 2)?7:1);
		t.tm_isdst = -1;
		next = mktime(&t);
	}

	this->m_NextStart = next;
}

void CDungeonRace::SpawnNpc() // OK
{
	if(this->m_NpcIndex != -1 && gObjIsConnected(this->m_NpcIndex) != 0 && gObj[this->m_NpcIndex].Class == this->m_NpcClass && gObj[this->m_NpcIndex].Map == this->m_NpcMap && gObj[this->m_NpcIndex].X == this->m_NpcX && gObj[this->m_NpcIndex].Y == this->m_NpcY)
	{
		return;
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
	LogAdd(LOG_BLUE,"[DungeonRace] NPC class %d spawned at map %d (%d,%d), index %d",this->m_NpcClass,this->m_NpcMap,this->m_NpcX,this->m_NpcY,index);
}

//////////////////////////////////////////////////////////////////////
// State machine
//////////////////////////////////////////////////////////////////////

void CDungeonRace::MainProc() // OK
{
	if((GetTickCount()-this->m_SecTick) < 1000)
	{
		return;
	}

	this->m_SecTick = GetTickCount();

	if(this->m_Switch == 0 || this->Served() == false)
	{
		if(this->m_State >= DR_STATE_REGISTER)
		{
			CsLock lock(&this->m_cs);
			this->CancelRace("the event was switched off");
		}

		this->m_State = DR_STATE_OFF;
		gServerDisplayer.EventDungeonRace = -1;
		return;
	}

	CsLock lock(&this->m_cs);

	if(this->m_State == DR_STATE_OFF)
	{
		this->m_State = DR_STATE_IDLE;
		this->SetNextStart();
	}

	this->SpawnNpc();

	gServerDisplayer.EventDungeonRace = this->GetDisplayTime();

	time_t now = time(0);

	switch(this->m_State)
	{
		case DR_STATE_IDLE:
		{
			if(this->m_Schedule != 0 && this->m_NextStart != 0 && now >= this->m_NextStart)
			{
				this->OpenRegistration();
			}

			break;
		}

		case DR_STATE_REGISTER:
		{
			this->WaitingTick();

			int remain = (int)(this->m_StateEnd-now);

			if(remain > 0 && (remain%60) == 0 && remain != this->m_LastAnnounce)
			{
				this->m_LastAnnounce = remain;
				this->Announce("[Dungeon Race] Registration closes in %d minute(s)! %d racer(s) so far. Talk to the registration NPC at %s.",remain/60,(int)this->m_Players.size(),this->NpcPlace());
			}
			else if((remain == 30 || remain == 10) && remain != this->m_LastAnnounce)
			{
				this->m_LastAnnounce = remain;
				this->Announce("[Dungeon Race] Registration closes in %d seconds!",remain);
			}

			if(now >= this->m_StateEnd)
			{
				this->CloseRegistration();
			}

			break;
		}

		case DR_STATE_COUNTDOWN:
		{
			this->WaitingTick();

			int remain = (int)(this->m_StateEnd-now);

			if((remain == 10 || (remain <= 5 && remain >= 1)) && remain != this->m_LastAnnounce)
			{
				this->m_LastAnnounce = remain;
				this->Announce("[Dungeon Race] Starts in %d...",remain);
			}

			if(now >= this->m_StateEnd)
			{
				this->StartRace();
			}

			break;
		}

		case DR_STATE_RACE:
		{
			this->RaceTick();
			break;
		}
	}

	this->ExitTick();
	this->SendHudTick();

	// stale NPC confirmations
	for(std::map<int,DWORD>::iterator it = this->m_Confirm.begin();it != this->m_Confirm.end();)
	{
		if((GetTickCount()-it->second) > 60000)
		{
			it = this->m_Confirm.erase(it);
		}
		else
		{
			++it;
		}
	}
}

// "Lorencia (127,229)": where the registration NPC really is, taken from the same settings the NPC is spawned from
const char* CDungeonRace::NpcPlace() // OK
{
	static char text[96];

	const char* name = gMapManager.GetMapName(this->m_NpcMap);

	if(name == 0 || name[0] == 0 || strcmp(name,"NoName") == 0)
	{
		sprintf_s(text,"map %d (%d,%d)",this->m_NpcMap,this->m_NpcX,this->m_NpcY);
	}
	else
	{
		sprintf_s(text,"%s (%d,%d)",name,this->m_NpcX,this->m_NpcY);
	}

	return text;
}

void CDungeonRace::OpenRegistration() // OK
{
	this->m_Players.clear();
	this->m_Pending.clear();
	this->m_Confirm.clear();
	this->m_FinishCount = 0;
	this->m_EndRequest = false;
	memset(this->m_WinnerName,0,sizeof(this->m_WinnerName));

	this->m_State = DR_STATE_REGISTER;
	this->m_StateEnd = time(0)+(time_t)this->m_RegisterMin*60;
	this->m_LastAnnounce = this->m_RegisterMin*60;

	this->Announce("[Dungeon Race] Registration is OPEN! It closes in %d minute(s). Talk to the registration NPC at %s to join.",this->m_RegisterMin,this->NpcPlace());

	LogAdd(LOG_BLUE,"[DungeonRace] registration open (%d minutes)",this->m_RegisterMin);
}

void CDungeonRace::CloseRegistration() // OK
{
	// drop racers who left the game
	int count = 0;

	for(std::map<int,DR_PLAYER>::iterator it = this->m_Players.begin();it != this->m_Players.end();++it)
	{
		if(it->second.Active == 1)
		{
			count++;
		}
	}

	if(count < this->m_MinPlayers)
	{
		char reason[96];
		sprintf_s(reason,"not enough racers (%d joined, %d needed)",count,this->m_MinPlayers);
		this->CancelRace(reason);
		return;
	}

	this->StartCountdown();
}

void CDungeonRace::StartCountdown() // OK
{
	int count = 0;

	for(std::map<int,DR_PLAYER>::iterator it = this->m_Players.begin();it != this->m_Players.end();++it)
	{
		if(it->second.Active == 1)
		{
			count++;
		}
	}

	this->m_State = DR_STATE_COUNTDOWN;
	this->m_StateEnd = time(0)+this->m_CountdownSec;
	this->m_LastAnnounce = -1;

	this->KickBystanders();
	this->SwapMonsters(true);

	this->Announce("[Dungeon Race] Registration is CLOSED with %d racer(s). The race starts in %d seconds!",count,this->m_CountdownSec);

	LogAdd(LOG_BLUE,"[DungeonRace] countdown, %d racers",count);
}

void CDungeonRace::StartRace() // OK
{
	this->m_State = DR_STATE_RACE;
	this->m_RaceStart = time(0);
	this->m_RaceStartTick = GetTickCount();
	this->m_StateEnd = this->m_RaceStart+(time_t)this->m_MaxMin*60;
	this->m_FinishCount = 0;

	this->Announce("[Dungeon Race] GO! The first %d to reach Dungeon 3 win!",this->m_Winners);

	LogAdd(LOG_BLUE,"[DungeonRace] race started");
}

void CDungeonRace::EndRace(const char* reason) // OK
{
	char line[256];

	if(this->m_FinishCount == 0)
	{
		sprintf_s(line,"[Dungeon Race] The race is over (%s). Nobody reached Dungeon 3.",reason);
	}
	else
	{
		sprintf_s(line,"[Dungeon Race] The race is over. Winners:");

		for(int n=0;n < this->m_FinishCount && n < 3;n++)
		{
			char part[48];
			sprintf_s(part," %d) %s",n+1,this->m_WinnerName[n]);
			strcat_s(line,part);
		}
	}

	this->Announce("%s",line);

	LogAdd(LOG_BLUE,"[DungeonRace] %s",line);

	this->Cleanup();
}

void CDungeonRace::CancelRace(const char* reason) // OK
{
	this->Announce("[Dungeon Race] Cancelled: %s.",reason);

	LogAdd(LOG_BLUE,"[DungeonRace] cancelled: %s",reason);

	this->Cleanup();
}

void CDungeonRace::Cleanup() // OK
{
	bool swapped = (this->m_State == DR_STATE_COUNTDOWN || this->m_State == DR_STATE_RACE);

	std::vector<int> ids;

	for(std::map<int,DR_PLAYER>::iterator it = this->m_Players.begin();it != this->m_Players.end();++it)
	{
		ids.push_back(it->first);
	}

	for(size_t n=0;n < ids.size();n++)
	{
		this->ReleasePlayer(ids[n],true);
	}

	this->m_Players.clear();
	this->m_Pending.clear();
	this->m_Confirm.clear();

	if(swapped)
	{
		this->SwapMonsters(false);
	}

	this->m_State = ((this->m_Switch != 0 && this->Served() != false)?DR_STATE_IDLE:DR_STATE_OFF);
	this->SetNextStart();
}

void CDungeonRace::ReleasePlayer(int aIndex,bool eject) // OK
{
	std::map<int,DR_PLAYER>::iterator it = this->m_Players.find(aIndex);

	if(it == this->m_Players.end())
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) != 0 && strcmp(lpObj->Name,it->second.Name) == 0)
	{
		it->second.Active = 0;

		this->RestoreItems(lpObj,&it->second);

		if(eject != false && lpObj->DieRegen == 0)
		{
			this->SendToExit(lpObj);
		}

		if(eject != false)
		{
			this->Notify(lpObj,"Dungeon Race: you are back in Lorencia.");
		}
	}

	this->m_Players.erase(aIndex);
}

void CDungeonRace::WaitingTick() // OK
{
	for(std::map<int,DR_PLAYER>::iterator it = this->m_Players.begin();it != this->m_Players.end();)
	{
		LPOBJ lpObj = &gObj[it->first];

		if(gObjIsConnectedGP(it->first) == 0 || strcmp(lpObj->Name,it->second.Name) != 0)
		{
			it = this->m_Players.erase(it);
			continue;
		}

		DR_PLAYER* p = &it->second;

		if(p->Active == 1 && lpObj->DieRegen == 0 && lpObj->Teleport == 0)
		{
			int dx = lpObj->X-this->m_StartX;
			int dy = lpObj->Y-this->m_StartY;

			if(lpObj->Map != this->m_StartMap || dx > this->m_StartRadius || dx < -this->m_StartRadius || dy > this->m_StartRadius || dy < -this->m_StartRadius)
			{
				if((GetTickCount()-p->JoinTick) > 3000)
				{
					p->JoinTick = GetTickCount();
					this->SendToStart(lpObj);
					this->Notify(lpObj,"Dungeon Race: wait at the starting point until the race begins.");
				}
			}
		}

		++it;
	}
}

void CDungeonRace::RaceTick() // OK
{
	std::vector<std::pair<DWORD,int> > arrived;

	for(std::map<int,DR_PLAYER>::iterator it = this->m_Players.begin();it != this->m_Players.end();)
	{
		LPOBJ lpObj = &gObj[it->first];

		if(gObjIsConnectedGP(it->first) == 0 || strcmp(lpObj->Name,it->second.Name) != 0)
		{
			it = this->m_Players.erase(it);
			continue;
		}

		DR_PLAYER* p = &it->second;

		if(p->Active == 1 && lpObj->Map == this->m_DungeonMap && lpObj->X >= this->m_FinishX1 && lpObj->X <= this->m_FinishX2 && lpObj->Y >= this->m_FinishY1 && lpObj->Y <= this->m_FinishY2)
		{
			if(p->ArriveTick == 0)
			{
				p->ArriveTick = GetTickCount();
			}

			arrived.push_back(std::make_pair(p->ArriveTick,it->first));
		}

		++it;
	}

	std::sort(arrived.begin(),arrived.end());

	for(size_t n=0;n < arrived.size() && this->m_FinishCount < this->m_Winners;n++)
	{
		std::map<int,DR_PLAYER>::iterator it = this->m_Players.find(arrived[n].second);

		if(it != this->m_Players.end())
		{
			this->DoFinish(arrived[n].second,&it->second);
		}
	}

	if(this->m_FinishCount >= this->m_Winners)
	{
		this->EndRace("all places taken");
		return;
	}

	bool anyRacer = false;

	for(std::map<int,DR_PLAYER>::iterator it = this->m_Players.begin();it != this->m_Players.end();++it)
	{
		if(it->second.Active == 1)
		{
			anyRacer = true;
			break;
		}
	}

	if(anyRacer == false)
	{
		this->EndRace("no racers left");
		return;
	}

	if(time(0) >= this->m_StateEnd)
	{
		this->EndRace("time is up");
	}
}

void CDungeonRace::DoFinish(int aIndex,DR_PLAYER* p) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	int rank = ++this->m_FinishCount;

	DWORD ms = p->ArriveTick-this->m_RaceStartTick;

	if(rank >= 1 && rank <= 3)
	{
		CopyText(this->m_WinnerName[rank-1],sizeof(this->m_WinnerName[rank-1]),lpObj->Name);
	}

	this->Announce("[Dungeon Race] %s reached Dungeon 3 in place %d! (%d:%02d)",lpObj->Name,rank,(int)(ms/60000),(int)((ms/1000)%60));

	p->Active = 2;
	p->Place = (BYTE)rank;
	p->ExitAt = time(0)+this->m_FinishExitDelaySec;

	this->RestoreItems(lpObj,p);
	this->GiveReward(lpObj,rank);

	LogAdd(LOG_BLUE,"[DungeonRace] %s finished place %d (%u ms)",lpObj->Name,rank,ms);
}

void CDungeonRace::ExitTick() // OK
{
	time_t now = time(0);

	// disqualified players: items back, then out
	std::vector<int> pending;
	pending.swap(this->m_Pending);

	for(size_t n=0;n < pending.size();n++)
	{
		std::map<int,DR_PLAYER>::iterator it = this->m_Players.find(pending[n]);

		if(it != this->m_Players.end() && it->second.Active == 0)
		{
			this->ReleasePlayer(pending[n],it->second.Eject);
		}
	}

	// finished players leave after their delay
	std::vector<int> leaving;

	for(std::map<int,DR_PLAYER>::iterator it = this->m_Players.begin();it != this->m_Players.end();++it)
	{
		if(it->second.Active == 2 && now >= it->second.ExitAt)
		{
			leaving.push_back(it->first);
		}
	}

	for(size_t n=0;n < leaving.size();n++)
	{
		this->ReleasePlayer(leaving[n],true);
	}
}

//////////////////////////////////////////////////////////////////////
// Players
//////////////////////////////////////////////////////////////////////

bool CDungeonRace::Dialog(LPOBJ lpObj,LPOBJ lpNpc) // OK
{
	if(this->m_Switch == 0 || lpNpc->Class != this->m_NpcClass || lpNpc->Map != this->m_NpcMap || lpNpc->X != this->m_NpcX || lpNpc->Y != this->m_NpcY)
	{
		return false;
	}

	if(lpObj->Interface.use != 0 || lpObj->PShopOpen != 0)
	{
		return true;
	}

	CsLock lock(&this->m_cs);

	// the window (Join / Leave buttons) shows every state, so the player also sees when the next race is
	LogAdd(LOG_BLUE,"[DungeonRace] NPC clicked by %s (state %d)",lpObj->Name,(int)this->m_State);
	this->SendOpen(lpObj);

	return true;
}

void CDungeonRace::SendOpen(LPOBJ lpObj) // OK
{
	PMSG_DR_OPEN_SEND pMsg;

	pMsg.header.set(0xD3,0x65,sizeof(pMsg));

	pMsg.State = (BYTE)this->m_State;
	pMsg.Mine = (this->IsActiveRacer(lpObj) != false)?1:0;
	pMsg.Winners = (BYTE)this->m_Winners;
	pMsg.Pad = 0;

	time_t now = time(0);
	time_t end = 0;

	if(this->m_State == DR_STATE_IDLE)
	{
		end = this->m_NextStart;
	}
	else if(this->m_State >= DR_STATE_REGISTER)
	{
		end = this->m_StateEnd;
	}

	pMsg.RemainSec = (WORD)((end > now)?min((long long)(end-now),(long long)65535):0);
	pMsg.Players = (WORD)this->m_Players.size();
	pMsg.MinPlayers = (WORD)this->m_MinPlayers;
	pMsg.MaxPlayers = (WORD)this->m_MaxPlayers;

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
}

void CDungeonRace::SendHudTick() // OK
{
	if(this->m_State < DR_STATE_REGISTER)
	{
		return;
	}

	time_t now = time(0);

	PMSG_DR_HUD_SEND pMsg;

	pMsg.header.set(0xD3,0x66,sizeof(pMsg));

	pMsg.State = (BYTE)this->m_State;
	pMsg.Finished = (BYTE)this->m_FinishCount;
	pMsg.RemainSec = (WORD)((this->m_StateEnd > now)?min((long long)(this->m_StateEnd-now),(long long)65535):0);
	pMsg.Players = (WORD)this->m_Players.size();

	for(std::map<int,DR_PLAYER>::iterator it = this->m_Players.begin();it != this->m_Players.end();++it)
	{
		if(it->second.Active != 1 && it->second.Active != 2)
		{
			continue;
		}

		if(gObjIsConnectedGP(it->first) == 0 || strcmp(gObj[it->first].Name,it->second.Name) != 0)
		{
			continue;
		}

		pMsg.Mine = (BYTE)it->second.Active;
		pMsg.Place = it->second.Place;

		DataSend(it->first,(BYTE*)&pMsg,pMsg.header.size);
	}
}

void CDungeonRace::CGActionRecv(PMSG_DR_ACTION_RECV* lpMsg,int aIndex) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0 || this->m_Switch == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Interface.use != 0 || lpObj->PShopOpen != 0)
	{
		return;
	}

	CsLock lock(&this->m_cs);

	if(this->m_State != DR_STATE_REGISTER)
	{
		this->Notify(lpObj,"Dungeon Race: registration is not open.");
		this->SendOpen(lpObj);
		return;
	}

	// the player must still be next to the NPC (the window is closed by the client when he walks away)
	if(lpObj->Map != this->m_NpcMap || abs(lpObj->X-this->m_NpcX) > 10 || abs(lpObj->Y-this->m_NpcY) > 10)
	{
		if(this->Find(lpObj) == 0)
		{
			return;
		}
	}

	if(lpMsg->Action == 0)
	{
		if(this->Find(lpObj) != 0)
		{
			this->Notify(lpObj,"Dungeon Race: you are already registered.");
		}
		else
		{
			this->Join(lpObj);
		}
	}
	else if(lpMsg->Action == 1)
	{
		DR_PLAYER* p = this->Find(lpObj);

		if(p != 0 && p->Active == 1)
		{
			this->ReleasePlayer(lpObj->Index,true);
			this->Notify(lpObj,"Dungeon Race: you left the race.");
		}
	}

	this->SendOpen(lpObj);
}

bool CDungeonRace::Join(LPOBJ lpObj) // OK
{
	if(this->m_State != DR_STATE_REGISTER)
	{
		return false;
	}

	if(lpObj->Level < this->m_MinLevel)
	{
		this->Notify(lpObj,"Dungeon Race: you need level %d.",this->m_MinLevel);
		return false;
	}

	if((int)this->m_Players.size() >= this->m_MaxPlayers)
	{
		this->Notify(lpObj,"Dungeon Race: the race is full.");
		return false;
	}

	if(lpObj->DieRegen != 0 || lpObj->Teleport != 0 || OBJECT_RANGE(lpObj->DuelUser) != 0 || (lpObj->Guild != 0 && lpObj->Guild->WarState == 1))
	{
		this->Notify(lpObj,"Dungeon Race: you cannot join right now.");
		return false;
	}

	if(BC_MAP_RANGE(lpObj->Map) != 0 || DS_MAP_RANGE(lpObj->Map) != 0 || CC_MAP_RANGE(lpObj->Map) != 0 || IT_MAP_RANGE(lpObj->Map) != 0 || KALIMA_MAP_RANGE(lpObj->Map) != 0 || CA_MAP_RANGE(lpObj->Map) != 0)
	{
		this->Notify(lpObj,"Dungeon Race: leave the event map first.");
		return false;
	}

	DR_PLAYER np;
	memset(&np.Name,0,sizeof(np.Name));
	CopyText(np.Name,sizeof(np.Name),lpObj->Name);
	np.Active = 1;
	np.Eject = false;
	np.Place = 0;
	np.ArriveTick = 0;
	np.JoinTick = GetTickCount();
	np.ExitAt = 0;

	if(this->UnequipBlocked(lpObj,&np) == false)
	{
		this->Notify(lpObj,"Dungeon Race: no room in your inventory for the items that must come off. Free some cells and try again.");
		return false;
	}

	this->m_Players[lpObj->Index] = np;

	gObjectManager.CharacterCalcAttribute(lpObj->Index);

	this->SendToStart(lpObj);

	this->Notify(lpObj,"Dungeon Race: you are in! Wait at the starting point. %d racer(s) so far.",(int)this->m_Players.size());

	LogAdd(LOG_BLUE,"[DungeonRace] %s joined (%d items off)",lpObj->Name,(int)np.Saved.size());

	return true;
}

bool CDungeonRace::UnequipBlocked(LPOBJ lpObj,DR_PLAYER* p) // OK
{
	static const int slots[] = {0,1,7,8,9,10,11};

	for(int i=0;i < (int)(sizeof(slots)/sizeof(slots[0]));i++)
	{
		int slot = slots[i];

		CItem* lpItem = &lpObj->Inventory[slot];

		if(lpItem->IsItem() == 0 || this->BlockedCategory(lpItem) == 0)
		{
			continue;
		}

		BYTE target = gItemManager.InventoryCheckInsertItem(lpObj->Index,lpItem->m_Index);

		if(target == 0xFF)
		{
			this->RestoreItems(lpObj,p);
			return false;
		}

		DR_SAVED_ITEM saved;
		saved.Serial = lpItem->m_Serial;
		saved.ItemIndex = lpItem->m_Index;
		saved.Level = lpItem->m_Level;
		saved.Slot = slot;

		PMSG_ITEM_MOVE_RECV msg;
		memset(&msg,0,sizeof(msg));
		msg.header.set(0x24,sizeof(msg));
		msg.SourceFlag = 0;
		msg.SourceSlot = (BYTE)slot;
		msg.TargetFlag = 0;
		msg.TargetSlot = target;

		gItemManager.CGItemMoveRecv(&msg,lpObj->Index);

		if(lpObj->Inventory[slot].IsItem() != 0)
		{
			this->RestoreItems(lpObj,p);
			return false;
		}

		p->Saved.push_back(saved);
	}

	return true;
}

void CDungeonRace::RestoreItems(LPOBJ lpObj,DR_PLAYER* p) // OK
{
	if(p->Saved.empty())
	{
		return;
	}

	int left = 0;
	int max = gItemManager.GetInventoryMaxValue(lpObj);

	for(size_t i=0;i < p->Saved.size();i++)
	{
		DR_SAVED_ITEM* s = &p->Saved[i];

		if(lpObj->Inventory[s->Slot].IsItem() != 0)
		{
			left++; // the slot was taken in the meantime: the item stays in the bag
			continue;
		}

		int src = -1;

		for(int n=INVENTORY_WEAR_SIZE;n < max;n++)
		{
			CItem* it = &lpObj->Inventory[n];

			if(it->IsItem() != 0 && it->m_Index == s->ItemIndex && it->m_Serial == s->Serial && it->m_Level == s->Level)
			{
				src = n;
				break;
			}
		}

		if(src < 0)
		{
			continue; // gone (traded / dropped): nothing to restore
		}

		PMSG_ITEM_MOVE_RECV msg;
		memset(&msg,0,sizeof(msg));
		msg.header.set(0x24,sizeof(msg));
		msg.SourceFlag = 0;
		msg.SourceSlot = (BYTE)src;
		msg.TargetFlag = 0;
		msg.TargetSlot = (BYTE)s->Slot;

		gItemManager.CGItemMoveRecv(&msg,lpObj->Index);

		if(lpObj->Inventory[src].IsItem() != 0 && lpObj->Inventory[s->Slot].IsItem() == 0)
		{
			left++;
		}
	}

	p->Saved.clear();

	gObjectManager.CharacterCalcAttribute(lpObj->Index);

	if(left > 0)
	{
		this->Notify(lpObj,"Dungeon Race: %d item(s) are in your inventory, wear them again.",left);
	}
	else
	{
		this->Notify(lpObj,"Dungeon Race: your equipment is back on.");
	}
}

void CDungeonRace::Disqualify(LPOBJ lpObj,const char* reason,bool eject) // OK
{
	DR_PLAYER* p = this->Find(lpObj);

	if(p == 0 || p->Active != 1)
	{
		return;
	}

	p->Active = 0;
	p->Eject = eject;

	this->m_Pending.push_back(lpObj->Index);

	this->Notify(lpObj,"Dungeon Race: you are DISQUALIFIED (%s).",reason);

	LogAdd(LOG_BLUE,"[DungeonRace] %s disqualified: %s",lpObj->Name,reason);
}

void CDungeonRace::GiveReward(LPOBJ lpObj,int rank) // OK
{
	if(rank < 1 || rank > 3)
	{
		return;
	}

	// the item bag (EventItemBag); its DropInventory column decides inventory or ground
	if(gItemBagManager.DropItemBySpecialValue(this->m_Bag[rank-1],lpObj,lpObj->Map,lpObj->X,lpObj->Y) == 0)
	{
		LogAdd(LOG_RED,"[DungeonRace] no item bag with SpecialValue %d in EventItemBagManager.txt (place %d)",this->m_Bag[rank-1],rank);
	}

	if(this->m_Zen[rank-1] != 0)
	{
		DWORD total = lpObj->Money+this->m_Zen[rank-1];

		if(total < lpObj->Money || total > 2000000000)
		{
			total = 2000000000;
		}

		lpObj->Money = total;
		GCMoneySend(lpObj->Index,lpObj->Money);
	}

	this->Notify(lpObj,"Dungeon Race: you won place %d! Your reward is in your inventory.",rank);
}

//////////////////////////////////////////////////////////////////////
// Warp rules
//////////////////////////////////////////////////////////////////////

void CDungeonRace::BeginGateStep() // OK
{
	t_GateStep++;
}

void CDungeonRace::EndGateStep() // OK
{
	if(t_GateStep > 0){t_GateStep--;}
}

int CDungeonRace::WarpFilter(LPOBJ lpObj,int map,int x,int y) // OK
{
	if(this->m_Switch == 0 || this->m_State < DR_STATE_REGISTER || lpObj->Type != OBJECT_USER || t_Internal != 0)
	{
		return 0;
	}

	CsLock lock(&this->m_cs);

	bool fromGate = (t_GateStep != 0);
	bool raceMap = (map == this->m_StartMap || map == this->m_DungeonMap);

	DR_PLAYER* p = this->Find(lpObj);

	if(p != 0)
	{
		if(p->Active != 1)
		{
			return 0; // finished or already disqualified
		}

		int dx = lpObj->X-x;
		int dy = lpObj->Y-y;
		int jump = (int)sqrt((double)(dx*dx+dy*dy));

		if(this->m_State == DR_STATE_REGISTER || this->m_State == DR_STATE_COUNTDOWN)
		{
			if(fromGate)
			{
				this->Notify(lpObj,"Dungeon Race: wait for the race to start.");
				return 1;
			}

			if(map == lpObj->Map && jump <= this->m_MaxJump)
			{
				return 0;
			}

			if(this->m_WarpDisqualify == 0)
			{
				this->Notify(lpObj,"Dungeon Race: you cannot warp during the race.");
				return 1;
			}

			this->Disqualify(lpObj,"warped away before the start",raceMap);

			return (raceMap)?1:0;
		}

		// RACE
		if(fromGate)
		{
			if(raceMap)
			{
				return 0;
			}

			if(this->m_WarpDisqualify == 0)
			{
				this->Notify(lpObj,"Dungeon Race: you cannot leave the race map.");
				return 1;
			}

			this->Disqualify(lpObj,"left the race maps",false);

			return 0;
		}

		if(map == lpObj->Map && jump <= this->m_MaxJump)
		{
			return 0; // short move (skill / summon inside the same map)
		}

		if(this->m_WarpDisqualify == 0)
		{
			this->Notify(lpObj,"Dungeon Race: you cannot warp during the race.");
			return 1;
		}

		this->Disqualify(lpObj,"warped during the race",raceMap);

		return (raceMap)?1:0;
	}

	// everybody else: the Dungeon is closed while the race runs
	if(this->m_LockDungeon != 0 && (this->m_State == DR_STATE_COUNTDOWN || this->m_State == DR_STATE_RACE) && map == this->m_DungeonMap && lpObj->Map != this->m_DungeonMap)
	{
		if(this->m_GMBypass != 0 && gGameMaster.CheckGameMasterLevel(lpObj,1) != 0)
		{
			return 0;
		}

		this->Notify(lpObj,"The Dungeon is closed: the Dungeon Race is running.");

		return 1;
	}

	return 0;
}

bool CDungeonRace::GetUserRespawnLocation(LPOBJ lpObj,int* gate,int* map,int* x,int* y,int* dir,int* level) // OK
{
	if(this->m_Switch == 0 || this->m_RespawnAtStart == 0 || this->m_State < DR_STATE_REGISTER || lpObj->Type != OBJECT_USER)
	{
		return false;
	}

	CsLock lock(&this->m_cs);

	if(this->IsActiveRacer(lpObj) == false)
	{
		return false;
	}

	*gate = 0;
	*map = this->m_StartMap;
	*x = this->m_StartX+(GetLargeRand()%3)-1;
	*y = this->m_StartY+(GetLargeRand()%3)-1;
	*dir = 3;
	*level = 0;

	return true;
}

void CDungeonRace::OnEnterMap(LPOBJ lpObj) // OK
{
	if(this->m_Switch == 0 || this->m_State != DR_STATE_RACE || lpObj->Type != OBJECT_USER || lpObj->Map != this->m_DungeonMap)
	{
		return;
	}

	CsLock lock(&this->m_cs);

	DR_PLAYER* p = this->Find(lpObj);

	if(p != 0 && p->Active == 1 && p->ArriveTick == 0 && lpObj->X >= this->m_FinishX1 && lpObj->X <= this->m_FinishX2 && lpObj->Y >= this->m_FinishY1 && lpObj->Y <= this->m_FinishY2)
	{
		p->ArriveTick = GetTickCount(); // exact arrival time; the 1 second tick does the ranking
	}
}

void CDungeonRace::OnUserLogout(LPOBJ lpObj) // OK
{
	if(this->m_Switch == 0 || this->m_State < DR_STATE_REGISTER || lpObj->Type != OBJECT_USER)
	{
		return;
	}

	CsLock lock(&this->m_cs);

	DR_PLAYER* p = this->Find(lpObj);

	if(p == 0)
	{
		return;
	}

	p->Active = 0;

	this->RestoreItems(lpObj,p);

	this->m_Players.erase(lpObj->Index);
}

//////////////////////////////////////////////////////////////////////
// Monsters
//////////////////////////////////////////////////////////////////////

void CDungeonRace::KickBystanders() // OK
{
	if(this->m_LockDungeon == 0)
	{
		return;
	}

	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) == 0 || gObj[n].Type != OBJECT_USER || gObj[n].Map != this->m_DungeonMap)
		{
			continue;
		}

		if(this->IsActiveRacer(&gObj[n]) != false)
		{
			continue;
		}

		if(this->m_GMBypass != 0 && gGameMaster.CheckGameMasterLevel(&gObj[n],1) != 0)
		{
			continue;
		}

		this->Notify(&gObj[n],"The Dungeon Race is starting: the Dungeon is closed.");
		this->SendToExit(&gObj[n]);
	}
}

void CDungeonRace::SwapMonsters(bool race) // OK
{
	int removed = 0;
	int spawned = 0;

	for(int n=0;n < MAX_OBJECT_MONSTER;n++)
	{
		if(n != this->m_NpcIndex && gObjIsConnected(n) != 0 && gObj[n].Type == OBJECT_MONSTER && gObj[n].Map == this->m_DungeonMap && gObj[n].SummonIndex == -1)
		{
			gObjDel(n);
			removed++;
		}
	}

	for(int n=0;n < gMonsterSetBase.m_count;n++)
	{
		MONSTER_SET_BASE_INFO* lpInfo = &gMonsterSetBase.m_MonsterSetBaseInfo[n];

		if(lpInfo->Map != this->m_DungeonMap || lpInfo->Type == 3 || lpInfo->Type == 4)
		{
			continue;
		}

		int cls = lpInfo->MonsterClass;

		if(race != false)
		{
			std::map<int,int>::iterator it = this->m_MonsterMap.find(cls);

			if(it != this->m_MonsterMap.end())
			{
				cls = it->second;
			}
		}

		int index = gObjAddMonster(lpInfo->Map);

		if(OBJECT_RANGE(index) == 0)
		{
			break;
		}

		if(gObjSetPosMonster(index,n) == 0 || gObjSetMonster(index,cls) == 0)
		{
			gObjDel(index);
			continue;
		}

		if(gObj[index].Type != OBJECT_MONSTER)
		{
			gObjDel(index); // NPCs were never removed: drop the duplicate
			continue;
		}

		spawned++;
	}

	LogAdd(LOG_BLUE,"[DungeonRace] map %d monsters: %d removed, %d spawned (%s)",this->m_DungeonMap,removed,spawned,(race != false)?"race monsters":"normal monsters");
}

//////////////////////////////////////////////////////////////////////
// Monster effects
//////////////////////////////////////////////////////////////////////

void CDungeonRace::OnMonsterHit(LPOBJ lpMonster,LPOBJ lpTarget) // OK
{
	if(this->m_State != DR_STATE_RACE || this->m_Switch == 0 || lpMonster->Type != OBJECT_MONSTER || lpTarget->Type != OBJECT_USER)
	{
		return;
	}

	CsLock lock(&this->m_cs);

	if(this->IsActiveRacer(lpTarget) == false)
	{
		return;
	}

	if(this->m_IceClasses.find(lpMonster->Class) != this->m_IceClasses.end() && (GetLargeRand()%100) < this->m_IceChance)
	{
		gEffectManager.AddEffect(lpTarget,0,EFFECT_ICE,this->m_IceSeconds,0,0,0,0);
	}

	if(this->m_LightningClasses.find(lpMonster->Class) != this->m_LightningClasses.end() && (GetLargeRand()%100) < this->m_LightningChance)
	{
		this->Lightning(lpTarget,lpMonster);
	}
}

void CDungeonRace::Lightning(LPOBJ lpTarget,LPOBJ lpMonster) // OK
{
	int mode = this->m_LightningMode;

	if(mode == 2)
	{
		mode = GetLargeRand()%2;
	}

	if(mode == 0)
	{
		gObjBackSpring2(lpTarget,lpMonster,this->m_LightningTiles);
		return;
	}

	// pull: the same step logic as gObjBackSpring2 with the direction reversed, never onto the monster's own tile
	if(MAP_RANGE(lpTarget->Map) == 0 || lpTarget->Teleport != 0)
	{
		return;
	}

	int dir = GetPathPacketDirPos(lpMonster->X-lpTarget->X,lpMonster->Y-lpTarget->Y)*2;

	int x = lpTarget->X;
	int y = lpTarget->Y;

	for(int n=0;n < this->m_LightningTiles;n++)
	{
		int px = x;
		int py = y;

		if(BackSpringCheck(x,y,dir,lpTarget->Map) == FALSE)
		{
			break;
		}

		if(x == lpMonster->X && y == lpMonster->Y)
		{
			x = px;
			y = py;
			break;
		}
	}

	if(x == lpTarget->X && y == lpTarget->Y)
	{
		return;
	}

	PMSG_POSITION_RECV pMsg;

	pMsg.header.set(PROTOCOL_CODE3,sizeof(pMsg));

	pMsg.x = (BYTE)x;
	pMsg.y = (BYTE)y;

	lpTarget->Rest = 0;
	gMap[lpTarget->Map].DelStandAttr(lpTarget->OldX,lpTarget->OldY);
	gMap[lpTarget->Map].SetStandAttr(x,y);
	lpTarget->OldX = x;
	lpTarget->OldY = y;
	CGPositionRecv(&pMsg,lpTarget->Index);
}

//////////////////////////////////////////////////////////////////////
// GM command
//////////////////////////////////////////////////////////////////////

int CDungeonRace::GetDisplayTime() // OK
{
	if(this->m_Switch == 0 || this->Served() == false)
	{
		return -1;
	}

	switch(this->m_State)
	{
		case DR_STATE_IDLE:
			return (this->m_NextStart != 0)?max((int)(this->m_NextStart-time(0)),1):-1;
		case DR_STATE_REGISTER:
		case DR_STATE_COUNTDOWN:
		case DR_STATE_RACE:
			return 0;
	}

	return -1;
}

void CDungeonRace::MenuCommand(const char* arg) // OK
{
	if(this->m_Switch == 0)
	{
		LogAdd(LOG_RED,"[DungeonRace] DungeonRaceSwitch is 0 in CustomConfig.ini: nothing started.");
		return;
	}

	CsLock lock(&this->m_cs);

	if(this->Served() == false)
	{
		LogAdd(LOG_RED,"[DungeonRace] this server does not run the race (DungeonRaceServer / the Lorencia + Dungeon maps).");
		return;
	}

	if(_stricmp(arg,"open") == 0)
	{
		if(this->m_State == DR_STATE_IDLE || this->m_State == DR_STATE_OFF)
		{
			this->OpenRegistration(); // registration for DungeonRaceRegisterMin, then the countdown and the race start by themselves
		}
		else
		{
			LogAdd(LOG_RED,"[DungeonRace] a race is already running (state %d).",(int)this->m_State);
		}
	}
	else if(_stricmp(arg,"start") == 0)
	{
		if(this->m_State == DR_STATE_REGISTER)
		{
			this->CloseRegistration(); // closes registration now: countdown, or cancelled when below the minimum
		}
		else
		{
			LogAdd(LOG_RED,"[DungeonRace] start now needs an open registration (state %d).",(int)this->m_State);
		}
	}
	else if(_stricmp(arg,"end") == 0 || _stricmp(arg,"cancel") == 0)
	{
		if(this->m_State == DR_STATE_RACE)
		{
			this->EndRace("ended by a GM");
		}
		else if(this->m_State >= DR_STATE_REGISTER)
		{
			this->CancelRace("cancelled by a GM");
		}
	}
}

bool CDungeonRace::Command(LPOBJ lpObj,char* arg) // OK
{
	// Players: /drace (or /drace window) opens the window, /drace join, /drace leave. Works from anywhere, so a player who cannot
	// click the NPC (character standing on it, touch screen) can still register.
	if(gGameMaster.CheckGameMasterLevel(lpObj,1) == 0 || _stricmp(arg,"join") == 0 || _stricmp(arg,"leave") == 0 || _stricmp(arg,"window") == 0 || arg[0] == 0)
	{
		if(this->m_Switch == 0 || this->Served() == false)
		{
			this->Notify(lpObj,"Dungeon Race is not running on this server.");
			return true;
		}

		if(lpObj->Interface.use != 0 || lpObj->PShopOpen != 0)
		{
			return true;
		}

		CsLock lock(&this->m_cs);

		if(_stricmp(arg,"join") == 0)
		{
			if(this->m_State != DR_STATE_REGISTER)
			{
				this->Notify(lpObj,"Dungeon Race: registration is not open.");
			}
			else if(this->Find(lpObj) != 0)
			{
				this->Notify(lpObj,"Dungeon Race: you are already registered.");
			}
			else
			{
				this->Join(lpObj);
			}
		}
		else if(_stricmp(arg,"leave") == 0)
		{
			DR_PLAYER* p = this->Find(lpObj);

			if(this->m_State == DR_STATE_REGISTER && p != 0 && p->Active == 1)
			{
				this->ReleasePlayer(lpObj->Index,true);
				this->Notify(lpObj,"Dungeon Race: you left the race.");
			}
		}
		else
		{
			this->Notify(lpObj,"Dungeon Race: %s. Commands: /drace join, /drace leave.",(this->m_State == DR_STATE_REGISTER)?"registration is OPEN":((this->m_State == DR_STATE_IDLE)?"no race open right now":"a race is running"));
		}

		this->SendOpen(lpObj);

		return true;
	}

	if(gGameMaster.CheckGameMasterLevel(lpObj,1) == 0)
	{
		return false;
	}

	if(this->m_Switch == 0)
	{
		this->Notify(lpObj,"[DungeonRace] DungeonRaceSwitch is 0 in CustomConfig.ini.");
		return true;
	}

	CsLock lock(&this->m_cs);

	if(this->Served() == false)
	{
		this->Notify(lpObj,"[DungeonRace] this server does not run the race (DungeonRaceServer / map).");
		return true;
	}

	this->MenuCommand(arg);

	this->Notify(lpObj,"[DungeonRace] state %d, %d racer(s), %d finished, next start %s",(int)this->m_State,(int)this->m_Players.size(),this->m_FinishCount,(this->m_NextStart != 0)?"scheduled":"-");

	return true;
}
