// DuelRanked.h: Ranked 1v1 matchmaking for the Duel Arena, DataServer side.
//
// See Working Project\MD Files\RankedDuel-PLAN.md and DuelRanked_Schema.sql.
// Settings: DuelRanked.ini. Wire structs: DuelRankedProtocol.h.
//
// Threading: every handler and the 1 s tick run on the queue thread, so the
// tables need no lock and SQL (gQueryManager is one unlocked statement) is only
// called from there. OnServerDown is the one entry from another thread (the
// IOCP thread inside CServerManager::DelServer): it only records the server
// code under a leaf lock and the queue thread acts on it. PostTick/PostReload
// run on the UI thread and only post a packet / raise a flag for the queue
// thread.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "CriticalSection.h"
#include "DataServerProtocol.h"
#include "CharacterManager.h"
#include "DuelRankedProtocol.h"

#define DUELRANKED_MAX_TIERS 16
#define DUELRANKED_MAX_REWARD_ITEMS 5	// = FRIENDMAIL_MAX_ITEMS, one letter per division
#define DUELRANKED_MAX_STEPS 64

// Internal only: the DataServer posts this to itself (0xD9 / 0x5F, every second
// from the UI timer) so the work runs on the queue thread. Magic keeps a stray
// packet from a GameServer out.
#pragma pack(push,1)
struct SDHP_DUELRANKED_INTERNAL
{
	PSBMSG_HEAD header;
	DWORD Magic;
};
#pragma pack(pop)

static_assert(sizeof(SDHP_DUELRANKED_INTERNAL) == 4 + 4,"SDHP_DUELRANKED_INTERNAL is internal to the DataServer");

#define DUELRANKED_INTERNAL_MAGIC 0x44524B31 // 'DRK1'

struct DR_CONFIG
{
	// [Rating]
	int StartMMR;
	int KFactor;
	int MMRFloor;
	// [Ranks] + [Steps]
	int TierCount;
	char TierName[DUELRANKED_MAX_TIERS][24];
	int TierDivisions[DUELRANKED_MAX_TIERS];
	int StepCount;
	int Step[DUELRANKED_MAX_STEPS];
	int StepTier[DUELRANKED_MAX_STEPS];
	int StepDiv[DUELRANKED_MAX_STEPS];
	// [Rewards] TierN=WCoinC,WCoinP,GoblinPoint - given once per division the
	// first time a character reaches it (DuelRankedBest keeps the best).
	int RewardWCoinC[DUELRANKED_MAX_TIERS];
	int RewardWCoinP[DUELRANKED_MAX_TIERS];
	int RewardGoblin[DUELRANKED_MAX_TIERS];
	// [RewardItems] TierN_ItemK=Section,Index,Level,Durability,Skill,Luck,Option,Excellent
	// (K = 1..5) - mailed with the same rule, one letter per division reached.
	int RewardItemCount[DUELRANKED_MAX_TIERS];
	BYTE RewardItem[DUELRANKED_MAX_TIERS][DUELRANKED_MAX_REWARD_ITEMS][8];
	// [Match]
	int Enabled;
	int HostServer;
	int MaxLoadPercent;
	int MaxConcurrentMatches;
	int MaxSpectators;
	int BaseWindow;
	int WidenStep;
	int WidenSecs;
	int MaxWindow;
	int AnyOpponentAfterSec;
	int RematchCooldownSec;
	// Accounts that share an IP or a hardware id (HardwareLog) may be paired at most
	// SameDeviceLimit times a day (rated matches started today); 0 = no limit.
	int SameDeviceLimit;
	int SameDeviceDays;			// how recent a HardwareLog row must be to count as "the same device"
	int DeclineCooldownSec;
	int NoShowCooldownSec;
	int UnavailableDropSec;
	// [Timing]
	int PromptSec;
	int PromptGraceSec;
	int StageTimeoutSec;
	int MaxMatchSec;
	int ReconnectGraceSec;
	int WatchdogExtraSec;
	int HostSilentSec;
};

enum eDuelRankedPhase
{
	DR_PHASE_PROMPT = 1,
	DR_PHASE_STAGING = 2,
	DR_PHASE_FIGHTING = 3,
};

// One fighter of a match.
struct DR_SLOT
{
	char Name[11];
	char Account[11];
	int Mmr;
	BYTE Accepted;
	BYTE Departed;		// the origin server reported DEPART
	BYTE Arrived;		// the host reported ARRIVED
	BYTE Absent;		// the host reported PAUSED
	WORD OriginServer;
	BYTE OriginMap;
	BYTE OriginX;
	BYTE OriginY;
};

