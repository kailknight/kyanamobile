// CustomMining.h - "Tarkan Mining" event. Config: Data\Custom\CustomMining.txt (every number lives there).
//
// A scheduled, announced event on Tarkan: blue / purple / gold Mining Stones (NPC classes 616-618) appear in phases. A player with
// the Pick Axe equipped clicks a stone, confirms "Mine this stone?" and then gambles for jewels step by step. Each attempt costs Pick Axe
// durability; a success raises the jewel total and asks "Continue?", a failure loses everything that stone gave. Every result is decided
// here: the client only shows the window and the progress bar.
#pragma once

#include "Protocol.h"
#include "User.h"
#include <map>
#include <string>
#include <vector>

#define CM_MAX_TIERS 3
#define CM_MAX_STEPS 8
#define CM_MAX_START 16
#define CM_MAX_JEWELS 12

// C1:D3:6D server -> client Code
enum eMiningCode
{
	CM_CODE_ASK = 0,      // "Mine this stone?"
	CM_CODE_CONTINUE = 1, // a success: pending jewels, "Continue / Cash out?"
	CM_CODE_MINING = 2,   // attempt running: swing animation + progress bar for AttemptMs
	CM_CODE_FAIL = 3,     // lost everything
	CM_CODE_CASHED = 4,   // paid out (Pending jewels of JewelItem)
	CM_CODE_ERROR = 5,    // refused, the reason went out as a chat notice; the client just resets
	CM_CODE_CLOSE = 6,    // close the window
};

enum eMiningState
{
	CM_STATE_OFF = 0,
	CM_STATE_IDLE = 1,    // waiting for the next start time
	CM_STATE_RUNNING = 2,
};

#pragma pack(push,1)

// C1:D3:6D - the mining window / result (server -> client)
struct PMSG_CM_STATE_SEND
{
	PSBMSG_HEAD header;
	BYTE Code;        // eMiningCode
	BYTE Class;       // colour of the stone: 0 blue, 1 purple, 2 gold
	BYTE Step;        // successes so far
	BYTE Steps;       // steps this stone has
	BYTE Err;         // reserved
	WORD JewelItem;   // item index of the jewel (0 until the first success)
	WORD Pending;     // jewels won so far (CASHED: the amount paid)
	WORD NextTotal;   // jewels after the next success
	WORD Chance;      // chance of the next attempt x100
	WORD AttemptMs;   // length of the mining bar
	BYTE DurCost;     // Pick Axe durability the next attempt costs
	BYTE PickDur;     // Pick Axe durability left
};

// C1:D3:6E - the event line, once a second to everybody on the map (server -> client)
struct PMSG_CM_HUD_SEND
{
	PSBMSG_HEAD header;
	BYTE Phase;       // 0 none, 1 blue, 2 purple, 3 gold
	BYTE Pad;
	WORD RemainSec;   // seconds until the event ends
};

// C1:D3:6D - the window buttons (client -> server, unencrypted)
struct PMSG_CM_ACTION_RECV
{
	PSBMSG_HEAD header;
	BYTE Action;      // 0 mine / continue, 1 cash out, 2 close the window, 3 the progress bar finished
};

#pragma pack(pop)

static_assert(sizeof(PMSG_CM_STATE_SEND) == 4+5+10+2,"CM state size");
static_assert(sizeof(PMSG_CM_HUD_SEND) == 4+4,"CM hud size");
static_assert(sizeof(PMSG_CM_ACTION_RECV) == 5,"CM action size");

struct CM_TIER
{
	int Class;
	int MaxAlive;
	int SpawnIntervalSec;
	int SpawnCount[3]; // stones added each interval while the blue / purple / gold phase is the current one
	int DurCost;
	int PhaseFrom;   // seconds after the event start
	int PhaseTo;
	int Steps;
	int Chance[CM_MAX_STEPS]; // x100 (47.5 % = 4750)
	int Total[CM_MAX_STEPS];  // jewels after that many successes
	DWORD NextSpawn;
};

struct CM_STONE
{
	int Index;
	int Tier;
	int X;
	int Y;
	int Owner;       // gObj index of the miner, -1 free
	char OwnerName[11];
};

struct CM_SESSION
{
	char Name[11];
	int Stone;
	int Tier;
	int Step;
	int JewelItem;
	int Pending;
	bool Busy;        // an attempt is running (bar on the client)
	bool Won;         // the hidden outcome of the running attempt
	DWORD StartTick;
	DWORD DueTick;
	DWORD IdleTick;
};

class CCustomMining
{
public:
	CCustomMining();
	~CCustomMining();

