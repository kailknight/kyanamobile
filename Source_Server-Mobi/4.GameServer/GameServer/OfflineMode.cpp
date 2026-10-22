#include "StdAfx.h"
#include "OfflineMode.h"
#include "user.h"
#include "GameMain.h"
#include "Message.h"
#include "MapServerManager.h"
#include "Util.h"
#include "Path.h"
#include "Notice.h"
#include "Map.h"
#include "Move.h"
#include "Skill.h"
#include "Party.h"
#include "Attack.h"
#include "Quest.h"
#include "QuestObjective.h"
#include "SkillManager.h"
#include "ServerInfo.h"
#include "ObjectManager.h"
#include "Viewport.h"
#include "DeviceBan.h"
#include "EffectManager.h"
#include "CustomAttack.h"
#include "ItemManager.h"
#include "MapManager.h"
#include "Log.h"
#include "ItemLevel.h"
#include <map>
#include <vector>
#include <algorithm>

OfflineMode g_OfflineMode;


OfflineMode::OfflineMode()
{
	this->m_bLoadedDB = false;
}
// ----------------------------------------------------------------------------------------------

OfflineMode::~OfflineMode()
{
}

void OfflineMode::ReadOffLine(char* section,char* path)
{
	this->m_MUOffHelperEnabled[0] = GetPrivateProfileInt(section, "MuOffHelperEnabled_AL0", 0, path);
	this->m_MUOffHelperEnabled[1] = GetPrivateProfileInt(section, "MuOffHelperEnabled_AL1", 0, path);
	this->m_MUOffHelperEnabled[2] = GetPrivateProfileInt(section, "MuOffHelperEnabled_AL2", 0, path);
	this->m_MUOffHelperEnabled[3] = GetPrivateProfileInt(section, "MuOffHelperEnabled_AL3", 0, path);
	this->m_MUOffHelperTimer[0] = GetPrivateProfileInt(section, "MuOffHelperTime_AL0", -1, path);
	this->m_MUOffHelperTimer[1] = GetPrivateProfileInt(section, "MuOffHelperTime_AL1", -1, path);
	this->m_MUOffHelperTimer[2] = GetPrivateProfileInt(section, "MuOffHelperTime_AL2", -1, path);
	this->m_MUOffHelperTimer[3] = GetPrivateProfileInt(section, "MuOffHelperTime_AL3", -1, path);
	// 1 = after dying, the off helper teleports back to where it was started.
	this->m_MUOffHelperReturn[0] = GetPrivateProfileInt(section, "MuOffHelperReturn_AL0", 0, path);
	this->m_MUOffHelperReturn[1] = GetPrivateProfileInt(section, "MuOffHelperReturn_AL1", 0, path);
	this->m_MUOffHelperReturn[2] = GetPrivateProfileInt(section, "MuOffHelperReturn_AL2", 0, path);
	this->m_MUOffHelperReturn[3] = GetPrivateProfileInt(section, "MuOffHelperReturn_AL3", 0, path);
	// Most characters one phone/PC may have in off helper at once (0 = no limit).
	this->m_MUOffHelperMaxPerDevice = GetPrivateProfileInt(section, "MuOffHelperMaxPerDevice", 0, path);
}

// Same machine as lpObj: the hardware id when both sides sent one (0xD3:0x7D),
// otherwise the IP, so a client too old to send an id cannot dodge the limit.
static bool OffHelperSameDevice(LPOBJ lpObj,LPOBJ lpOther)
{
	if(DeviceIdValid(lpObj->m_DeviceId) && DeviceIdValid(lpOther->m_DeviceId))
	{
		return strcmp(lpObj->m_DeviceId,lpOther->m_DeviceId) == 0;
	}

	return strcmp(lpObj->IpAddr,lpOther->IpAddr) == 0;
}

// Ends a character's off helper and removes it from the game. Called when the
// account logs in again, from the GameServer window's Disconnect, /disconnect,
// a device ban and server shutdown. It used to require IsFakeOnline too, so it
// only ever removed fake-online bots and a real player's off helper could not
// be disconnected at all (it has no socket for CloseClient to close).
void OfflineMode::OnHelperpAlreadyConnected(LPOBJ lpObj) // OK
{
	if(lpObj->m_OfflineMode != 0)
	{
		gObjDel(lpObj->Index);
		lpObj->m_OfflineMode = 0;
		lpObj->MuOffHelperTime = 0;
	}
}

// A helper character standing in a safe zone: fake-online bots are removed,
// a real player's off helper is left alone (exactly the old behaviour).
void OfflineMode::OnHelperInSafeZone(LPOBJ lpObj) // OK
{
	if(lpObj->m_OfflineMode != 0 && lpObj->IsFakeOnline !=0)
	{
		gObjDel(lpObj->Index);
		lpObj->m_OfflineMode = 0;
		lpObj->MuOffHelperTime = 0;
	}
}

