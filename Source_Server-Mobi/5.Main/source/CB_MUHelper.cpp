#include "stdafx.h"
#include <map>
#include "CB_MUHelper.h"
#include "NewUISystem.h"
#include "CBInterface.h"
#include "CUIController.h"
#include "CharacterManager.h"
#include "Util.h"
#include "Protocol.h"
#include "NewUIBase.h"
#include "Other.h"
#include "Protocol.h"
#include "ZzzLodTerrain.h"
#include "SkillManager.h"
#include "ZzzInterface.h"
#include "wsclientinline.h"
#include "ZzzAI.h"

extern bool LogOut;
extern int TargetX, TargetY;
extern MovementSkill g_MovementSkill;
extern int SelectedCharacter;
SpinLock _targetsLock;
SpinLock _itemsLock;

CB_MUHelper* gCB_MUHelper = nullptr;
int CheckKQ = 0;
// Monsters the helper could not path to, skipped for a few seconds (otherwise it stays locked on one it cannot reach).
static std::map<int, DWORD> s_HelperUnreachable;
static bool IsHelperUnreachable(int key)
{
	auto it = s_HelperUnreachable.find(key);
	if (it == s_HelperUnreachable.end()) return false;
	if (GetTickCount() > it->second) { s_HelperUnreachable.erase(it); return false; }
	return true;
}
CB_MUHelper::CB_MUHelper()
{
	CB_MUHelper::ClearData();

	for (int i = 0; i < eMaxVaule; i++)
	{
		CacheAction[i] = 0;
	}
	m_ActionMoveRegen = 0;
	TimeDrawCircle = 0;
	RangeDraw = -1;
}

CB_MUHelper::~CB_MUHelper()
{

}
void CB_MUHelper::ClearData()
{
	m_setTargets.clear();
	m_setTargetsAttacking.clear();
	m_setItems.clear();
	m_iCurrentItem = -1;
	m_iCurrentTarget = -1;
	m_iCurrentItemMove = -1;
	ClearDateOn = false;
	m_bTimerActivatedBuffOngoing = true;


	m_SelectedCharacter = -1;
	m_DistanceSelectCharacter = -1;



}
void CB_MUHelper::AddTarget(int iTargetId, bool bIsAttacking)
{
	_targetsLock.lock();

	m_setTargets.insert(iTargetId);

	if (bIsAttacking)
	{
		m_setTargetsAttacking.insert(iTargetId);
	}

	_targetsLock.unlock();
}

void CB_MUHelper::DeleteTarget(int iTargetId)
{
	_targetsLock.lock();

	m_setTargets.erase(iTargetId);
	m_setTargetsAttacking.erase(iTargetId);

	_targetsLock.unlock();

	if (iTargetId == m_iCurrentTarget)
	{
		m_iCurrentTarget = -1;
	}
}


void CB_MUHelper::AddItem(int iItemId)
{

	_itemsLock.lock();
	m_setItems.insert(iItemId);
	_itemsLock.unlock();


}

void CB_MUHelper::DeleteItem(int iItemId)
{
	_itemsLock.lock();
	m_setItems.erase(iItemId);
	_itemsLock.unlock();

	if (iItemId == m_iCurrentItem)
	{
		m_iCurrentItem = MAX_ITEMS;
	}
}

double GetDistance(CHARACTER* a2)
{
	float v3; // [esp+4h] [ebp-18h]
	float v4; // [esp+14h] [ebp-8h]
	float v5; // [esp+18h] [ebp-4h]

	if (!a2)
		return 0.0;
	v4 = (float)(Hero->PositionX - a2->PositionX);
	v5 = (float)(Hero->PositionY - a2->PositionY);
	v3 = v4 * v4 + v5 * v5;
	//return sqrtf(v3);
	return sqrt(pow(((float)Hero->PositionX - (float)a2->PositionX), 2) + pow(((float)Hero->PositionY - (float)a2->PositionY), 2));
}
double GetDistanceXY(int X, int Y, int bX, int bY)
{
	float v3; // [esp+4h] [ebp-18h]
	float v4; // [esp+14h] [ebp-8h]
	float v5; // [esp+18h] [ebp-4h]

	v4 = (float)(X - bX);
	v5 = (float)(Y - bY);
	v3 = v4 * v4 + v5 * v5;
	//return sqrtf(v3);
	return sqrt(pow(((float)X - (float)bX), 2) + pow(((float)Y - (float)bY), 2));
}
int BuffTick = 0;
int GetSkillBuffParty(int TypeSkill)
{
	switch (TypeSkill)
	{
	case 1:
	{
		if (gCharacterManager.GetBaseClass(Hero->Class) == CNewUIBCustomMenuInfo::Fairy_Elf)
		{
			//=== khong co skill mns thi bo qua
			if (gSkillManager.FindHeroSkill((ActionSkillType)AT_SKILL_HEALING)) return AT_SKILL_HEALING;
			if (gSkillManager.FindHeroSkill((ActionSkillType)MASTER_SKILL_ADD_HEAL_IMPROVED)) return MASTER_SKILL_ADD_HEAL_IMPROVED;
		}
	}
	break;
	case 2:
	{
		if (gCharacterManager.GetBaseClass(Hero->Class) == CNewUIBCustomMenuInfo::Dark_Wizard)
		{
			//=== khong co skill mns thi bo qua
			if (gSkillManager.FindHeroSkill(AT_SKILL_WIZARDDEFENSE)) return AT_SKILL_WIZARDDEFENSE;
			if (gSkillManager.FindHeroSkill((ActionSkillType)MASTER_SKILL_ADD_MANA_SHIELD_IMPROVED)) return MASTER_SKILL_ADD_MANA_SHIELD_IMPROVED;
			if (gSkillManager.FindHeroSkill((ActionSkillType)MASTER_SKILL_ADD_MANA_SHIELD_ENHANCED)) return MASTER_SKILL_ADD_MANA_SHIELD_ENHANCED;
			if (gSkillManager.FindHeroSkill((ActionSkillType)MASTER_SKILL_ADD_MANA_SHIELD_MASTERED)) return MASTER_SKILL_ADD_MANA_SHIELD_MASTERED;
		}
	}
	break;
	case 3:
	{
		if (gCharacterManager.GetBaseClass(Hero->Class) == CNewUIBCustomMenuInfo::Fairy_Elf)
		{

			if (gSkillManager.FindHeroSkill((ActionSkillType)AT_SKILL_DEFENSE)) return AT_SKILL_DEFENSE;
			if (gSkillManager.FindHeroSkill((ActionSkillType)MASTER_SKILL_ADD_GREATER_DEFENSE_IMPROVED)) return MASTER_SKILL_ADD_GREATER_DEFENSE_IMPROVED;
			if (gSkillManager.FindHeroSkill((ActionSkillType)MASTER_SKILL_ADD_GREATER_DEFENSE_ENHANCED)) return MASTER_SKILL_ADD_GREATER_DEFENSE_ENHANCED;
		}
	}
	break;
	case 4:
	{
		if (gCharacterManager.GetBaseClass(Hero->Class) == CNewUIBCustomMenuInfo::Fairy_Elf)
		{

			if (gSkillManager.FindHeroSkill((ActionSkillType)AT_SKILL_ATTACK)) return AT_SKILL_ATTACK;
			if (gSkillManager.FindHeroSkill((ActionSkillType)MASTER_SKILL_ADD_GREATER_DAMAGE_IMPROVED)) return MASTER_SKILL_ADD_GREATER_DAMAGE_IMPROVED;
			if (gSkillManager.FindHeroSkill((ActionSkillType)MASTER_SKILL_ADD_GREATER_DAMAGE_ENHANCED)) return MASTER_SKILL_ADD_GREATER_DAMAGE_ENHANCED;
		}
	}
	break;
	case 5: //Get Skill hut mau
	{
		if (gCharacterManager.GetBaseClass(Hero->Class) == CNewUIBCustomMenuInfo::Summoner)
		{
			if (gSkillManager.FindHeroSkill((ActionSkillType)AT_SKILL_ALICE_DRAINLIFE)) return AT_SKILL_ALICE_DRAINLIFE;
			if (gSkillManager.FindHeroSkill((ActionSkillType)MASTER_SKILL_ADD_DRAIN_LIFE_IMPROVED)) return MASTER_SKILL_ADD_DRAIN_LIFE_IMPROVED;
			if (gSkillManager.FindHeroSkill((ActionSkillType)MASTER_SKILL_ADD_DRAIN_LIFE_ENHANCED)) return MASTER_SKILL_ADD_DRAIN_LIFE_ENHANCED;
		}
	}
	break;
	default:
		break;
	}
	return -1;
}

