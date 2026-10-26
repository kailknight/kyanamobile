// MasterPage.cpp: Master skill tree pages, GameServer side. See MasterPage.h.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MasterPage.h"
#include "DSProtocol.h"
#include "DuelRanked.h"
#include "GameMain.h"
#include "Map.h"
#include "MasterSkillTree.h"
#include "Notice.h"
#include "ObjectManager.h"
#include "ServerInfo.h"
#include "SkillManager.h"
#include "User.h"
#include "Util.h"

CMasterPage gMasterPage;

CMasterPage::CMasterPage() // OK
{

}

CMasterPage::~CMasterPage() // OK
{

}

void CMasterPage::Lock() // OK
{
	this->m_lock.lock();
}

void CMasterPage::Unlock() // OK
{
	this->m_lock.unlock();
}

CMasterPageGuard::CMasterPageGuard() // OK
{
	gMasterPage.Lock();
}

CMasterPageGuard::~CMasterPageGuard() // OK
{
	gMasterPage.Unlock();
}

#if(GAMESERVER_UPDATE>=602)

//**********************************************//
//***************** helpers ********************//
//**********************************************//

// Empty entries are FF 00 FF, like CSkillManager::SkillByteConvert writes a cleared slot.
static void MasterPageFillEmpty(BYTE* blob) // OK
{
	for(int n=0;n < MAX_MASTER_SKILL_LIST;n++)
	{
		blob[(n*3)+0] = 0xFF;
		blob[(n*3)+1] = 0x00;
		blob[(n*3)+2] = 0xFF;
	}
}

static void MasterPageDefaultName(char* name,int page) // OK
{
	sprintf_s(name,MASTER_PAGE_NAME_SIZE,"Page %d",page);
}

// Trims and checks a page name that came off the wire or the DataServer: 1-12
// characters, ASCII letters, digits, space, '-' and '_' only. 'in' is a fixed
// buffer that need not be terminated; 'out' is MASTER_PAGE_NAME_SIZE bytes.
static bool MasterPageCleanName(const char* in,int inSize,char* out) // OK
{
	char tmp[MASTER_PAGE_NAME_SIZE+1];

	int len = 0;

	while(len < inSize && len < MASTER_PAGE_NAME_SIZE && in[len] != 0)
	{
		tmp[len] = in[len];
		len++;
	}

	if(len >= MASTER_PAGE_NAME_SIZE)
	{
		return 0; // 13 characters and no terminator: too long
	}

	tmp[len] = 0;

	int start = 0;

	while(start < len && tmp[start] == ' ')
	{
		start++;
	}

	int end = len;

	while(end > start && tmp[end-1] == ' ')
	{
		end--;
	}

	if(end <= start)
	{
		return 0;
	}

	for(int n=start;n < end;n++)
	{
		const char c = tmp[n];

		if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_'))
		{
			return 0;
		}
	}

	memset(out,0,MASTER_PAGE_NAME_SIZE);

	memcpy(out,&tmp[start],(end-start));

	return 1;
}

// What a master skill of 'level' (0 based, like CSkill::m_level) has cost.
//
// CMasterSkillTree::CGMasterSkillRecv charges info.MinLevel for EVERY point:
//   first point: skill is created at level MinLevel-1       -> 1 point
//   each further point: level+1, until level+1 >= MaxLevel  -> level can reach MaxLevel-1
// so a skill sitting at level L has taken (L - (MinLevel-1) + 1) points and
// cost MinLevel * (L - MinLevel + 2). MinLevel is 1 for most skills (cost L+1)
// and 10 for the few that start at level 10 (cost 10 per point).
static int MasterPagePoints(MASTER_SKILL_TREE_INFO* lpInfo,int level) // OK
{
	const int first = lpInfo->MinLevel-1;

	int last = lpInfo->MaxLevel-1;

	last = ((last < first) ? first : last);

	level = ((level < first) ? first : level);

	level = ((level > last) ? last : level);

	return lpInfo->MinLevel*(level-first+1);
}