void OfflineMode::Start(CG_OFFMODE_RESULT* aRecv, int aIndex)
{
	if (!gObjIsConnectedGP(aIndex))
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(gMapManager.GetMapDisableCustomAttack(lpObj->Map) == 0 && lpObj->IsFakeOnline == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"[Helper] OfflineMode Disable");
		return;
	}

	if(this->m_MUOffHelperEnabled[lpObj->AccountLevel] == 0 && lpObj->IsFakeOnline == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,gMessage.GetMessage(64));
		return;
	}

	switch(lpObj->Map)
	{
	case MAP_INDEX_CASTLESIEGE:
		gNotice.GCNoticeSend(aIndex, 1, 0, 0, 0, 0, 0, "[OfflineMode] Disable in Castle siege map");
		return;
	case MAP_INDEX_CRYWOLF_FIRSTZONE:
		gNotice.GCNoticeSend(aIndex, 1, 0, 0, 0, 0, 0, "[OfflineMode] Disable in Crywolf map");
		return;
	case MAP_INDEX_CRYWOLF_SECONDZONE:
		gNotice.GCNoticeSend(aIndex, 1, 0, 0, 0, 0, 0, "[OfflineMode] Disable in Crywolf map");
		return;
	case MAP_INDEX_CASTLEHUNTZONE:
		gNotice.GCNoticeSend(aIndex, 1, 0, 0, 0, 0, 0, "[OfflineMode] Disable in This Map");
		return;
	case MAP_INDEX_BALGASS_BARRACKS:
		gNotice.GCNoticeSend(aIndex, 1, 0, 0, 0, 0, 0, "[OfflineMode] Disable in This Map");
		return;
	case MAP_INDEX_BALGASS_REFUGEE:
		gNotice.GCNoticeSend(aIndex, 1, 0, 0, 0, 0, 0, "[OfflineMode] Disable in This Map");
		return;
	}

	// Per-device limit (MuOffHelperMaxPerDevice in GameServerInfo - Custom.ini).
	// Counts this machine's characters that are already in off helper; the one
	// asking now is still online, so it is never counted.
	if(this->m_MUOffHelperMaxPerDevice > 0 && lpObj->IsFakeOnline == 0)
	{
		int count = 0;

		for(int n = OBJECT_START_USER; n < MAX_OBJECT; n++)
		{
			if(n != aIndex && gObj[n].m_OfflineMode != 0 && gObj[n].IsFakeOnline == 0 && OffHelperSameDevice(lpObj,&gObj[n]))
			{
				count++;
			}
		}

		if(count >= this->m_MUOffHelperMaxPerDevice)
		{
			gNotice.GCNoticeSend(aIndex,1,0,0,0,0,0,"[OfflineMode] This device already has %d character(s) in off helper (limit %d)",count,this->m_MUOffHelperMaxPerDevice);
			LogAdd(LOG_RED,"[OfflineMode][%s][%s] refused: %d off helper(s) already on device %s / %s",lpObj->Account,lpObj->Name,count,DeviceIdValid(lpObj->m_DeviceId) ? lpObj->m_DeviceId : "(no id)",lpObj->IpAddr);
			return;
		}
	}

	/*if (lpObj->PShopOpen == true)
	{
		if (this->bOnlyLorenMarket != 0 && lpObj->Map != 79)
		{
			gNotice.GCNoticeSend(aIndex, 1, 0, 0, 0, 0, 0, "[OffTrade] OffTrade work only Loren Market");
			return;
		}
		else
		{
			gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0, gMessage.GetMessage(3070), lpObj->Name);
		}
	}*/
	//===========
	/*+172*/
	//LogAdd(LOG_RED, "Active OfflineMode %s Range %d Skill %d", lpObj->Name, aRecv->HuntingRange, aRecv->SkillBasicID);
	lpObj->HuntingRange = aRecv->HuntingRange;
	//--
	lpObj->RecoveryPotionOn = aRecv->RecoveryPotionOn;
	lpObj->RecoveryPotionPercent = aRecv->RecoveryPotionPercent;
	lpObj->RecoveryHealOn = (lpObj->Class == CLASS_FE) ? aRecv->RecoveryHealOn : 0; //-> CLASS_ELF
	lpObj->RecoveryHealPercent = (lpObj->Class == CLASS_FE) ? aRecv->RecoveryHealPercent : 0; //-> CLASS_ELF
	lpObj->RecoveryDrainOn = (lpObj->Class == CLASS_SU) ? aRecv->RecoveryDrainOn : 0; //-> CLASS_SUMMONER
	lpObj->RecoveryDrainPercent = (lpObj->Class == CLASS_SU) ?aRecv->RecoveryDrainPercent : 0; //-> CLASS_SUMMONER
	//--
	lpObj->DistanceLongOn = aRecv->DistanceLongOn;
	lpObj->DistanceReturnOn = aRecv->DistanceReturnOn;
	lpObj->DistanceMin = aRecv->DistanceMin;
	lpObj->SkillBasicID = aRecv->SkillBasicID;
	lpObj->ComboOn = (lpObj->Class == CLASS_DK) ? aRecv->ComboOn : 0;				//-> CLASS_KNIGHT only
	//-- Activation skills 1/2 (an older client sends a shorter packet: basic skill only)
	for(int i = 0; i < 2; i++)
	{
		bool has = (aRecv->h.size >= sizeof(CG_OFFMODE_RESULT));
		lpObj->OffActiveSkill[i] = has ? aRecv->ActiveSkill[i] : 0xFFFF;
		lpObj->OffActiveDelayOn[i] = has ? aRecv->ActiveSkillDelayOn[i] : 0;
		lpObj->OffActiveConditionOn[i] = has ? aRecv->ActiveSkillConditionOn[i] : 0;
		lpObj->OffActiveSubCon[i] = has ? aRecv->ActiveSkillSubCon[i] : 0;
		lpObj->OffActiveDelay[i] = has ? aRecv->ActiveSkillDelay[i] : 0;
		lpObj->OffActiveTick[i] = 0;
	}
	if(aRecv->h.size < sizeof(CG_OFFMODE_RESULT))
	{
		lpObj->ComboOn = 0;
	}
	lpObj->OffComboStep = 0;
	lpObj->OffComboTick = 0;
	// Hunting options: only from a client that marked them (older clients
	// leave the end padding as junk).
	lpObj->OffHelperFlags = (aRecv->h.size >= sizeof(CG_OFFMODE_RESULT) && aRecv->HelperFlagsMark == 0xA5) ? aRecv->HelperFlags : 0;
	lpObj->OffCloseTurn = 0;
	lpObj->OffRegularTick = 0;
	//--
	lpObj->PartyModeOn = aRecv->PartyModeOn;
	lpObj->PartyModeHealOn = (lpObj->Class == CLASS_FE) ? aRecv->PartyModeHealOn : 0;		//-> CLASS_ELF
	lpObj->PartyModeHealPercent = (lpObj->Class == CLASS_FE) ? aRecv->PartyModeHealPercent : 0;	//-> CLASS_ELF
	lpObj->PartyModeBuffOn = aRecv->PartyModeBuffOn;
	//--
	lpObj->BuffOn = aRecv->BuffOn;
	lpObj->BuffSkill[0] = aRecv->BuffSkill[0];
	lpObj->BuffSkill[1] = aRecv->BuffSkill[1];
	lpObj->BuffSkill[2] = aRecv->BuffSkill[2];
	lpObj->ObtainRange = (DWORD) aRecv->ObtainRange;
	lpObj->ObtainRepairOn = aRecv->ObtainRepairOn;
	lpObj->ObtainPickNear = (aRecv->ObtainPickNear != 0) ? 1 : 0;
	lpObj->ObtainPickSelected = aRecv->ObtainPickSelected;
	lpObj->ObtainPickJewels = (aRecv->ObtainPickJewels != 0) ? 1 : 0;
	lpObj->ObtainPickAncient = (aRecv->ObtainPickAncient != 0) ? 1 : 0;
	lpObj->ObtainPickMoney = (aRecv->ObtainPickMoney != 0) ? 1 : 0;
	lpObj->ObtainPickExcellent = (aRecv->ObtainPickExcellent != 0) ? 1 : 0;
	lpObj->ObtainPickExtra = (aRecv->ObtainPickExtra != 0) ? 1 : 0;
	// All 12 slots, always: the client sends how many are FILLED, and copying
	// that many from the top lost entries after a gap (and a count above 12
	// wrote past the list). Empty slots are skipped when matching.
	lpObj->ObtainPickExtraCount = 12;

	for(int i = 0 ; i < 12 ; ++i)
	{
		memcpy(lpObj->ObtainPickItemList[ i ], aRecv->ObtainPickItemList[ i ], sizeof(lpObj->ObtainPickItemList[ i ]));
		lpObj->ObtainPickItemList[ i ][ sizeof(lpObj->ObtainPickItemList[ i ]) - 1 ] = 0;
	}

	//===========
	lpObj->m_OfflineCoordX = lpObj->X;
	lpObj->m_OfflineCoordY = lpObj->Y;
	lpObj->m_OfflineMap = lpObj->Map;
	lpObj->m_OfflineReturning = 0;
	lpObj->m_OfflineTimeResetMove = GetTickCount();
	lpObj->AttackCustomDelay = GetTickCount();
	lpObj->MuOffHelperTime = this->m_MUOffHelperTimer[lpObj->AccountLevel] * 600;
	lpObj->m_OfflineSocket = true;
	CloseClient(aIndex);
	lpObj->m_OfflineSocket = false;
	lpObj->m_OfflineMode = 1;
	lpObj->orderskill = 0;
}

void OfflineMode::OnAttackSecondProcHelper(LPOBJ lpObj) // OK
{
	if(lpObj->m_OfflineMode != 0)
	{
		lpObj->CheckSumTime = GetTickCount();
		lpObj->ConnectTickCount = GetTickCount();
		lpObj->PcPointPointTime = ((gCustomAttack.m_CustomAttackOfflineGPGain == 0)?GetTickCount():lpObj->PcPointPointTime);
		lpObj->CashShopGoblinPointTime = ((gCustomAttack.m_CustomAttackOfflineGPGain == 0)?GetTickCount():lpObj->CashShopGoblinPointTime);
	}
}