int CB_MUHelper::SimulateSkill(int iSkill, bool bTargetRequired, int iTarget)
{
	extern MovementSkill g_MovementSkill;
	extern int SelectedCharacter;
	extern int TargetX, TargetY;

	g_MovementSkill.m_iSkill = iSkill;
	g_MovementSkill.m_bMagic = true;

	float fSkillDistance = gSkillManager.GetSkillDistance(iSkill, Hero);

	// Inferno is cast around the caster, so Skill_eng.bmd gives it distance 0.
	// The approach below casts only once the target is inside that distance,
	// which 0 never is: the helper kept walking into the monster and never
	// cast. Treat it as a 3-tile skill here - well inside the ring it hits.
	if (fSkillDistance < 1.0f && (iSkill == AT_SKILL_INFERNO || iSkill == MASTER_SKILL_ADD_INFERNO_IMPROVED1 || iSkill == MASTER_SKILL_ADD_INFERNO_IMPROVED2))
	{
		fSkillDistance = 3.0f;
	}

	if (bTargetRequired)
	{
		if (iTarget == -1)
		{
			CheckKQ = 1;
			return 0;
		}

		SelectedCharacter = FindCharacterIndex(iTarget);
		if (SelectedCharacter == MAX_CHARACTERS_CLIENT)
		{
			DeleteTarget(iTarget);
			CheckKQ = 2;
			return 0;
		}

		CHARACTER* pTarget = &CharactersClient[SelectedCharacter];
		if (pTarget->Dead)
		{
			DeleteTarget(iTarget);
			CheckKQ = 3;
			return 0;
		}


		//gInterface.DrawMessage(1, "Target %d (%d) (%d,%d) Skill %d %f / %d", iTarget, pTarget->Object.Kind , CheckTile(Hero, &Hero->Object, fSkillDistance), CheckWall(Hero->PositionX, Hero->PositionY, TargetX, TargetY), m_iCurrentSkill, gSkillManager.GetSkillDistance(iSkill, Hero), ComputeDistanceFromTarget(pTarget));
		//
		TargetX = (int)(pTarget->Object.Position[0] / TERRAIN_SCALE);
		TargetY = (int)(pTarget->Object.Position[1] / TERRAIN_SCALE);

		g_MovementSkill.m_iTarget = SelectedCharacter;
		float DistanceTarget = GetDistanceXY(Hero->PositionX, Hero->PositionY, pTarget->PositionX, pTarget->PositionY);
		if (DistanceTarget < (fSkillDistance - 1.0f))
		{
			//gInterface.DrawMessage(1, "AttackNext %d %d/%d", iTarget, ComputeDistanceFromTarget(pTarget) , (int)(fSkillDistance - 1.0f));
			TargetX = Hero->PositionX;
			TargetY = Hero->PositionY;
			goto AttackNext;
			return 1;
		}

		PATH_t tempPath;

		bool bHasPath = PathFinding2(Hero->PositionX, Hero->PositionY, TargetX, TargetY, &tempPath, fSkillDistance + 5);
		bool bTargetNear = CheckTile(Hero, &Hero->Object, fSkillDistance);
		bool bNoWall = CheckWall(Hero->PositionX, Hero->PositionY, TargetX, TargetY);

		// target not reachable, ignore it
		if (!bHasPath)
		{
			DeleteTarget(iTarget);
			s_HelperUnreachable[iTarget] = GetTickCount() + 5000;
			m_SelectedCharacter = -1;
			m_DistanceSelectCharacter = 1.f;
			SelectedCharacter = -1;
			CheckKQ = 4;
			return 0;
		}

		// target is not near or the path is obstructed by a wall, move closer
		if (!bTargetNear || !bNoWall)
		{
			//Hero->Path.Lock.lock();

			// Limit movement to 2 steps at a time
			int pathNum = min(tempPath.PathNum, 2);
			for (int i = 0; i < pathNum; i++)
			{
				Hero->Path.PathX[i] = tempPath.PathX[i];
				Hero->Path.PathY[i] = tempPath.PathY[i];
			}
			Hero->Path.PathNum = pathNum;
			Hero->Path.CurrentPath = 0;
			Hero->Path.CurrentPathFloat = 0;

			//Hero->Path.Lock.unlock();

			SendMove(Hero, &Hero->Object);
			CheckKQ = 5;
			return 0;
		}

	}
	else
	{
		SelectedCharacter = FindCharacterIndex(Hero->Key);
		g_MovementSkill.m_iTarget = SelectedCharacter;
		TargetX = Hero->PositionX;
		TargetY = Hero->PositionY;
	}
AttackNext:
	int iSkillResult = ExecuteSkill(Hero, iSkill, fSkillDistance);
	if (iSkillResult == -1)
	{
		DeleteTarget(iTarget);
	}
	CheckKQ = 6;
	return (int)(iSkillResult == 1);
}


