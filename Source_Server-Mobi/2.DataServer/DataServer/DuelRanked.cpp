// DuelRanked.cpp: Ranked 1v1 matchmaking for the Duel Arena, DataServer side.
//
// See Working Project\MD Files\RankedDuel-PLAN.md (sections 3-6) and
// DuelRanked_Schema.sql. Players are looked up by NAME (gCharacterManager) and
// pushed to through the GameServer they are registered on.
//
// Everything below runs on the queue thread (DataServerProtocolCore), except
// LoadConfig(true) at start, PostTick/PostReload (UI thread) and OnServerDown
// (IOCP thread). Names reach SQL as bound parameters only (see MonthlyCard.cpp);
// gQueryManager is one unlocked statement, so every Fetch/Close finishes before
// the next ExecQuery.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <algorithm>
#include "DuelRanked.h"
#include "DataServerProtocol.h"
#include "FriendMail.h"
#include "CharacterManager.h"
#include "QueryManager.h"
#include "ServerManager.h"
#include "SocketManager.h"
#include "Util.h"

CDuelRanked gDuelRanked;

namespace
{
	const char* kDuelRankedConfig = ".\\DuelRanked.ini";

	// Prompt tokens have this bit set; a database MatchId never does.
	const DWORD kTokenBit = 0x80000000;

	// A fighter sent home after a void is still hopping between servers for a
	// few seconds: its C1:71 must not drop it from the queue.
	const int kHomeHopSec = 15;

	// A fighter that left his origin for the host but had not landed when the
	// match died: the host sends him back when he appears (its pending-home wait
	// is 50 s), so he must not be paired or dropped before that.
	const int kTransitHomeSec = 60;

	// Re-sending GO to a fighter that registered somewhere else.
	const DWORD kGoResendMs = 3000;

	const char* kDefaultTierName[7] = {"Iron","Bronze","Silver","Gold","Platinum","Legend","Duel Master"};
	const char* kDefaultTierDivisions = "3,3,3,3,3,3,1";
	const char* kDefaultSteps = "0,1067,1134,1200,1267,1334,1400,1467,1534,1600,1667,1734,1800,1867,1934,2000,2067,2134,2200";

	void CopyText(char* out,int outSize,const char* in,int inSize)
	{
		memset(out,0,outSize);
		int n = 0;
		while(n < inSize && n < outSize - 1 && in[n] != 0)
		{
			out[n] = in[n];
			n++;
		}
	}

	std::string Key(const char* name)
	{
		std::string key(name);

		for(size_t n = 0; n < key.size(); n++)
		{
			if(key[n] >= 'A' && key[n] <= 'Z')
				key[n] = (char)(key[n] + ('a' - 'A'));
		}

		return key;
	}

	std::string PairKey(const char* a,const char* b)
	{
		std::string ka = Key(a);
		std::string kb = Key(b);
		return (ka < kb) ? (ka + "|" + kb) : (kb + "|" + ka);
	}

	WORD ClampWord(int value)
	{
		return (WORD)((value < 0) ? 0 : ((value > 65535) ? 65535 : value));
	}

	BYTE ClampByte(int value)
	{
		return (BYTE)((value < 0) ? 0 : ((value > 255) ? 255 : value));
	}

	DWORD After(DWORD now,int seconds)
	{
		return now + (DWORD)seconds * 1000;
	}

	bool Past(DWORD deadline,DWORD now)
	{
		return ((int)(now - deadline)) >= 0;
	}

	// "0,1067, 1134" -> numbers. Returns the count, or -1 when something is not a number.
	int ParseList(const char* text,int* out,int maxCount)
	{
		int count = 0;
		const char* p = text;

		while(true)
		{
			while(*p == ' ' || *p == '\t')
				p++;

			if(*p == 0)
				break;

			bool negative = false;

			if(*p == '-')
			{
				negative = true;
				p++;
			}

			if(*p < '0' || *p > '9')
				return -1;

			long long value = 0;

			while(*p >= '0' && *p <= '9')
			{
				value = value * 10 + (*p - '0');

				if(value > 1000000000)
					return -1;

				p++;
			}

			while(*p == ' ' || *p == '\t')
				p++;

			if(count >= maxCount)
				return -1;

			out[count++] = (int)(negative ? -value : value);

			if(*p == ',')
			{
				p++;
				continue;
			}

			if(*p != 0)
				return -1;

			break;
		}

		return count;
	}
}

CDuelRanked::CDuelRanked()
{
	this->m_CfgValid = false;
	this->m_Enabled = false;
	this->DefaultConfig(&this->m_Cfg);

	this->m_NextToken = 0;
	this->m_PairHold = 0;

	memset(&this->m_Host,0,sizeof(this->m_Host));
	this->m_HostReported = false;
	this->m_HostReportTick = 0;
	this->m_UnavailableSince = 0;

	this->m_TickCount = 0;
	this->m_TickPending = 0;
	this->m_ReloadPending = 0;
	this->m_Active = 0;
	this->m_TickPostTick = 0;
}

//////////////////////////////////////////////////////////////////////
// Configuration, ranks
//////////////////////////////////////////////////////////////////////

void CDuelRanked::DefaultConfig(DR_CONFIG* cfg)
{
	memset(cfg,0,sizeof(*cfg));

	cfg->StartMMR = 1000;
	cfg->KFactor = 32;
	cfg->MMRFloor = 0;

	cfg->TierCount = 7;

	for(int n = 0; n < 7; n++)
		strcpy_s(cfg->TierName[n],sizeof(cfg->TierName[n]),kDefaultTierName[n]);

	cfg->Enabled = 1;
	cfg->HostServer = 19;
	cfg->MaxLoadPercent = 80;
	cfg->MaxConcurrentMatches = 3;
	cfg->MaxSpectators = 10;
	cfg->BaseWindow = 100;
	cfg->WidenStep = 50;
	cfg->WidenSecs = 15;
	cfg->MaxWindow = 400;
	cfg->AnyOpponentAfterSec = 120;
	cfg->RematchCooldownSec = 300;
	cfg->SameDeviceLimit = 3;
	cfg->SameDeviceDays = 7;
	cfg->DeclineCooldownSec = 30;
	cfg->NoShowCooldownSec = 60;
	cfg->UnavailableDropSec = 60;

	cfg->PromptSec = 15;
	cfg->PromptGraceSec = 2;
	cfg->StageTimeoutSec = 60;
	cfg->MaxMatchSec = 600;
	cfg->ReconnectGraceSec = 60;
	cfg->WatchdogExtraSec = 120;
	cfg->HostSilentSec = 15;
}

