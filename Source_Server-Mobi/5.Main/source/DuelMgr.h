// DuelMgr.h: interface for the CDuelMgr class.
//////////////////////////////////////////////////////////////////////
#pragma once


typedef struct _DUEL_PLAYER_INFO
{
	int m_iIndex;
	char m_szID[24];
	int m_iScore;
	float m_fHPRate;
	float m_fSDRate;
} DUEL_PLAYER_INFO;

enum _DUEL_PLAYER_TYPE
{
	DUEL_HERO,
	DUEL_ENEMY,
	MAX_DUEL_PLAYERS
};

typedef struct _DUEL_CHANNEL_INFO
{
	BOOL m_bEnable;
	BOOL m_bJoinable;
	char m_szID1[24];
	char m_szID2[24];
} DUEL_CHANNEL_INFO;

#define MAX_DUEL_CHANNELS 4

// ---- ranked duels (cross-server matchmaking), see CB_DuelRanked.h ----
#define MAX_RANKED_FIGHTS 4

typedef struct _RANKED_STATUS_INFO
{
	BOOL  m_bValid;			// a STATUS arrived since the world was entered
	DWORD m_dwTick;			// GetTickCount() when the last STATUS arrived
	DWORD m_dwStateTick;	// GetTickCount() when m_byState last changed (STATUS or a MSG), the base of the search clock
	WORD  m_wMmr;
	BYTE  m_byTier;			// 0..6 Iron .. Duel Master
	BYTE  m_byDiv;			// 3 = III, 2 = II, 1 = I, 0 = none
	WORD  m_wStepLo;		// lower bound of the current rank step
	WORD  m_wStepNext;		// lower bound of the next step, 0 at the top
	WORD  m_wWins;
	WORD  m_wLosses;
	BYTE  m_byState;		// 0 none, 1 queued, 2 prompt, 3 staging, 4 fighting, 5 watching
	WORD  m_wQueuedSec;
	BYTE  m_byAccepting;	// 0 = the arena host is not taking matches
} RANKED_STATUS_INFO;

typedef struct _RANKED_PROMPT_INFO
{
	BOOL  m_bActive;
	BOOL  m_bAnswered;		// accepted, waiting for the opponent
	DWORD m_dwMatchId;
	char  m_szOpp[11];
	WORD  m_wOppMmr;
	BYTE  m_byOppTier;
	BYTE  m_byOppDiv;
	DWORD m_dwEndTick;		// when the countdown reaches zero
	DWORD m_dwLength;		// the countdown's length in ms, for the bar
} RANKED_PROMPT_INFO;

typedef struct _RANKED_FIGHT_INFO
{
	DWORD m_dwMatchId;
	BYTE  m_byRoom;
	char  m_szName1[11];
	char  m_szName2[11];
	WORD  m_wMmr1;
	WORD  m_wMmr2;
	BYTE  m_byTier1;
	BYTE  m_byDiv1;
	BYTE  m_byTier2;
	BYTE  m_byDiv2;
	BYTE  m_byScore1;
	BYTE  m_byScore2;
	WORD  m_wRemainSec;
} RANKED_FIGHT_INFO;

typedef struct _RANKED_RESULT_INFO
{
	BOOL  m_bActive;
	BYTE  m_byOutcome;		// 0 win, 1 loss, 2 draw, 3 void
	WORD  m_wOldMmr;
	WORD  m_wNewMmr;
	short m_sDelta;
	BYTE  m_byOldTier;
	BYTE  m_byOldDiv;
	BYTE  m_byNewTier;
	BYTE  m_byNewDiv;
	DWORD m_dwTick;
} RANKED_RESULT_INFO;

class CDuelMgr
{
public:
	CDuelMgr();
	virtual ~CDuelMgr();
	// A server hop / logout (InitGame) and construction wipe everything, ranked state included.
	// EnableDuel(FALSE) - a normal duel ending - only wipes the duel part, so a ranked fight that
	// ends does not throw away a queue or a result that arrived around the same time.
	void Reset();
	void ResetDuel();
public:
	void EnableDuel(BOOL bEnable);
	BOOL IsDuelEnabled();

	void EnablePetDuel(BOOL bEnable);
	BOOL IsPetDuelEnabled();

	void SetDuelPlayer(int iPlayerNum, int iIndex, char * pszID);
	void SetHeroAsDuelPlayer(int iPlayerNum);
	void SetScore(int iPlayerNum, int iScore);
	void SetHP(int iPlayerNum, int iRate);
	void SetSD(int iPlayerNum, int iRate);

	char * GetDuelPlayerID(int iPlayerNum);
	int GetDuelPlayerIndex(int iPlayerNum);
	int GetScore(int iPlayerNum);
	float GetHP(int iPlayerNum);
	float GetSD(int iPlayerNum);

	BOOL IsDuelPlayer(CHARACTER * pCharacter, int iPlayerNum, BOOL bIncludeSummon = TRUE);
	BOOL IsDuelPlayer(WORD wIndex, int iPlayerNum);