int CB_MUHelper::BuffTarget(CHARACTER * pTargetChar, int iBuffSkill)
{
	// TODO: List other buffs here
	if ((iBuffSkill == AT_SKILL_ATTACK
		|| iBuffSkill == AT_SKILL_ATT_POWER_UP
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_DAMAGE_IMPROVED
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_DAMAGE_ENHANCED
		|| iBuffSkill == AT_SKILL_ATT_POWER_UP + 1
		|| iBuffSkill == AT_SKILL_ATT_POWER_UP + 2
		|| iBuffSkill == AT_SKILL_ATT_POWER_UP + 3
		|| iBuffSkill == AT_SKILL_ATT_POWER_UP + 4)
		&& ((!g_isCharacterBuff((&pTargetChar->Object), eBuff_Attack))
			))
	{
		return 0;
	}

	if ((iBuffSkill == AT_SKILL_DEFENSE
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_DEFENSE_IMPROVED
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_DEFENSE_ENHANCED
		|| iBuffSkill == AT_SKILL_DEF_POWER_UP
		|| iBuffSkill == AT_SKILL_DEF_POWER_UP + 1
		|| iBuffSkill == AT_SKILL_DEF_POWER_UP + 2
		|| iBuffSkill == AT_SKILL_DEF_POWER_UP + 3
		|| iBuffSkill == AT_SKILL_DEF_POWER_UP + 4)
		&& ((!g_isCharacterBuff((&pTargetChar->Object), eBuff_Defense))
			))
	{
		return 0;
	}



	if ((iBuffSkill == AT_SKILL_WIZARDDEFENSE
		|| iBuffSkill == MASTER_SKILL_ADD_MANA_SHIELD_IMPROVED
		|| iBuffSkill == MASTER_SKILL_ADD_MANA_SHIELD_ENHANCED
		|| iBuffSkill == MASTER_SKILL_ADD_MANA_SHIELD_MASTERED
		|| iBuffSkill == AT_SKILL_SOUL_UP
		|| iBuffSkill == AT_SKILL_SOUL_UP + 1
		|| iBuffSkill == AT_SKILL_SOUL_UP + 2
		|| iBuffSkill == AT_SKILL_SOUL_UP + 3
		|| iBuffSkill == AT_SKILL_SOUL_UP + 4)
		&& ((!g_isCharacterBuff((&pTargetChar->Object), eBuff_PhysDefense)
			&& !g_isCharacterBuff((&pTargetChar->Object), EFFECT_MANA_SHIELD_MASTERED))))
	{
		return 0;
	}

	if ((iBuffSkill == AT_SKILL_VITALITY
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_LIFE_IMPROVED
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_LIFE_ENHANCED
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_LIFE_MASTERED
		|| iBuffSkill == AT_SKILL_LIFE_UP
		|| iBuffSkill == AT_SKILL_LIFE_UP + 1
		|| iBuffSkill == AT_SKILL_LIFE_UP + 2
		|| iBuffSkill == AT_SKILL_LIFE_UP + 3
		|| iBuffSkill == AT_SKILL_LIFE_UP + 4)
		&& ((!g_isCharacterBuff((&pTargetChar->Object), eBuff_HpRecovery) && !g_isCharacterBuff((&pTargetChar->Object), EFFECT_GREATER_LIFE_ENHANCED) && !g_isCharacterBuff((&pTargetChar->Object), EFFECT_GREATER_LIFE_MASTERED))))
	{
		return 0;
	}



	if ((iBuffSkill == AT_SKILL_ADD_CRITICAL
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_CRITICAL_DAMAGE_EXTENDED
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_CRITICAL_DAMAGE_ENHANCED
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_CRITICAL_DAMAGE_MASTERED
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_CRITICAL_DAMAGE_IMPROVED
		)
		&& (!g_isCharacterBuff((&pTargetChar->Object), eBuff_AddCriticalDamage)
			&& !g_isCharacterBuff((&pTargetChar->Object), EFFECT_GREATER_CRITICAL_DAMAGE_MASTERED)
			&& !g_isCharacterBuff((&pTargetChar->Object), EFFECT_GREATER_CRITICAL_DAMAGE_EXTENDED)
			))
	{
		return 0;
	}

	if ((iBuffSkill == AT_SKILL_ALICE_BERSERKER
		|| iBuffSkill == MASTER_SKILL_ADD_SWORD_POWER_IMPROVED
		|| iBuffSkill == MASTER_SKILL_ADD_SWORD_POWER_ENHANCED
		|| iBuffSkill == MASTER_SKILL_ADD_SWORD_POWER_MASTERED
		)
		&& (!g_isCharacterBuff((&pTargetChar->Object), eBuff_Berserker)
			&& !g_isCharacterBuff((&pTargetChar->Object), EFFECT_SWORD_POWER_IMPROVED)
			&& !g_isCharacterBuff((&pTargetChar->Object), EFFECT_SWORD_POWER_ENHANCED)
			&& !g_isCharacterBuff((&pTargetChar->Object), EFFECT_SWORD_POWER_MASTERED)
			))
	{
		return 0;
	}
	if ((iBuffSkill == AT_SKILL_ALICE_THORNS)
		&& (!g_isCharacterBuff((&pTargetChar->Object), eBuff_Thorns)))
	{
		return 0;
	}
	//==RF
	if ((iBuffSkill == AT_SKILL_ATT_UP_OURFORCES)
		&& (!g_isCharacterBuff((&pTargetChar->Object), eBuff_Att_up_Ourforces)))
	{
		return 0;
	}
	if ((iBuffSkill == AT_SKILL_HP_UP_OURFORCES
		|| iBuffSkill == MASTER_SKILL_ADD_FITNESS_IMPROVED)
		&& (!g_isCharacterBuff((&pTargetChar->Object), eBuff_Hp_up_Ourforces) && !g_isCharacterBuff((&pTargetChar->Object), EFFECT_FITNESS_IMPROVED)))
	{
		return 0;
	}
	if ((iBuffSkill == AT_SKILL_DEF_UP_OURFORCES
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_DEFENSE_SUCCESS_RATE_IMPROVED
		|| iBuffSkill == MASTER_SKILL_ADD_GREATER_DEFENSE_SUCCESS_RATE_ENHANCED
		)
		&& (!g_isCharacterBuff((&pTargetChar->Object), eBuff_Def_up_Ourforces)
			&& !g_isCharacterBuff((&pTargetChar->Object), EFFECT_GREATER_DEFENSE_SUCCESS_RATE_IMPROVED)
			&& !g_isCharacterBuff((&pTargetChar->Object), EFFECT_GREATER_DEFENSE_SUCCESS_RATE_ENHANCED)
			))
	{
		return 0;
	}


	if (pTargetChar->Key == Hero->Key)
	{
		//=== Vo dinh tien
		if ((iBuffSkill == AT_SKILL_INFINITY_ARROW
			|| iBuffSkill == MASTER_SKILL_ADD_INFINITY_ARROW_IMPROVED)
			&&
			(!g_isCharacterBuff((&pTargetChar->Object), eBuff_InfinityArrow) && !g_isCharacterBuff((&pTargetChar->Object), EFFECT_INFINITY_ARROW_IMPROVED))
			)
		{
			return 0;
		}
		//=== Tang ma luc
		if ((iBuffSkill == AT_SKILL_SWELL_OF_MAGICPOWER
			|| iBuffSkill == MASTER_SKILL_ADD_MAGIC_CIRCLE_IMPROVED
			|| iBuffSkill == MASTER_SKILL_ADD_MAGIC_CIRCLE_ENHANCED
			)
			&& (!g_isCharacterBuff((&pTargetChar->Object), eBuff_SwellOfMagicPower)
				&& !g_isCharacterBuff((&pTargetChar->Object), EFFECT_MAGIC_CIRCLE_IMPROVED)
				&& !g_isCharacterBuff((&pTargetChar->Object), EFFECT_MAGIC_CIRCLE_ENHANCED)
				))
		{
			return 0;
		}
	}

	return 1;
}
int GetSkillBuffHero = 0;
bool MuHelperActiveBuff()
{

	if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.AutoBuff && gCB_MUHelper->CacheAction[CB_MUHelper::eBuff] < 6)
	{

		if (GetSkillBuffHero > 2) GetSkillBuffHero = 0;
		if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Buff[GetSkillBuffHero] != 0)
		{
			if (gSkillManager.FindHeroSkill((ActionSkillType)g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Buff[GetSkillBuffHero]) == 1 && g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Buff[GetSkillBuffHero] > 0)
			{
				int iSkillType = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Buff[GetSkillBuffHero];
				//DrawMessage(1, "Get Buff %d <> %d %d (%d)", gSkillManager.FindHeroCurrentSkill((ActionSkillType)iSkillType), iSkillType, gCB_MUHelper->BuffTarget(Hero, iSkillType), GetSkillBuffHero);

				if (!gCB_MUHelper->BuffTarget(Hero, iSkillType) && !Hero->SafeZone)
				{
					//DrawMessage(1, "MuHelperActiveBuff Hero %d (%d) (%d)", iSkillType, gCB_MUHelper->BuffTarget(Hero, iSkillType), GetSkillBuffHero);
					Hero->CurrentSkill = gSkillManager.FindHeroCurrentSkill((ActionSkillType)iSkillType);
					gCB_MUHelper->SimulateSkill(iSkillType, true, Hero->Key);
					gCB_MUHelper->CacheAction[CB_MUHelper::eBuff]++;
					return 0;
				}

			}
		}
		GetSkillBuffHero++;

	}
	if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PartyAutoBuff && gCB_MUHelper->CacheAction[CB_MUHelper::eBuffParty] < PartyNumber * 5) //Neu tick vao party buff
	{
		//if (GetTickCount() >= g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.TimerBuffParty)
		{
			//===Buff Party
			for (int i = 0; i < PartyNumber; i++)
			{
				if (Party[i].index >= 0 && Party[i].index < MAX_CHARACTERS_CLIENT)
				{

					// Party buffs: the Buff skills you selected (Buff Duration slots), cast
					// on every party member nearby who does not have that buff - so an
					// expired buff is put back. Used to be hard-coded per class (Wizard:
					// Mana Shield, Elf: Greater Damage/Defense) and ignored your choice.
					CHARACTER* pChar = &CharactersClient[Party[i].index];
					if (pChar == Hero || pChar->SafeZone || pChar->Dead || !pChar->Object.Live || !pChar->Object.Visible) continue;
					if (GetDistanceXY(Hero->PositionX, Hero->PositionY, pChar->PositionX, pChar->PositionY) > 8) continue;

					for (int b = 0; b < 3; b++)
					{
						const int iSkillType = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Buff[b];
						if (iSkillType <= 0 || gSkillManager.FindHeroSkill((ActionSkillType)iSkillType) == 0) continue;
						if (gCB_MUHelper->BuffTarget(pChar, iSkillType)) continue;

						Hero->CurrentSkill = gSkillManager.FindHeroCurrentSkill((ActionSkillType)iSkillType);
						gCB_MUHelper->SimulateSkill(iSkillType, true, pChar->Key);
						gCB_MUHelper->CacheAction[CB_MUHelper::eBuffParty]++;
						return 0;
					}
				}
			}

			if (GetTickCount() >= g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.TimerBuffParty)
			{
				gCB_MUHelper->m_bTimerActivatedBuffOngoing = true;
				g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.TimerBuffParty = GetTickCount() + ((g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PartyBuffTime) * 1000);
			}
			else
			{
				gCB_MUHelper->m_bTimerActivatedBuffOngoing = false;
			}
		}
	}
	if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PartyAutoHeal && gCB_MUHelper->CacheAction[CB_MUHelper::eBuffPartyHeal] < PartyNumber * 5) //hoi mau trong nhom
	{
		//===Buff Party
		for (int i = 0; i < PartyNumber; i++)
		{
			if (Party[i].index >= 0 && Party[i].index < MAX_CHARACTERS_CLIENT)
			{
				CHARACTER* pChar = &CharactersClient[Party[i].index];
				OBJECT* pObj = &pChar->Object;
				if ((Party[i].stepHP * 10) < g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PartyHealPercent && !pChar->SafeZone)
				{
					int iSkillType = GetSkillBuffParty(1);
					if (iSkillType == -1) break;
					Hero->CurrentSkill = gSkillManager.FindHeroCurrentSkill((ActionSkillType)iSkillType);
					SelectedCharacter = Party[i].index;
					gCB_MUHelper->SimulateSkill(iSkillType, true, pChar->Key);
					gCB_MUHelper->CacheAction[CB_MUHelper::eBuffPartyHeal]++;
					return 0;

				}
			}
		}

	}
	if (gCB_MUHelper->CacheAction[CB_MUHelper::eBuff] > 0) gCB_MUHelper->CacheAction[CB_MUHelper::eBuff]--;
	if (gCB_MUHelper->CacheAction[CB_MUHelper::eBuffParty] > 0) gCB_MUHelper->CacheAction[CB_MUHelper::eBuffParty]--;
	if (gCB_MUHelper->CacheAction[CB_MUHelper::eBuffPartyHeal] > 0) gCB_MUHelper->CacheAction[CB_MUHelper::eBuffPartyHeal]--;
	return 1;
}

extern int HeroIndex;

// Debuff skills (Summoner): allowed in the Activation slots, where their
// "Con" box means Instant Cast - see MuHelperActiveAttack.
bool IsHelperDebuffSkill(int skill)
{
	return skill == AT_SKILL_ALICE_SLEEP || skill == AT_SKILL_ALICE_BLIND
		|| skill == AT_SKILL_ALICE_WEAKNESS || skill == AT_SKILL_ALICE_ENERVATION
		|| (skill >= AT_SKILL_ALICE_SLEEP_UP && skill <= AT_SKILL_ALICE_SLEEP_UP + 4)
		|| skill == MASTER_SKILL_ADD_SLEEP_IMPROVED
		|| skill == MASTER_SKILL_ADD_BLIND || skill == MASTER_SKILL_ADD_BLIND_IMPROVED;
}

// Does the monster already carry the debuff this skill puts on?
static bool HelperTargetHasDebuff(CHARACTER* c, int skill)
{
	OBJECT* o = &c->Object;
	if (skill == AT_SKILL_ALICE_SLEEP || (skill >= AT_SKILL_ALICE_SLEEP_UP && skill <= AT_SKILL_ALICE_SLEEP_UP + 4) || skill == MASTER_SKILL_ADD_SLEEP_IMPROVED)
		return g_isCharacterBuff(o, eDeBuff_Sleep);
	if (skill == AT_SKILL_ALICE_BLIND || skill == MASTER_SKILL_ADD_BLIND || skill == MASTER_SKILL_ADD_BLIND_IMPROVED)
		return g_isCharacterBuff(o, eDeBuff_Blind);
	if (skill == AT_SKILL_ALICE_WEAKNESS)
		return g_isCharacterBuff(o, eDeBuff_AttackDown);
	if (skill == AT_SKILL_ALICE_ENERVATION)
		return g_isCharacterBuff(o, eDeBuff_DefenseDown);
	return false;
}

