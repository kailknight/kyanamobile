// MonsterScaling.cpp: implementation of the CMonsterScaling class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MonsterScaling.h"
#include "MonsterManager.h"
#include "Util.h"

CMonsterScaling gMonsterScaling;

CMonsterScaling::CMonsterScaling() // OK
{
	this->m_Enable = false;
	memset(this->m_Band,0,sizeof(this->m_Band));
	memset(this->m_Base,0,sizeof(this->m_Base));
}

void CMonsterScaling::Load(char* path) // OK
{
	this->m_Enable = GetPrivateProfileInt("General","Enable",0,path) != 0;
	this->m_HpRate = GetPrivateProfileInt("General","HpRate",100,path);
	this->m_DamageRate = GetPrivateProfileInt("General","DamageRate",100,path);
	this->m_DefenseRate = GetPrivateProfileInt("General","DefenseRate",100,path);
	this->m_HitRate = GetPrivateProfileInt("General","HitRate",100,path);
	this->m_ScaleRewards = GetPrivateProfileInt("General","ScaleRewards",1,path);
	this->m_BossLife = GetPrivateProfileInt("General","BossLifeAtLeast",1000000,path);
	this->m_MinFactor = GetPrivateProfileInt("General","MinFactor",25,path);
	this->m_MaxFactor = GetPrivateProfileInt("General","MaxFactor",2000,path);

	if(this->m_MinFactor < 1) this->m_MinFactor = 1;
	if(this->m_MaxFactor < this->m_MinFactor) this->m_MaxFactor = this->m_MinFactor;

	// [Maps] Map<number> = <min level>,<max level>. Maps not listed don't scale.
	memset(this->m_Band,0,sizeof(this->m_Band));

	for(int map = 0; map < MAX_MAP; map++)
	{
		char key[16];
		char value[64] = {0};
		wsprintf(key,"Map%d",map);
		GetPrivateProfileString("Maps",key,"",value,sizeof(value),path);

		int minLevel = 0, maxLevel = 0;

		if(sscanf_s(value,"%d,%d",&minLevel,&maxLevel) == 2 && minLevel > 0 && maxLevel >= minLevel)
		{
			this->m_Band[map].MinLevel = minLevel;
			this->m_Band[map].MaxLevel = maxLevel;
		}
	}

	// [Exclude] Monsters = comma-separated Monster.txt indexes that never scale.
	this->m_Exclude.clear();

	char list[4096] = {0};
	GetPrivateProfileString("Exclude","Monsters","",list,sizeof(list),path);

	char* context = NULL;

	for(char* token = strtok_s(list,", \t",&context); token != NULL; token = strtok_s(NULL,", \t",&context))
	{
		this->m_Exclude.insert(atoi(token));
	}

	int maps = 0;
	for(int map = 0; map < MAX_MAP; map++) if(this->m_Band[map].MinLevel > 0) maps++;

	LogAdd(LOG_BLACK,"[MonsterScaling] %s - %d map(s), %d excluded monster(s), bosses from %d HP",
		this->m_Enable ? "Enabled" : "Disabled",maps,(int)this->m_Exclude.size(),this->m_BossLife);
}

bool CMonsterScaling::IsExcluded(LPOBJ lpObj) // OK
{
	if(this->m_Exclude.count(lpObj->Class) != 0)
	{
		return 1;
	}

	MONSTER_INFO* lpInfo = gMonsterManager.GetInfo(lpObj->Class);

	if(lpInfo == 0 || lpInfo->Level <= 0)
	{
		return 1; // NPCs and unknown classes
	}

	// Bosses: anything this tough in Monster.txt is a boss or an event monster.
	if(this->m_BossLife > 0 && lpInfo->Life >= this->m_BossLife)
	{
		return 1;
	}

	return 0;
}

