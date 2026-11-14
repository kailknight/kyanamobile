// KingOfKira.cpp - see KingOfKira.h and Working Project\MD Files\KingOfKira-DESIGN.md.
#include "stdafx.h"
#include "KingOfKira.h"
#include "Connection.h"
#include "GameMain.h"
#include "GameMaster.h"
#include "Gate.h"
#include "Union.h"
#include "Guild.h"
#include "GuildClass.h"
#include "JSProtocol.h"
#include "Map.h"
#include "MapServerManager.h"
#include "Monster.h"
#include "MonsterSetBase.h"
#include "Notice.h"
#include "ServerInfo.h"
#include "ServerDisplayer.h"
#include "User.h"
#include "Util.h"
#include "Viewport.h"
#include "Log.h"

CKingOfKira gKingOfKira;

static void CopyText(char* dest,int destSize,const char* src)
{
	int n = 0;

	if(src != 0)
	{
		for(;n < destSize-1 && src[n] != 0;n++)
		{
			dest[n] = src[n];
		}
	}

	dest[n] = 0;
}

CKingOfKira::CKingOfKira() // OK
{
	this->m_Switch = 0;
	this->m_HostServer = 19;
	this->m_Schedule = 2;
	this->m_DayOfWeek = 6;
	this->m_Hour = 20;
	this->m_Minute = 0;
	this->m_RegisterOpenMin = 0;
	this->m_RegisterCloseMin = -1;
	this->m_WarnMinutes = 5;
	this->m_DurationMin = 30;
	this->m_DeathPenalty = 30;
	this->m_ChaosBonus = 5;
	this->m_NpcClass = 241;
	this->m_NpcMap = 2;
	this->m_NpcX = 205;
	this->m_NpcY = 41;
	this->m_NpcDir = 3;
	this->m_HillMap = 2;
	this->m_HillX1 = 225;
	this->m_HillX2 = 226;
	this->m_HillY = 237;
	this->m_ArriveX = 225;
	this->m_ArriveY = 196;
	this->m_StartGate = 73;
	this->m_ExitDelaySec = 20;
	this->m_GMBypass = 1;

	memset(&this->m_S,0,sizeof(this->m_S));
	this->m_StateTick = GetTickCount();
	this->m_SecTick = GetTickCount();
	this->m_NpcIndex = -1;

	this->m_WinLoaded = false;
	this->m_BonusAlliance = 1;
	this->m_GoblinClass = 238;
	this->m_GoblinX = 207;
	this->m_GoblinY = 46;
	this->m_GoblinDir = 3;
	this->m_GoblinIndex = -1;
	this->m_WinAskTick = 0;
	this->m_WinAsks = 0;
	this->m_WinHas = 0;
	memset(this->m_WinGuild,0,sizeof(this->m_WinGuild));
	memset(this->m_WinMark,0xFF,sizeof(this->m_WinMark));
	this->m_WinKey = 0;

	this->m_NextStart = 0;
	this->m_StateEnd = 0;
	this->m_OpenAt = 0;
	this->m_LastWarnMinute = 0;
	this->m_LastRegAnnounce = 0;
	this->m_HoldAnnounceSec = 15;
	this->m_HoldSlot = -1;
	this->m_HoldSince = 0;
	this->m_LastHoldAnnounce = 0;
	this->m_CloseAt = 0;
	memset(this->m_List,0,sizeof(this->m_List));
	this->m_ListCount = 0;
	this->m_ListTick = 0;
	memset(this->m_EnforceTick,0,sizeof(this->m_EnforceTick));
	memset(this->m_Points,0,sizeof(this->m_Points));
	this->m_LastIdleSend = 0;
	this->m_DisplayRemain = 0;
}

//////////////////////////////////////////////////////////////////////
// Config
//////////////////////////////////////////////////////////////////////

void CKingOfKira::Load(char* section,char* path) // OK
{
	this->m_Switch = GetPrivateProfileInt(section,"KingOfKiraSwitch",0,path);
	this->m_HostServer = GetPrivateProfileInt(section,"KingOfKiraHostServer",19,path);
	this->m_Schedule = GetPrivateProfileInt(section,"KingOfKiraSchedule",2,path);
	this->m_DayOfWeek = GetPrivateProfileInt(section,"KingOfKiraDayOfWeek",6,path);
	this->m_Hour = GetPrivateProfileInt(section,"KingOfKiraHour",20,path);
	this->m_Minute = GetPrivateProfileInt(section,"KingOfKiraMinute",0,path);
	this->m_RegisterOpenMin = GetPrivateProfileInt(section,"KingOfKiraRegisterOpenMin",0,path);
	this->m_RegisterCloseMin = GetPrivateProfileInt(section,"KingOfKiraRegisterCloseMin",-1,path);
	this->m_WarnMinutes = GetPrivateProfileInt(section,"KingOfKiraWarnMinutes",5,path);
	this->m_DurationMin = GetPrivateProfileInt(section,"KingOfKiraDurationMin",30,path);
	this->m_DeathPenalty = GetPrivateProfileInt(section,"KingOfKiraDeathPenalty",30,path);
	this->m_HoldAnnounceSec = GetPrivateProfileInt(section,"KingOfKiraHoldAnnounceSec",15,path);
	this->m_ChaosBonus = GetPrivateProfileInt(section,"KingOfKiraChaosBonus",5,path);
	this->m_BonusAlliance = GetPrivateProfileInt(section,"KingOfKiraBonusAlliance",1,path);
	this->m_GoblinClass = GetPrivateProfileInt(section,"KingOfKiraGoblinClass",238,path);
	this->m_GoblinX = GetPrivateProfileInt(section,"KingOfKiraGoblinX",207,path);
	this->m_GoblinY = GetPrivateProfileInt(section,"KingOfKiraGoblinY",46,path);
	this->m_GoblinDir = GetPrivateProfileInt(section,"KingOfKiraGoblinDir",3,path);
	this->m_NpcClass = GetPrivateProfileInt(section,"KingOfKiraNpcClass",241,path);
	this->m_NpcMap = GetPrivateProfileInt(section,"KingOfKiraNpcMap",2,path);
	this->m_NpcX = GetPrivateProfileInt(section,"KingOfKiraNpcX",205,path);
	this->m_NpcY = GetPrivateProfileInt(section,"KingOfKiraNpcY",41,path);
	this->m_NpcDir = GetPrivateProfileInt(section,"KingOfKiraNpcDir",3,path);
	this->m_HillMap = GetPrivateProfileInt(section,"KingOfKiraHillMap",2,path);
	this->m_HillX1 = GetPrivateProfileInt(section,"KingOfKiraHillX1",225,path);
	this->m_HillX2 = GetPrivateProfileInt(section,"KingOfKiraHillX2",226,path);
	this->m_HillY = GetPrivateProfileInt(section,"KingOfKiraHillY",237,path);
	this->m_ArriveX = GetPrivateProfileInt(section,"KingOfKiraArriveX",225,path);
	this->m_ArriveY = GetPrivateProfileInt(section,"KingOfKiraArriveY",196,path);
	this->m_StartGate = GetPrivateProfileInt(section,"KingOfKiraStartGate",73,path);
	this->m_ExitDelaySec = GetPrivateProfileInt(section,"KingOfKiraExitDelaySec",20,path);
	this->m_GMBypass = GetPrivateProfileInt(section,"KingOfKiraGMBypass",1,path);

	if(this->m_RegisterOpenMin < 0){this->m_RegisterOpenMin = 0;}
	if(this->m_RegisterCloseMin < -1){this->m_RegisterCloseMin = -1;}
	if(this->m_DurationMin < 1){this->m_DurationMin = 1;}
	if(this->m_DeathPenalty < 0){this->m_DeathPenalty = 0;}
	if(this->m_WarnMinutes < 0){this->m_WarnMinutes = 0;}
	if(this->m_ChaosBonus < 0){this->m_ChaosBonus = 0;}
	if(this->m_ExitDelaySec < 1){this->m_ExitDelaySec = 1;}
	if(this->m_Schedule < 0 || this->m_Schedule > 2){this->m_Schedule = 0;}
	if(this->m_DayOfWeek < 0 || this->m_DayOfWeek > 6){this->m_DayOfWeek = 6;}

	if(this->IsHost() != 0)
	{
		// A reload never interrupts a running event: only an idle event is rescheduled.
		if(this->m_S.State == KOK_STATE_BLANK || this->m_S.State == KOK_STATE_IDLE)
		{
			this->m_S.State = ((this->m_Switch != 0 && this->m_Schedule != 0)?KOK_STATE_IDLE:KOK_STATE_BLANK);
			this->HostSetNextStart();
		}
	}
}