// A monster whose last attack was at the hero.
static bool IsMonsterAttackingHero(CHARACTER* c)
{
	return c->TargetCharacter == HeroIndex;
}

static bool IsLiveHelperMonster(CHARACTER* c)
{
	OBJECT* o = &c->Object;
	return KIND_MONSTER == o->Kind && o->Live && o->Visible && o->Alpha > 0.f && o->CurrentAction != 6 && !c->Dead;
}

// Activation skill condition for slot SkillNum (1 or 2): -1 = not now,
// otherwise a priority (higher wins when both are ready).
int ActiveCheckMonterCount(int SkillNum)
{
	auto& d = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu;
	const int skill = d.Skill[SkillNum];

	if (gSkillManager.FindHeroSkill((ActionSkillType)skill) == 0)
	{
		return -1;
	}

	if (d.SkillDelay[SkillNum - 1] == 1)
	{
		if ((GetTickCount() - d.TimerDelay[SkillNum]) >= (DWORD)(d.Delay[SkillNum - 1]) * 1000) //s
		{
			return 1;
		}
		return -1;
	}

	if (d.SkillCondition[SkillNum - 1] == 1)
	{
		// For a debuff the box means Instant Cast, handled before this.
		if (IsHelperDebuffSkill(skill))
		{
			return -1;
		}

		// "Monster Within Hunting range" (PreCon 0) or "Monster Attacking Me" (1).
		const bool attackingMeOnly = (d.SkillPreCon[SkillNum - 1] == 1);
		const float SkillDistance = gSkillManager.GetSkillDistance(gSkillManager.FindHeroCurrentSkill((ActionSkillType)skill), Hero);
		int HostileEnemyNum = 0;

		for (int i = 0; i < MAX_CHARACTERS_CLIENT; i++)
		{
			CHARACTER* c = &CharactersClient[i];
			if (!IsLiveHelperMonster(c))
			{
				continue;
			}
			if (attackingMeOnly && !IsMonsterAttackingHero(c))
			{
				continue;
			}

			const float Distance = GetDistance(c);
			const float limit = d.LongDistance ? (float)(d.Range[0] + 1) : SkillDistance;
			if (Distance < limit)
			{
				HostileEnemyNum++;
			}
		}

		// The boxes read "More Than 2 / 3 / 4 / 5 Mobs" (they used to need 2/4/6/8).
		static const int kNeed[4] = { 2, 3, 4, 5 };
		const int sub = d.SkillSubCon[SkillNum - 1];
		if (sub < 0 || sub > 3)
		{
			return -1;
		}
		return (HostileEnemyNum >= kNeed[sub]) ? sub : -1;
	}

	return -1;
}

void CB_MUHelper::GetTarget()
{
	auto& d = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu;

	for (int i = 0; i < MAX_CHARACTERS_CLIENT; i++)
	{
		CHARACTER* c = &CharactersClient[i];
		OBJECT* o = &c->Object;
		if (KIND_MONSTER != o->Kind || !o->Live)
		{
			continue;
		}

		if (IsHelperUnreachable(c->Key))
		{
			continue;
		}

		// A wall between the hero and the monster (it is on the other side of
		// it): not a target - it cannot be hit and the helper would stand there.
		if (!CheckWall(Hero->PositionX, Hero->PositionY, c->PositionX, c->PositionY))
		{
			continue;
		}

		// Long Distance Attacks: a monster hitting the hero is fought back
		// whatever the Range.
		if (d.LongDistance && IsMonsterAttackingHero(c) && GetDistance(c) <= 12.0f)
		{
			this->AddTarget(c->Key, false);
			continue;
		}

		const float DistanceTarget = !d.LongDistance
			? GetDistanceXY(c->PositionX, c->PositionY, d.StartX, d.StartY)
			: GetDistanceXY(c->PositionX, c->PositionY, Hero->PositionX, Hero->PositionY);

		if (DistanceTarget > d.Range[0] + 1)
		{
			continue;
		}
		this->AddTarget(c->Key, false);
	}
}

int CB_MUHelper::GetMonter()
{
	auto& d = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu;
	int Target = -1;

	gCB_MUHelper->CheckLiveTarget();

	if (m_SelectedCharacter != -1)
	{
		return m_SelectedCharacter;
	}

	GetTarget();

	if (!m_setTargets.empty())
	{
		CleanupTargets();

		std::set<int> setTargets;
		{
			_targetsLock.lock();
			setTargets = m_setTargets;
			_targetsLock.unlock();
		}

		int bestDistance = 1000;
		int bestGroup = -1;

		for (const int& iMonsterId : setTargets)
		{
			int iIndex = FindCharacterIndex(iMonsterId);
			if (iIndex < 0 || iIndex >= MAX_CHARACTERS_CLIENT)
			{
				continue;
			}
			CHARACTER* pTarget = &CharactersClient[iIndex];
			const int iDistance = GetDistance(pTarget);

			// Target Concentration: the monster with the most others around it
			// (within 2 tiles); otherwise the closest.
			int group = 0;
			if (d.TargetConcentration)
			{
				for (const int& otherId : setTargets)
				{
					const int o = FindCharacterIndex(otherId);
					if (o < 0 || o >= MAX_CHARACTERS_CLIENT || o == iIndex)
					{
						continue;
					}
					if (GetDistanceXY(pTarget->PositionX, pTarget->PositionY, CharactersClient[o].PositionX, CharactersClient[o].PositionY) <= 2)
					{
						group++;
					}
				}
			}

			const bool better = d.TargetConcentration
				? (group > bestGroup || (group == bestGroup && iDistance < bestDistance))
				: (iDistance < bestDistance);

			if (better)
			{
				bestDistance = iDistance;
				bestGroup = group;
				Target = iIndex;
			}
		}

		if (Target != -1)
		{
			m_SelectedCharacter = Target;
			m_DistanceSelectCharacter = (float)(bestDistance + 1);
		}
	}

	return Target;
}

// Use Regular Attack: a plain weapon hit on the target (the helper's lowest
// priority, when no skill can be used). Same moves as the phone's normal
// attack button (TriggerVirtualNormalAutoAttack).
extern int Attacking;
extern int ActionTarget;
extern int TargetX, TargetY;
static bool HelperRegularAttack(int target)
{
	if (target < 0 || target >= MAX_CHARACTERS_CLIENT)
	{
		return false;
	}
	CHARACTER* t = &CharactersClient[target];
	if (t->Dead > 0 || !t->Object.Live)
	{
		return false;
	}

	SelectedCharacter = target;
	CHARACTER* c = Hero;
	OBJECT* o = &c->Object;

	Attacking = 1;
	c->MovementType = MOVEMENT_ATTACK;
	ActionTarget = target;
	TargetX = (int)(t->Object.Position[0] / TERRAIN_SCALE);
	TargetY = (int)(t->Object.Position[1] / TERRAIN_SCALE);

	if (!CheckWall(c->PositionX, c->PositionY, TargetX, TargetY))
	{
		return false;
	}

	const bool ranged = (gCharacterManager.GetEquipedBowType() != BOWTYPE_NONE);
	if (!PathFinding2(c->PositionX, c->PositionY, TargetX, TargetY, &c->Path))
	{
		if (ranged && !CheckArrow())
		{
			return false;
		}
		Action(c, o, true);
		return true;
	}

	if (ranged)
	{
		if (!CheckArrow())
		{
			return false;
		}
		Action(c, o, true);
	}
	else
	{
		SendMove(c, o);
	}
	return true;
}

// Can this skill be cast right now (owned, and enough mana)?
static bool HelperSkillUsable(int skill)
{
	if (skill <= 0 || gSkillManager.FindHeroSkill((ActionSkillType)skill) == 0)
	{
		return false;
	}
	int mana = 0;
	gSkillManager.GetSkillInformation(skill, 1, NULL, &mana, NULL);
	return CharacterAttribute->Mana >= mana;
}

// Combo step bookkeeping (MuHelperActiveAttack).
static DWORD s_LastComboStepTick = 0;
// A combo step asked for but not on the wire yet. Some skills (Twisting Slash
// and the other "continue" skills) are queued by ExecuteSkill and sent a frame
// or more later, so the request counter only moves after the helper's call
// returned. Without this the helper kept re-asking the same step - Twisting
// over and over - and never reached Activation Skill 2.
static int s_ComboPendingSlot = -1;
// Last helper cast that really went out (speed-based attack rate).
static DWORD s_LastHelperCastTick = 0;
static DWORD s_ComboPendingSeq = 0;
static DWORD s_ComboPendingTick = 0;