static bool MasterPageEntryLess(const MASTER_PAGE_ENTRY& a,const MASTER_PAGE_ENTRY& b) // OK
{
	// Rank order is a valid dependency order: every skill a tree row replaces
	// or aliases sits on a lower rank (checked against MasterSkillTree.txt).
	return ((a.Rank != b.Rank) ? (a.Rank < b.Rank) : (a.Index < b.Index));
}

int CMasterPage::GetMaxCount() // OK
{
	int count = gServerInfo.m_MasterPageMaxCount;

	count = ((count > MASTER_PAGE_COUNT) ? MASTER_PAGE_COUNT : count);

	count = ((count < 1) ? 1 : count);

	return count;
}

// Points spent on the live page, charged the way CGMasterSkillRecv charges them.
int CMasterPage::GetSpentLive(LPOBJ lpObj) // OK
{
	int spent = 0;

	for(int n=0;n < MAX_MASTER_SKILL_LIST;n++)
	{
		if(lpObj->MasterSkill[n].IsMasterSkill() == 0)
		{
			continue;
		}

		MASTER_SKILL_TREE_INFO info;

		if(gMasterSkillTree.GetInfo(lpObj->MasterSkill[n].m_index,&info) == 0)
		{
			continue;
		}

		spent += MasterPagePoints(&info,lpObj->MasterSkill[n].m_level);
	}

	return spent;
}

int CMasterPage::GetEarned(LPOBJ lpObj) // OK
{
	return (lpObj->MasterPoint+this->GetSpentLive(lpObj));
}

// Reads a parked page. Entries the tree no longer knows, that belong to another
// class or that repeat are left out (counted in *lpDropped) - they stop costing
// points, which is the refund. Returns the points the kept entries cost; the
// list comes back in the order they must be learned.
int CMasterPage::DecodeBlob(LPOBJ lpObj,const BYTE* blob,std::vector<MASTER_PAGE_ENTRY>* lpList,int* lpDropped) // OK
{
	std::vector<MASTER_PAGE_ENTRY> local;

	std::vector<MASTER_PAGE_ENTRY>* list = ((lpList == 0) ? &local : lpList);

	list->clear();

	int dropped = 0;

	for(int n=0;n < MAX_MASTER_SKILL_LIST;n++)
	{
		CSkill skill;

		if(gSkillManager.ConvertSkillByte(&skill,(BYTE*)&blob[n*3]) == 0)
		{
			continue;
		}

		MASTER_SKILL_TREE_INFO info;

		if(gMasterSkillTree.GetInfo(skill.m_index,&info) == 0)
		{
			dropped++;
			continue;
		}

		if(CHECK_RANGE(lpObj->Class,MAX_CLASS) == 0 || info.RequireClass[lpObj->Class] == 0)
		{
			dropped++;
			continue;
		}

		bool repeated = 0;

		for(size_t i=0;i < list->size();i++)
		{
			if((*list)[i].Index == info.Index)
			{
				repeated = 1;
				break;
			}
		}

		if(repeated != 0)
		{
			dropped++;
			continue;
		}

		const int first = info.MinLevel-1;

		int last = info.MaxLevel-1;

		last = ((last < first) ? first : last);

		int level = skill.m_level;

		level = ((level < first) ? first : level);

		level = ((level > last) ? last : level);

		MASTER_PAGE_ENTRY entry;

		entry.Index = info.Index;
		entry.Rank = info.Rank;
		entry.Level = level;
		entry.Points = MasterPagePoints(&info,level);
		entry.ClientSkill = (BYTE)info.RequireClass[lpObj->Class];

		list->push_back(entry);
	}

	std::sort(list->begin(),list->end(),MasterPageEntryLess);

	int total = 0;

	for(size_t i=0;i < list->size();i++)
	{
		total += (*list)[i].Points;
	}

	if(lpDropped != 0)
	{
		(*lpDropped) = dropped;
	}

	return total;
}

//**********************************************//
//**************** state glue ******************//
//**********************************************//

