// DuelRanked.cpp: Ranked 1v1 matchmaking for the Duel Arena, GameServer side.
//
// See DuelRanked.h and Working Project\MD Files\RankedDuel-PLAN.md.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "DuelRanked.h"
#include "CastleSiege.h"
#include "CommandManager.h"
#include "Duel.h"
#include "EffectManager.h"
#include "GameMain.h"
#include "GameMaster.h"
#include "Gate.h"
#include "Guild.h"
#include "JSProtocol.h"
#include "Map.h"
#include "MapServerManager.h"
#include "Message.h"
#include "Monster.h"
#include "Notice.h"
#include "CashShop.h"
#include "Party.h"
#include "ServerInfo.h"
#include "User.h"
#include "Util.h"
#include "Viewport.h"
#include "Warehouse.h"

CDuelRanked gDuelRanked;

namespace
{
	const DWORD kResultResendMs = 5000;
	const DWORD kResultKeepMs = 1800000;	// give up on an unanswered RESULT after 30 minutes
	const DWORD kCleanMs = 10000;			// the room stays in CLEAN (PK guard on) after a result
	const DWORD kCleanWaitMs = 15000;		// extra wait for the DataServer's answer before sending everyone home
	const DWORD kCleanDeadMs = 25000;		// extra wait for a dying fighter
	const DWORD kBeginMaxMs = 45000;		// placing the fighters on their spawn cells
	const DWORD kSpecWaitMs = 60000;		// a spectator that never became ready
	const DWORD kHomeWaitMs = 50000;		// how long a match-less arrival is waited for before being given up on (below kStrandMs, so the sweep never races it)
	const DWORD kIdleMs = 60000;			// no hit between the two fighters
	const DWORD kLingerMs = 8000;			// the record outlives LeaveArena until the fighters have left the map
	const DWORD kGoRepeatMs = 8000;
	const DWORD kGoKeepMs = 120000;
	const DWORD kReportMs = 5000;
	const DWORD kNoticeMs = 5000;
	const DWORD kStrandMs = 60000;
	const int kRoomRadius = 30;				// margin around a room's two spawn gates; a death respawn can land up to 18 cells away
	const BYTE kNoRoom = 0xFF;

	// dst is a char[11].
	void CopyName(char* dst,const char* src)
	{
		memset(dst,0,11);

		if(src != 0)
		{
			strncpy_s(dst,11,src,10);
		}
	}

	bool Elapsed(DWORD now,DWORD since,DWORD ms)
	{
		return ((DWORD)(now-since) >= ms);
	}

	BYTE ClampByte(int value)
	{
		return (BYTE)((value<0)?0:((value>255)?255:value));
	}

	WORD ClampWord(int value)
	{
		return (WORD)((value<0)?0:((value>65535)?65535:value));
	}
}

CDuelRanked::CDuelRanked() // OK
{
	this->m_Inbox.clear();

	this->m_SnapCount = 0;

	memset(this->m_Snap,0,sizeof(this->m_Snap));

	this->m_SecondTick = GetTickCount();

	this->m_SecCount = 0;

	this->m_SnapDirty = 0;

	memset(this->m_Match,0,sizeof(this->m_Match));

	memset(this->m_Spec,0,sizeof(this->m_Spec));

	memset(this->m_Result,0,sizeof(this->m_Result));

	memset(this->m_GoSeen,0,sizeof(this->m_GoSeen));

	memset(this->m_HomeWait,0,sizeof(this->m_HomeWait));

	memset(this->m_ViewTier,0,sizeof(this->m_ViewTier));

	memset(this->m_ViewDiv,0,sizeof(this->m_ViewDiv));

	this->m_LastReportTick = 0;

	memset(&this->m_LastReport,0,sizeof(this->m_LastReport));

	memset(this->m_LastReq,0,sizeof(this->m_LastReq));

	memset(this->m_StrandSince,0,sizeof(this->m_StrandSince));
}

CDuelRanked::~CDuelRanked() // OK
{

}

void CDuelRanked::MainProc() // OK
{
	this->DrainInbox();

	DWORD now = GetTickCount();

	if(Elapsed(now,this->m_SecondTick,1000) != 0)
	{
		this->m_SecondTick = now;

		this->Tick1s();
	}

	if(this->m_SnapDirty != 0)
	{
		this->PublishSnap();
	}
}

bool CDuelRanked::IsHost() // OK
{
	return (gServerInfo.m_ServerCode == gServerInfo.m_DuelRankedHostServer);
}

// The arena can take ranked fights: switch on, map 64 served here and, on the
// Castle Siege build, no siege coming up or running.
bool CDuelRanked::HostAccepting() // OK
{
	if(gServerInfo.m_DuelRankedSwitch == 0)
	{
		return 0;
	}

	if(gMapServerManager.CheckMapServer(MAP_DUEL_ARENA) == 0)
	{
		return 0;
	}

	#if(GAMESERVER_TYPE==1)

	if(gCastleSiege.GetCastleState() == CASTLESIEGE_STATE_NOTIFY || gCastleSiege.GetCastleState() == CASTLESIEGE_STATE_READYSIEGE || gCastleSiege.GetCastleState() == CASTLESIEGE_STATE_STARTSIEGE)
	{
		return 0;
	}

	#endif

	return 1;
}

//////////////////////////////////////////////////////////////////////
// Lookups
//////////////////////////////////////////////////////////////////////

// closing: also an object that is being closed (its scores are still readable).
int CDuelRanked::FindUser(char* name,char* account,bool closing) // OK
{
	if(name == 0 || name[0] == 0)
	{
		return -1;
	}

	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		LPOBJ lpObj = &gObj[n];

		if(lpObj->Connected != OBJECT_ONLINE || lpObj->Type != OBJECT_USER)
		{
			continue;
		}

		// An object that is closing (kicked, or hopping to another server) is already gone.
		if(closing == 0 && (lpObj->CloseCount > 0 || lpObj->MapServerMoveQuit != 0))
		{
			continue;
		}

		if(strcmp(lpObj->Name,name) != 0)
		{
			continue;
		}

		// Account names are not case sensitive in the database, so a login typed in
		// another case must still be found.
		if(account != 0 && account[0] != 0 && _stricmp(lpObj->Account,account) != 0)
		{
			continue;
		}

		return n;
	}

	return -1;
}

bool CDuelRanked::IsReady(LPOBJ lpObj) // OK
{
	if(lpObj->Connected != OBJECT_ONLINE || lpObj->Type != OBJECT_USER)
	{
		return 0;
	}

	if(lpObj->State != OBJECT_PLAYING || lpObj->Live == 0 || lpObj->DieRegen != 0)
	{
		return 0;
	}

	if(lpObj->RegenOk != 0 || lpObj->Teleport != 0)
	{
		return 0;
	}

	if(lpObj->CloseCount > 0 || lpObj->MapServerMoveQuit != 0)
	{
		return 0;
	}

	return 1;
}

DR_HOSTMATCH* CDuelRanked::FindMatch(DWORD matchId) // OK
{
	if(matchId == 0)
	{
		return 0;
	}

	for(int n=0;n < DR_MAX_MATCH;n++)
	{
		if(this->m_Match[n].Used != 0 && this->m_Match[n].MatchId == matchId)
		{
			return &this->m_Match[n];
		}
	}

	return 0;
}

DR_HOSTMATCH* CDuelRanked::NewMatch(DWORD matchId) // OK
{
	for(int n=0;n < DR_MAX_MATCH;n++)
	{
		if(this->m_Match[n].Used == 0)
		{
			DR_HOSTMATCH* rec = &this->m_Match[n];

			memset(rec,0,sizeof(DR_HOSTMATCH));

			rec->Used = 1;
			rec->State = DRH_STAGING;
			rec->Room = kNoRoom;
			rec->MatchId = matchId;
			rec->StateTick = GetTickCount();
			rec->F[0].Index = -1;
			rec->F[1].Index = -1;
			rec->ResumeSlot = 0xFF;

			return rec;
		}
	}

	return 0;
}

int CDuelRanked::SlotOf(DR_HOSTMATCH* rec,char* name) // OK
{
	for(int n=0;n < 2;n++)
	{
		if(rec->F[n].HaveExpect != 0 && strcmp(rec->F[n].Name,name) == 0)
		{
			return n;
		}
	}

	return -1;
}

// Is this player one of the fighters of a ranked match on this host?
bool CDuelRanked::IsParticipant(char* name) // OK
{
	if(this->m_SnapCount == 0 || name == 0 || name[0] == 0)
	{
		return 0;
	}

	bool result = 0;

	this->m_SnapLock.lock();

	for(int n=0;n < this->m_SnapCount;n++)
	{
		if(this->m_Snap[n].Role == DR_ROLE_FIGHTER && strcmp(this->m_Snap[n].Name,name) == 0)
		{
			result = 1;
			break;
		}
	}

	this->m_SnapLock.unlock();

	return result;
}

// A fighter waiting out the reconnect pause (ObjectManager skips the relocation to Lorencia for it).
bool CDuelRanked::IsPausedFighter(char* name) // OK
{
	if(this->m_SnapCount == 0 || name == 0 || name[0] == 0)
	{
		return 0;
	}

	bool result = 0;

	this->m_SnapLock.lock();

	for(int n=0;n < this->m_SnapCount;n++)
	{
		if(this->m_Snap[n].Role == DR_ROLE_FIGHTER && this->m_Snap[n].State == DRH_PAUSED && this->m_Snap[n].Absent != 0 && strcmp(this->m_Snap[n].Name,name) == 0)
		{
			result = 1;
			break;
		}
	}

	this->m_SnapLock.unlock();

	return result;
}

// The PK hole: once a duel is reset the two are ordinary players on map 64 (NonPK 0).
// Anyone taking part in a ranked match is untouchable except by the opponent of a live fight.
bool CDuelRanked::CheckProtectedTarget(LPOBJ lpObj,LPOBJ lpTarget) // OK
{
	if(this->m_SnapCount == 0)
	{
		return 0;
	}

	if(DA_MAP_RANGE(lpObj->Map) == 0 && DA_MAP_RANGE(lpTarget->Map) == 0)
	{
		return 0;
	}

	if(this->IsParticipant(lpObj->Name) == 0 && this->IsParticipant(lpTarget->Name) == 0)
	{
		return 0;
	}

	return ((gDuel.CheckDuel(lpObj,lpTarget)==0)?1:0);
}

void CDuelRanked::PublishSnap() // OK
{
	this->m_SnapDirty = 0;

	DR_SNAP snap[DR_MAX_SNAP];

	int count = 0;

	for(int n=0;n < DR_MAX_MATCH;n++)
	{
		DR_HOSTMATCH* rec = &this->m_Match[n];

		if(rec->Used == 0)
		{
			continue;
		}

		for(int s=0;s < 2;s++)
		{
			if(rec->F[s].HaveExpect == 0 || count >= DR_MAX_SNAP)
			{
				continue;
			}

			memset(&snap[count],0,sizeof(DR_SNAP));

			CopyName(snap[count].Name,rec->F[s].Name);

			snap[count].MatchId = rec->MatchId;
			snap[count].Role = DR_ROLE_FIGHTER;
			snap[count].State = ((rec->Settled!=0)?DRH_CLEAN:rec->State);
			snap[count].Absent = rec->F[s].Absent;

			count++;
		}
	}

	for(int n=0;n < DR_MAX_SPEC;n++)
	{
		if(this->m_Spec[n].Used == 0 || count >= DR_MAX_SNAP)
		{
			continue;
		}

		memset(&snap[count],0,sizeof(DR_SNAP));

		CopyName(snap[count].Name,this->m_Spec[n].Name);

		snap[count].MatchId = this->m_Spec[n].MatchId;
		snap[count].Role = DR_ROLE_SPECTATOR;
		snap[count].State = this->m_Spec[n].State;

		count++;
	}

	this->m_SnapLock.lock();

	memcpy(this->m_Snap,snap,sizeof(DR_SNAP)*count);

	this->m_SnapCount = count;

	this->m_SnapLock.unlock();
}

//////////////////////////////////////////////////////////////////////
// Senders
//////////////////////////////////////////////////////////////////////

bool CDuelRanked::SendReq(int aIndex,BYTE op,DWORD matchId,BYTE room,WORD level) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return 0;
	}

	SDHP_DUELRANKED_REQ pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0xD9,0x51,sizeof(pMsg));

	pMsg.aIndex = (WORD)aIndex;

	CopyName(pMsg.Account,gObj[aIndex].Account);

	CopyName(pMsg.Name,gObj[aIndex].Name);

	pMsg.Op = op;

	pMsg.Level = level;

	pMsg.MatchId = matchId;

	pMsg.Room = room;

	return gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CDuelRanked::SendCantGo(char* name,char* account,DWORD matchId) // OK
{
	SDHP_DUELRANKED_REQ pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0xD9,0x51,sizeof(pMsg));

	pMsg.aIndex = 0;

	CopyName(pMsg.Account,account);

	CopyName(pMsg.Name,name);

	pMsg.Op = DUELRANKED_OP_CANTGO;

	pMsg.MatchId = matchId;

	pMsg.Room = kNoRoom;

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);

	LogAdd(LOG_RED,"[DuelRanked] %s cannot be moved for match %d",name,(int)matchId);
}