void CMonsterScaling::OnPlayerAttack(LPOBJ lpObj,LPOBJ lpTarget) // OK
{
	if(this->m_Enable == 0 || lpObj->Type != OBJECT_USER || lpTarget->Type != OBJECT_MONSTER)
	{
		return;
	}

	if(OBJECT_RANGE(lpTarget->Index) == 0 || MAP_RANGE(lpTarget->Map) == 0)
	{
		return;
	}

	MONSTER_SCALING_BASE& base = this->m_Base[lpTarget->Index];

	// First attacker only: the monster keeps that level until it respawns.
	if(base.Scaled || lpTarget->Live == 0 || OBJECT_RANGE(lpTarget->SummonIndex) != 0) // summoned monsters never scale
	{
		return;
	}

	const MONSTER_SCALING_BAND& band = this->m_Band[lpTarget->Map];

	if(band.MinLevel <= 0 || this->IsExcluded(lpTarget))
	{
		return;
	}

	MONSTER_INFO* lpInfo = gMonsterManager.GetInfo(lpTarget->Class);

	int baseLevel = (lpInfo != 0 && lpInfo->Level > 0) ? lpInfo->Level : lpTarget->Level;

	if(baseLevel <= 0)
	{
		return;
	}

	// The attacker's level (with master level), kept inside the map's band.
	int targetLevel = lpObj->Level + lpObj->MasterLevel;

	if(targetLevel < band.MinLevel) targetLevel = band.MinLevel;
	if(targetLevel > band.MaxLevel) targetLevel = band.MaxLevel;

	// factor in %: 100 = unchanged.
	int factor = (targetLevel * 100) / baseLevel;

	if(factor < this->m_MinFactor) factor = this->m_MinFactor;
	if(factor > this->m_MaxFactor) factor = this->m_MaxFactor;

	if(factor == 100)
	{
		return;
	}

	base.Scaled = true;
	base.Level = lpTarget->Level;
	base.Life = lpTarget->Life;
	base.MaxLife = lpTarget->MaxLife;
	base.ScriptMaxLife = lpTarget->ScriptMaxLife;
	base.DamageMin = lpTarget->PhysiDamageMin;
	base.DamageMax = lpTarget->PhysiDamageMax;
	base.Defense = lpTarget->Defense;
	base.MagicDefense = lpTarget->MagicDefense;
	base.AttackSuccessRate = lpTarget->AttackSuccessRate;
	base.DefenseSuccessRate = lpTarget->DefenseSuccessRate;

	// Only <rate>% of the change is applied: value * (100 + (factor-100)*rate/100) / 100.
	#define SCALE(value,rate) ((__int64)(value) * (100 + ((__int64)(factor) - 100) * (rate) / 100) / 100)

	float lifeRatio = (base.MaxLife > 0) ? (base.Life / base.MaxLife) : 1.0f;

	lpTarget->MaxLife = (float)max(1,SCALE(base.MaxLife,this->m_HpRate));
	lpTarget->Life = lpTarget->MaxLife * lifeRatio;
	lpTarget->ScriptMaxLife = (float)max(1,SCALE(base.ScriptMaxLife,this->m_HpRate));
	lpTarget->PhysiDamageMin = (int)max(0,SCALE(base.DamageMin,this->m_DamageRate));
	lpTarget->PhysiDamageMax = (int)max(lpTarget->PhysiDamageMin,SCALE(base.DamageMax,this->m_DamageRate));
	lpTarget->Defense = (int)max(0,SCALE(base.Defense,this->m_DefenseRate));
	lpTarget->MagicDefense = (int)max(0,SCALE(base.MagicDefense,this->m_DefenseRate));
	lpTarget->AttackSuccessRate = (int)max(0,SCALE(base.AttackSuccessRate,this->m_HitRate));
	lpTarget->DefenseSuccessRate = (int)max(0,SCALE(base.DefenseSuccessRate,this->m_HitRate));

	#undef SCALE

	// EXP, Zen and the drop level all read the monster's level.
	if(this->m_ScaleRewards != 0)
	{
		lpTarget->Level = targetLevel;
	}
}

void CMonsterScaling::Clear(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) != 0)
	{
		this->m_Base[aIndex].Scaled = false;
	}
}

void CMonsterScaling::Reset(LPOBJ lpObj) // OK
{
	if(OBJECT_RANGE(lpObj->Index) == 0)
	{
		return;
	}

	MONSTER_SCALING_BASE& base = this->m_Base[lpObj->Index];

	if(base.Scaled == 0)
	{
		return;
	}

	base.Scaled = false;

	lpObj->Level = base.Level;
	lpObj->MaxLife = base.MaxLife;
	lpObj->Life = base.MaxLife;
	lpObj->ScriptMaxLife = base.ScriptMaxLife;
	lpObj->PhysiDamageMin = base.DamageMin;
	lpObj->PhysiDamageMax = base.DamageMax;
	lpObj->Defense = base.Defense;
	lpObj->MagicDefense = base.MagicDefense;
	lpObj->AttackSuccessRate = base.AttackSuccessRate;
	lpObj->DefenseSuccessRate = base.DefenseSuccessRate;
}
