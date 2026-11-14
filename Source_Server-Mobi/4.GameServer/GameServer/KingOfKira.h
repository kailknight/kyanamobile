// KingOfKira.h - "King of Kira" guild war in Devias (1.0.50).
// Design: Working Project\MD Files\KingOfKira-DESIGN.md.
//
// One host GameServer (KingOfKiraHostServer, default 19 = GameServerCS) owns the event: schedule, registration list,
// scoring. Every GameServer (host included) keeps a mirror of the host's state, which the host broadcasts through the
// DataServer (SDHP_KOK_RELAY, see KingOfKiraProtocol.h), and uses it for the registration NPC, the warp rules, the
// winner flags and the Chaos Goblin bonus. Keys are read from CustomConfig.ini ([CustomConfig]) in Load().
#pragma once

#include "Protocol.h"
#include "User.h"
#include "KingOfKiraProtocol.h"

#pragma pack(push,1)

// Server -> client (C1:D3:xx). Strings are NUL terminated, guild names are at most 8 characters.
// C2:D3:60 - the registration window, built by hand because it grows with the list:
//   [C2][size hi][size lo][D3][60] State, CanRegister, IsMaster, MyGuildRegistered (0 no, 1 own guild, 2 through the alliance),
//   WORD RemainSec (little endian), BYTE Count, then Count * { char Guild[9]; BYTE AllyCount; char Allies[5][9]; } (55 bytes each).
#define KOK_OPEN_ENTRY_SIZE (9+1+KOK_MAX_ALLY*9)

struct PMSG_KOK_REG_RECV // C1:D3:61 - client -> server
{
	PSBMSG_HEAD header;
	BYTE Action;           // 0 = register, 1 = withdraw
};

struct PMSG_KOK_RESULT_SEND // C1:D3:62 - answer to a registration (eKokRegResult)
{
	PSBMSG_HEAD header;
	BYTE Result;
};

struct PMSG_KOK_HUD_SEND // C1:D3:63 - the on-screen counter, once a second while on the event map
{
	PSBMSG_HEAD header;
	BYTE State;
	BYTE Contested;
	WORD RemainSec;
	WORD Points1;
	WORD Points2;
	char Guild1[9];
	char Guild2[9];
	char OnHill[9];
};

struct PMSG_KOK_FLAG_SEND // C1:D3:64 - the current King, for the flag marks in Devias
{
	PSBMSG_HEAD header;
	BYTE Has;
	char Guild[9];
	BYTE Mark[32];
};

#pragma pack(pop)

static_assert(sizeof(PMSG_KOK_REG_RECV) == 5,"KOK reg size");
static_assert(sizeof(PMSG_KOK_RESULT_SEND) == 5,"KOK result size");
static_assert(sizeof(PMSG_KOK_HUD_SEND) == 4+2+2+2+2+9+9+9,"KOK hud size");
static_assert(sizeof(PMSG_KOK_FLAG_SEND) == 4+1+9+32,"KOK flag size");

class CKingOfKira
{
public:
	CKingOfKira();

	void Load(char* section,char* path); // CustomConfig.ini; also reschedules

	void MainProc(); // every GameServer, called from the 1 second user loop (User.cpp)

	// NPC click (NpcTalk.cpp): true when the NPC is the registration NPC.
	bool Dialog(LPOBJ lpObj,LPOBJ lpNpc);

	// Client -> server
	void CGRegisterRecv(PMSG_KOK_REG_RECV* lpMsg,int aIndex);

	// DataServer -> GameServer (D9:60 relay, D9:63 winner)
	void DGRelayRecv(BYTE* lpMsg,int size);
	void DGWinnerRecv(SDHP_KOK_WINNER* lpMsg);

	// Warp rules: called from gObjMoveGate and gObjTeleport before the map changes.
	// 0 = allowed (map/x/y may have been changed), 1 = refused (player told), 2 = the player is being sent to the host server.
	int WarpFilter(LPOBJ lpObj,int* map,int* x,int* y);

	bool IsParticipant(LPOBJ lpObj);
	bool IsKing(LPOBJ lpObj); // winner guild or its alliance
	int GetChaosBonus(LPOBJ lpObj); // percent added to a mix in the Devias Chaos Goblin, 0 when none

	void OnUserDie(LPOBJ lpObj,LPOBJ lpTarget);
	bool GetUserRespawnLocation(LPOBJ lpObj,int* gate,int* map,int* x,int* y,int* dir,int* level);
	void OnEnterMap(LPOBJ lpObj); // after a teleport/login: send the flag mark and the counter

	bool Command(LPOBJ lpObj,char* arg); // GM: /kok open|start|cancel|end|status
	void MenuCommand(const char* arg);   // the GameServer window, Events menu
	int GetDisplayTime();                // seconds to the start for the Events list (-1 off, 0 running)