// 0 = valid, 1 = no file, 2 = the rank table (or a number the table needs) is wrong.
// Everything else is clamped to something usable instead of refusing to run.
int CDuelRanked::ReadConfig(DR_CONFIG* cfg,char* error,int errorSize)
{
	this->DefaultConfig(cfg);

	error[0] = 0;

	if(GetFileAttributesA(kDuelRankedConfig) == INVALID_FILE_ATTRIBUTES)
	{
		_snprintf_s(error,errorSize,_TRUNCATE,"DuelRanked.ini not found");
		return 1;
	}

	const char* file = kDuelRankedConfig;

	// [Rating]
	cfg->StartMMR = (int)GetPrivateProfileIntA("Rating","StartMMR",cfg->StartMMR,file);
	cfg->KFactor = (int)GetPrivateProfileIntA("Rating","KFactor",cfg->KFactor,file);
	cfg->MMRFloor = (int)GetPrivateProfileIntA("Rating","MMRFloor",cfg->MMRFloor,file);

	if(cfg->MMRFloor < 0) cfg->MMRFloor = 0;
	if(cfg->StartMMR < cfg->MMRFloor) cfg->StartMMR = cfg->MMRFloor;
	if(cfg->StartMMR > 60000) cfg->StartMMR = 60000;
	if(cfg->KFactor < 1) cfg->KFactor = 1;
	if(cfg->KFactor > 200) cfg->KFactor = 200;

	// [Ranks]
	cfg->TierCount = (int)GetPrivateProfileIntA("Ranks","TierCount",cfg->TierCount,file);

	if(cfg->TierCount < 1 || cfg->TierCount > DUELRANKED_MAX_TIERS)
	{
		_snprintf_s(error,errorSize,_TRUNCATE,"[Ranks] TierCount %d is not 1..%d",cfg->TierCount,DUELRANKED_MAX_TIERS);
		return 2;
	}

	// Tier0Name = the lowest tier. A file that starts at Tier1Name is read that way.
	char probe[24];
	const int nameBase = (GetPrivateProfileStringA("Ranks","Tier0Name","",probe,sizeof(probe),file) == 0 && GetPrivateProfileStringA("Ranks","Tier1Name","",probe,sizeof(probe),file) != 0) ? 1 : 0;

	for(int n = 0; n < cfg->TierCount; n++)
	{
		char key[32];
		char fallback[24];
		_snprintf_s(key,sizeof(key),_TRUNCATE,"Tier%dName",nameBase + n);

		if(n < 7)
			CopyText(fallback,sizeof(fallback),kDefaultTierName[n],23);
		else
			_snprintf_s(fallback,sizeof(fallback),_TRUNCATE,"Tier %d",n);

		GetPrivateProfileStringA("Ranks",key,fallback,cfg->TierName[n],sizeof(cfg->TierName[n]),file);
	}

	char text[512];
	char text2[512];
	int divisions[DUELRANKED_MAX_TIERS + 1];

	// TierNDivisions=3,3,3,3,3,3,1 (TierDivisions is accepted too).
	GetPrivateProfileStringA("Ranks","TierNDivisions",kDefaultTierDivisions,text,sizeof(text),file);
	GetPrivateProfileStringA("Ranks","TierDivisions",text,text2,sizeof(text2),file);
	strcpy_s(text,sizeof(text),text2);

	int divisionCount = ParseList(text,divisions,DUELRANKED_MAX_TIERS + 1);

	if(divisionCount != cfg->TierCount)
	{
		_snprintf_s(error,errorSize,_TRUNCATE,"[Ranks] TierDivisions has %d values for TierCount %d",divisionCount,cfg->TierCount);
		return 2;
	}

	int stepsNeeded = 0;

	for(int n = 0; n < cfg->TierCount; n++)
	{
		if(divisions[n] < 1 || divisions[n] > 3)
		{
			_snprintf_s(error,errorSize,_TRUNCATE,"[Ranks] tier %d has %d divisions (1..3)",n,divisions[n]);
			return 2;
		}

		cfg->TierDivisions[n] = divisions[n];
		stepsNeeded += divisions[n];
	}

	// [Steps] Steps=...: one ascending list of lower bounds, assigned to the tiers in order.
	int steps[DUELRANKED_MAX_STEPS + 1];

	GetPrivateProfileStringA("Steps","Steps",kDefaultSteps,text,sizeof(text),file);

	int stepCount = ParseList(text,steps,DUELRANKED_MAX_STEPS + 1);

	if(stepCount < 0 || stepCount > DUELRANKED_MAX_STEPS)
	{
		_snprintf_s(error,errorSize,_TRUNCATE,"[Steps] Steps is not a list of 1..%d numbers",DUELRANKED_MAX_STEPS);
		return 2;
	}

	if(stepCount != stepsNeeded)
	{
		_snprintf_s(error,errorSize,_TRUNCATE,"[Steps] has %d steps but the tiers need %d",stepCount,stepsNeeded);
		return 2;
	}

	if(steps[0] != 0)
	{
		_snprintf_s(error,errorSize,_TRUNCATE,"[Steps] the first step must be 0, not %d",steps[0]);
		return 2;
	}

	for(int n = 1; n < stepCount; n++)
	{
		if(steps[n] <= steps[n - 1])
		{
			_snprintf_s(error,errorSize,_TRUNCATE,"[Steps] step %d (%d) is not above step %d (%d)",n,steps[n],n - 1,steps[n - 1]);
			return 2;
		}
	}

	if(steps[stepCount - 1] > 65535)
	{
		_snprintf_s(error,errorSize,_TRUNCATE,"[Steps] the last step %d does not fit 16 bits",steps[stepCount - 1]);
		return 2;
	}

	cfg->StepCount = stepCount;

	int step = 0;

	for(int tier = 0; tier < cfg->TierCount; tier++)
	{
		for(int j = 0; j < cfg->TierDivisions[tier]; j++)
		{
			cfg->Step[step] = steps[step];
			cfg->StepTier[step] = tier;
			// III, II, I from the lowest step up; a one-division tier has none.
			cfg->StepDiv[step] = (cfg->TierDivisions[tier] == 1) ? 0 : (cfg->TierDivisions[tier] - j);
			step++;
		}
	}

	// [Match]
	// [Rewards] TierN=WCoinC,WCoinP,GoblinPoint (Tier0 = the lowest tier). Missing = nothing.
	for(int n = 0; n < cfg->TierCount; n++)
	{
		char key[16];
		char value[64];
		_snprintf_s(key,sizeof(key),_TRUNCATE,"Tier%d",n);
		GetPrivateProfileStringA("Rewards",key,"0,0,0",value,sizeof(value),file);
		int c = 0, p = 0, g = 0;
		sscanf_s(value,"%d,%d,%d",&c,&p,&g);
		cfg->RewardWCoinC[n] = (c > 0) ? c : 0;
		cfg->RewardWCoinP[n] = (p > 0) ? p : 0;
		cfg->RewardGoblin[n] = (g > 0) ? g : 0;

		// [RewardItems] TierN_ItemK=Section,Index,Level,Durability,Skill,Luck,Option,Excellent
		cfg->RewardItemCount[n] = 0;

		for(int k = 1; k <= DUELRANKED_MAX_REWARD_ITEMS; k++)
		{
			_snprintf_s(key,sizeof(key),_TRUNCATE,"Tier%d_Item%d",n,k);

			if(GetPrivateProfileStringA("RewardItems",key,"",value,sizeof(value),file) == 0)
				continue;

			int v[8] = {-1,-1,0,1,0,0,0,0};
			sscanf_s(value,"%d,%d,%d,%d,%d,%d,%d,%d",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7]);

			if(v[0] < 0 || v[0] > 15 || v[1] < 0 || v[1] > 511)
			{
				LogAdd(LOG_RED,"[DuelRanked] [RewardItems] %s=%s ignored: section 0..15, index 0..511",key,value);
				continue;
			}

			BYTE* it = cfg->RewardItem[n][cfg->RewardItemCount[n]++];
			it[0] = (BYTE)v[0];
			it[1] = (BYTE)(v[1] & 0xFF);
			it[2] = (BYTE)(v[1] >> 8);
			it[3] = (BYTE)((v[2] < 0) ? 0 : ((v[2] > 15) ? 15 : v[2]));		// level
			it[4] = (BYTE)((v[3] < 1) ? 1 : ((v[3] > 255) ? 255 : v[3]));		// durability / stack
			it[5] = (BYTE)(((v[4] != 0) ? 1 : 0) | ((v[5] != 0) ? 2 : 0));		// skill | luck
			it[6] = (BYTE)((v[6] < 0) ? 0 : ((v[6] > 7) ? 7 : v[6]));			// option (+4 steps)
			it[7] = (BYTE)(v[7] & 63);											// excellent bits
		}
	}

	cfg->Enabled = (int)GetPrivateProfileIntA("Match","Enabled",cfg->Enabled,file);
	cfg->HostServer = (int)GetPrivateProfileIntA("Match","HostServer",cfg->HostServer,file);
	cfg->MaxLoadPercent = (int)GetPrivateProfileIntA("Match","MaxLoadPercent",cfg->MaxLoadPercent,file);
	cfg->MaxConcurrentMatches = (int)GetPrivateProfileIntA("Match","MaxConcurrentMatches",cfg->MaxConcurrentMatches,file);
	cfg->MaxSpectators = (int)GetPrivateProfileIntA("Match","MaxSpectators",cfg->MaxSpectators,file);
	cfg->BaseWindow = (int)GetPrivateProfileIntA("Match","BaseWindow",cfg->BaseWindow,file);
	cfg->WidenStep = (int)GetPrivateProfileIntA("Match","WidenStep",cfg->WidenStep,file);
	cfg->WidenSecs = (int)GetPrivateProfileIntA("Match","WidenSecs",cfg->WidenSecs,file);
	cfg->MaxWindow = (int)GetPrivateProfileIntA("Match","MaxWindow",cfg->MaxWindow,file);
	cfg->AnyOpponentAfterSec = (int)GetPrivateProfileIntA("Match","AnyOpponentAfterSec",cfg->AnyOpponentAfterSec,file);
	cfg->RematchCooldownSec = (int)GetPrivateProfileIntA("Match","RematchCooldownSec",cfg->RematchCooldownSec,file);
	cfg->SameDeviceLimit = (int)GetPrivateProfileIntA("Match","SameDeviceLimit",cfg->SameDeviceLimit,file);
	cfg->SameDeviceDays = (int)GetPrivateProfileIntA("Match","SameDeviceDays",cfg->SameDeviceDays,file);
	cfg->DeclineCooldownSec = (int)GetPrivateProfileIntA("Match","DeclineCooldownSec",cfg->DeclineCooldownSec,file);
	cfg->NoShowCooldownSec = (int)GetPrivateProfileIntA("Match","NoShowCooldownSec",cfg->NoShowCooldownSec,file);
	cfg->UnavailableDropSec = (int)GetPrivateProfileIntA("Match","UnavailableDropSec",cfg->UnavailableDropSec,file);

	// [Timing]
	cfg->PromptSec = (int)GetPrivateProfileIntA("Timing","PromptSec",cfg->PromptSec,file);
	cfg->PromptGraceSec = (int)GetPrivateProfileIntA("Timing","PromptGraceSec",cfg->PromptGraceSec,file);
	cfg->StageTimeoutSec = (int)GetPrivateProfileIntA("Timing","StageTimeoutSec",cfg->StageTimeoutSec,file);
	cfg->MaxMatchSec = (int)GetPrivateProfileIntA("Timing","MaxMatchSec",cfg->MaxMatchSec,file);
	cfg->ReconnectGraceSec = (int)GetPrivateProfileIntA("Timing","ReconnectGraceSec",cfg->ReconnectGraceSec,file);
	cfg->WatchdogExtraSec = (int)GetPrivateProfileIntA("Timing","WatchdogExtraSec",cfg->WatchdogExtraSec,file);
	cfg->HostSilentSec = (int)GetPrivateProfileIntA("Timing","HostSilentSec",cfg->HostSilentSec,file);

	// A bad value here must not stop the tick or divide by zero.
	if(cfg->HostServer < 0 || cfg->HostServer > 65534) cfg->HostServer = 19;
	if(cfg->MaxLoadPercent < 1) cfg->MaxLoadPercent = 1;
	if(cfg->MaxLoadPercent > 100) cfg->MaxLoadPercent = 100;
	if(cfg->MaxConcurrentMatches < 1) cfg->MaxConcurrentMatches = 1;
	if(cfg->MaxConcurrentMatches > 4) cfg->MaxConcurrentMatches = 4;
	if(cfg->MaxSpectators < 0) cfg->MaxSpectators = 0;
	if(cfg->BaseWindow < 0) cfg->BaseWindow = 0;
	if(cfg->WidenStep < 0) cfg->WidenStep = 0;
	if(cfg->WidenSecs < 1) cfg->WidenSecs = 1;
	if(cfg->MaxWindow < cfg->BaseWindow) cfg->MaxWindow = cfg->BaseWindow;
	if(cfg->AnyOpponentAfterSec < 0) cfg->AnyOpponentAfterSec = 0;
	if(cfg->RematchCooldownSec < 0) cfg->RematchCooldownSec = 0;
	if(cfg->SameDeviceLimit < 0) cfg->SameDeviceLimit = 0;
	if(cfg->SameDeviceDays < 1) cfg->SameDeviceDays = 1;
	if(cfg->SameDeviceDays > 365) cfg->SameDeviceDays = 365;
	if(cfg->DeclineCooldownSec < 0) cfg->DeclineCooldownSec = 0;
	if(cfg->NoShowCooldownSec < 0) cfg->NoShowCooldownSec = 0;
	if(cfg->UnavailableDropSec < 0) cfg->UnavailableDropSec = 0;
	if(cfg->PromptSec < 3) cfg->PromptSec = 3;
	if(cfg->PromptSec > 120) cfg->PromptSec = 120;
	if(cfg->PromptGraceSec < 0) cfg->PromptGraceSec = 0;
	if(cfg->StageTimeoutSec < 10) cfg->StageTimeoutSec = 10;
	if(cfg->MaxMatchSec < 60) cfg->MaxMatchSec = 60;
	if(cfg->ReconnectGraceSec < 0) cfg->ReconnectGraceSec = 0;
	if(cfg->WatchdogExtraSec < 0) cfg->WatchdogExtraSec = 0;
	// The host reports every 5 s, so anything shorter would flap.
	if(cfg->HostSilentSec < 8) cfg->HostSilentSec = 8;

	return 0;
}

void CDuelRanked::LoadConfig(bool initial)
{
	DR_CONFIG cfg;
	char error[160];

	const int result = this->ReadConfig(&cfg,error,sizeof(error));

	if(result == 0)
	{
		this->m_Cfg = cfg;
		this->m_Enabled = (cfg.Enabled != 0);
		this->m_CfgValid = true;

		LogAdd(LOG_BLACK,"[DuelRanked] %s: host %d, K %d, %d tiers, %d steps",(this->m_Enabled) ? "ranked is ON" : "ranked is OFF",cfg.HostServer,cfg.KFactor,cfg.TierCount,cfg.StepCount);
		return;
	}

	if(initial)
	{
		this->m_CfgValid = false;
		this->m_Enabled = false;

		LogAdd((result == 1) ? LOG_BLACK : LOG_RED,"[DuelRanked] ranked is OFF: %s",error);
	}
	else
	{
		if(this->m_CfgValid)
			LogAdd(LOG_RED,"[DuelRanked] reload refused, keeping the running settings: %s",error);
		else
			LogAdd(LOG_RED,"[DuelRanked] reload refused, ranked stays OFF: %s",error);
	}
}

int CDuelRanked::StepOf(int mmr)
{
	int step = 0;

	for(int n = 0; n < this->m_Cfg.StepCount; n++)
	{
		if(this->m_Cfg.Step[n] <= mmr)
			step = n;
	}

	return step;
}

// Rank-up reward. Every division a character reaches for the first time pays
// that tier's [Rewards] coins into the account's CashShopData; dropping a rank
// and climbing back pays nothing again (DuelRankedBest holds the best division
// ever reached). Nothing happens without the DuelRankedBest table.
void CDuelRanked::GrantRankRewards(const char* name,int oldMmr,int newMmr)
{
	const int oldStep = this->StepOf(oldMmr);
	const int newStep = this->StepOf(newMmr);

	if(newStep <= oldStep)
		return;

	char nm[11];
	CopyText(nm,sizeof(nm),name,10);

	int best = -1;

	gQueryManager.BindParameterAsString(1,nm,sizeof(nm));

	if(gQueryManager.ExecQuery("SELECT BestStep FROM DuelRankedBest WHERE Name=?") == 0)
	{
		gQueryManager.Close();
		LogAdd(LOG_RED,"[DuelRanked] rank reward for %s skipped: no DuelRankedBest table (run DuelRanked_Schema.sql)",nm);
		return;
	}

	if(SQL_SUCCEEDED(gQueryManager.Fetch()))
		best = gQueryManager.GetAsInteger("BestStep");

	gQueryManager.Close();

	// First time seen: the division they were in before this match counts as reached.
	if(best < 0)
	{
		best = oldStep;
		gQueryManager.BindParameterAsString(1,nm,sizeof(nm));
		gQueryManager.ExecQuery("INSERT INTO DuelRankedBest (Name,BestStep) VALUES (?,%d)",best);
		gQueryManager.Close();
	}

	if(newStep <= best)
		return;

	int wcoinC = 0, wcoinP = 0, goblin = 0;

	// Only a NEW RANK pays (Gold -> Platinum), not a new division inside a rank
	// (Gold III -> Gold II): a step pays when its tier differs from the step below it.
	for(int s = best + 1; s <= newStep; s++)
	{
		if(s <= 0 || this->m_Cfg.StepTier[s] == this->m_Cfg.StepTier[s - 1])
			continue;

		const int tier = this->m_Cfg.StepTier[s];
		wcoinC += this->m_Cfg.RewardWCoinC[tier];
		wcoinP += this->m_Cfg.RewardWCoinP[tier];
		goblin += this->m_Cfg.RewardGoblin[tier];
	}

	gQueryManager.BindParameterAsString(1,nm,sizeof(nm));
	gQueryManager.ExecQuery("UPDATE DuelRankedBest SET BestStep=%d WHERE Name=?",newStep);
	gQueryManager.Close();

	// [RewardItems]: one letter per division reached, claimed from the mailbox.
	bool mailed = false;

	for(int s = best + 1; s <= newStep; s++)
	{
		if(s <= 0 || this->m_Cfg.StepTier[s] == this->m_Cfg.StepTier[s - 1])
			continue;

		const int tier = this->m_Cfg.StepTier[s];
		const int count = this->m_Cfg.RewardItemCount[tier];

		if(count <= 0)
			continue;

		BYTE items[DUELRANKED_MAX_REWARD_ITEMS][FRIENDMAIL_ITEM_BYTES];

		for(int k = 0; k < count; k++)
		{
			const BYTE* c = this->m_Cfg.RewardItem[tier][k];
			const int idx = (c[0] * 512) + (c[1] | (c[2] << 8));
			const DWORD serial = (DWORD)GetNewItemSerial();
			BYTE* b = items[k];

			// CItemManager::DBItemByteConvert layout.
			memset(b,0xFF,FRIENDMAIL_ITEM_BYTES);
			b[0] = (BYTE)(idx & 0xFF);
			b[1] = (BYTE)((c[3] * 8) | ((c[5] & 1) ? 128 : 0) | ((c[5] & 2) ? 4 : 0) | (c[6] & 3));
			b[2] = c[4];
			b[3] = (BYTE)(serial >> 24);
			b[4] = (BYTE)(serial >> 16);
			b[5] = (BYTE)(serial >> 8);
			b[6] = (BYTE)(serial);
			b[7] = (BYTE)(((idx & 256) >> 1) | ((c[6] > 3) ? 64 : 0) | (c[7] & 63));
			b[8] = 0;
			b[9] = (BYTE)((idx & (15 * 512)) >> 5);
			b[10] = 0;
		}

		char subject[33];
		char memo[160];
		_snprintf_s(subject,sizeof(subject),_TRUNCATE,"Ranked reward: %s",this->m_Cfg.TierName[tier]);
		_snprintf_s(memo,sizeof(memo),_TRUNCATE,"Congratulations! You reached %s in ranked duels. Your reward is attached.",this->m_Cfg.TierName[tier]);

		const int stored = gFriendMail.MailItems(nm,"Ranked",subject,memo,items,count,0);

		if(stored > 0)
			mailed = true;

		if(stored != count)
		{
			// Usually no mailbox yet (T_FriendMain is made at the first friend-list load).
			LogAdd(LOG_RED,"[DuelRanked] rank reward items for %s, step %d: %d of %d mailed - give the rest by hand",nm,s,stored,count);
		}
		else
		{
			LogAdd(LOG_GREEN,"[DuelRanked] rank reward: %s reached step %d - %d item(s) mailed",nm,s,stored);
		}
	}

	if(wcoinC == 0 && wcoinP == 0 && goblin == 0)
	{
		if(mailed)
			this->SendMsgToName(nm,0,DUELRANKED_MSG_RANK_REWARD,(BYTE)this->m_Cfg.StepTier[newStep]);

		return;
	}

	char account[11] = {0};

	gQueryManager.BindParameterAsString(1,nm,sizeof(nm));

	if(gQueryManager.ExecQuery("SELECT AccountID FROM Character WHERE Name=?") == 0 || SQL_SUCCEEDED(gQueryManager.Fetch()) == 0)
	{
		gQueryManager.Close();
		LogAdd(LOG_RED,"[DuelRanked] rank reward for %s: account not found",nm);
		return;
	}

	gQueryManager.GetAsString("AccountID",account,sizeof(account));
	gQueryManager.Close();

	gQueryManager.BindParameterAsString(1,account,sizeof(account));

	if(gQueryManager.ExecQuery("SELECT WCoinC FROM CashShopData WHERE AccountID=?") != 0 && SQL_SUCCEEDED(gQueryManager.Fetch()))
	{
		gQueryManager.Close();
		gQueryManager.BindParameterAsString(1,account,sizeof(account));
		gQueryManager.ExecQuery("UPDATE CashShopData SET WCoinC=WCoinC+%d,WCoinP=WCoinP+%d,GoblinPoint=GoblinPoint+%d WHERE AccountID=?",wcoinC,wcoinP,goblin);
	}
	else
	{
		gQueryManager.Close();
		gQueryManager.BindParameterAsString(1,account,sizeof(account));
		gQueryManager.ExecQuery("INSERT INTO CashShopData (AccountID,WCoinC,WCoinP,GoblinPoint,Ruud) VALUES (?,%d,%d,%d,0)",wcoinC,wcoinP,goblin);
	}

	gQueryManager.Close();

	LogAdd(LOG_GREEN,"[DuelRanked] rank reward: %s (%s) reached step %d - WCoinC %d WCoinP %d GoblinPoint %d",nm,account,newStep,wcoinC,wcoinP,goblin);

	this->SendMsgToName(nm,0,DUELRANKED_MSG_RANK_REWARD,(BYTE)this->m_Cfg.StepTier[newStep]);
}