void CMasterPage::Reset(LPOBJ lpObj) // OK
{
	lpObj->LoadMasterPages = 0;

	if(lpObj->MasterPages == 0)
	{
		return;
	}

	this->m_lock.lock();

	MASTER_PAGES_DATA* lpPages = lpObj->MasterPages;

	memset(lpPages,0,sizeof(MASTER_PAGES_DATA));

	lpPages->Active = 1;
	lpPages->Unlocked = 1;

	for(int n=0;n < MASTER_PAGE_COUNT;n++)
	{
		MasterPageDefaultName(lpPages->Name[n],(n+1));

		MasterPageFillEmpty(lpPages->Skill[n]);
	}

	this->m_lock.unlock();
}

bool CMasterPage::IsReady(LPOBJ lpObj) // OK
{
	if(gServerInfo.m_MasterSkillTree == 0 || gServerInfo.m_MasterPageSwitch == 0)
	{
		return 0;
	}

	if(lpObj->Type != OBJECT_USER || lpObj->MasterPages == 0)
	{
		return 0;
	}

	return ((lpObj->LoadMasterPages != 0 && lpObj->LoadMasterLevel != 0) ? 1 : 0);
}

void CMasterPage::OnTreeLoaded(LPOBJ lpObj) // OK
{
	// The pages answer normally arrives right after the tree one. Whichever came
	// last tells the client.
	if(this->IsReady(lpObj) != 0)
	{
		this->GCPagesInfoSend(lpObj->Index);
	}
}

void CMasterPage::OnFullWipe(LPOBJ lpObj) // OK
{
	if(lpObj->MasterPages == 0)
	{
		return;
	}

	this->m_lock.lock();

	MASTER_PAGES_DATA* lpPages = lpObj->MasterPages;

	for(int n=0;n < MASTER_PAGE_COUNT;n++)
	{
		MasterPageFillEmpty(lpPages->Skill[n]);
	}

	lpPages->Active = 1;

	lpPages->Dirty = 1;

	this->m_lock.unlock();

	if(this->IsReady(lpObj) != 0)
	{
		this->GCPagesInfoSend(lpObj->Index);
	}
}

//**********************************************//
//***************** DataServer *****************//
//**********************************************//