	void SendDuelRequestAnswer(int iPlayerNum, BOOL bOK);

protected:
	BOOL m_bIsDuelEnabled;
	BOOL m_bIsPetDuelEnabled;
	DUEL_PLAYER_INFO m_DuelPlayer[MAX_DUEL_PLAYERS];

public:
	void SetDuelChannel(int iChannelIndex, BOOL bEnable, BOOL bJoinable, char * pszID1, char * pszID2);
	BOOL IsDuelChannelEnabled(int iChannelIndex) { return m_DuelChannels[iChannelIndex].m_bEnable; }
	BOOL IsDuelChannelJoinable(int iChannelIndex) { return m_DuelChannels[iChannelIndex].m_bJoinable; }
	char * GetDuelChannelUserID1(int iChannelIndex) { return m_DuelChannels[iChannelIndex].m_szID1; }
	char * GetDuelChannelUserID2(int iChannelIndex) { return m_DuelChannels[iChannelIndex].m_szID2; }

	void SetCurrentChannel(int iChannel = -1) { m_iCurrentChannel = iChannel; }
	int GetCurrentChannel() { return m_iCurrentChannel; }

	void RemoveAllDuelWatchUser();
	void AddDuelWatchUser(char * pszUserID);
	void RemoveDuelWatchUser(char * pszUserID);
	char * GetDuelWatchUser(int iIndex);
	int GetDuelWatchUserCount() { return m_DuelWatchUserList.size(); }

	BOOL GetFighterRegenerated() { return m_bRegenerated; }
	void SetFighterRegenerated(BOOL bFlag) { m_bRegenerated = bFlag; }

	// ---- ranked duels ----
	void ResetRanked();
	void SetRankedStatus(const RANKED_STATUS_INFO& info);
	// A MSG already says the state changed (queued, left, declined...): follow it at once; the
	// STATUS requested right after confirms it.
	void SetRankedLocalState(BYTE byState);
	const RANKED_STATUS_INFO& GetRankedStatus() const { return m_RankedStatus; }
	BOOL IsRankedQueued() const { return m_RankedStatus.m_byState == 1; }
	// Seconds spent searching, counted on from when the queued state began.
	int  GetRankedQueuedSec() const;

	void SetRankedPrompt(DWORD dwMatchId, const char* pszOpp, WORD wMmr, BYTE byTier, BYTE byDiv, BYTE bySecLeft);
	void ClearRankedPrompt();
	void SetRankedPromptAnswered() { m_RankedPrompt.m_bAnswered = TRUE; }
	RANKED_PROMPT_INFO& GetRankedPrompt() { return m_RankedPrompt; }

	void SetRankedFights(int iCount, const RANKED_FIGHT_INFO* pFights);
	int  GetRankedFightCount() const { return m_iRankedFightCount; }
	const RANKED_FIGHT_INFO* GetRankedFight(int iIndex) const { return (iIndex >= 0 && iIndex < m_iRankedFightCount) ? &m_RankedFights[iIndex] : NULL; }
	const RANKED_FIGHT_INFO* FindRankedFightByRoom(int iRoom) const;
	BOOL IsRankedListValid() const { return m_bRankedListValid; }
	void InvalidateRankedList() { m_bRankedListValid = FALSE; m_iRankedFightCount = 0; }

	void SetRankedResult(const RANKED_RESULT_INFO& info);
	RANKED_RESULT_INFO& GetRankedResult() { return m_RankedResult; }
	void ClearRankedResult() { m_RankedResult.m_bActive = FALSE; }

	// The "opponent disconnected, waiting NN s" pause: MSG 15 starts it, 16/17/18 end it.
	// The server repeats MSG 15 every few seconds, so a pause nobody ended or renewed for 10 s past
	// its countdown is dropped here instead of staying on screen.
	void StartRankedPause(int iSecondsLeft);
	void EndRankedPause() { m_dwRankedPauseEnd = 0; }
	BOOL IsRankedPaused() const;
	int  GetRankedPauseSecLeft() const;

	// Wanted: a STATUS request at/after dwDelayMs from now. Lives here (not in the overlay) so a
	// server hop, which wipes this class, also drops a request meant for the old server.
	void ScheduleRankedStatus(DWORD dwDelayMs);
	BOOL TakeRankedStatusDue();
	DWORD GetRankedListAgeMs() const { return m_bRankedListValid ? (GetTickCount() - m_dwRankedListTick) : 0; }

	void SetRankedOpenTick(DWORD dwTick) { m_dwRankedOpenTick = dwTick; }
	DWORD GetRankedOpenTick() const { return m_dwRankedOpenTick; }
	// Set while the player is spectating a ranked fight (MSG 26), cleared when the watch ends.
	void SetRankedWatching(BOOL bFlag) { m_bRankedWatching = bFlag; }
	BOOL IsRankedWatching() const { return m_bRankedWatching; }

protected:
	RANKED_STATUS_INFO m_RankedStatus;
	RANKED_PROMPT_INFO m_RankedPrompt;
	RANKED_FIGHT_INFO  m_RankedFights[MAX_RANKED_FIGHTS];
	int   m_iRankedFightCount;
	BOOL  m_bRankedListValid;
	RANKED_RESULT_INFO m_RankedResult;
	DWORD m_dwRankedPauseEnd;
	DWORD m_dwRankedOpenTick;
	DWORD m_dwRankedListTick;
	BOOL  m_bRankedWatching;
	BOOL  m_bRankedStatusDue;
	DWORD m_dwRankedStatusDueTick;

protected:
	DUEL_CHANNEL_INFO m_DuelChannels[MAX_DUEL_CHANNELS];
	int m_iCurrentChannel;

	BOOL m_bRegenerated;

	std::list<std::string> m_DuelWatchUserList;
};

extern CDuelMgr g_DuelMgr;