bool CB_MUHelper::MuHelperActiveAttack()
{
	int SelectSkillNum = 0;
	int GetCount1 = ActiveCheckMonterCount(1);
	int GetCount2 = ActiveCheckMonterCount(2);

	//===Combo
	if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Combo == 1)
	{
		auto& pendData = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu;
		if (s_ComboPendingSlot >= 0)
		{
			if (g_SkillRequestSendSeq != s_ComboPendingSeq)
			{
				// The queued cast went out since the last call: that step is done.
				s_LastHelperCastTick = GetTickCount();
				pendData.TimerDelay[s_ComboPendingSlot] = GetTickCount();
				pendData.SkillCache = (s_ComboPendingSlot >= 2 ? 0 : s_ComboPendingSlot + 1);
				s_LastComboStepTick = GetTickCount();
				s_ComboPendingSlot = -1;
			}
			else if ((GetTickCount() - s_ComboPendingTick) > 1200) // Twisting Slash can go out ~1 s later (after the previous swing); 350 ms broke combo
			{
				// Never went out (no mana, out of range...): ask again.
				s_ComboPendingSlot = -1;
			}
			else
			{
				// Still waiting for it - don't queue the same step again.
				return 1;
			}
		}
		// Cycles Basic -> Skill 1 -> Skill 2, moving on only after a cast. A slot
		// with no usable skill (left empty, or a skill the hero no longer has)
		// never casts, so the cycle used to stop on it for good - the helper
		// "stuck" with combo on. Step past such slots instead.
		auto& data = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu;
		if (data.SkillCache < 0 || data.SkillCache > 2)
		{
			data.SkillCache = 0;
		}
		for (int tries = 0; tries < 3; ++tries)
		{
			if (gSkillManager.FindHeroSkill((ActionSkillType)data.Skill[data.SkillCache]) == 1)
			{
				break;
			}
			data.SkillCache = (data.SkillCache >= 2 ? 0 : data.SkillCache + 1);
		}
		SelectSkillNum = data.SkillCache;
	}
	else
	{
		auto& d = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu;
		if (GetCount1 != -1 || GetCount2 != -1)
		{
			SelectSkillNum = (GetCount2 > GetCount1 ? 2 : 1);
		}
		else if (d.ShortDistance)
		{
			// Use Skills Closely: activation skills without a Delay/Con
			// condition are used too, in turn with the Basic Skill, instead
			// of waiting for a condition that will never be set.
			static int s_CloseTurn = 0;
			for (int tries = 0; tries < 3; ++tries)
			{
				s_CloseTurn = (s_CloseTurn + 1) % 3;
				const int slot = s_CloseTurn;
				if (slot == 0)
				{
					SelectSkillNum = 0;
					break;
				}
				if (d.SkillDelay[slot - 1] == 0 && d.SkillCondition[slot - 1] == 0
					&& !IsHelperDebuffSkill(d.Skill[slot]) && HelperSkillUsable(d.Skill[slot]))
				{
					SelectSkillNum = slot;
					break;
				}
			}
		}
	}

	// Debuff with Instant Cast (its "Con" box): highest priority, put on the
	// target as soon as it lacks it - before combo and every other choice.
	{
		auto& d = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu;
		for (int slot = 1; slot <= 2; ++slot)
		{
			const int skill = d.Skill[slot];
			if (d.SkillCondition[slot - 1] != 1 || !IsHelperDebuffSkill(skill) || !HelperSkillUsable(skill))
			{
				continue;
			}
			const int target = GetMonter();
			if (target != -1 && !HelperTargetHasDebuff(&CharactersClient[target], skill))
			{
				SelectSkillNum = slot;
				break;
			}
		}
	}
SetAttackSkill:
	//DrawMessage(1, "%d ~ %d (%d) Check %d", GetCount1, GetCount2, SelectSkillNum, gSkillManager.FindHeroSkill((ActionSkillType)g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Skill[SelectSkillNum]));
	if (SelectSkillNum == -1 || gSkillManager.FindHeroSkill((ActionSkillType)g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Skill[SelectSkillNum]) == 0) SelectSkillNum = 0;

	// Use Regular Attack: no skill can be used (none set, not learned, or not
	// enough mana) - hit with the weapon instead; the lowest priority.
	if (!HelperSkillUsable(g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Skill[SelectSkillNum]))
	{
		// Out of mana for the skill (it is learned): drink a mana potion and go
		// on - the helper used to stand still until mana came back on its own,
		// which also broke every combo. The AFK helper already does this.
		{
			const int sk = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Skill[SelectSkillNum];
			static DWORD s_LastManaPotionTick = 0;
			if (sk > 0 && gSkillManager.FindHeroSkill((ActionSkillType)sk) == 1 && (GetTickCount() - s_LastManaPotionTick) > 900)
			{
				const int potion = g_pMyInventory->FindManaItemIndex();
				if (potion != -1)
				{
					SendRequestUse(potion, 0);
					s_LastManaPotionTick = GetTickCount();
				}
			}
		}
		if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.RegularAttack)
		{
			static DWORD s_LastRegularTick = 0;
			const int speed = (CharacterAttribute->AttackSpeed > CharacterAttribute->MagicSpeed)
				? CharacterAttribute->AttackSpeed : CharacterAttribute->MagicSpeed;
			const int cut = speed * 5;
			const DWORD interval = (cut >= 1200) ? 300 : (DWORD)(1500 - cut);
			if ((GetTickCount() - s_LastRegularTick) >= interval)
			{
				const int target = GetMonter();
				if (target != -1 && HelperRegularAttack(target))
				{
					s_LastRegularTick = GetTickCount();
				}
			}
		}
		return 1;
	}

	if (gSkillManager.FindHeroSkill((ActionSkillType)g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Skill[SelectSkillNum]) == 1)
	{
		SelectedCharacter = GetMonter();

		int iSkillType = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Skill[SelectSkillNum];
		//Hero->CurrentSkill = gSkillManager.FindHeroCurrentSkill((ActionSkillType)iSkillType);
		//g_MovementSkill.m_iSkill = gSkillManager.FindHeroCurrentSkill((ActionSkillType)iSkillType);
		//g_MovementSkill.m_bMagic = true;
		if (SelectedCharacter != -1)
		{
			auto& data = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu;

			// The server's combo (ComboSkill.cpp) is starter -> finisher ->
			// different finisher, each within 3 s of the last. Past ~2.5 s the
			// half-built chain can't finish any more, so start again from the
			// starter (Basic Skill) rather than casting a finisher that only
			// resets it.
			if (data.Combo == 1 && data.SkillCache != 0 && (GetTickCount() - s_LastComboStepTick) > 2500)
			{
				// Restart from the starter - and cast the starter NOW (this used to
				// reset the step but still cast the finisher it had picked).
				data.SkillCache = 0;
				SelectSkillNum = 0;
				iSkillType = data.Skill[0];
				if (!HelperSkillUsable(iSkillType))
				{
					return 1;
				}
			}

			// Attack rate from the character's own speed - the same rule as the
			// AFK off-helper (OfflineMode::RenderAttack): 1500 ms - 5 x speed
			// (the faster of attack and magic speed), never under 300 ms, the
			// fastest SendRequestMagic lets a cast out anyway.
			{
				const int speed = (CharacterAttribute->AttackSpeed > CharacterAttribute->MagicSpeed)
					? CharacterAttribute->AttackSpeed : CharacterAttribute->MagicSpeed;
				const int cut = speed * 5;
				const DWORD interval = (cut >= 1200) ? 300 : (DWORD)(1500 - cut);
				// Not enforced any more: the client already paces casts by attack
				// speed (the next skill waits for the current swing, whose length
				// comes from attack speed). On top of that this formula added a
				// ~1.2 s pause per skill for a normal BK. Kept for reference.
				(void)interval;
			}

			//Attacking = 2;
			// ExecuteSkill's return value describes the previous cast; a request
			// on the wire is the only honest "it went out" (g_SkillRequestSendSeq).
			const DWORD sentBefore = g_SkillRequestSendSeq;
			gCB_MUHelper->SimulateSkill(iSkillType, true, CharactersClient[SelectedCharacter].Key);
			const bool sent = (g_SkillRequestSendSeq != sentBefore);
			//MouseRButtonPush = 1;
			// data.Stage++ removed: ZzzInterface re-cast Hero->CurrentSkill once per
			// Stage point, so every helper cast went out 2-4 times and the next
			// skill only came ~2 s later. The helper casts directly now.

			if (sent)
			{
				s_LastHelperCastTick = GetTickCount();
				data.TimerDelay[SelectSkillNum] = GetTickCount();

				// Next slot only when this cast really went out. Moving on after a
				// cast that never left (still in the last swing, no mana, walking
				// into range) put the chain out of order, so the server kept
				// resetting it and combo never fired.
				data.SkillCache = (data.SkillCache >= 2 ? 0 : data.SkillCache + 1);
				s_LastComboStepTick = GetTickCount();
			}
			// Not waited for any more: late sends came from the removed Stage
			// re-cast, so a refused cast (swing not finished) just retries on the
			// next helper tick (0.1 s) instead of idling 1.2 s.
			else if (false && data.Combo == 1 && CheckKQ == 6)
			{
				// ExecuteSkill ran but nothing is on the wire yet: it may be a
				// queued skill. Check on the next call before moving or retrying.
				s_ComboPendingSlot = SelectSkillNum;
				s_ComboPendingSeq = g_SkillRequestSendSeq;
				s_ComboPendingTick = GetTickCount();
			}
			//DrawMessage(1, "Attack Skill %d -> Monter %d", g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Skill[SelectSkillNum], SelectedCharacter);
		}

	}
	return 1;
}
bool CB_MUHelper::MuHelperActiveHeal()
{
	BYTE RateHP = (BYTE)((CharacterAttribute->PrintPlayer.ViewCurHP * 100) / (CharacterAttribute->PrintPlayer.ViewMaxHP));
	//==SKill Heal  , hut mau
	if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.AutoHeal && gCB_MUHelper->CacheAction[CB_MUHelper::eHeal] < 25)
	{
		//DrawMessage(1, "Heal HP %d / %d", RateHP, g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.HealPercent);
		if (RateHP <= g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.HealPercent)
		{
			if (gCharacterManager.GetBaseClass(Hero->Class) == CNewUIBCustomMenuInfo::Fairy_Elf)
			{
				int iSkillType = GetSkillBuffParty(1); //lay skill hoi mau
				if (iSkillType != -1)
				{
					g_MovementSkill.m_iSkill = gSkillManager.FindHeroCurrentSkill((ActionSkillType)iSkillType);
					g_MovementSkill.m_bMagic = true;
					Hero->CurrentSkill = gSkillManager.FindHeroCurrentSkill((ActionSkillType)iSkillType);
					SelectedCharacter = FindCharacterIndex(Hero->Key);
					g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Stage++;
					gCB_MUHelper->CacheAction[CB_MUHelper::eHeal]++;
					return 0;
				}
			}
			else 	if (gCharacterManager.GetBaseClass(Hero->Class) == CNewUIBCustomMenuInfo::Summoner)
			{
				int iSkillType = GetSkillBuffParty(5); //lay skill hut maiu
				if (iSkillType != -1)
				{
					g_MovementSkill.m_iSkill = gSkillManager.FindHeroCurrentSkill((ActionSkillType)iSkillType);
					g_MovementSkill.m_bMagic = true;
					Hero->CurrentSkill = gSkillManager.FindHeroCurrentSkill((ActionSkillType)iSkillType);
					SelectedCharacter = GetMonter();
					g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Stage++;
					gCB_MUHelper->CacheAction[CB_MUHelper::eHeal]++;
					return 0;
				}
			}
		}
	}
	//==Dung thuoc hoi mau
	if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.AutoPotion)
	{
		if (RateHP <= g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PotPercent)
		{
			int Index = g_pMyInventory->FindHPItemIndex();

			if (Index != -1)
			{
				SendRequestUse(Index, 0);
			}
		}
	}
	if (gCB_MUHelper->CacheAction[CB_MUHelper::eHeal] > 0) gCB_MUHelper->CacheAction[CB_MUHelper::eHeal]--;
	return 1;
}
bool CompareStringsIgnoringSpaces(const std::string & strA, const std::string & strB)
{

	size_t lenA = strA.size();
	size_t lenB = strB.size();

	if (lenA < lenB)
	{
		//	std::cout << "CompareStringsIgnoringSpaces 1" << std::endl;
		return false;
	}
	int j = 0;
	for (int i = 0; i < lenB; ++i)
	{
		if (tolower(strA[i]) == ' ' || tolower(strB[i]) == ' ')
		{
			continue;
		}
		if (tolower(strA[i]) != tolower(strB[i]))
		{
			std::string charAsStringA(1, strA[i]);
			std::string charAsStringB(1, strB[i]);
			//DrawMessage(4, "Khac (%d) %s ~ %s", i, charAsStringA.c_str(), charAsStringB.c_str());
			//std::cout << "CompareStringsIgnoringSpaces 2 " << charAsStringA << " ~ " << charAsStringB << std::endl;
			return false;
		}

	}

	return true;
}
bool FindItemName(const std::string & strA, const std::string & strB)
{
	for (int i = 0; i < strA.size(); ++i)
	{
		if (tolower(strA[i]) == tolower(strB[0]))
		{

			std::string remainingString = strA.substr(i);

			if (remainingString.size() >= strB.size())
			{
				if (CompareStringsIgnoringSpaces(remainingString, strB))
				{
					return true;
				}
			}
		}
	}
	return false;
}