void CDuelRanked::SendDepart(LPOBJ lpObj,DWORD matchId,BYTE kind) // OK
{
	SDHP_DUELRANKED_DEPART pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0xD9,0x53,sizeof(pMsg));

	CopyName(pMsg.Name,lpObj->Name);

	CopyName(pMsg.Account,lpObj->Account);

	pMsg.MatchId = matchId;

	pMsg.Kind = kind;

	pMsg.OriginServer = (WORD)gServerInfo.m_ServerCode;

	pMsg.OriginMap = (BYTE)lpObj->Map;

	pMsg.OriginX = (BYTE)lpObj->X;

	pMsg.OriginY = (BYTE)lpObj->Y;

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CDuelRanked::SendHostEvent(BYTE ev,DWORD matchId,BYTE room,char* name) // OK
{
	SDHP_DUELRANKED_HOSTEVENT pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0xD9,0x54,sizeof(pMsg));

	pMsg.MatchId = matchId;

	pMsg.Event = ev;

	pMsg.Room = room;

	CopyName(pMsg.Name,name);

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CDuelRanked::SendClientMsg(int aIndex,BYTE code,BYTE p1) // OK
{
	if(gObjIsConnected(aIndex) == 0)
	{
		return;
	}

	PMSG_DUELRANKED_MSG_SEND pMsg;

	pMsg.header.set(0xAA,0x13,sizeof(pMsg));

	pMsg.Code = code;

	pMsg.P1 = p1;

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

bool CDuelRanked::Throttle(int aIndex,int kind,DWORD ms) // OK
{
	if(OBJECT_RANGE(aIndex) == 0 || kind < 0 || kind > 3)
	{
		return 0;
	}

	DWORD now = GetTickCount();

	if((DWORD)(now-this->m_LastReq[kind][aIndex]) < ms)
	{
		return 0;
	}

	this->m_LastReq[kind][aIndex] = now;

	return 1;
}

//////////////////////////////////////////////////////////////////////
// Origin side: client requests
//////////////////////////////////////////////////////////////////////

// 0 = fine, otherwise the MSG code to show the player. mode: 0 JOIN, 1 WATCH,
// 2 ACCEPT, 3 GO for a fighter, 4 GO for a spectator, 5 GO to resume (the character
// reconnected somewhere else and may still be saved on the arena map).
int CDuelRanked::CheckOrigin(LPOBJ lpObj,int mode,BYTE* p1) // OK
{
	(*p1) = 0;

	if(gServerInfo.m_DuelRankedSwitch == 0)
	{
		return DUELRANKED_MSG_RANKED_DISABLED;
	}

	if((mode == 0 || mode == 2 || mode == 3) && lpObj->Level < gServerInfo.m_DuelRankedMinLevel)
	{
		(*p1) = ClampByte(gServerInfo.m_DuelRankedMinLevel);
		return DUELRANKED_MSG_LEVEL_TOO_LOW;
	}

	if(lpObj->Live == 0 || lpObj->DieRegen != 0)
	{
		return DUELRANKED_MSG_NOT_ALLOWED_NOW;
	}

	if(lpObj->m_OfflineMode != 0 || lpObj->IsFakeOnline != 0)
	{
		return DUELRANKED_MSG_NOT_ALLOWED_NOW;
	}

	if(lpObj->PShopOpen != 0 || (lpObj->Interface.use != 0 && (lpObj->Interface.type == INTERFACE_TRADE || lpObj->Interface.type == INTERFACE_PERSONAL_SHOP)))
	{
		return DUELRANKED_MSG_NOT_ALLOWED_NOW;
	}

	if(lpObj->HelperDelayTime != 0)
	{
		return DUELRANKED_MSG_HELPER_BLOCK;
	}

	if(OBJECT_RANGE(lpObj->PartyNumber) != 0 && gParty.IsParty(lpObj->PartyNumber) != 0)
	{
		return DUELRANKED_MSG_PARTY_BLOCK;
	}

	if(lpObj->Guild != 0 && lpObj->Guild->WarState == 1)
	{
		return DUELRANKED_MSG_NOT_ALLOWED_NOW;
	}

	for(int n=0;n < MAX_SELF_DEFENSE;n++)
	{
		if(lpObj->SelfDefense[n] >= 0)
		{
			return DUELRANKED_MSG_NOT_ALLOWED_NOW;
		}
	}

	if(CA_MAP_RANGE(lpObj->Map) != 0 || DS_MAP_RANGE(lpObj->Map) != 0 || BC_MAP_RANGE(lpObj->Map) != 0 || CC_MAP_RANGE(lpObj->Map) != 0 || IT_MAP_RANGE(lpObj->Map) != 0 || (DA_MAP_RANGE(lpObj->Map) != 0 && mode != 5) || DG_MAP_RANGE(lpObj->Map) != 0 || IG_MAP_RANGE(lpObj->Map) != 0)
	{
		return DUELRANKED_MSG_NOT_ALLOWED_NOW;
	}

	if(OBJECT_RANGE(lpObj->DuelUser) != 0 || OBJECT_RANGE(lpObj->DuelUserReserved) != 0 || OBJECT_RANGE(lpObj->DuelUserRequested) != 0)
	{
		return DUELRANKED_MSG_NOT_ALLOWED_NOW;
	}

	// The DataServer is the authority once it orders a move (GO).
	if(mode < 3 && (gDuel.GetDuelArenaBySpectator(lpObj->Index) != 0 || this->IsParticipant(lpObj->Name) != 0))
	{
		return DUELRANKED_MSG_IN_MATCH;
	}

	#if(GAMESERVER_TYPE==1)

	if(gCastleSiege.GetCastleState() == CASTLESIEGE_STATE_NOTIFY || gCastleSiege.GetCastleState() == CASTLESIEGE_STATE_READYSIEGE || gCastleSiege.GetCastleState() == CASTLESIEGE_STATE_STARTSIEGE)
	{
		return DUELRANKED_MSG_ARENA_UNAVAILABLE;
	}

	#endif

	if(mode == 0 || mode == 1)
	{
		// The Titus window (INTERFACE_COMMON) has to be open to queue or to pick a fight.
		if(lpObj->Interface.use == 0 || lpObj->Interface.type != INTERFACE_COMMON)
		{
			return DUELRANKED_MSG_OPEN_TITUS_FIRST;
		}
	}
	else if(mode >= 3 && lpObj->Interface.use != 0)
	{
		// The order to move: the Titus window and the plain NPC windows are simply
		// closed (HandleGo). A window with items in flight (trade, shop, chaos box,
		// cash shop, ...) is not: the hop would run CharacterGameClose on it.
		switch(lpObj->Interface.type)
		{
			case INTERFACE_NONE:
			case INTERFACE_COMMON:
			case INTERFACE_PARTY:
			case INTERFACE_SHOP:
			case INTERFACE_GUILD:
			case INTERFACE_GUILD_CREATE:
			case INTERFACE_WAREHOUSE:
			case INTERFACE_QUEST:
			case INTERFACE_QUEST_WORLD:
				break;
			default:
				return DUELRANKED_MSG_NOT_ALLOWED_NOW;
		}
	}

	return 0;
}

void CDuelRanked::CGStatusRecv(int aIndex) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	if(gServerInfo.m_DuelRankedSwitch == 0)
	{
		this->SendClientMsg(aIndex,DUELRANKED_MSG_RANKED_DISABLED,0);
		return;
	}

	if(this->Throttle(aIndex,0,500) == 0)
	{
		return;
	}

	this->SendReq(aIndex,DUELRANKED_OP_STATUS,0,0,0);
}

void CDuelRanked::CGQueueRecv(PMSG_DUELRANKED_QUEUE_RECV* lpMsg,int size,int aIndex) // OK
{
	if(size < (int)sizeof(PMSG_DUELRANKED_QUEUE_RECV) || gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpMsg->On == 0)
	{
		this->SendReq(aIndex,DUELRANKED_OP_LEAVE,0,0,0);
		return;
	}

	if(this->Throttle(aIndex,1,1000) == 0)
	{
		return;
	}

	BYTE p1 = 0;

	int code = this->CheckOrigin(lpObj,0,&p1);

	if(code != 0)
	{
		this->SendClientMsg(aIndex,(BYTE)code,p1);
		return;
	}

	if(gDataServerConnection.CheckState() == 0 || this->SendReq(aIndex,DUELRANKED_OP_JOIN,0,0,(WORD)lpObj->Level) == 0)
	{
		this->SendClientMsg(aIndex,DUELRANKED_MSG_ARENA_UNAVAILABLE,0);
	}
}

void CDuelRanked::CGAcceptRecv(PMSG_DUELRANKED_ACCEPT_RECV* lpMsg,int size,int aIndex) // OK
{
	if(size < (int)sizeof(PMSG_DUELRANKED_ACCEPT_RECV) || gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpMsg->Flag == 0)
	{
		this->SendReq(aIndex,DUELRANKED_OP_DECLINE,lpMsg->MatchId,0,0);
		return;
	}

	BYTE p1 = 0;

	int code = this->CheckOrigin(lpObj,2,&p1);

	if(code != 0)
	{
		// Blocked: tell the player, nothing goes to the DataServer (the prompt
		// can still be accepted once the block is gone, or it times out).
		this->SendClientMsg(aIndex,(BYTE)code,p1);
		return;
	}

	if(this->SendReq(aIndex,DUELRANKED_OP_ACCEPT,lpMsg->MatchId,0,0) == 0)
	{
		this->SendClientMsg(aIndex,DUELRANKED_MSG_ARENA_UNAVAILABLE,0);
	}
}

void CDuelRanked::CGListRecv(int aIndex) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	if(gServerInfo.m_DuelRankedSwitch == 0)
	{
		return;
	}

	if(this->Throttle(aIndex,2,1000) == 0)
	{
		return;
	}

	this->SendReq(aIndex,DUELRANKED_OP_LIST,0,0,0);
}