void CDuelRanked::RankOf(int mmr,BYTE* tier,BYTE* div,WORD* stepLo,WORD* stepNext)
{
	int step = 0;

	for(int n = 0; n < this->m_Cfg.StepCount; n++)
	{
		if(this->m_Cfg.Step[n] <= mmr)
			step = n;
	}

	if(tier != 0) *tier = (BYTE)this->m_Cfg.StepTier[step];
	if(div != 0) *div = (BYTE)this->m_Cfg.StepDiv[step];
	if(stepLo != 0) *stepLo = ClampWord(this->m_Cfg.Step[step]);
	if(stepNext != 0) *stepNext = (step + 1 < this->m_Cfg.StepCount) ? ClampWord(this->m_Cfg.Step[step + 1]) : 0;
}

//////////////////////////////////////////////////////////////////////
// Threads in and out
//////////////////////////////////////////////////////////////////////

bool CDuelRanked::IsInternal(BYTE head,BYTE* lpMsg,int size)
{
	if(head != 0xD9 || size != (int)sizeof(SDHP_DUELRANKED_INTERNAL) || lpMsg[0] != 0xC1 || lpMsg[3] != 0x5F)
		return false;

	return ((SDHP_DUELRANKED_INTERNAL*)lpMsg)->Magic == DUELRANKED_INTERNAL_MAGIC;
}

// UI thread, once a second. Only posts: the tables belong to the queue thread.
void CDuelRanked::PostTick()
{
	// Nothing to do until there is a valid file, ranked is on, or something is left to wind down.
	if(this->m_ReloadPending == 0 && (this->m_CfgValid == false || (this->m_Enabled == false && this->m_Active == 0)))
		return;

	const DWORD now = GetTickCount();

	// One tick in flight at a time. A tick the consumer dropped (its server went
	// offline in between) would block the next ones, so give up on it after 5 s.
	if(this->m_TickPending != 0 && (now - this->m_TickPostTick) < 5000)
		return;

	int index = -1;

	for(int n = 0; n < MAX_SERVER; n++)
	{
		if(gServerManager[n].CheckState() != 0)
		{
			index = n;
			break;
		}
	}

	if(index == -1)
		return;

	SDHP_DUELRANKED_INTERNAL pMsg;
	pMsg.header.set(0xD9,0x5F,sizeof(pMsg));
	pMsg.Magic = DUELRANKED_INTERNAL_MAGIC;

	this->m_TickPostTick = now;
	InterlockedExchange(&this->m_TickPending,1);

	if(gSocketManager.PostInternal(index,0xD9,(BYTE*)&pMsg,sizeof(pMsg)) == 0)
		InterlockedExchange(&this->m_TickPending,0);
}

// UI thread (menu): the queue thread reloads on its next tick.
void CDuelRanked::PostReload()
{
	InterlockedExchange(&this->m_ReloadPending,1);
}

// IOCP thread, inside CServerManager::DelServer. Memory only.
void CDuelRanked::OnServerDown(WORD serverCode)
{
	if(this->m_CfgValid == false || serverCode == 0xFFFF)
		return;

	this->m_DownLock.lock();

	if(this->m_Down.size() < 64)
		this->m_Down.push_back(serverCode);

	this->m_DownLock.unlock();
}

void CDuelRanked::DrainDown()
{
	std::vector<WORD> down;

	this->m_DownLock.lock();
	down.swap(this->m_Down);
	this->m_DownLock.unlock();

	for(size_t n = 0; n < down.size(); n++)
		this->HandleServerDown(down[n]);
}

void CDuelRanked::OnPacket(BYTE* lpMsg,int size,int index)
{
	if(size < (int)sizeof(PSBMSG_HEAD) || lpMsg[0] != 0xC1 || SERVER_RANGE(index) == 0)
		return;

	const BYTE sub = lpMsg[3];

	if(sub == 0x5F)
	{
		if(IsInternal(0xD9,lpMsg,size) != 0)
			this->Tick();

		return;
	}

	switch(sub)
	{
		case 0x51:
			// Answered even when ranked is off, so the player is told.
			if(size >= (int)sizeof(SDHP_DUELRANKED_REQ))
				this->OnReq((SDHP_DUELRANKED_REQ*)lpMsg,index);
			break;
	}

	if(this->m_CfgValid == false)
		return;

	switch(sub)
	{
		case 0x52:
			if(size >= (int)sizeof(SDHP_DUELRANKED_HOSTREPORT) && this->FromHost(index))
				this->OnHostReport((SDHP_DUELRANKED_HOSTREPORT*)lpMsg);
			break;
		case 0x53:
			if(size >= (int)sizeof(SDHP_DUELRANKED_DEPART))
				this->OnDepart((SDHP_DUELRANKED_DEPART*)lpMsg);
			break;
		case 0x54:
			if(size >= (int)sizeof(SDHP_DUELRANKED_HOSTEVENT) && this->FromHost(index))
				this->OnHostEvent((SDHP_DUELRANKED_HOSTEVENT*)lpMsg);
			break;
		case 0x55:
			if(size >= (int)sizeof(SDHP_DUELRANKED_RESULT) && this->FromHost(index))
				this->OnResult((SDHP_DUELRANKED_RESULT*)lpMsg,index);
			break;
	}
}

bool CDuelRanked::FromHost(int index)
{
	return SERVER_RANGE(index) != 0 && gServerManager[index].m_ServerCode == (WORD)this->m_Cfg.HostServer;
}