static std::vector<std::string> accentChars =
{
};
void CheckSyntax(char* text) // OK
{
	for (int n = 0; n < accentChars.size(); n++)
	{
		char* temp = text;

		while (true)
		{
			temp = strstr(temp, accentChars[n].c_str());

			if (temp == 0)
			{
				break;
			}

			int len = strlen(accentChars[n].c_str());

			memset(temp, 0x20, len);

			temp += len;
		}
	}
}

// Add Extra Item list: each entry is looked for, CASE-SENSITIVE, in a line
// made of the item name plus its level and options, e.g.
// "Leather Boots +5 Option Luck Skill". So "+5" picks +5 items, "Option" items
// with a Jewel of Life option, "Boots" any boots, and "leather" no Leather Helm
// (it used to ignore case and spaces, and only searched the bare name).
bool PickUpItemName(ITEM * pItem)
{
	unicode::t_char szItemName[128];
	const int level = (pItem->Level >> 3) & 15;
	GetItemName(pItem->Type, level, szItemName);

	std::string text = szItemName;
	char extra[16];
	if (level > 0)
	{
		sprintf_s(extra, " +%d", level);
		text += extra;
	}
	if ((pItem->Level & 3) != 0 || (pItem->Option1 & 64) != 0) text += " Option";
	if (((pItem->Level >> 2) & 1) != 0) text += " Luck";
	if (((pItem->Level >> 7) & 1) != 0) text += " Skill";
	if ((pItem->Option1 & 63) != 0) text += " Excellent";
	if (pItem->ExtOption) text += " Ancient";

	std::string lowText = text;
	for (size_t k = 0; k < lowText.size(); k++) lowText[k] = (char)tolower((unsigned char)lowText[k]);
	for (int n = 0; n < MAX_HELPER_ITEM; n++)
	{
		const char* entry = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.ItemList[n];
		while (*entry == ' ') entry++; // entries saved before the add-box fix carry a leading blank
		if (entry[0] == 0) continue;
		std::string lowEntry = entry;
		for (size_t k = 0; k < lowEntry.size(); k++) lowEntry[k] = (char)tolower((unsigned char)lowEntry[k]);
		if (lowText.find(lowEntry) != std::string::npos)
		{
			return 1;
		}
	}

	return 0;
}

int BComputeDistanceBetween(POINT posA, POINT posB)
{
	int iDx = posA.x - posB.x;
	int iDy = posA.y - posB.y;

	return static_cast<int>(std::ceil(std::sqrt(iDx * iDx + iDy * iDy)));
}
int BomputeDistanceByRange(int iRange)
{
	return BComputeDistanceBetween({ 0, 0 }, { iRange, iRange });
}
struct ItemDistance
{
	int index;     // Ch峄?s峄?c峄 item
	float distance;  // Kho岷g c谩ch t峄沬 Hero
};
void GetListItemPickUp()
{
	for (int i = 0; i < MAX_ITEMS; i++)
	{
		OBJECT* o = &Items[i].Object;
		if (o->Live && o->Visible)
		{
			int dx = (int)(o->Position[0] / TERRAIN_SCALE);
			int dy = (int)(o->Position[1] / TERRAIN_SCALE);
			float iDistance = 15;

			if (!g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.LongDistance)
			{
				iDistance = GetDistanceXY(g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.StartX, g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.StartY, dx, dy);
			}
			else
			{
				iDistance = GetDistanceXY(Hero->PositionX, Hero->PositionY, dx, dy);
			}


			if ((int)iDistance > g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Range[1] + 1) continue;
			//gInterface.DrawMessage(1, "add item %d", Items[i].Item.Type);
			gCB_MUHelper->AddItem(i);
		}
	}
}
bool CB_MUHelper::ShouldObtainItem(int iItemId)
{
	ITEM_t* pDrop = &Items[iItemId];
	ITEM* pItem = &pDrop->Item;

	auto& d = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu;

	// Pick All Near Items: everything.
	if (d.PickAllItem)
	{
		return true;
	}

	// Pick Selected Items: only what is ticked in the list (it used to pick
	// the ticked kinds even with Pick Selected switched off).
	if (!d.PickSelected)
	{
		return false;
	}

	if ((pItem->Type == ITEM_POTION + 15 && d.PickMoney)
		|| (CheckItemIsJewel(pItem->Type) && d.PickJewel)
		|| (pItem->ExtOption && d.PickSet)
		|| ((pItem->Option1 & 63) != 0 && d.PickExc))
	{
		return true;
	}

	return d.PickExtra && PickUpItemName(pItem);
}
int CB_MUHelper::PickUpItemMuHelper()
{
	int iClosestItemId = -1;
	int iMinDistance = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Range[1] + 1;

	std::set<int> setItems;
	{
		_itemsLock.lock();
		setItems = m_setItems;
		_itemsLock.unlock();
	}

	for (const int& iItemId : setItems)
	{
		if (!ShouldObtainItem(iItemId) /*|| !gItemManager.CheckItemIsData(Items[iItemId].Item.Type)*/)
		{
			continue;
		}

		int iItemX = (int)(Items[iItemId].Object.Position[0] / TERRAIN_SCALE);
		int iItemY = (int)(Items[iItemId].Object.Position[1] / TERRAIN_SCALE);
		float iDistance = 15;
		if (!g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.LongDistance)
		{
			iDistance = GetDistanceXY(g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.StartX, g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.StartY, iItemX, iItemY);
		}
		else
		{
			iDistance = GetDistanceXY(Hero->PositionX, Hero->PositionY, iItemX, iItemY);
		}

		if (iDistance < iMinDistance)
		{
			iMinDistance = iDistance;
			iClosestItemId = iItemId;
		}
	}

	return iClosestItemId;
}
bool MuHelperRepairItem()
{
	//==sua do
	if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.RepairItem && GetTickCount() > g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.TimerRepair)
	{
		for (int i = 0; i < 12; i++)
		{
			ITEM* pItem = &CharacterMachine->Equipment[i];
			if (pItem == nullptr || pItem->Type == -1)
			{
				continue;
			}
			ITEM_ATTRIBUTE* p = &ItemAttribute[pItem->Type];
			int maxDurability = calcMaxDurability(pItem, p, pItem->Level);
			//DrawMessage(1, "Item %d Dur (%d/%d)", pItem->Type, pItem->Durability, maxDurability);
			if (maxDurability > 0 && pItem->Durability * 2 <= maxDurability) SendRequestRepair(i, 1); // repair at 50% durability, not at the first point lost
		}
		g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.TimerRepair = GetTickCount() + (2 * 60 * 1000);
	}
	return 1;
}
int CB_MUHelper::SimulateMove(POINT posMove)
{
	extern int TargetX, TargetY;

	Hero->MovementType = MOVEMENT_MOVE;
	TargetX = (int)posMove.x;
	TargetY = (int)posMove.y;

	if (!CheckTile(Hero, &Hero->Object, 1.5f))
	{
		if (PathFinding2((Hero->PositionX), (Hero->PositionY), TargetX, TargetY, &Hero->Path))
		{
			SendMove(Hero, &Hero->Object);
		}
		return 0;
	}

	return 1;
}
void CB_MUHelper::CheckLiveTarget()
{
	if (gCB_MUHelper->m_SelectedCharacter != -1 && gCB_MUHelper->m_SelectedCharacter <= 400)
	{
		CHARACTER* pTarget = &CharactersClient[gCB_MUHelper->m_SelectedCharacter];
		float fDistanceTarget = GetDistance(pTarget);
		if (pTarget->Dead || !pTarget->Object.Live || !pTarget->Object.Visible || fDistanceTarget > gCB_MUHelper->m_DistanceSelectCharacter || pTarget->Object.AnimationFrame == 6
			|| !CheckWall(Hero->PositionX, Hero->PositionY, pTarget->PositionX, pTarget->PositionY))
		{
			gCB_MUHelper->m_SelectedCharacter = -1;
			gCB_MUHelper->m_DistanceSelectCharacter = 1.f;
			SelectedCharacter = -1;
		}
	}
}


