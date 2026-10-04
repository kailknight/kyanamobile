// CastleSiegeSync.cpp: implementation of the CCastleSiegeSync class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CastleSiegeSync.h"
#include "DSProtocol.h"
#include "Guild.h"
#include "MapServerManager.h"
#include "User.h"
#include "Union.h"
#include "Util.h"
#include "GuildClass.h"
#include "CastleSiege.h"
#include "Protocol.h"

CCastleSiegeSync gCastleSiegeSync;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCastleSiegeSync::CCastleSiegeSync() // OK
{
	this->Clear();
}

CCastleSiegeSync::~CCastleSiegeSync() // OK
{

}

void CCastleSiegeSync::Clear() // OK
{
	this->m_CurCastleState = -1;
	this->m_SiegeGuilds.clear();
	this->m_SiegeGuildsLoaded = false;
	this->m_SiegeGuildsRequestTick = 0;
	this->m_CurTaxRateChaos = 0;
	this->m_CurTaxRateStore = 0;
	this->m_CastleTributeMoney = 0;
	this->m_CsTributeMoneyTimer = 0;
	memset(this->m_CastleOwnerGuild,0,sizeof(this->m_CastleOwnerGuild));
}

int CCastleSiegeSync::GetCastleState() // OK
{
	return this->m_CurCastleState;
}

int CCastleSiegeSync::GetTaxRateChaos(int aIndex) // OK
{
	int CurTaxRateChaos = this->m_CurTaxRateChaos;

	if(this->CheckCastleOwnerMember(aIndex) != 0 || this->CheckCastleOwnerUnionMember(aIndex) != 0)
	{
		CurTaxRateChaos = 0;
	}

	(CurTaxRateChaos > 3) ? 3 : CurTaxRateChaos;
	(CurTaxRateChaos < 0) ? 0 : CurTaxRateChaos;

	return CurTaxRateChaos;
}

int CCastleSiegeSync::GetTaxRateStore(int aIndex) // OK
{
	int CurTaxRateStore = this->m_CurTaxRateStore;

	if(this->CheckCastleOwnerMember(aIndex) != 0 || this->CheckCastleOwnerUnionMember(aIndex) != 0)
	{
		CurTaxRateStore = 0;
	}

	(CurTaxRateStore > 3) ? 3 : CurTaxRateStore;
	(CurTaxRateStore < 0) ? 0 : CurTaxRateStore;

	return CurTaxRateStore;
}

int CCastleSiegeSync::GetTaxHuntZone(int aIndex,bool CheckOwnerGuild) // OK
{
	int CurTaxHuntZone = this->m_CurTaxHuntZone;

	if(CheckOwnerGuild != 0)
	{
		if(this->CheckCastleOwnerMember(aIndex) != 0 || this->CheckCastleOwnerUnionMember(aIndex) != 0)
		{
			CurTaxHuntZone = 0;
		}
	}

	(CurTaxHuntZone > 3) ? 3 : CurTaxHuntZone;
	(CurTaxHuntZone < 0) ? 0 : CurTaxHuntZone;

	return CurTaxHuntZone;
}

char* CCastleSiegeSync::GetCastleOwnerGuild() // OK
{
	return this->m_CastleOwnerGuild;
}

bool CCastleSiegeSync::CheckCastleOwnerMember(int aIndex) // OK
{
	int iIndex=aIndex;
	if ( gObjIsConnected(iIndex) == FALSE )
	{
		return FALSE;
	}

	if ( strcmp(this->m_CastleOwnerGuild, "") == 0 )
	{
		return FALSE;
	}

	if ( strcmp(gObj[iIndex].GuildName, this->m_CastleOwnerGuild) != 0 )
	{
		return FALSE;
	}

	return TRUE;
}

bool CCastleSiegeSync::CheckCastleOwnerUnionMember(int aIndex) // OK
{
	int iIndex=aIndex;
	if ( gObjIsConnected(iIndex) == FALSE )
	{
		return FALSE;
	}

	if ( strcmp(this->m_CastleOwnerGuild, "") == 0 )
	{
		return FALSE;
	}

	GUILD_INFO_STRUCT * lpGuildInfo = gObj[iIndex].Guild;
	
	if ( lpGuildInfo == NULL )
	{
		return FALSE;
	}
	
	CUnionInfo * pUnionInfo = gUnionManager.SearchUnion(lpGuildInfo->GuildUnion);

	if ( pUnionInfo == NULL )
	{
		return FALSE;
	}

	if ( strcmp( pUnionInfo->m_szMasterGuild, this->m_CastleOwnerGuild) == 0 )
	{
		return TRUE;
	}

	return FALSE;
}

void CCastleSiegeSync::ResetTributeMoney() // OK
{
	InterlockedExchange((long*)&this->m_CastleTributeMoney,0);
}