// A prompt (Id = token with the high bit set) or a real match (Id = MatchId).
struct DR_MATCH
{
	DWORD Id;
	BYTE Phase;			// eDuelRankedPhase
	BYTE Room;			// 0xFF until the host reports STARTED
	BYTE ExpectSent;	// bit 0/1: EXPECT sent to the host for that fighter
	WORD HostServer;
	DWORD Deadline;		// prompt / staging / watchdog deadline, by Phase
	DWORD StartTick;
	DR_SLOT Slot[2];
};

// Everyone who is not idle: queued, in a prompt, in a match, or watching.
struct DR_PLAYER
{
	char Name[11];
	char Account[11];
	BYTE State;			// eDuelRankedState
	BYTE Slot;			// fighter 0/1
	BYTE Departed;		// spectator: origin server reported it
	BYTE Arrived;		// spectator: host reported it
	WORD Level;
	WORD ServerCode;	// where it is registered
	int Mmr;
	DWORD MatchId;		// prompt token, match id, or the match being watched
	DWORD QueuedTick;	// when it joined the queue (kept across a cancelled prompt)
	DWORD StateTick;
	DWORD KeepUntil;	// 0 = none. A requeued fighter on its way home from the host
	DWORD LastGoTick;
	WORD OriginServer;	// spectator: where it was
	BYTE OriginMap;
	BYTE OriginX;
	BYTE OriginY;
};

struct DR_STATS
{
	int Mmr;
	int Wins;
	int Losses;
	int Draws;
};

struct DR_RESULTROW
{
	int Applied;		// 1 rated now, 0 settled before, -1 no such match
	int Status;			// match row: 1 settled, 2 draw, 3 void, 4 abandoned, -1 unknown
	int MmrABefore;
	int MmrBBefore;
	int NewA;
	int NewB;
	int DeltaA;
	int DeltaB;
	char NameA[11];
	char NameB[11];
	char WinnerName[11];
};

class CDuelRanked
{
public:
	CDuelRanked();

	// DataServerProtocolCore: the tick is the DataServer talking to itself, it
	// must not count as a GameServer packet (m_PacketTime) or reach the packet log.
	static bool IsInternal(BYTE head,BYTE* lpMsg,int size);

	// DuelRanked.ini. initial = at start (a missing or invalid file leaves ranked
	// off); on a reload an invalid file keeps what is running.
	void LoadConfig(bool initial);

	// UI thread (WM_TIMER / menu): post to the queue thread.
	void PostTick();
	void PostReload();

	// Queue thread: 0xD9 / 0x51-0x55 and the internal 0x5F.
	void OnPacket(BYTE* lpMsg,int size,int index);

	// Queue thread: C1:70 / C1:71 (character registered / left a server).
	void OnCharacterRegistered(const char* name,int index);
	void OnCharacterLeft(const char* name,int index);

	// IOCP thread (CServerManager::DelServer): memory only.
	void OnServerDown(WORD serverCode);

private:
	// config / rank
	void DefaultConfig(DR_CONFIG* cfg);
	int ReadConfig(DR_CONFIG* cfg,char* error,int errorSize);	// 0 ok, 1 file missing, 2 invalid
	void RankOf(int mmr,BYTE* tier,BYTE* div,WORD* stepLo,WORD* stepNext);
	bool HostAvailable(bool checkLoad);
	bool FromHost(int index);

	// lookups
	DR_PLAYER* FindPlayer(const char* name);
	void ErasePlayer(const char* name);
	DR_MATCH* FindMatch(DWORD id);		// a real match (staging / fighting)
	DR_MATCH* FindPrompt(DWORD token);
	bool Verify(int index,const char* account,const char* name,CHARACTER_INFO* info);
	bool Resolve(const char* name,CHARACTER_INFO* info,int* serverIndex);
	int CooldownLeft(const char* name);
	void SetCooldown(const char* name,int seconds);
	bool RematchBlocked(const char* a,const char* b);
	bool SameDeviceBlocked(const char* nameA,const char* accountA,const char* nameB,const char* accountB);
	void SetRematch(const char* a,const char* b);
	int CountSpectators(DWORD matchId);
	DWORD NextToken();

