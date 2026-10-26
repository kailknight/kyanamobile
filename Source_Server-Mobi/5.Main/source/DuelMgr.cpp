// DuelMgr.cpp: implementation of the CDuelMgr class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "DuelMgr.h"
#include "ZzzBMD.h"
#include "ZzzEffect.h"
#include "ZzzObject.h"
#include "ZzzInventory.h"
#include "ZzzInterface.h"
#include "ZzzInfomation.h"
#include "ZzzCharacter.h"
#include "wsclientinline.h"
#include "CharacterManager.h"

CDuelMgr g_DuelMgr;

CDuelMgr::CDuelMgr()
{
	// ResetRanked() looks at the previous result to decide what to keep.
	memset(&m_RankedResult, 0, sizeof(m_RankedResult));
	Reset();
}

CDuelMgr::~CDuelMgr()
{
	Reset();
}

void CDuelMgr::Reset()
{
	ResetDuel();
	ResetRanked();
}

void CDuelMgr::ResetDuel()
{
	m_bIsDuelEnabled = FALSE;
	m_bIsPetDuelEnabled = FALSE;
	memset(m_DuelPlayer, 0, sizeof(DUEL_PLAYER_INFO) * MAX_DUEL_PLAYERS);
	memset(m_DuelChannels, 0, sizeof(DUEL_CHANNEL_INFO) * MAX_DUEL_CHANNELS);
	m_iCurrentChannel = -1;
	m_bRegenerated = FALSE;
	RemoveAllDuelWatchUser();
}

void CDuelMgr::EnableDuel(BOOL bEnable)
{
	m_bIsDuelEnabled = bEnable;

	if (bEnable == FALSE)
	{
		ResetDuel();
	}
}

//////////////////////////////////////////////////////////////////////
// Ranked duels (cross-server matchmaking). The packets are parsed in WSclient.cpp
// (ReceiveRankedXxx); CB_DuelRanked.cpp and the Titus window read this state.
//////////////////////////////////////////////////////////////////////

void CDuelMgr::ResetRanked()
{
	// The fighters are sent home with a server hop right after the result arrives, which wipes
	// this class; keep a banner that is still fresh so it can be read on the other side.
	RANKED_RESULT_INFO keptResult = m_RankedResult;
	const bool keepResult = (keptResult.m_bActive != FALSE) && ((int)(GetTickCount() - keptResult.m_dwTick) < 12000);

	memset(&m_RankedStatus, 0, sizeof(m_RankedStatus));
	memset(&m_RankedPrompt, 0, sizeof(m_RankedPrompt));
	memset(m_RankedFights, 0, sizeof(m_RankedFights));
	memset(&m_RankedResult, 0, sizeof(m_RankedResult));
	if (keepResult)
	{
		m_RankedResult = keptResult;
	}
	m_iRankedFightCount = 0;
	m_bRankedListValid = FALSE;
	m_dwRankedPauseEnd = 0;
	m_dwRankedOpenTick = 0;
	m_dwRankedListTick = 0;
	m_bRankedWatching = FALSE;
	m_bRankedStatusDue = FALSE;
	m_dwRankedStatusDueTick = 0;
}

void CDuelMgr::ScheduleRankedStatus(DWORD dwDelayMs)
{
	m_bRankedStatusDue = TRUE;
	m_dwRankedStatusDueTick = GetTickCount() + dwDelayMs;
}

BOOL CDuelMgr::TakeRankedStatusDue()
{
	if (m_bRankedStatusDue && (int)(GetTickCount() - m_dwRankedStatusDueTick) >= 0)
	{
		m_bRankedStatusDue = FALSE;
		return TRUE;
	}
	return FALSE;
}

void CDuelMgr::SetRankedStatus(const RANKED_STATUS_INFO& info)
{
	m_RankedStatus = info;
	m_RankedStatus.m_bValid = TRUE;
	m_RankedStatus.m_dwTick = GetTickCount();

	// The search clock starts when the server says it did, not when this packet arrived.
	m_RankedStatus.m_dwStateTick = m_RankedStatus.m_dwTick - (DWORD)m_RankedStatus.m_wQueuedSec * 1000;

	// The server is the authority: a prompt that is no longer pending there goes away here. A
	// STATUS that was already on its way when the prompt arrived must not kill a brand new one.
	if (m_RankedStatus.m_byState != 2 && m_RankedPrompt.m_bActive)
	{
		const DWORD arrived = m_RankedPrompt.m_dwEndTick - m_RankedPrompt.m_dwLength;
		if ((int)(GetTickCount() - arrived) > 2000)
		{
			ClearRankedPrompt();
		}
	}

	if (m_RankedStatus.m_byState != 5)
	{
		m_bRankedWatching = FALSE;
	}

	if (m_RankedStatus.m_byState < 3)
	{
		m_dwRankedPauseEnd = 0;
	}
}