bool CKingOfKira::IsHost() // OK
{
	return (gServerInfo.m_ServerCode == this->m_HostServer);
}

int CKingOfKira::GetState() // OK
{
	if(this->m_Switch == 0)
	{
		return KOK_STATE_BLANK;
	}

	return this->m_S.State;
}

bool CKingOfKira::ServesDevias() // OK
{
	return (gMapServerManager.CheckMapServer(this->m_NpcMap) != 0);
}

//////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////

void CKingOfKira::Announce(const char* format,...) // OK
{
	char buff[256] = {0};

	va_list arg;
	va_start(arg,format);
	vsprintf_s(buff,format,arg);
	va_end(arg);

	gNotice.GCNoticeSendToAll(0,0,0,0,0,0,"%s",buff); // type 0 = the golden server-wide line
}

int CKingOfKira::PlayerKey(LPOBJ lpObj) // OK
{
	if(lpObj->Guild == 0)
	{
		return 0;
	}

	return ((lpObj->Guild->GuildUnion != 0)?lpObj->Guild->GuildUnion:lpObj->Guild->Number);
}

bool CKingOfKira::IsParticipant(LPOBJ lpObj) // OK
{
	int key = this->PlayerKey(lpObj);

	if(key == 0)
	{
		return false;
	}

	if(this->m_S.State != KOK_STATE_FIGHT && this->m_S.State != KOK_STATE_END)
	{
		return false;
	}

	for(int n=0;n < this->m_ListCount && n < KOK_MAX_ENTRIES;n++)
	{
		if(this->m_List[n].Guild[0] != 0 && key == this->m_List[n].Key)
		{
			return true;
		}
	}

	return false;
}

int CKingOfKira::FindSlot(const char* guild) // OK
{
	for(int n=0;n < this->m_ListCount && n < KOK_MAX_ENTRIES;n++)
	{
		if(this->m_List[n].Guild[0] != 0 && strcmp(guild,this->m_List[n].Guild) == 0)
		{
			return n;
		}
	}

	return -1;
}

void CKingOfKira::UpdateTop() // OK
{
	int first = -1, second = -1;

	for(int n=0;n < this->m_ListCount && n < KOK_MAX_ENTRIES;n++)
	{
		if(first == -1 || this->m_Points[n] > this->m_Points[first])
		{
			second = first;
			first = n;
		}
		else if(second == -1 || this->m_Points[n] > this->m_Points[second])
		{
			second = n;
		}
	}

	memset(this->m_S.Guild1,0,sizeof(this->m_S.Guild1));
	memset(this->m_S.Guild2,0,sizeof(this->m_S.Guild2));
	this->m_S.Points1 = 0;
	this->m_S.Points2 = 0;
	this->m_S.Key1 = 0;
	this->m_S.Key2 = 0;

	if(first >= 0)
	{
		CopyText(this->m_S.Guild1,sizeof(this->m_S.Guild1),this->m_List[first].Guild);
		this->m_S.Points1 = (WORD)this->m_Points[first];
		this->m_S.Key1 = this->m_List[first].Key;
	}

	if(second >= 0)
	{
		CopyText(this->m_S.Guild2,sizeof(this->m_S.Guild2),this->m_List[second].Guild);
		this->m_S.Points2 = (WORD)this->m_Points[second];
		this->m_S.Key2 = this->m_List[second].Key;
	}
}

bool CKingOfKira::IsMasterOfRegistered(LPOBJ lpObj,int* slot) // OK
{
	if(lpObj->Guild == 0 || lpObj->GuildStatus != GUILD_STATUS_MASTER)
	{
		return false;
	}

	int found = this->FindSlot(lpObj->Guild->Name);

	if(found < 0)
	{
		return false;
	}

	*slot = found;
	return true;
}

bool CKingOfKira::IsKing(LPOBJ lpObj) // OK
{
	if(this->m_WinHas == 0 || this->m_WinKey == 0)
	{
		return false;
	}

	return (this->PlayerKey(lpObj) == this->m_WinKey);
}

int CKingOfKira::GetChaosBonus(LPOBJ lpObj) // OK
{
	if(this->m_Switch == 0 || this->m_ChaosBonus <= 0 || lpObj->Map != this->m_NpcMap)
	{
		return 0;
	}

	// only at the King's Chaos Goblin (Devias, KingOfKiraGoblinX/Y): the mixer stands next to it while the box is open
	if(this->m_GoblinClass <= 0 || abs(lpObj->X-this->m_GoblinX) > 5 || abs(lpObj->Y-this->m_GoblinY) > 5)
	{
		return 0;
	}

	if(this->IsKing(lpObj) == 0)
	{
		return 0;
	}

	// KingOfKiraBonusAlliance = 0: only members of the winning guild itself, not its allies
	if(this->m_BonusAlliance == 0 && (lpObj->Guild == 0 || _strnicmp(lpObj->Guild->Name,this->m_WinGuild,8) != 0))
	{
		return 0;
	}

	return this->m_ChaosBonus;
}

//////////////////////////////////////////////////////////////////////
// Network
//////////////////////////////////////////////////////////////////////

void CKingOfKira::SendRelay(WORD target,BYTE kind,void* data,int len) // OK
{
	if(len < 0 || len > KOK_RELAY_DATA)
	{
		return;
	}

	SDHP_KOK_RELAY pMsg;
	memset(&pMsg,0,sizeof(pMsg));

	pMsg.type = 0xC1;
	pMsg.size = sizeof(pMsg);
	pMsg.head = KOK_HEAD;
	pMsg.subh = KOK_SUB_RELAY;
	pMsg.Target = target;
	pMsg.FromServer = (WORD)gServerInfo.m_ServerCode;
	pMsg.Kind = kind;
	pMsg.Len = (BYTE)len;

	memcpy(pMsg.Data,data,len);

	gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));
}

void CKingOfKira::SendState(bool force) // OK
{
	if(this->IsHost() == 0)
	{
		return;
	}

	this->SendRelay(KOK_TARGET_ALL,KOK_RELAY_STATE,&this->m_S,sizeof(this->m_S));
}