bool MUHelperRegenPos()
{
	gCB_MUHelper->CheckLiveTarget();

	//==Di chuyen ve vi tri cu
	float DistanceTarget = GetDistanceXY(Hero->PositionX, Hero->PositionY, g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.StartX, g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.StartY);

	//gInterface.DrawMessage(1, "Move Regen %f Target %d %f", DistanceTarget, gCB_MUHelper->m_SelectedCharacter, gCB_MUHelper->m_DistanceSelectCharacter);

	// Original Position: every MoveTime SECONDS go back to where the helper was
	// started. The timer only marks the return as due; the walk back waits
	// until the current target is dead (CheckLiveTarget above clears it), so a
	// fight is finished first and the helper does not drop a half-killed
	// monster. A target that never dies (out of reach, immune) gets 30 s
	// before the return goes ahead anyway. Busy spots still return, because a
	// pending return stops a new target being picked.
	{
		auto& d = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu;
		static bool s_Returning = false;
		static bool s_ReturnPending = false;
		static DWORD s_ReturnDue = 0;
		static DWORD s_ReturnStart = 0;

		if (d.OriginalPosition != 1)
		{
			s_ReturnPending = false;
		}

		if (d.OriginalPosition == 1 && d.MoveTime > 0 && GetTickCount() > d.TimerMove)
		{
			d.TimerMove = GetTickCount() + ((DWORD)d.MoveTime * 1000);
			if (DistanceTarget > 1 && s_Returning == false && s_ReturnPending == false)
			{
				s_ReturnPending = true;
				s_ReturnDue = GetTickCount();
			}
		}

		if (s_ReturnPending && (gCB_MUHelper->m_SelectedCharacter == -1 || (GetTickCount() - s_ReturnDue) > 30000))
		{
			s_ReturnPending = false;
			s_Returning = true;
			s_ReturnStart = GetTickCount();
		}

		if (s_Returning)
		{
			if (d.OriginalPosition != 1 || DistanceTarget <= 1 || (GetTickCount() - s_ReturnStart) > 10000)
			{
				s_Returning = false; // there, switched off, or could not get there in 10 s
			}
			else
			{
				// No progress for 1.5 s (blocked path, nowhere to walk): give the return up and fight,
				// instead of standing still next to monsters for the full 10 s.
				static int s_RetX = -1, s_RetY = -1; static DWORD s_RetSince = 0;
				if (Hero->PositionX != s_RetX || Hero->PositionY != s_RetY || s_RetSince == 0 || (GetTickCount() - s_ReturnStart) < 200)
				{
					s_RetX = Hero->PositionX; s_RetY = Hero->PositionY; s_RetSince = GetTickCount();
				}
				else if ((GetTickCount() - s_RetSince) > 1500)
				{
					s_Returning = false; s_RetSince = 0;
					d.TimerMove = GetTickCount() + ((DWORD)d.MoveTime * 1000);
					return 1;
				}
				gCB_MUHelper->m_SelectedCharacter = -1;
				SelectedCharacter = -1;
				POINT pos;
				pos.x = d.StartX;
				pos.y = d.StartY;
				gCB_MUHelper->SimulateMove(pos);
				return 0;
			}
		}
	}

	if (gCB_MUHelper->m_SelectedCharacter != -1) return 1;

	if (gCB_MUHelper->CacheAction[CB_MUHelper::eMoveRegen] <= 5)
	{
		//if ((g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.MoveTime > 1 && GetTickCount() > g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.TimerMove
		//	&& g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.OriginalPosition == 1) || ((DistanceTarget >= g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Range[0]+2) && !g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.LongDistance))

		if (DistanceTarget > 3 && !g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.LongDistance ||
			(DistanceTarget > g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Range[0] && GetTickCount() > g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.TimerMove && g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.OriginalPosition == 1))
		{
			g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.TimerMove = GetTickCount() + ((g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.MoveTime) * 1000);
			//bool bTargetNear = CheckTile(Hero, &Hero->Object, 1.5f);
			//bool bNoWall = CheckWall(Hero->PositionX, Hero->PositionY, TargetX, TargetY);
			//if (!bTargetNear || !bNoWall)
			//{
			//	if (PathFinding2(Hero->PositionX, Hero->PositionY, g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.StartX, g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.StartY, &Hero->Path))
			//	{
			//		Hero->MovementType = MOVEMENT_MOVE;
			//		Hero->Movement = true;
			//		SendMove(Hero, &Hero->Object);
			//		gCB_MUHelper->CacheAction[CB_MUHelper::eMoveRegen]++;
			//		gCB_MUHelper->m_ActionMoveRegen = GetTickCount() + 1000;
			//		return 0;
			//	}
			//}
			POINT  m_posOriginal;
			m_posOriginal.x = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.StartX;
			m_posOriginal.y = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.StartY;
			if (!gCB_MUHelper->SimulateMove(m_posOriginal))
			{
				return 0;
			}

		}
	}
	else
	{
		g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Started = false;
		return 1;
	}
	if (gCB_MUHelper->CacheAction[CB_MUHelper::eMoveRegen] > 0) gCB_MUHelper->CacheAction[CB_MUHelper::eMoveRegen]--;
	return 1;
}
bool CB_MUHelper::MUHelperNhatItem()
{
	if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem != -1)
	{
		OBJECT* o = &Items[g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem].Object;
		if (!o->Live || !o->Visible)
		{
			g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem = -1;
		}
	}


	if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem == -1)
	{
		this->m_setItems.clear();
		GetListItemPickUp();
	}
	g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem = PickUpItemMuHelper();
	//gInterface.DrawMessage(1, "Pick Item %d %d %d size %d", gCB_MUHelper->m_iCurrentItem, PickUpItemMuHelper(), g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem, this->m_setItems.size());

	if (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem >= 0 && g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem <= 1000 && gCB_MUHelper->CacheAction[CB_MUHelper::ePickUp] < 5)
	{
		OBJECT* o = &Items[g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem].Object;
		if (o->Live && o->Visible)
		{
			int dx = (int)(o->Position[0] / TERRAIN_SCALE);
			int dy = (int)(o->Position[1] / TERRAIN_SCALE);
			float ItemiDistance = GetDistanceXY(Hero->PositionX, Hero->PositionY, dx, dy);
			if (ItemiDistance < 2.5f)
			{
				SendRequestGetItem(g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem);
				DeleteItem(g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem);
				gCB_MUHelper->CacheAction[CB_MUHelper::ePickUp]++;
				return 0;
			}
			else
			{
				CHARACTER* c = Hero;
				OBJECT* o = &c->Object;
				c->MovementType = MOVEMENT_GET;
				ItemKey = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem;
				TargetX = (int)(Items[g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem].Object.Position[0] / TERRAIN_SCALE);
				TargetY = (int)(Items[g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem].Object.Position[1] / TERRAIN_SCALE);
				OBJECT * obiItem = &Items[g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem].Object;

				bool bFinding = PathFinding2((Hero->PositionX), (Hero->PositionY), TargetX, TargetY, &Hero->Path);
				bool bTargetNear = CheckTile(Hero, &Hero->Object, 1.5f);
				bool bNoWall = CheckWall(Hero->PositionX, Hero->PositionY, TargetX, TargetY);
				//PATH_t tempPath;

				//bool bHasPath = PathFinding2(Hero->PositionX, Hero->PositionY, TargetX, TargetY, &tempPath, fSkillDistance + 5);
				//bool bTargetNear = CheckTile(Hero, &Hero->Object, fSkillDistance);
				//bool bNoWall = CheckWall(Hero->PositionX, Hero->PositionY, TargetX, TargetY);
				//gInterface.DrawMessage(1, "Pick Item %d %d %d size %d", gCB_MUHelper->m_iCurrentItem, PickUpItemMuHelper(), g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem, this->m_setItems.size());
				//gInterface.DrawMessage(1, "bTargetNear %d bNoWall %d bFinding %d", bTargetNear, bNoWall, bFinding);
				if (!bTargetNear || !bNoWall)
				{
					if (bFinding)
					{
						SendMove(Hero, &Hero->Object);
						m_iCurrentItemMove = ItemKey;

					}
					//else
					//{
					//	SendRequestGetItem(g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem);
					//	DeleteItem(g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem);
					//}
					gCB_MUHelper->CacheAction[CB_MUHelper::ePickUp]++;
					return 0;
				}
				//else
				//{
				//	SendRequestGetItem(g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem);
				//	DeleteItem(g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem);
				//	gCB_MUHelper->CacheAction[CB_MUHelper::ePickUp]++;
				//	return 0;
				//}
			}
		}
	}
	if (gCB_MUHelper->CacheAction[CB_MUHelper::ePickUp] > 0) gCB_MUHelper->CacheAction[CB_MUHelper::ePickUp]--;
	m_iCurrentItemMove = -1;
	return 1;
}
void CB_MUHelper::CleanupTargets()
{
	std::set<int> setTargets;
	{
		_targetsLock.lock();
		setTargets = m_setTargets;
		_targetsLock.unlock();
	}

	for (const int& iMonsterId : setTargets)
	{
		int iIndex = FindCharacterIndex(iMonsterId);
		if (iIndex == MAX_CHARACTERS_CLIENT)
		{
			DeleteTarget(iMonsterId);
		}

		CHARACTER* pTarget = &CharactersClient[iIndex];
		if (!pTarget || (pTarget && (pTarget->Dead || !pTarget->Object.Live)))
		{
			DeleteTarget(iMonsterId);
		}
	}
}
void CB_MUHelper::CleanupItems()
{
	std::set<int> setItems;
	{
		_itemsLock.lock();
		setItems = m_setItems;
		_itemsLock.unlock();
	}

	for (const int& iItemId : setItems)
	{
		ITEM_t* pDrop = &Items[iItemId];
		ITEM* pItem = &pDrop->Item;

		if (!pDrop->Object.Live || !pDrop->Object.Visible || !ShouldObtainItem(iItemId))
		{
			DeleteTarget(iItemId);
		}
	}
}
bool CB_MUHelper::MUHelperAttack()
{
	//if (m_setTargets.empty())
	//{
	//	GetTarget();
	//}
	//gInterface.DrawMessage(1, "MUHelperAttack %d Size %d", SelectedCharacter, m_setTargets.size());
	if (SelectedCharacter == -1 || SelectedCharacter == FindCharacterIndex(Hero->Key))
	{
		SelectedCharacter = GetMonter();
	}

	if (SelectedCharacter != -1)
	{
		int Skill = CharacterAttribute->Skill[Hero->CurrentSkill];
		float DistanceSkill = gSkillManager.GetSkillDistance(Skill, Hero) - 0.3f;
		float DistanceTarget = GetDistanceXY(Hero->PositionX, Hero->PositionY, CharactersClient[SelectedCharacter].PositionX, CharactersClient[SelectedCharacter].PositionY);

		//if (DistanceSkill == 0) DistanceSkill = 1.2f;
		CHARACTER* c = &CharactersClient[SelectedCharacter];
		OBJECT* o = &c->Object;
		//float Distance = GetDistance(c);
		//gInterface.DrawMessage(1, "Get %d Dis %f [%f] <> %f (%d)", CharactersClient[SelectedCharacter].Key, DistanceTarget, Distance, DistanceSkill, (CheckWall((Hero->PositionX), (Hero->PositionY), c->PositionX, c->PositionY)));

		//if (!g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.LongDistance && GetDistanceXY(Hero->PositionX, Hero->PositionY, CharactersClient[SelectedCharacter].PositionX, CharactersClient[SelectedCharacter].PositionY) > 3)
		if (CheckWall((Hero->PositionX), (Hero->PositionY), c->PositionX, c->PositionY) == 0 || DistanceTarget > DistanceSkill)
		{
			if (PathFinding2(Hero->PositionX, Hero->PositionY, CharactersClient[SelectedCharacter].PositionX, CharactersClient[SelectedCharacter].PositionY, &Hero->Path))
			{
				Hero->MovementType = MOVEMENT_SKILL;

				Hero->Movement = true;
				SendMove(Hero, &Hero->Object);
				return 0;
			}
		}

		//if (o->CurrentAction == 6 || !o->Live || !o->Visible || c->Dead /*|| Distance > (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Range[0] + 1)*/
		//	|| !CheckWall((Hero->PositionX), (Hero->PositionY), c->PositionX, c->PositionY)
		//	|| CheckTarget(Hero)
		//	)

		//if (!o->Live || !o->Visible)
		//{
		//	SelectedCharacter = GetMonter();
		//	//gInterface.DrawMessage(1, "Get lai Monmter %d (%f)", SelectedCharacter, DistanceSkill);
		//	return 0;
		//}

		//==Attack
		//gInterface.DrawMessage(1, "Attack Monmter %d Item %d (%d) DisSkill %f", SelectedCharacter, ItemKey, g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SelectedItem, DistanceSkill);
		if (!MuHelperActiveAttack()) return 0;

	}
	return 1;
}