void CDuelRanked::CGWatchRecv(PMSG_DUELRANKED_WATCH_RECV* lpMsg,int size,int aIndex) // OK
{
	if(size < (int)sizeof(PMSG_DUELRANKED_WATCH_RECV) || gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(this->Throttle(aIndex,3,1000) == 0)
	{
		return;
	}

	BYTE p1 = 0;

	int code = this->CheckOrigin(lpObj,1,&p1);

	if(code != 0)
	{
		this->SendClientMsg(aIndex,(BYTE)code,p1);
		return;
	}

	if(gDataServerConnection.CheckState() == 0 || this->SendReq(aIndex,DUELRANKED_OP_WATCH,lpMsg->MatchId,lpMsg->Room,0) == 0)
	{
		this->SendClientMsg(aIndex,DUELRANKED_MSG_ARENA_UNAVAILABLE,0);
	}
}

//////////////////////////////////////////////////////////////////////
// Inbox: the DataServer's replies and the events other threads raise
//////////////////////////////////////////////////////////////////////

void CDuelRanked::DGRecv(BYTE* lpMsg,int size) // OK
{
	if(size < 4 || size > 255 || lpMsg[0] != 0xC1)
	{
		return;
	}

	BYTE sub = lpMsg[3];

	int need = 0;

	switch(sub)
	{
		case 0x51:
			need = sizeof(SDHP_DUELRANKED_STATUS);
			break;
		case 0x52:
			need = sizeof(SDHP_DUELRANKED_PROMPT);
			break;
		case 0x53:
			need = sizeof(SDHP_DUELRANKED_GO);
			break;
		case 0x54:
			need = sizeof(SDHP_DUELRANKED_EXPECT);
			break;
		case 0x55:
			need = sizeof(SDHP_DUELRANKED_APPLIED);
			break;
		case 0x56:
			need = DUELRANKED_LIST_BASE;
			break;
		case 0x57:
			need = sizeof(SDHP_DUELRANKED_MSG);
			break;
		case 0x58:
			need = sizeof(SDHP_DUELRANKED_MATCHCANCEL);
			break;
		case 0x59:
			need = sizeof(SDHP_DUELRANKED_RANK);
			break;
		default:
			return;
	}

	if(size < need)
	{
		return;
	}

	if(sub == 0x56)
	{
		int count = lpMsg[17];

		if(count > 4 || size < (DUELRANKED_LIST_BASE+count*(int)sizeof(DUELRANKED_LISTENTRY)))
		{
			return;
		}
	}

	DR_INBOX item;

	item.Kind = DR_IN_DS;
	item.Sub = sub;
	item.Size = (WORD)size;

	memcpy(item.Data,lpMsg,size);

	this->Push(&item);
}

void CDuelRanked::Push(const DR_INBOX* item) // OK
{
	this->m_InLock.lock();

	if(this->m_Inbox.size() < DR_MAX_INBOX)
	{
		this->m_Inbox.push_back(*item);
	}

	this->m_InLock.unlock();
}

void CDuelRanked::DrainInbox() // OK
{
	std::vector<DR_INBOX> list;

	this->m_InLock.lock();

	if(this->m_Inbox.empty() == 0)
	{
		list.swap(this->m_Inbox);
	}

	this->m_InLock.unlock();

	for(size_t n=0;n < list.size();n++)
	{
		DR_INBOX* item = &list[n];

		switch(item->Kind)
		{
			case DR_IN_DS:
				this->HandleDS(item->Sub,item->Data,item->Size);
				break;
			case DR_IN_CLOSE:
				this->HandleClose((DR_EVT_CLOSE*)item->Data);
				break;
			case DR_IN_SCOREWIN:
				this->HandleScoreWin((DR_EVT_SCOREWIN*)item->Data);
				break;
			case DR_IN_SPECLEAVE:
				this->HandleSpecLeave((DR_EVT_SPEC*)item->Data);
				break;
		}
	}
}

void CDuelRanked::HandleDS(BYTE sub,BYTE* data,int size) // OK
{
	switch(sub)
	{
		case 0x51:
			this->ForwardStatus((SDHP_DUELRANKED_STATUS*)data);
			break;
		case 0x52:
			this->ForwardPrompt((SDHP_DUELRANKED_PROMPT*)data);
			break;
		case 0x53:
			this->HandleGo((SDHP_DUELRANKED_GO*)data);
			break;
		case 0x54:
			this->HandleExpect((SDHP_DUELRANKED_EXPECT*)data);
			break;
		case 0x55:
			this->HandleApplied((SDHP_DUELRANKED_APPLIED*)data);
			break;
		case 0x56:
			this->ForwardList((SDHP_DUELRANKED_LIST*)data);
			break;
		case 0x57:
			this->ForwardMsg((SDHP_DUELRANKED_MSG*)data);
			break;
		case 0x58:
			this->HandleCancel((SDHP_DUELRANKED_MATCHCANCEL*)data);
			break;
		case 0x59:
			this->HandleRank((SDHP_DUELRANKED_RANK*)data);
			break;
	}
}

//////////////////////////////////////////////////////////////////////
// DataServer -> client
//////////////////////////////////////////////////////////////////////

void CDuelRanked::ForwardStatus(SDHP_DUELRANKED_STATUS* p) // OK
{
	char account[11];

	CopyName(account,p->Account);

	if(gObjIsAccountValid(p->aIndex,account) == 0 || gObjIsConnected(p->aIndex) == 0)
	{
		return;
	}

	PMSG_DUELRANKED_STATUS_SEND pMsg;

	pMsg.header.set(0xAA,0x10,sizeof(pMsg));

	pMsg.Mmr = p->Mmr;
	pMsg.Tier = p->Tier;
	pMsg.Div = p->Div;
	pMsg.StepLo = p->StepLo;
	pMsg.StepNext = p->StepNext;
	pMsg.Wins = p->Wins;
	pMsg.Losses = p->Losses;
	pMsg.State = p->State;
	pMsg.QueuedSec = p->QueuedSec;
	pMsg.Accepting = p->Accepting;

	DataSend(p->aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

// DataServer 0x59: a character's rank changed (login, or a rated match was applied).
void CDuelRanked::HandleRank(SDHP_DUELRANKED_RANK* p)
{
	if(gServerInfo.m_DuelRankedSwitch == 0)
	{
		return;
	}

	char name[11];
	char account[11];

	CopyName(name,p->Name);
	CopyName(account,p->Account);

	int aIndex = this->FindUser(name,account);

	LogAdd(LOG_BLACK,"[DuelRanked] rank icon for %s: played %d tier %d div %d, object %d",name,(int)p->Played,(int)p->Tier,(int)p->Div,aIndex);

	if(aIndex < 0)
	{
		return;
	}

	BYTE tier = 0;
	BYTE div = 0;

	if(p->Played != 0)
	{
		tier = (BYTE)(p->Tier+1);
		div = p->Div;
	}

	if(this->m_ViewTier[aIndex] == tier && this->m_ViewDiv[aIndex] == div)
	{
		return;
	}

	this->m_ViewTier[aIndex] = tier;
	this->m_ViewDiv[aIndex] = div;

	this->SendRankToAll(aIndex,1);
}

void CDuelRanked::SendRankToAll(int aIndex,bool force)
{
	if(gServerInfo.m_DuelRankedSwitch == 0 || OBJECT_RANGE(aIndex) == 0 || gObjIsConnected(aIndex) == 0 || (force == 0 && this->m_ViewTier[aIndex] == 0))
	{
		return;
	}

	BYTE send[sizeof(PMSG_DUELRANKED_VIEW_SEND)+sizeof(PMSG_DUELRANKED_VIEW_ENTRY)];

	PMSG_DUELRANKED_VIEW_SEND pMsg;

	pMsg.header.set(0xD3,0xC4,sizeof(send));

	pMsg.Count = 1;

	PMSG_DUELRANKED_VIEW_ENTRY info;

	info.Index = (WORD)aIndex;
	info.Tier = this->m_ViewTier[aIndex];
	info.Div = this->m_ViewDiv[aIndex];

	memcpy(send,&pMsg,sizeof(pMsg));
	memcpy(&send[sizeof(pMsg)],&info,sizeof(info));

	DataSend(aIndex,send,sizeof(send));

	MsgSendV2(&gObj[aIndex],send,sizeof(send));
}

// Called from the viewport code right after the new players were sent to aIndex:
// the ranked ones among them, plus aIndex itself, in packets of at most DUELRANKED_VIEW_MAX.
void CDuelRanked::SendRanksToViewer(int aIndex)
{
	if(gServerInfo.m_DuelRankedSwitch == 0 || OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	PMSG_DUELRANKED_VIEW_ENTRY list[DUELRANKED_VIEW_MAX];

	int count = 0;

	for(int n=-1;n < MAX_VIEWPORT;n++)
	{
		int target = aIndex;

		if(n >= 0)
		{
			if(lpObj->VpPlayer[n].state != VIEWPORT_SEND || lpObj->VpPlayer[n].type != OBJECT_USER)
			{
				continue;
			}

			target = lpObj->VpPlayer[n].index;

			if(OBJECT_RANGE(target) == 0)
			{
				continue;
			}
		}

		if(this->m_ViewTier[target] == 0)
		{
			continue;
		}

		list[count].Index = (WORD)target;
		list[count].Tier = this->m_ViewTier[target];
		list[count].Div = this->m_ViewDiv[target];

		count++;

		if(count >= DUELRANKED_VIEW_MAX)
		{
			BYTE send[sizeof(PMSG_DUELRANKED_VIEW_SEND)+sizeof(list)];

			PMSG_DUELRANKED_VIEW_SEND pMsg;

			pMsg.header.set(0xD3,0xC4,sizeof(PMSG_DUELRANKED_VIEW_SEND)+count*sizeof(PMSG_DUELRANKED_VIEW_ENTRY));

			pMsg.Count = (BYTE)count;

			memcpy(send,&pMsg,sizeof(pMsg));
			memcpy(&send[sizeof(pMsg)],list,count*sizeof(PMSG_DUELRANKED_VIEW_ENTRY));

			DataSend(aIndex,send,pMsg.header.size);

			count = 0;
		}
	}

	if(count > 0)
	{
		BYTE send[sizeof(PMSG_DUELRANKED_VIEW_SEND)+sizeof(list)];

		PMSG_DUELRANKED_VIEW_SEND pMsg;

		pMsg.header.set(0xD3,0xC4,sizeof(PMSG_DUELRANKED_VIEW_SEND)+count*sizeof(PMSG_DUELRANKED_VIEW_ENTRY));

		pMsg.Count = (BYTE)count;

		memcpy(send,&pMsg,sizeof(pMsg));
		memcpy(&send[sizeof(pMsg)],list,count*sizeof(PMSG_DUELRANKED_VIEW_ENTRY));

		DataSend(aIndex,send,pMsg.header.size);
	}
}

void CDuelRanked::ForwardPrompt(SDHP_DUELRANKED_PROMPT* p) // OK
{
	char account[11];

	CopyName(account,p->Account);

	if(gObjIsAccountValid(p->aIndex,account) == 0 || gObjIsConnected(p->aIndex) == 0)
	{
		return;
	}

	PMSG_DUELRANKED_PROMPT_SEND pMsg;

	pMsg.header.set(0xAA,0x11,sizeof(pMsg));

	pMsg.MatchId = p->MatchId;

	memset(pMsg.Opp,0,sizeof(pMsg.Opp));

	memcpy(pMsg.Opp,p->Opp,sizeof(pMsg.Opp));

	pMsg.OppMmr = p->OppMmr;
	pMsg.OppTier = p->OppTier;
	pMsg.OppDiv = p->OppDiv;
	pMsg.SecLeft = p->Sec;

	DataSend(p->aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CDuelRanked::ForwardList(SDHP_DUELRANKED_LIST* p) // OK
{
	char account[11];

	CopyName(account,p->Account);

	if(gObjIsAccountValid(p->aIndex,account) == 0 || gObjIsConnected(p->aIndex) == 0)
	{
		return;
	}

	BYTE send[256];

	PMSG_DUELRANKED_LIST_SEND pMsg;

	pMsg.header.set(0xAA,0x12,0);

	int size = sizeof(pMsg);

	pMsg.Count = 0;

	for(int n=0;n < p->Count && n < 4;n++)
	{
		DUELRANKED_LISTENTRY* in = &p->e[n];

		PMSG_DUELRANKED_LIST_ENTRY out;

		out.MatchId = in->MatchId;
		out.Room = in->Room;

		memcpy(out.N1,in->Name1,sizeof(out.N1));
		memcpy(out.N2,in->Name2,sizeof(out.N2));

		out.Mmr1 = in->Mmr1;
		out.Mmr2 = in->Mmr2;
		out.Tier1 = in->Tier1;
		out.Div1 = in->Div1;
		out.Tier2 = in->Tier2;
		out.Div2 = in->Div2;
		out.S1 = in->Score1;
		out.S2 = in->Score2;
		out.RemainSec = in->RemainSec;

		memcpy(&send[size],&out,sizeof(out));

		size += sizeof(out);

		pMsg.Count++;
	}

	pMsg.header.size = (BYTE)size;

	memcpy(send,&pMsg,sizeof(pMsg));

	DataSend(p->aIndex,send,size);
}

void CDuelRanked::ForwardMsg(SDHP_DUELRANKED_MSG* p) // OK
{
	char account[11];

	CopyName(account,p->Account);

	if(gObjIsAccountValid(p->aIndex,account) == 0)
	{
		return;
	}

	if(p->Code == DUELRANKED_MSG_RANK_REWARD)
	{
		// Told here as a plain notice (the client has no text for this code), and
		// the coin display is refreshed: the DataServer already added the coins.
		gNotice.GCNoticeSend(p->aIndex,1,0,0,0,0,0,"Ranked: new rank reached! Your rank-up reward was added to your account.");
		gCashShop.CGCashShopPointRecv(p->aIndex);
		return;
	}

	this->SendClientMsg(p->aIndex,p->Code,p->P1);
}

//////////////////////////////////////////////////////////////////////
// Origin side: GO
//////////////////////////////////////////////////////////////////////

DWORD CDuelRanked::GoLast(char* name,DWORD matchId,BYTE kind) // OK
{
	for(int n=0;n < DR_MAX_GOSEEN;n++)
	{
		DR_GOSEEN* e = &this->m_GoSeen[n];

		if(e->Used != 0 && e->MatchId == matchId && e->Kind == kind && strcmp(e->Name,name) == 0)
		{
			return ((e->Tick==0)?1:e->Tick);
		}
	}

	return 0;
}

void CDuelRanked::GoMark(char* name,DWORD matchId,BYTE kind) // OK
{
	DR_GOSEEN* slot = 0;

	for(int n=0;n < DR_MAX_GOSEEN;n++)
	{
		DR_GOSEEN* e = &this->m_GoSeen[n];

		if(e->Used != 0 && e->MatchId == matchId && e->Kind == kind && strcmp(e->Name,name) == 0)
		{
			slot = e;
			break;
		}

		if(e->Used == 0 && slot == 0)
		{
			slot = e;
		}
	}

	if(slot == 0)
	{
		// Table full: reuse the oldest.
		DWORD now = GetTickCount();

		slot = &this->m_GoSeen[0];

		for(int n=1;n < DR_MAX_GOSEEN;n++)
		{
			if((DWORD)(now-this->m_GoSeen[n].Tick) > (DWORD)(now-slot->Tick))
			{
				slot = &this->m_GoSeen[n];
			}
		}
	}

	slot->Used = 1;
	slot->Kind = kind;
	slot->MatchId = matchId;
	slot->Tick = GetTickCount();

	CopyName(slot->Name,name);
}

bool CDuelRanked::HopToHost(LPOBJ lpObj,WORD hostServer) // OK
{
	int gate,map,x,y,dir,level;

	if(gGate.GetGate(gServerInfo.m_DuelRankedStageGate,&gate,&map,&x,&y,&dir,&level) == 0 || map != MAP_DUEL_ARENA)
	{
		return 0;
	}

	char ip[16] = {0};

	WORD port = 0;

	if(gMapServerManager.GetMapServerData(hostServer,ip,&port) == 0)
	{
		return 0;
	}

	if(lpObj->m_OfflineMode == 0)
	{
		lpObj->State = OBJECT_DELCMD;
	}

	// Servers 0-3 serve map 64 themselves, so CheckMapServerMove would keep the player
	// here: the hop to the host is ordered directly.
	GJMapServerMoveSend(lpObj->Index,hostServer,MAP_DUEL_ARENA,(BYTE)x,(BYTE)y);

	return 1;
}

void CDuelRanked::HandleGo(SDHP_DUELRANKED_GO* p) // OK
{
	char name[11];
	char account[11];

	CopyName(name,p->Name);
	CopyName(account,p->Account);

	if(p->Kind > DUELRANKED_KIND_RESUME)
	{
		return;
	}

	DWORD now = GetTickCount();

	DWORD last = this->GoLast(name,p->MatchId,p->Kind);

	int aIndex = this->FindUser(name,account);

	if(aIndex < 0)
	{
		// Already handled (the player is on its way, or the same order came twice): stay quiet.
		if(last != 0)
		{
			return;
		}

		this->SendCantGo(name,account,p->MatchId);
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->MapServerMoveQuit != 0)
	{
		return;
	}

	if(last != 0 && (DWORD)(now-last) < kGoRepeatMs)
	{
		return;
	}

	if(p->Kind == DUELRANKED_KIND_RESUME && gServerInfo.m_ServerCode == p->HostServer)
	{
		return;
	}

	// Already part of a match here (a repeated order): nothing to do.
	if(p->Kind != DUELRANKED_KIND_RESUME && this->IsParticipant(name) != 0)
	{
		return;
	}

	if(lpObj->CloseCount > 0)
	{
		this->SendCantGo(name,account,p->MatchId);
		return;
	}

	BYTE p1 = 0;

	int code = this->CheckOrigin(lpObj,((p->Kind==DUELRANKED_KIND_SPECTATOR)?4:((p->Kind==DUELRANKED_KIND_RESUME)?5:3)),&p1);

	if(code != 0)
	{
		this->SendClientMsg(aIndex,(BYTE)code,p1);
		this->SendCantGo(name,account,p->MatchId);
		return;
	}

	if(lpObj->Interface.use != 0 && lpObj->Interface.type == INTERFACE_WAREHOUSE)
	{
		gWarehouse.CGWarehouseClose(aIndex);
	}

	lpObj->Interface.use = 0;
	lpObj->Interface.type = INTERFACE_NONE;
	lpObj->Interface.state = 0;

	if(p->Kind != DUELRANKED_KIND_RESUME)
	{
		this->SendDepart(lpObj,p->MatchId,p->Kind);
	}

	this->GoMark(name,p->MatchId,p->Kind);

	if(gServerInfo.m_ServerCode == p->HostServer)
	{
		// This is the host: nothing to move, the host's tables take over.
		return;
	}

	this->SendClientMsg(aIndex,DUELRANKED_MSG_MOVING_TO_ARENA,0);

	if(this->HopToHost(lpObj,p->HostServer) == 0)
	{
		this->SendCantGo(name,account,p->MatchId);
	}
}

//////////////////////////////////////////////////////////////////////
// Host: the DataServer's orders
//////////////////////////////////////////////////////////////////////

void CDuelRanked::HandleExpect(SDHP_DUELRANKED_EXPECT* p) // OK
{
	char name[11];
	char account[11];

	CopyName(name,p->Name);
	CopyName(account,p->Account);

	if(this->IsHost() == 0)
	{
		LogAdd(LOG_RED,"[DuelRanked] EXPECT for match %d received but this server (%d) is not the host (%d)",(int)p->MatchId,(int)gServerInfo.m_ServerCode,(int)gServerInfo.m_DuelRankedHostServer);
		return;
	}

	if(p->Kind == DUELRANKED_KIND_SPECTATOR)
	{
		DR_SPEC* e = 0;

		for(int n=0;n < DR_MAX_SPEC;n++)
		{
			if(this->m_Spec[n].Used != 0 && strcmp(this->m_Spec[n].Name,name) == 0)
			{
				e = &this->m_Spec[n];
				break;
			}
		}

		if(e == 0)
		{
			for(int n=0;n < DR_MAX_SPEC;n++)
			{
				if(this->m_Spec[n].Used == 0)
				{
					e = &this->m_Spec[n];
					break;
				}
			}
		}

		if(e == 0)
		{
			// No place for him, but he is already on his way: he goes straight back.
			this->AddHomeWait(name,account,p->OriginServer,p->OriginMap,p->OriginX,p->OriginY);

			this->SendHostEvent(DUELRANKED_EVENT_SPECTATOR_LEFT,p->MatchId,p->Room,name);
			return;
		}

		this->DropHomeWait(name);

		memset(e,0,sizeof(DR_SPEC));

		e->Used = 1;
		e->State = 1;
		e->Room = p->Room;
		e->MatchId = p->MatchId;

		CopyName(e->Name,name);
		CopyName(e->Account,account);

		e->OriginServer = p->OriginServer;
		e->OriginMap = p->OriginMap;
		e->OriginX = p->OriginX;
		e->OriginY = p->OriginY;
		e->Index = -1;
		e->AddedTick = GetTickCount();

		this->m_SnapDirty = 1;

		return;
	}

	if(p->Slot > 1 || this->HostAccepting() == 0)
	{
		// The fighter left his origin when this was sent, so he is landing here with
		// no record to send him back: remember where he came from.
		this->AddHomeWait(name,account,p->OriginServer,p->OriginMap,p->OriginX,p->OriginY);

		this->SendHostEvent(DUELRANKED_EVENT_ABORT,p->MatchId,kNoRoom,"");
		return;
	}

	DR_HOSTMATCH* rec = this->FindMatch(p->MatchId);

	if(rec == 0)
	{
		rec = this->NewMatch(p->MatchId);

		if(rec == 0)
		{
			LogAdd(LOG_RED,"[DuelRanked] no room in the match table for match %d",(int)p->MatchId);

			this->AddHomeWait(name,account,p->OriginServer,p->OriginMap,p->OriginX,p->OriginY);

			this->SendHostEvent(DUELRANKED_EVENT_ABORT,p->MatchId,kNoRoom,"");
			return;
		}

		LogAdd(LOG_BLUE,"[DuelRanked] match %d expected: %s vs %s",(int)p->MatchId,name,p->Opp);
	}

	DR_FIGHTER* f = &rec->F[p->Slot];

	if(rec->State != DRH_STAGING && f->HaveExpect != 0)
	{
		return;
	}

	// He is part of a match again: a note that sends him home no longer applies.
	this->DropHomeWait(name);

	CopyName(f->Name,name);
	CopyName(f->Account,account);
	CopyName(f->Opp,p->Opp);

	f->Mmr = p->Mmr;
	f->Tier = p->Tier;
	f->Div = p->Div;
	f->OriginServer = p->OriginServer;
	f->OriginMap = p->OriginMap;
	f->OriginX = p->OriginX;
	f->OriginY = p->OriginY;
	f->HaveExpect = 1;

	this->m_SnapDirty = 1;
}

void CDuelRanked::HandleApplied(SDHP_DUELRANKED_APPLIED* p) // OK
{
	DR_RESULT* res = 0;

	for(int n=0;n < DR_MAX_RESULT;n++)
	{
		if(this->m_Result[n].Used != 0 && this->m_Result[n].Msg.MatchId == p->MatchId)
		{
			res = &this->m_Result[n];
			break;
		}
	}

	if(res != 0)
	{
		res->Used = 0;
	}

	DR_HOSTMATCH* rec = this->FindMatch(p->MatchId);

	if(rec == 0 || rec->ResultShown != 0)
	{
		return;
	}

	char winner[11];

	CopyName(winner,p->Winner);

	if(winner[0] == 0)
	{
		// Voided or unknown to the DataServer: nothing was rated.
		rec->ResultShown = 1;

		for(int s=0;s < 2;s++)
		{
			int aIndex = this->FindUser(rec->F[s].Name,rec->F[s].Account);

			if(aIndex >= 0)
			{
				this->SendClientMsg(aIndex,DUELRANKED_MSG_MATCH_VOID,0);
			}
		}

		return;
	}

	rec->ResultShown = 1;

	for(int s=0;s < 2;s++)
	{
		int aIndex = this->FindUser(rec->F[s].Name,rec->F[s].Account);

		if(aIndex < 0)
		{
			continue;
		}

		bool isWinnerSide = (_stricmp(rec->F[s].Name,winner) == 0);

		PMSG_DUELRANKED_RESULT_SEND pMsg;

		pMsg.header.set(0xAA,0x14,sizeof(pMsg));

		if(p->IsDraw != 0)
		{
			pMsg.Outcome = 2;
		}
		else
		{
			pMsg.Outcome = ((isWinnerSide)?0:1);
		}

		pMsg.OldMmr = ((isWinnerSide)?p->OldMmrW:p->OldMmrL);
		pMsg.NewMmr = ((isWinnerSide)?p->NewMmrW:p->NewMmrL);
		pMsg.Delta = ((isWinnerSide)?p->DeltaW:p->DeltaL);
		pMsg.OldTier = ((isWinnerSide)?p->OldTierW:p->OldTierL);
		pMsg.OldDiv = ((isWinnerSide)?p->OldDivW:p->OldDivL);
		pMsg.NewTier = ((isWinnerSide)?p->NewTierW:p->NewTierL);
		pMsg.NewDiv = ((isWinnerSide)?p->NewDivW:p->NewDivL);

		DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
	}
}

void CDuelRanked::HandleCancel(SDHP_DUELRANKED_MATCHCANCEL* p) // OK
{
	DR_HOSTMATCH* rec = this->FindMatch(p->MatchId);

	if(rec == 0)
	{
		return;
	}

	LogAdd(LOG_RED,"[DuelRanked] match %d cancelled by the DataServer (reason %d)",(int)p->MatchId,(int)p->Reason);

	if(rec->Settled != 0)
	{
		// Already decided: the result keeps going to the DataServer, which answers it.
		return;
	}

	if(rec->State == DRH_STAGING)
	{
		// Whoever was announced goes back: the ones here at the next tick, the ones
		// still hopping when they land.
		this->SendFightersHome(rec);

		this->DropSpectatorsOf(rec->MatchId);

		rec->Used = 0;

		this->m_SnapDirty = 1;

		return;
	}

	this->EndVoid(rec,"cancelled by the DataServer",0,"");
}

//////////////////////////////////////////////////////////////////////
// Events raised by other threads
//////////////////////////////////////////////////////////////////////

// CharacterGameClose, before the duel reset block. A fighter of a live ranked
// fight is not allowed to reset the opponent's duel: the host pauses instead.
bool CDuelRanked::OnCharacterClose(LPOBJ lpObj) // OK
{
	if(OBJECT_RANGE(lpObj->Index) != 0)
	{
		// The index is reused: the next character on it starts without an icon.
		this->m_ViewTier[lpObj->Index] = 0;
		this->m_ViewDiv[lpObj->Index] = 0;
	}

	if(this->m_SnapCount == 0 || lpObj->Name[0] == 0)
	{
		return 0;
	}

	DWORD matchId = 0;

	bool found = 0;

	this->m_SnapLock.lock();

	for(int n=0;n < this->m_SnapCount;n++)
	{
		if(this->m_Snap[n].Role != DR_ROLE_FIGHTER || strcmp(this->m_Snap[n].Name,lpObj->Name) != 0)
		{
			continue;
		}

		if(this->m_Snap[n].State == DRH_BEGIN || this->m_Snap[n].State == DRH_FIGHT || this->m_Snap[n].State == DRH_PAUSED)
		{
			matchId = this->m_Snap[n].MatchId;
			found = 1;
		}

		break;
	}

	this->m_SnapLock.unlock();

	if(found == 0)
	{
		return 0;
	}

	DR_EVT_CLOSE evt;

	memset(&evt,0,sizeof(evt));

	evt.MatchId = matchId;

	CopyName(evt.Name,lpObj->Name);

	int bIndex = lpObj->DuelUser;

	if(OBJECT_RANGE(bIndex) != 0 && gObj[bIndex].Connected == OBJECT_ONLINE && gObj[bIndex].DuelUser == lpObj->Index)
	{
		evt.HaveScores = 1;
		evt.ScoreSelf = lpObj->DuelScore;
		evt.ScoreOther = gObj[bIndex].DuelScore;

		// The opponent must not be reset: it keeps its place in the room.
		gObj[bIndex].DuelUser = -1;
	}

	lpObj->DuelUser = -1;

	DR_INBOX item;

	item.Kind = DR_IN_CLOSE;
	item.Sub = 0;
	item.Size = sizeof(evt);

	memcpy(item.Data,&evt,sizeof(evt));

	this->Push(&item);

	return 1;
}

// CDuel::RankedScoreWin: a fighter reached the winning score.
void CDuelRanked::OnScoreWin(DWORD matchId,char* winner,char* loser,BYTE scoreW,BYTE scoreL) // OK
{
	DR_EVT_SCOREWIN evt;

	memset(&evt,0,sizeof(evt));

	evt.MatchId = matchId;

	CopyName(evt.Winner,winner);
	CopyName(evt.Loser,loser);

	evt.ScoreW = scoreW;
	evt.ScoreL = scoreL;

	DR_INBOX item;

	item.Kind = DR_IN_SCOREWIN;
	item.Sub = 0;
	item.Size = sizeof(evt);

	memcpy(item.Data,&evt,sizeof(evt));

	this->Push(&item);
}

// AA:09 from a spectator of a ranked room.
void CDuelRanked::OnSpectatorLeave(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	DR_EVT_SPEC evt;

	memset(&evt,0,sizeof(evt));

	CopyName(evt.Name,gObj[aIndex].Name);

	DR_INBOX item;

	item.Kind = DR_IN_SPECLEAVE;
	item.Sub = 0;
	item.Size = sizeof(evt);

	memcpy(item.Data,&evt,sizeof(evt));

	this->Push(&item);
}

void CDuelRanked::HandleClose(DR_EVT_CLOSE* e) // OK
{
	DR_HOSTMATCH* rec = this->FindMatch(e->MatchId);

	if(rec == 0 || rec->Settled != 0)
	{
		return;
	}

	int slot = this->SlotOf(rec,e->Name);

	if(slot < 0)
	{
		return;
	}

	if(e->HaveScores != 0)
	{
		rec->F[slot].Score = e->ScoreSelf;
		rec->F[1-slot].Score = e->ScoreOther;
	}

	switch(rec->State)
	{
		case DRH_FIGHT:
			this->PauseFighter(rec,slot);
			break;
		case DRH_PAUSED:
			if(rec->F[slot].Absent == 0)
			{
				this->EndVoid(rec,"the present fighter logged out during the pause",1,rec->F[slot].Name);
			}
			break;
		case DRH_BEGIN:
			if(rec->Resuming != 0 && rec->ResumeSlot == slot)
			{
				// The returning fighter dropped again while being placed.
				rec->State = DRH_PAUSED;
				rec->Resuming = 0;
				rec->F[slot].ReadyCount = 0;
				this->m_SnapDirty = 1;
			}
			else
			{
				this->AbortBegin(rec,slot);
			}
			break;
	}
}

void CDuelRanked::HandleScoreWin(DR_EVT_SCOREWIN* e) // OK
{
	DR_HOSTMATCH* rec = this->FindMatch(e->MatchId);

	if(rec == 0 || rec->Settled != 0)
	{
		return;
	}

	int ws = this->SlotOf(rec,e->Winner);
	int ls = this->SlotOf(rec,e->Loser);

	if(ws < 0 || ls < 0 || ws == ls)
	{
		return;
	}

	rec->F[ws].Score = e->ScoreW;
	rec->F[ls].Score = e->ScoreL;

	this->Settle(rec,ws,DUELRANKED_REASON_SCORE,0);
}

void CDuelRanked::HandleSpecLeave(DR_EVT_SPEC* e) // OK
{
	for(int n=0;n < DR_MAX_SPEC;n++)
	{
		if(this->m_Spec[n].Used != 0 && strcmp(this->m_Spec[n].Name,e->Name) == 0)
		{
			this->RemoveSpectator(&this->m_Spec[n],1);
			return;
		}
	}

	// A local player watching a ranked room through the normal Titus list.
	int aIndex = this->FindUser(e->Name,0);

	if(aIndex < 0)
	{
		return;
	}

	DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaBySpectator(aIndex);

	if(lpInfo == 0)
	{
		return;
	}

	gEffectManager.DelEffect(&gObj[aIndex],EFFECT_INVISIBILITY);

	gEffectManager.DelEffect(&gObj[aIndex],EFFECT_DUEL_ARENA_WATCH);

	gDuel.GCDuelSpectatorDelSend(aIndex,lpInfo);

	gDuel.DelDuelArenaSpectator(lpInfo,aIndex);

	this->SendHome(aIndex);
}

//////////////////////////////////////////////////////////////////////
// Going home
//////////////////////////////////////////////////////////////////////

void CDuelRanked::SendHomeRecord(int aIndex,WORD originServer,BYTE originMap,BYTE x,BYTE y) // OK
{
	if(gObjIsConnected(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	bool ok = 0;

	// An origin that is a closed event map (or unknown) falls back to gate 17.
	if(MAP_RANGE(originMap) != 0 && originMap != MAP_DUEL_ARENA && CA_MAP_RANGE(originMap) == 0 && DS_MAP_RANGE(originMap) == 0 && BC_MAP_RANGE(originMap) == 0 && CC_MAP_RANGE(originMap) == 0 && IT_MAP_RANGE(originMap) == 0 && DG_MAP_RANGE(originMap) == 0 && IG_MAP_RANGE(originMap) == 0)
	{
		short next = gMapServerManager.CheckMapServerMove(aIndex,originMap,lpObj->LastServerCode);

		ok = ((next==-1)?0:1);
	}

	if(ok != 0)
	{
		// gObjTeleport hops back through LastServerCode when the origin map is served elsewhere.
		gObjTeleport(aIndex,originMap,x,y);
	}
	else
	{
		gObjMoveGate(aIndex,17);
	}
}

// Anyone leaving the arena for good: back to where they came from, gate 17 when unknown.
void CDuelRanked::SendHome(int aIndex) // OK
{
	if(gObjIsConnected(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	for(int n=0;n < DR_MAX_SPEC;n++)
	{
		DR_SPEC* e = &this->m_Spec[n];

		if(e->Used != 0 && strcmp(e->Name,lpObj->Name) == 0)
		{
			this->SendHomeRecord(aIndex,e->OriginServer,e->OriginMap,e->OriginX,e->OriginY);
			return;
		}
	}

	for(int n=0;n < DR_MAX_MATCH;n++)
	{
		DR_HOSTMATCH* rec = &this->m_Match[n];

		if(rec->Used == 0)
		{
			continue;
		}

		int slot = this->SlotOf(rec,lpObj->Name);

		if(slot >= 0)
		{
			this->SendHomeRecord(aIndex,rec->F[slot].OriginServer,rec->F[slot].OriginMap,rec->F[slot].OriginX,rec->F[slot].OriginY);
			return;
		}
	}

	gObjMoveGate(aIndex,17);
}

//////////////////////////////////////////////////////////////////////
// Host: the per second work
//////////////////////////////////////////////////////////////////////

void CDuelRanked::Tick1s() // OK
{
	this->m_SecCount++;

	DWORD now = GetTickCount();

	for(int n=0;n < DR_MAX_GOSEEN;n++)
	{
		if(this->m_GoSeen[n].Used != 0 && Elapsed(now,this->m_GoSeen[n].Tick,kGoKeepMs) != 0)
		{
			this->m_GoSeen[n].Used = 0;
		}
	}

	this->ProcResults();

	for(int n=0;n < DR_MAX_MATCH;n++)
	{
		DR_HOSTMATCH* rec = &this->m_Match[n];

		if(rec->Used == 0)
		{
			continue;
		}

		switch(rec->State)
		{
			case DRH_STAGING:
				this->ProcStaging(rec);
				break;
			case DRH_BEGIN:
				this->ProcBegin(rec);
				break;
			case DRH_FIGHT:
			case DRH_PAUSED:
				this->ProcFight(rec);
				break;
			case DRH_CLEAN:
				this->ProcClean(rec);
				break;
		}
	}

	this->ProcSpectators();

	this->ProcHomeWait();

	if(this->IsHost() != 0)
	{
		this->Sweep();

		this->SendHostReport();
	}
}

void CDuelRanked::ProcStaging(DR_HOSTMATCH* rec) // OK
{
	DWORD now = GetTickCount();

	bool ready = 1;

	for(int s=0;s < 2;s++)
	{
		DR_FIGHTER* f = &rec->F[s];

		if(f->HaveExpect == 0)
		{
			ready = 0;
			continue;
		}

		int aIndex = this->FindUser(f->Name,f->Account);

		f->Index = aIndex;

		if(aIndex < 0)
		{
			f->ReadyCount = 0;
			ready = 0;
			continue;
		}

		if(f->Arrived == 0)
		{
			f->Arrived = 1;

			this->SendHostEvent(DUELRANKED_EVENT_ARRIVED,rec->MatchId,kNoRoom,f->Name);
		}

		if(this->IsReady(&gObj[aIndex]) != 0)
		{
			f->ReadyCount = ((f->ReadyCount<255)?(f->ReadyCount+1):255);
		}
		else
		{
			f->ReadyCount = 0;
		}

		if(f->ReadyCount < gServerInfo.m_DuelRankedReadyHoldSec)
		{
			ready = 0;
		}
	}

	if(ready != 0)
	{
		if(this->HostAccepting() == 0)
		{
			this->AbortStaging(rec,0);
			return;
		}

		if(this->TryBegin(rec) != 0)
		{
			return;
		}
	}

	if(Elapsed(now,rec->StateTick,gServerInfo.m_DuelRankedNoShowSec*1000) != 0)
	{
		this->AbortStaging(rec,1);
	}
}

void CDuelRanked::AbortStaging(DR_HOSTMATCH* rec,bool blame) // OK
{
	char who[11] = {0};

	int missing = 0;

	for(int s=0;s < 2;s++)
	{
		if(rec->F[s].Arrived == 0)
		{
			missing++;

			// A fighter whose origin never announced him has no name here, but the other
			// fighter's EXPECT named him as the opponent.
			CopyName(who,((rec->F[s].HaveExpect!=0)?rec->F[s].Name:rec->F[1-s].Opp));
		}
	}

	if(missing != 1 || blame == 0)
	{
		who[0] = 0;
	}

	LogAdd(LOG_RED,"[DuelRanked] match %d: the fighters were not all ready in time (%s)",(int)rec->MatchId,((who[0]==0)?"no one to blame":who));

	this->SendHostEvent(DUELRANKED_EVENT_ABORT,rec->MatchId,kNoRoom,who);

	// The fighters here go home now; one who has not landed yet goes when he does.
	this->SendFightersHome(rec);

	this->DropSpectatorsOf(rec->MatchId);

	rec->Used = 0;

	this->m_SnapDirty = 1;
}

bool CDuelRanked::TryBegin(DR_HOSTMATCH* rec) // OK
{
	int ranked = 0;

	for(int n=0;n < MAX_DUEL_ARENA;n++)
	{
		DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaByIndex(n);

		if(lpInfo != 0 && lpInfo->Ranked != 0)
		{
			ranked++;
		}
	}

	if(ranked >= gServerInfo.m_DuelRankedMaxConcurrent)
	{
		return 0;
	}

	int aIndex = this->FindUser(rec->F[0].Name,rec->F[0].Account);
	int bIndex = this->FindUser(rec->F[1].Name,rec->F[1].Account);

	if(aIndex < 0 || bIndex < 0 || this->IsReady(&gObj[aIndex]) == 0 || this->IsReady(&gObj[bIndex]) == 0)
	{
		return 0;
	}

	int room = gDuel.BeginDuel(&gObj[aIndex],&gObj[bIndex],rec->MatchId);

	if(room < 0)
	{
		return 0;
	}

	DWORD now = GetTickCount();

	rec->Room = (BYTE)room;
	rec->State = DRH_BEGIN;
	rec->BeginTick = now;
	rec->StateTick = now;
	rec->RemainSec = gServerInfo.m_DuelRankedMatchSec;
	rec->Resuming = 0;
	rec->F[0].Score = 0;
	rec->F[1].Score = 0;
	rec->F[0].Index = aIndex;
	rec->F[1].Index = bIndex;

	LogAdd(LOG_BLUE,"[DuelRanked] match %d: %s vs %s in room %d",(int)rec->MatchId,rec->F[0].Name,rec->F[1].Name,room);

	this->m_SnapDirty = 1;

	if(this->PlaceFighters(rec) != 0)
	{
		this->FinishBegin(rec);
	}

	return 1;
}

bool CDuelRanked::PlacedAt(LPOBJ lpObj,int gate) // OK
{
	GATE_INFO info;

	if(gGate.GetInfo(gate,&info) == 0)
	{
		return 1;
	}

	if(lpObj->Map != info.Map)
	{
		return 0;
	}

	return (abs(lpObj->X-info.X) <= 1 && abs(lpObj->Y-info.Y) <= 1);
}

// Both fighters on their spawn cells of the room? Moves the ones that are not
// (gObjMoveGate silently refuses while RegenOk != 0, so this is retried every second).
bool CDuelRanked::PlaceFighters(DR_HOSTMATCH* rec) // OK
{
	bool placed = 1;

	for(int s=0;s < 2;s++)
	{
		int aIndex = this->FindUser(rec->F[s].Name,rec->F[s].Account);

		if(aIndex < 0)
		{
			placed = 0;
			continue;
		}

		rec->F[s].Index = aIndex;

		LPOBJ lpObj = &gObj[aIndex];

		int gate = 295+(rec->Room*2)+s;

		if(this->PlacedAt(lpObj,gate) == 0)
		{
			if(this->IsReady(lpObj) != 0)
			{
				lpObj->Interface.use = 0;
				lpObj->Interface.type = INTERFACE_NONE;
				lpObj->Interface.state = 0;

				gObjMoveGate(aIndex,gate);
			}
		}

		if(this->PlacedAt(lpObj,gate) == 0)
		{
			placed = 0;
		}
	}

	return placed;
}

void CDuelRanked::ProcBegin(DR_HOSTMATCH* rec) // OK
{
	DWORD now = GetTickCount();

	for(int s=0;s < 2;s++)
	{
		if(this->FindUser(rec->F[s].Name,rec->F[s].Account) < 0)
		{
			if(rec->Resuming != 0 && rec->ResumeSlot == s)
			{
				rec->State = DRH_PAUSED;
				rec->Resuming = 0;
				rec->F[s].ReadyCount = 0;
				this->m_SnapDirty = 1;
			}
			else
			{
				this->AbortBegin(rec,s);
			}

			return;
		}
	}

	if(Elapsed(now,rec->BeginTick,kBeginMaxMs) != 0)
	{
		if(rec->Resuming != 0)
		{
			// Could not place them: fall back to the pause, whose grace timer decides.
			rec->State = DRH_PAUSED;
			rec->Resuming = 0;
			rec->F[rec->ResumeSlot].ReadyCount = 0;
			this->m_SnapDirty = 1;
		}
		else
		{
			this->AbortBegin(rec,-1);
		}

		return;
	}

	if(this->PlaceFighters(rec) != 0)
	{
		this->FinishBegin(rec);
	}
}

void CDuelRanked::AbortBegin(DR_HOSTMATCH* rec,int slot) // OK
{
	this->EndVoid(rec,"the fighters could not be placed",1,((slot>=0)?rec->F[slot].Name:(char*)""));
}

// Both fighters are on their spawn cells: the post-block of CGDuelOkRecv.
void CDuelRanked::FinishBegin(DR_HOSTMATCH* rec) // OK
{
	DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaByIndex(rec->Room);

	int aIndex = this->FindUser(rec->F[0].Name,rec->F[0].Account);
	int bIndex = this->FindUser(rec->F[1].Name,rec->F[1].Account);

	if(lpInfo == 0 || aIndex < 0 || bIndex < 0)
	{
		return;
	}

	LPOBJ lpA = &gObj[aIndex];
	LPOBJ lpB = &gObj[bIndex];

	lpInfo->Gladiator[0] = aIndex;
	lpInfo->Gladiator[1] = bIndex;

	LPOBJ list[2] = {lpA,lpB};

	for(int s=0;s < 2;s++)
	{
		gEffectManager.ClearAllEffect(list[s]);

		list[s]->Life = list[s]->MaxLife+list[s]->AddLife;
		list[s]->Mana = list[s]->MaxMana+list[s]->AddMana;
		list[s]->BP = list[s]->MaxBP+list[s]->AddBP;
		list[s]->Shield = list[s]->MaxShield+list[s]->AddShield;

		GCLifeSend(list[s]->Index,0xFF,(int)list[s]->Life,list[s]->Shield);

		GCManaSend(list[s]->Index,0xFF,(int)list[s]->Mana,list[s]->BP);

		rec->F[s].Index = list[s]->Index;
		rec->F[s].ReadyCount = 0;
	}

	bool resume = (rec->Resuming != 0);

	gDuel.StartRankedFight(lpInfo,lpA,lpB,rec->F[0].Score,rec->F[1].Score,((resume==0)?1:0));

	rec->State = DRH_FIGHT;
	rec->StateTick = GetTickCount();
	rec->ClockTick = rec->StateTick;
	rec->Resuming = 0;

	gDuel.GCDuelScoreSend(lpInfo);

	gDuel.GCDuelInterfaceSend(lpInfo);

	if(resume == 0)
	{
		rec->StartedSent = 1;

		this->SendHostEvent(DUELRANKED_EVENT_STARTED,rec->MatchId,rec->Room,"");

		this->SendClientMsg(aIndex,DUELRANKED_MSG_FIGHT_STARTING,0);

		this->SendClientMsg(bIndex,DUELRANKED_MSG_FIGHT_STARTING,0);

		LogAdd(LOG_GREEN,"[DuelRanked] match %d started in room %d",(int)rec->MatchId,(int)rec->Room);
	}
	else
	{
		int back = rec->ResumeSlot;

		rec->F[back].Absent = 0;

		this->SendHostEvent(DUELRANKED_EVENT_RESUMED,rec->MatchId,rec->Room,rec->F[back].Name);

		this->SendClientMsg(((back==0)?aIndex:bIndex),DUELRANKED_MSG_RESUMING,0);

		this->SendClientMsg(((back==0)?bIndex:aIndex),DUELRANKED_MSG_OPPONENT_RETURNED,0);

		this->NoticeSpectators(rec,DUELRANKED_MSG_OPPONENT_RETURNED,0);

		LogAdd(LOG_GREEN,"[DuelRanked] match %d resumed (%s is back)",(int)rec->MatchId,rec->F[back].Name);
	}

	rec->ResumeSlot = 0xFF;

	this->m_SnapDirty = 1;
}

// The arena pockets of map 64 are walled off from each other, so a box around the
// room's two spawn gates is enough to tell "still in my room" from "somewhere else".
bool CDuelRanked::InRoomArea(int room,int x,int y) // OK
{
	GATE_INFO g1;
	GATE_INFO g2;

	if(gGate.GetInfo(295+(room*2),&g1) == 0 || gGate.GetInfo(296+(room*2),&g2) == 0)
	{
		return 1;
	}

	int minX = ((g1.X<g2.X)?g1.X:g2.X)-kRoomRadius;
	int maxX = ((g1.X>g2.X)?g1.X:g2.X)+kRoomRadius;
	int minY = ((g1.Y<g2.Y)?g1.Y:g2.Y)-kRoomRadius;
	int maxY = ((g1.Y>g2.Y)?g1.Y:g2.Y)+kRoomRadius;

	return (x >= minX && x <= maxX && y >= minY && y <= maxY);
}

void CDuelRanked::NoticeRoom(DR_HOSTMATCH* rec,int toSlot,BYTE code,BYTE p1) // OK
{
	int aIndex = this->FindUser(rec->F[toSlot].Name,rec->F[toSlot].Account);

	if(aIndex >= 0)
	{
		this->SendClientMsg(aIndex,code,p1);
	}
}

void CDuelRanked::NoticeSpectators(DR_HOSTMATCH* rec,BYTE code,BYTE p1) // OK
{
	DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaByIndex(rec->Room);

	if(lpInfo == 0)
	{
		return;
	}

	for(int n=0;n < MAX_DUEL_ARENA_SPECTATOR;n++)
	{
		if(OBJECT_RANGE(lpInfo->Spectator[n]) != 0)
		{
			this->SendClientMsg(lpInfo->Spectator[n],code,p1);
		}
	}
}

// The integrity watcher: FIGHT and PAUSED matches, once a second, before any state change.
void CDuelRanked::ProcFight(DR_HOSTMATCH* rec) // OK
{
	DWORD now = GetTickCount();

	DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaByIndex(rec->Room);

	if(lpInfo == 0 || lpInfo->Ranked == 0 || lpInfo->MatchId != rec->MatchId)
	{
		this->EndVoid(rec,"the room was lost",1,"");
		return;
	}

	int aIndex[2];
	bool here[2];
	bool inside[2];

	for(int s=0;s < 2;s++)
	{
		aIndex[s] = this->FindUser(rec->F[s].Name,rec->F[s].Account);

		here[s] = (aIndex[s] >= 0 && gObjIsConnectedGP(aIndex[s]) != 0);

		inside[s] = (here[s] != 0 && DA_MAP_RANGE(gObj[aIndex[s]].Map) != 0 && this->InRoomArea(rec->Room,gObj[aIndex[s]].X,gObj[aIndex[s]].Y) != 0);

		if(here[s] != 0)
		{
			rec->F[s].Index = aIndex[s];
		}
	}

	if(rec->State == DRH_FIGHT)
	{
		// Read the live scores first: a fighter that is closing is not found by the
		// plain lookups below, but its object still holds its score.
		this->SyncScores(rec);

		// A fighter that vanished without the close hook seeing it (a race): same as a drop.
		for(int s=0;s < 2;s++)
		{
			if(here[s] == 0 && rec->F[s].Absent == 0)
			{
				this->PauseFighter(rec,s);

				if(rec->State != DRH_FIGHT)
				{
					return;
				}
			}
		}

		// Left the map or the arena: forfeit.
		if(inside[0] == 0 && inside[1] == 0)
		{
			this->EndVoid(rec,"both fighters left the arena",1,"");
			return;
		}

		for(int s=0;s < 2;s++)
		{
			if(inside[s] == 0)
			{
				LogAdd(LOG_RED,"[DuelRanked] match %d: %s left the arena",(int)rec->MatchId,rec->F[s].Name);

				this->SyncScores(rec);

				this->Settle(rec,1-s,DUELRANKED_REASON_FORFEIT,0);
				return;
			}
		}

		this->SyncScores(rec);

		// The ranked clock runs on real time and only while the fight is live.
		int elapsedSec = (int)((DWORD)(now-rec->ClockTick)/1000);

		if(elapsedSec > 0)
		{
			rec->RemainSec -= elapsedSec;
			rec->ClockTick += (DWORD)elapsedSec*1000;
		}

		if(rec->RemainSec <= 0)
		{
			this->SettleByScore(rec,DUELRANKED_REASON_TIMEOUT);
			return;
		}

		LPOBJ lpA = &gObj[aIndex[0]];
		LPOBJ lpB = &gObj[aIndex[1]];

		if(lpA->DuelUser == lpB->Index && lpB->DuelUser == lpA->Index)
		{
			DWORD last = ((lpA->DuelTickCount>lpB->DuelTickCount)?lpA->DuelTickCount:lpB->DuelTickCount);

			if(Elapsed(now,last,kIdleMs) != 0)
			{
				this->SettleByScore(rec,DUELRANKED_REASON_IDLE);
				return;
			}
		}

		if(OBJECT_RANGE(lpInfo->Gladiator[0]) != 0 && OBJECT_RANGE(lpInfo->Gladiator[1]) != 0)
		{
			gDuel.CheckDuelArenaSpectator(lpInfo);

			gDuel.GCDuelInterfaceSend(lpInfo);
		}

		return;
	}

	// PAUSED
	int away = ((rec->F[0].Absent!=0)?0:1);

	int present = 1-away;

	if(here[present] == 0 || inside[present] == 0)
	{
		this->EndVoid(rec,"the present fighter left during the pause",1,rec->F[present].Name);
		return;
	}

	gDuel.CheckDuelArenaSpectator(lpInfo);

	DR_FIGHTER* f = &rec->F[away];

	int elapsed = (int)((DWORD)(now-f->AbsentSince)/1000);

	if(here[away] != 0)
	{
		if(this->IsReady(&gObj[aIndex[away]]) != 0)
		{
			f->ReadyCount = ((f->ReadyCount<255)?(f->ReadyCount+1):255);
		}
		else
		{
			f->ReadyCount = 0;
		}

		if(f->ReadyCount >= gServerInfo.m_DuelRankedReadyHoldSec)
		{
			this->StartResume(rec,away);
			return;
		}
	}
	else
	{
		f->ReadyCount = 0;
	}

	if(elapsed >= gServerInfo.m_DuelRankedReconnectGraceSec)
	{
		LogAdd(LOG_RED,"[DuelRanked] match %d: %s did not come back, %s wins",(int)rec->MatchId,f->Name,rec->F[present].Name);

		this->Settle(rec,present,DUELRANKED_REASON_DISCONNECT,0);
		return;
	}

	if(Elapsed(now,rec->NoticeTick,kNoticeMs) != 0)
	{
		rec->NoticeTick = now;

		BYTE left = ClampByte(gServerInfo.m_DuelRankedReconnectGraceSec-elapsed);

		this->NoticeRoom(rec,present,DUELRANKED_MSG_OPPONENT_DISCONNECTED,left);

		this->NoticeSpectators(rec,DUELRANKED_MSG_OPPONENT_DISCONNECTED,left);
	}
}

// Copies the live scores into the record while the two are linked.
void CDuelRanked::SyncScores(DR_HOSTMATCH* rec) // OK
{
	int a = this->FindUser(rec->F[0].Name,rec->F[0].Account,1);
	int b = this->FindUser(rec->F[1].Name,rec->F[1].Account,1);

	if(a < 0 || b < 0)
	{
		return;
	}

	if(gObj[a].DuelUser == b && gObj[b].DuelUser == a)
	{
		rec->F[0].Score = gObj[a].DuelScore;
		rec->F[1].Score = gObj[b].DuelScore;
	}
}

void CDuelRanked::SettleByScore(DR_HOSTMATCH* rec,BYTE reason) // OK
{
	this->SyncScores(rec);

	if(rec->F[0].Score == rec->F[1].Score)
	{
		this->Settle(rec,-1,reason,1);
	}
	else
	{
		this->Settle(rec,((rec->F[0].Score>rec->F[1].Score)?0:1),reason,0);
	}
}

// A fighter dropped (or vanished) in a live fight.
void CDuelRanked::PauseFighter(DR_HOSTMATCH* rec,int slot) // OK
{
	if(rec->State != DRH_FIGHT || rec->Settled != 0)
	{
		return;
	}

	DR_FIGHTER* f = &rec->F[slot];

	int other = this->FindUser(rec->F[1-slot].Name,rec->F[1-slot].Account);

	if(other < 0 || gObjIsConnectedGP(other) == 0)
	{
		this->EndVoid(rec,"both fighters are gone",1,"");
		return;
	}

	if(f->Reconnects >= gServerInfo.m_DuelRankedMaxReconnects || gServerInfo.m_DuelRankedReconnectGraceSec <= 0)
	{
		LogAdd(LOG_RED,"[DuelRanked] match %d: %s dropped again, %s wins",(int)rec->MatchId,f->Name,rec->F[1-slot].Name);

		this->SyncScores(rec);

		this->Settle(rec,1-slot,DUELRANKED_REASON_DISCONNECT,0);
		return;
	}

	DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaByIndex(rec->Room);

	f->Absent = 1;
	f->Reconnects++;
	f->AbsentSince = GetTickCount();
	f->ReadyCount = 0;

	rec->State = DRH_PAUSED;
	rec->NoticeTick = GetTickCount();

	if(lpInfo != 0)
	{
		lpInfo->Gladiator[slot] = -1;
	}

	// The opponent holds no stale index and the pause is not an end for its client.
	gObj[other].DuelUser = -1;

	this->SendHostEvent(DUELRANKED_EVENT_PAUSED,rec->MatchId,rec->Room,f->Name);

	BYTE left = ClampByte(gServerInfo.m_DuelRankedReconnectGraceSec);

	this->SendClientMsg(other,DUELRANKED_MSG_OPPONENT_DISCONNECTED,left);

	this->NoticeSpectators(rec,DUELRANKED_MSG_OPPONENT_DISCONNECTED,left);

	LogAdd(LOG_RED,"[DuelRanked] match %d: %s dropped, waiting %d s (score %d-%d)",(int)rec->MatchId,f->Name,gServerInfo.m_DuelRankedReconnectGraceSec,(int)rec->F[0].Score,(int)rec->F[1].Score);

	this->m_SnapDirty = 1;
}

// The dropped fighter is back and ready: a fresh round with the score kept.
void CDuelRanked::StartResume(DR_HOSTMATCH* rec,int slot) // OK
{
	DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaByIndex(rec->Room);

	int aIndex = this->FindUser(rec->F[slot].Name,rec->F[slot].Account);
	int bIndex = this->FindUser(rec->F[1-slot].Name,rec->F[1-slot].Account);

	if(lpInfo == 0 || aIndex < 0 || bIndex < 0)
	{
		return;
	}

	lpInfo->Gladiator[slot] = aIndex;
	lpInfo->Gladiator[1-slot] = bIndex;

	rec->State = DRH_BEGIN;
	rec->Resuming = 1;
	rec->ResumeSlot = (BYTE)slot;
	rec->BeginTick = GetTickCount();

	this->m_SnapDirty = 1;

	if(this->PlaceFighters(rec) != 0)
	{
		this->FinishBegin(rec);
	}
}

// The one place a result is decided. winner -1 with draw: a tie.
void CDuelRanked::Settle(DR_HOSTMATCH* rec,int winner,BYTE reason,bool draw) // OK
{
	if(rec->Settled != 0)
	{
		return;
	}

	rec->Settled = 1;

	DWORD now = GetTickCount();

	rec->State = DRH_CLEAN;
	rec->StateTick = now;
	rec->CleanEnd = now;

	DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaByIndex(rec->Room);

	if(lpInfo != 0 && lpInfo->Ranked != 0 && lpInfo->MatchId == rec->MatchId)
	{
		lpInfo->State = DA_STATE_CLEAN;
		lpInfo->TickCount = now;
	}

	int w = ((draw!=0)?0:winner);
	int l = 1-w;

	this->QueueResult(rec,w,l,reason,draw);

	LogAdd(LOG_GREEN,"[DuelRanked] match %d settled: %s %d - %d %s (%s, reason %d)",(int)rec->MatchId,rec->F[w].Name,(int)rec->F[w].Score,(int)rec->F[l].Score,rec->F[l].Name,((draw!=0)?"draw":"win"),(int)reason);

	int idx[2];

	for(int s=0;s < 2;s++)
	{
		idx[s] = this->FindUser(rec->F[s].Name,rec->F[s].Account);

		// The scores are in the record. A fighter whose duel was cleared by the
		// pause still has to be told the fight is over (AA:03), once.
		if(idx[s] >= 0)
		{
			gDuel.ResetDuel(&gObj[idx[s]]);

			gDuel.GCDuelEndSend(idx[s],0);
		}
	}

	// The win effects, as the legacy duel shows them. No Gladiator's Glory for ranked,
	// and nothing is written to the legacy RankingDuel table.
	if(rec->EffectsDone == 0 && draw == 0 && lpInfo != 0)
	{
		rec->EffectsDone = 1;

		// No "Duel Finished" box (AA:0C, GCDuelWinSend): the ranked result panel (AA:14) is the
		// result screen of a ranked match.
		if(idx[w] >= 0)
		{
			GCFireworksSend(&gObj[idx[w]],gObj[idx[w]].X,gObj[idx[w]].Y);
		}

		if(gServerInfo.m_DuelArenaAnnounceSwitch != 0)
		{
			gNotice.GCNoticeSendToAll(0,0,0,0,0,0,gMessage.GetMessage(175),rec->F[w].Name,rec->F[l].Name);
		}
	}

	if(reason == DUELRANKED_REASON_DISCONNECT)
	{
		if(idx[w] >= 0)
		{
			this->SendClientMsg(idx[w],DUELRANKED_MSG_OPPONENT_TIMED_OUT,0);
		}

		if(idx[l] >= 0)
		{
			this->SendClientMsg(idx[l],DUELRANKED_MSG_YOU_TIMED_OUT,0);
		}
	}

	this->m_SnapDirty = 1;
}

void CDuelRanked::QueueResult(DR_HOSTMATCH* rec,int w,int l,BYTE reason,bool draw) // OK
{
	DR_RESULT* res = 0;

	for(int n=0;n < DR_MAX_RESULT;n++)
	{
		if(this->m_Result[n].Used == 0)
		{
			res = &this->m_Result[n];
			break;
		}
	}

	if(res == 0)
	{
		// Full of unanswered results: the oldest is the least likely to ever be answered.
		res = &this->m_Result[0];

		for(int n=1;n < DR_MAX_RESULT;n++)
		{
			if(this->m_Result[n].FirstTick < res->FirstTick)
			{
				res = &this->m_Result[n];
			}
		}

		LogAdd(LOG_RED,"[DuelRanked] result table full, dropping the result of match %d",(int)res->Msg.MatchId);
	}

	memset(res,0,sizeof(DR_RESULT));

	res->Used = 1;

	res->Msg.header.set(0xD9,0x55,sizeof(res->Msg));

	res->Msg.MatchId = rec->MatchId;

	CopyName(res->Msg.Winner,rec->F[w].Name);

	CopyName(res->Msg.Loser,rec->F[l].Name);

	res->Msg.Reason = reason;

	res->Msg.ScoreW = rec->F[w].Score;

	res->Msg.ScoreL = rec->F[l].Score;

	res->Msg.IsDraw = ((draw!=0)?1:0);

	res->FirstTick = GetTickCount();

	res->LastTick = res->FirstTick;

	gDataServerConnection.DataSend((BYTE*)&res->Msg,res->Msg.header.size);
}

void CDuelRanked::ProcResults() // OK
{
	DWORD now = GetTickCount();

	for(int n=0;n < DR_MAX_RESULT;n++)
	{
		DR_RESULT* res = &this->m_Result[n];

		if(res->Used == 0)
		{
			continue;
		}

		if(Elapsed(now,res->FirstTick,kResultKeepMs) != 0)
		{
			LogAdd(LOG_RED,"[DuelRanked] no answer for the result of match %d, given up",(int)res->Msg.MatchId);

			res->Used = 0;

			continue;
		}

		if(Elapsed(now,res->LastTick,kResultResendMs) != 0)
		{
			res->LastTick = now;

			gDataServerConnection.DataSend((BYTE*)&res->Msg,res->Msg.header.size);
		}
	}
}

// No rated result: voided from here (a drop, the fighters left, the DataServer said so).
void CDuelRanked::EndVoid(DR_HOSTMATCH* rec,char* why,bool sendAbort,char* blame) // OK
{
	if(rec->Settled != 0)
	{
		return;
	}

	rec->Settled = 1;
	rec->Voided = 1;
	rec->ResultShown = 1;
	rec->EffectsDone = 1;

	DWORD now = GetTickCount();

	rec->State = DRH_CLEAN;
	rec->StateTick = now;
	rec->CleanEnd = now-kCleanMs+2000;

	LogAdd(LOG_RED,"[DuelRanked] match %d voided: %s",(int)rec->MatchId,why);

	if(sendAbort != 0)
	{
		this->SendHostEvent(DUELRANKED_EVENT_ABORT,rec->MatchId,rec->Room,blame);
	}

	DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaByIndex(rec->Room);

	if(lpInfo != 0 && lpInfo->Ranked != 0 && lpInfo->MatchId == rec->MatchId)
	{
		lpInfo->State = DA_STATE_CLEAN;
		lpInfo->TickCount = now;
	}

	for(int s=0;s < 2;s++)
	{
		int aIndex = this->FindUser(rec->F[s].Name,rec->F[s].Account);

		if(aIndex >= 0 && rec->StartedSent != 0)
		{
			gDuel.ResetDuel(&gObj[aIndex]);

			gDuel.GCDuelEndSend(aIndex,0);
		}
		else if(aIndex >= 0)
		{
			gDuel.ResetDuel(&gObj[aIndex]);
		}
	}

	this->m_SnapDirty = 1;
}

void CDuelRanked::ProcClean(DR_HOSTMATCH* rec) // OK
{
	DWORD now = GetTickCount();

	// Everyone is on their way home: keep the record (and the PK guard) until they
	// have left the arena map, but not for long.
	if(rec->Gone != 0)
	{
		bool stillThere = 0;

		for(int s=0;s < 2;s++)
		{
			int aIndex = this->FindUser(rec->F[s].Name,rec->F[s].Account);

			if(aIndex >= 0 && DA_MAP_RANGE(gObj[aIndex].Map) != 0)
			{
				stillThere = 1;
			}
		}

		if(stillThere == 0 || Elapsed(now,rec->GoneTick,kLingerMs) != 0)
		{
			rec->Used = 0;

			this->m_SnapDirty = 1;
		}

		return;
	}

	if(Elapsed(now,rec->CleanEnd,kCleanMs) == 0)
	{
		return;
	}

	// The DataServer's answer shows the fighters their MMR; wait for it a little.
	if(rec->ResultShown == 0 && Elapsed(now,rec->CleanEnd,kCleanMs+kCleanWaitMs) == 0)
	{
		return;
	}

	if(Elapsed(now,rec->CleanEnd,kCleanMs+kCleanDeadMs) == 0)
	{
		for(int s=0;s < 2;s++)
		{
			int aIndex = this->FindUser(rec->F[s].Name,rec->F[s].Account);

			if(aIndex >= 0 && gObj[aIndex].DieRegen != 0)
			{
				return;
			}
		}
	}

	this->LeaveArena(rec);
}

// Everyone in the room goes home, then the room is free again.
void CDuelRanked::LeaveArena(DR_HOSTMATCH* rec) // OK
{
	DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaByIndex(rec->Room);

	for(int s=0;s < 2;s++)
	{
		int aIndex = this->FindUser(rec->F[s].Name,rec->F[s].Account);

		if(aIndex >= 0)
		{
			this->SendClientMsg(aIndex,DUELRANKED_MSG_GOING_HOME,0);

			this->SendHomeRecord(aIndex,rec->F[s].OriginServer,rec->F[s].OriginMap,rec->F[s].OriginX,rec->F[s].OriginY);
		}
	}

	if(lpInfo != 0 && lpInfo->Ranked != 0 && lpInfo->MatchId == rec->MatchId)
	{
		for(int n=0;n < MAX_DUEL_ARENA_SPECTATOR;n++)
		{
			int aIndex = lpInfo->Spectator[n];

			if(OBJECT_RANGE(aIndex) == 0)
			{
				continue;
			}

			gEffectManager.DelEffect(&gObj[aIndex],EFFECT_INVISIBILITY);

			gEffectManager.DelEffect(&gObj[aIndex],EFFECT_DUEL_ARENA_WATCH);

			gDuel.GCDuelGladiatorDelSend(aIndex,0);

			this->SendClientMsg(aIndex,DUELRANKED_MSG_GOING_HOME,0);

			this->SendHome(aIndex);

			lpInfo->Spectator[n] = -1;
		}

		this->DropSpectatorsOf(rec->MatchId);

		memset(lpInfo->Gladiator,-1,sizeof(lpInfo->Gladiator));

		memset(lpInfo->Spectator,-1,sizeof(lpInfo->Spectator));

		gDuel.SetState(lpInfo,DA_STATE_EMPTY);
	}

	LogAdd(LOG_BLUE,"[DuelRanked] match %d: the room is free again",(int)rec->MatchId);

	rec->Gone = 1;
	rec->GoneTick = GetTickCount();

	this->m_SnapDirty = 1;
}

//////////////////////////////////////////////////////////////////////
// Arrivals that outlived their match
//////////////////////////////////////////////////////////////////////

// A staging match is over before it began: every fighter that was announced goes
// back to where he came from.
void CDuelRanked::SendFightersHome(DR_HOSTMATCH* rec) // OK
{
	for(int s=0;s < 2;s++)
	{
		DR_FIGHTER* f = &rec->F[s];

		if(f->HaveExpect != 0)
		{
			this->AddHomeWait(f->Name,f->Account,f->OriginServer,f->OriginMap,f->OriginX,f->OriginY);
		}
	}
}

// Remember where someone came from, to send him back as soon as he is on this
// host's arena map. Covers the one who is already here (the next tick sends him)
// and the one who left his origin but has not landed yet: without this he lands
// with no record and stands on the arena map until the sweep sends him to gate 17.
void CDuelRanked::AddHomeWait(char* name,char* account,WORD originServer,BYTE originMap,BYTE x,BYTE y) // OK
{
	// The origin is this host: he was never moved, there is nothing to undo.
	if(name == 0 || name[0] == 0 || gServerInfo.m_ServerCode == originServer)
	{
		return;
	}

	DR_HOMEWAIT* slot = 0;

	for(int n=0;n < DR_MAX_HOMEWAIT;n++)
	{
		DR_HOMEWAIT* e = &this->m_HomeWait[n];

		if(e->Used != 0 && strcmp(e->Name,name) == 0)
		{
			slot = e;
			break;
		}

		if(e->Used == 0 && slot == 0)
		{
			slot = e;
		}
	}

	if(slot == 0)
	{
		// Table full: reuse the oldest.
		DWORD now = GetTickCount();

		slot = &this->m_HomeWait[0];

		for(int n=1;n < DR_MAX_HOMEWAIT;n++)
		{
			if((DWORD)(now-this->m_HomeWait[n].Since) > (DWORD)(now-slot->Since))
			{
				slot = &this->m_HomeWait[n];
			}
		}
	}

	memset(slot,0,sizeof(DR_HOMEWAIT));

	slot->Used = 1;

	CopyName(slot->Name,name);
	CopyName(slot->Account,account);

	slot->OriginServer = originServer;
	slot->OriginMap = originMap;
	slot->OriginX = x;
	slot->OriginY = y;
	slot->Since = GetTickCount();
}

void CDuelRanked::DropHomeWait(char* name) // OK
{
	for(int n=0;n < DR_MAX_HOMEWAIT;n++)
	{
		if(this->m_HomeWait[n].Used != 0 && strcmp(this->m_HomeWait[n].Name,name) == 0)
		{
			this->m_HomeWait[n].Used = 0;
		}
	}
}

void CDuelRanked::ProcHomeWait() // OK
{
	DWORD now = GetTickCount();

	for(int n=0;n < DR_MAX_HOMEWAIT;n++)
	{
		DR_HOMEWAIT* e = &this->m_HomeWait[n];

		if(e->Used == 0)
		{
			continue;
		}

		bool expired = Elapsed(now,e->Since,kHomeWaitMs);

		int aIndex = this->FindUser(e->Name,e->Account);

		if(aIndex < 0)
		{
			// Still hopping (or never made it): wait for him a while.
			if(expired != 0)
			{
				e->Used = 0;
			}

			continue;
		}

		// Not on the arena map: the hop did not take or he is already elsewhere, nothing to undo.
		if(DA_MAP_RANGE(gObj[aIndex].Map) == 0)
		{
			e->Used = 0;

			continue;
		}

		// Just landed and still loading: gObjTeleport needs a character that is playing.
		if(this->IsReady(&gObj[aIndex]) == 0)
		{
			if(expired != 0)
			{
				e->Used = 0;
			}

			continue;
		}

		this->SendClientMsg(aIndex,DUELRANKED_MSG_GOING_HOME,0);

		this->SendHomeRecord(aIndex,e->OriginServer,e->OriginMap,e->OriginX,e->OriginY);

		e->Used = 0;
	}
}

//////////////////////////////////////////////////////////////////////
// Spectators
//////////////////////////////////////////////////////////////////////

// Forget the spectator entries of a match (the room is going away). Those
// still waiting to arrive are sent home when they are here, or when they land.
void CDuelRanked::DropSpectatorsOf(DWORD matchId) // OK
{
	for(int n=0;n < DR_MAX_SPEC;n++)
	{
		DR_SPEC* e = &this->m_Spec[n];

		if(e->Used == 0 || e->MatchId != matchId)
		{
			continue;
		}

		if(e->State == 1)
		{
			int aIndex = this->FindUser(e->Name,e->Account);

			if(aIndex >= 0 && gServerInfo.m_ServerCode != e->OriginServer)
			{
				this->SendHomeRecord(aIndex,e->OriginServer,e->OriginMap,e->OriginX,e->OriginY);
			}
			else if(aIndex < 0)
			{
				this->AddHomeWait(e->Name,e->Account,e->OriginServer,e->OriginMap,e->OriginX,e->OriginY);
			}
		}

		this->SendHostEvent(DUELRANKED_EVENT_SPECTATOR_LEFT,e->MatchId,e->Room,e->Name);

		e->Used = 0;
	}

	this->m_SnapDirty = 1;
}

void CDuelRanked::ProcSpectators() // OK
{
	DWORD now = GetTickCount();

	for(int n=0;n < DR_MAX_SPEC;n++)
	{
		DR_SPEC* e = &this->m_Spec[n];

		if(e->Used == 0)
		{
			continue;
		}

		DR_HOSTMATCH* rec = this->FindMatch(e->MatchId);

		int aIndex = this->FindUser(e->Name,e->Account);

		if(e->State == 1)
		{
			bool give = (rec == 0 || rec->Settled != 0 || rec->StartedSent == 0 || Elapsed(now,e->AddedTick,kSpecWaitMs) != 0);

			if(give != 0)
			{
				if(aIndex >= 0 && gServerInfo.m_ServerCode != e->OriginServer)
				{
					this->SendHomeRecord(aIndex,e->OriginServer,e->OriginMap,e->OriginX,e->OriginY);
				}
				else if(aIndex >= 0)
				{
					// The origin is this host: it was never moved, nothing to undo.
				}
				else
				{
					// The match ended while he was still hopping: he goes home when he lands.
					this->AddHomeWait(e->Name,e->Account,e->OriginServer,e->OriginMap,e->OriginX,e->OriginY);
				}

				this->SendHostEvent(DUELRANKED_EVENT_SPECTATOR_LEFT,e->MatchId,e->Room,e->Name);

				e->Used = 0;

				this->m_SnapDirty = 1;

				continue;
			}

			if(aIndex < 0)
			{
				e->ReadyCount = 0;
				continue;
			}

			e->Index = aIndex;

			if(this->IsReady(&gObj[aIndex]) != 0)
			{
				e->ReadyCount = ((e->ReadyCount<255)?(e->ReadyCount+1):255);
			}
			else
			{
				e->ReadyCount = 0;
			}

			if(e->ReadyCount >= gServerInfo.m_DuelRankedReadyHoldSec)
			{
				this->AddSpectator(e,rec);
			}
		}
		else
		{
			DUEL_ARENA_INFO* lpInfo = ((rec!=0)?gDuel.GetDuelArenaByIndex(rec->Room):0);

			if(aIndex < 0 || lpInfo == 0 || gDuel.GetDuelArenaSpectator(lpInfo,aIndex) == 0)
			{
				// Left, logged out or was pruned by the room: forget it.
				this->SendHostEvent(DUELRANKED_EVENT_SPECTATOR_LEFT,e->MatchId,e->Room,e->Name);

				e->Used = 0;

				this->m_SnapDirty = 1;
			}
		}
	}
}

void CDuelRanked::AddSpectator(DR_SPEC* e,DR_HOSTMATCH* rec) // OK
{
	DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaByIndex(rec->Room);

	int aIndex = this->FindUser(e->Name,e->Account);

	if(lpInfo == 0 || aIndex < 0 || lpInfo->Ranked == 0 || lpInfo->MatchId != rec->MatchId || (rec->State != DRH_FIGHT && rec->State != DRH_PAUSED))
	{
		return;
	}

	if(gDuel.GetDuelArenaSpectatorCount(lpInfo) >= MAX_DUEL_ARENA_SPECTATOR || gDuel.AddRankedSpectator(lpInfo,&gObj[aIndex]) == 0)
	{
		// Room full: send the player home; the DataServer is told it left.
		if(gServerInfo.m_ServerCode != e->OriginServer)
		{
			this->SendHomeRecord(aIndex,e->OriginServer,e->OriginMap,e->OriginX,e->OriginY);
		}

		this->SendHostEvent(DUELRANKED_EVENT_SPECTATOR_LEFT,e->MatchId,e->Room,e->Name);

		e->Used = 0;

		this->m_SnapDirty = 1;

		return;
	}

	e->State = 2;
	e->Arrived = 1;
	e->Index = aIndex;

	this->SendHostEvent(DUELRANKED_EVENT_SPECTATOR_ARRIVED,e->MatchId,e->Room,e->Name);

	this->SendClientMsg(aIndex,DUELRANKED_MSG_SPECTATING,0);

	this->m_SnapDirty = 1;
}

void CDuelRanked::RemoveSpectator(DR_SPEC* e,bool sendHome) // OK
{
	int aIndex = this->FindUser(e->Name,e->Account);

	DR_HOSTMATCH* rec = this->FindMatch(e->MatchId);

	if(aIndex >= 0)
	{
		DUEL_ARENA_INFO* lpInfo = ((rec!=0)?gDuel.GetDuelArenaByIndex(rec->Room):0);

		gEffectManager.DelEffect(&gObj[aIndex],EFFECT_INVISIBILITY);

		gEffectManager.DelEffect(&gObj[aIndex],EFFECT_DUEL_ARENA_WATCH);

		if(lpInfo != 0 && gDuel.GetDuelArenaSpectator(lpInfo,aIndex) != 0)
		{
			gDuel.GCDuelSpectatorDelSend(aIndex,lpInfo);

			gDuel.DelDuelArenaSpectator(lpInfo,aIndex);
		}

		if(sendHome != 0)
		{
			this->SendClientMsg(aIndex,DUELRANKED_MSG_GOING_HOME,0);

			this->SendHomeRecord(aIndex,e->OriginServer,e->OriginMap,e->OriginX,e->OriginY);
		}
	}

	this->SendHostEvent(DUELRANKED_EVENT_SPECTATOR_LEFT,e->MatchId,e->Room,e->Name);

	e->Used = 0;

	this->m_SnapDirty = 1;
}

//////////////////////////////////////////////////////////////////////
// Host report and the stranded sweep
//////////////////////////////////////////////////////////////////////

void CDuelRanked::SendHostReport() // OK
{
	SDHP_DUELRANKED_HOSTREPORT r;

	memset(&r,0,sizeof(r));

	r.header.set(0xD9,0x52,sizeof(r));

	r.UserCount = ClampWord(gObjTotalUser);

	r.UserMax = ClampWord(((gServerInfo.m_ServerMaxUserNumber>0)?gServerInfo.m_ServerMaxUserNumber:MAX_OBJECT_USER));

	r.Accepting = ((this->HostAccepting()!=0)?1:0);

	int empty = 0;
	int ranked = 0;

	for(int n=0;n < MAX_DUEL_ARENA && n < 4;n++)
	{
		DUEL_ARENA_INFO* lpInfo = gDuel.GetDuelArenaByIndex(n);

		DUELRANKED_ROOM* room = &r.Rooms[n];

		if(lpInfo == 0 || lpInfo->State == DA_STATE_EMPTY)
		{
			empty++;
			continue;
		}

		room->Ranked = ((lpInfo->Ranked!=0)?1:0);

		if(lpInfo->Ranked != 0)
		{
			ranked++;
		}

		DR_HOSTMATCH* rec = ((lpInfo->Ranked!=0)?this->FindMatch(lpInfo->MatchId):0);

		if(rec != 0)
		{
			room->State = ((rec->State==DRH_PAUSED)?2:((rec->State==DRH_CLEAN)?3:1));

			room->MatchId = rec->MatchId;

			CopyName(room->Name1,rec->F[0].Name);

			CopyName(room->Name2,rec->F[1].Name);

			room->Score1 = rec->F[0].Score;

			room->Score2 = rec->F[1].Score;

			room->RemainSec = ClampWord(rec->RemainSec);
		}
		else
		{
			room->State = ((lpInfo->State==DA_STATE_START)?1:3);

			if(OBJECT_RANGE(lpInfo->Gladiator[0]) != 0)
			{
				CopyName(room->Name1,gObj[lpInfo->Gladiator[0]].Name);

				room->Score1 = gObj[lpInfo->Gladiator[0]].DuelScore;
			}

			if(OBJECT_RANGE(lpInfo->Gladiator[1]) != 0)
			{
				CopyName(room->Name2,gObj[lpInfo->Gladiator[1]].Name);

				room->Score2 = gObj[lpInfo->Gladiator[1]].DuelScore;
			}

			room->RemainSec = ClampWord(lpInfo->RemainTime);
		}
	}

	int quota = gServerInfo.m_DuelRankedMaxConcurrent-ranked;

	r.FreeRooms = (BYTE)((quota<empty)?((quota<0)?0:quota):empty);

	// A report goes out when the arena changed, or as the 5 s heartbeat. The
	// clocks and the user count change all the time and do not count as a change.
	SDHP_DUELRANKED_HOSTREPORT cmp = r;

	cmp.UserCount = 0;

	for(int n=0;n < 4;n++)
	{
		cmp.Rooms[n].RemainSec = 0;
	}

	DWORD now = GetTickCount();

	if(memcmp(&cmp,&this->m_LastReport,sizeof(cmp)) != 0 || Elapsed(now,this->m_LastReportTick,kReportMs) != 0)
	{
		this->m_LastReport = cmp;

		this->m_LastReportTick = now;

		gDataServerConnection.DataSend((BYTE*)&r,r.header.size);
	}
}

// Anyone left standing on map 64 who is not part of a duel for a minute goes to gate 17.
void CDuelRanked::Sweep() // OK
{
	if(gServerInfo.m_DuelRankedSwitch == 0)
	{
		return;
	}

	DWORD now = GetTickCount();

	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		LPOBJ lpObj = &gObj[n];

		if(lpObj->Connected != OBJECT_ONLINE || lpObj->Type != OBJECT_USER || DA_MAP_RANGE(lpObj->Map) == 0)
		{
			this->m_StrandSince[n] = 0;
			continue;
		}

		bool legit = 0;

		if(OBJECT_RANGE(lpObj->DuelUser) != 0 || OBJECT_RANGE(lpObj->DuelUserReserved) != 0 || OBJECT_RANGE(lpObj->DuelUserRequested) != 0)
		{
			legit = 1;
		}
		else if(gDuel.GetDuelArenaByGladiator(n) != 0 || gDuel.GetDuelArenaBySpectator(n) != 0 || this->IsParticipant(lpObj->Name) != 0)
		{
			legit = 1;
		}
		else if(this->IsPendingSpectator(lpObj->Name) != 0)
		{
			legit = 1;
		}
		else if(gGameMaster.CheckGameMasterLevel(lpObj,1) != 0)
		{
			legit = 1;
		}

		if(legit != 0)
		{
			this->m_StrandSince[n] = 0;
			continue;
		}

		if(this->m_StrandSince[n] == 0)
		{
			this->m_StrandSince[n] = now|1;
		}
		else if(Elapsed(now,this->m_StrandSince[n],kStrandMs) != 0)
		{
			LogAdd(LOG_RED,"[DuelRanked] %s was left on the arena map, sent to gate 17",lpObj->Name);

			this->m_StrandSince[n] = 0;

			gObjMoveGate(n,17);
		}
	}
}

bool CDuelRanked::IsPendingSpectator(char* name) // OK
{
	for(int n=0;n < DR_MAX_SPEC;n++)
	{
		if(this->m_Spec[n].Used != 0 && strcmp(this->m_Spec[n].Name,name) == 0)
		{
			return 1;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////
// GM test commands: /rankedhop and /rankedhome
//////////////////////////////////////////////////////////////////////

// /rankedhop: send the GM to the host's arena map the way a ranked fighter is sent.
// The GM stays where it is until /rankedhome (the stranded sweep skips GMs).
bool CDuelRanked::TestHop(LPOBJ lpObj) // OK
{
	if(gGameMaster.CheckGameMasterLevel(lpObj,1) == 0)
	{
		return 0;
	}

	WORD host = (WORD)gServerInfo.m_DuelRankedHostServer;

	gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"[DuelRanked] hop to server %d, map %d (from server %d, map %d %d,%d)",(int)host,(int)MAP_DUEL_ARENA,(int)gServerInfo.m_ServerCode,(int)lpObj->Map,(int)lpObj->X,(int)lpObj->Y);

	if(gServerInfo.m_ServerCode == host)
	{
		int gate,map,x,y,dir,level;

		if(gGate.GetGate(gServerInfo.m_DuelRankedStageGate,&gate,&map,&x,&y,&dir,&level) == 0)
		{
			gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"[DuelRanked] stage gate %d is not usable",(int)gServerInfo.m_DuelRankedStageGate);
			return 1;
		}

		gObjTeleport(lpObj->Index,map,x,y);

		return 1;
	}

	if(this->HopToHost(lpObj,host) == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"[DuelRanked] the hop could not be started (host %d not in MapServerInfo.dat, or stage gate %d unusable)",(int)host,(int)gServerInfo.m_DuelRankedStageGate);
	}

	return 1;
}

// /rankedhome [map x y]: back with gObjTeleport, which hops through LastServerCode.
// Without arguments: the Lorencia gate 17 spot.
bool CDuelRanked::TestHome(LPOBJ lpObj,char* arg) // OK
{
	if(gGameMaster.CheckGameMasterLevel(lpObj,1) == 0)
	{
		return 0;
	}

	int map = -1;
	int x = 0;
	int y = 0;

	if(arg != 0 && arg[0] != 0)
	{
		map = gCommandManager.GetNumber(arg,0);
		x = gCommandManager.GetNumber(arg,1);
		y = gCommandManager.GetNumber(arg,2);
	}

	if(MAP_RANGE(map) == 0 || map == MAP_DUEL_ARENA)
	{
		int gate,dir,level;

		if(gGate.GetGate(17,&gate,&map,&x,&y,&dir,&level) == 0)
		{
			return 1;
		}
	}

	gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"[DuelRanked] teleport to map %d (%d,%d), last server %d",map,x,y,(int)lpObj->LastServerCode);

	gObjTeleport(lpObj->Index,map,x,y);

	return 1;
}