// The arena is usable when the host is connected, reporting, accepting, and (to
// queue) not above MaxLoadPercent.
bool CDuelRanked::HostAvailable(bool checkLoad)
{
	if(this->m_Enabled == false || this->m_HostReported == false)
		return false;

	if(FindServerByCode((WORD)this->m_Cfg.HostServer) == 0)
		return false;

	if((GetTickCount() - this->m_HostReportTick) > (DWORD)this->m_Cfg.HostSilentSec * 1000)
		return false;

	if(this->m_Host.Accepting == 0)
		return false;

	if(checkLoad && (int)this->m_Host.UserCount + 2 > (int)this->m_Host.UserMax * this->m_Cfg.MaxLoadPercent / 100)
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////
// Lookups
//////////////////////////////////////////////////////////////////////

DR_PLAYER* CDuelRanked::FindPlayer(const char* name)
{
	std::map<std::string,DR_PLAYER>::iterator it = this->m_Players.find(Key(name));

	return (it == this->m_Players.end()) ? 0 : &it->second;
}

void CDuelRanked::ErasePlayer(const char* name)
{
	this->m_Players.erase(Key(name));
}

DR_MATCH* CDuelRanked::FindMatch(DWORD id)
{
	std::map<DWORD,DR_MATCH>::iterator it = this->m_Matches.find(id);

	return (it == this->m_Matches.end() || it->second.Phase == DR_PHASE_PROMPT) ? 0 : &it->second;
}

DR_MATCH* CDuelRanked::FindPrompt(DWORD token)
{
	std::map<DWORD,DR_MATCH>::iterator it = this->m_Matches.find(token);

	return (it == this->m_Matches.end() || it->second.Phase != DR_PHASE_PROMPT) ? 0 : &it->second;
}

// The request really comes from this character on this GameServer.
bool CDuelRanked::Verify(int index,const char* account,const char* name,CHARACTER_INFO* info)
{
	if(gCharacterManager.GetCharacterInfo(info,(char*)name) == 0)
		return false;

	return _stricmp(info->Account,account) == 0 && info->GameServerCode == gServerManager[index].m_ServerCode;
}

bool CDuelRanked::Resolve(const char* name,CHARACTER_INFO* info,int* serverIndex)
{
	if(gCharacterManager.GetCharacterInfo(info,(char*)name) == 0)
		return false;

	CServerManager* lpServerManager = FindServerByCode(info->GameServerCode);

	if(lpServerManager == 0)
		return false;

	if(serverIndex != 0)
		(*serverIndex) = lpServerManager->m_index;

	return true;
}

int CDuelRanked::CooldownLeft(const char* name)
{
	std::map<std::string,DWORD>::iterator it = this->m_Cooldown.find(Key(name));

	if(it == this->m_Cooldown.end())
		return 0;

	const int ms = (int)(it->second - GetTickCount());

	if(ms <= 0)
	{
		this->m_Cooldown.erase(it);
		return 0;
	}

	return (ms + 999) / 1000;
}

void CDuelRanked::SetCooldown(const char* name,int seconds)
{
	if(seconds > 0)
		this->m_Cooldown[Key(name)] = After(GetTickCount(),seconds);
}

bool CDuelRanked::RematchBlocked(const char* a,const char* b)
{
	std::map<std::string,DWORD>::iterator it = this->m_Rematch.find(PairKey(a,b));

	if(it == this->m_Rematch.end())
		return false;

	if((int)(it->second - GetTickCount()) <= 0)
	{
		this->m_Rematch.erase(it);
		return false;
	}

	return true;
}

// Two accounts "share a device" when their HardwareLog rows seen in the last
// SameDeviceDays days have the same hardware id or the same IP. Such a pair (win trading,
// alts) is paired only while each of them has had fewer than SameDeviceLimit matches today
// against opponents that share their device. Cached for 30 s per pair; any database
// failure lets the pair through (and is logged once).
bool CDuelRanked::SameDeviceBlocked(const char* nameA,const char* accountA,const char* nameB,const char* accountB)
{
	if(this->m_Cfg.SameDeviceLimit <= 0)
		return false;

	const DWORD now = GetTickCount();
	const std::string key = PairKey(nameA,nameB);

	std::map<std::string,std::pair<DWORD,bool> >::iterator hit = this->m_DeviceCache.find(key);

	if(hit != this->m_DeviceCache.end())
	{
		if((int)(hit->second.first - now) > 0)
			return hit->second.second;

		this->m_DeviceCache.erase(hit);
	}

	static bool warned = false;
	bool blocked = false;
	char na[11],nb[11],aa[11],ab[11];
	CopyText(na,sizeof(na),nameA,10);
	CopyText(nb,sizeof(nb),nameB,10);
	CopyText(aa,sizeof(aa),accountA,10);
	CopyText(ab,sizeof(ab),accountB,10);

	const int days = this->m_Cfg.SameDeviceDays;

	// 1. Do they share a device at all?
	int shared = 0;

	gQueryManager.BindParameterAsString(1,aa,sizeof(aa));
	gQueryManager.BindParameterAsString(2,ab,sizeof(ab));

	if(gQueryManager.ExecQuery("SELECT COUNT(*) FROM HardwareLog ha JOIN HardwareLog hb ON ((ha.HardwareId<>'' AND ha.HardwareId=hb.HardwareId) OR (ha.IpAddress<>'' AND ha.IpAddress=hb.IpAddress)) WHERE ha.AccountID=? AND hb.AccountID=? AND ha.LastSeen>=DATEADD(DAY,-%d,GETDATE()) AND hb.LastSeen>=DATEADD(DAY,-%d,GETDATE())",days,days) != 0 && SQL_SUCCEEDED(gQueryManager.Fetch()))
	{
		shared = gQueryManager.GetResult(0);
	}
	else if(warned == false)
	{
		warned = true;
		LogAdd(LOG_RED,"[DuelRanked] same-device limit skipped: HardwareLog is not readable (run DeviceLog_Schema.sql)");
	}

	gQueryManager.Close();

	if(shared > 0)
	{
		// 2. Today's matches of each of them against opponents that share their device.
		const char* names[2] = {na,nb};
		const char* accounts[2] = {aa,ab};

		for(int s = 0; s < 2 && blocked == false; s++)
		{
			int today = 0;

			gQueryManager.BindParameterAsString(1,(void*)names[s],11);
			gQueryManager.BindParameterAsString(2,(void*)names[s],11);
			gQueryManager.BindParameterAsString(3,(void*)names[s],11);
			gQueryManager.BindParameterAsString(4,(void*)accounts[s],11);

			if(gQueryManager.ExecQuery("SELECT COUNT(*) FROM DuelRankedMatch m JOIN DuelRanked r ON r.Name=(CASE WHEN m.NameA=? THEN m.NameB ELSE m.NameA END) WHERE m.CreatedAt>=CAST(GETDATE() AS DATE) AND m.Status<>3 AND (m.NameA=? OR m.NameB=?) AND EXISTS (SELECT 1 FROM HardwareLog ha JOIN HardwareLog hb ON ((ha.HardwareId<>'' AND ha.HardwareId=hb.HardwareId) OR (ha.IpAddress<>'' AND ha.IpAddress=hb.IpAddress)) WHERE ha.AccountID=? AND hb.AccountID=r.Account AND ha.LastSeen>=DATEADD(DAY,-%d,GETDATE()) AND hb.LastSeen>=DATEADD(DAY,-%d,GETDATE()))",days,days) != 0 && SQL_SUCCEEDED(gQueryManager.Fetch()))
			{
				today = gQueryManager.GetResult(0);
			}

			gQueryManager.Close();

			if(today >= this->m_Cfg.SameDeviceLimit)
			{
				blocked = true;
				LogAdd(LOG_BLACK,"[DuelRanked] %s and %s share an IP or hardware id: %s already has %d such match(es) today, not paired",na,nb,names[s],today);
			}
		}
	}

	this->m_DeviceCache[key] = std::make_pair(After(now,30),blocked);

	return blocked;
}

void CDuelRanked::SetRematch(const char* a,const char* b)
{
	if(this->m_Cfg.RematchCooldownSec > 0)
		this->m_Rematch[PairKey(a,b)] = After(GetTickCount(),this->m_Cfg.RematchCooldownSec);
}

int CDuelRanked::CountSpectators(DWORD matchId)
{
	int count = 0;

	for(std::map<std::string,DR_PLAYER>::iterator it = this->m_Players.begin(); it != this->m_Players.end(); it++)
	{
		if(it->second.State == DUELRANKED_STATE_WATCHING && it->second.MatchId == matchId)
			count++;
	}

	return count;
}

DWORD CDuelRanked::NextToken()
{
	do
	{
		this->m_NextToken = (this->m_NextToken + 1) & 0x7FFFFFFF;
	}
	while(this->m_NextToken == 0);

	return kTokenBit | this->m_NextToken;
}

//////////////////////////////////////////////////////////////////////
// Senders
//////////////////////////////////////////////////////////////////////

bool CDuelRanked::SendToCode(WORD serverCode,BYTE* lpMsg,int size)
{
	CServerManager* lpServerManager = FindServerByCode(serverCode);

	if(lpServerManager == 0)
		return false;

	return gSocketManager.DataSend(lpServerManager->m_index,lpMsg,size) != 0;
}

void CDuelRanked::SendMsg(int serverIndex,WORD aIndex,const char* account,DWORD matchId,BYTE code,BYTE p1)
{
	SDHP_DUELRANKED_MSG pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x57,sizeof(pMsg));
	pMsg.aIndex = aIndex;
	CopyText(pMsg.Account,sizeof(pMsg.Account),account,10);
	pMsg.MatchId = matchId;
	pMsg.Code = code;
	pMsg.P1 = p1;

	gSocketManager.DataSend(serverIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CDuelRanked::SendMsgToName(const char* name,DWORD matchId,BYTE code,BYTE p1)
{
	CHARACTER_INFO info;
	int serverIndex;

	if(this->Resolve(name,&info,&serverIndex) != 0)
		this->SendMsg(serverIndex,info.UserIndex,info.Account,matchId,code,p1);
}

void CDuelRanked::SendStatus(int serverIndex,WORD aIndex,const char* account,const char* name,const DR_STATS* known)
{
	DR_STATS stats;

	if(known != 0)
	{
		stats = *known;
	}
	else if(this->DbGet(name,account,&stats) == 0)
	{
		this->SendMsg(serverIndex,aIndex,account,0,DUELRANKED_MSG_NOT_ALLOWED_NOW,0);
		return;
	}

	SDHP_DUELRANKED_STATUS pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x51,sizeof(pMsg));
	pMsg.aIndex = aIndex;
	CopyText(pMsg.Account,sizeof(pMsg.Account),account,10);
	pMsg.Mmr = ClampWord(stats.Mmr);
	this->RankOf(stats.Mmr,&pMsg.Tier,&pMsg.Div,&pMsg.StepLo,&pMsg.StepNext);
	pMsg.Wins = ClampWord(stats.Wins);
	pMsg.Losses = ClampWord(stats.Losses);

	DR_PLAYER* p = this->FindPlayer(name);

	if(p != 0)
	{
		pMsg.State = p->State;

		if(p->State == DUELRANKED_STATE_QUEUED)
			pMsg.QueuedSec = ClampWord((int)((GetTickCount() - p->QueuedTick) / 1000));
	}

	pMsg.Accepting = (this->HostAvailable(true)) ? 1 : 0;

	gSocketManager.DataSend(serverIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CDuelRanked::SendPrompt(int serverIndex,const CHARACTER_INFO* to,DWORD token,const DR_SLOT* opp)
{
	SDHP_DUELRANKED_PROMPT pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x52,sizeof(pMsg));
	pMsg.aIndex = to->UserIndex;
	CopyText(pMsg.Account,sizeof(pMsg.Account),to->Account,10);
	pMsg.MatchId = token;
	CopyText(pMsg.Opp,sizeof(pMsg.Opp),opp->Name,10);
	pMsg.OppMmr = ClampWord(opp->Mmr);
	this->RankOf(opp->Mmr,&pMsg.OppTier,&pMsg.OppDiv,0,0);
	pMsg.Sec = ClampByte(this->m_Cfg.PromptSec);

	gSocketManager.DataSend(serverIndex,(BYTE*)&pMsg,pMsg.header.size);
}

// Addressed by name. serverIndex < 0 = find where the character is registered.
bool CDuelRanked::SendGo(const char* name,const char* account,DWORD matchId,BYTE kind,BYTE room,int serverIndex)
{
	if(serverIndex < 0)
	{
		CHARACTER_INFO info;

		if(this->Resolve(name,&info,&serverIndex) == 0)
			return false;
	}

	SDHP_DUELRANKED_GO pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x53,sizeof(pMsg));
	CopyText(pMsg.Name,sizeof(pMsg.Name),name,10);
	CopyText(pMsg.Account,sizeof(pMsg.Account),account,10);
	pMsg.MatchId = matchId;
	pMsg.Kind = kind;
	pMsg.HostServer = (WORD)this->m_Cfg.HostServer;
	pMsg.Room = room;

	return gSocketManager.DataSend(serverIndex,(BYTE*)&pMsg,pMsg.header.size) != 0;
}

bool CDuelRanked::SendExpect(const DR_MATCH* m,int slot)
{
	const DR_SLOT* s = &m->Slot[slot];

	SDHP_DUELRANKED_EXPECT pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x54,sizeof(pMsg));
	pMsg.MatchId = m->Id;
	pMsg.Kind = DUELRANKED_KIND_FIGHTER;
	pMsg.Slot = (BYTE)slot;
	CopyText(pMsg.Name,sizeof(pMsg.Name),s->Name,10);
	CopyText(pMsg.Account,sizeof(pMsg.Account),s->Account,10);
	CopyText(pMsg.Opp,sizeof(pMsg.Opp),m->Slot[1 - slot].Name,10);
	pMsg.Mmr = ClampWord(s->Mmr);
	this->RankOf(s->Mmr,&pMsg.Tier,&pMsg.Div,0,0);
	pMsg.OriginServer = s->OriginServer;
	pMsg.OriginMap = s->OriginMap;
	pMsg.OriginX = s->OriginX;
	pMsg.OriginY = s->OriginY;
	pMsg.Room = 0xFF;	// not assigned yet: the host picks the room

	return this->SendToCode(m->HostServer,(BYTE*)&pMsg,pMsg.header.size);
}

bool CDuelRanked::SendExpectSpectator(const DR_MATCH* m,const DR_PLAYER* p)
{
	SDHP_DUELRANKED_EXPECT pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x54,sizeof(pMsg));
	pMsg.MatchId = m->Id;
	pMsg.Kind = DUELRANKED_KIND_SPECTATOR;
	CopyText(pMsg.Name,sizeof(pMsg.Name),p->Name,10);
	CopyText(pMsg.Account,sizeof(pMsg.Account),p->Account,10);
	pMsg.OriginServer = p->OriginServer;
	pMsg.OriginMap = p->OriginMap;
	pMsg.OriginX = p->OriginX;
	pMsg.OriginY = p->OriginY;
	pMsg.Room = m->Room;

	return this->SendToCode(m->HostServer,(BYTE*)&pMsg,pMsg.header.size);
}

void CDuelRanked::SendMatchCancel(WORD hostServer,DWORD matchId,BYTE reason)
{
	SDHP_DUELRANKED_MATCHCANCEL pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x58,sizeof(pMsg));
	pMsg.MatchId = matchId;
	pMsg.Reason = reason;

	this->SendToCode(hostServer,(BYTE*)&pMsg,pMsg.header.size);
}

// A fighter's DEPART for a match that is already gone. He is on his way to the
// host: announce him (the same EXPECT a live match sends, built from the DEPART)
// and cancel the match in the same breath, so the host keeps a note of where he
// came from and sends him back when he lands. The two arrive in this order on
// the one connection.
void CDuelRanked::SendStrayFighter(const SDHP_DUELRANKED_DEPART* d)
{
	SDHP_DUELRANKED_EXPECT pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x54,sizeof(pMsg));
	pMsg.MatchId = d->MatchId;
	pMsg.Kind = DUELRANKED_KIND_FIGHTER;
	pMsg.Slot = 0;
	CopyText(pMsg.Name,sizeof(pMsg.Name),d->Name,10);
	CopyText(pMsg.Account,sizeof(pMsg.Account),d->Account,10);
	pMsg.OriginServer = d->OriginServer;
	pMsg.OriginMap = d->OriginMap;
	pMsg.OriginX = d->OriginX;
	pMsg.OriginY = d->OriginY;
	pMsg.Room = 0xFF;

	const WORD host = (WORD)this->m_Cfg.HostServer;

	if(this->SendToCode(host,(BYTE*)&pMsg,pMsg.header.size) == false)
		return;

	LogAdd(LOG_BLACK,"[DuelRanked] match %d is gone but %s is already on the way to the host, asking it to send him back",(int)d->MatchId,pMsg.Name);

	this->SendMatchCancel(host,d->MatchId,DUELRANKED_CANCEL_VOID);
}

// Answers a RESULT from what the database holds. Winner empty = no rated result
// (voided, or no such match) so the host stops resending.
void CDuelRanked::SendApplied(int hostIndex,DWORD matchId,const DR_RESULTROW* row)
{
	SDHP_DUELRANKED_APPLIED pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x55,sizeof(pMsg));
	pMsg.MatchId = matchId;

	if(row != 0 && row->Applied >= 0 && (row->Status == 1 || row->Status == 2))
	{
		const bool draw = (row->Status == 2);
		// A draw has no winner: slot A is reported as "winner" with IsDraw set.
		const bool aWon = (draw || _stricmp(row->WinnerName,row->NameA) == 0);

		pMsg.IsDraw = (draw) ? 1 : 0;
		pMsg.Applied = (row->Applied == 1) ? 1 : 0;

		const int oldW = (aWon) ? row->MmrABefore : row->MmrBBefore;
		const int newW = (aWon) ? row->NewA : row->NewB;
		const int oldL = (aWon) ? row->MmrBBefore : row->MmrABefore;
		const int newL = (aWon) ? row->NewB : row->NewA;

		CopyText(pMsg.Winner,sizeof(pMsg.Winner),(aWon) ? row->NameA : row->NameB,10);
		CopyText(pMsg.Loser,sizeof(pMsg.Loser),(aWon) ? row->NameB : row->NameA,10);
		pMsg.DeltaW = (short)(newW - oldW);
		pMsg.DeltaL = (short)(newL - oldL);
		pMsg.OldMmrW = ClampWord(oldW);
		pMsg.OldMmrL = ClampWord(oldL);
		pMsg.NewMmrW = ClampWord(newW);
		pMsg.NewMmrL = ClampWord(newL);
		this->RankOf(oldW,&pMsg.OldTierW,&pMsg.OldDivW,0,0);
		this->RankOf(newW,&pMsg.NewTierW,&pMsg.NewDivW,0,0);
		this->RankOf(oldL,&pMsg.OldTierL,&pMsg.OldDivL,0,0);
		this->RankOf(newL,&pMsg.NewTierL,&pMsg.NewDivL,0,0);
	}

	gSocketManager.DataSend(hostIndex,(BYTE*)&pMsg,pMsg.header.size);
}