void CB_MUHelper::Work()
{
	try
	{
		if (!g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Started)
		{
			if (ClearDateOn) CB_MUHelper::ClearData();
			return;
		}

		//gInterface.DrawMessage(1, "CB_MUHelper::Start() [%d/%d] %d",Hero->PositionX,Hero->PositionY, Hero->MovementType);

		if (LogOut)
		{
			return;
		}
		// Stuck-walk guard. The helper waits while Movement is set, so a walk
		// that never finishes (a knockback or forced move landing mid-path)
		// parked it for good. Moving but on the same tile for 3 s: drop it.
		{
			static int s_StuckX = -1, s_StuckY = -1;
			static DWORD s_StuckSince = 0;

			if (Hero->Movement && Hero->JumpTime == 0)
			{
				if (Hero->PositionX != s_StuckX || Hero->PositionY != s_StuckY)
				{
					s_StuckX = Hero->PositionX;
					s_StuckY = Hero->PositionY;
					s_StuckSince = GetTickCount();
				}
				else if ((GetTickCount() - s_StuckSince) > 3000)
				{
					Hero->Movement = false;
					Hero->Path.PathNum = 0;
					Hero->Path.CurrentPath = 0;
					Hero->Path.CurrentPathFloat = 0;
					SetPlayerStop(Hero);
					s_StuckX = -1;
				}
			}
			else
			{
				s_StuckX = -1;
			}
		}

		if (Hero->Movement || Hero->Dead || g_isCharacterBuff((&Hero->Object), eDeBuff_Stun) /*||| Hero->Appear | g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Stage > 1*/)
		{
			return;
		}

		if (gInterface.Data[eStatus_KepChuot].OnShow)
		{
			g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Started = false;
			return;
		}
		//MouseOnWindow = true;

		//gInterface.DrawMessage(1, "End : %d ,%d ,%d ,%d, %d, %d SizeAttack(%d) [%d] %f",
		//	CacheAction[0],
		//	CacheAction[1],
		//	CacheAction[2],
		//	CacheAction[3],
		//	CacheAction[4],
		//	CacheAction[5],
		//	m_setTargets.size(),
		//	m_SelectedCharacter,
		//	m_DistanceSelectCharacter
		//);

		if (!MUHelperRegenPos())
		{
			//gInterface.DrawMessage(1, "MUHelperRegenPos()");
			CB_MUHelper::ClearData();
			return;
		}

		if (gCB_MUHelper->m_ActionMoveRegen > GetTickCount())
		{
			//gInterface.DrawMessage(1, "MUHelperRegenPos()");
			return;
		}

		//==BUff & Buff Nhom
		if (!MuHelperActiveBuff())
		{
			//gInterface.DrawMessage(1, "MuHelperActiveBuff()");
			return;
		}
		//==Hoi mau , hut mau
		if (!MuHelperActiveHeal())
		{
			//gInterface.DrawMessage(1, "MuHelperActiveHeal()");
			return;
		}

		if (!MUHelperNhatItem())
		{
			//gInterface.DrawMessage(1, "MUHelperNhatItem()");
			return;
		}

		if (m_iCurrentItemMove == -1)
		{
			//gInterface.DrawMessage(1, "Attack %d / %d ", m_iCurrentTarget, m_setTargets.size());
			MUHelperAttack();
			//gInterface.DrawMessage(1, "MUHelperAttack() %d", m_setTargets.size());
		}


		for (int i = 0; i < eMaxVaule; i++)
		{
			if (CacheAction[i] > 0) CacheAction[i]--;
		}
		//==sua do
		MuHelperRepairItem();
		ClearDateOn = true;
	}
	catch (...)
	{
		g_ConsoleDebug->Write(MCD_NORMAL, "[MU Helper] Exception occurred. Ignoring...");
	}
}
void CB_MUHelper::SetCircle(float Range)
{
	TimeDrawCircle = GetTickCount() + 1200;
	RangeDraw = Range;
}
void CB_MUHelper::RenderCircle(CHARACTER * c)
{

}