void AnimationMove(int aIndex, int x, int y)
{
	LPOBJ lpObj = &gObj[aIndex];

	if (lpObj->Teleport != 0 && lpObj->IsFakeOnline == 0)
	{
		return;
	}

	if (gObjCheckMapTile(lpObj, 1) != 0)
	{
		return;
	}

	if ((GetTickCount() - lpObj->LastMoveTime) < 100)
	{
		return;
	}

	if (gEffectManager.CheckStunEffect(lpObj) != 0 || gEffectManager.CheckImmobilizeEffect(lpObj) != 0)
	{
		return;
	}

	if (lpObj->SkillSummonPartyTime != 0)
	{
		lpObj->SkillSummonPartyTime = 0;
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, gMessage.GetMessage(272));
	}

	int tx = x;
	int ty = y;

	lpObj->Dir = GetPathPacketDirPos(tx - lpObj->X, ty - lpObj->Y);
	lpObj->Rest = 0;
	lpObj->LastMoveTime = GetTickCount();

	gMap[lpObj->Map].DelStandAttr(lpObj->OldX, lpObj->OldY);

	lpObj->X = tx;
	lpObj->Y = ty;
	lpObj->TX = tx;
	lpObj->TY = ty;
	lpObj->OldX = tx;
	lpObj->OldY = ty;
	lpObj->ViewState = 0;

	gMap[lpObj->Map].SetStandAttr(lpObj->TX, lpObj->TY);

	PMSG_MOVE_SEND pMsg;
	pMsg.header.set(PROTOCOL_CODE1, sizeof(pMsg));
	pMsg.index[0] = SET_NUMBERHB(lpObj->Index);
	pMsg.index[1] = SET_NUMBERLB(lpObj->Index);
	pMsg.x = (BYTE)lpObj->TX;
	pMsg.y = (BYTE)lpObj->TY;
	pMsg.dir = lpObj->Dir << 4;
 
	for (int n = 0; n < MAX_VIEWPORT; n++)
	{
		if (lpObj->VpPlayer2[n].type == OBJECT_USER)
		{
			if (lpObj->VpPlayer2[n].state != OBJECT_EMPTY && lpObj->VpPlayer2[n].state != OBJECT_DIECMD && lpObj->VpPlayer2[n].state != OBJECT_DIED)
			{
				DataSend(lpObj->VpPlayer2[n].index, (BYTE*)&pMsg, pMsg.header.size);
			}
		}
	}
}

void MuOffHelperRepair(int aIndex)
{
	if (!gObjIsConnectedGP(aIndex))
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	// Only with Repair Item ticked in the helper, and only once an equipped
	// item is down to half its durability (it used to repair everything on
	// every tick, whatever the setting).
	if(lpObj->ObtainRepairOn == 0)
	{
		return;
	}

	for(int n=0 ; n < INVENTORY_WEAR_SIZE; ++n)
	{
		if(lpObj->Inventory[n].IsItem() != 0)
		{
			if(lpObj->Inventory[n].m_BaseDurability <= 0 || (lpObj->Inventory[n].m_Durability * 2) > lpObj->Inventory[n].m_BaseDurability)
			{
				continue;
			}

			int money = gItemManager.RepairItem(lpObj,&lpObj->Inventory[n], n, 1);

			if(money != 0)
			{
				gObjectManager.CharacterCalcAttribute(aIndex);
			}
		}
	}
}

// After a death with MuOffHelperReturn on: get back to the start spot the way a
// player would. On another map, use the Move (M) entry for that map - same
// level, zen, wings and PK checks as the Move window, and it charges the zen.
// On the right map, walk there in short path-found legs, then hunt again.
// Walk-back route for each off-helper: the whole way from the respawn point to
// the start spot, found once with a breadth-first search over the map's tile
// attributes. The old walker only planned 8 tiles ahead in a straight line, so
// in a maze (Atlans 3) it walked into walls and stood there.
struct OFFHELPER_ROUTE
{
	int map;
	int tx;
	int ty;
	std::vector<WORD> points; // x | (y << 8), start excluded
	size_t next;
};

static std::map<int,OFFHELPER_ROUTE> gOffHelperRoute;

static bool OffHelperTileWalkable(CMap* lpMap,int x,int y)
{
	// 4 = wall, 8 = no-walk. 1 (safe zone) and 2 (someone standing) are fine.
	return (lpMap->GetAttr(x,y) & 0x0C) == 0;
}

static bool OffHelperFindRoute(int map,int sx,int sy,int tx,int ty,std::vector<WORD>& out)
{
	out.clear();

	CMap* lpMap = &gMap[map];
	const int w = lpMap->m_width;
	const int h = lpMap->m_height;

	if(w <= 0 || h <= 0 || w > 256 || h > 256 || sx < 0 || sy < 0 || tx < 0 || ty < 0 || sx >= w || sy >= h || tx >= w || ty >= h)
	{
		return false;
	}

	static int prev[256*256];
	static WORD queue[256*256];

	for(int n = 0; n < w*h; n++)
	{
		prev[n] = -1;
	}

	const int start = sy*w+sx;
	const int goal = ty*w+tx;
	int head = 0;
	int tail = 0;

	prev[start] = start;
	queue[tail++] = (WORD)start;

	static const int dx[8] = { 1,-1,0,0,1,1,-1,-1 };
	static const int dy[8] = { 0,0,1,-1,1,-1,1,-1 };

	while(head < tail && prev[goal] == -1)
	{
		const int cur = queue[head++];
		const int cx = cur%w;
		const int cy = cur/w;

		for(int d = 0; d < 8; d++)
		{
			const int nx = cx+dx[d];
			const int ny = cy+dy[d];

			if(nx < 0 || ny < 0 || nx >= w || ny >= h)
			{
				continue;
			}

			const int ni = ny*w+nx;

			if(prev[ni] != -1 || (ni != goal && OffHelperTileWalkable(lpMap,nx,ny) == 0))
			{
				continue;
			}

			// No cutting a wall corner on a diagonal step.
			if(d >= 4 && (OffHelperTileWalkable(lpMap,cx+dx[d],cy) == 0 || OffHelperTileWalkable(lpMap,cx,cy+dy[d]) == 0))
			{
				continue;
			}

			prev[ni] = cur;
			queue[tail++] = (WORD)ni;
		}
	}

	if(prev[goal] == -1)
	{
		return false;
	}

	for(int cur = goal; cur != start; cur = prev[cur])
	{
		out.push_back((WORD)((cur%w) | ((cur/w) << 8)));
	}

	std::reverse(out.begin(),out.end());
	return true;
}

