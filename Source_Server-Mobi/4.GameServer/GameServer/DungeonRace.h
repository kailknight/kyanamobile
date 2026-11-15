// DungeonRace.h - "Dungeon Race" event (1.0.50). Design: Working Project\MD Files\DungeonRace-DESIGN.md.
//
// A per-GameServer event (no DataServer): players join through an NPC in Lorencia, are taken to the start point with their
// wings / weapons / rings / pendants / Uniria / Dinorant taken off (moved into the bag, put back by serial afterwards), and race
// through the Dungeon (map 1) where the monsters are swapped for race monsters (ice slow, Thunder Lich push/pull). The first
// DungeonRaceWinners players to stand in the Dungeon 3 arrival box win. Keys are read from CustomConfig.ini ([CustomConfig]).
#pragma once

#include "Protocol.h"
#include "User.h"
#include <map>
#include <set>
#include <vector>

enum eDungeonRaceState
{
	DR_STATE_OFF = 0,
	DR_STATE_IDLE = 1,      // waiting for the next start time
	DR_STATE_REGISTER = 2,  // registration open, joined players wait at the start
	DR_STATE_COUNTDOWN = 3, // registration closed, monsters swapped, waiting for GO
	DR_STATE_RACE = 4,      // running
};

#pragma pack(push,1)

// C1:D3:65 - the NPC window (server -> client). Mine: 0 not joined, 1 racing.
struct PMSG_DR_OPEN_SEND
{
	PSBMSG_HEAD header;
	BYTE State;      // eDungeonRaceState
	BYTE Mine;
	BYTE Winners;
	BYTE Pad;
	WORD RemainSec;  // seconds left in the current state (0 when not timed)
	WORD Players;
	WORD MinPlayers;
	WORD MaxPlayers;
};

// C1:D3:66 - the on-screen counter, once a second for a racer (server -> client)
struct PMSG_DR_HUD_SEND
{
	PSBMSG_HEAD header;
	BYTE State;
	BYTE Mine;       // 1 racing, 2 finished
	BYTE Place;      // own place when finished
	BYTE Finished;   // how many finished so far
	WORD RemainSec;
	WORD Players;
};

// C1:D3:67 - the window buttons (client -> server, unencrypted)
struct PMSG_DR_ACTION_RECV
{
	PSBMSG_HEAD header;
	BYTE Action;     // 0 = join, 1 = leave
};

#pragma pack(pop)

static_assert(sizeof(PMSG_DR_OPEN_SEND) == 4+4+8,"DR open size");
static_assert(sizeof(PMSG_DR_HUD_SEND) == 4+4+4,"DR hud size");
static_assert(sizeof(PMSG_DR_ACTION_RECV) == 5,"DR action size");

struct DR_SAVED_ITEM
{
	DWORD Serial;
	int ItemIndex;
	int Level;
	int Slot; // wear slot it came from
};

struct DR_PLAYER
{
	char Name[11];
	int Active;          // 1 racing, 2 finished (waiting to be sent out), 0 disqualified (waiting for the restore)
	BYTE Place;          // 1-3 once finished
	bool Eject;          // send to the exit once the items are back
	DWORD ArriveTick;    // when the player landed in the finish box (0 = not yet)
	DWORD JoinTick;
	time_t ExitAt;       // finished players leave at this time
	std::vector<DR_SAVED_ITEM> Saved;
};

class CDungeonRace
{
public:
	CDungeonRace();
	~CDungeonRace();

	void Load(char* section,char* path); // CustomConfig.ini

	void MainProc(); // every GameServer, from the 1 second user loop (User.cpp)

	// NPC click (NpcTalk.cpp): true when the NPC is the Dungeon Race NPC.
	bool Dialog(LPOBJ lpObj,LPOBJ lpNpc);

	// Client -> server (C1:D3:67): Join / Leave buttons of the NPC window.
	void CGActionRecv(PMSG_DR_ACTION_RECV* lpMsg,int aIndex);

	// Warp rules: called from gObjMoveGate (fromGate = the player walked through a gate) and gObjTeleport.
	// 0 = allowed, 1 = refused (player told).
	int WarpFilter(LPOBJ lpObj,int map,int x,int y);
	void BeginGateStep(); // CMove::CGTeleportRecv wraps its gObjMoveGate call with these two
	void EndGateStep();

	// CItemManager::CheckItemMoveToInventory: true when the racer may not wear this item.
	bool IsItemBlocked(LPOBJ lpObj,CItem* lpItem);

	// Death respawn of a racer: back to the start point.
	bool GetUserRespawnLocation(LPOBJ lpObj,int* gate,int* map,int* x,int* y,int* dir,int* level);

	void OnEnterMap(LPOBJ lpObj);   // after a map change: finish box arrival time
	void OnUserLogout(LPOBJ lpObj); // gObjDel: give the items back before the character is saved