	// senders
	bool SendToCode(WORD serverCode,BYTE* lpMsg,int size);
	void SendMsg(int serverIndex,WORD aIndex,const char* account,DWORD matchId,BYTE code,BYTE p1);
	void SendMsgToName(const char* name,DWORD matchId,BYTE code,BYTE p1);
	void SendStatus(int serverIndex,WORD aIndex,const char* account,const char* name,const DR_STATS* known);
	void SendPrompt(int serverIndex,const CHARACTER_INFO* to,DWORD token,const DR_SLOT* opp);
	bool SendGo(const char* name,const char* account,DWORD matchId,BYTE kind,BYTE room,int serverIndex);
	bool SendExpect(const DR_MATCH* m,int slot);
	bool SendExpectSpectator(const DR_MATCH* m,const DR_PLAYER* p);
	void SendMatchCancel(WORD hostServer,DWORD matchId,BYTE reason);
	void SendStrayFighter(const SDHP_DUELRANKED_DEPART* d);
	void SendApplied(int hostIndex,DWORD matchId,const DR_RESULTROW* row);
	void SendRank(int serverIndex,const char* name,const char* account);
	void SendRankToName(const char* name);

	// database
	bool DbGet(const char* name,const char* account,DR_STATS* out);
	int DbPeek(const char* name,DR_STATS* out);	// read only, no row is created: 1 found, 0 none, -1 error
	int StepOf(int mmr);
	void GrantRankRewards(const char* name,int oldMmr,int newMmr);	// queue thread, after a rated result
	bool DbCreate(const DR_MATCH* m,int* matchId,int* mmrA,int* mmrB);
	bool DbResult(DWORD matchId,int winnerSlot,int reason,int scoreA,int scoreB,DR_RESULTROW* row);
	void DbVoid(DWORD matchId,int status);
	int DbMatchGet(DWORD matchId,char* nameA,char* nameB);	// 1 found, 0 none, -1 error
	void DbSweep();

	// packet handlers
	void OnReq(SDHP_DUELRANKED_REQ* lpMsg,int index);
	void OnJoin(int index,WORD aIndex,const char* account,const char* name,WORD level);
	void OnLeave(int index,WORD aIndex,const char* account,const char* name);
	void OnAnswer(const char* name,DWORD token,bool accept);
	void OnList(int index,WORD aIndex,const char* account);
	void OnWatch(int index,WORD aIndex,const char* account,const char* name,DWORD matchId,BYTE room);
	void OnCantGo(const char* name,DWORD matchId);
	void OnHostReport(SDHP_DUELRANKED_HOSTREPORT* lpMsg);
	void OnDepart(SDHP_DUELRANKED_DEPART* lpMsg);
	void OnHostEvent(SDHP_DUELRANKED_HOSTEVENT* lpMsg);
	void OnResult(SDHP_DUELRANKED_RESULT* lpMsg,int index);

	// state machine
	void TryPair();
	int Window(const DR_PLAYER* p,DWORD now);
	void StartPrompt(DR_PLAYER* a,DR_PLAYER* b,const CHARACTER_INFO* infoA,const CHARACTER_INFO* infoB,int indexA,int indexB);
	void CancelPrompt(DWORD token,int faultMask,int leftMask);
	void BeginStaging(DWORD token);
	void VoidMatch(DWORD id,int dbStatus,int cancelReason,int faultMask,int cooldownSec,bool requeueRest);
	void EraseSpectators(DWORD matchId);
	void DropQueue(BYTE code);
	void Housekeeping(DWORD now);
	void Tick();
	void DrainDown();
	void HandleServerDown(WORD serverCode);

private:
	DR_CONFIG m_Cfg;
	volatile bool m_CfgValid;	// m_Cfg holds a validated file (read by the UI thread)
	bool m_Enabled;				// valid and [Match] Enabled=1

	std::map<std::string,DR_PLAYER> m_Players;		// lower case name
	std::map<DWORD,DR_MATCH> m_Matches;				// prompt tokens and real matches
	std::map<std::string,DWORD> m_Cooldown;			// name -> until
	std::map<std::string,DWORD> m_Rematch;			// "a|b" -> until
	std::map<std::string,std::pair<DWORD,bool> > m_DeviceCache;	// "a|b" -> (until, blocked): SameDeviceBlocked answers for 30 s
	DWORD m_NextToken;
	DWORD m_PairHold;			// 0 = none. No pairing until then (the database failed)

	// last HOSTREPORT
	SDHP_DUELRANKED_HOSTREPORT m_Host;
	bool m_HostReported;
	DWORD m_HostReportTick;
	DWORD m_UnavailableSince;	// 0 = the host is available

	DWORD m_TickCount;

	// UI thread -> queue thread
	volatile LONG m_TickPending;
	volatile LONG m_ReloadPending;
	volatile LONG m_Active;		// the tables are not empty: the tick must keep running
	DWORD m_TickPostTick;

	// IOCP thread -> queue thread
	CCriticalSection m_DownLock;
	std::vector<WORD> m_Down;
};

extern CDuelRanked gDuelRanked;
