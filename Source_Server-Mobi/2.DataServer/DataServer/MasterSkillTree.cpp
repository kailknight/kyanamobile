// MasterSkillTree.cpp: implementation of the CMasterSkillTree class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MasterSkillTree.h"
#include "QueryManager.h"
#include "Util.h"
#include "SocketManager.h"
#include <algorithm>
#include <map>
#include <vector>

CMasterSkillTree gMasterSkillTree;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMasterSkillTree::CMasterSkillTree() // OK
{

}

CMasterSkillTree::~CMasterSkillTree() // OK
{

}

void CMasterSkillTree::GDMasterSkillTreeRecv(SDHP_MASTER_SKILL_TREE_RECV* lpMsg,int index) // OK
{
	#if(DATASERVER_UPDATE>=401)

	SDHP_MASTER_SKILL_TREE_SEND pMsg;

	pMsg.header.set(0x0D, 0x00, sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	memcpy(pMsg.name,lpMsg->name,sizeof(pMsg.name));

	if(gQueryManager.ExecQuery("SELECT * FROM MasterSkillTree WHERE Name='%s'",lpMsg->name) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		pMsg.MasterLevel = 0;

		pMsg.MasterPoint = 0;

		pMsg.MasterExperience = 0;

		#if(DATASERVER_UPDATE>=602)

		memset(pMsg.MasterSkill, 0xFF, sizeof(pMsg.MasterSkill));

		#endif
	}
	else
	{
		pMsg.MasterLevel = gQueryManager.GetAsInteger("MasterLevel");

		pMsg.MasterPoint = gQueryManager.GetAsInteger("MasterPoint");

		pMsg.MasterExperience = gQueryManager.GetAsInteger64("MasterExperience");

		#if(DATASERVER_UPDATE>=602)

		gQueryManager.GetAsBinary("MasterSkill",pMsg.MasterSkill[0],sizeof(pMsg.MasterSkill));

		#endif

		gQueryManager.Close();
	}

	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

void CMasterSkillTree::GDMasterSkillTreeSaveRecv(SDHP_MASTER_SKILL_TREE_SAVE_RECV* lpMsg) // OK
{
	#if(DATASERVER_UPDATE>=401)

	if(gQueryManager.ExecQuery("SELECT Name FROM MasterSkillTree WHERE Name='%s'",lpMsg->name) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		#if(DATASERVER_UPDATE>=602)
		gQueryManager.Close();
		gQueryManager.BindParameterAsBinary(1,lpMsg->MasterSkill[0],sizeof(lpMsg->MasterSkill));
		gQueryManager.ExecQuery("INSERT INTO MasterSkillTree (Name,MasterLevel,MasterPoint,MasterExperience,MasterSkill) VALUES ('%s',%d,%d,%I64d,?)",lpMsg->name,lpMsg->MasterLevel,lpMsg->MasterPoint,lpMsg->MasterExperience);
		gQueryManager.Close();
		#else
		gQueryManager.Close();
		gQueryManager.ExecQuery("INSERT INTO MasterSkillTree (Name,MasterLevel,MasterPoint,MasterExperience) VALUES ('%s',%d,%d,%I64d)",lpMsg->name,lpMsg->MasterLevel,lpMsg->MasterPoint,lpMsg->MasterExperience);
		gQueryManager.Close();
		#endif
	}
	else
	{
		#if(DATASERVER_UPDATE>=602)
		gQueryManager.Close();
		gQueryManager.BindParameterAsBinary(1,lpMsg->MasterSkill[0],sizeof(lpMsg->MasterSkill));
		gQueryManager.ExecQuery("UPDATE MasterSkillTree SET MasterLevel=%d,MasterPoint=%d,MasterExperience=%I64d,MasterSkill=? WHERE Name='%s'",lpMsg->MasterLevel,lpMsg->MasterPoint,lpMsg->MasterExperience,lpMsg->name);
		gQueryManager.Close();
		#else
		gQueryManager.Close();
		gQueryManager.ExecQuery("UPDATE MasterSkillTree SET MasterLevel=%d,MasterPoint=%d,MasterExperience=%I64d WHERE Name='%s'",lpMsg->MasterLevel,lpMsg->MasterPoint,lpMsg->MasterExperience,lpMsg->name);
		gQueryManager.Close();
		#endif
	}

	#endif
}

//**********************************************//
//************* Master skill pages *************//
//**********************************************//

// Page names that reach SQL are bound parameters, never part of the query text.
// A blob is the same 3 byte format as MasterSkillTree.MasterSkill (FF 00 FF = empty).
static void MasterPageFillDefault(SDHP_MASTER_PAGE_DATA* lpPage,int page) // OK
{
	memset(lpPage,0,sizeof(SDHP_MASTER_PAGE_DATA));

	sprintf_s(lpPage->PageName,sizeof(lpPage->PageName),"Page %d",page);

	for(int n=0;n < MAX_MASTER_SKILL_LIST;n++)
	{
		lpPage->Skill[(n*3)+0] = 0xFF;
		lpPage->Skill[(n*3)+1] = 0x00;
		lpPage->Skill[(n*3)+2] = 0xFF;
	}
}

void CMasterSkillTree::GDMasterPagesRecv(SDHP_MASTER_PAGES_REQ_RECV* lpMsg,int index) // OK
{
	SDHP_MASTER_PAGES_SEND pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0x0D,0x01,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	memcpy(pMsg.name,lpMsg->name,sizeof(pMsg.name));

	pMsg.name[sizeof(pMsg.name)-1] = 0;

	pMsg.Active = 1;

	pMsg.Unlocked = 1;

	for(int n=0;n < MASTER_PAGE_COUNT;n++)
	{
		MasterPageFillDefault(&pMsg.Page[n],(n+1));
	}

	if(CheckTextSyntax(pMsg.name,sizeof(pMsg.name)) == 0)
	{
		return;
	}

	// The info row. Failing to ask at all (the table is missing) is NOT the same
	// as having no row: it stays unanswered, so the GameServer keeps pages off for
	// this session instead of loading defaults it would later save over real data.
	bool asked = 0;

	gQueryManager.BindParameterAsString(1,pMsg.name,sizeof(pMsg.name));

	if(gQueryManager.ExecQuery("SELECT ActivePage,UnlockedPages FROM MasterSkillPageInfo WHERE Name=?") != 0)
	{
		asked = 1;

		if(gQueryManager.Fetch() != SQL_NO_DATA)
		{
			pMsg.Active = (BYTE)gQueryManager.GetAsInteger("ActivePage");

			pMsg.Unlocked = (BYTE)gQueryManager.GetAsInteger("UnlockedPages");
		}
	}

	gQueryManager.Close();

	if(asked == 0)
	{
		LogAdd(LOG_RED,"[MasterPage] %s: MasterSkillPageInfo could not be read (run MasterSkillPage_Schema.sql), pages stay off",pMsg.name);
		return;
	}

	if(pMsg.Unlocked < 1 || pMsg.Unlocked > MASTER_PAGE_COUNT)
	{
		pMsg.Unlocked = 1;
	}

	if(pMsg.Active < 1 || pMsg.Active > pMsg.Unlocked)
	{
		pMsg.Active = 1;
	}

	gQueryManager.BindParameterAsString(1,pMsg.name,sizeof(pMsg.name));

	if(gQueryManager.ExecQuery("SELECT Page,PageName,MasterSkill FROM MasterSkillPage WHERE Name=?") == 0)
	{
		gQueryManager.Close();

		LogAdd(LOG_RED,"[MasterPage] %s: MasterSkillPage could not be read, pages stay off",pMsg.name);
		return;
	}

	while(gQueryManager.Fetch() != SQL_NO_DATA)
	{
		const int page = gQueryManager.GetAsInteger("Page");

		if(page < 1 || page > MASTER_PAGE_COUNT)
		{
			continue;
		}

		SDHP_MASTER_PAGE_DATA* lpPage = &pMsg.Page[(page-1)];

		char PageName[MASTER_PAGE_NAME_SIZE] = {0};

		gQueryManager.GetAsString("PageName",PageName,sizeof(PageName));

		if(PageName[0] != 0)
		{
			memcpy(lpPage->PageName,PageName,sizeof(lpPage->PageName));
		}

		BYTE Skill[MASTER_PAGE_BLOB_SIZE];

		memset(Skill,0xFF,sizeof(Skill));

		gQueryManager.GetAsBinary("MasterSkill",Skill,sizeof(Skill));

		memcpy(lpPage->Skill,Skill,sizeof(lpPage->Skill));
	}

	gQueryManager.Close();

	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));
}