void CMasterPage::GDPagesLoadSend(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(gServerInfo.m_MasterSkillTree == 0 || gServerInfo.m_MasterPageSwitch == 0)
	{
		return;
	}

	if(lpObj->Type != OBJECT_USER || lpObj->MasterPages == 0 || lpObj->LoadMasterPages != 0)
	{
		return;
	}

	if(gObjIsAccountValid(aIndex,lpObj->Account) == 0)
	{
		return;
	}

	SDHP_MASTER_PAGES_REQ_SEND pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0x0D,0x01,sizeof(pMsg));

	pMsg.index = (WORD)aIndex;

	memcpy(pMsg.account,lpObj->Account,(sizeof(pMsg.account)-1));

	memcpy(pMsg.name,lpObj->Name,(sizeof(pMsg.name)-1));

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CMasterPage::DGPagesRecv(SDHP_MASTER_PAGES_RECV* lpMsg,int size) // OK
{
	if(size != (int)sizeof(SDHP_MASTER_PAGES_RECV))
	{
		LogAdd(LOG_RED,"[MasterPage] Bad pages packet size %d (want %d)",size,(int)sizeof(SDHP_MASTER_PAGES_RECV));
		return;
	}

	lpMsg->account[sizeof(lpMsg->account)-1] = 0;
	lpMsg->name[sizeof(lpMsg->name)-1] = 0;

	if(gObjIsAccountValid(lpMsg->index,lpMsg->account) == 0)
	{
		LogAdd(LOG_RED,"[MasterPage] Pages for an unknown account [%d](%s)",lpMsg->index,lpMsg->account);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	if(lpObj->Type != OBJECT_USER || lpObj->MasterPages == 0 || strcmp(lpObj->Name,lpMsg->name) != 0)
	{
		return;
	}

	if(gServerInfo.m_MasterSkillTree == 0 || gServerInfo.m_MasterPageSwitch == 0 || lpObj->LoadMasterPages != 0)
	{
		return;
	}

	this->m_lock.lock();

	MASTER_PAGES_DATA* lpPages = lpObj->MasterPages;

	int unlocked = lpMsg->Unlocked;

	unlocked = ((unlocked < 1) ? 1 : unlocked);

	unlocked = ((unlocked > MASTER_PAGE_COUNT) ? MASTER_PAGE_COUNT : unlocked);

	int active = lpMsg->Active;

	active = (((active < 1) || (active > unlocked)) ? 1 : active);

	lpPages->Active = (BYTE)active;

	lpPages->Unlocked = (BYTE)unlocked;

	lpPages->LastSwitchTick = 0;

	lpPages->LastRenameTick = 0;

	lpPages->Dirty = 0;

	for(int n=0;n < MASTER_PAGE_COUNT;n++)
	{
		if(MasterPageCleanName(lpMsg->Page[n].PageName,sizeof(lpMsg->Page[n].PageName),lpPages->Name[n]) == 0)
		{
			memset(lpPages->Name[n],0,sizeof(lpPages->Name[n]));

			MasterPageDefaultName(lpPages->Name[n],(n+1));
		}

		memcpy(lpPages->Skill[n],lpMsg->Page[n].Skill,sizeof(lpPages->Skill[n]));
	}

	lpObj->LoadMasterPages = 1;

	this->m_lock.unlock();

	LogAdd(LOG_BLACK,"[MasterPage][%s][%s] Loaded (active %d, unlocked %d)",lpObj->Account,lpObj->Name,active,unlocked);

	this->OnTreeLoaded(lpObj);
}

void CMasterPage::GDPagesSaveSend(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	// Never before the pages were read back, or the defaults would overwrite them.
	if(this->IsReady(lpObj) == 0)
	{
		return;
	}

	SDHP_MASTER_PAGES_SAVE_SEND pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0x0D,0x31,sizeof(pMsg));

	pMsg.index = (WORD)aIndex;

	memcpy(pMsg.account,lpObj->Account,(sizeof(pMsg.account)-1));

	memcpy(pMsg.name,lpObj->Name,(sizeof(pMsg.name)-1));

	this->m_lock.lock();

	MASTER_PAGES_DATA* lpPages = lpObj->MasterPages;

	// Every character save comes through here: only send what changed.
	if(lpPages->Dirty == 0)
	{
		this->m_lock.unlock();
		return;
	}

	lpPages->Dirty = 0;

	pMsg.Active = lpPages->Active;

	pMsg.Unlocked = lpPages->Unlocked;

	for(int n=0;n < MASTER_PAGE_COUNT;n++)
	{
		memcpy(pMsg.Page[n].PageName,lpPages->Name[n],(sizeof(pMsg.Page[n].PageName)-1));

		memcpy(pMsg.Page[n].Skill,lpPages->Skill[n],sizeof(pMsg.Page[n].Skill));
	}

	// The live page is the one MasterSkillTree holds; keep its slot in step too
	// so the row never looks stale to anyone reading the table.
	for(int n=0;n < MAX_MASTER_SKILL_LIST;n++)
	{
		gSkillManager.SkillByteConvert(&pMsg.Page[(lpPages->Active-1)].Skill[(n*3)],&lpObj->MasterSkill[n]);
	}

	// Sent under the lock so a change landing meanwhile cannot have its flag cleared.
	if(gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg)) == 0)
	{
		lpPages->Dirty = 1;
	}

	this->m_lock.unlock();
}

//**********************************************//
//******************* Client *******************//
//**********************************************//

