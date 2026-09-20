// MemoryAllocatorInfo.h: interface for the CMemoryAllocatorInfo class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "User.h"

class CMemoryAllocatorInfo
{
public:
	CMemoryAllocatorInfo();
	void Clear();
	void Alloc();
public:
	int m_Index;
	int m_Active;
	DWORD m_ActiveTime;
	CSkill* m_SkillBackup;
	CSkill* m_Skill;
	VIEWPORT_STRUCT* m_VpPlayer;
	VIEWPORT_STRUCT* m_VpPlayer2;
	VIEWPORT_STRUCT* m_VpPlayerItem;
	// Parallel to m_VpPlayerItem, indexed identically - lets DestroyViewportItem
	// detect a map item slot being reused by a DIFFERENT item before this
	// player's next viewport tick sees it go empty/DIECMD (see Viewport.cpp).
	// A plain new array kept separate from VIEWPORT_STRUCT (shared by VpPlayer/
	// VpPlayer2 for unrelated player/monster tracking) so this stays scoped to
	// item tracking only.
	DWORD* m_VpPlayerItemSerial;
	HIT_DAMAGE_STRUCT* m_HitDamage;
	CEffect* m_Effect;
	PER_SOCKET_CONTEXT* m_PerSocketContext;
	CItem* m_Inventory1;
	CItem* m_Inventory2;
	BYTE* m_InventoryMap1;
	BYTE* m_InventoryMap2;
	CItem* m_Trade;
	BYTE* m_TradeMap;
	CItem* m_Warehouse;
	BYTE* m_WarehouseMap;
	CItem* m_ChaosBox;
	BYTE* m_ChaosBoxMap;
	#if(GAMESERVER_UPDATE>=802)
	CItem* m_EventInventory1;
	CItem* m_EventInventory2;
	BYTE* m_EventInventoryMap1;
	BYTE* m_EventInventoryMap2;
	#endif
	#if(GAMESERVER_UPDATE>=803)
	CItem* m_MuunInventory;
	BYTE* m_MuunInventoryMap;
	#endif
	// Spin wheel claim box - see SPIN_CLAIM_SIZE (CustomVongQuay.h).
	CItem* m_SpinClaim;
#if(HAISLOTRING) //ZEUS UP20
	CItem* m_EquipInventory1;
	CItem* m_EquipInventory2;
	BYTE* m_EquipInventoryMap1;
	BYTE* m_EquipInventoryMap2;
#endif
	CSkill* m_MasterSkill;
	QUEST_KILL_COUNT* m_QuestKillCount;
	QUEST_WORLD_LIST* m_QuestWorldList;
	GENS_SYSTEM_VICTIM_LIST* m_GensVictimList;
	DWORD* m_SkillDelay;
	DWORD* m_HackPacketDelay;
	DWORD* m_HackPacketCount;
	#if(GAMESERVER_UPDATE>=701)
	PENTAGRAM_JEWEL_INFO* m_PentagramJewelInfo_Inventory;
	PENTAGRAM_JEWEL_INFO* m_PentagramJewelInfo_Warehouse;
	#endif
	#if(GAMESERVER_UPDATE>=802)
	CMuRummyInfo* m_MuRummyInfo;
	#endif
#if(JEWELBANKVER2)
	USER_BANK_DATA* m_BankJewelData;
#endif
};