void OfflineMode::ReturnToStartSpot(LPOBJ lpObj)
{
	const DWORD now = GetTickCount();

	if ((int)(now - lpObj->m_OfflineReturnTick) < 500)
	{
		return;
	}

	lpObj->m_OfflineReturnTick = now;

	if (lpObj->Map != lpObj->m_OfflineMap)
	{
		// Move refused (not enough zen, level, no wings for Icarus...) or no
		// Move entry for that map: stay where we are, as before this switch.
		const int MoveIndex = gMove.GetMoveIndexByMap(lpObj->m_OfflineMap);

		if (MoveIndex == -1 || lpObj->m_OfflineReturnTries >= 3)
		{
			lpObj->m_OfflineReturning = 0;
			return;
		}

		lpObj->m_OfflineReturnTries++;
		lpObj->m_OfflineReturnTick = now + 2500; // let the gate move land before checking again
		gMove.Move(lpObj, MoveIndex);
		return;
	}

	const int dx = lpObj->m_OfflineCoordX - lpObj->X;
	const int dy = lpObj->m_OfflineCoordY - lpObj->Y;

	if (abs(dx) <= 1 && abs(dy) <= 1)
	{
		lpObj->m_OfflineReturning = 0;
		lpObj->m_OfflineTimeResetMove = now;
		gOffHelperRoute.erase(lpObj->Index);
		return;
	}

	// Stuck for three minutes (walls the short path-finder can't get round):
	// finish with a same-map teleport rather than standing there forever.
	if ((now - lpObj->m_OfflineReturnStart) > 180000)
	{
		lpObj->m_OfflineReturning = 0;
		gObjTeleport(lpObj->Index, lpObj->m_OfflineMap, lpObj->m_OfflineCoordX, lpObj->m_OfflineCoordY);
		return;
	}

	// Follow a full route (found once, again if knocked off it), 2 tiles per
	// step (~4 tiles a second).
	OFFHELPER_ROUTE& route = gOffHelperRoute[lpObj->Index];

	bool onRoute = (route.map == lpObj->Map && route.tx == lpObj->m_OfflineCoordX && route.ty == lpObj->m_OfflineCoordY && route.next <= route.points.size());

	if (onRoute)
	{
		// Where are we along it? (pushed, or the last step was refused)
		const WORD here = (WORD)(lpObj->X | (lpObj->Y << 8));
		const size_t from = (route.next >= 3) ? route.next - 3 : 0;
		const size_t to = (route.next + 3 < route.points.size()) ? route.next + 3 : route.points.size();
		bool found = (route.next == 0);

		for (size_t n = from; n < to; n++)
		{
			if (route.points[n] == here)
			{
				route.next = n + 1;
				found = true;
				break;
			}
		}

		onRoute = found;
	}

	if (onRoute == false)
	{
		route.map = lpObj->Map;
		route.tx = lpObj->m_OfflineCoordX;
		route.ty = lpObj->m_OfflineCoordY;
		route.next = 0;

		if (OffHelperFindRoute(lpObj->Map, lpObj->X, lpObj->Y, route.tx, route.ty, route.points) == false)
		{
			// No way there on foot: teleport rather than stand still.
			gOffHelperRoute.erase(lpObj->Index);
			lpObj->m_OfflineReturning = 0;
			gObjTeleport(lpObj->Index, lpObj->m_OfflineMap, lpObj->m_OfflineCoordX, lpObj->m_OfflineCoordY);
			return;
		}
	}

	if (route.next >= route.points.size())
	{
		return;
	}

	const size_t step = (route.next + 1 < route.points.size()) ? route.next + 1 : route.next;
	const WORD target = route.points[step];
	AnimationMove(lpObj->Index, target & 0xFF, target >> 8);

	if (lpObj->X == (target & 0xFF) && lpObj->Y == (target >> 8))
	{
		route.next = step + 1;
	}
}

void OfflineMode::Attack(int aIndex)
{
	if (OBJMAX_RANGE(aIndex) == FALSE)
	{
		return;
	}

	if (!gObjIsConnectedGP(aIndex))
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	// Died: with MuOffHelperReturn on, head back to the start spot once
	// respawned - wherever the respawn point is, safe zone or not.
	if (lpObj->DieRegen != 0 && lpObj->m_OfflineMode != 0 && lpObj->IsFakeOnline == 0
		&& this->m_MUOffHelperReturn[lpObj->AccountLevel] != 0
		&& MAP_RANGE(lpObj->m_OfflineMap) != 0
		&& (gMap[lpObj->m_OfflineMap].GetAttr(lpObj->m_OfflineCoordX, lpObj->m_OfflineCoordY) & 1) == 0)
	{
		lpObj->m_OfflineReturning = 1;
		lpObj->m_OfflineReturnTries = 0;
		lpObj->m_OfflineReturnTick = GetTickCount() + 2000; // stand a moment after respawning
		lpObj->m_OfflineReturnStart = GetTickCount();
		gOffHelperRoute.erase(lpObj->Index);
	}

	if (lpObj->m_OfflineMode == 0 && lpObj->IsFakeOnline == 0 || lpObj->DieRegen != 0
		|| lpObj->Teleport != 0)
	{
		return;
	}

	if (lpObj->m_OfflineReturning != 0 && lpObj->IsFakeOnline == 0)
	{
		this->ReturnToStartSpot(lpObj);
		return;
	}

	if (gServerInfo.InSafeZone(aIndex) == true)
	{
		// Killed and respawned in town: head back to the hunting spot if this
		// account level has MuOffHelperReturn on, and that spot isn't a safe
		// zone itself (a helper started in town has nowhere to return to).
		if (lpObj->m_OfflineMode != 0 && lpObj->IsFakeOnline == 0
			&& this->m_MUOffHelperReturn[lpObj->AccountLevel] != 0
			&& MAP_RANGE(lpObj->m_OfflineMap) != 0
			&& (gMap[lpObj->m_OfflineMap].GetAttr(lpObj->m_OfflineCoordX, lpObj->m_OfflineCoordY) & 1) == 0)
		{
			lpObj->m_OfflineReturning = 1;
			lpObj->m_OfflineReturnTries = 0;
			lpObj->m_OfflineReturnTick = 0;
			lpObj->m_OfflineReturnStart = GetTickCount();
			return;
		}

		this->OnHelperInSafeZone(lpObj);
		return;
	}

	if(lpObj->MuOffHelperTime > 0 && lpObj->IsFakeOnline == 0)
	{
		if((--lpObj->MuOffHelperTime) == 0)
		{
			gObjDel(lpObj->Index);
			lpObj->m_OfflineMode = 0;
			lpObj->MuOffHelperTime = 0;
		}
	}

	this->RenderAutoPote(aIndex);

	this->RenderAutoBuffParty(aIndex);

	this->RenderAttack(aIndex);

	MuOffHelperRepair(aIndex);
}

void OfflineMode::RenderAutoPote(int aIndex)	//-- OK
{
	if (!gObjIsConnectedGP(aIndex))
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	//-- AUTO POTION HP
	if(lpObj->RecoveryPotionOn != 0)
	{
		if( lpObj->Life > 0 && lpObj->Life < (( lpObj->MaxLife * lpObj->RecoveryPotionPercent) / 100) )
		{
			PMSG_ITEM_USE_RECV pMsg;

			pMsg.header.set(0x26,sizeof(pMsg));

			pMsg.SourceSlot = 0xFF;

			pMsg.SourceSlot = ((pMsg.SourceSlot==0xFF)?gItemManager.GetInventoryItemSlot(lpObj,GET_ITEM(14,3),-1):pMsg.SourceSlot);

			pMsg.SourceSlot = ((pMsg.SourceSlot==0xFF)?gItemManager.GetInventoryItemSlot(lpObj,GET_ITEM(14,2),-1):pMsg.SourceSlot);

			pMsg.SourceSlot = ((pMsg.SourceSlot==0xFF)?gItemManager.GetInventoryItemSlot(lpObj,GET_ITEM(14,1),-1):pMsg.SourceSlot);

			pMsg.TargetSlot = 0xFF;

			pMsg.type = 0;

			if(INVENTORY_FULL_RANGE(pMsg.SourceSlot) != 0)
			{
				gItemManager.CGItemUseRecv(&pMsg,lpObj->Index);
			}
		}
	}

	//-- RECOVERY HEALING ELF
	if(lpObj->RecoveryHealOn != 0 && (lpObj->m_OfflineMode != 0 || lpObj->IsFakeOnline != 0)) // off helper too, not only bots
	{
		CSkill* RenderSkillHealing;

		RenderSkillHealing = gSkillManager.GetSkill(lpObj, SKILL_HEAL);

		if (RenderSkillHealing != 0)
		{
			if( lpObj->Life < (( lpObj->MaxLife * lpObj->RecoveryHealPercent) / 100) )
			{
				if( gEffectManager.CheckEffect(lpObj, gSkillManager.GetSkillEffect(RenderSkillHealing->m_index)) == 0 )
				{
					gSkillManager.UseAttackSkill(lpObj->Index, lpObj->Index, RenderSkillHealing);
				}
			}
		}
	}
}