// The public rank icon: tells the GameServer a character's tier so it can show it to
// everyone who sees that character. Nothing is sent when ranked has no valid
// DuelRanked.ini. Queue thread only (it reads the database).
void CDuelRanked::SendRank(int serverIndex,const char* name,const char* account)
{
	if(this->m_CfgValid == false)
		return;

	DR_STATS stats;

	if(this->DbPeek(name,&stats) < 0)
		return;

	SDHP_DUELRANKED_RANK pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x59,sizeof(pMsg));
	CopyText(pMsg.Name,sizeof(pMsg.Name),name,10);
	CopyText(pMsg.Account,sizeof(pMsg.Account),account,10);

	// No row, or no finished rated match yet: no rank to show.
	if(stats.Wins + stats.Losses + stats.Draws > 0)
	{
		pMsg.Played = 1;
		this->RankOf(stats.Mmr,&pMsg.Tier,&pMsg.Div,0,0);
	}

	LogAdd(LOG_BLACK,"[DuelRanked] rank icon for %s: played %d tier %d div %d (mmr %d)",pMsg.Name,(int)pMsg.Played,(int)pMsg.Tier,(int)pMsg.Div,stats.Mmr);

	gSocketManager.DataSend(serverIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CDuelRanked::SendRankToName(const char* name)
{
	CHARACTER_INFO info;
	int serverIndex;

	if(this->Resolve(name,&info,&serverIndex) != 0)
		this->SendRank(serverIndex,info.Name,info.Account);
}

//////////////////////////////////////////////////////////////////////
// Database (queue thread only)
//////////////////////////////////////////////////////////////////////

// Unlike DbGet (WZ_DuelRanked_Get) this never inserts: it runs for every login.
int CDuelRanked::DbPeek(const char* name,DR_STATS* out)
{
	char nm[11];
	CopyText(nm,sizeof(nm),name,10);

	memset(out,0,sizeof(*out));

	int result = -1;

	gQueryManager.BindParameterAsString(1,nm,sizeof(nm));

	if(gQueryManager.ExecQuery("SELECT MMR,Wins,Losses,Draws FROM DuelRanked WHERE Name=?") != 0)
	{
		if(SQL_SUCCEEDED(gQueryManager.Fetch()))
		{
			out->Mmr = gQueryManager.GetAsInteger("MMR");
			out->Wins = gQueryManager.GetAsInteger("Wins");
			out->Losses = gQueryManager.GetAsInteger("Losses");
			out->Draws = gQueryManager.GetAsInteger("Draws");
			result = 1;
		}
		else
		{
			result = 0;
		}
	}

	gQueryManager.Close();

	return result;
}

bool CDuelRanked::DbGet(const char* name,const char* account,DR_STATS* out)
{
	char nm[11];
	char acc[11];
	CopyText(nm,sizeof(nm),name,10);
	CopyText(acc,sizeof(acc),account,10);

	bool ok = false;

	gQueryManager.BindParameterAsString(1,nm,sizeof(nm));
	gQueryManager.BindParameterAsString(2,acc,sizeof(acc));

	if(gQueryManager.ExecQuery("EXEC WZ_DuelRanked_Get ?,?,%d",this->m_Cfg.StartMMR) != 0 && SQL_SUCCEEDED(gQueryManager.Fetch()))
	{
		out->Mmr = gQueryManager.GetAsInteger("MMR");
		out->Wins = gQueryManager.GetAsInteger("Wins");
		out->Losses = gQueryManager.GetAsInteger("Losses");
		out->Draws = gQueryManager.GetAsInteger("Draws");
		ok = true;
	}

	gQueryManager.Close();

	if(ok == false)
		LogAdd(LOG_RED,"[DuelRanked] could not read the rating of %s",nm);

	return ok;
}

bool CDuelRanked::DbCreate(const DR_MATCH* m,int* matchId,int* mmrA,int* mmrB)
{
	char nameA[11];
	char accA[11];
	char nameB[11];
	char accB[11];
	CopyText(nameA,sizeof(nameA),m->Slot[0].Name,10);
	CopyText(accA,sizeof(accA),m->Slot[0].Account,10);
	CopyText(nameB,sizeof(nameB),m->Slot[1].Name,10);
	CopyText(accB,sizeof(accB),m->Slot[1].Account,10);

	bool ok = false;

	gQueryManager.BindParameterAsString(1,nameA,sizeof(nameA));
	gQueryManager.BindParameterAsString(2,accA,sizeof(accA));
	gQueryManager.BindParameterAsString(3,nameB,sizeof(nameB));
	gQueryManager.BindParameterAsString(4,accB,sizeof(accB));

	if(gQueryManager.ExecQuery("EXEC WZ_DuelRanked_Create ?,?,?,?,%d,%d",this->m_Cfg.HostServer,this->m_Cfg.StartMMR) != 0 && SQL_SUCCEEDED(gQueryManager.Fetch()))
	{
		(*matchId) = gQueryManager.GetAsInteger("MatchId");
		(*mmrA) = gQueryManager.GetAsInteger("MmrA");
		(*mmrB) = gQueryManager.GetAsInteger("MmrB");
		ok = ((*matchId) > 0);
	}

	gQueryManager.Close();

	if(ok == false)
		LogAdd(LOG_RED,"[DuelRanked] could not open a match for %s and %s",nameA,nameB);

	return ok;
}

bool CDuelRanked::DbResult(DWORD matchId,int winnerSlot,int reason,int scoreA,int scoreB,DR_RESULTROW* row)
{
	memset(row,0,sizeof(*row));
	row->Applied = -1;
	row->Status = -1;

	bool ok = false;

	if(gQueryManager.ExecQuery("EXEC WZ_DuelRanked_Result %d,%d,%d,%d,%d,%d,%d",(int)matchId,winnerSlot,reason,scoreA,scoreB,this->m_Cfg.KFactor,this->m_Cfg.MMRFloor) != 0 && SQL_SUCCEEDED(gQueryManager.Fetch()))
	{
		row->Applied = gQueryManager.GetAsInteger("Applied");
		row->Status = gQueryManager.GetAsInteger("Status");
		row->MmrABefore = gQueryManager.GetAsInteger("MmrABefore");
		row->MmrBBefore = gQueryManager.GetAsInteger("MmrBBefore");
		row->NewA = gQueryManager.GetAsInteger("NewA");
		row->NewB = gQueryManager.GetAsInteger("NewB");
		row->DeltaA = gQueryManager.GetAsInteger("DeltaA");
		row->DeltaB = gQueryManager.GetAsInteger("DeltaB");
		gQueryManager.GetAsString("NameA",row->NameA,sizeof(row->NameA));
		gQueryManager.GetAsString("NameB",row->NameB,sizeof(row->NameB));
		gQueryManager.GetAsString("WinnerName",row->WinnerName,sizeof(row->WinnerName));
		ok = true;
	}

	gQueryManager.Close();

	return ok;
}

void CDuelRanked::DbVoid(DWORD matchId,int status)
{
	gQueryManager.ExecQuery("EXEC WZ_DuelRanked_Void %d,%d",(int)matchId,status);
	gQueryManager.Close();
}

// 1 = found (names filled), 0 = no such match, -1 = database error.
int CDuelRanked::DbMatchGet(DWORD matchId,char* nameA,char* nameB)
{
	int result = -1;

	if(gQueryManager.ExecQuery("SELECT NameA,NameB FROM DuelRankedMatch WHERE MatchId=%d",(int)matchId) != 0)
	{
		SQLRETURN fetch = gQueryManager.Fetch();

		if(SQL_SUCCEEDED(fetch))
		{
			gQueryManager.GetAsString("NameA",nameA,11);
			gQueryManager.GetAsString("NameB",nameB,11);
			result = 1;
		}
		else if(fetch == SQL_NO_DATA)
		{
			result = 0;
		}
	}

	gQueryManager.Close();

	return result;
}

// A match left open (Status 0) for over an hour is never going to be settled.
// An hour is far longer than the host keeps resending a RESULT.
void CDuelRanked::DbSweep()
{
	gQueryManager.ExecQuery("UPDATE DuelRankedMatch SET Status=4, SettledAt=GETDATE() WHERE Status=0 AND CreatedAt < DATEADD(HOUR,-1,GETDATE())");
	gQueryManager.Close();
}

//////////////////////////////////////////////////////////////////////
// Requests from a GameServer
//////////////////////////////////////////////////////////////////////

void CDuelRanked::OnReq(SDHP_DUELRANKED_REQ* lpMsg,int index)
{
	char account[11];
	char name[11];
	CopyText(account,sizeof(account),lpMsg->Account,10);
	CopyText(name,sizeof(name),lpMsg->Name,10);

	if(account[0] == 0 || name[0] == 0)
		return;

	const WORD aIndex = lpMsg->aIndex;

	if(this->m_Enabled == false && lpMsg->Op != DUELRANKED_OP_LEAVE && lpMsg->Op != DUELRANKED_OP_DECLINE && lpMsg->Op != DUELRANKED_OP_CANTGO)
	{
		this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_RANKED_DISABLED,0);
		return;
	}

	// The account must be the one this name was queued under.
	if(lpMsg->Op == DUELRANKED_OP_LEAVE || lpMsg->Op == DUELRANKED_OP_ACCEPT || lpMsg->Op == DUELRANKED_OP_DECLINE || lpMsg->Op == DUELRANKED_OP_CANTGO)
	{
		DR_PLAYER* known = this->FindPlayer(name);

		if(known != 0 && _stricmp(known->Account,account) != 0)
			return;
	}

	switch(lpMsg->Op)
	{
		case DUELRANKED_OP_STATUS:
		{
			// Only for a character that really is on this server: STATUS creates a rating row.
			CHARACTER_INFO info;

			if(this->Verify(index,account,name,&info) != 0)
				this->SendStatus(index,aIndex,account,info.Name,0);
		}
			break;
		case DUELRANKED_OP_JOIN:
			this->OnJoin(index,aIndex,account,name,lpMsg->Level);
			break;
		case DUELRANKED_OP_LEAVE:
			this->OnLeave(index,aIndex,account,name);
			break;
		case DUELRANKED_OP_ACCEPT:
			this->OnAnswer(name,lpMsg->MatchId,true);
			break;
		case DUELRANKED_OP_DECLINE:
			this->OnAnswer(name,lpMsg->MatchId,false);
			break;
		case DUELRANKED_OP_LIST:
			this->OnList(index,aIndex,account);
			break;
		case DUELRANKED_OP_WATCH:
			this->OnWatch(index,aIndex,account,name,lpMsg->MatchId,lpMsg->Room);
			break;
		case DUELRANKED_OP_CANTGO:
			this->OnCantGo(name,lpMsg->MatchId);
			break;
	}
}

void CDuelRanked::OnJoin(int index,WORD aIndex,const char* account,const char* name,WORD level)
{
	CHARACTER_INFO info;

	if(this->Verify(index,account,name,&info) == 0)
	{
		this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_NOT_ALLOWED_NOW,0);
		return;
	}

	if(this->HostAvailable(true) == false)
	{
		this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_ARENA_UNAVAILABLE,0);
		return;
	}

	DR_PLAYER* p = this->FindPlayer(info.Name);

	if(p != 0)
	{
		if(p->State == DUELRANKED_STATE_QUEUED)
		{
			this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_ALREADY_QUEUED,0);
			this->SendStatus(index,aIndex,account,info.Name,0);
		}
		else
		{
			this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_IN_MATCH,0);
		}

		return;
	}

	const int cooldown = this->CooldownLeft(info.Name);

	if(cooldown > 0)
	{
		this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_COOLDOWN,ClampByte(cooldown));
		return;
	}

	DR_STATS stats;

	if(this->DbGet(info.Name,info.Account,&stats) == 0)
	{
		this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_NOT_ALLOWED_NOW,0);
		return;
	}

	const DWORD now = GetTickCount();

	DR_PLAYER np;
	memset(&np,0,sizeof(np));
	CopyText(np.Name,sizeof(np.Name),info.Name,10);
	CopyText(np.Account,sizeof(np.Account),info.Account,10);
	np.State = DUELRANKED_STATE_QUEUED;
	np.Level = level;
	np.ServerCode = info.GameServerCode;
	np.Mmr = stats.Mmr;
	np.QueuedTick = now;
	np.StateTick = now;

	this->m_Players[Key(np.Name)] = np;

	this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_QUEUED,0);
	this->SendStatus(index,aIndex,account,np.Name,&stats);

	LogAdd(LOG_BLACK,"[DuelRanked] %s joined the queue (MMR %d)",np.Name,np.Mmr);

	this->TryPair();
}

void CDuelRanked::OnLeave(int index,WORD aIndex,const char* account,const char* name)
{
	DR_PLAYER* p = this->FindPlayer(name);

	if(p == 0)
	{
		// Not queued (a restart forgot it): tell the client so it clears its state.
		this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_LEFT_QUEUE,0);
		return;
	}

	switch(p->State)
	{
		case DUELRANKED_STATE_QUEUED:
			this->ErasePlayer(name);
			this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_LEFT_QUEUE,0);
			break;
		case DUELRANKED_STATE_PROMPT:
			// Leaving while a match is offered is declining it.
			this->OnAnswer(name,p->MatchId,false);
			break;
		default:
			this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_IN_MATCH,0);
			break;
	}
}

