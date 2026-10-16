// MonsterScaling.h: monsters that match the player fighting them.
//
// A normal monster on a map listed in Data\Custom\MonsterScaling.ini takes on
// the level of the first player who hits it, clamped to that map's level band,
// and its HP, damage, defense and hit rates grow (or shrink) with it. Bosses
// never scale. The monster returns to its Monster.txt stats when it respawns.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "User.h"
#include "Map.h"
#include <set>

struct MONSTER_SCALING_BAND
{
	int MinLevel; // 0 = map not scaled
	int MaxLevel;
};

// The Monster.txt stats a scaled monster had, put back on respawn.
struct MONSTER_SCALING_BASE
{
	bool Scaled;
	int Level;
	float Life;
	float MaxLife;
	float ScriptMaxLife;
	int DamageMin;
	int DamageMax;
	int Defense;
	int MagicDefense;
	int AttackSuccessRate;
	int DefenseSuccessRate;
};

class CMonsterScaling
{
public:
	CMonsterScaling();
	void Load(char* path);

	// A player is about to hit a monster: scale it the first time.
	void OnPlayerAttack(LPOBJ lpObj,LPOBJ lpTarget);

	// The monster respawns: back to its Monster.txt stats.
	void Reset(LPOBJ lpObj);

	// The object slot gets a new monster: forget the old one's saved stats.
	void Clear(int aIndex);

private:
	bool IsExcluded(LPOBJ lpObj);

	bool m_Enable;
	int m_HpRate;        // % of the level change applied to HP
	int m_DamageRate;    // ...to damage
	int m_DefenseRate;   // ...to defense and magic defense
	int m_HitRate;       // ...to attack / defense success rates
	int m_ScaleRewards;  // 1 = monster level changes too (EXP, Zen and drop level follow)
	int m_BossLife;      // monsters with this much HP or more never scale (0 = off)
	int m_MinFactor;     // never weaker than this % of the original
	int m_MaxFactor;     // never stronger than this % of the original

	MONSTER_SCALING_BAND m_Band[MAX_MAP];
	std::set<int> m_Exclude;
	MONSTER_SCALING_BASE m_Base[MAX_OBJECT];
};

extern CMonsterScaling gMonsterScaling;