void CMasterSkillTree::GDMasterPagesSaveRecv(SDHP_MASTER_PAGES_SAVE_RECV* lpMsg,int size) // OK
{
	if(size != (int)sizeof(SDHP_MASTER_PAGES_SAVE_RECV))
	{
		LogAdd(LOG_RED,"[MasterPage] Bad save packet size %d (want %d)",size,(int)sizeof(SDHP_MASTER_PAGES_SAVE_RECV));
		return;
	}

	lpMsg->name[sizeof(lpMsg->name)-1] = 0;

	if(lpMsg->name[0] == 0 || CheckTextSyntax(lpMsg->name,sizeof(lpMsg->name)) == 0)
	{
		return;
	}

	int unlocked = lpMsg->Unlocked;

	unlocked = ((unlocked < 1) ? 1 : ((unlocked > MASTER_PAGE_COUNT) ? MASTER_PAGE_COUNT : unlocked));

	int active = lpMsg->Active;

	active = (((active < 1) || (active > unlocked)) ? 1 : active);

	// Upsert the info row. UPDATE first, INSERT when nothing matched.
	gQueryManager.BindParameterAsString(1,lpMsg->name,sizeof(lpMsg->name));

	gQueryManager.ExecQuery("UPDATE MasterSkillPageInfo SET ActivePage=%d,UnlockedPages=%d WHERE Name=?",active,unlocked);

	gQueryManager.Close();

	gQueryManager.BindParameterAsString(1,lpMsg->name,sizeof(lpMsg->name));

	gQueryManager.BindParameterAsString(2,lpMsg->name,sizeof(lpMsg->name));

	gQueryManager.ExecQuery("INSERT INTO MasterSkillPageInfo (Name,ActivePage,UnlockedPages) SELECT ?,%d,%d WHERE NOT EXISTS (SELECT 1 FROM MasterSkillPageInfo WHERE Name=?)",active,unlocked);

	gQueryManager.Close();

	for(int n=0;n < MASTER_PAGE_COUNT;n++)
	{
		char PageName[MASTER_PAGE_NAME_SIZE];

		memcpy(PageName,lpMsg->Page[n].PageName,sizeof(PageName));

		PageName[sizeof(PageName)-1] = 0;

		gQueryManager.BindParameterAsString(1,PageName,sizeof(PageName));

		gQueryManager.BindParameterAsBinary(2,lpMsg->Page[n].Skill,sizeof(lpMsg->Page[n].Skill));

		gQueryManager.BindParameterAsString(3,lpMsg->name,sizeof(lpMsg->name));

		gQueryManager.ExecQuery("UPDATE MasterSkillPage SET PageName=?,MasterSkill=? WHERE Name=? AND Page=%d",(n+1));

		gQueryManager.Close();

		gQueryManager.BindParameterAsString(1,lpMsg->name,sizeof(lpMsg->name));

		gQueryManager.BindParameterAsString(2,PageName,sizeof(PageName));

		gQueryManager.BindParameterAsBinary(3,lpMsg->Page[n].Skill,sizeof(lpMsg->Page[n].Skill));

		gQueryManager.BindParameterAsString(4,lpMsg->name,sizeof(lpMsg->name));

		gQueryManager.ExecQuery("INSERT INTO MasterSkillPage (Name,Page,PageName,MasterSkill) SELECT ?,%d,?,? WHERE NOT EXISTS (SELECT 1 FROM MasterSkillPage WHERE Name=? AND Page=%d)",(n+1),(n+1));

		gQueryManager.Close();
	}
}