void CDuelRanked::OnAnswer(const char* name,DWORD token,bool accept)
{
	DR_PLAYER* p = this->FindPlayer(name);
	DR_MATCH* m = this->FindPrompt(token);

	if(p == 0 || m == 0 || p->State != DUELRANKED_STATE_PROMPT || p->MatchId != token)
	{
		// A late answer to a prompt that is already gone: let the client drop it.
		if(p == 0)
			this->SendMsgToName(name,0,DUELRANKED_MSG_LEFT_QUEUE,0);
		else if(p->State == DUELRANKED_STATE_QUEUED)
			this->SendMsgToName(name,0,DUELRANKED_MSG_MATCH_CANCELLED,0);

		return;
	}

	const int slot = p->Slot;

	if(accept == false)
	{
		LogAdd(LOG_BLACK,"[DuelRanked] %s declined the match",p->Name);
		this->CancelPrompt(token,1 << slot,0);
		return;
	}

	m->Slot[slot].Accepted = 1;

	if(m->Slot[0].Accepted != 0 && m->Slot[1].Accepted != 0)
		this->BeginStaging(token);
}

void CDuelRanked::OnList(int index,WORD aIndex,const char* account)
{
	SDHP_DUELRANKED_LIST pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.aIndex = aIndex;
	CopyText(pMsg.Account,sizeof(pMsg.Account),account,10);

	int count = 0;

	if(this->HostAvailable(false))
	{
		for(int r = 0; r < 4 && count < 4; r++)
		{
			const DUELRANKED_ROOM& room = this->m_Host.Rooms[r];

			if((room.State != 1 && room.State != 2) || room.Ranked == 0 || room.MatchId == 0)
				continue;

			DR_MATCH* m = this->FindMatch(room.MatchId);

			if(m == 0 || m->Phase != DR_PHASE_FIGHTING)
				continue;

			DUELRANKED_LISTENTRY& e = pMsg.e[count++];
			e.MatchId = room.MatchId;
			e.Room = (BYTE)r;
			e.HostServer = m->HostServer;
			CopyText(e.Name1,sizeof(e.Name1),room.Name1,10);
			CopyText(e.Name2,sizeof(e.Name2),room.Name2,10);
			e.Score1 = room.Score1;
			e.Score2 = room.Score2;
			e.RemainSec = room.RemainSec;

			// The rating comes from the match, looked up by the name the host shows.
			for(int s = 0; s < 2; s++)
			{
				if(_stricmp(m->Slot[s].Name,e.Name1) == 0) e.Mmr1 = ClampWord(m->Slot[s].Mmr);
				if(_stricmp(m->Slot[s].Name,e.Name2) == 0) e.Mmr2 = ClampWord(m->Slot[s].Mmr);
			}

			this->RankOf(e.Mmr1,&e.Tier1,&e.Div1,0,0);
			this->RankOf(e.Mmr2,&e.Tier2,&e.Div2,0,0);
		}
	}

	pMsg.Count = (BYTE)count;

	const int size = DUELRANKED_LIST_BASE + count * (int)sizeof(DUELRANKED_LISTENTRY);
	pMsg.header.set(0xD9,0x56,(BYTE)size);

	gSocketManager.DataSend(index,(BYTE*)&pMsg,size);
}

void CDuelRanked::OnWatch(int index,WORD aIndex,const char* account,const char* name,DWORD matchId,BYTE room)
{
	CHARACTER_INFO info;

	if(this->Verify(index,account,name,&info) == 0)
	{
		this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_NOT_ALLOWED_NOW,0);
		return;
	}

	DR_PLAYER* known = this->FindPlayer(info.Name);

	if(known != 0)
	{
		this->SendMsg(index,aIndex,account,0,(known->State == DUELRANKED_STATE_QUEUED) ? DUELRANKED_MSG_ALREADY_QUEUED : DUELRANKED_MSG_IN_MATCH,0);
		return;
	}

	if(this->HostAvailable(false) == false)
	{
		this->SendMsg(index,aIndex,account,0,DUELRANKED_MSG_ARENA_UNAVAILABLE,0);
		return;
	}

	DR_MATCH* m = this->FindMatch(matchId);

	if(m == 0 || m->Phase != DR_PHASE_FIGHTING || m->Room > 3)
	{
		this->SendMsg(index,aIndex,account,matchId,DUELRANKED_MSG_WATCH_NOT_FOUND,0);
		return;
	}

	if(this->CountSpectators(matchId) >= this->m_Cfg.MaxSpectators)
	{
		this->SendMsg(index,aIndex,account,matchId,DUELRANKED_MSG_WATCH_FULL,0);
		return;
	}

	const DWORD now = GetTickCount();

	DR_PLAYER np;
	memset(&np,0,sizeof(np));
	CopyText(np.Name,sizeof(np.Name),info.Name,10);
	CopyText(np.Account,sizeof(np.Account),info.Account,10);
	np.State = DUELRANKED_STATE_WATCHING;
	np.ServerCode = info.GameServerCode;
	np.MatchId = matchId;
	np.StateTick = now;
	np.LastGoTick = now;

	// The room comes from the match (the host told us on STARTED); the room in the
	// request is only what the client's list showed.
	if(this->SendGo(np.Name,np.Account,matchId,DUELRANKED_KIND_SPECTATOR,m->Room,index) == 0)
	{
		this->SendMsg(index,aIndex,account,matchId,DUELRANKED_MSG_NOT_ALLOWED_NOW,0);
		return;
	}

	this->m_Players[Key(np.Name)] = np;
}

// The GameServer could not move the player when ordered.
void CDuelRanked::OnCantGo(const char* name,DWORD matchId)
{
	DR_PLAYER* p = this->FindPlayer(name);

	if(p == 0 || p->MatchId != matchId)
		return;

	if(p->State == DUELRANKED_STATE_STAGING)
	{
		DR_MATCH* m = this->FindMatch(matchId);

		if(m == 0)
			return;

		const int slot = p->Slot;

		LogAdd(LOG_BLACK,"[DuelRanked] match %d: %s could not be moved, voided",(int)matchId,p->Name);

		// Same as a no-show for that player, but a block is not punished.
		this->VoidMatch(matchId,3,DUELRANKED_CANCEL_VOID,1 << slot,0,true);
	}
	else if(p->State == DUELRANKED_STATE_WATCHING)
	{
		this->ErasePlayer(name);
	}
}

//////////////////////////////////////////////////////////////////////
// Reports from the host and the origin servers
//////////////////////////////////////////////////////////////////////

void CDuelRanked::OnHostReport(SDHP_DUELRANKED_HOSTREPORT* lpMsg)
{
	this->m_Host = *lpMsg;

	for(int r = 0; r < 4; r++)
	{
		this->m_Host.Rooms[r].Name1[10] = 0;
		this->m_Host.Rooms[r].Name2[10] = 0;
	}

	this->m_HostReported = true;
	this->m_HostReportTick = GetTickCount();
}

void CDuelRanked::OnDepart(SDHP_DUELRANKED_DEPART* lpMsg)
{
	char name[11];
	char account[11];
	CopyText(name,sizeof(name),lpMsg->Name,10);
	CopyText(account,sizeof(account),lpMsg->Account,10);

	DR_PLAYER* p = this->FindPlayer(name);
	DR_MATCH* m = this->FindMatch(lpMsg->MatchId);

	if(m == 0)
	{
		// The match died between GO and DEPART (the other fighter was blocked, say).
		// This fighter left his origin anyway and lands on the host, which knows
		// nothing about him: it has to be told, or he stands on the arena map.
		if(lpMsg->Kind == DUELRANKED_KIND_FIGHTER && lpMsg->MatchId != 0 && (lpMsg->MatchId & kTokenBit) == 0)
			this->SendStrayFighter(lpMsg);

		return;
	}

	if(p == 0 || p->MatchId != lpMsg->MatchId || _stricmp(p->Account,account) != 0)
		return;

	if(lpMsg->Kind == DUELRANKED_KIND_FIGHTER && p->State == DUELRANKED_STATE_STAGING && m->Phase == DR_PHASE_STAGING)
	{
		const int slot = p->Slot;
		const BYTE bit = (BYTE)(1 << slot);
		DR_SLOT* s = &m->Slot[slot];

		s->Departed = 1;

		// Every fighter is announced to the host the moment he leaves his origin
		// (not when both have), so one who lands before the match dies is known
		// there and goes home with it. The first DEPART fixes his origin: a second
		// one (GO was sent again) could come from a server he only passed through.
		if((m->ExpectSent & bit) == 0)
		{
			m->ExpectSent |= bit;

			s->OriginServer = lpMsg->OriginServer;
			s->OriginMap = lpMsg->OriginMap;
			s->OriginX = lpMsg->OriginX;
			s->OriginY = lpMsg->OriginY;

			if(this->SendExpect(m,slot) == 0)
			{
				LogAdd(LOG_RED,"[DuelRanked] match %d: the host is not reachable, voided",(int)m->Id);
				this->VoidMatch(m->Id,3,DUELRANKED_CANCEL_VOID,0,0,false);
			}
		}
	}
	else if(lpMsg->Kind == DUELRANKED_KIND_SPECTATOR && p->State == DUELRANKED_STATE_WATCHING && p->Departed == 0 && m->Phase == DR_PHASE_FIGHTING)
	{
		p->Departed = 1;
		p->OriginServer = lpMsg->OriginServer;
		p->OriginMap = lpMsg->OriginMap;
		p->OriginX = lpMsg->OriginX;
		p->OriginY = lpMsg->OriginY;

		if(this->SendExpectSpectator(m,p) == 0)
			this->ErasePlayer(name);
	}
}

void CDuelRanked::OnHostEvent(SDHP_DUELRANKED_HOSTEVENT* lpMsg)
{
	char name[11];
	CopyText(name,sizeof(name),lpMsg->Name,10);

	DR_MATCH* m = this->FindMatch(lpMsg->MatchId);

	if(m == 0)
		return;

	const DWORD now = GetTickCount();

	int slot = -1;

	if(name[0] != 0)
	{
		if(_stricmp(m->Slot[0].Name,name) == 0) slot = 0;
		else if(_stricmp(m->Slot[1].Name,name) == 0) slot = 1;
	}

	switch(lpMsg->Event)
	{
		case DUELRANKED_EVENT_ARRIVED:
			if(slot >= 0)
				m->Slot[slot].Arrived = 1;
			break;
		case DUELRANKED_EVENT_STARTED:
			if(m->Phase == DR_PHASE_STAGING)
			{
				m->Phase = DR_PHASE_FIGHTING;
				m->Room = lpMsg->Room;
				m->StartTick = now;
				m->Deadline = After(now,this->m_Cfg.MaxMatchSec + this->m_Cfg.ReconnectGraceSec + this->m_Cfg.WatchdogExtraSec);
				m->Slot[0].Arrived = 1;
				m->Slot[1].Arrived = 1;

				for(int s = 0; s < 2; s++)
				{
					DR_PLAYER* p = this->FindPlayer(m->Slot[s].Name);

					if(p != 0 && p->MatchId == m->Id)
					{
						p->State = DUELRANKED_STATE_FIGHTING;
						p->StateTick = now;
					}
				}

				LogAdd(LOG_GREEN,"[DuelRanked] match %d started: %s (%d) vs %s (%d), room %d",(int)m->Id,m->Slot[0].Name,m->Slot[0].Mmr,m->Slot[1].Name,m->Slot[1].Mmr,(int)lpMsg->Room);
			}
			break;
		case DUELRANKED_EVENT_ABORT:
			// The host gave up on a match nobody could start. It already sent the
			// arrived players home, so no MATCHCANCEL back.
			LogAdd(LOG_BLACK,"[DuelRanked] match %d aborted by the host (%s)",(int)m->Id,name);
			if(m->Phase == DR_PHASE_STAGING)
				this->VoidMatch(m->Id,3,0,(slot >= 0) ? (1 << slot) : 0,this->m_Cfg.NoShowCooldownSec,true);
			else
				this->VoidMatch(m->Id,3,0,0,0,false);	// a running fight the host gave up on: no blame
			break;
		case DUELRANKED_EVENT_PAUSED:
			if(slot >= 0)
			{
				m->Slot[slot].Absent = 1;

				// It may have logged in somewhere else before this report reached us.
				CHARACTER_INFO info;
				int serverIndex;
				DR_PLAYER* p = this->FindPlayer(name);

				if(p != 0 && p->MatchId == m->Id && this->Resolve(name,&info,&serverIndex) != 0 && info.GameServerCode != (WORD)this->m_Cfg.HostServer && (now - p->LastGoTick) >= kGoResendMs)
				{
					p->LastGoTick = now;
					this->SendGo(p->Name,p->Account,m->Id,DUELRANKED_KIND_RESUME,m->Room,serverIndex);
				}
			}
			break;
		case DUELRANKED_EVENT_RESUMED:
			if(slot >= 0)
				m->Slot[slot].Absent = 0;
			break;
		case DUELRANKED_EVENT_SPECTATOR_LEFT:
		{
			DR_PLAYER* p = this->FindPlayer(name);

			if(p != 0 && p->State == DUELRANKED_STATE_WATCHING && p->MatchId == m->Id)
				this->ErasePlayer(name);
		}
			break;
		case DUELRANKED_EVENT_SPECTATOR_ARRIVED:
		{
			DR_PLAYER* p = this->FindPlayer(name);

			if(p != 0 && p->State == DUELRANKED_STATE_WATCHING && p->MatchId == m->Id)
				p->Arrived = 1;
		}
			break;
	}
}

