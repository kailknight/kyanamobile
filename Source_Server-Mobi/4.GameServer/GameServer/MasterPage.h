// MasterPage.h: Master skill tree pages, GameServer side.
//
// A character has up to MASTER_PAGE_COUNT master skill pages. Page 1 is free,
// the others are opened one at a time by the Master Page Scroll (14,493). Only
// the ACTIVE page is live: its skills sit in lpObj->Skill[] / lpObj->MasterSkill[]
// exactly as before pages existed, and it is what MasterSkillTree.MasterSkill
// holds. The other pages are parked here as the same 3 byte blob the database
// uses. Every page has its own point budget:
//
//   Earned = MasterPoint + points spent on the live page   (grows on level up)
//   a parked page's points left = Earned - points spent on its blob
//
// See Working Project\MD Files\MasterSkillPages-PLAN.md.
//
// Threads: client packets arrive on the socket queue thread, the DataServer's
// answer on the connection thread, saves on the timer thread. The page data
// and a page switch run under CMasterPage::m_lock (recursive, one for all
// users), and the character saves hold it too (CMasterPageGuard), so a save
// never copies a half done switch. It is a leaf lock: nothing that takes it is
// ever called with a socket or the DataServer connection held.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Protocol.h"
#include "CriticalSection.h"
#include "SkillManager.h"

#define MASTER_PAGE_COUNT 5
#define MASTER_PAGE_NAME_SIZE 13 // 12 characters + NUL
#define MASTER_PAGE_BLOB_SIZE (MAX_MASTER_SKILL_LIST*3) // 3 bytes per entry: index low, level, index high (FF 00 FF = empty)

// PAGE_RESULT.Code
enum eMasterPageResult
{
	MASTER_PAGE_RESULT_OK = 0,
	MASTER_PAGE_RESULT_NOT_SAFE_ZONE = 1,
	MASTER_PAGE_RESULT_LOCKED = 2,
	MASTER_PAGE_RESULT_ALREADY_ACTIVE = 3,
	MASTER_PAGE_RESULT_COOLDOWN = 4, // P1 = seconds left
	MASTER_PAGE_RESULT_BUSY = 5,
	MASTER_PAGE_RESULT_BAD_NAME = 6,
	MASTER_PAGE_RESULT_ERROR = 7, // error or feature unavailable
};

// Per user, allocated with the object slot (see CMemoryAllocatorInfo) and reset
// for every connection (gObjClearPlayerOption).
struct MASTER_PAGES_DATA
{
	BYTE Active; // 1..MASTER_PAGE_COUNT, the page that is live
	BYTE Unlocked; // 1..MASTER_PAGE_COUNT, how many pages are open
	DWORD LastSwitchTick; // 0 = never switched
	DWORD LastRenameTick; // renames are throttled to one a second
	BYTE Dirty; // 1 = the pages changed since they were loaded or last sent; only then is the 1.9 KB pages packet sent with a character save
	char Name[MASTER_PAGE_COUNT][MASTER_PAGE_NAME_SIZE];
	BYTE Skill[MASTER_PAGE_COUNT][MASTER_PAGE_BLOB_SIZE]; // the active page's entry is NOT kept up to date: the live skills are the truth
};

//**********************************************//
//************ Client -> GameServer ************//
//**********************************************//

#pragma pack(push,1)

struct PMSG_MASTER_PAGES_REQ_RECV
{
	PSBMSG_HEAD header; // C1:D3:C0
};

struct PMSG_MASTER_PAGE_SELECT_RECV
{
	PSBMSG_HEAD header; // C1:D3:C1 (view) and C1:D3:C3 (activate)
	BYTE Page; // 1..5
};

struct PMSG_MASTER_PAGE_RENAME_RECV
{
	PSBMSG_HEAD header; // C1:D3:C4
	BYTE Page; // 1..5
	char Name[MASTER_PAGE_NAME_SIZE]; // zero padded
};

//**********************************************//
//************ GameServer -> Client ************//
//**********************************************//

