// ChaosBox.h: interface for the CChaosMix class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "ItemManager.h"
#include "Protocol.h"
#include "User.h"

// Fallback only. The live cap is gServerInfo.m_MaxTalismanOfLuck, read from
// MaxTalismanOfLuck in CustomConfig.ini; this is what that key defaults to, so
// an ini predating the key keeps the value that used to be compiled in.
#define MAX_TALISMAN_OF_LUCK 10

// Results for GCChaosMixSend when a Talisman of Luck is the reason a mix was
// refused. 240 is the code this has always sent; the client ignored it, so an
// over-limit mix simply did nothing. Both are handled in ReceiveMix now.
#define CHAOS_MIX_RESULT_TALISMAN_OVER_LIMIT  240
#define CHAOS_MIX_RESULT_TALISMAN_NOT_ALLOWED 241

// "Just close the mix window, the player has already been told why" - for
// refusals that send their own message box. Falls into ReceiveMix's default
// case, which is what an unhandled 240 used to do before 240 gained a message.
#define CHAOS_MIX_RESULT_CLOSE_SILENT         242

enum eChaosMixNumber
{
	CHAOS_MIX_CHAOS_ITEM = 1,
	CHAOS_MIX_DEVIL_SQUARE = 2,
	CHAOS_MIX_PLUS_ITEM_LEVEL1 = 3,
	CHAOS_MIX_PLUS_ITEM_LEVEL2 = 4,
	CHAOS_MIX_DINORANT = 5,
	CHAOS_MIX_FRUIT = 6,
	CHAOS_MIX_WING1 = 7,
	CHAOS_MIX_BLOOD_CASTLE = 8,
	CHAOS_MIX_WING2 = 11,
	CHAOS_MIX_PET1 = 13,
	CHAOS_MIX_PET2 = 14,
	CHAOS_MIX_SIEGE_POTION1 = 15,
	CHAOS_MIX_SIEGE_POTION2 = 16,
	CHAOS_MIX_LIFE_STONE = 17,
	CHAOS_MIX_SENIOR = 18,
	CHAOS_MIX_PLUS_ITEM_LEVEL3 = 22,
	CHAOS_MIX_PLUS_ITEM_LEVEL4 = 23,
	CHAOS_MIX_WING3 = 24,
	CHAOS_MIX_PIECE_OF_HORN = 25,
	CHAOS_MIX_BROKEN_HORN = 26,
	CHAOS_MIX_HORN_OF_FENRIR = 27,
	CHAOS_MIX_HORN_OF_FENRIR_UPGRADE = 28,
	CHAOS_MIX_SHIELD_POTION1 = 30,
	CHAOS_MIX_SHIELD_POTION2 = 31,
	CHAOS_MIX_SHIELD_POTION3 = 32,
	CHAOS_MIX_JEWEL_OF_HARMONY_ITEM_PURITY = 33,
	CHAOS_MIX_JEWEL_OF_HARMONY_ITEM_SMELT = 34,
	CHAOS_MIX_JEWEL_OF_HARMONY_ITEM_RESTORE = 35,
	CHAOS_MIX_ITEM_380 = 36,
	CHAOS_MIX_ILLUSION_TEMPLE = 37,
	CHAOS_MIX_FEATHER_OF_CONDOR = 38,
	CHAOS_MIX_WING4 = 39,
	CHAOS_MIX_CHAOS_CARD = 40,
	CHAOS_MIX_CHERRY_BLOSSOM = 41,
	CHAOS_MIX_SOCKET_ITEM_CREATE_SEED = 42,
	CHAOS_MIX_SOCKET_ITEM_CREATE_SEED_SPHERE = 43,
	CHAOS_MIX_SOCKET_ITEM_MOUNT_SEED_SPHERE = 44,
	CHAOS_MIX_SOCKET_ITEM_UN_MOUNT_SEED_SPHERE = 45,
	CHAOS_MIX_IMPERIAL_GUARDIAN = 46,
	CHAOS_MIX_CHEST = 47,
	CHAOS_MIX_SUMMON_SCROLL = 48,
	CHAOS_MIX_PLUS_ITEM_LEVEL5 = 49,
	CHAOS_MIX_PLUS_ITEM_LEVEL6 = 50,
	CHAOS_MIX_LUCKY_ITEM_CREATE = 51,
	CHAOS_MIX_LUCKY_ITEM_REFINE = 52,
	CHAOS_MIX_MONSTER_WING = 56,
	CHAOS_MIX_SOCKET_WEAPON = 57,
	CHAOS_MIX_FENRIR_GOLD = 58,
};

//**********************************************//
//************ Client -> GameServer ************//
//**********************************************//

struct PMSG_CHAOS_MIX_RECV
{
	PBMSG_HEAD header; // C1:86
	BYTE type[2];
	BYTE info;
};

//**********************************************//
//************ GameServer -> Client ************//
//**********************************************//

struct PMSG_CHAOS_MIX_SEND
{
	PBMSG_HEAD header; // C1:86
	BYTE result;
	BYTE ItemInfo[MAX_ITEM_INFO];
};
#if (CB_GETMIXRATE)
struct DATA_CUSTOMRATEMIX
{
	int CodeMix;
	int RateShow;
};

#endif
//**********************************************//
//**********************************************//
//**********************************************//