void CKingOfKira::DGRelayRecv(BYTE* lpMsg,int size) // OK
{
	if(size < (int)sizeof(SDHP_KOK_RELAY))
	{
		return;
	}

	SDHP_KOK_RELAY* p = (SDHP_KOK_RELAY*)lpMsg;

	if(p->Len > KOK_RELAY_DATA)
	{
		return;
	}

	switch(p->Kind)
	{
		case KOK_RELAY_STATE:
		{
			if(p->Len < sizeof(KOK_STATE_DATA) || this->IsHost() != 0)
			{
				break;
			}

			BYTE oldState = this->m_S.State;

			memcpy(&this->m_S,p->Data,sizeof(KOK_STATE_DATA));

			this->m_S.Guild1[8] = 0;
			this->m_S.Guild2[8] = 0;
			this->m_S.OnHill[8] = 0;

			this->m_StateTick = GetTickCount();

			if(oldState != KOK_STATE_FIGHT && this->m_S.State == KOK_STATE_FIGHT)
			{
				this->SendParticipantsToStart(); // the war just started on the host: send this server's participants there
			}

			// the war just ended on the host: the same golden line for the players of this server
			if(oldState == KOK_STATE_FIGHT && this->m_S.State == KOK_STATE_END)
			{
				if(this->m_S.Points1 == this->m_S.Points2)
				{
					this->Announce("[King of Kira] Ended in a draw (%d - %d). Nobody takes the crown.",(int)this->m_S.Points1,(int)this->m_S.Points2);
				}
				else
				{
					bool first = (this->m_S.Points1 > this->m_S.Points2);
					this->Announce("[King of Kira] Congratulations! %s is now the King of Kira! (%d - %d)",first?this->m_S.Guild1:this->m_S.Guild2,(int)(first?this->m_S.Points1:this->m_S.Points2),(int)(first?this->m_S.Points2:this->m_S.Points1));
				}
			}
			break;
		}

		case KOK_RELAY_REGISTER:
		{
			if(p->Len < sizeof(KOK_REG_DATA) || this->IsHost() == 0)
			{
				break;
			}

			KOK_REG_DATA data;
			memcpy(&data,p->Data,sizeof(data));
			data.Name[10] = 0;

			this->HostRegister(&data,p->FromServer);
			break;
		}

		case KOK_RELAY_LIST:
		{
			if(p->Len < sizeof(KOK_ENTRY_DATA) || this->IsHost() != 0)
			{
				break;
			}

			KOK_ENTRY_DATA entry;
			memcpy(&entry,p->Data,sizeof(entry));

			entry.Guild[8] = 0;

			for(int n=0;n < KOK_MAX_ALLY;n++)
			{
				entry.Allies[n][8] = 0;
			}

			if(entry.Total == 0 || entry.Total > KOK_MAX_ENTRIES)
			{
				this->m_ListCount = 0;
			}
			else if(entry.Index < KOK_MAX_ENTRIES)
			{
				this->m_List[entry.Index] = entry;
				this->m_ListCount = entry.Total;
			}

			break;
		}

		case KOK_RELAY_REGREPLY:
		{
			if(p->Len < sizeof(KOK_REGREPLY_DATA))
			{
				break;
			}

			KOK_REGREPLY_DATA data;
			memcpy(&data,p->Data,sizeof(data));
			data.Name[10] = 0;

			if(OBJECT_RANGE(data.aIndex) != 0 && gObjIsConnectedGP(data.aIndex) != 0 && strcmp(gObj[data.aIndex].Name,data.Name) == 0)
			{
				this->SendRegResult(data.aIndex,data.Result);
			}

			break;
		}
	}
}

void CKingOfKira::DGWinnerRecv(SDHP_KOK_WINNER* lpMsg) // OK
{
	// A King is never taken away by the DataServer: "no King" while this server already has one means the DataServer's
	// query failed (the table always keeps the last winner), so the flags keep the current King.
	if(lpMsg->Has == 0 && this->m_WinHas != 0)
	{
		LogAdd(LOG_RED,"[KingOfKira] the DataServer reported no King (query failed?): keeping %s",this->m_WinGuild);
		this->m_WinLoaded = true;
		return;
	}

	this->m_WinLoaded = true;
	this->m_WinHas = lpMsg->Has;
	this->m_WinKey = lpMsg->Key;

	CopyText(this->m_WinGuild,sizeof(this->m_WinGuild),lpMsg->Guild);

	memcpy(this->m_WinMark,lpMsg->Mark,sizeof(this->m_WinMark));

	LogAdd(LOG_BLUE,"[KingOfKira] current King from the DataServer: %s",(this->m_WinHas != 0)?this->m_WinGuild:"(none)");

	this->SendFlagToAll();
}

void CKingOfKira::SendRegResult(int aIndex,BYTE result) // OK
{
	PMSG_KOK_RESULT_SEND pMsg;

	pMsg.header.set(0xD3,0x62,sizeof(pMsg));

	pMsg.Result = result;

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CKingOfKira::SendHud(LPOBJ lpObj) // OK
{
	PMSG_KOK_HUD_SEND pMsg;

	pMsg.header.set(0xD3,0x63,sizeof(pMsg));

	pMsg.State = this->m_S.State;
	pMsg.Contested = this->m_S.Contested;
	pMsg.RemainSec = this->m_S.RemainSec;
	pMsg.Points1 = this->m_S.Points1;
	pMsg.Points2 = this->m_S.Points2;

	memcpy(pMsg.Guild1,this->m_S.Guild1,sizeof(pMsg.Guild1));
	memcpy(pMsg.Guild2,this->m_S.Guild2,sizeof(pMsg.Guild2));
	memcpy(pMsg.OnHill,this->m_S.OnHill,sizeof(pMsg.OnHill));

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
}

void CKingOfKira::SendHudToMap() // OK
{
	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) != 0 && gObj[n].Type == OBJECT_USER && gObj[n].Map == this->m_HillMap)
		{
			this->SendHud(&gObj[n]);
		}
	}
}

void CKingOfKira::SendFlag(LPOBJ lpObj) // OK
{
	PMSG_KOK_FLAG_SEND pMsg;

	pMsg.header.set(0xD3,0x64,sizeof(pMsg));

	pMsg.Has = this->m_WinHas;

	memcpy(pMsg.Guild,this->m_WinGuild,sizeof(pMsg.Guild));
	memcpy(pMsg.Mark,this->m_WinMark,sizeof(pMsg.Mark));

	DataSend(lpObj->Index,(BYTE*)&pMsg,pMsg.header.size);
}

void CKingOfKira::SendFlagToAll() // OK
{
	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) != 0 && gObj[n].Type == OBJECT_USER && gObj[n].Map == this->m_NpcMap)
		{
			this->SendFlag(&gObj[n]);
		}
	}
}

void CKingOfKira::OnEnterMap(LPOBJ lpObj) // OK
{
	if(this->m_Switch == 0 || lpObj->Type != OBJECT_USER)
	{
		return;
	}

	if(lpObj->Map == this->m_NpcMap)
	{
		this->SendFlag(lpObj);
	}

	if(lpObj->Map == this->m_HillMap && (this->m_S.State == KOK_STATE_FIGHT || this->m_S.State == KOK_STATE_END))
	{
		this->SendHud(lpObj);
	}
}

//////////////////////////////////////////////////////////////////////
// NPC + registration
//////////////////////////////////////////////////////////////////////

void CKingOfKira::SpawnGoblin() // OK
{
	if(this->m_Switch == 0 || this->m_GoblinClass <= 0 || this->ServesDevias() == 0)
	{
		return;
	}

	if(this->m_GoblinIndex != -1 && gObjIsConnected(this->m_GoblinIndex) != 0 && gObj[this->m_GoblinIndex].Class == this->m_GoblinClass)
	{
		return;
	}

	int index = gObjAddMonster(this->m_NpcMap);

	if(OBJECT_RANGE(index) == 0)
	{
		this->m_GoblinIndex = -1;
		return;
	}

	if(CBCustomAddMob(index,this->m_GoblinClass,this->m_NpcMap,this->m_GoblinX,this->m_GoblinY,this->m_GoblinDir) == 0)
	{
		this->m_GoblinIndex = -1;
		return;
	}

	this->m_GoblinIndex = index;

	LogAdd(LOG_BLUE,"[KingOfKira] King's Chaos Goblin (class %d) spawned at map %d (%d,%d), index %d",this->m_GoblinClass,this->m_NpcMap,this->m_GoblinX,this->m_GoblinY,index);
}

void CKingOfKira::SpawnNpc() // OK
{
	if(this->m_Switch == 0 || this->ServesDevias() == 0)
	{
		return;
	}

	if(this->m_NpcIndex != -1 && gObjIsConnected(this->m_NpcIndex) != 0)
	{
		return;
	}

	int index = gObjAddMonster(this->m_NpcMap);

	if(OBJECT_RANGE(index) == 0)
	{
		this->m_NpcIndex = -1;
		return;
	}

	if(CBCustomAddMob(index,this->m_NpcClass,this->m_NpcMap,this->m_NpcX,this->m_NpcY,this->m_NpcDir) == 0)
	{
		this->m_NpcIndex = -1;
		return;
	}

	this->m_NpcIndex = index;

	LogAdd(LOG_BLUE,"[KingOfKira] NPC class %d spawned at map %d (%d,%d), index %d",this->m_NpcClass,this->m_NpcMap,this->m_NpcX,this->m_NpcY,index);
}

