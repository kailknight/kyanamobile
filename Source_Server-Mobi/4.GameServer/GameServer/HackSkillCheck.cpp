// HackSkillCheck.cpp: implementation of the CHackSkillCheck class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CommandManager.h"
#include "HackSkillCheck.h"
#include "GensSystem.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "MemScript.h"
#include "Message.h"
#include "Notice.h"
#include "SkillManager.h"
#include "ServerInfo.h"
#include "Util.h"

CHackSkillCheck gHackSkillCheck;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CHackSkillCheck::CHackSkillCheck() // OK
{
	this->m_HackSkillInfo.clear();
}

CHackSkillCheck::~CHackSkillCheck() // OK
{

}

void CHackSkillCheck::Load(char* path) // OK
{
	CMemScript* lpMemScript = new CMemScript;

	if(lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR,path);
		return;
	}

	if(lpMemScript->SetBuffer(path) == 0)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}

	this->m_HackSkillInfo.clear();

	int count = 0;

	try
	{
		while(true)
		{
			if(lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}

			if(strcmp("end",lpMemScript->GetString()) == 0)
			{
				break;
			}

			HACKSKILL_INFO info;

			info.Index = count;

			info.Skill = lpMemScript->GetNumber();

			info.MinSpeed = lpMemScript->GetAsNumber();

			info.MaxSpeed = lpMemScript->GetAsNumber();

			info.Delay = lpMemScript->GetAsNumber();

			info.MaxCount = lpMemScript->GetAsNumber();

			this->m_HackSkillInfo.insert(std::pair<int,HACKSKILL_INFO>(info.Index,info));

			count++;
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
}

bool CHackSkillCheck::GetInfo(int index,HACKSKILL_INFO* lpInfo) // OK
{
	std::map<int,HACKSKILL_INFO>::iterator it = this->m_HackSkillInfo.find(index);

	if(it == this->m_HackSkillInfo.end())
	{
		return 0;
	}
	else
	{
		(*lpInfo) = it->second;
		return 1;
	}
}

bool CHackSkillCheck::GetInfoBySpeed(int Skill,int Speed, HACKSKILL_INFO* lpInfo) // OK
{
	for(std::map<int,HACKSKILL_INFO>::iterator it=this->m_HackSkillInfo.begin();it != this->m_HackSkillInfo.end();it++)
	{
		if(it->second.Skill == Skill && Speed >= it->second.MinSpeed && (Speed <= it->second.MaxSpeed || it->second.MaxSpeed == -1))
		{
			(*lpInfo) = it->second;
			return 1;
		}
	}

	return 0;
}

bool CHackSkillCheck::CheckSpeedHack(LPOBJ lpObj,int skill) // OK
{
	if (gServerInfo.m_SpeedHackSkillEnable != 1)
	{
		return 0;
	}

	HACKSKILL_INFO HackSkillInfo;

	if(this->GetInfoBySpeed(skill,lpObj->PhysiSpeed,&HackSkillInfo) == 0)
	{
		return 0;
	}

	if(lpObj->SpeedHackSkill != skill)
	{
		lpObj->SpeedHackSkill = skill;
		lpObj->SpeedHackCount = 0;
		lpObj->SpeedHackDelay = GetTickCount();
		lpObj->SpeedHackDialog = 0;
	}

	if(lpObj->SpeedHackCount > HackSkillInfo.MaxCount)
	{

		if( lpObj->SpeedHackDialog == 0)
		{
			LogAdd(LOG_RED,"[SpeedHackDetected][%s][%s] Skill: %d",lpObj->Account,lpObj->Name,skill);

			if (gServerInfo.m_SpeedHackSkillDialog == 1)
			{
				gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(843));	
			}
			lpObj->SpeedHackDialog = 1;
		}

		if (gServerInfo.m_SpeedHackSkillType == 1)
		{
			GCCloseClientSend(lpObj->Index,0);
			return 1;
		}
		else if (gServerInfo.m_SpeedHackSkillType == 2)
		{
			return 1;
		}
		else if (gServerInfo.m_SpeedHackSkillType == 3)
		{
			if( GetTickCount() > (lpObj->SpeedHackDelay + HackSkillInfo.Delay + (DWORD)gServerInfo.m_SpeedHackSkillPenalty))
			{
				lpObj->SpeedHackCount = 0;
				lpObj->SpeedHackDelay = GetTickCount();
				lpObj->SpeedHackDialog = 0;
			}
			return 1;
		}
		else
		{
			if( GetTickCount() > (lpObj->SpeedHackDelay+HackSkillInfo.Delay))
			{
				lpObj->SpeedHackCount = 0;
				lpObj->SpeedHackDelay = GetTickCount();
			}
		}
		return 0;
	}

	if( GetTickCount() < (lpObj->SpeedHackDelay+HackSkillInfo.Delay))
	{
		lpObj->SpeedHackCount++;
		return 0;
	}
	else
	{
		if (gServerInfo.m_SpeedHackSkillDebug == 1)
		{
			LogAdd(LOG_RED,"[SpeedHackTeste] Skill: %d | AttackSpeed: %d | Count: %d",skill,lpObj->PhysiSpeed,lpObj->SpeedHackCount);
		}
	}

	lpObj->SpeedHackCount = 0;
	lpObj->SpeedHackDelay = GetTickCount();

	return 0;
}

