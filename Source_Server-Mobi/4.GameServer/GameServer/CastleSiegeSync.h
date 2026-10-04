// CastleSiegeSync.h: interface for the CCastleSiegeSync class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#define MAX_TRIBUTE_MONEY MAX_MONEY

#include <set>
#include <string>

struct CSP_CSLOADTOTALGUILDINFO;

class CCastleSiegeSync
{
public:
	CCastleSiegeSync();
	virtual ~CCastleSiegeSync();
	void Clear();
	int GetCastleState();
	int GetTaxRateChaos(int aIndex);
	int GetTaxRateStore(int aIndex);
	int GetTaxHuntZone(int aIndex,bool CheckOwnerGuild);
	char* GetCastleOwnerGuild();
	bool CheckCastleOwnerMember(int aIndex);
	bool CheckCastleOwnerUnionMember(int aIndex);
	void ResetTributeMoney();
	void AddTributeMoney(int money);
	void AdjustTributeMoney();
	void SetCastleOwnerGuild(char* GuildName);
	void SetCastleState(int state);
	void SetTaxRateChaos(int rate);
	void SetTaxRateStore(int rate);
	void SetTaxHuntZone(int rate);
	int GetTributeMoney();
	// The guilds fighting the current siege (attackers and defenders, as
	// GameServerCS registered them), fetched from the DataServer when the
	// siege reaches announce/ready/battle. Only these - plus the castle owner
	// and anyone allied to one of them - are frozen out of alliance and
	// hostility changes; every other guild may change them during a siege.
	void RequestSiegeGuilds();
	void SetSiegeGuilds(CSP_CSLOADTOTALGUILDINFO* list, int count);
	bool IsSiegeGuildListLoaded() { return this->m_SiegeGuildsLoaded; }
	bool IsSiegeGuild(char* guildName, int guildUnion);
private:
	std::set<std::string> m_SiegeGuilds;
	bool m_SiegeGuildsLoaded;
	DWORD m_SiegeGuildsRequestTick;
	int m_CurCastleState;
	int m_CurTaxRateChaos;
	int m_CurTaxRateStore;
	int m_CurTaxHuntZone;
	int m_CastleTributeMoney;
	char m_CastleOwnerGuild[16];
	int m_CsTributeMoneyTimer;
};

extern CCastleSiegeSync gCastleSiegeSync;