void CMasterPage::GCResultSend(int aIndex,BYTE code,BYTE p1) // OK
{
	PMSG_MASTER_PAGE_RESULT_SEND pMsg;

	pMsg.header.set(0xD3,0xC3,sizeof(pMsg));

	pMsg.Code = code;

	pMsg.P1 = p1;

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CMasterPage::GCPagesInfoSend(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(this->IsReady(lpObj) == 0)
	{
		return;
	}

	PMSG_MASTER_PAGES_INFO_SEND pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0xD3,0xC0,sizeof(pMsg));

	this->m_lock.lock();

	MASTER_PAGES_DATA* lpPages = lpObj->MasterPages;

	pMsg.Active = lpPages->Active;

	pMsg.Unlocked = lpPages->Unlocked;

	pMsg.Count = (BYTE)this->GetMaxCount();

	const int live = this->GetSpentLive(lpObj);

	int total = lpObj->MasterPoint+live;

	total = ((total < 0) ? 0 : ((total > 0xFFFF) ? 0xFFFF : total));

	pMsg.Total = (WORD)total;

	for(int n=0;n < MASTER_PAGE_COUNT;n++)
	{
		memcpy(pMsg.Names[n],lpPages->Name[n],(sizeof(pMsg.Names[n])-1));

		const int used = (((n+1) == lpPages->Active) ? live : this->DecodeBlob(lpObj,lpPages->Skill[n],0,0));

		pMsg.Used[n] = (WORD)((used > 0xFFFF) ? 0xFFFF : used);
	}

	this->m_lock.unlock();

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CMasterPage::GCSnapshotSend(int aIndex,int page,int used,std::vector<MASTER_PAGE_ENTRY>* lpList) // OK
{
	BYTE send[256];

	PMSG_MASTER_PAGE_SNAPSHOT_SEND pMsg;

	int size = sizeof(pMsg);

	pMsg.Page = (BYTE)page;

	pMsg.Used = (WORD)((used > 0xFFFF) ? 0xFFFF : used);

	pMsg.Count = 0;

	for(size_t n=0;n < lpList->size() && pMsg.Count < MAX_MASTER_SKILL_LIST;n++)
	{
		PMSG_MASTER_PAGE_SNAPSHOT_ENTRY entry;

		entry.Skill = (*lpList)[n].ClientSkill;

		entry.Level = (BYTE)((*lpList)[n].Level+1);

		memcpy(&send[size],&entry,sizeof(entry));

		size += sizeof(entry);

		pMsg.Count++;
	}

	pMsg.header.set(0xD3,0xC2,size);

	memcpy(send,&pMsg,sizeof(pMsg));

	DataSend(aIndex,send,size);
}

void CMasterPage::CGPagesReqRecv(int aIndex) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	// An old server never answers; neither does one with the feature off.
	this->GCPagesInfoSend(aIndex);
}

void CMasterPage::ViewParkedPage(int aIndex,int page) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	std::vector<MASTER_PAGE_ENTRY> list;

	this->m_lock.lock();

	const int used = this->DecodeBlob(lpObj,lpObj->MasterPages->Skill[page-1],&list,0);

	this->m_lock.unlock();

	this->GCSnapshotSend(aIndex,page,used,&list);
}

void CMasterPage::CGPageViewRecv(PMSG_MASTER_PAGE_SELECT_RECV* lpMsg,int aIndex) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(this->IsReady(lpObj) == 0)
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_ERROR,0);
		return;
	}

	const int page = lpMsg->Page;

	if(page < 1 || page > MASTER_PAGE_COUNT)
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_ERROR,0);
		return;
	}

	if(page > lpObj->MasterPages->Unlocked || page > this->GetMaxCount())
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_LOCKED,0);
		return;
	}

	if(page == lpObj->MasterPages->Active)
	{
		// The live page: the client's tree is already right, a fresh F3:53 puts it back.
		gMasterSkillTree.GCMasterSkillListSend(aIndex);
		return;
	}

	this->ViewParkedPage(aIndex,page);
}

