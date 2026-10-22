#include "StdAfx.h"
#include "CB_OffMuHelper.h"
#include "NewUISystem.h"
#include "CBInterface.h"
#include "CUIController.h"
#include "CharacterManager.h"
#include "Util.h"
#include "Protocol.h"
#include "NewUIBase.h"
#include "Other.h"
#include "ZzzInterface.h"

#if(CB_OFFMUHELPER)
CB_OffMuHelper* gCB_OffMuHelper;

CB_OffMuHelper::CB_OffMuHelper()
{
	this->ActiveOfflineMode = false;
}


CB_OffMuHelper::~CB_OffMuHelper()
{
}
void CB_OffMuHelper::Render(float X, float Y)
{
	if (g_pBCustomMenuInfo->RenderCheckBox(X + 8, Y+5, 0xFFFFFFFF, this->ActiveOfflineMode == 1 ? TRUE : FALSE, "AFK Offline Mode"))
	{
		this->ActiveOfflineMode ^= 1;
	}
	g_pRenderText->SetTextColor(0xFF00B4FF);
	g_pRenderText->RenderText(X + 8, Y + 25, "*Activate Offline Mode", 0, 0, RT3_SORT_CENTER);
}
void CB_OffMuHelper::RunningOffHelper()
{

	if (!this->ActiveOfflineMode)
	{
		return;
	}

	gInterface.m_Disconnect = true;

	CG_OFFMODE_RESULT pRequest;
	pRequest.h.set((LPBYTE)& pRequest, 0xFB, 0x3D, sizeof(pRequest));
	//-- Range Attack
	pRequest.HuntingRange = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Range[0];
	//-- Auto Heling, Drain and Potion
	pRequest.RecoveryPotionOn = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.AutoPotion;
	pRequest.RecoveryPotionPercent = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PotPercent;
	pRequest.RecoveryHealOn = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.AutoHeal;
	pRequest.RecoveryHealPercent = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.HealPercent;
	pRequest.RecoveryDrainOn = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.AutoDrainLife;
	pRequest.RecoveryDrainPercent = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.DrainPercent;
	//-- Attack
	pRequest.DistanceLongOn = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.LongDistance;
	pRequest.DistanceReturnOn = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.OriginalPosition;
	pRequest.DistanceMin = (g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.MoveTime);
	pRequest.SkillBasicID = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Skill[0];
	pRequest.ComboOn = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Combo;
	//-- Party Settings
	pRequest.PartyModeOn = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Party;
	pRequest.PartyModeHealOn = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PartyAutoHeal;
	pRequest.PartyModeHealPercent = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PartyHealPercent;
	pRequest.PartyModeBuffOn = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PartyAutoBuff;
	//-- AutoBuffo
	pRequest.BuffOn = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.AutoBuff;
	pRequest.BuffSkill[0] = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Buff[0];
	pRequest.BuffSkill[1] = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Buff[1];
	pRequest.BuffSkill[2] = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Buff[2];
	//-- PickObtain
	pRequest.ObtainRange = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Range[1];
	pRequest.ObtainRepairOn = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.RepairItem;
	pRequest.ObtainPickNear = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PickAllItem;
	pRequest.ObtainPickSelected = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PickSelected;
	pRequest.ObtainPickMoney = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PickMoney;
	pRequest.ObtainPickJewels = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PickJewel;
	pRequest.ObtainPickExcellent = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PickExc;
	pRequest.ObtainPickAncient = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PickSet;
	pRequest.ObtainPickExtra = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.PickExtra;

	pRequest.ObtainPickExtraCount = 0;

	for (int i = 0; i < 12; i++)
	{
		ZeroMemory(pRequest.ObtainPickItemList[i], sizeof(g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.ItemList[i]));
		wsprintf(pRequest.ObtainPickItemList[i], g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.ItemList[i]);

		if ((strcmp(g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.ItemList[i], "") != 0))
		{
			pRequest.ObtainPickExtraCount++;
		}
	}

	//-- Activation skills 1/2 with their delay/condition, as the online helper uses them
	for (int i = 0; i < 2; i++)
	{
		pRequest.ActiveSkill[i] = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Skill[i + 1];
		pRequest.ActiveSkillDelayOn[i] = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SkillDelay[i];
		pRequest.ActiveSkillConditionOn[i] = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SkillCondition[i];
		pRequest.ActiveSkillSubCon[i] = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.SkillSubCon[i];
		pRequest.ActiveSkillDelay[i] = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu.Delay[i];
	}

	{
		const auto& d = g_pNewUISystem->Get_pNewUIMuHelper()->DataAutoMu;
		pRequest.HelperFlags = (BYTE)((d.RegularAttack ? 1 : 0) | (d.TargetConcentration ? 2 : 0) | (d.ShortDistance ? 4 : 0)
			| (d.SkillPreCon[0] == 1 ? 8 : 0) | (d.SkillPreCon[1] == 1 ? 16 : 0));
		pRequest.HelperFlagsMark = 0xA5;
	}
	static_assert(sizeof(CG_OFFMODE_RESULT) <= 255, "C1 packet: the size is one byte");

	DataSend((LPBYTE)& pRequest, pRequest.h.size);
}
#endif