// Drops both page tables' rows of a character name. Called when a character is deleted and
// again when one is created, so rows left by an earlier delete never reach a new owner.
void CMasterSkillTree::PagesDelete(const char* name) // OK
{
	char nm[11];

	memset(nm,0,sizeof(nm));

	strncpy_s(nm,sizeof(nm),name,(sizeof(nm)-1));

	if(nm[0] == 0 || CheckTextSyntax(nm,sizeof(nm)) == 0)
	{
		return;
	}

	gQueryManager.BindParameterAsString(1,nm,sizeof(nm));

	gQueryManager.ExecQuery("DELETE FROM MasterSkillPage WHERE Name=?");

	gQueryManager.Close();

	gQueryManager.BindParameterAsString(1,nm,sizeof(nm));

	gQueryManager.ExecQuery("DELETE FROM MasterSkillPageInfo WHERE Name=?");

	gQueryManager.Close();
}

// After WZ_RenameCharacter: the pages follow the character, but only when the rename really
// happened (the new name exists as a character and the old one no longer does).
void CMasterSkillTree::PagesRename(const char* oldName,const char* newName) // OK
{
	char from[11];
	char to[11];

	memset(from,0,sizeof(from));
	memset(to,0,sizeof(to));

	strncpy_s(from,sizeof(from),oldName,(sizeof(from)-1));
	strncpy_s(to,sizeof(to),newName,(sizeof(to)-1));

	if(from[0] == 0 || to[0] == 0 || CheckTextSyntax(from,sizeof(from)) == 0 || CheckTextSyntax(to,sizeof(to)) == 0)
	{
		return;
	}

	bool newExists = 0;
	bool oldExists = 1;

	gQueryManager.BindParameterAsString(1,to,sizeof(to));

	if(gQueryManager.ExecQuery("SELECT Name FROM Character WHERE Name=?") != 0)
	{
		newExists = (gQueryManager.Fetch() != SQL_NO_DATA);
	}

	gQueryManager.Close();

	gQueryManager.BindParameterAsString(1,from,sizeof(from));

	if(gQueryManager.ExecQuery("SELECT Name FROM Character WHERE Name=?") != 0)
	{
		oldExists = (gQueryManager.Fetch() != SQL_NO_DATA);
	}

	gQueryManager.Close();

	if(newExists == 0 || oldExists != 0)
	{
		return;
	}

	// Anything under the new name is a leftover.
	this->PagesDelete(to);

	gQueryManager.BindParameterAsString(1,to,sizeof(to));

	gQueryManager.BindParameterAsString(2,from,sizeof(from));

	gQueryManager.ExecQuery("UPDATE MasterSkillPage SET Name=? WHERE Name=?");

	gQueryManager.Close();

	gQueryManager.BindParameterAsString(1,to,sizeof(to));

	gQueryManager.BindParameterAsString(2,from,sizeof(from));

	gQueryManager.ExecQuery("UPDATE MasterSkillPageInfo SET Name=? WHERE Name=?");

	gQueryManager.Close();
}