struct PMSG_MASTER_PAGES_INFO_SEND
{
	PSBMSG_HEAD header; // C1:D3:C0
	BYTE Active;
	BYTE Unlocked;
	BYTE Count; // pages this server offers (MasterPageMaxCount)
	char Names[MASTER_PAGE_COUNT][MASTER_PAGE_NAME_SIZE];
	WORD Total; // Earned
	WORD Used[MASTER_PAGE_COUNT]; // points spent on each page (the live one computed on the fly)
};

struct PMSG_MASTER_PAGE_SNAPSHOT_SEND
{
	PSBMSG_HEAD header; // C1:D3:C2, followed by Count x PMSG_MASTER_PAGE_SNAPSHOT_ENTRY
	BYTE Page;
	WORD Used;
	BYTE Count;
};

struct PMSG_MASTER_PAGE_SNAPSHOT_ENTRY
{
	BYTE Skill; // tree index, same meaning as in F3:53
	BYTE Level; // same meaning as in F3:53 (level + 1)
};

struct PMSG_MASTER_PAGE_RESULT_SEND
{
	PSBMSG_HEAD header; // C1:D3:C3
	BYTE Code; // eMasterPageResult
	BYTE P1;
};

//**********************************************//
//************ GameServer <-> DataServer *******//
//**********************************************//
// Mirrored byte for byte in the DataServer's MasterSkillTree.h.

struct SDHP_MASTER_PAGE_DATA
{
	char PageName[MASTER_PAGE_NAME_SIZE];
	BYTE Skill[MASTER_PAGE_BLOB_SIZE];
};

struct SDHP_MASTER_PAGES_REQ_SEND
{
	PSBMSG_HEAD header; // C1:0D:01 GameServer -> DataServer
	WORD index;
	char account[11];
	char name[11];
};

struct SDHP_MASTER_PAGES_RECV
{
	PSWMSG_HEAD header; // C2:0D:01 DataServer -> GameServer
	WORD index;
	char account[11];
	char name[11];
	BYTE Active;
	BYTE Unlocked;
	SDHP_MASTER_PAGE_DATA Page[MASTER_PAGE_COUNT];
};

struct SDHP_MASTER_PAGES_SAVE_SEND
{
	PSWMSG_HEAD header; // C2:0D:31 GameServer -> DataServer
	WORD index;
	char account[11];
	char name[11];
	BYTE Active;
	BYTE Unlocked;
	SDHP_MASTER_PAGE_DATA Page[MASTER_PAGE_COUNT];
};

#pragma pack(pop)

static_assert(sizeof(PMSG_MASTER_PAGES_REQ_RECV) == 4, "PMSG_MASTER_PAGES_REQ_RECV must match the client's copy");
static_assert(sizeof(PMSG_MASTER_PAGE_SELECT_RECV) == 5, "PMSG_MASTER_PAGE_SELECT_RECV must match the client's copy");
static_assert(sizeof(PMSG_MASTER_PAGE_RENAME_RECV) == 4 + 1 + 13, "PMSG_MASTER_PAGE_RENAME_RECV must match the client's copy");
static_assert(sizeof(PMSG_MASTER_PAGES_INFO_SEND) == 4 + 3 + 5 * 13 + 2 + 5 * 2, "PMSG_MASTER_PAGES_INFO_SEND must match the client's copy");
static_assert(sizeof(PMSG_MASTER_PAGE_SNAPSHOT_SEND) == 4 + 1 + 2 + 1, "PMSG_MASTER_PAGE_SNAPSHOT_SEND must match the client's copy");
static_assert(sizeof(PMSG_MASTER_PAGE_SNAPSHOT_ENTRY) == 2, "PMSG_MASTER_PAGE_SNAPSHOT_ENTRY must match the client's copy");
static_assert(sizeof(PMSG_MASTER_PAGE_RESULT_SEND) == 4 + 2, "PMSG_MASTER_PAGE_RESULT_SEND must match the client's copy");
static_assert(sizeof(PMSG_MASTER_PAGE_SNAPSHOT_SEND) + MAX_MASTER_SKILL_LIST * sizeof(PMSG_MASTER_PAGE_SNAPSHOT_ENTRY) <= 255, "a snapshot must fit a C1 packet");
static_assert(sizeof(SDHP_MASTER_PAGE_DATA) == 13 + 360, "SDHP_MASTER_PAGE_DATA must match the DataServer's copy");
static_assert(sizeof(SDHP_MASTER_PAGES_REQ_SEND) == 4 + 2 + 11 + 11, "SDHP_MASTER_PAGES_REQ_SEND must match the DataServer's copy");
static_assert(sizeof(SDHP_MASTER_PAGES_RECV) == 5 + 2 + 11 + 11 + 2 + 5 * (13 + 360), "SDHP_MASTER_PAGES_RECV must match the DataServer's copy");
static_assert(sizeof(SDHP_MASTER_PAGES_SAVE_SEND) == sizeof(SDHP_MASTER_PAGES_RECV), "SDHP_MASTER_PAGES_SAVE_SEND must match the DataServer's copy");
static_assert(sizeof(SDHP_MASTER_PAGES_RECV) < 8192, "a DataServer packet must stay under the 8 KB buffers");