bool CKingOfKira::Dialog(LPOBJ lpObj,LPOBJ lpNpc) // OK
{
	// any NPC of the configured class on the configured map opens the window (the event's own one, or one placed in MonsterSetBase)
	if(this->m_Switch == 0 || lpNpc->Class != this->m_NpcClass || lpNpc->Map != this->m_NpcMap)
	{
		return false;
	}

	LogAdd(LOG_BLUE,"[KingOfKira] NPC clicked by %s (state %d)",lpObj->Name,this->GetState());

	this->SendFlag(lpObj); // the current King first, so the window can name him

	if(lpObj->Interface.use != 0 || lpObj->PShopOpen != 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"King of Kira: close your other window (trade, shop, storage) first.");
		return true;
	}

	BYTE buff[6+KOK_MAX_ENTRIES*KOK_OPEN_ENTRY_SIZE+16] = {0};

	int count = min(this->m_ListCount,KOK_MAX_ENTRIES);
	int size = 0;

	buff[0] = 0xC2;
	buff[3] = 0xD3;
	buff[4] = 0x60;

	size = 5;

	int state = this->GetState();

	// 0 = no, 1 = his own guild is registered, 2 = his alliance has a registered guild
	BYTE mine = 0;

	int key = this->PlayerKey(lpObj);

	for(int n=0;n < count;n++)
	{
		if(lpObj->Guild != 0 && strcmp(this->m_List[n].Guild,lpObj->Guild->Name) == 0)
		{
			mine = 1;
			break;
		}

		if(key != 0 && this->m_List[n].Key == key)
		{
			mine = 2;
		}
	}

	buff[size++] = (BYTE)state;
	buff[size++] = ((state == KOK_STATE_REGISTER)?1:0);
	buff[size++] = ((lpObj->Guild != 0 && lpObj->GuildStatus == GUILD_STATUS_MASTER)?1:0);
	buff[size++] = mine;
	buff[size++] = (BYTE)(this->m_S.RemainSec & 0xFF);
	buff[size++] = (BYTE)(this->m_S.RemainSec >> 8);
	buff[size++] = 0; // pad: the client reads the count at byte 7 and the entries from byte 8 (without it the window never opened)
	buff[size++] = (BYTE)count;

	for(int n=0;n < count;n++)
	{
		memcpy(&buff[size],this->m_List[n].Guild,9);
		size += 9;
		buff[size++] = this->m_List[n].AllyCount;
		memcpy(&buff[size],this->m_List[n].Allies,KOK_MAX_ALLY*9);
		size += KOK_MAX_ALLY*9;
	}

	buff[1] = (BYTE)(size >> 8);
	buff[2] = (BYTE)(size & 0xFF);

	DataSend(lpObj->Index,buff,size);

	return true;
}

void CKingOfKira::BuildEntry(LPOBJ lpObj,KOK_ENTRY_DATA* entry) // OK
{
	memset(entry,0,sizeof(KOK_ENTRY_DATA));

	entry->Key = this->PlayerKey(lpObj);

	CopyText(entry->Guild,sizeof(entry->Guild),lpObj->Guild->Name);

	// The allies: every other guild of his union. A name the server cannot resolve is left out of the list.
	if(lpObj->Guild->GuildUnion != 0)
	{
		CUnionInfo* lpUnion = gUnionManager.SearchUnion(lpObj->Guild->GuildUnion);

		if(lpUnion != 0)
		{
			if(lpUnion->m_iMasterGuild != lpObj->Guild->Number && lpUnion->m_szMasterGuild[0] != 0 && entry->AllyCount < KOK_MAX_ALLY)
			{
				CopyText(entry->Allies[entry->AllyCount++],9,lpUnion->m_szMasterGuild);
			}

			for(size_t n=0;n < lpUnion->m_vtUnionMember.size() && entry->AllyCount < KOK_MAX_ALLY;n++)
			{
				int number = lpUnion->m_vtUnionMember[n];

				if(number == lpObj->Guild->Number || number == lpUnion->m_iMasterGuild)
				{
					continue;
				}

				GUILD_INFO_STRUCT* lpGuild = gGuildClass.SearchGuild_Number(number);

				if(lpGuild != 0)
				{
					CopyText(entry->Allies[entry->AllyCount++],9,lpGuild->Name);
				}
			}
		}
	}
}

void CKingOfKira::CGRegisterRecv(PMSG_KOK_REG_RECV* lpMsg,int aIndex) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(this->m_Switch == 0)
	{
		this->SendRegResult(aIndex,KOK_REG_DISABLED);
		return;
	}

	// Only next to the NPC: the window is opened by clicking it, but nothing stops a client from sending this anywhere.
	if(lpObj->Map != this->m_NpcMap || abs(lpObj->X-this->m_NpcX) > 12 || abs(lpObj->Y-this->m_NpcY) > 12)
	{
		return;
	}

	if(lpObj->Guild == 0)
	{
		this->SendRegResult(aIndex,KOK_REG_NOGUILD);
		return;
	}

	if(lpObj->GuildStatus != GUILD_STATUS_MASTER)
	{
		this->SendRegResult(aIndex,KOK_REG_NOTMASTER);
		return;
	}

	KOK_REG_DATA data;
	memset(&data,0,sizeof(data));

	data.aIndex = (WORD)aIndex;
	data.Cancel = ((lpMsg->Action == 1)?1:0);

	CopyText(data.Name,sizeof(data.Name),lpObj->Name);

	this->BuildEntry(lpObj,&data.Entry);

	if(this->IsHost() != 0)
	{
		this->HostRegister(&data,(WORD)gServerInfo.m_ServerCode);
		return;
	}

	this->SendRelay((WORD)this->m_HostServer,KOK_RELAY_REGISTER,&data,sizeof(data));
}

void CKingOfKira::HostRegister(KOK_REG_DATA* p,WORD fromServer) // OK
{
	KOK_REGREPLY_DATA reply;
	memset(&reply,0,sizeof(reply));

	reply.aIndex = p->aIndex;
	memcpy(reply.Name,p->Name,sizeof(reply.Name));
	memcpy(reply.Guild,p->Entry.Guild,sizeof(reply.Guild));

	reply.Result = KOK_REG_OK;

	p->Entry.Guild[8] = 0;

	for(int n=0;n < KOK_MAX_ALLY;n++)
	{
		p->Entry.Allies[n][8] = 0;
	}

	if(p->Entry.AllyCount > KOK_MAX_ALLY)
	{
		p->Entry.AllyCount = KOK_MAX_ALLY;
	}

	int found = -1;
	int allied = -1;

	for(int n=0;n < this->m_ListCount;n++)
	{
		if(strcmp(this->m_List[n].Guild,p->Entry.Guild) == 0){found = n;}
		else if(p->Entry.Key != 0 && this->m_List[n].Key == p->Entry.Key){allied = n;}
	}

	if(this->m_S.State != KOK_STATE_REGISTER)
	{
		reply.Result = KOK_REG_CLOSED;
	}
	else if(p->Cancel != 0)
	{
		if(found < 0)
		{
			reply.Result = KOK_REG_NOGUILD;
		}
		else
		{
			for(int n=found;n < this->m_ListCount-1;n++)
			{
				this->m_List[n] = this->m_List[n+1];
			}

			this->m_ListCount--;

			reply.Result = KOK_REG_CANCELLED;

			this->Announce("[King of Kira] %s withdrew its registration.",p->Entry.Guild);
		}
	}
	else if(found >= 0)
	{
		reply.Result = KOK_REG_ALREADY;
	}
	else if(allied >= 0)
	{
		reply.Result = KOK_REG_ALLIED;
	}
	else if(this->m_ListCount >= KOK_MAX_ENTRIES)
	{
		reply.Result = KOK_REG_LIMIT;
	}
	else
	{
		this->m_List[this->m_ListCount] = p->Entry;
		this->m_ListCount++;

		this->Announce("[King of Kira] %s (with its alliance) registered for King of Kira. %d guild%s registered.",p->Entry.Guild,this->m_ListCount,(this->m_ListCount == 1)?"":"s");
	}

	this->SendList();
	this->SendState(true);

	if(fromServer == (WORD)gServerInfo.m_ServerCode)
	{
		if(OBJECT_RANGE(p->aIndex) != 0 && gObjIsConnectedGP(p->aIndex) != 0)
		{
			this->SendRegResult(p->aIndex,reply.Result);
		}
	}
	else
	{
		this->SendRelay(fromServer,KOK_RELAY_REGREPLY,&reply,sizeof(reply));
	}
}