void CDuelRanked::OnResult(SDHP_DUELRANKED_RESULT* lpMsg,int index)
{
	char winner[11];
	char loser[11];
	CopyText(winner,sizeof(winner),lpMsg->Winner,10);
	CopyText(loser,sizeof(loser),lpMsg->Loser,10);

	if(lpMsg->MatchId == 0 || (lpMsg->MatchId & kTokenBit) != 0)
		return;

	char nameA[11] = {0};
	char nameB[11] = {0};

	DR_MATCH* m = this->FindMatch(lpMsg->MatchId);

	if(m != 0)
	{
		CopyText(nameA,sizeof(nameA),m->Slot[0].Name,10);
		CopyText(nameB,sizeof(nameB),m->Slot[1].Name,10);
	}
	else
	{
		// Not in memory (the DataServer restarted since): the database knows.
		const int found = this->DbMatchGet(lpMsg->MatchId,nameA,nameB);

		if(found < 0)
			return;	// no answer, the host sends it again

		if(found == 0)
		{
			LogAdd(LOG_RED,"[DuelRanked] result for unknown match %d ignored",(int)lpMsg->MatchId);
			this->SendApplied(index,lpMsg->MatchId,0);
			return;
		}
	}

	const bool winnerIsA = (_stricmp(winner,nameA) == 0);
	const bool winnerIsB = (_stricmp(winner,nameB) == 0);
	const bool loserIsA = (_stricmp(loser,nameA) == 0);
	const bool loserIsB = (_stricmp(loser,nameB) == 0);

	if((winnerIsA == false || loserIsB == false) && (winnerIsB == false || loserIsA == false))
	{
		LogAdd(LOG_RED,"[DuelRanked] result for match %d names the wrong players, ignored",(int)lpMsg->MatchId);
		this->SendApplied(index,lpMsg->MatchId,0);
		return;
	}

	const int winnerSlot = (lpMsg->IsDraw != 0) ? 0 : ((winnerIsA) ? 1 : 2);
	const int scoreA = (winnerIsA) ? lpMsg->ScoreW : lpMsg->ScoreL;
	const int scoreB = (winnerIsA) ? lpMsg->ScoreL : lpMsg->ScoreW;

	DR_RESULTROW row;

	if(this->DbResult(lpMsg->MatchId,winnerSlot,lpMsg->Reason,scoreA,scoreB,&row) == 0)
	{
		LogAdd(LOG_RED,"[DuelRanked] could not settle match %d, the host will send it again",(int)lpMsg->MatchId);
		return;
	}

	if(row.Applied == 1)
	{
		LogAdd(LOG_GREEN,"[DuelRanked] match %d: %s %d (%+d) vs %s %d (%+d), reason %d",(int)lpMsg->MatchId,row.NameA,row.NewA,row.DeltaA,row.NameB,row.NewB,row.DeltaB,(int)lpMsg->Reason);
	}

	this->SendApplied(index,lpMsg->MatchId,&row);

	// Both players' rank icons follow the new rating, wherever they are logged in.
	if(row.Applied == 1)
	{
		this->SendRankToName(row.NameA);
		this->SendRankToName(row.NameB);

		// Only on the first application (Applied == 1), so a resent result never pays twice.
		this->GrantRankRewards(row.NameA,row.MmrABefore,row.NewA);
		this->GrantRankRewards(row.NameB,row.MmrBBefore,row.NewB);
	}

	if(m == 0)
		return;

	// Settled: the fighters are free again, and these two don't meet straight away.
	this->SetRematch(nameA,nameB);

	const DWORD id = m->Id;

	for(int s = 0; s < 2; s++)
	{
		DR_PLAYER* p = this->FindPlayer((s == 0) ? nameA : nameB);

		if(p != 0 && p->MatchId == id)
			this->ErasePlayer((s == 0) ? nameA : nameB);
	}

	this->EraseSpectators(id);
	this->m_Matches.erase(id);
}

//////////////////////////////////////////////////////////////////////
// C1:70 / C1:71
//////////////////////////////////////////////////////////////////////

void CDuelRanked::OnCharacterRegistered(const char* name,int index)
{
	if(this->m_CfgValid == false || SERVER_RANGE(index) == 0)
		return;

	char nm[11];
	CopyText(nm,sizeof(nm),name,10);

	// Every login: the GameServer shows this character's rank to the players around it.
	{
		CHARACTER_INFO rankInfo;

		if(gCharacterManager.GetCharacterInfo(&rankInfo,nm) != 0)
			this->SendRank(index,rankInfo.Name,rankInfo.Account);
	}

	DR_PLAYER* p = this->FindPlayer(nm);

	if(p == 0)
		return;

	const WORD code = gServerManager[index].m_ServerCode;
	const DWORD now = GetTickCount();

	p->ServerCode = code;

	if(code == (WORD)this->m_Cfg.HostServer || (now - p->LastGoTick) < kGoResendMs)
		return;

	DR_MATCH* m = this->FindMatch(p->MatchId);

	if(m == 0)
		return;

	if(p->State == DUELRANKED_STATE_FIGHTING && m->Slot[p->Slot].Absent != 0)
	{
		// A fighter that dropped logged in on another server: send it back to the host.
		p->LastGoTick = now;
		this->SendGo(p->Name,p->Account,m->Id,DUELRANKED_KIND_RESUME,m->Room,index);
	}
	else if(p->State == DUELRANKED_STATE_STAGING && m->Phase == DR_PHASE_STAGING && m->Slot[p->Slot].Arrived == 0)
	{
		// Dropped while moving to the host and came back somewhere else.
		p->LastGoTick = now;
		this->SendGo(p->Name,p->Account,m->Id,DUELRANKED_KIND_FIGHTER,0xFF,index);
	}
}

void CDuelRanked::OnCharacterLeft(const char* name,int index)
{
	if(this->m_CfgValid == false)
		return;

	char nm[11];
	CopyText(nm,sizeof(nm),name,10);

	DR_PLAYER* p = this->FindPlayer(nm);

	if(p == 0)
		return;

	// Staging, fighting and watching: this is our own hop (or the reconnect case),
	// the host and the timers settle those.
	if(p->State == DUELRANKED_STATE_QUEUED)
	{
		// A fighter sent home after a void is still moving between servers.
		if(p->KeepUntil != 0 && Past(p->KeepUntil,GetTickCount()) == false)
			return;

		this->ErasePlayer(nm);
	}
	else if(p->State == DUELRANKED_STATE_PROMPT)
	{
		const DWORD token = p->MatchId;
		const int slot = p->Slot;

		this->CancelPrompt(token,0,1 << slot);
	}
}

//////////////////////////////////////////////////////////////////////
// Pairing and the match state machine
//////////////////////////////////////////////////////////////////////

// How far apart two ratings may be for this player, from how long it has waited.
int CDuelRanked::Window(const DR_PLAYER* p,DWORD now)
{
	const int wait = (int)((now - p->QueuedTick) / 1000);

	if(this->m_Cfg.AnyOpponentAfterSec > 0 && wait >= this->m_Cfg.AnyOpponentAfterSec)
		return 0x7FFFFFFF;

	__int64 window = (__int64)this->m_Cfg.BaseWindow + (__int64)(wait / this->m_Cfg.WidenSecs) * this->m_Cfg.WidenStep;

	if(window > this->m_Cfg.MaxWindow)
		window = this->m_Cfg.MaxWindow;

	return (int)window;
}

namespace
{
	struct PairCandidate
	{
		DR_PLAYER* p;
		CHARACTER_INFO info;
		int index;
		DWORD wait;
		bool used;
	};

	bool LongerWait(const PairCandidate& a,const PairCandidate& b)
	{
		return a.wait > b.wait;
	}
}

void CDuelRanked::TryPair()
{
	if(this->m_Enabled == false)
		return;

	const DWORD now = GetTickCount();

	if(this->m_PairHold != 0)
	{
		if(Past(this->m_PairHold,now) == false)
			return;

		this->m_PairHold = 0;
	}

	if(this->HostAvailable(true) == false)
		return;

	// Matches already offered or on their way to the host still need a room.
	int active = 0;
	int early = 0;

	for(std::map<DWORD,DR_MATCH>::iterator it = this->m_Matches.begin(); it != this->m_Matches.end(); it++)
	{
		active++;

		if(it->second.Phase != DR_PHASE_FIGHTING)
			early++;
	}

	if(active >= this->m_Cfg.MaxConcurrentMatches || (int)this->m_Host.FreeRooms <= early)
		return;

	std::vector<PairCandidate> list;
	std::vector<std::string> gone;

	for(std::map<std::string,DR_PLAYER>::iterator it = this->m_Players.begin(); it != this->m_Players.end(); it++)
	{
		DR_PLAYER* p = &it->second;

		if(p->State != DUELRANKED_STATE_QUEUED)
			continue;

		if(p->KeepUntil != 0)
		{
			// Still hopping home from a voided match.
			if(Past(p->KeepUntil,now) == false)
				continue;

			p->KeepUntil = 0;
		}

		PairCandidate c;
		c.p = p;
		c.used = false;
		c.wait = now - p->QueuedTick;

		if(this->Resolve(p->Name,&c.info,&c.index) == 0 || _stricmp(c.info.Account,p->Account) != 0)
		{
			// Not registered on any server any more.
			gone.push_back(it->first);
			continue;
		}

		list.push_back(c);
	}

	for(size_t n = 0; n < gone.size(); n++)
		this->m_Players.erase(gone[n]);

	std::sort(list.begin(),list.end(),LongerWait);

	for(size_t i = 0; i < list.size(); i++)
	{
		if(list[i].used)
			continue;

		if(active >= this->m_Cfg.MaxConcurrentMatches || (int)this->m_Host.FreeRooms <= early)
			break;

		const int windowA = this->Window(list[i].p,now);

		int best = -1;
		int bestDiff = 0;

		for(size_t j = 0; j < list.size(); j++)
		{
			if(j == i || list[j].used)
				continue;

			if(_stricmp(list[i].p->Account,list[j].p->Account) == 0)
				continue;

			const int diff = abs(list[i].p->Mmr - list[j].p->Mmr);
			const int windowB = this->Window(list[j].p,now);

			if(diff > ((windowA > windowB) ? windowA : windowB))
				continue;

			if(this->RematchBlocked(list[i].p->Name,list[j].p->Name))
				continue;

			if(this->SameDeviceBlocked(list[i].p->Name,list[i].p->Account,list[j].p->Name,list[j].p->Account))
				continue;

			// Closest rating wins; on a tie the longer waiter (earlier in the list).
			if(best == -1 || diff < bestDiff)
			{
				best = (int)j;
				bestDiff = diff;
			}
		}

		if(best == -1)
			continue;

		list[i].used = true;
		list[best].used = true;
		active++;
		early++;

		this->StartPrompt(list[i].p,list[best].p,&list[i].info,&list[best].info,list[i].index,list[best].index);
	}
}

void CDuelRanked::StartPrompt(DR_PLAYER* a,DR_PLAYER* b,const CHARACTER_INFO* infoA,const CHARACTER_INFO* infoB,int indexA,int indexB)
{
	const DWORD now = GetTickCount();

	DR_MATCH m;
	memset(&m,0,sizeof(m));
	m.Id = this->NextToken();
	m.Phase = DR_PHASE_PROMPT;
	m.Room = 0xFF;
	m.HostServer = (WORD)this->m_Cfg.HostServer;
	m.StartTick = now;
	m.Deadline = After(now,this->m_Cfg.PromptSec + this->m_Cfg.PromptGraceSec);

	DR_PLAYER* players[2] = {a,b};

	for(int s = 0; s < 2; s++)
	{
		CopyText(m.Slot[s].Name,sizeof(m.Slot[s].Name),players[s]->Name,10);
		CopyText(m.Slot[s].Account,sizeof(m.Slot[s].Account),players[s]->Account,10);
		m.Slot[s].Mmr = players[s]->Mmr;
		m.Slot[s].OriginMap = 0xFF;

		players[s]->State = DUELRANKED_STATE_PROMPT;
		players[s]->MatchId = m.Id;
		players[s]->Slot = (BYTE)s;
		players[s]->StateTick = now;
	}

	this->m_Matches[m.Id] = m;

	LogAdd(LOG_BLACK,"[DuelRanked] match offered: %s (%d) vs %s (%d)",a->Name,a->Mmr,b->Name,b->Mmr);

	this->SendPrompt(indexA,infoA,m.Id,&m.Slot[1]);
	this->SendPrompt(indexB,infoB,m.Id,&m.Slot[0]);
}

// A prompt that will not become a match. faultMask: slots that declined or timed
// out (out of the queue, with a cooldown). leftMask: slots that logged out (out,
// no message). The others go back to waiting with their original queue time.
void CDuelRanked::CancelPrompt(DWORD token,int faultMask,int leftMask)
{
	DR_MATCH* pm = this->FindPrompt(token);

	if(pm == 0)
		return;

	const DR_MATCH prompt = *pm;

	this->m_Matches.erase(token);

	const DWORD now = GetTickCount();

	for(int s = 0; s < 2; s++)
	{
		const char* name = prompt.Slot[s].Name;
		DR_PLAYER* p = this->FindPlayer(name);

		if(p == 0 || p->State != DUELRANKED_STATE_PROMPT || p->MatchId != token)
			continue;

		if((leftMask & (1 << s)) != 0)
		{
			this->ErasePlayer(name);
		}
		else if((faultMask & (1 << s)) != 0)
		{
			this->ErasePlayer(name);
			this->SetCooldown(name,this->m_Cfg.DeclineCooldownSec);
			this->SendMsgToName(name,0,DUELRANKED_MSG_REMOVED_FROM_QUEUE,0);
		}
		else
		{
			p->State = DUELRANKED_STATE_QUEUED;
			p->MatchId = 0;
			p->StateTick = now;
			this->SendMsgToName(name,0,((faultMask & (1 << (1 - s))) != 0) ? DUELRANKED_MSG_OPPONENT_DECLINED : DUELRANKED_MSG_MATCH_CANCELLED,0);
		}
	}
}