void CMasterSkillTree::GDMasterRecommendRecv(SDHP_MASTER_RECOMMEND_RECV* lpMsg,int index) // OK
{
	for(int cls=0;cls < MASTER_RECOMMEND_CLASSES;cls++)
	{
		// skill -> how many of the sampled characters have at least one point in it,
		// and how many of them stopped at each level
		std::map<WORD,int> picks;
		std::map<WORD,std::map<int,int>> levels;
		int sample = 0;

		// The strongest characters of the class: highest Master Level, then the
		// most master experience. Only characters with points to spend count.
		if(gQueryManager.ExecQuery("SELECT TOP %d M.MasterSkill FROM MasterSkillTree M JOIN Character C ON C.Name = M.Name WHERE (C.Class / 16) = %d AND M.MasterLevel > 0 ORDER BY M.MasterLevel DESC, M.MasterExperience DESC",MASTER_RECOMMEND_SAMPLE,cls) != 0)
		{
			while(gQueryManager.Fetch() != SQL_NO_DATA)
			{
				BYTE MasterSkill[MAX_MASTER_SKILL_LIST][3];

				memset(MasterSkill,0xFF,sizeof(MasterSkill));

				gQueryManager.GetAsBinary("MasterSkill",MasterSkill[0],sizeof(MasterSkill));

				sample++;

				for(int n=0;n < MAX_MASTER_SKILL_LIST;n++)
				{
					// Same 3-byte layout as CSkillManager::SkillByteConvert on the
					// GameServer: index low, level, index high; FF..00 is empty.
					if(MasterSkill[n][0] == 0xFF && MasterSkill[n][2] == 0x00)
					{
						continue;
					}

					const WORD skill = (WORD)((MasterSkill[n][2] << 8) | MasterSkill[n][0]);

					if(skill == 0xFFFF || MasterSkill[n][1] == 0)
					{
						continue;
					}

					picks[skill]++;
					levels[skill][MasterSkill[n][1]]++;
				}
			}
		}

		gQueryManager.Close();

		std::vector<std::pair<int,WORD>> ranked;

		for(std::map<WORD,int>::iterator it=picks.begin();it != picks.end();it++)
		{
			ranked.push_back(std::make_pair(-it->second,it->first)); // most picks first, then lowest skill id
		}

		std::sort(ranked.begin(),ranked.end());

		SDHP_MASTER_RECOMMEND_SEND pMsg;

		memset(&pMsg,0,sizeof(pMsg));

		pMsg.header.set(0xD9,0x36,sizeof(pMsg));

		pMsg.Class = (BYTE)cls;
		pMsg.Sample = (BYTE)sample;

		for(int n=0;n < (int)ranked.size() && n < MASTER_RECOMMEND_MAX;n++)
		{
			pMsg.Entry[n][0] = SET_NUMBERLB(ranked[n].second);
			pMsg.Entry[n][1] = SET_NUMBERHB(ranked[n].second);
			pMsg.Entry[n][2] = (BYTE)(-ranked[n].first);

			// The level most of them took it to; a tie goes to the higher level.
			int bestLevel = 0;
			int bestCount = 0;

			std::map<int,int>& counts = levels[ranked[n].second];

			for(std::map<int,int>::iterator lt=counts.begin();lt != counts.end();lt++)
			{
				if(lt->second >= bestCount)
				{
					bestCount = lt->second;
					bestLevel = lt->first;
				}
			}

			pMsg.Entry[n][3] = (BYTE)bestLevel;
			pMsg.Count++;
		}

		gSocketManager.DataSend(index,(BYTE*)&pMsg,pMsg.header.size);
	}
}