// Self buffs, party buffs and the elf's party heal - what the Helper does in
// game. It used to run only for fake-online bots (IsFakeOnline), so a real
// player's off helper never buffed itself or its party. Party members must be
// on the same map and within MAX_PARTY_DISTANCE, like a normal party buff.
void OfflineMode::RenderAutoBuffParty(int aIndex)	//-- OK
{
	if (!gObjIsConnectedGP(aIndex))
	{
		return;
	}
	LPOBJ lpObj = &gObj[aIndex];
	LPOBJ lpTarget;

	if (lpObj->m_OfflineMode == 0 && lpObj->IsFakeOnline == 0)
	{
		return;
	}

	//-- AUTOBUFF (self)
	if ( lpObj->BuffOn != 0 )
	{
		CSkill* RenderBuff;
		for(int n = 0; n < 3 ; n++)
		{
			if(lpObj->BuffSkill[n] > 0)
			{
				RenderBuff = gSkillManager.GetSkill(lpObj, lpObj->BuffSkill[n]);

				if (RenderBuff != 0)
				{
					if(gEffectManager.CheckEffect(lpObj, gSkillManager.GetSkillEffect(RenderBuff->m_index)) == 0 )
					{
						gSkillManager.UseAttackSkill(lpObj->Index, lpObj->Index, RenderBuff); //Buff Your self
					}
				}
			}
		}
	}

	//-- PARTY MODE ON (only with a real party)
	if ( lpObj->PartyModeOn == 0 || OBJECT_RANGE(lpObj->PartyNumber) == 0 || gParty.IsParty(lpObj->PartyNumber) == 0 )
	{
		return;
	}

	for(int i = 0; i < MAX_PARTY_USER ; i++)
	{
		int member = gParty.m_PartyInfo[lpObj->PartyNumber].Index[i];

		if(OBJECT_RANGE(member) == 0 || member == lpObj->Index)
		{
			continue;
		}

		lpTarget = &gObj[member];

		if(gObjIsConnectedGP(member) == 0 || lpTarget->Live == 0 || lpTarget->Map != lpObj->Map
			|| gObjCalcDistance(lpObj,lpTarget) > MAX_PARTY_DISTANCE)
		{
			continue;
		}

		//-- PARTY HEAL ELF
		if( lpObj->PartyModeHealOn != 0 && lpObj->Class == CLASS_FE)
		{
			CSkill* RenderPartyHealing = gSkillManager.GetSkill(lpObj, SKILL_HEAL);

			if (RenderPartyHealing != 0 && lpTarget->Life < (( lpTarget->MaxLife * lpObj->PartyModeHealPercent) / 100))
			{
				gSkillManager.UseAttackSkill(lpObj->Index, lpTarget->Index, RenderPartyHealing);
			}
		}

		//-- PARTY BUFF
		if( lpObj->PartyModeBuffOn != 0 )
		{
			for(int n = 0; n < 3 ; n++)
			{
				if(lpObj->BuffSkill[n] <= 0)
				{
					continue;
				}

				CSkill* RenderPartyBuff = gSkillManager.GetSkill(lpObj, lpObj->BuffSkill[n]);

				if (RenderPartyBuff != 0 && gEffectManager.CheckEffect(lpTarget, gSkillManager.GetSkillEffect(RenderPartyBuff->m_index)) == 0 )
				{
					gSkillManager.UseAttackSkill(lpObj->Index, lpTarget->Index, RenderPartyBuff);
				}
			}
		}
	}
}

CSkill* OfflineMode::GetSlotSkill(LPOBJ lpObj, int slot)
{
	int id = (slot == 0) ? (int)lpObj->SkillBasicID : (int)lpObj->OffActiveSkill[slot - 1];

	if(id <= 0 || id >= 0xFFFF)
	{
		return 0;
	}

	return gSkillManager.GetSkill(lpObj, id);
}

// Summoner debuffs (Sleep, Weakness, Innovation, Blind): in an activation
// slot their "Con" box means Instant Cast (same as the online helper).
static bool OffHelperIsDebuff(CSkill* lpSkill)
{
	if(lpSkill == 0)
	{
		return false;
	}
	const int base = lpSkill->m_skill;
	return base == SKILL_SLEEP || base == SKILL_LESSER_DAMAGE || base == SKILL_LESSER_DEFENSE || base == SKILL_BLIND;
}

static bool OffHelperMonsterAttackingMe(LPOBJ lpObj, LPOBJ lpMon)
{
	return lpMon->TargetNumber == lpObj->Index;
}

// Mirrors the client's ActiveCheckMonterCount: -1 = not now, else a priority.
int OfflineMode::CheckActiveSkill(LPOBJ lpObj, int n)
{
	CSkill* lpSkill = this->GetSlotSkill(lpObj, n + 1);

	if(lpSkill == 0)
	{
		return -1;
	}

	if(lpObj->OffActiveDelayOn[n] != 0)
	{
		DWORD delay = ((lpObj->OffActiveDelay[n] > 1) ? lpObj->OffActiveDelay[n] : 1) * 1000;
		return ((GetTickCount() - lpObj->OffActiveTick[n]) >= delay) ? 1 : -1;
	}

	if(lpObj->OffActiveConditionOn[n] != 0)
	{
		if(OffHelperIsDebuff(lpSkill))
		{
			return -1; // Instant Cast, handled in SelectSkillSlot
		}

		// "Monster Within Hunting range", or "Monster Attacking Me" (flag 8/16).
		const bool attackingMeOnly = (lpObj->OffHelperFlags & (n == 0 ? 8 : 16)) != 0;
		int count = 0;

		for(int v = 0; v < MAX_VIEWPORT; v++)
		{
			if(lpObj->VpPlayer2[v].state == VIEWPORT_NONE || OBJECT_RANGE(lpObj->VpPlayer2[v].index) == 0 || lpObj->VpPlayer2[v].type != OBJECT_MONSTER)
			{
				continue;
			}

			LPOBJ lpTarget = &gObj[lpObj->VpPlayer2[v].index];

			if(lpTarget->Live == 0 || lpTarget->State == OBJECT_EMPTY)
			{
				continue;
			}

			if(attackingMeOnly && OffHelperMonsterAttackingMe(lpObj, lpTarget) == 0)
			{
				continue;
			}

			if(lpObj->DistanceLongOn != 0)
			{
				if(gObjCalcDistance(lpObj, lpTarget) <= (int)lpObj->HuntingRange)
				{
					count++;
				}
			}
			else if(gSkillManager.CheckSkillRange(lpSkill->m_index, lpObj->X, lpObj->Y, lpTarget->X, lpTarget->Y) != 0)
			{
				count++;
			}
		}

		// The boxes read "More Than 2 / 3 / 4 / 5 Mobs" (they used to need 2/4/6/8).
		static const int need[4] = { 2, 3, 4, 5 };
		int sub = lpObj->OffActiveSubCon[n];

		if(sub < 0 || sub > 3)
		{
			return -1;
		}

		return (count >= need[sub]) ? sub : -1;
	}

	return -1;
}

int OfflineMode::SelectSkillSlot(LPOBJ lpObj, int target)
{
	// Debuff with Instant Cast: highest priority, as soon as the target lacks it.
	if(OBJECT_RANGE(target) != 0)
	{
		for(int n = 0; n < 2; n++)
		{
			CSkill* lpSkill = this->GetSlotSkill(lpObj, n + 1);

			if(lpObj->OffActiveConditionOn[n] != 0 && OffHelperIsDebuff(lpSkill)
				&& gEffectManager.CheckEffect(&gObj[target], gSkillManager.GetSkillEffect(lpSkill->m_index)) == 0)
			{
				return n + 1;
			}
		}
	}

	if(lpObj->ComboOn != 0)
	{
		// Combo: basic -> skill 1 -> skill 2. A half-built chain older than
		// 2.5 s can't finish any more (ComboSkill 3 s window), so restart it.
		if(lpObj->OffComboStep < 0 || lpObj->OffComboStep > 2 || (lpObj->OffComboStep != 0 && (GetTickCount() - lpObj->OffComboTick) > 2500))
		{
			lpObj->OffComboStep = 0;
		}

		for(int tries = 0; tries < 3; tries++)
		{
			if(this->GetSlotSkill(lpObj, lpObj->OffComboStep) != 0)
			{
				return lpObj->OffComboStep;
			}

			lpObj->OffComboStep = (lpObj->OffComboStep >= 2) ? 0 : lpObj->OffComboStep + 1;
		}

		return 0;
	}

	int c1 = this->CheckActiveSkill(lpObj, 0);
	int c2 = this->CheckActiveSkill(lpObj, 1);

	if(c1 != -1 || c2 != -1)
	{
		return (c2 > c1) ? 2 : 1;
	}

	// Use Skills Closely: activation skills with no Delay/Con condition are
	// used too, in turn with the Basic Skill.
	if((lpObj->OffHelperFlags & 4) != 0)
	{
		for(int tries = 0; tries < 3; tries++)
		{
			lpObj->OffCloseTurn = (lpObj->OffCloseTurn + 1) % 3;
			const int slot = lpObj->OffCloseTurn;

			if(slot == 0)
			{
				return 0;
			}

			CSkill* lpSkill = this->GetSlotSkill(lpObj, slot);

			if(lpSkill != 0 && lpObj->OffActiveDelayOn[slot - 1] == 0 && lpObj->OffActiveConditionOn[slot - 1] == 0 && OffHelperIsDebuff(lpSkill) == 0)
			{
				return slot;
			}
		}
	}

	return 0;
}