// One decoded entry of a page, in the order it is learned.
struct MASTER_PAGE_ENTRY
{
	int Index; // tree index (300+)
	int Rank;
	int Level; // 0 based, like CSkill::m_level
	int Points; // what it cost
	BYTE ClientSkill; // RequireClass[class], what F3:53 calls "skill"
};

class CMasterPage
{
public:
	CMasterPage();
	virtual ~CMasterPage();

	// Connection reset (gObjCharZeroSet): defaults, nothing loaded.
	void Reset(LPOBJ lpObj);

	// Pages loaded for this session and the feature on: the page rules apply.
	bool IsReady(LPOBJ lpObj);

	// The page lock, for the saves (see CMasterPageGuard).
	void Lock();
	void Unlock();

	// Points the character has earned in total: what is left on the live page
	// plus what the live page has spent.
	int GetEarned(LPOBJ lpObj);

	// DataServer.
	void GDPagesLoadSend(int aIndex);
	void DGPagesRecv(SDHP_MASTER_PAGES_RECV* lpMsg,int size);
	void GDPagesSaveSend(int aIndex);

	// The tree itself came back from the DataServer (DGMasterSkillTreeRecv).
	void OnTreeLoaded(LPOBJ lpObj);

	// Client.
	void CGPagesReqRecv(int aIndex);
	void CGPageViewRecv(PMSG_MASTER_PAGE_SELECT_RECV* lpMsg,int aIndex);
	void CGPageActivateRecv(PMSG_MASTER_PAGE_SELECT_RECV* lpMsg,int aIndex);
	void CGPageRenameRecv(PMSG_MASTER_PAGE_RENAME_RECV* lpMsg,int aIndex);

	// The Master Page Scroll (14,493). CheckScroll tells the player why not and
	// returns 0 (keep the item); ApplyScroll runs AFTER the item has been taken
	// from the inventory, so the save that follows never holds both the item and
	// the page it opened.
	bool CheckScroll(LPOBJ lpObj);
	void ApplyScroll(LPOBJ lpObj);

	// Class change / full skill wipes: every page is emptied, page 1 is live
	// again; the unlocked count and the names stay.
	void OnFullWipe(LPOBJ lpObj);

	void GCPagesInfoSend(int aIndex);

private:
	int GetMaxCount();
	int GetSpentLive(LPOBJ lpObj);
	int DecodeBlob(LPOBJ lpObj,const BYTE* blob,std::vector<MASTER_PAGE_ENTRY>* lpList,int* lpDropped);
	int GetBusyReason(LPOBJ lpObj);
	bool ActivatePage(int aIndex,int page);
	void ViewParkedPage(int aIndex,int page);
	void GCResultSend(int aIndex,BYTE code,BYTE p1);
	void GCSnapshotSend(int aIndex,int page,int used,std::vector<MASTER_PAGE_ENTRY>* lpList);
private:
	CCriticalSection m_lock;
};

extern CMasterPage gMasterPage;

// Holds the page lock for a scope: put one at the top of anything that copies a
// character's skills for a save.
class CMasterPageGuard
{
public:
	CMasterPageGuard();
	virtual ~CMasterPageGuard();
};