// Why the player cannot switch right now (0 = nothing in the way).
int CMasterPage::GetBusyReason(LPOBJ lpObj) // OK
{
	if(lpObj->Live == 0 || lpObj->DieRegen != 0 || lpObj->Teleport != 0 || lpObj->State == OBJECT_DELCMD)
	{
		return 1;
	}

	if(lpObj->Interface.use != 0 || lpObj->PShopOpen != 0 || lpObj->Transaction == 1 || lpObj->MapServerMoveQuit != 0)
	{
		return 2;
	}

	if(lpObj->DuelUser >= 0 || gDuelRanked.IsParticipant(lpObj->Name) != 0)
	{
		return 3;
	}

	if(DS_MAP_RANGE(lpObj->Map) != 0 || BC_MAP_RANGE(lpObj->Map) != 0 || CC_MAP_RANGE(lpObj->Map) != 0 || IT_MAP_RANGE(lpObj->Map) != 0 || DA_MAP_RANGE(lpObj->Map) != 0 || DG_MAP_RANGE(lpObj->Map) != 0 || IG_MAP_RANGE(lpObj->Map) != 0)
	{
		return 4;
	}

	return 0;
}

void CMasterPage::CGPageActivateRecv(PMSG_MASTER_PAGE_SELECT_RECV* lpMsg,int aIndex) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	this->ActivatePage(aIndex,lpMsg->Page);
}