// The monster to fight: within the helper Range (from the start spot, or from
// the character with Long Distance on). With Long Distance on, a monster
// attacking the character counts whatever the Range. Target Concentration
// picks the one with the most other monsters within 2 tiles; otherwise the
// closest.
int OfflineMode::SelectTarget(LPOBJ lpObj, int distance)
{
	const bool longDistance = (lpObj->DistanceLongOn != 0);
	const bool concentration = (lpObj->OffHelperFlags & 2) != 0;
	const int ox = longDistance ? lpObj->X : lpObj->m_OfflineCoordX;
	const int oy = longDistance ? lpObj->Y : lpObj->m_OfflineCoordY;

	int best = -1;
	int bestDist = 1000;
	int bestGroup = -1;

	for(int v = 0; v < MAX_VIEWPORT; v++)
	{
		if(lpObj->VpPlayer2[v].state == VIEWPORT_NONE || OBJECT_RANGE(lpObj->VpPlayer2[v].index) == 0 || lpObj->VpPlayer2[v].type != OBJECT_MONSTER)
		{
			continue;
		}

		LPOBJ lpMon = &gObj[lpObj->VpPlayer2[v].index];

		if(lpMon->Live == 0 || lpMon->State == OBJECT_EMPTY || lpMon->RegenType != 0 || lpMon->Map != lpObj->Map || gServerInfo.InSafeZone(lpMon->Index) == true)
		{
			continue;
		}

		if(gSkillManager.CheckSkillTarget(lpObj, lpMon->Index, -1, OBJECT_MONSTER) == 0)
		{
			continue;
		}

		// A wall between us (the monster is on the other side of it): not a target.
		// It could not be hit, and the helper would stand there trying.
		if(gMap[lpObj->Map].CheckWall(lpObj->X, lpObj->Y, lpMon->X, lpMon->Y) == FALSE)
		{
			continue;
		}

		const int d = gObjCalcDistance(lpObj, lpMon);
		const int fromSpot = (int)sqrt((float)((ox - lpMon->X) * (ox - lpMon->X) + (oy - lpMon->Y) * (oy - lpMon->Y)));
		const bool inRange = (fromSpot <= distance);
		const bool counter = longDistance && OffHelperMonsterAttackingMe(lpObj, lpMon) && d <= 12;

		if(inRange == false && counter == false)
		{
			continue;
		}

		int group = 0;

		if(concentration)
		{
			for(int w = 0; w < MAX_VIEWPORT; w++)
			{
				if(w == v || lpObj->VpPlayer2[w].state == VIEWPORT_NONE || OBJECT_RANGE(lpObj->VpPlayer2[w].index) == 0 || lpObj->VpPlayer2[w].type != OBJECT_MONSTER)
				{
					continue;
				}

				LPOBJ lpOther = &gObj[lpObj->VpPlayer2[w].index];

				if(lpOther->Live != 0 && gObjCalcDistance(lpMon, lpOther) <= 2)
				{
					group++;
				}
			}
		}

		const bool better = concentration ? (group > bestGroup || (group == bestGroup && d < bestDist)) : (d < bestDist);

		if(better)
		{
			best = lpMon->Index;
			bestDist = d;
			bestGroup = group;
		}
	}

	return best;
}

