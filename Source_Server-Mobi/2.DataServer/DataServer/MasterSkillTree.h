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
//************* Master skill pages *************//
//**********************************************//

// Mirrored byte for byte in the GameServer's MasterPage.h. Page 1..5 as the
// player sees them; the ACTIVE page's real skills stay in MasterSkillTree, the
// blob stored for it here is only kept in step by the GameServer.
#define MASTER_PAGE_COUNT 5
#define MASTER_PAGE_NAME_SIZE 13 // 12 characters + NUL
#define MASTER_PAGE_BLOB_SIZE (MAX_MASTER_SKILL_LIST*3)

#pragma pack(push,1)

struct SDHP_MASTER_PAGE_DATA
{
	char PageName[MASTER_PAGE_NAME_SIZE];
	BYTE Skill[MASTER_PAGE_BLOB_SIZE];
};

struct SDHP_MASTER_PAGES_REQ_RECV
{
	PSBMSG_HEAD header; // C1:0D:01 GameServer -> DataServer
	WORD index;
	char account[11];
	char name[11];
};

struct SDHP_MASTER_PAGES_SAVE_RECV
{
	PSWMSG_HEAD header; // C2:0D:31 GameServer -> DataServer
	WORD index;
	char account[11];
	char name[11];
	BYTE Active;
	BYTE Unlocked;
	SDHP_MASTER_PAGE_DATA Page[MASTER_PAGE_COUNT];
};

struct SDHP_MASTER_PAGES_SEND
{
	PSWMSG_HEAD header; // C2:0D:01 DataServer -> GameServer
	WORD index;
	char account[11];
	char name[11];
	BYTE Active;
	BYTE Unlocked;
	SDHP_MASTER_PAGE_DATA Page[MASTER_PAGE_COUNT];
};

#pragma pack(pop)

static_assert(sizeof(SDHP_MASTER_PAGE_DATA) == 13 + 360, "SDHP_MASTER_PAGE_DATA must match the GameServer's copy");
static_assert(sizeof(SDHP_MASTER_PAGES_REQ_RECV) == 4 + 2 + 11 + 11, "SDHP_MASTER_PAGES_REQ_RECV must match the GameServer's copy");
static_assert(sizeof(SDHP_MASTER_PAGES_SAVE_RECV) == 5 + 2 + 11 + 11 + 2 + 5 * (13 + 360), "SDHP_MASTER_PAGES_SAVE_RECV must match the GameServer's copy");
static_assert(sizeof(SDHP_MASTER_PAGES_SEND) == sizeof(SDHP_MASTER_PAGES_SAVE_RECV), "SDHP_MASTER_PAGES_SEND must match the GameServer's copy");

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
	// Master skill pages (MasterSkillPageInfo / MasterSkillPage, MasterSkillPage_Schema.sql).
	// A missing table makes the load go unanswered, which keeps pages off for the session.
	void GDMasterPagesRecv(SDHP_MASTER_PAGES_REQ_RECV* lpMsg,int index);
	void GDMasterPagesSaveRecv(SDHP_MASTER_PAGES_SAVE_RECV* lpMsg,int size);
	// The stored procedures that create, delete and rename a character do not know the page
	// tables: these keep the rows in step (name reuse must not inherit unlocked pages).
	void PagesDelete(const char* name);
	void PagesRename(const char* oldName,const char* newName);
};


extern CMasterSkillTree gMasterSkillTree;