void CKingOfKira::SendList() // OK
{
	if(this->IsHost() == 0)
	{
		return;
	}

	this->m_ListTick = GetTickCount();

	if(this->m_ListCount == 0)
	{
		KOK_ENTRY_DATA empty;
		memset(&empty,0,sizeof(empty));

		this->SendRelay(KOK_TARGET_ALL,KOK_RELAY_LIST,&empty,sizeof(empty));
		return;
	}

	for(int n=0;n < this->m_ListCount;n++)
	{
		KOK_ENTRY_DATA entry = this->m_List[n];

		entry.Index = (WORD)n;
		entry.Total = (WORD)this->m_ListCount;

		this->SendRelay(KOK_TARGET_ALL,KOK_RELAY_LIST,&entry,sizeof(entry));
	}
}

//////////////////////////////////////////////////////////////////////
// Schedule + state machine (host)
//////////////////////////////////////////////////////////////////////

void CKingOfKira::HostSetNextStart() // OK
{
	this->m_NextStart = 0;
	this->m_OpenAt = 0;
	this->m_CloseAt = 0;

	if(this->m_Switch == 0 || this->m_Schedule == 0)
	{
		return;
	}

	time_t now = time(0);

	struct tm t;
	localtime_s(&t,&now);

	t.tm_hour = this->m_Hour;
	t.tm_min = this->m_Minute;
	t.tm_sec = 0;
	t.tm_isdst = -1;

	if(this->m_Schedule == 2)
	{
		t.tm_mday += (this->m_DayOfWeek-t.tm_wday+7)%7;
	}

	time_t next = mktime(&t);

	if(next <= now)
	{
		t.tm_mday += ((this->m_Schedule == 2)?7:1);
		t.tm_isdst = -1;
		next = mktime(&t);
	}

	this->m_NextStart = next;

	// midnight at the start of the event day
	struct tm day;
	localtime_s(&day,&next);
	day.tm_hour = 0;
	day.tm_min = 0;
	day.tm_sec = 0;
	day.tm_isdst = -1;

	time_t dayStart = mktime(&day);

	// Registration opens: N minutes before the start when KingOfKiraRegisterOpenMin > 0. Otherwise (auto) a weekly event opens it the
	// day after the previous event day (Sunday event: Monday 00:00), a daily event right away.
	if(this->m_RegisterOpenMin > 0)
	{
		this->m_OpenAt = next-(time_t)this->m_RegisterOpenMin*60;
	}
	else if(this->m_Schedule == 2)
	{
		day.tm_mday -= 6;
		day.tm_isdst = -1;
		this->m_OpenAt = mktime(&day);
	}
	else
	{
		this->m_OpenAt = 0;
	}

	// ... and closes: N minutes before the start when KingOfKiraRegisterCloseMin >= 0. Auto: a weekly event closes at 00:00 of the event
	// day (Monday to Saturday for a Sunday event), a daily one 10 minutes before the start.
	if(this->m_RegisterCloseMin >= 0)
	{
		this->m_CloseAt = next-(time_t)this->m_RegisterCloseMin*60;
	}
	else if(this->m_Schedule == 2)
	{
		this->m_CloseAt = dayStart;
	}
	else
	{
		this->m_CloseAt = next-600;
	}

	if(this->m_CloseAt > next)
	{
		this->m_CloseAt = next;
	}

	LogAdd(LOG_BLUE,"[KingOfKira] next start %s",ctime(&next));
}

void CKingOfKira::HostClear() // OK
{
	memset(this->m_S.Guild1,0,sizeof(this->m_S.Guild1));
	memset(this->m_S.Guild2,0,sizeof(this->m_S.Guild2));
	memset(this->m_S.OnHill,0,sizeof(this->m_S.OnHill));

	this->m_S.Key1 = 0;
	this->m_S.Key2 = 0;
	this->m_S.Points1 = 0;
	this->m_S.Points2 = 0;
	this->m_S.Contested = 0;
	this->m_S.RemainSec = 0;

	memset(this->m_Points,0,sizeof(this->m_Points));

	memset(this->m_List,0,sizeof(this->m_List));
	this->m_ListCount = 0;

	this->SendList();
}

void CKingOfKira::HostOpenRegistration() // OK
{
	this->m_S.State = KOK_STATE_REGISTER;
	this->m_LastWarnMinute = 0;

	this->m_LastRegAnnounce = GetTickCount();

	this->Announce("[King of Kira] Registration for King of Kira is now open! Guild masters: register at the NPC in Devias (%d,%d).",this->m_NpcX,this->m_NpcY);

	this->SendState(true);
	this->SendList();
}

void CKingOfKira::HostLock() // OK
{
	this->m_S.State = KOK_STATE_LOCKED;

	this->Announce("[King of Kira] Registration is closed: %d guild%s registered.",this->m_ListCount,(this->m_ListCount == 1)?"":"s");

	this->SendState(true);
}

void CKingOfKira::Countdown(int remain) // OK
{
	if(remain > 0 && this->m_WarnMinutes > 0)
	{
		int minutes = (remain+59)/60;

		if(minutes <= this->m_WarnMinutes && this->m_LastWarnMinute != minutes)
		{
			this->m_LastWarnMinute = minutes;

			this->Announce("[King of Kira] King of Kira will start in %d minute%s!",minutes,(minutes == 1)?"":"s");
		}
	}
}

void CKingOfKira::HostCancel(const char* reason) // OK
{
	this->Announce("[King of Kira] Cancelled: %s",reason);

	if(this->m_S.State == KOK_STATE_FIGHT || this->m_S.State == KOK_STATE_END)
	{
		this->HostFinishExit();
		return;
	}

	this->HostClear();

	this->m_S.State = ((this->m_Switch != 0 && this->m_Schedule != 0)?KOK_STATE_IDLE:KOK_STATE_BLANK);

	this->HostSetNextStart();

	this->SendState(true);
}

void CKingOfKira::HostClearMonsters() // OK
{
	for(int n=0;n < MAX_OBJECT_MONSTER;n++)
	{
		if(gObjIsConnected(n) != 0 && gObj[n].Type == OBJECT_MONSTER && gObj[n].Map == this->m_NpcMap && gObj[n].SummonIndex == -1)
		{
			gObjDel(n);
		}
	}
}

void CKingOfKira::HostRespawnMonsters() // OK
{
	int spawned = 0;

	for(int n=0;n < gMonsterSetBase.m_count;n++)
	{
		MONSTER_SET_BASE_INFO* lpInfo = &gMonsterSetBase.m_MonsterSetBaseInfo[n];

		if(lpInfo->Map != this->m_NpcMap || lpInfo->Type == 3 || lpInfo->Type == 4)
		{
			continue;
		}

		int index = gObjAddMonster(lpInfo->Map);

		if(OBJECT_RANGE(index) == 0)
		{
			break;
		}

		if(gObjSetPosMonster(index,n) == 0 || gObjSetMonster(index,lpInfo->MonsterClass) == 0)
		{
			gObjDel(index);
			continue;
		}

		// NPCs were never removed: drop the duplicate the set base just made.
		if(gObj[index].Type != OBJECT_MONSTER)
		{
			gObjDel(index);
			continue;
		}

		spawned++;
	}

	LogAdd(LOG_BLUE,"[KingOfKira] %d monsters respawned on map %d",spawned,this->m_NpcMap);
}