void OfflineMode::RenderAttack(int aIndex)	//-- INCOMPLETO
{
	if (!gObjIsConnectedGP(aIndex))
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];
	int caminar = 0;
	// Helper Range as set (was silently capped at 6); 15 = about the viewport.
	int distance = (lpObj->HuntingRange > 15) ? 15 : lpObj->HuntingRange;

	//-- SKILL ATTACK
	CSkill * SkillRender = 0;

	// Target first (Range, Long Distance counter-attack, Target Concentration),
	// then the skill for it - the same choices as the online helper.
	const int target = this->SelectTarget(lpObj, distance);

	if(target == -1)
	{
		return;
	}

	// slot 0 = basic skill, 1/2 = activation skills.
	int slot = this->SelectSkillSlot(lpObj, target);

	if(lpObj->RecoveryDrainOn != 0 && lpObj->Life < (( lpObj->MaxLife * lpObj->RecoveryDrainPercent) / 100))
	{
		SkillRender = gSkillManager.GetSkill(lpObj, SKILL_DRAIN_LIFE);
		slot = -1;
	}

	if(SkillRender == 0)
	{
		if(slot < 0) slot = 0;

		SkillRender = this->GetSlotSkill(lpObj, slot);

		if(SkillRender == 0 && slot != 0)
		{
			slot = 0;
			SkillRender = this->GetSlotSkill(lpObj, 0);
		}
	}


	// Use Regular Attack: no skill to use (none set or not learned, or not
	// enough mana and no potion) - a plain weapon hit, at the same speed rule.
	bool regular = (SkillRender == 0);

	if(SkillRender != 0 && lpObj->Mana < gSkillManager.GetSkillMana(SkillRender->m_index)
		&& gItemManager.GetInventoryItemSlot(lpObj, GET_ITEM(14,6), -1) < 0
		&& gItemManager.GetInventoryItemSlot(lpObj, GET_ITEM(14,5), -1) < 0
		&& gItemManager.GetInventoryItemSlot(lpObj, GET_ITEM(14,4), -1) < 0)
	{
		regular = true;
	}

	if(regular)
	{
		if((lpObj->OffHelperFlags & 1) == 0)
		{
			return;
		}

		LPOBJ lpTarget = &gObj[target];

		if(gObjCalcDistance(lpObj, lpTarget) > 1)
		{
			PATH_INFO path;

			if(gMap[lpObj->Map].PathFinding2(lpObj->X, lpObj->Y, lpTarget->X, lpTarget->Y, &path) != 0 && path.PathNum > 1)
			{
				const int step = (path.PathNum > 2) ? 2 : 1;
				AnimationMove(lpObj->Index, path.PathX[step], path.PathY[step]);
			}
			return;
		}

		const DWORD hitSpeed = (DWORD)lpObj->PhysiSpeed * 5;
		const DWORD hitDelay = (hitSpeed >= 1200) ? 300 : (1500 - hitSpeed);

		if((GetTickCount() - lpObj->OffRegularTick) >= hitDelay)
		{
			lpObj->OffRegularTick = GetTickCount();
			lpObj->Dir = GetPathPacketDirPos(lpTarget->X - lpObj->X, lpTarget->Y - lpObj->Y);
			gAttack.Attack(lpObj, lpTarget, 0, FALSE, 1, 0, FALSE, 0);
		}
		return;
	}

	int atacar = 0;

	int tObjNum = target;

	{
		
		atacar = 0;

		if (gObj[tObjNum].Live == 0 || gObj[tObjNum].State == OBJECT_EMPTY || gObj[tObjNum].RegenType != 0)
		{
			return;
		}

		if (OBJMAX_RANGE(tObjNum) == FALSE)
		{
			return;
		}

		if (gObj[tObjNum].Type != OBJECT_MONSTER)
		{
			return;
		}

		if (gServerInfo.InSafeZone(tObjNum) == true)
		{
			return;
		}

		// Out of the skill's reach: walk toward it, 2 tiles a step (it used to
		// jump straight onto the monster), and attack once there.
		if(gSkillManager.CheckSkillRange(SkillRender->m_index, lpObj->X, lpObj->Y, gObj[tObjNum].X, gObj[tObjNum].Y) == 0
			&& gSkillManager.CheckSkillRadio(SkillRender->m_index, lpObj->X, lpObj->Y, gObj[tObjNum].X, gObj[tObjNum].Y) == 0)
		{
			PATH_INFO path;

			if(gMap[lpObj->Map].PathFinding2(lpObj->X, lpObj->Y, gObj[tObjNum].X, gObj[tObjNum].Y, &path) != 0 && path.PathNum > 1)
			{
				const int step = (path.PathNum > 2) ? 2 : 1;
				AnimationMove(lpObj->Index, path.PathX[step], path.PathY[step]);
			}
			return;
		}

		atacar = 1;
		(void)caminar;

		if(atacar != 0)
		{
			if(lpObj->Mana < gSkillManager.GetSkillMana(SkillRender->m_index))
			{
				//-- AUTO POTION MP PRIMERO VALIDA QUE NO ESTE MUERTO
				PMSG_ITEM_USE_RECV pMsgMP;

				pMsgMP.header.set(0x26,sizeof(pMsgMP));

				pMsgMP.SourceSlot = 0xFF;

				pMsgMP.SourceSlot = ((pMsgMP.SourceSlot==0xFF) ? gItemManager.GetInventoryItemSlot(lpObj, GET_ITEM(14,6), -1) : pMsgMP.SourceSlot);

				pMsgMP.SourceSlot = ((pMsgMP.SourceSlot==0xFF) ? gItemManager.GetInventoryItemSlot(lpObj, GET_ITEM(14,5), -1) : pMsgMP.SourceSlot);

				pMsgMP.SourceSlot = ((pMsgMP.SourceSlot==0xFF) ? gItemManager.GetInventoryItemSlot(lpObj, GET_ITEM(14,4), -1) : pMsgMP.SourceSlot);

				pMsgMP.TargetSlot = 0xFF;

				pMsgMP.type = 0;

				if(INVENTORY_FULL_RANGE(pMsgMP.SourceSlot) != 0)
				{
					gItemManager.CGItemUseRecv(&pMsgMP,lpObj->Index);
				}
				else
				{
					return;
				}
			}

			// Time between casts, matched to how fast the ONLINE helper casts.
			// Online skill casts are not on the 1500 - 5 x speed timer (that one
			// is only for its regular attack); they are held back by the client:
			// - every attack locks the next for 14 AttackTime steps at 25/s
			//   (560 ms) and the helper polls every 100 ms, so ~600 ms at best;
			// - below that the attack animation sets the pace: PlaySpeed is
			//   0.25 + speed x 0.004 for weapon skills, 0.29 + magic speed x
			//   0.002 for spells (ZzzCharacter.cpp SetAttackSpeed), over ~10
			//   animation frames at 25/s.
			// AFK used 1500 - 5 x speed here, which made it slower than online
			// for anything under ~180 speed. Never slower than that old value.
			const bool castMagic = (lpObj->Class == CLASS_DW || lpObj->Class == CLASS_SU);
			const DWORD castSpeed = (DWORD)(castMagic ? lpObj->MagicSpeed : lpObj->PhysiSpeed);
			const double castPlay = castMagic ? (0.29 + castSpeed * 0.002) : (0.25 + castSpeed * 0.004);
			DWORD castDelay = (DWORD)(10.0 * 1000.0 / (25.0 * castPlay));
			if(castDelay < 600) castDelay = 600;
			const DWORD castBest = (DWORD)((lpObj->PhysiSpeed > lpObj->MagicSpeed) ? lpObj->PhysiSpeed : lpObj->MagicSpeed);
			const DWORD castOld = ((castBest * 5) >= 1200) ? 300 : (1500 - castBest * 5);
			if(castDelay > castOld) castDelay = castOld;

			if((GetTickCount()-((DWORD) lpObj->AttackCustomDelay )) >= castDelay)
			{
				lpObj->AttackCustomDelay = GetTickCount();

				if(slot >= 1)
				{
					lpObj->OffActiveTick[slot - 1] = GetTickCount();
				}

				if(lpObj->ComboOn != 0 && slot >= 0)
				{
					// Next combo step only after a cast really went out.
					lpObj->OffComboStep = (slot >= 2) ? 0 : slot + 1;
					lpObj->OffComboTick = GetTickCount();
				}

				if(SkillRender->m_skill != SKILL_FLAME 
					&& SkillRender->m_skill != SKILL_TWISTER 
					&& SkillRender->m_skill != SKILL_EVIL_SPIRIT 
					&& SkillRender->m_skill != SKILL_HELL_FIRE 
					&& SkillRender->m_skill != SKILL_AQUA_BEAM 
					&& SkillRender->m_skill != SKILL_BLAST 
					&& SkillRender->m_skill != SKILL_INFERNO 
					&& SkillRender->m_skill != SKILL_TRIPLE_SHOT 
					&& SkillRender->m_skill != SKILL_IMPALE
					&& SkillRender->m_skill != SKILL_MONSTER_AREA_ATTACK 
					&& SkillRender->m_skill != SKILL_PENETRATION 
					&& SkillRender->m_skill != SKILL_FIRE_SLASH 
					&& SkillRender->m_skill != SKILL_FIRE_SCREAM)
				{
					if(SkillRender->m_skill != SKILL_DARK_SIDE)
					{

						PMSG_DURATION_SKILL_ATTACK_RECV pMsg;

						pMsg.header.set(0x1E, sizeof(pMsg));

						#if(GAMESERVER_UPDATE>=701)

						pMsg.skillH = SET_NUMBERHB(0);

						pMsg.skillL = SET_NUMBERLB((((GetLargeRand()%100)>=25)?1:0));

						#else

						pMsg.skill[0] = SET_NUMBERHB(SkillRender->m_index);

						pMsg.skill[1] = SET_NUMBERLB(SkillRender->m_index);

						#endif

						pMsg.x = (BYTE)gObj[tObjNum].X;

						pMsg.y = (BYTE)gObj[tObjNum].Y;

						pMsg.dir = (gSkillManager.GetSkillAngle(gObj[tObjNum].X, gObj[tObjNum].Y, lpObj->X, lpObj->Y)*255) / 360;

						pMsg.dis = 0;

						pMsg.angle = (gSkillManager.GetSkillAngle(lpObj->X, lpObj->Y, gObj[tObjNum].X, gObj[tObjNum].Y)*255) / 360;

						#if(GAMESERVER_UPDATE>=803)

						pMsg.indexH = SET_NUMBERHB(lpObj->TargetNumber);

						pMsg.indexL = SET_NUMBERLB(lpObj->TargetNumber);

						#else

						pMsg.index[0] = SET_NUMBERHB(tObjNum);

						pMsg.index[1] = SET_NUMBERLB(tObjNum);

						#endif

						pMsg.MagicKey = 0;

						gSkillManager.CGDurationSkillAttackRecv(&pMsg,lpObj->Index);
						gAttack.Attack(lpObj, &gObj[tObjNum], SkillRender, FALSE, 1, 0, FALSE, 0);
					}
					else
					{
						gCustomAttack.SendRFSkillAttack(lpObj, tObjNum, SkillRender->m_index);
					}
				}
				else
				{
					gCustomAttack.SendMultiSkillAttack(lpObj, tObjNum, SkillRender->m_index);
				}
			}
		}
	}
}