	bool IsHost();
	int GetState();

private:
	int PlayerKey(LPOBJ lpObj);
	bool IsMasterOfRegistered(LPOBJ lpObj,int* slot);
	void UpdateTop(); // m_S.Guild1/Points1 = leader, Guild2/Points2 = runner-up (what the HUD shows)
	int FindSlot(const char* guild);
	void SendState(bool force);
	void SendRelay(WORD target,BYTE kind,void* data,int len);
	void SendHud(LPOBJ lpObj);
	void SendHudToMap();
	void SendFlag(LPOBJ lpObj);
	void SendFlagToAll();
	void SendRegResult(int aIndex,BYTE result);
	void SendList();
	void BuildEntry(LPOBJ lpObj,KOK_ENTRY_DATA* entry);
	void Countdown(int remain);
	void EnforceDevias();
	bool GetStartPoint(int* x,int* y); // a random tile inside the KingOfKiraStartGate area (false = no gate set)
	void SendParticipantsToStart();
	void SpawnGoblin(); // war start: every participant on this server goes to KingOfKiraStartGate (Devias 3)
	void HostLock();

	// host only
	void HostTick();
	void HostSetNextStart();
	void HostOpenRegistration();
	void HostStartFight();
	void HostEndFight(bool aborted);
	void HostFinishExit();
	void HostCancel(const char* reason);
	void HostClear();
	void HostRegister(KOK_REG_DATA* p,WORD fromServer);
	void HostScore();
	void HostClearMonsters();
	void HostRespawnMonsters();
	void HostSaveWinner(int winSlot);
	void Announce(const char* format,...);

	void MirrorTick();
	void SpawnNpc();
	bool ServesDevias();

private:
	// config
	int m_Switch;
	int m_HostServer;
	int m_Schedule;      // 0 off, 1 daily, 2 weekly
	int m_DayOfWeek;
	int m_Hour;
	int m_Minute;
	int m_RegisterOpenMin;
	int m_WarnMinutes;
	int m_DurationMin;
	int m_DeathPenalty;
	int m_HoldAnnounceSec; // KingOfKiraHoldAnnounceSec: golden "<guild> is holding the throne for mm:ss" every N seconds (0 = off)
	int m_HoldSlot;        // 0/1 = the registered guild whose master holds the throne right now, -1 = nobody / contested
	time_t m_HoldSince;    // when that hold started
	time_t m_LastHoldAnnounce;
	int m_ChaosBonus;
	int m_BonusAlliance; // KingOfKiraBonusAlliance: 1 = the winner guild and its alliance get the goblin bonus, 0 = the winner guild only
	int m_GoblinClass; // KingOfKiraGoblin*: the King's own Chaos Goblin in Devias - the bonus only works there
	int m_GoblinX;
	int m_GoblinY;
	int m_GoblinDir;
	int m_GoblinIndex;
	int m_NpcClass;
	int m_NpcMap;
	int m_NpcX;
	int m_NpcY;
	int m_NpcDir;
	int m_HillMap;
	int m_HillX1;
	int m_HillX2;
	int m_HillY;
	int m_ArriveX;
	int m_ArriveY;
	int m_StartGate; // KingOfKiraStartGate: gate the participants are sent to when the war starts (73 = Devias 3, 0 = off)
	int m_ExitDelaySec;
	int m_GMBypass;
	int m_RegisterCloseMin;

	// state, mirrored on every server
	KOK_STATE_DATA m_S;
	DWORD m_StateTick;   // when the mirror was last refreshed (non-host servers expire a silent host)
	DWORD m_SecTick;
	int m_NpcIndex;

	// the King
	bool m_WinLoaded;
	DWORD m_WinAskTick;
	int m_WinAsks;
	BYTE m_WinHas;
	char m_WinGuild[9];
	BYTE m_WinMark[32];
	int m_WinKey;

	// the registered guilds: owned by the host, mirrored on the other servers (KOK_RELAY_LIST)
	KOK_ENTRY_DATA m_List[KOK_MAX_ENTRIES];
	int m_ListCount;
	DWORD m_ListTick;
	DWORD m_EnforceTick[MAX_OBJECT];

	// host only
	time_t m_NextStart;
	time_t m_StateEnd;
	time_t m_OpenAt;
	time_t m_CloseAt;
	DWORD m_LastRegAnnounce; // GetTickCount() of the last "registration is open" golden line
	int m_LastWarnMinute;
	int m_Points[KOK_MAX_ENTRIES]; // throne points of every fighting guild (index = position in m_List)
	int m_DisplayRemain;
	DWORD m_LastIdleSend;
};

extern CKingOfKira gKingOfKira;