void CKingOfKira::HostStartFight() // OK
{
	// the caller made sure at least two guilds are registered: every one of them fights (free-for-all)
	memset(this->m_Points,0,sizeof(this->m_Points));

	this->m_S.State = KOK_STATE_FIGHT;
	this->UpdateTop();
	this->m_S.Contested = 0;
	memset(this->m_S.OnHill,0,sizeof(this->m_S.OnHill));

	this->m_HoldSlot = -1;
	this->m_HoldSince = 0;
	this->m_LastHoldAnnounce = 0;

	this->m_StateEnd = time(0)+(time_t)this->m_DurationMin*60;

	this->HostClearMonsters();

	{
		char names[200] = {0};

		for(int n=0;n < this->m_ListCount && n < KOK_MAX_ENTRIES;n++)
		{
			if(strlen(names)+strlen(this->m_List[n].Guild)+3 >= sizeof(names)-8)
			{
				strcat_s(names,", ...");
				break;
			}

			if(n > 0)
			{
				strcat_s(names,", ");
			}

			strcat_s(names,this->m_List[n].Guild);
		}

		this->Announce("[King of Kira] King of Kira has started in Devias! %d guilds fight for the throne: %s",this->m_ListCount,names);
	}

	this->SendList(); // every server needs the fighters' alliance keys
	this->SendState(true);

	this->SendParticipantsToStart();
}

bool CKingOfKira::GetStartPoint(int* x,int* y) // OK
{
	GATE_INFO gate;

	if(this->m_StartGate <= 0 || gGate.GetInfo(this->m_StartGate,&gate) == 0 || gate.Map != this->m_NpcMap)
	{
		return false;
	}

	int x1 = min(gate.X,gate.TX), x2 = max(gate.X,gate.TX);
	int y1 = min(gate.Y,gate.TY), y2 = max(gate.Y,gate.TY);

	*x = x1+(int)(GetLargeRand()%(x2-x1+1));
	*y = y1+(int)(GetLargeRand()%(y2-y1+1));

	return true;
}

void CKingOfKira::SendParticipantsToStart() // OK
{
	if(this->m_StartGate <= 0)
	{
		return;
	}

	GATE_INFO gate;

	if(gGate.GetInfo(this->m_StartGate,&gate) == 0)
	{
		LogAdd(LOG_RED,"[KingOfKira] KingOfKiraStartGate %d is not in Gate.txt: participants not moved.",this->m_StartGate);
		return;
	}

	DWORD now = GetTickCount();
	int moved = 0;

	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) == 0)
		{
			continue;
		}

		LPOBJ lpObj = &gObj[n];

		if(lpObj->Type != OBJECT_USER || lpObj->State != OBJECT_PLAYING || lpObj->MapServerMoveQuit != 0 || this->IsParticipant(lpObj) == 0)
		{
			continue;
		}

		this->m_EnforceTick[n] = now;

		gNotice.GCNoticeSend(n,1,0,0,0,0,0,"King of Kira has started! You are being sent to Devias 3.");

		if(this->IsHost() != 0)
		{
			gObjMoveGate(n,this->m_StartGate);
		}
		else
		{
			// the war is played on the host server: move there, landing inside the gate's area
			int x = gate.X+(((gate.TX > gate.X)?(int)(GetLargeRand()%(gate.TX-gate.X+1)):0));
			int y = min(gate.Y,gate.TY)+(((gate.TY != gate.Y)?(int)(GetLargeRand()%(abs(gate.TY-gate.Y)+1)):0));

			if(lpObj->m_OfflineMode == 0)
			{
				lpObj->State = OBJECT_DELCMD;
			}

			GJMapServerMoveSend(n,(WORD)this->m_HostServer,(WORD)gate.Map,(BYTE)x,(BYTE)y);
		}

		moved++;
	}

	LogAdd(LOG_BLUE,"[KingOfKira] war start: %d participant(s) on this server sent to gate %d",moved,this->m_StartGate);
}

void CKingOfKira::HostSaveWinner(int winSlot) // OK
{
	SDHP_KOK_SAVE pMsg;
	memset(&pMsg,0,sizeof(pMsg));

	pMsg.type = 0xC1;
	pMsg.size = sizeof(pMsg);
	pMsg.head = KOK_HEAD;
	pMsg.subh = KOK_SUB_SAVE;

	// the loser column holds the runner-up
	int second = -1;

	for(int n=0;n < this->m_ListCount && n < KOK_MAX_ENTRIES;n++)
	{
		if(n != winSlot && (second == -1 || this->m_Points[n] > this->m_Points[second]))
		{
			second = n;
		}
	}

	CopyText(pMsg.Winner,sizeof(pMsg.Winner),this->m_List[winSlot].Guild);

	if(second >= 0)
	{
		CopyText(pMsg.Loser,sizeof(pMsg.Loser),this->m_List[second].Guild);
		pMsg.LoserPoints = (WORD)this->m_Points[second];
	}

	pMsg.WinnerPoints = (WORD)this->m_Points[winSlot];
	pMsg.WinnerKey = this->m_List[winSlot].Key;

	gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));
}

void CKingOfKira::HostEndFight(bool aborted) // OK
{
	// the highest score wins; a tie for the top means nobody is crowned
	int win = -1;
	int best = -1;
	bool tie = false;

	for(int n=0;n < this->m_ListCount && n < KOK_MAX_ENTRIES;n++)
	{
		if(this->m_Points[n] > best)
		{
			best = this->m_Points[n];
			win = n;
			tie = false;
		}
		else if(this->m_Points[n] == best)
		{
			tie = true;
		}
	}

	if(tie != false || best <= 0)
	{
		win = -1;
	}

	this->UpdateTop();

	this->m_S.State = KOK_STATE_END;
	this->m_StateEnd = time(0)+this->m_ExitDelaySec;
	this->m_S.RemainSec = (WORD)this->m_ExitDelaySec;
	memset(this->m_S.OnHill,0,sizeof(this->m_S.OnHill));

	if(win < 0)
	{
		this->Announce("[King of Kira] Ended in a draw (%d - %d). Nobody takes the crown.",(int)this->m_S.Points1,(int)this->m_S.Points2);
	}
	else
	{
		this->Announce("[King of Kira] Congratulations! %s is now the King of Kira! (%d - %d)",this->m_List[win].Guild,(int)this->m_S.Points1,(int)this->m_S.Points2);

		this->HostSaveWinner(win);

		// crown him on this server right away (the DataServer confirms it to every server a moment later): the flags in Devias
		// change in real time, without waiting for the DataServer round trip or a relog
		this->m_WinLoaded = true;
		this->m_WinHas = 1;
		this->m_WinKey = this->m_List[win].Key;
		CopyText(this->m_WinGuild,sizeof(this->m_WinGuild),this->m_List[win].Guild);

		GUILD_INFO_STRUCT* lpGuild = gGuildClass.SearchGuild(this->m_List[win].Guild);

		if(lpGuild != 0)
		{
			memcpy(this->m_WinMark,lpGuild->Mark,sizeof(this->m_WinMark));
		}

		this->SendFlagToAll();
	}

	this->SendState(true);
	this->SendHudToMap();
}

void CKingOfKira::HostFinishExit() // OK
{
	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) != 0 && gObj[n].Type == OBJECT_USER && gObj[n].Map == this->m_HillMap)
		{
			gObjMoveGate(n,17);
		}
	}

	this->HostRespawnMonsters();

	this->HostClear();

	this->m_S.State = ((this->m_Switch != 0 && this->m_Schedule != 0)?KOK_STATE_IDLE:KOK_STATE_BLANK);

	this->HostSetNextStart();

	this->SendState(true);
}