void CCastleSiegeSync::AddTributeMoney(int money) // OK
{
	if(this->m_CastleTributeMoney < 0)
	{
		InterlockedExchange((long*)&this->m_CastleTributeMoney,0);
	}

	if(money <= 0)
	{
		return;
	}

	if((this->m_CastleTributeMoney+money) > MAX_TRIBUTE_MONEY)
	{
		return;
	}

	InterlockedExchangeAdd((long*)&this->m_CastleTributeMoney,money);
}

void CCastleSiegeSync::AdjustTributeMoney()
{
	if ( this->m_CastleTributeMoney < 0 )
	{
		InterlockedExchange((LPLONG)&this->m_CastleTributeMoney, 0);
	}

	if ( this->m_CastleTributeMoney == 0 )
	{
		return;
	}

	this->m_CsTributeMoneyTimer++;

	this->m_CsTributeMoneyTimer %= 180;

	if ( this->m_CsTributeMoneyTimer != 0 )
	{
		return;
	}

	GS_GDReqCastleTributeMoney(gMapServerManager.GetMapServerGroup(), this->m_CastleTributeMoney);
}

void CCastleSiegeSync::SetCastleOwnerGuild(char* GuildName) // OK
{
	memset(this->m_CastleOwnerGuild,0,sizeof(this->m_CastleOwnerGuild));
	memcpy(this->m_CastleOwnerGuild,GuildName,(sizeof(this->m_CastleOwnerGuild)/2));
}

void CCastleSiegeSync::SetCastleState(int state) // OK
{
	const bool wasLocked = (this->m_CurCastleState >= CASTLESIEGE_STATE_NOTIFY && this->m_CurCastleState <= CASTLESIEGE_STATE_STARTSIEGE);
	const bool isLocked = (state >= CASTLESIEGE_STATE_NOTIFY && state <= CASTLESIEGE_STATE_STARTSIEGE);

	this->m_CurCastleState = state;

	// Entering announce/ready/battle (or starting up inside it): the
	// participants are final by now, so fetch them once. Leaving it: forget
	// them, the next siege has its own.
	if(isLocked != wasLocked)
	{
		this->m_SiegeGuilds.clear();
		this->m_SiegeGuildsLoaded = false;
		this->m_SiegeGuildsRequestTick = 0;

		if(isLocked)
		{
			this->RequestSiegeGuilds();
		}
	}
}

void CCastleSiegeSync::RequestSiegeGuilds() // OK
{
	#if(GAMESERVER_TYPE==0)

	// GameServerCS loads the same list for its own use; only the ordinary
	// GameServers ask for it here. Not more than once every 5 seconds.
	if(this->m_SiegeGuildsRequestTick != 0 && (GetTickCount()-this->m_SiegeGuildsRequestTick) < 5000)
	{
		return;
	}

	this->m_SiegeGuildsRequestTick = GetTickCount();

	GS_GDReqCsLoadTotalGuildInfo(gMapServerManager.GetMapServerGroup());

	#endif
}

void CCastleSiegeSync::SetSiegeGuilds(CSP_CSLOADTOTALGUILDINFO* list, int count) // OK
{
	this->m_SiegeGuilds.clear();

	for(int n=0;list != 0 && n < count;n++)
	{
		char name[9] = {0};

		memcpy(name,list[n].szGuildName,8);

		if(name[0] != 0)
		{
			this->m_SiegeGuilds.insert(std::string(name));
		}
	}

	this->m_SiegeGuildsLoaded = true;
}

bool CCastleSiegeSync::IsSiegeGuild(char* guildName, int guildUnion) // OK
{
	if(guildName == 0 || guildName[0] == 0)
	{
		return false;
	}

	if(this->m_CastleOwnerGuild[0] != 0 && strcmp(guildName,this->m_CastleOwnerGuild) == 0)
	{
		return true;
	}

	if(this->m_SiegeGuilds.find(std::string(guildName)) != this->m_SiegeGuilds.end())
	{
		return true;
	}

	// A guild in an alliance fights on its alliance master's side.
	if(guildUnion != 0)
	{
		GUILD_INFO_STRUCT* lpUnionMaster = gGuildClass.SearchGuild_Number(guildUnion);

		if(lpUnionMaster != 0 && strcmp(lpUnionMaster->Name,guildName) != 0)
		{
			if(this->m_CastleOwnerGuild[0] != 0 && strcmp(lpUnionMaster->Name,this->m_CastleOwnerGuild) == 0)
			{
				return true;
			}

			if(this->m_SiegeGuilds.find(std::string(lpUnionMaster->Name)) != this->m_SiegeGuilds.end())
			{
				return true;
			}
		}
	}

	return false;
}

void CCastleSiegeSync::SetTaxRateChaos(int rate) // OK
{
	this->m_CurTaxRateChaos = rate;
}

void CCastleSiegeSync::SetTaxRateStore(int rate) // OK
{
	this->m_CurTaxRateStore = rate;
}

void CCastleSiegeSync::SetTaxHuntZone(int rate) // OK
{
	this->m_CurTaxHuntZone = rate;
}

int CCastleSiegeSync::GetTributeMoney() // OK
{
	return this->m_CastleTributeMoney;
}
