// Attack.h: interface for the CAttack class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Protocol.h"

//**********************************************//
//************ Client -> GameServer ************//
//**********************************************//

struct PMSG_ATTACK_RECV
{
	PBMSG_HEAD header; // C1:[PROTOCOL_CODE2]
	BYTE index[2];
	BYTE action;
	BYTE dir;
};

//**********************************************//
//**********************************************//
//**********************************************//

class CAttack
{
public:
	CAttack();
	virtual ~CAttack();
	bool Attack(LPOBJ lpObj, LPOBJ lpTarget, CSkill* lpSkill, bool send, BYTE flag, __int64 damage, int count, bool combo);
	bool AttackElemental(LPOBJ lpObj,LPOBJ lpTarget,CSkill* lpSkill,bool send,BYTE flag,int damage,int count,bool combo);
	bool DecreaseArrow(LPOBJ lpObj);
	void WingSprite(LPOBJ lpObj, LPOBJ lpTarget, __int64* damage);
	void HelperSprite(LPOBJ lpObj, LPOBJ lpTarget, __int64* damage);
	void DamageSprite(LPOBJ lpObj, __int64 damage);
	bool DarkHorseSprite(LPOBJ lpObj, __int64 damage);
	bool FenrirSprite(LPOBJ lpObj, __int64 damage);
	bool MuunPet(LPOBJ lpObj, __int64 damage); // OK
	
	void WeaponDurabilityDown(LPOBJ lpObj,LPOBJ lpTarget);
	void ArmorDurabilityDown(LPOBJ lpObj,LPOBJ lpTarget);
	bool CheckPlayerTarget(LPOBJ lpObj,LPOBJ lpTarget);

	// Advances lpObj's PvP damage ramp against lpTarget and returns the bonus
	// as a percentage to add on top of 100. Safe to call more than once for one
	// blow - the bonus comes from elapsed time, not from a counter that this
	// bumps - which is what lets both the physical and the elemental damage
	// paths call it without double-counting.
	int GetPvPDamageRampPercent(LPOBJ lpObj,LPOBJ lpTarget);

	// Opens or refreshes the pairing clock for lpObj hitting lpTarget. One
	// clock serves both the damage ramp and the reflect decay, so it is
	// maintained whenever either is switched on.
	PVP_RAMP_SLOT* UpdatePvPPairClock(LPOBJ lpObj,LPOBJ lpTarget);

	// How much of its reflect damage a defender still gets back, as a
	// percentage: 100 at the start of a fight, falling to
	// m_PvPReflectMinPercent. Takes the reflect call's own argument order -
	// lpObj reflects, lpTarget receives.
	int GetPvPReflectPercent(LPOBJ lpObj,LPOBJ lpTarget);
	void MissSend(LPOBJ lpObj,LPOBJ lpTarget,CSkill* lpSkill,int send,int count);
	bool MissCheck(LPOBJ lpObj,LPOBJ lpTarget,CSkill* lpSkill,int send,int count,BYTE* miss);
	bool MissCheckPvP(LPOBJ lpObj,LPOBJ lpTarget,CSkill* lpSkill,int send,int count,BYTE* miss);
	bool MissCheckElemental(LPOBJ lpObj,LPOBJ lpTarget,CSkill* lpSkill,int send,int count,BYTE* miss);
	bool ApplySkillEffect(LPOBJ lpObj,LPOBJ lpTarget,CSkill* lpSkill,int damage);
	int GetTargetDefense(LPOBJ lpObj,LPOBJ lpTarget,WORD* effect);
	int GetTargetElementalDefense(LPOBJ lpObj,LPOBJ lpTarget,WORD* effect);
	int GetAttackDamage(LPOBJ lpObj,LPOBJ lpTarget,CSkill* lpSkill,WORD* effect,int TargetDefense);
	int GetAttackDamageWizard(LPOBJ lpObj,LPOBJ lpTarget,CSkill* lpSkill,WORD* effect,int TargetDefense);
	int GetAttackDamageCursed(LPOBJ lpObj,LPOBJ lpTarget,CSkill* lpSkill,WORD* effect,int TargetDefense);
	int GetAttackDamageFenrir(LPOBJ lpObj,LPOBJ lpTarget,CSkill* lpSkill,WORD* effect,int TargetDefense);
	int GetAttackDamageElemental(LPOBJ lpObj,LPOBJ lpTarget,CSkill* lpSkill,WORD* effect,int AttackDamage,int TargetDefense);
	int GetShieldDamage(LPOBJ lpObj, LPOBJ lpTarget, __int64 damage);
	__int64 GetReflectDamage(LPOBJ lpTarget, __int64 damage);
	int ApplyTargetDefense(LPOBJ lpObj, __int64 damage, int defense);
	void GetPreviewDefense(LPOBJ lpObj,DWORD* defense);
	void GetPreviewPhysiDamage(LPOBJ lpObj,DWORD* DamageMin,DWORD* DamageMax,DWORD* MulDamage,DWORD* DivDamage);
	void GetPreviewMagicDamage(LPOBJ lpObj,DWORD* DamageMin,DWORD* DamageMax,DWORD* MulDamage,DWORD* DivDamage,DWORD* DamageRate);
	void GetPreviewCurseDamage(LPOBJ lpObj,DWORD* DamageMin,DWORD* DamageMax,DWORD* MulDamage,DWORD* DivDamage,DWORD* DamageRate);
	void GetPreviewDamageMultiplier(LPOBJ lpObj,DWORD* DamageMultiplier,DWORD* RFDamageMultiplierA,DWORD* RFDamageMultiplierB,DWORD* RFDamageMultiplierC);
	void CGAttackRecv(PMSG_ATTACK_RECV* lpMsg,int aIndex);
};

extern CAttack gAttack;