void CKingOfKira::HostScore() // OK
{
	int count[KOK_MAX_ENTRIES];
	memset(count,0,sizeof(count));

	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) == 0)
		{
			continue;
		}

		LPOBJ lpObj = &gObj[n];

		if(lpObj->Type != OBJECT_USER || lpObj->Map != this->m_HillMap || lpObj->Live == 0 || lpObj->State != OBJECT_PLAYING)
		{
			continue;
		}

		if(lpObj->Y != this->m_HillY || (lpObj->X != this->m_HillX1 && lpObj->X != this->m_HillX2))
		{
			continue;
		}

		int slot = 0;

		if(this->IsMasterOfRegistered(lpObj,&slot) != 0) // only the main guild's master: an ally's master never counts
		{
			count[slot]++;
		}
	}

	memset(this->m_S.OnHill,0,sizeof(this->m_S.OnHill));

	this->m_S.Contested = 0;

	int guildsOnSpot = 0;
	int onlySlot = -1;

	for(int n=0;n < this->m_ListCount && n < KOK_MAX_ENTRIES;n++)
	{
		if(count[n] > 0)
		{
			guildsOnSpot++;
			onlySlot = n;
		}
	}

	// who holds the throne (one guild's master alone on it), and for how long without a break
	int holder = -1;

	if(guildsOnSpot > 1)
	{
		this->m_S.Contested = 1; // masters of two or more guilds on the spot: counting stops
	}
	else if(guildsOnSpot == 1 && count[onlySlot] == 1)
	{
		this->m_Points[onlySlot] = min(this->m_Points[onlySlot]+1,65535);

		CopyText(this->m_S.OnHill,sizeof(this->m_S.OnHill),this->m_List[onlySlot].Guild);

		holder = onlySlot;
	}

	this->UpdateTop();

	time_t now = time(0);

	if(holder != this->m_HoldSlot)
	{
		this->m_HoldSlot = holder;
		this->m_HoldSince = now;
		this->m_LastHoldAnnounce = now;
	}
	else if(holder >= 0 && this->m_HoldAnnounceSec > 0 && (now-this->m_LastHoldAnnounce) >= this->m_HoldAnnounceSec)
	{
		this->m_LastHoldAnnounce = now;

		int held = (int)(now-this->m_HoldSince);

		this->Announce("[King of Kira] %s is holding the throne for %d:%02d! (%d points)",this->m_S.OnHill,held/60,held%60,this->m_Points[holder]);
	}
}

void CKingOfKira::OnUserDie(LPOBJ lpObj,LPOBJ lpTarget) // OK
{
	if(this->m_Switch == 0 || this->IsHost() == 0 || this->m_S.State != KOK_STATE_FIGHT || lpObj->Map != this->m_HillMap || lpObj->Type != OBJECT_USER)
	{
		return;
	}

	int slot = 0;

	if(this->IsMasterOfRegistered(lpObj,&slot) == 0 || this->m_DeathPenalty <= 0)
	{
		return;
	}

	int before = this->m_Points[slot];

	this->m_Points[slot] = max(this->m_Points[slot]-this->m_DeathPenalty,0);

	this->UpdateTop();

	char msg[128];
	sprintf_s(msg,"[King of Kira] The master of %s fell: -%d points.",lpObj->Guild->Name,before-this->m_Points[slot]);

	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) != 0 && gObj[n].Type == OBJECT_USER && gObj[n].Map == this->m_HillMap)
		{
			gNotice.GCNoticeSend(n,1,0,0,0,0,0,"%s",msg);
		}
	}

	this->SendHudToMap();
}

bool CKingOfKira::GetUserRespawnLocation(LPOBJ lpObj,int* gate,int* map,int* x,int* y,int* dir,int* level) // OK
{
	if(this->m_Switch == 0 || this->IsHost() == 0 || lpObj->Map != this->m_HillMap)
	{
		return false;
	}

	if((this->m_S.State != KOK_STATE_FIGHT && this->m_S.State != KOK_STATE_END) || this->IsParticipant(lpObj) == 0)
	{
		return false;
	}

	*gate = 0;
	*map = this->m_HillMap;
	*x = this->m_ArriveX;
	*y = this->m_ArriveY;
	*dir = 3;
	*level = 0;

	return true;
}

//////////////////////////////////////////////////////////////////////
// Warp rules
//////////////////////////////////////////////////////////////////////

int CKingOfKira::WarpFilter(LPOBJ lpObj,int* map,int* x,int* y) // OK
{
	if(this->m_Switch == 0 || lpObj->Type != OBJECT_USER || this->m_S.State != KOK_STATE_FIGHT || *map != this->m_NpcMap)
	{
		return 0;
	}

	if(this->m_GMBypass != 0 && gGameMaster.CheckGameMasterLevel(lpObj,1) != 0)
	{
		return 0;
	}

	bool participant = (this->IsParticipant(lpObj) != 0);

	// Already on the map and not taking part: nothing to filter (the war loop sends him out). A participant is always
	// redirected below, also when he is already in Devias, so every warp to Devias puts him at the start gate.
	if(lpObj->Map == *map && participant == false)
	{
		return 0;
	}

	if(participant != false)
	{
		// a participant warping to Devias during the war (Move window, command, gate) lands at the start gate (Devias 3)
		if(this->GetStartPoint(x,y) == false)
		{
			*x = this->m_ArriveX;
			*y = this->m_ArriveY;
		}

		if(this->IsHost() == 0)
		{
			// Devias on this server is the ordinary one: the event happens on the host.
			if(lpObj->m_OfflineMode == 0)
			{
				lpObj->State = OBJECT_DELCMD;
			}

			GJMapServerMoveSend(lpObj->Index,(WORD)this->m_HostServer,(WORD)*map,(BYTE)*x,(BYTE)*y);
			return 2;
		}

		return 0;
	}

	gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"King of Kira is playing in Devias: you cannot warp there until it ends.");

	return 1;
}

//////////////////////////////////////////////////////////////////////
// Per second work
//////////////////////////////////////////////////////////////////////

void CKingOfKira::HostTick() // OK
{
	time_t now = time(0);

	switch(this->m_S.State)
	{
		case KOK_STATE_BLANK:
		{
			if(this->m_Switch != 0 && this->m_Schedule != 0)
			{
				this->m_S.State = KOK_STATE_IDLE;
				this->HostSetNextStart();
			}

			break;
		}

		case KOK_STATE_IDLE:
		{
			if(this->m_Switch == 0 || this->m_Schedule == 0)
			{
				this->m_S.State = KOK_STATE_BLANK;
				break;
			}

			if(this->m_NextStart == 0)
			{
				this->HostSetNextStart();
			}

			this->m_S.RemainSec = (WORD)min(max((int)(this->m_NextStart-now),0),65535);

			if(this->m_NextStart != 0 && now >= this->m_CloseAt)
			{
				this->HostLock();
			}
			else if(this->m_NextStart != 0 && now >= this->m_OpenAt)
			{
				this->HostOpenRegistration();
			}
			else if(GetTickCount()-this->m_LastIdleSend >= 10000)
			{
				this->m_LastIdleSend = GetTickCount();
				this->SendState(false);
			}

			break;
		}

		case KOK_STATE_REGISTER:
		case KOK_STATE_LOCKED:
		{
			int remain = (int)(this->m_NextStart-now);

			this->m_S.RemainSec = (WORD)min(max(remain,0),65535);

			this->Countdown(remain);

			if(this->m_S.State == KOK_STATE_REGISTER && now >= this->m_CloseAt && remain > 0)
			{
				this->HostLock();
				break;
			}

			// golden reminder every 10 minutes while registration is open
			if(this->m_S.State == KOK_STATE_REGISTER && (GetTickCount()-this->m_LastRegAnnounce) >= 600000)
			{
				this->m_LastRegAnnounce = GetTickCount();
				this->Announce("[King of Kira] Registration for King of Kira is open! %d guild%s registered. Guild masters: register at the NPC in Devias (%d,%d).",this->m_ListCount,(this->m_ListCount == 1)?"":"s",this->m_NpcX,this->m_NpcY);
			}

			if(remain <= 0)
			{
				// free-for-all: at least two guilds, or no event
				if(this->m_ListCount >= 2)
				{
					this->HostStartFight();
				}
				else
				{
					char reason[96];
					sprintf_s(reason,"at least 2 guilds must be registered (%d registered).",this->m_ListCount);
					this->HostCancel(reason);
				}

				break;
			}

			if((GetTickCount()-this->m_ListTick) >= 30000)
			{
				this->SendList();
			}

			this->SendState(false);
			break;
		}

		case KOK_STATE_FIGHT:
		{
			int remain = (int)(this->m_StateEnd-now);

			this->m_S.RemainSec = (WORD)min(max(remain,0),65535);

			this->HostScore();

			if(remain <= 0)
			{
				this->HostEndFight(false);
				break;
			}

			this->SendState(false);
			this->SendHudToMap();
			break;
		}

		case KOK_STATE_END:
		{
			int remain = (int)(this->m_StateEnd-now);

			this->m_S.RemainSec = (WORD)min(max(remain,0),65535);

			if(remain <= 0)
			{
				this->HostFinishExit();
				break;
			}

			this->SendState(false);
			this->SendHudToMap();
			break;
		}
	}
}

