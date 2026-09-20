// MapManager.h: interface for the CMapManager class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include "User.h"

// ItemDropType is a sum of the bits below (or -1/"*" for the server default,
// meaning every type is allowed) - see Monster.cpp's kill-drop sequence for
// where each one is actually gated.
#define ITEMDROPTYPE_MONEY			1
#define ITEMDROPTYPE_COMMON			2
#define ITEMDROPTYPE_ITEMDROPTXT	4
#define ITEMDROPTYPE_EVENTITEMBAG	8

struct MAP_MANAGER_INFO
{
	int Index;
	int NonPK;
	int ViewRange;
	int ExperienceRate;
	int ItemDropRate;
	int ExcItemDropRate;
	int SetItemDropRate;
	int SocketItemDrop;
	int HelperEnable;
	int GensBattle;
	int DisableCustomAttack;
	int PartyEnable;
	int AllowTrade;
	int ItemDropType;
	char Name[32];
};

class CMapManager
{
public:
	CMapManager();
	virtual ~CMapManager();
	void Load(char* path);
	int GetMapNonPK(int index,LPOBJ lpObj,LPOBJ lpTarget);
	int GetMapNonPK(int index);
	int GetMapNonOutlaw(int index);
	int GetMapViewRange(int index);
	int GetMapExperienceRate(int index);
	int GetMapMasterExperienceRate(int index);
	int GetMapItemDropRate(int index);
	int GetMapExcItemDropRate(int index);
	int GetMapSetItemDropRate(int index);
	int GetMapSocketItemDrop(int index);
	int GetMapHelperEnable(int index);
	int GetMapGensBattle(int index);
	int GetMapDisableCustomAttack(int index);
	int GetMapPartyEnable(int index);
	int GetMapAllowTrade(int index);
	int GetMapItemDropType(int index);
	int CheckMapItemDropType(int index,int type);

	char* GetMapName(int index);
public:
	std::map<int,MAP_MANAGER_INFO> m_MapManagerInfo;
};

extern CMapManager gMapManager;