// Makes 'page' the live one. Gates in the order the client reports them; the
// rewrite itself runs under the lock so a save never copies a half done switch.
bool CMasterPage::ActivatePage(int aIndex,int page) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(this->IsReady(lpObj) == 0 || page < 1 || page > MASTER_PAGE_COUNT)
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_ERROR,0);
		return 0;
	}

	MASTER_PAGES_DATA* lpPages = lpObj->MasterPages;

	if(page > lpPages->Unlocked || page > this->GetMaxCount())
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_LOCKED,0);
		return 0;
	}

	if(page == lpPages->Active)
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_ALREADY_ACTIVE,0);
		return 0;
	}

	if(this->GetBusyReason(lpObj) != 0)
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_BUSY,0);
		return 0;
	}

	if(MAP_RANGE(lpObj->Map) == 0 || gMap[lpObj->Map].CheckAttr(lpObj->X,lpObj->Y,1) == 0)
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_NOT_SAFE_ZONE,0);
		return 0;
	}

	const DWORD delay = (DWORD)((gServerInfo.m_MasterPageSwitchDelaySec < 0) ? 0 : gServerInfo.m_MasterPageSwitchDelaySec)*1000;

	if(lpPages->LastSwitchTick != 0 && (GetTickCount()-lpPages->LastSwitchTick) < delay)
	{
		const DWORD left = (delay-(GetTickCount()-lpPages->LastSwitchTick)+999)/1000;

		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_COOLDOWN,(BYTE)((left > 255) ? 255 : left));
		return 0;
	}

	this->m_lock.lock();

	// Things may have moved while the lock was taken.
	if(this->IsReady(lpObj) == 0 || page == lpPages->Active || page > lpPages->Unlocked)
	{
		this->m_lock.unlock();
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_ERROR,0);
		return 0;
	}

	std::vector<MASTER_PAGE_ENTRY> list;

	int dropped = 0;

	const int spentTarget = this->DecodeBlob(lpObj,lpPages->Skill[page-1],&list,&dropped);

	const int spentLive = this->GetSpentLive(lpObj);

	const int earned = lpObj->MasterPoint+spentLive;

	if(spentTarget > earned)
	{
		this->m_lock.unlock();
		LogAdd(LOG_RED,"[MasterPage][%s][%s] Page %d costs %d but only %d were earned, switch refused",lpObj->Account,lpObj->Name,page,spentTarget,earned);
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_ERROR,0);
		return 0;
	}

	const int oldPage = lpPages->Active;

	// 1. Park the live page, in the same format the DataServer holds.
	for(int n=0;n < MAX_MASTER_SKILL_LIST;n++)
	{
		gSkillManager.SkillByteConvert(&lpPages->Skill[(oldPage-1)][(n*3)],&lpObj->MasterSkill[n]);
	}

	// 2. Back to the base skills, MasterPoint untouched.
	gMasterSkillTree.RestoreBaseSkills(lpObj);

	// 3. Learn the target page in rank order. The data was valid when it was
	// saved, so the prerequisite checks are skipped; the same calls
	// CGMasterSkillRecv makes do the work.
	int spentApplied = 0;

	for(size_t n=0;n < list.size();n++)
	{
		MASTER_SKILL_TREE_INFO info;

		if(gMasterSkillTree.GetInfo(list[n].Index,&info) == 0)
		{
			dropped++;
			continue;
		}

		const int first = info.MinLevel-1;

		// A skill that replaces a base one only finds it while the weapon is in
		// hand; the master skill is kept either way, like a login does.
		gMasterSkillTree.ReplaceMasterSkill(lpObj,info.ReplaceSkill,info.Index,first);

		if(gSkillManager.AddMasterSkill(lpObj,info.Index,first) < 0)
		{
			LogAdd(LOG_RED,"[MasterPage][%s][%s] Skill %d could not be learned, dropped",lpObj->Account,lpObj->Name,info.Index);
			dropped++;
			continue;
		}

		if(list[n].Level > first)
		{
			CSkill* lpMasterSkill = gSkillManager.GetMasterSkill(lpObj,info.Index);

			if(lpMasterSkill != 0)
			{
				// CGMasterSkillRecv refreshes the skill list slot with the level the
				// skill had BEFORE the new point, so the slot trails by one; the same
				// here keeps a switched page identical to one that was bought.
				gMasterSkillTree.ReplaceMasterSkill(lpObj,info.Index,info.Index,(list[n].Level-1));

				lpMasterSkill->Set(info.Index,list[n].Level);
			}
		}

		spentApplied += list[n].Points;
	}

	// 4. Only the difference moves, so a level up landing meanwhile is not lost.
	lpObj->MasterPoint += (spentLive-spentApplied);

	if(lpObj->MasterPoint < 0)
	{
		lpObj->MasterPoint = 0; // cannot happen: spentApplied <= spentTarget <= earned
	}

	lpPages->Active = (BYTE)page;

	lpPages->LastSwitchTick = GetTickCount();

	lpPages->Dirty = 1;

	this->m_lock.unlock();

	// Derived stats only: Skill[], MasterSkill[], MasterPoint and Active already
	// agree, so a save that runs from here on copies a consistent switch. Kept out
	// of the lock because the recalculation sends packets and reaches many systems.
	gObjectManager.CharacterCalcAttribute(aIndex);

	LogAdd(LOG_BLACK,"[MasterPage][%s][%s] Page %d -> %d (spent %d -> %d, points left %d, dropped %d)",lpObj->Account,lpObj->Name,oldPage,page,spentLive,spentApplied,lpObj->MasterPoint,dropped);

	gMasterSkillTree.GCMasterInfoSend(lpObj);

	gSkillManager.GCSkillListSend(lpObj,0);

	gMasterSkillTree.GCMasterSkillListSend(aIndex);

	this->GCPagesInfoSend(aIndex);

	this->GCResultSend(aIndex,MASTER_PAGE_RESULT_OK,0);

	if(dropped != 0)
	{
		gNotice.GCNoticeSend(aIndex,1,0,0,0,0,0,"[Master] %d skill(s) of this page no longer exist and were refunded.",dropped);
	}

	// So a crash cannot take the switch back.
	GDCharacterInfoSaveSend(aIndex);

	if(gServerInfo.m_MasterPageRelogOnSwitch != 0)
	{
		lpObj->CloseCount = gServerInfo.m_ThoiGianThoatGame;

		lpObj->CloseType = 1;
	}

	return 1;
}

