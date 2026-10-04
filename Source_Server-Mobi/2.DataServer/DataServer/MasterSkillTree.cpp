// MasterSkillTree.cpp: implementation of the CMasterSkillTree class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MasterSkillTree.h"
#include "QueryManager.h"
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