// Both accepted: open the match in the database and order the fighters to the host.
void CDuelRanked::BeginStaging(DWORD token)
{
	DR_MATCH* pm = this->FindPrompt(token);

	if(pm == 0)
		return;

	const DR_MATCH prompt = *pm;
	const DWORD now = GetTickCount();

	DR_PLAYER* p[2];
	CHARACTER_INFO info[2];
	int serverIndex[2] = {-1,-1};
	int gone = 0;

	for(int s = 0; s < 2; s++)
	{
		p[s] = this->FindPlayer(prompt.Slot[s].Name);

		if(p[s] == 0 || p[s]->State != DUELRANKED_STATE_PROMPT || p[s]->MatchId != token || this->Resolve(prompt.Slot[s].Name,&info[s],&serverIndex[s]) == 0)
			gone |= (1 << s);
	}

	if(gone != 0)
	{
		this->CancelPrompt(token,0,gone);
		return;
	}

	if(this->HostAvailable(false) == false)
	{
		this->CancelPrompt(token,0,0);
		return;
	}

	int matchId = 0;
	int mmrA = 0;
	int mmrB = 0;

	if(this->DbCreate(&prompt,&matchId,&mmrA,&mmrB) == 0)
	{
		// The database is not answering: don't offer matches for a few seconds.
		this->m_PairHold = After(now,5) | 1;
		this->CancelPrompt(token,0,0);
		return;
	}

	DR_MATCH m = prompt;
	m.Id = (DWORD)matchId;
	m.Phase = DR_PHASE_STAGING;
	m.Room = 0xFF;
	m.ExpectSent = 0;
	m.HostServer = (WORD)this->m_Cfg.HostServer;
	m.StartTick = now;
	m.Deadline = After(now,this->m_Cfg.StageTimeoutSec);
	m.Slot[0].Mmr = mmrA;
	m.Slot[1].Mmr = mmrB;

	this->m_Matches.erase(token);
	this->m_Matches[m.Id] = m;

	for(int s = 0; s < 2; s++)
	{
		p[s]->State = DUELRANKED_STATE_STAGING;
		p[s]->MatchId = m.Id;
		p[s]->Mmr = m.Slot[s].Mmr;
		p[s]->StateTick = now;
		p[s]->LastGoTick = now;
	}

	LogAdd(LOG_GREEN,"[DuelRanked] match %d: %s vs %s accepted, moving to host %d",matchId,m.Slot[0].Name,m.Slot[1].Name,this->m_Cfg.HostServer);

	// Both servers were found above, so this can only fail if one dropped just now;
	// the staging timeout then voids the match.
	for(int s = 0; s < 2; s++)
		this->SendGo(m.Slot[s].Name,m.Slot[s].Account,m.Id,DUELRANKED_KIND_FIGHTER,0xFF,serverIndex[s]);
}

// Close a match without a result. dbStatus 3 void / 4 abandoned. cancelReason 0 =
// don't tell the host (it is the one that asked, or it is gone). faultMask slots
// leave the queue with cooldownSec; the rest go back to waiting (requeueRest) or
// are told it was voided.
void CDuelRanked::VoidMatch(DWORD id,int dbStatus,int cancelReason,int faultMask,int cooldownSec,bool requeueRest)
{
	DR_MATCH* pm = this->FindMatch(id);

	if(pm == 0)
		return;

	const DR_MATCH m = *pm;

	this->m_Matches.erase(id);

	this->DbVoid(id,dbStatus);

	if(cancelReason != 0)
		this->SendMatchCancel(m.HostServer,id,(BYTE)cancelReason);

	this->EraseSpectators(id);

	const DWORD now = GetTickCount();

	for(int s = 0; s < 2; s++)
	{
		const char* name = m.Slot[s].Name;
		DR_PLAYER* p = this->FindPlayer(name);

		if(p == 0 || p->MatchId != id)
			continue;

		if((faultMask & (1 << s)) != 0)
		{
			this->ErasePlayer(name);
			this->SetCooldown(name,cooldownSec);
			this->SendMsgToName(name,id,DUELRANKED_MSG_REMOVED_FROM_QUEUE,0);
		}
		else if(requeueRest)
		{
			p->State = DUELRANKED_STATE_QUEUED;
			p->MatchId = 0;
			p->Slot = 0;
			p->StateTick = now;
			// One that has not landed yet may still be hopping to the host (his
			// DEPART can even reach us after this): the host sends him back when he
			// appears, which can take longer than the hop home.
			p->KeepUntil = After(now,kHomeHopSec + ((m.Slot[s].Arrived == 0) ? kTransitHomeSec : 0));
			this->SendMsgToName(name,id,DUELRANKED_MSG_MATCH_CANCELLED,0);
		}
		else
		{
			this->ErasePlayer(name);
			this->SendMsgToName(name,id,DUELRANKED_MSG_MATCH_VOID,0);
		}
	}

	LogAdd(LOG_BLACK,"[DuelRanked] match %d voided (%s vs %s)",(int)id,m.Slot[0].Name,m.Slot[1].Name);
}

void CDuelRanked::EraseSpectators(DWORD matchId)
{
	for(std::map<std::string,DR_PLAYER>::iterator it = this->m_Players.begin(); it != this->m_Players.end();)
	{
		if(it->second.State == DUELRANKED_STATE_WATCHING && it->second.MatchId == matchId)
			it = this->m_Players.erase(it);
		else
			it++;
	}
}

// Ranked went off (or the arena is gone for good): nobody waits for it.
void CDuelRanked::DropQueue(BYTE code)
{
	std::vector<DWORD> tokens;
	std::vector<std::string> names;

	for(std::map<DWORD,DR_MATCH>::iterator it = this->m_Matches.begin(); it != this->m_Matches.end(); it++)
	{
		if(it->second.Phase == DR_PHASE_PROMPT)
			tokens.push_back(it->first);
	}

	for(size_t n = 0; n < tokens.size(); n++)
		this->CancelPrompt(tokens[n],0,0);

	for(std::map<std::string,DR_PLAYER>::iterator it = this->m_Players.begin(); it != this->m_Players.end(); it++)
	{
		if(it->second.State == DUELRANKED_STATE_QUEUED)
			names.push_back(it->first);
	}

	for(size_t n = 0; n < names.size(); n++)
	{
		char name[11];
		DR_PLAYER* p = this->FindPlayer(names[n].c_str());

		if(p == 0)
			continue;

		CopyText(name,sizeof(name),p->Name,10);
		this->ErasePlayer(name);
		this->SendMsgToName(name,0,code,0);
	}
}

// A GameServer connection closed (queue thread; the IOCP thread only noted it).
void CDuelRanked::HandleServerDown(WORD serverCode)
{
	if(serverCode == (WORD)this->m_Cfg.HostServer)
	{
		this->m_HostReported = false;

		std::vector<DWORD> ids;

		for(std::map<DWORD,DR_MATCH>::iterator it = this->m_Matches.begin(); it != this->m_Matches.end(); it++)
		{
			if(it->second.Phase != DR_PHASE_PROMPT)
				ids.push_back(it->first);
		}

		for(size_t n = 0; n < ids.size(); n++)
		{
			LogAdd(LOG_RED,"[DuelRanked] host %d went offline, match %d voided",(int)serverCode,(int)ids[n]);
			this->VoidMatch(ids[n],3,0,0,0,false);
		}
	}

	struct Dropped
	{
		std::string name;
		DWORD token;
		int slot;
		BYTE state;
	};

	std::vector<Dropped> dropped;

	for(std::map<std::string,DR_PLAYER>::iterator it = this->m_Players.begin(); it != this->m_Players.end(); it++)
	{
		const DR_PLAYER& p = it->second;

		if(p.ServerCode != serverCode || (p.State != DUELRANKED_STATE_QUEUED && p.State != DUELRANKED_STATE_PROMPT))
			continue;

		Dropped d;
		d.name = it->first;
		d.token = p.MatchId;
		d.slot = p.Slot;
		d.state = p.State;
		dropped.push_back(d);
	}

	for(size_t n = 0; n < dropped.size(); n++)
	{
		if(dropped[n].state == DUELRANKED_STATE_PROMPT)
			this->CancelPrompt(dropped[n].token,0,1 << dropped[n].slot);
		else
			this->m_Players.erase(dropped[n].name);
	}
}

//////////////////////////////////////////////////////////////////////
// The 1 second tick
//////////////////////////////////////////////////////////////////////

void CDuelRanked::Tick()
{
	InterlockedExchange(&this->m_TickPending,0);

	if(this->m_ReloadPending != 0)
	{
		InterlockedExchange(&this->m_ReloadPending,0);
		this->LoadConfig(false);
	}

	if(this->m_CfgValid == false)
		return;

	this->m_TickCount++;

	const DWORD now = GetTickCount();

	this->DrainDown();

	if(this->m_Enabled == false)
	{
		this->DropQueue(DUELRANKED_MSG_RANKED_DISABLED);
	}
	else if(this->HostAvailable(false))
	{
		// Busy is not gone: players keep waiting for room, only a missing arena drops them.
		this->m_UnavailableSince = 0;
	}
	else if(this->m_UnavailableSince == 0)
	{
		this->m_UnavailableSince = now | 1;
	}
	else if(this->m_Cfg.UnavailableDropSec > 0 && Past(After(this->m_UnavailableSince,this->m_Cfg.UnavailableDropSec),now))
	{
		// Waiting for an arena that does not come back.
		this->m_UnavailableSince = now | 1;
		this->DropQueue(DUELRANKED_MSG_ARENA_UNAVAILABLE);
	}

	// Prompts, staging and the fight watchdog.
	std::vector<DWORD> ids;

	for(std::map<DWORD,DR_MATCH>::iterator it = this->m_Matches.begin(); it != this->m_Matches.end(); it++)
		ids.push_back(it->first);

	for(size_t n = 0; n < ids.size(); n++)
	{
		std::map<DWORD,DR_MATCH>::iterator it = this->m_Matches.find(ids[n]);

		if(it == this->m_Matches.end() || Past(it->second.Deadline,now) == false)
			continue;

		const DR_MATCH m = it->second;

		if(m.Phase == DR_PHASE_PROMPT)
		{
			const int silent = ((m.Slot[0].Accepted == 0) ? 1 : 0) | ((m.Slot[1].Accepted == 0) ? 2 : 0);

			LogAdd(LOG_BLACK,"[DuelRanked] prompt for %s and %s timed out",m.Slot[0].Name,m.Slot[1].Name);
			this->CancelPrompt(m.Id,silent,0);
		}
		else if(m.Phase == DR_PHASE_STAGING)
		{
			// Only blame a fighter when the other one did turn up.
			const int missing = ((m.Slot[0].Arrived == 0) ? 1 : 0) | ((m.Slot[1].Arrived == 0) ? 2 : 0);
			const int fault = (missing == 1 || missing == 2) ? missing : 0;

			LogAdd(LOG_BLACK,"[DuelRanked] match %d: the fighters did not all arrive in time",(int)m.Id);
			this->VoidMatch(m.Id,3,DUELRANKED_CANCEL_NOSHOW,fault,this->m_Cfg.NoShowCooldownSec,true);
		}
		else
		{
			LogAdd(LOG_RED,"[DuelRanked] match %d: no result from the host, abandoned",(int)m.Id);
			this->VoidMatch(m.Id,4,DUELRANKED_CANCEL_WATCHDOG,0,0,false);
		}
	}

	this->Housekeeping(now);

	if(this->m_Enabled && (this->m_TickCount == 20 || (this->m_TickCount % 3600) == 0))
		this->DbSweep();

	this->TryPair();

	InterlockedExchange(&this->m_Active,(this->m_Players.empty() && this->m_Matches.empty()) ? 0 : 1);
}

void CDuelRanked::Housekeeping(DWORD now)
{
	// Anyone still marked as in a match whose match is gone, and spectators whose
	// hop never reached the host.
	std::vector<std::string> stale;

	for(std::map<std::string,DR_PLAYER>::iterator it = this->m_Players.begin(); it != this->m_Players.end(); it++)
	{
		const DR_PLAYER& p = it->second;

		if(p.State == DUELRANKED_STATE_QUEUED)
			continue;

		if(p.State == DUELRANKED_STATE_PROMPT)
		{
			if(this->FindPrompt(p.MatchId) == 0)
				stale.push_back(it->first);
		}
		else if(this->FindMatch(p.MatchId) == 0)
		{
			stale.push_back(it->first);
		}
		else if(p.State == DUELRANKED_STATE_WATCHING && p.Arrived == 0 && Past(After(p.StateTick,this->m_Cfg.StageTimeoutSec),now))
		{
			stale.push_back(it->first);
		}
	}

	for(size_t n = 0; n < stale.size(); n++)
		this->m_Players.erase(stale[n]);

	if((this->m_TickCount % 30) != 0)
		return;

	for(std::map<std::string,DWORD>::iterator it = this->m_Cooldown.begin(); it != this->m_Cooldown.end();)
	{
		if(Past(it->second,now))
			it = this->m_Cooldown.erase(it);
		else
			it++;
	}

	for(std::map<std::string,DWORD>::iterator it = this->m_Rematch.begin(); it != this->m_Rematch.end();)
	{
		if(Past(it->second,now))
			it = this->m_Rematch.erase(it);
		else
			it++;
	}

	for(std::map<std::string,std::pair<DWORD,bool> >::iterator it = this->m_DeviceCache.begin(); it != this->m_DeviceCache.end();)
	{
		if(Past(it->second.first,now))
			it = this->m_DeviceCache.erase(it);
		else
			it++;
	}
}
