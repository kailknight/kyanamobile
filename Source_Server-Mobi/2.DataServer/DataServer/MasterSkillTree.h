// MasterSkillTree.h: interface for the CMasterSkillTree class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "DataServerProtocol.h"

#define MAX_MASTER_SKILL_LIST 120

//**********************************************//
//********** GameServer -> DataServer **********//
//**********************************************//

struct SDHP_MASTER_SKILL_TREE_RECV
{
	PSBMSG_HEAD header; // C1:0D:00
	WORD index;
	char account[11];
	char name[11];
};

struct SDHP_MASTER_SKILL_TREE_SAVE_RECV
{
	PSWMSG_HEAD header; // C2:0D:30
	WORD index;
	char account[11];
	char name[11];
	DWORD MasterLevel;
	DWORD MasterPoint;
	QWORD MasterExperience;
	#if(DATASERVER_UPDATE>=602)
	BYTE MasterSkill[MAX_MASTER_SKILL_LIST][3];
	#endif
};

//**********************************************//
//********** DataServer -> GameServer **********//
//**********************************************//

struct SDHP_MASTER_SKILL_TREE_SEND
{
	PSWMSG_HEAD header; // C2:0D:00
	WORD index;
	char account[11];
	char name[11];
	DWORD MasterLevel;
	DWORD MasterPoint;
	QWORD MasterExperience;
	#if(DATASERVER_UPDATE>=602)
	BYTE MasterSkill[MAX_MASTER_SKILL_LIST][3];
	#endif
};

//**********************************************//
//******* Master skill recommendations *********//
//**********************************************//

// What the strongest characters of each class put their master points into,
// for the "pick this next" highlight in the client's skill tree. The GameServer
// asks (C1:D9:36) and gets one answer per base class.
#define MASTER_RECOMMEND_CLASSES 7   // DW, DK, Elf, MG, DL, Summoner, RF
#define MASTER_RECOMMEND_SAMPLE 20   // top characters of a class, by Master Level
#define MASTER_RECOMMEND_MAX 40      // most popular skills sent per class

struct SDHP_MASTER_RECOMMEND_RECV
{
	PSBMSG_HEAD header; // C1:D9:36
};

struct SDHP_MASTER_RECOMMEND_SEND
{
	PSBMSG_HEAD header; // C1:D9:36
	BYTE Class;         // base class 0-6
	BYTE Sample;        // characters counted
	BYTE Count;         // entries used
	BYTE Entry[MASTER_RECOMMEND_MAX][4]; // skill low, skill high, picks, level most of them took it to - most popular first
};

//**********************************************//
//**********************************************//
//**********************************************//

class CMasterSkillTree
{
public:
	CMasterSkillTree();
	virtual ~CMasterSkillTree();
	void GDMasterSkillTreeRecv(SDHP_MASTER_SKILL_TREE_RECV* lpMsg,int index);
	void GDMasterSkillTreeSaveRecv(SDHP_MASTER_SKILL_TREE_SAVE_RECV* lpMsg);
	void GDMasterRecommendRecv(SDHP_MASTER_RECOMMEND_RECV* lpMsg,int index);
};


extern CMasterSkillTree gMasterSkillTree;