void CDuelMgr::SetRankedLocalState(BYTE byState)
{
	if (m_RankedStatus.m_byState != byState)
	{
		m_RankedStatus.m_byState = byState;
		m_RankedStatus.m_wQueuedSec = 0;
		m_RankedStatus.m_dwStateTick = GetTickCount();
	}
}

int CDuelMgr::GetRankedQueuedSec() const
{
	if (m_RankedStatus.m_byState != 1)
	{
		return 0;
	}

	return (int)((GetTickCount() - m_RankedStatus.m_dwStateTick) / 1000);
}

void CDuelMgr::SetRankedPrompt(DWORD dwMatchId, const char* pszOpp, WORD wMmr, BYTE byTier, BYTE byDiv, BYTE bySecLeft)
{
	m_RankedPrompt.m_bActive = TRUE;
	m_RankedPrompt.m_bAnswered = FALSE;
	m_RankedPrompt.m_dwMatchId = dwMatchId;
	memset(m_RankedPrompt.m_szOpp, 0, sizeof(m_RankedPrompt.m_szOpp));
	strncpy(m_RankedPrompt.m_szOpp, pszOpp, MAX_ID_SIZE);
	m_RankedPrompt.m_wOppMmr = wMmr;
	m_RankedPrompt.m_byOppTier = byTier;
	m_RankedPrompt.m_byOppDiv = byDiv;
	m_RankedPrompt.m_dwLength = (DWORD)bySecLeft * 1000;
	m_RankedPrompt.m_dwEndTick = GetTickCount() + m_RankedPrompt.m_dwLength;
}

void CDuelMgr::ClearRankedPrompt()
{
	memset(&m_RankedPrompt, 0, sizeof(m_RankedPrompt));
}

void CDuelMgr::SetRankedFights(int iCount, const RANKED_FIGHT_INFO* pFights)
{
	if (iCount < 0)
	{
		iCount = 0;
	}
	if (iCount > MAX_RANKED_FIGHTS)
	{
		iCount = MAX_RANKED_FIGHTS;
	}

	memset(m_RankedFights, 0, sizeof(m_RankedFights));
	for (int i = 0; i < iCount; ++i)
	{
		m_RankedFights[i] = pFights[i];
	}
	m_iRankedFightCount = iCount;
	m_bRankedListValid = TRUE;
	m_dwRankedListTick = GetTickCount();
}

const RANKED_FIGHT_INFO* CDuelMgr::FindRankedFightByRoom(int iRoom) const
{
	for (int i = 0; i < m_iRankedFightCount; ++i)
	{
		if (m_RankedFights[i].m_byRoom == iRoom)
		{
			return &m_RankedFights[i];
		}
	}
	return NULL;
}

void CDuelMgr::SetRankedResult(const RANKED_RESULT_INFO& info)
{
	m_RankedResult = info;
	m_RankedResult.m_bActive = TRUE;
	m_RankedResult.m_dwTick = GetTickCount();
}

void CDuelMgr::StartRankedPause(int iSecondsLeft)
{
	if (iSecondsLeft < 1)
	{
		iSecondsLeft = 1;
	}

	m_dwRankedPauseEnd = GetTickCount() + (DWORD)iSecondsLeft * 1000;
	if (m_dwRankedPauseEnd == 0)
	{
		m_dwRankedPauseEnd = 1;
	}
}

BOOL CDuelMgr::IsRankedPaused() const
{
	if (m_dwRankedPauseEnd == 0)
	{
		return FALSE;
	}

	return ((int)(GetTickCount() - m_dwRankedPauseEnd) < 10000) ? TRUE : FALSE;
}

int CDuelMgr::GetRankedPauseSecLeft() const
{
	if (m_dwRankedPauseEnd == 0)
	{
		return 0;
	}

	int ms = (int)(m_dwRankedPauseEnd - GetTickCount());
	return (ms <= 0) ? 0 : (ms + 999) / 1000;
}

BOOL CDuelMgr::IsDuelEnabled()
{
	return m_bIsDuelEnabled;
}

void CDuelMgr::EnablePetDuel(BOOL bEnable)
{
	m_bIsPetDuelEnabled = bEnable;
}

BOOL CDuelMgr::IsPetDuelEnabled()
{
	return m_bIsPetDuelEnabled;
}

void CDuelMgr::SetDuelPlayer(int iPlayerNum, int iIndex, char * pszID)
{
	m_DuelPlayer[iPlayerNum].m_iIndex = iIndex;
	strncpy(m_DuelPlayer[iPlayerNum].m_szID, pszID, MAX_ID_SIZE);
	m_DuelPlayer[iPlayerNum].m_szID[MAX_ID_SIZE] = '\0';
}