	// A monster hit a player: ice slow / lightning move for racers.
	void OnMonsterHit(LPOBJ lpMonster,LPOBJ lpTarget);

	bool Command(LPOBJ lpObj,char* arg); // GM: /drace open|start|end|cancel|status
	void MenuCommand(const char* arg);   // the GameServer window, Events menu (open / start / cancel)
	int GetDisplayTime();                // seconds to the next race for the Events list (-1 off, 0 running)

	int GetState();

private:
	// helpers
	void Announce(const char* format,...);
	const char* NpcPlace();
	void Notify(LPOBJ lpObj,const char* format,...);
	bool Served();
	bool IsActiveRacer(LPOBJ lpObj);
	DR_PLAYER* Find(LPOBJ lpObj);
	int BlockedCategory(CItem* lpItem);
	void SpawnNpc();
	void SetNextStart();

	// state machine (MainProc thread)
	void OpenRegistration();
	void CloseRegistration();
	void StartCountdown();
	void StartRace();
	void EndRace(const char* reason);
	void CancelRace(const char* reason);
	void Cleanup();
	void WaitingTick();
	void RaceTick();
	void ExitTick();
	void SendOpen(LPOBJ lpObj);
	void SendHudTick();
	void DoFinish(int aIndex,DR_PLAYER* p);

	// players
	bool Join(LPOBJ lpObj);
	bool UnequipBlocked(LPOBJ lpObj,DR_PLAYER* p);
	void RestoreItems(LPOBJ lpObj,DR_PLAYER* p);
	void Disqualify(LPOBJ lpObj,const char* reason,bool eject);
	void SendToStart(LPOBJ lpObj);
	void SendToExit(LPOBJ lpObj);
	void GiveReward(LPOBJ lpObj,int rank);
	void ReleasePlayer(int aIndex,bool eject);

	// monsters
	void SwapMonsters(bool race);
	void KickBystanders();

	void Lightning(LPOBJ lpTarget,LPOBJ lpMonster);
	void ParseIntList(const char* text,std::set<int>* out);
	void ParseMonsterMap(const char* text);

private:
	CRITICAL_SECTION m_cs;

	// config
	int m_Switch;
	int m_ServerFilter;
	int m_Schedule;
	int m_DayOfWeek;
	int m_Hour;
	int m_Minute;
	int m_RegisterMin;
	int m_MinPlayers;
	int m_MaxPlayers;
	int m_CountdownSec;
	int m_MaxMin;
	int m_Winners;
	int m_FinishExitDelaySec;
	int m_MinLevel;
	int m_StartMap;
	int m_StartX;
	int m_StartY;
	int m_StartRadius;
	int m_DungeonMap;
	int m_FinishX1;
	int m_FinishY1;
	int m_FinishX2;
	int m_FinishY2;
	int m_NpcClass;
	int m_NpcMap;
	int m_NpcX;
	int m_NpcY;
	int m_NpcDir;
	int m_BlockWings;
	int m_BlockWeapons;
	int m_BlockRings;
	int m_BlockPendants;
	int m_BlockHornUniria;
	int m_BlockHornDinorant;
	int m_BlockFenrir;
	int m_IceChance;
	int m_IceSeconds;
	int m_LightningChance;
	int m_LightningMode; // 0 push, 1 pull, 2 random
	int m_LightningTiles;
	int m_MaxJump;
	int m_WarpDisqualify;
	int m_RespawnAtStart;
	int m_LockDungeon;
	int m_GMBypass;
	int m_ConfirmSec;
	std::set<int> m_IceClasses;
	std::set<int> m_LightningClasses;
	std::map<int,int> m_MonsterMap;
	int m_Bag[3];    // EventItemBagManager.txt SpecialValue of the 1st / 2nd / 3rd place item bag
	DWORD m_Zen[3];  // extra zen per place
	DWORD m_WCoinC[3];     // WCoin (C) per place
	DWORD m_WCoinP[3];     // WCoin (P) per place
	DWORD m_GoblinPoint[3]; // Goblin Points per place

	// state
	volatile int m_State;
	time_t m_NextStart;
	time_t m_StateEnd;
	time_t m_RaceStart;
	DWORD m_RaceStartTick;
	char m_WinnerName[3][11];
	DWORD m_SecTick;
	int m_NpcIndex;
	int m_LastAnnounce;
	int m_FinishCount;
	bool m_EndRequest;
	std::map<int,DR_PLAYER> m_Players;  // key = gObj index
	std::map<int,DWORD> m_Confirm;      // NPC confirm click
	std::vector<int> m_Pending;         // disqualified players waiting for restore + eject (MainProc)
};

extern CDungeonRace gDungeonRace;