void CKingOfKira::EnforceDevias() // OK
{
	// While it plays, Devias belongs to the participants: a registered member is sent to the event map, everybody else is sent to Lorencia.
	if(this->m_S.State != KOK_STATE_FIGHT)
	{
		return;
	}

	DWORD now = GetTickCount();

	for(int n=OBJECT_START_USER;n < MAX_OBJECT;n++)
	{
		if(gObjIsConnectedGP(n) == 0)
		{
			continue;
		}

		LPOBJ lpObj = &gObj[n];

		if(lpObj->Type != OBJECT_USER || lpObj->Map != this->m_NpcMap || lpObj->State != OBJECT_PLAYING || lpObj->MapServerMoveQuit != 0 || lpObj->Teleport != 0)
		{
			continue;
		}

		if(this->m_GMBypass != 0 && gGameMaster.CheckGameMasterLevel(lpObj,1) != 0)
		{
			continue;
		}

		if((now-this->m_EnforceTick[n]) < 5000)
		{
			continue;
		}

		if(this->IsParticipant(lpObj) != 0)
		{
			if(this->IsHost() == 0)
			{
				this->m_EnforceTick[n] = now;

				if(lpObj->m_OfflineMode == 0)
				{
					lpObj->State = OBJECT_DELCMD;
				}

				GJMapServerMoveSend(n,(WORD)this->m_HostServer,(WORD)this->m_NpcMap,(BYTE)this->m_ArriveX,(BYTE)this->m_ArriveY);
			}

			continue;
		}

		this->m_EnforceTick[n] = now;

		gNotice.GCNoticeSend(n,1,0,0,0,0,0,"King of Kira has started: Devias is closed. You are being sent to Lorencia.");

		gObjMoveGate(n,17);
	}
}

void CKingOfKira::MirrorTick() // OK
{
	// A host that went quiet must not leave the whole server locked out of Devias.
	if(this->m_S.State != KOK_STATE_BLANK && (GetTickCount()-this->m_StateTick) > 30000)
	{
		memset(&this->m_S,0,sizeof(this->m_S));
	}
}

void CKingOfKira::MainProc() // OK
{
	if((GetTickCount()-this->m_SecTick) < 1000)
	{
		return;
	}

	this->m_SecTick = GetTickCount();

	if(this->m_Switch == 0)
	{
		return;
	}

	// the King: ask until the DataServer answers (it may not be connected yet when the server starts)
	if(this->m_WinLoaded == false && this->m_WinAsks < 60 && (GetTickCount()-this->m_WinAskTick) >= 5000)
	{
		this->m_WinAskTick = GetTickCount();
		this->m_WinAsks++;

		if(gDataServerConnection.CheckState() != 0)
		{
			SDHP_KOK_LOAD pMsg;

			pMsg.type = 0xC1;
			pMsg.size = sizeof(pMsg);
			pMsg.head = KOK_HEAD;
			pMsg.subh = KOK_SUB_LOAD;
			pMsg.ServerCode = (WORD)gServerInfo.m_ServerCode;

			gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));
		}
	}

	this->SpawnNpc();
	this->SpawnGoblin();

	this->EnforceDevias();

	gServerDisplayer.EventKingOfKira = this->GetDisplayTime();

	if(this->IsHost() != 0)
	{
		this->m_StateTick = GetTickCount();

		this->HostTick();
	}
	else
	{
		this->MirrorTick();
	}
}

//////////////////////////////////////////////////////////////////////
// GM command
//////////////////////////////////////////////////////////////////////

void CKingOfKira::MenuCommand(const char* arg) // OK
{
	if(this->m_Switch == 0)
	{
		LogAdd(LOG_RED,"[KingOfKira] KingOfKiraSwitch is 0 in CustomConfig.ini: nothing started.");
		return;
	}

	if(this->IsHost() == 0)
	{
		LogAdd(LOG_RED,"[KingOfKira] this is server %d, the event runs on server %d: use the menu there.",(int)gServerInfo.m_ServerCode,this->m_HostServer);
		return;
	}

	if(_stricmp(arg,"open") == 0)
	{
		if(this->m_S.State == KOK_STATE_IDLE || this->m_S.State == KOK_STATE_BLANK)
		{
			// Open now and start in 10 minutes unless the schedule is sooner: the golden countdown then runs from there.
			this->m_NextStart = time(0)+600;
			this->m_CloseAt = this->m_NextStart-300;
			this->HostOpenRegistration();
		}
		else if(this->m_S.State == KOK_STATE_LOCKED)
		{
			// Registration already closed for today's war (weekly schedule: it closes at 00:00 of the event day): the
			// administrator reopens it until 5 minutes before the scheduled start (or for 5 minutes when that is too close).
			time_t now = time(0);

			if(this->m_NextStart <= now+360)
			{
				this->m_NextStart = now+600;
			}

			this->m_CloseAt = this->m_NextStart-300;
			this->HostOpenRegistration();
		}
		else
		{
			LogAdd(LOG_RED,"[KingOfKira] registration cannot be opened now (state %d).",this->m_S.State);
		}
	}
	else if(_stricmp(arg,"start") == 0)
	{
		if(this->m_S.State == KOK_STATE_REGISTER || this->m_S.State == KOK_STATE_LOCKED)
		{
			this->m_NextStart = time(0); // the tick starts it, or cancels it when fewer than 2 guilds are registered
		}
	}
	else if(_stricmp(arg,"end") == 0)
	{
		if(this->m_S.State == KOK_STATE_FIGHT)
		{
			this->m_StateEnd = time(0);
		}
	}
	else if(_stricmp(arg,"cancel") == 0)
	{
		if(this->m_S.State == KOK_STATE_FIGHT)
		{
			this->m_StateEnd = time(0); // ends the war now (the winner is whoever leads)
		}
		else if(this->m_S.State != KOK_STATE_IDLE && this->m_S.State != KOK_STATE_BLANK)
		{
			this->HostCancel("cancelled by the administrator.");
		}
	}

	LogAdd(LOG_BLUE,"[KingOfKira] state %d, remain %ds, %d registered, leader %s(%d), next %s(%d)",this->m_S.State,(int)this->m_S.RemainSec,this->m_ListCount,this->m_S.Guild1,(int)this->m_S.Points1,this->m_S.Guild2,(int)this->m_S.Points2);
}

int CKingOfKira::GetDisplayTime() // OK
{
	if(this->m_Switch == 0 || this->IsHost() == 0)
	{
		return -1;
	}

	switch(this->m_S.State)
	{
		case KOK_STATE_IDLE:
		case KOK_STATE_REGISTER:
		case KOK_STATE_LOCKED:
			return max((int)(this->m_NextStart-time(0)),1);
		case KOK_STATE_FIGHT:
		case KOK_STATE_END:
			return 0;
	}

	return -1;
}

bool CKingOfKira::Command(LPOBJ lpObj,char* arg) // OK
{
	if(gGameMaster.CheckGameMasterLevel(lpObj,1) == 0)
	{
		return false;
	}

	if(this->IsHost() == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"King of Kira is run from server %d: use the command there (this is server %d, state %d).",this->m_HostServer,(int)gServerInfo.m_ServerCode,this->GetState());
		return true;
	}

	this->MenuCommand(arg);

	char buff[128];
	sprintf_s(buff,"[KoK] state %d, remain %ds, %d registered, leader %s(%d), next %s(%d), king %s",this->m_S.State,(int)this->m_S.RemainSec,this->m_ListCount,this->m_S.Guild1,(int)this->m_S.Points1,this->m_S.Guild2,(int)this->m_S.Points2,(this->m_WinHas != 0)?this->m_WinGuild:"-");

	gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"%s",buff);

	return true;
}