void CDuelMgr::SetHeroAsDuelPlayer(int iPlayerNum)
{
	m_DuelPlayer[iPlayerNum].m_iIndex = Hero->Key;
	strncpy(m_DuelPlayer[iPlayerNum].m_szID, Hero->ID, MAX_ID_SIZE);
	m_DuelPlayer[iPlayerNum].m_szID[MAX_ID_SIZE] = '\0';
}

void CDuelMgr::SetScore(int iPlayerNum, int iScore)
{
	m_DuelPlayer[iPlayerNum].m_iScore = iScore;
}

void CDuelMgr::SetHP(int iPlayerNum, int iRate)
{
	m_DuelPlayer[iPlayerNum].m_fHPRate = iRate * 0.01f;
}

void CDuelMgr::SetSD(int iPlayerNum, int iRate)
{
	m_DuelPlayer[iPlayerNum].m_fSDRate = iRate * 0.01f;
}

char * CDuelMgr::GetDuelPlayerID(int iPlayerNum)
{
	return m_DuelPlayer[iPlayerNum].m_szID;
}

int CDuelMgr::GetDuelPlayerIndex(int iPlayerNum)
{
	return m_DuelPlayer[iPlayerNum].m_iIndex;
}

int CDuelMgr::GetScore(int iPlayerNum)
{
	return m_DuelPlayer[iPlayerNum].m_iScore;
}

float CDuelMgr::GetHP(int iPlayerNum)
{
	return m_DuelPlayer[iPlayerNum].m_fHPRate;
}

float CDuelMgr::GetSD(int iPlayerNum)
{
	return m_DuelPlayer[iPlayerNum].m_fSDRate;
}

BOOL CDuelMgr::IsDuelPlayer(CHARACTER * pCharacter, int iPlayerNum, BOOL bIncludeSummon)
{
	if (pCharacter->Key == m_DuelPlayer[iPlayerNum].m_iIndex && strncmp(pCharacter->ID, m_DuelPlayer[iPlayerNum].m_szID, MAX_ID_SIZE) == 0)
	{
		return TRUE;
	}
	else if (bIncludeSummon == TRUE && gCharacterManager.GetBaseClass(pCharacter->Class) == 0 && strncmp(pCharacter->OwnerID, m_DuelPlayer[iPlayerNum].m_szID, MAX_ID_SIZE) == 0)
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CDuelMgr::IsDuelPlayer(WORD wIndex, int iPlayerNum)
{
	return (m_DuelPlayer[iPlayerNum].m_iIndex == wIndex);
}

void CDuelMgr::SendDuelRequestAnswer(int iPlayerNum, BOOL bOK)
{
	SendRequestDuelOk(bOK, m_DuelPlayer[iPlayerNum].m_iIndex, m_DuelPlayer[iPlayerNum].m_szID);
}

void CDuelMgr::SetDuelChannel(int iChannelIndex, BOOL bEnable, BOOL bJoinable, char * pszID1, char * pszID2)
{
	m_DuelChannels[iChannelIndex].m_bEnable = bEnable;
	m_DuelChannels[iChannelIndex].m_bJoinable = bJoinable;
	strncpy(m_DuelChannels[iChannelIndex].m_szID1, pszID1, MAX_ID_SIZE);
	m_DuelChannels[iChannelIndex].m_szID1[MAX_ID_SIZE] = '\0';
	strncpy(m_DuelChannels[iChannelIndex].m_szID2, pszID2, MAX_ID_SIZE);
	m_DuelChannels[iChannelIndex].m_szID2[MAX_ID_SIZE] = '\0';
}

void CDuelMgr::RemoveAllDuelWatchUser()
{
	m_DuelWatchUserList.clear();
}

void CDuelMgr::AddDuelWatchUser(char * pszUserID)
{
	char szUserID[24] = { 0, };
	strncpy(szUserID, pszUserID, MAX_ID_SIZE);
	szUserID[MAX_ID_SIZE] = '\0';
	m_DuelWatchUserList.push_back(szUserID);
}

void CDuelMgr::RemoveDuelWatchUser(char * pszUserID)
{
	std::list<std::string>::iterator iter;
	for (iter = m_DuelWatchUserList.begin(); iter != m_DuelWatchUserList.end(); ++iter)
	{
		if (strncmp(iter->c_str(), pszUserID, MAX_ID_SIZE) == 0)
		{
			m_DuelWatchUserList.erase(iter);
			return;
		}
	}
	assert(!"RemoveDuelWatchUser!");
}

char * CDuelMgr::GetDuelWatchUser(int iIndex)
{
	if(m_DuelWatchUserList.size() <= (DWORD)iIndex)
		return NULL;

	int i = 0;
	std::list<std::string>::iterator iter;
	for (iter = m_DuelWatchUserList.begin(); iter != m_DuelWatchUserList.end(); ++iter, ++i)
	{
		if (i == iIndex)
		{
			return (char *)iter->c_str();
		}
	}
	return NULL;
}