bool isJewels(int index)
{
	if (index == GET_ITEM(12, 15) ||
		index == GET_ITEM(14, 13) ||
		index == GET_ITEM(14, 14) ||
		index == GET_ITEM(14, 16) ||
		index == GET_ITEM(14, 22) ||
		index == GET_ITEM(14, 31) ||
		index == GET_ITEM(14, 42))
	{
		return true;
	}

	return false;
}

// Add Extra Item list, same as the online helper (CB_MUHelper.cpp): each
// entry is looked for, case-sensitive, in "Name +Level Option Luck Skill
// Excellent Ancient", so "+5" and "Option" work as well as part of a name.
bool itemListPickUp(CItem* lpItem, LPOBJ lpObj)
{
	char text[160];
	_snprintf(text, sizeof(text) - 1, "%s", gItemLevel.GetItemName(lpItem->m_Index, lpItem->m_Level));
	text[sizeof(text) - 1] = 0;

	char extra[16];
	if(lpItem->m_Level > 0)
	{
		_snprintf(extra, sizeof(extra) - 1, " +%d", (int)lpItem->m_Level);
		extra[sizeof(extra) - 1] = 0;
		strncat(text, extra, sizeof(text) - strlen(text) - 1);
	}
	if(lpItem->m_Option3 != 0) strncat(text, " Option", sizeof(text) - strlen(text) - 1);
	if(lpItem->m_Option2 != 0) strncat(text, " Luck", sizeof(text) - strlen(text) - 1);
	if(lpItem->m_Option1 != 0) strncat(text, " Skill", sizeof(text) - strlen(text) - 1);
	if((lpItem->m_NewOption & 63) != 0) strncat(text, " Excellent", sizeof(text) - strlen(text) - 1);
	if(lpItem->m_SetOption != 0) strncat(text, " Ancient", sizeof(text) - strlen(text) - 1);

	for( int i = 0 ; i < lpObj->ObtainPickExtraCount && i < 12 ; i++ )
	{
		if( lpObj->ObtainPickItemList[ i ][0] != 0 && strstr(text, lpObj->ObtainPickItemList[ i ]) != NULL )
		{
			return true;
		}
	}
	return false;
}

int OfflineMode::PickUP(int aIndex)
{
	if (OBJMAX_RANGE(aIndex) == FALSE)
	{
		return 0;
	}

	if (!gObjIsConnectedGP(aIndex))
	{
		return 0;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if (lpObj->m_OfflineMode == 0 && lpObj->IsFakeOnline == 0 || lpObj->DieRegen != 0
		|| lpObj->Teleport != 0)
	{
		return 0;
	}

	if (gServerInfo.InSafeZone(aIndex) == true)
	{
		this->OnHelperInSafeZone(lpObj);
		return 0;
	}

	CMapItem * lpMapItem;

	int distance = lpObj->ObtainRange;

	int map_num = gObj[aIndex].Map;

	if (gObj[aIndex].DieRegen != 0)
	{
		return 0;
	}

	if (MAP_RANGE(map_num) == FALSE)
	{
		return 0;
	}

	int OldX;
	int OldY;
	bool PickItem;

	for (int n = 0; n < MAX_MAP_ITEM; n++)
	{
		PickItem = false;

		lpMapItem = &gMap[map_num].m_Item[n];

		if (lpMapItem->IsItem() == TRUE  && lpMapItem->m_Give == false && lpMapItem->m_Live == true)
		{
			int dis = (int) sqrt((float)((lpObj->X - lpMapItem->m_X) * (lpObj->X - lpMapItem->m_X) + (lpObj->Y - lpMapItem->m_Y) * (lpObj->Y - lpMapItem->m_Y)));

			if (dis > distance)
			{
				continue;
			}
			OldX = lpObj->X;
			OldY = lpObj->Y;
			// "Pick up all items nearby". It was received but never read here,
			// so with only that box ticked the AFK helper picked up nothing.
			if (lpObj->ObtainPickNear == 1)
			{
				PickItem = true;
				AnimationMove(lpObj->Index, lpMapItem->m_X, lpMapItem->m_Y);
			}
			else if (lpObj->ObtainPickSelected == 1)
			{
				if( (lpObj->ObtainPickMoney == 1 && lpMapItem->m_Index == GET_ITEM(14, 15)) 
					|| (lpObj->ObtainPickExcellent == 1 && lpMapItem->m_NewOption > 0) 
					|| (lpObj->ObtainPickAncient == 1 && lpMapItem->m_SetOption > 0) 
					|| (lpObj->ObtainPickJewels == 1 && isJewels(lpMapItem->m_Index) == true))
				{
					PickItem = true;
					AnimationMove(lpObj->Index, lpMapItem->m_X, lpMapItem->m_Y);
				}
				else if(lpObj->ObtainPickExtra == 1)
				{
					if( itemListPickUp(lpMapItem, lpObj) == true )
					{
						PickItem = true;
						AnimationMove(lpObj->Index, lpMapItem->m_X, lpMapItem->m_Y);
					}
				}
				else
				{
					PickItem = false;
					continue;
				}
			}
			else
			{
				PickItem = false;
				continue;
			}

			if (PickItem == false)
			{
				continue;
			}

			if(lpObj->X == lpMapItem->m_X && lpObj->Y == lpMapItem->m_Y)
			{
				//zen item
				if ( lpMapItem->m_Index == GET_ITEM(14, 15) )
				{
					if ( lpObj->ObtainPickMoney == 1 )
					{
						gMap[map_num].ItemGive(aIndex, n);

						if (!gObjCheckMaxMoney(aIndex, lpMapItem->m_BuyMoney))
						{
							if (lpObj->Money < MAX_MONEY)
							{
								lpObj->Money = MAX_MONEY;
								continue;
							}
						}
						else
						{
							lpObj->Money += lpMapItem->m_BuyMoney;
						}
						continue;
					}
				}
				//all items
				if (lpMapItem->m_QuestItem != false)
				{
					if (!gQuestObjective.CheckQuestObjectiveItemCount(lpObj, lpMapItem->m_Index, lpMapItem->m_Level))
					{
						continue;
					}
				}

				CItem item = (*lpMapItem);

				BYTE result = gItemManager.InventoryInsertItemStack(&gObj[aIndex], lpMapItem);
				if (result != 0xFF)
				{
					gMap[map_num].ItemGive(aIndex, n);
					{
						BYTE pos = gItemManager.InventoryInsertItem(aIndex, item);
						if (pos != 0xFF)
						{
							::GCPartyItemInfoSend(aIndex, lpMapItem);
						}
						
					}
				}
			}
		}
	}

	return 1;
}

void OfflineMode::regresar(int aIndex)
{
	if (OBJMAX_RANGE(aIndex) == FALSE)
	{
		return;
	}

	if (!gObjIsConnectedGP(aIndex))
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if (lpObj->m_OfflineMode == 0 && lpObj->IsFakeOnline == 0 || lpObj->DieRegen != 0
		|| lpObj->Teleport != 0)
	{
		return;
	}

	// Walking back after a death (ReturnToStartSpot): this snap-back would
	// jump him the rest of the way.
	if (lpObj->m_OfflineReturning != 0)
	{
		return;
	}

	if (gServerInfo.InSafeZone(aIndex) == true)
	{
		this->OnHelperInSafeZone(lpObj);
		return;
	}

	if(lpObj->DistanceReturnOn != 0)
	{
		if (GetTickCount() >= lpObj->m_OfflineTimeResetMove + 1000 + (lpObj->DistanceMin * 1000))
		{
			if(lpObj->m_OfflineCoordX != lpObj->X && lpObj->m_OfflineCoordY != lpObj->Y)
			{
				AnimationMove(lpObj->Index, lpObj->m_OfflineCoordX, lpObj->m_OfflineCoordY);
			}
			lpObj->m_OfflineTimeResetMove = GetTickCount();
		}
	}
}