	void Load(char* path);             // Data\Custom\CustomMining.txt, also called by /reload
	void MainProc();                   // every GameServer, from the 1 second user loop (User.cpp)

	bool Dialog(LPOBJ lpObj,LPOBJ lpNpc);                        // NpcTalk.cpp: true when the NPC is one of the stone classes
	void CGActionRecv(PMSG_CM_ACTION_RECV* lpMsg,int aIndex);    // C1:D3:6D
	void OnUserLogout(LPOBJ lpObj);                              // gObjDel: pay out what the player has won

	bool Command(LPOBJ lpObj,char* arg);   // GM: /mining start|stop|status|spawn <colour>|test <colour> <n>
	void MenuCommand(const char* arg);     // the GameServer window, Events menu (start / stop)
	int GetDisplayTime();                  // the CUSTOM EVENT window: -1 off, 0 running, N seconds to the next start

private:
	// helpers
	void Announce(const char* format,...);
	void Notify(LPOBJ lpObj,const char* format,...);
	bool Served();
	int TierOfClass(int cls);
	int TierByName(const char* name);
	int ParseInts(const char* text,int* out,int max);
	void SetDefaults();
	void SetNextStart();

	// state machine (MainProc thread, m_cs held)
	void StartEvent();
	void EndEvent(const char* reason);
	int CurrentPhase();
	bool IsStoneValid(CM_STONE* s);
	void SpawnTick();
	bool SpawnStone(int tier);
	void RemoveStone(int index);
	CM_STONE* FindStone(int index);
	CM_SESSION* FindSession(LPOBJ lpObj);
	int AliveCount(int tier);
	void SendHud();
	void SessionTick();

	// player flow (m_cs held)
	void SendState(LPOBJ lpObj,CM_SESSION* s,int code);
	bool CheckRequirements(LPOBJ lpObj,CM_STONE* stone,int tier,bool checkDur);
	void StartAttempt(LPOBJ lpObj,CM_SESSION* s);
	bool ApplyOutcome(CM_SESSION* s);
	void Resolve(LPOBJ lpObj,CM_SESSION* s);
	bool CashOut(LPOBJ lpObj,CM_SESSION* s,bool forced);
	void EndSession(int aIndex,bool removeStone);
	int RollJewel();
	const char* JewelName(int item);
	bool GrantJewels(LPOBJ lpObj,int item,int count,bool forced);
	void ForceCashAll();
	void RunTest(LPOBJ lpObj,int tier,int sessions);

private:
	CRITICAL_SECTION m_cs;

	// config
	int m_Switch;
	int m_ServerFilter;
	int m_Map;
	int m_Duration;
	int m_StartCount;
	int m_StartDay[CM_MAX_START];    // -1 = every day, 0-6 Sunday-Saturday
	int m_StartHour[CM_MAX_START];
	int m_StartMinute[CM_MAX_START];
	int m_AreaX;
	int m_AreaY;
	int m_AreaRange;
	int m_MinGap;
	int m_MaxStones; // all colours together
	int m_MaxDistance;
	int m_AttemptMs;
	int m_PickAxe;
	int m_UseJewelBank;
	int m_IdleCashoutSec;
	int m_JewelCount;
	int m_JewelItem[CM_MAX_JEWELS];
	int m_JewelWeight[CM_MAX_JEWELS];
	CM_TIER m_Tier[CM_MAX_TIERS];
	char m_TextStart[160];
	char m_TextBlue[160];
	char m_TextPurple[160];
	char m_TextGold[160];
	char m_TextOneMin[160];
	char m_TextEnd[160];

	// state
	volatile int m_State;
	time_t m_NextStart;
	time_t m_EventStart;
	time_t m_EventEnd;
	DWORD m_SecTick;
	int m_LastPhase;
	bool m_OneMinDone;
	std::vector<CM_STONE> m_Stones;
	std::map<int,CM_SESSION> m_Sessions;  // key = gObj index

	// statistics of the running event, announced when it ends
	int m_StatJewels[CM_MAX_JEWELS];      // jewels paid out, per m_JewelItem entry
	int m_StatOtherJewels;                // paid out jewels that are not in the weight list (config changed meanwhile)
	int m_StatStones;                     // stones finished (cashed out or crumbled)
	int m_StatLostStones;                 // stones that crumbled
	int m_StatLostJewels;                 // jewels lost with them
	std::map<std::string,int> m_StatMiner; // jewels paid out per character
	void ResetStats();
	void AnnounceStats();
};

extern CCustomMining gCustomMining;
