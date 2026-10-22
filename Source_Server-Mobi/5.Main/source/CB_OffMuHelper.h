#pragma once
#include "Protocol.h"

#if(CB_OFFMUHELPER)
class CB_OffMuHelper
{
public:
	struct CG_OFFMODE_RESULT
	{
		PBMSG_HEAD2 h;
		WORD HuntingRange;
		//-- Potion
		BYTE RecoveryPotionOn;
		BYTE RecoveryPotionPercent;
		BYTE RecoveryHealOn; //-> CLASS_ELF
		BYTE RecoveryHealPercent; //-> CLASS_ELF
		BYTE RecoveryDrainOn; //-> CLASS_SUMMONER
		BYTE RecoveryDrainPercent; //-> CLASS_SUMMONER
		//-- Attack
		BYTE DistanceLongOn;
		BYTE DistanceReturnOn;
		WORD DistanceMin;//-- tiempo
		WORD SkillBasicID;
		BYTE ComboOn;				//-> CLASS_KNIGHT
		BYTE PartyModeOn;
		BYTE PartyModeHealOn;		//-> CLASS_ELF
		BYTE PartyModeHealPercent;	//-> CLASS_ELF
		BYTE PartyModeBuffOn;
		BYTE BuffOn;
		WORD BuffSkill[3];
		//-- Obtaing
		WORD ObtainRange;
		BYTE ObtainRepairOn;
		BYTE ObtainPickNear;
		BYTE ObtainPickSelected;
		BYTE ObtainPickMoney;
		BYTE ObtainPickJewels;
		BYTE ObtainPickExcellent;
		BYTE ObtainPickAncient;
		BYTE ObtainPickExtra;
		int ObtainPickExtraCount;
		char ObtainPickItemList[12][16];
		//-- Activation skills 1/2 (appended: an older server just ignores the tail)
		WORD ActiveSkill[2];
		BYTE ActiveSkillDelayOn[2];
		BYTE ActiveSkillConditionOn[2];
		BYTE ActiveSkillSubCon[2];
		WORD ActiveSkillDelay[2];
		// Hunting options (bits): 1 Regular Attack, 2 Target Concentration, 4 Use
		// Skills Closely, 8/16 "Monster Attacking Me" for activation skill 1/2.
		// These two bytes were the struct's end padding, so the packet stays 252
		// bytes; the mark tells the server they are set (an old client leaves junk).
		BYTE HelperFlags;
		BYTE HelperFlagsMark;  // 0xA5
	};

	CB_OffMuHelper();
	~CB_OffMuHelper();
	void RunningOffHelper();
	void Render(float X, float Y);
	bool ActiveOfflineMode;
};

extern CB_OffMuHelper* gCB_OffMuHelper;
#endif