// Rejects casts that arrive faster than the character's attack speed allows.
// HackSkillCheck.txt ships empty and every Delay in Skill.txt is 0, so nothing
// else limited how fast cast packets could come in.
//
// The minimum gap between two casts is derived the way the client derives its
// animation speed (ZzzCharacter.cpp SetAttackSpeed: PlaySpeed = base + speed*0.004):
// SkillRateBaseMS at no speed, scaled by 0.30/(0.30+speed*0.004), then taken at
// SkillRatePercent of that and never below SkillRateMinMS. It is measured over
// the last 6 casts rather than per packet so latency bunching and the odd
// double packet never count against a legitimate player; only a sustained rate
// above the limit does. The higher of PhysiSpeed/MagicSpeed is used so a class
// that animates off either one gets the benefit of the doubt.
bool CHackSkillCheck::CheckSkillRate(LPOBJ lpObj,int skillIndex)
{
	if(gServerInfo.m_SkillRateEnable == 0)
	{
		return 0;
	}

	if(lpObj->Type != OBJECT_USER || lpObj->m_OfflineMode != 0 || lpObj->IsFakeOnline != 0)
	{
		return 0;
	}

	if(lpObj->Class < 0 || lpObj->Class > 30 || ((gServerInfo.m_SkillRateClassMask >> lpObj->Class) & 1) == 0)
	{
		return 0;
	}

	SKILL_INFO SkillInfo;

	if(gSkillManager.GetInfo(skillIndex,&SkillInfo) == 0 || (SkillInfo.Range == 0 && SkillInfo.Radio == 0))
	{
		return 0;
	}

	const int window = (int)(sizeof(lpObj->SkillRateTick)/sizeof(lpObj->SkillRateTick[0]));

	DWORD now = GetTickCount();

	if(lpObj->SkillRateLockUntil != 0 && (int)(lpObj->SkillRateLockUntil-now) > 0)
	{
		return 1;
	}

	int speed = ((lpObj->PhysiSpeed > lpObj->MagicSpeed) ? lpObj->PhysiSpeed : lpObj->MagicSpeed);

	speed = ((speed < 0) ? 0 : speed);

	float minMs = ((float)gServerInfo.m_SkillRateBaseMS*0.30f/(0.30f+((float)speed*0.004f)))*((float)gServerInfo.m_SkillRatePercent/100.0f);

	minMs = ((minMs < (float)gServerInfo.m_SkillRateMinMS) ? (float)gServerInfo.m_SkillRateMinMS : minMs);

	bool violation = 0;

	DWORD span = 0;

	if(lpObj->SkillRateFilled >= window)
	{
		span = now-lpObj->SkillRateTick[lpObj->SkillRatePos];

		violation = ((float)span < (minMs*(float)(window-1)));
	}

	lpObj->SkillRateTick[lpObj->SkillRatePos] = now;

	lpObj->SkillRatePos = ((lpObj->SkillRatePos+1)%window);

	lpObj->SkillRateFilled = ((lpObj->SkillRateFilled < window) ? (lpObj->SkillRateFilled+1) : window);

	if(violation == 0)
	{
		if(lpObj->SkillRateStrikes > 0 && (now-lpObj->SkillRateStrikeTick) > 10000)
		{
			lpObj->SkillRateStrikes = 0;
		}

		return 0;
	}

	lpObj->SkillRateStrikes++;

	lpObj->SkillRateStrikeTick = now;

	if((now-lpObj->SkillRateLogTick) > 5000)
	{
		lpObj->SkillRateLogTick = now;

		LogAdd(LOG_RED,"[SkillRate][%s][%s] Class: %d Skill: %d Speed: %d Last %d casts in %u ms (min %d ms each) Strikes: %d",lpObj->Account,lpObj->Name,lpObj->Class,skillIndex,speed,window,span,(int)minMs,lpObj->SkillRateStrikes);

		gLog.Output(LOG_HACK,"[SkillRate][%s][%s] Class: %d Skill: %d Speed: %d Last %d casts in %u ms (min %d ms each) Strikes: %d",lpObj->Account,lpObj->Name,lpObj->Class,skillIndex,speed,window,span,(int)minMs,lpObj->SkillRateStrikes);
	}

	// Same penalty types as SpeedHackSkillType: 0 log only, 1 disconnect,
	// 2 drop the cast, 3 drop casts for SpeedHackSkillPenalty ms.
	if(gServerInfo.m_SpeedHackSkillType == 0)
	{
		return 0;
	}

	if(gServerInfo.m_SpeedHackSkillType == 1)
	{
		if(lpObj->SkillRateStrikes >= gServerInfo.m_SkillRateKickCount)
		{
			gLog.Output(LOG_HACK,"[SkillRate][%s][%s] Disconnected for casting too fast",lpObj->Account,lpObj->Name);

			GCCloseClientSend(lpObj->Index,0);
		}

		return 1;
	}

	if(gServerInfo.m_SpeedHackSkillType == 3 && lpObj->SkillRateStrikes >= gServerInfo.m_SkillRateKickCount)
	{
		lpObj->SkillRateLockUntil = now+(DWORD)gServerInfo.m_SpeedHackSkillPenalty;

		lpObj->SkillRateStrikes = 0;
	}

	return 1;
}