void CMasterPage::CGPageRenameRecv(PMSG_MASTER_PAGE_RENAME_RECV* lpMsg,int aIndex) // OK
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(this->IsReady(lpObj) == 0)
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_ERROR,0);
		return;
	}

	const int page = lpMsg->Page;

	if(page < 1 || page > MASTER_PAGE_COUNT)
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_ERROR,0);
		return;
	}

	MASTER_PAGES_DATA* lpPages = lpObj->MasterPages;

	if(page > lpPages->Unlocked || page > this->GetMaxCount())
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_LOCKED,0);
		return;
	}

	char name[MASTER_PAGE_NAME_SIZE];

	if(MasterPageCleanName(lpMsg->Name,sizeof(lpMsg->Name),name) == 0)
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_BAD_NAME,0);
		return;
	}

	// One a second is plenty for a text box; it also keeps a hostile client from
	// turning every rename into a DataServer write.
	if(lpPages->LastRenameTick != 0 && (GetTickCount()-lpPages->LastRenameTick) < 1000)
	{
		this->GCResultSend(aIndex,MASTER_PAGE_RESULT_COOLDOWN,1);
		return;
	}

	this->m_lock.lock();

	memcpy(lpPages->Name[page-1],name,MASTER_PAGE_NAME_SIZE);

	lpPages->LastRenameTick = GetTickCount();

	lpPages->Dirty = 1;

	this->m_lock.unlock();

	this->GCPagesInfoSend(aIndex);

	this->GCResultSend(aIndex,MASTER_PAGE_RESULT_OK,0);

	// Only the pages changed, so only they are written.
	this->GDPagesSaveSend(aIndex);
}

//**********************************************//
//************** Master Page Scroll ************//
//**********************************************//

bool CMasterPage::CheckScroll(LPOBJ lpObj) // OK
{
	if(this->IsReady(lpObj) == 0)
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Master pages are not available right now.");
		return 0;
	}

	if(lpObj->MasterPages->Unlocked >= this->GetMaxCount())
	{
		gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"All master pages are already unlocked.");
		return 0;
	}

	return 1;
}

void CMasterPage::ApplyScroll(LPOBJ lpObj) // OK
{
	this->m_lock.lock();

	if(lpObj->MasterPages->Unlocked < this->GetMaxCount())
	{
		lpObj->MasterPages->Unlocked++;

		lpObj->MasterPages->Dirty = 1;
	}

	const int unlocked = lpObj->MasterPages->Unlocked;

	this->m_lock.unlock();

	LogAdd(LOG_BLACK,"[MasterPage][%s][%s] Master Page Scroll used, %d page(s) open",lpObj->Account,lpObj->Name,unlocked);

	gNotice.GCNoticeSend(lpObj->Index,1,0,0,0,0,0,"Master page %d unlocked.",unlocked);

	this->GCPagesInfoSend(lpObj->Index);

	GDCharacterInfoSaveSend(lpObj->Index);
}

#else

// Master pages need the 6.02+ skill tree: everything is off.

void CMasterPage::Reset(LPOBJ lpObj){lpObj->LoadMasterPages = 0;}
bool CMasterPage::IsReady(LPOBJ lpObj){return 0;}
int CMasterPage::GetEarned(LPOBJ lpObj){return lpObj->MasterPoint;}
void CMasterPage::GDPagesLoadSend(int aIndex){}
void CMasterPage::DGPagesRecv(SDHP_MASTER_PAGES_RECV* lpMsg,int size){}
void CMasterPage::GDPagesSaveSend(int aIndex){}
void CMasterPage::OnTreeLoaded(LPOBJ lpObj){}
void CMasterPage::CGPagesReqRecv(int aIndex){}
void CMasterPage::CGPageViewRecv(PMSG_MASTER_PAGE_SELECT_RECV* lpMsg,int aIndex){}
void CMasterPage::CGPageActivateRecv(PMSG_MASTER_PAGE_SELECT_RECV* lpMsg,int aIndex){}
void CMasterPage::CGPageRenameRecv(PMSG_MASTER_PAGE_RENAME_RECV* lpMsg,int aIndex){}
bool CMasterPage::CheckScroll(LPOBJ lpObj){return 0;}
void CMasterPage::ApplyScroll(LPOBJ lpObj){}
void CMasterPage::OnFullWipe(LPOBJ lpObj){}
void CMasterPage::GCPagesInfoSend(int aIndex){}

#endif