class CChaosBox
{
public:
	CChaosBox();
	virtual ~CChaosBox();
	void ChaosBoxInit(LPOBJ lpObj);
	void ChaosBoxItemDown(LPOBJ lpObj,int slot);
	void ChaosBoxItemKeep(LPOBJ lpObj,int slot);
	void ChaosBoxItemSave(LPOBJ lpObj);
	// bTalismanAllowed: which mixes take a Talisman of Luck. Passed true by
	// wing creation (Wing1/2/3Mix - all four wing tiers route through those),
	// the item-level upgrade, custom Regular Combination recipes, Dinorant,
	// pets, and the Fenrir chain (PieceOfHorn/BrokenHorn/HornOfFenrir/
	// HornOfFenrirUpgrade/HornOfFenrirGold/FeatherOfCondor).
	//
	// PentagramJewelUpgradeLevelMix passes true as well. It takes the
	// ELEMENTAL talisman, and that item has no other use anywhere - refusing
	// it there would leave players holding an item nothing accepts.
	//
	// Every other mix leaves the default false and is refused outright if a
	// talisman is in the box, rather than silently eating it.
	bool GetTalismanOfLuckRate(LPOBJ lpObj,int* rate,bool bTalismanAllowed = false);
	bool GetElementalTalismanOfLuckRate(LPOBJ lpObj,int* rate,bool bTalismanAllowed = false);

	// Set by the two calls above on refusal, so each caller can report which
	// of the two reasons it was without repeating the checks.
	BYTE m_TalismanMixResult;

	// Talisman of Luck / Elemental Talisman of Luck. These are success-rate
	// modifiers, not ingredients, so the recipe matcher in CustomMix.cpp has
	// to skip them - see CheckItemMixForIndex.
	static bool IsTalismanOfLuckItem(int ItemIndex);
	void ChaosItemMix(LPOBJ lpObj);
	void DevilSquareMix(LPOBJ lpObj);
	void PlusItemLevelMix(LPOBJ lpObj,int type);
	void DinorantMix(LPOBJ lpObj);
	void FruitMix(LPOBJ lpObj);
	void Wing2Mix(LPOBJ lpObj,int type);
	void BloodCastleMix(LPOBJ lpObj);
	void Wing1Mix(LPOBJ lpObj);
	void PetMix(LPOBJ lpObj,int type);
	void SiegePotionMix(LPOBJ lpObj,int type);
	void LifeStoneMix(LPOBJ lpObj);
	void SeniorMix(LPOBJ lpObj);
	void PieceOfHornMix(LPOBJ lpObj);
	void BrokenHornMix(LPOBJ lpObj);
	void HornOfFenrirMix(LPOBJ lpObj);
	void HornOfFenrirUpgradeMix(LPOBJ lpObj);
	void HornOfFenrirGoldMix(LPOBJ lpObj);
	void ShieldPotionMix(LPOBJ lpObj,int type);
	void JewelOfHarmonyItemPurityMix(LPOBJ lpObj);
	void JewelOfHarmonyItemSmeltMix(LPOBJ lpObj);
	void JewelOfHarmonyItemRestoreMix(LPOBJ lpObj);
	void Item380Mix(LPOBJ lpObj);
	void IllusionTempleMix(LPOBJ lpObj);
	void FeatherOfCondorMix(LPOBJ lpObj);
	void Wing3Mix(LPOBJ lpObj);
	void ChaosCardMix(LPOBJ lpObj,int type);
	void CherryBlossomMix(LPOBJ lpObj,int type);
	void SocketItemCreateSeedMix(LPOBJ lpObj);
	void SocketItemCreateSeedSphereMix(LPOBJ lpObj);
	void SocketItemMountSeedSphereMix(LPOBJ lpObj,BYTE info);
	void SocketItemUnMountSeedSphereMix(LPOBJ lpObj,BYTE info);
	void ImperialGuardianMix(LPOBJ lpObj);
	void ChestMix(LPOBJ lpObj);
	void SummonScrollMix(LPOBJ lpObj,int type);
	void LuckyItemCreateMix(LPOBJ lpObj);
	void LuckyItemRefineMix(LPOBJ lpObj);
	void MonsterWingMix(LPOBJ lpObj);
	void SocketWeaponMix(LPOBJ lpObj);
	void PentagramMithrilMix(LPOBJ lpObj);
	void PentagramElixirMix(LPOBJ lpObj);
	void PentagramJewelMix(LPOBJ lpObj);
	void PentagramDecompositeMix(LPOBJ lpObj,int type);
	void PentagramJewelUpgradeLevelMix(LPOBJ lpObj,BYTE info);
	void PentagramJewelUpgradeRankMix(LPOBJ lpObj,BYTE info);
	void CGChaosMixRecv(PMSG_CHAOS_MIX_RECV* lpMsg,int aIndex);
	void CGChaosMixCloseRecv(int aIndex);
	void GCChaosBoxSend(LPOBJ lpObj,BYTE type);
	void GCChaosMixSend(int aIndex,BYTE result,CItem* lpItem);
	//=====================================================
	void CustomItemMix(LPOBJ lpObj, int HeadCode);
#if (CB_GETMIXRATE)
	void CGChaosMixInfo(BYTE* lpMsg, int aIndex, int size);
	void RecalcAndSendMixRate(LPOBJ lpObj, int ChaosMixId, int ChaosMixInfo);
	void RefreshAllOpenMixRates();
	int MixRateShow;
	std::map<int, DATA_CUSTOMRATEMIX> m_DataMixRateShow;
	void LoadMixRateShow(char* FilePath);
#endif
public:
	int m_SeniorMixLimitDay;
	int m_SeniorMixLimitMonth;
	int m_SeniorMixLimitYear;
};

extern CChaosBox gChaosBox;
