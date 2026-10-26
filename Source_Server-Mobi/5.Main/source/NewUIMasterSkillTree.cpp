#include "stdafx.h"
#include "WinMain.h"
#include "wsclientinline.h"
#include "ZzzTexture.h"
#include "_TextureIndex.h"
#include "CSItemOption.h"
#include "UIControls.h"
#include "NewUISystem.h"
#include "NewUIMasterSkillTree.h"
#include "MultiLanguage.h"
#include "ZzzLodTerrain.h"

void TranslateTemplate(const char* pszEnglish, char* pszOut, size_t cbOut);	// MultiLanguage.cpp
#include "CharacterManager.h"
#include "NewUICustomMessageBox.h"
#include <vector>
#include <cmath>

// Master skill recommendations, pushed by the GameServer as C1:D3:B9: the master
// skills the top characters of this player's class learned, most popular first.
// Kept outside the window, which may not exist yet when the packet arrives at
// login.
namespace
{
	struct MasterRecommendEntry
	{
		int Skill;
		int Picks;
		int Level; // the level most of those players took it to
	};

	std::vector<MasterRecommendEntry> g_MasterRecommend;
	int g_MasterRecommendSample = 0;

	const MasterRecommendEntry* FindMasterRecommend(int skill)
	{
		for (size_t n = 0; n < g_MasterRecommend.size(); n++)
		{
			if (g_MasterRecommend[n].Skill == skill)
			{
				return &g_MasterRecommend[n];
			}
		}

		return NULL;
	}

	int GetMasterRecommendPicks(int skill)
	{
		for (size_t n = 0; n < g_MasterRecommend.size(); n++)
		{
			if (g_MasterRecommend[n].Skill == skill)
			{
				return g_MasterRecommend[n].Picks;
			}
		}

		return 0;
	}
}

void SetMasterSkillRecommendations(const BYTE* entries, int count, int sample)
{
	g_MasterRecommend.clear();

	for (int n = 0; n < count; n++)
	{
		MasterRecommendEntry entry;
		entry.Skill = entries[(n * 4) + 0] | (entries[(n * 4) + 1] << 8);
		entry.Picks = entries[(n * 4) + 2];
		entry.Level = entries[(n * 4) + 3];
		g_MasterRecommend.push_back(entry);
	}

	g_MasterRecommendSample = sample;
}

#if(NEW_MASTER_SKILL_TREE == 1)
extern int DisplayWinCDepthBox;
extern int DisplayWin;
extern int DisplayHeight;
extern int DisplayWinMid;
extern int DisplayHeightExt;
extern int DisplayWinExt;
extern int DisplayWinReal;
//MAX_MAGIC -> aumentar a 150
// MAX_SKILLS -> AUMENTAR A 650

_MASTER_SKILLTREE_DATA m_stMasterSkillTreeData[MAX_MASTER_SKILL_DATA];
_MASTER_SKILL_TOOLTIP m_stMasterSkillTooltip[MAX_MASTER_SKILL_DATA];

BOOL  InBounds(int a1, int  a2, int a3)
{
	return a1 >= (int)a2 && a1 <= (int)a3;
}

//**********************************************//
//************ Master skill pages **************//
//**********************************************//
// The server (MasterPage.cpp) keeps up to five pages per character; only the ACTIVE one is
// live and is what F3:53 describes, so the tree below always shows the live page. Another
// page is shown as a read-only preview from a D3:C2 snapshot, drawn over the same icons.
// All page state is file-static: CheckBtn reads window members by raw offset, so no data
// members are added to the class.
namespace
{
	const int kPgCount = 5;
	const int kPgNameSize = 13;

	// Strip below the 428 px window (virtual 640x480 space), relative to the window's x.
	const int kPgStripY = 432;
	const int kPgRow2Y = 459;
	const int kPgTabX = 10;
	const int kPgTabW = 96;
	const int kPgTabStep = 99;
	const int kPgTabH = 22;
	const int kPgRenameX = 508;
	const int kPgRenameW = 58;
	const int kPgActivateX = 570;
	const int kPgActivateW = 60;

	struct MasterPageUi
	{
		bool Have;					// a PAGES_INFO arrived: the server knows pages
		BYTE Active;
		BYTE Unlocked;
		BYTE Count;
		char Names[kPgCount][kPgNameSize];
		WORD Total;
		WORD Used[kPgCount];
		int Viewed;					// 1..5, 0 = the active page
		bool PreviewValid;			// the snapshot of Viewed has arrived
		WORD PreviewUsed;
		std::vector<BYTE> PreviewRaw;	// skill, level pairs as sent
		std::map<DWORD, BYTE> PreviewPts;	// SkillAttribute index -> points, built against the window's data
		BYTE PreviewCat[3];
		int PendingActivate;		// page asked for, 0 = none
		bool Renaming;
		int RenamePage;
		DWORD ToastTick;
		char Toast[96];
		DWORD LastRender;
		DWORD StaleTick;			// F3:53 arrived and the page list was asked for again; 0 = the list is current
	};

	MasterPageUi g_Pg;

#if !defined(MU_TOUCH_UI)
	CUITextInputBox* g_PgRenameBox = NULL;
#endif

#pragma pack(push,1)
	struct PMSG_MPAGE_REQ_SEND
	{
		PSBMSG_HEAD header;		// C1:D3:C0
	};

	struct PMSG_MPAGE_SELECT_SEND
	{
		PSBMSG_HEAD header;		// C1:D3:C1 (view), C1:D3:C3 (activate)
		BYTE Page;
	};

	struct PMSG_MPAGE_RENAME_SEND
	{
		PSBMSG_HEAD header;		// C1:D3:C4
		BYTE Page;
		char Name[kPgNameSize];
	};
#pragma pack(pop)

	// The page controls show: a page list is known, and an unanswered refresh is under 3 s old
	// (a character or server without pages never answers, so its strip goes away).
	bool PgOn()
	{
		if (g_Pg.Have == false)
		{
			return false;
		}

		return (g_Pg.StaleTick == 0 || (GetTickCount() - g_Pg.StaleTick) < 3000);
	}

	int PgViewed()
	{
		if (g_Pg.Viewed >= 1 && g_Pg.Viewed <= g_Pg.Count && g_Pg.Viewed <= kPgCount)
		{
			return g_Pg.Viewed;
		}

		return (g_Pg.Active >= 1 && g_Pg.Active <= kPgCount) ? g_Pg.Active : 1;
	}

	void PgToast(const char* text)
	{
		strncpy(g_Pg.Toast, text, sizeof(g_Pg.Toast) - 1);
		g_Pg.Toast[sizeof(g_Pg.Toast) - 1] = 0;
		g_Pg.ToastTick = GetTickCount();

		if (g_Pg.ToastTick == 0)
		{
			g_Pg.ToastTick = 1;
		}
	}

	void PgSendReq()
	{
		PMSG_MPAGE_REQ_SEND pMsg;
		pMsg.header.set(0xD3, 0xC0, sizeof(pMsg));
		DataSend((LPBYTE)&pMsg, pMsg.header.size);
	}

	void PgSendSelect(BYTE sub, int page)
	{
		PMSG_MPAGE_SELECT_SEND pMsg;
		pMsg.header.set(0xD3, sub, sizeof(pMsg));
		pMsg.Page = (BYTE)page;
		DataSend((LPBYTE)&pMsg, pMsg.header.size);
	}

	void PgSendRename(int page, const char* name)
	{
		PMSG_MPAGE_RENAME_SEND pMsg;
		memset(&pMsg, 0, sizeof(pMsg));
		pMsg.header.set(0xD3, 0xC4, sizeof(pMsg));
		pMsg.Page = (BYTE)page;
		strncpy(pMsg.Name, name, kPgNameSize - 1);
		DataSend((LPBYTE)&pMsg, pMsg.header.size);
	}

	// The hero stands in a safe zone (the server decides again).
	bool PgInSafeZone()
	{
		if (Hero == NULL)
		{
			return false;
		}

		int index = TERRAIN_INDEX_REPEAT(Hero->PositionX, Hero->PositionY);

		return (TerrainWall[index] & TW_SAFEZONE) == TW_SAFEZONE;
	}

	void PgRect(int x, int y, int w, int h, float r, float g, float b, float a)
	{
		::EnableAlphaTest();
		::glColor4f(r, g, b, a);
		RenderColor((float)x, (float)y, (float)w, (float)h);
		EndRenderColor();
		::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	}

	void PgFrame(int x, int y, int w, int h, float r, float g, float b, float a)
	{
		PgRect(x, y, w, 1, r, g, b, a);
		PgRect(x, y + h - 1, w, 1, r, g, b, a);
		PgRect(x, y, 1, h, r, g, b, a);
		PgRect(x + w - 1, y, 1, h, r, g, b, a);
	}

	void PgCancelRename()
	{
		const bool wasRenaming = g_Pg.Renaming;

		g_Pg.Renaming = false;

		// Key events only reach a window while the focus is on its related window.
		if (g_pMasterSkillTreeInterface)
		{
			g_pMasterSkillTreeInterface->SetRelatedWnd(NULL);
		}
#if !defined(MU_TOUCH_UI)
		if (g_PgRenameBox)
		{
			g_PgRenameBox->SetText("");
			g_PgRenameBox->SetState(UISTATE_HIDE);

			// Hiding the focused edit control leaves the focus nowhere, and the game
			// only reads keys while its own window has it.
			if (wasRenaming)
			{
				SetFocus(g_hWnd);
			}
		}
#endif
	}
}

// F3:53 is sent at login and after every activation, always BEFORE the page list (D3:C0).
// Clearing here keeps one character's page list from showing on the next one (or on a
// server where pages are off). PendingActivate survives: its result follows the list.
void MasterPagesOnTreeRebuilt()
{
	// The tree changed (login, activation, /remaster, and every point spent). Keep the strip,
	// ask for the page list again (Used changes with every point) and drop it if none comes.
	g_Pg.StaleTick = GetTickCount() | 1;
	PgSendReq();
	g_Pg.Viewed = 0;
	g_Pg.PreviewValid = false;
	g_Pg.PreviewPts.clear();
	memset(g_Pg.PreviewCat, 0, sizeof(g_Pg.PreviewCat));

	PgCancelRename();
}

void MasterPagesRecvInfo(const BYTE* data, int size)
{
	// C1:D3:C0, 84 bytes: Active, Unlocked, Count, Names[5][13], Total, Used[5]
	if (size < 84 || data[0] != 0xC1)
	{
		return;
	}

	g_Pg.Active = data[4];
	g_Pg.Unlocked = data[5];
	g_Pg.Count = data[6];

	if (g_Pg.Count < 1 || g_Pg.Count > kPgCount || g_Pg.Active < 1 || g_Pg.Active > kPgCount)
	{
		return;
	}

	if (g_Pg.Unlocked < 1 || g_Pg.Unlocked > g_Pg.Count)
	{
		g_Pg.Unlocked = 1;
	}

	for (int n = 0; n < kPgCount; n++)
	{
		memcpy(g_Pg.Names[n], &data[7 + (n * kPgNameSize)], kPgNameSize);
		g_Pg.Names[n][kPgNameSize - 1] = 0;
	}

	g_Pg.Total = (WORD)(data[72] | (data[73] << 8));

	for (int n = 0; n < kPgCount; n++)
	{
		g_Pg.Used[n] = (WORD)(data[74 + (n * 2)] | (data[75 + (n * 2)] << 8));
	}

	g_Pg.Have = true;
	g_Pg.StaleTick = 0;

	if (g_Pg.Viewed < 1 || g_Pg.Viewed > g_Pg.Unlocked)
	{
		g_Pg.Viewed = g_Pg.Active;
	}

	if (g_Pg.Viewed == g_Pg.Active)
	{
		g_Pg.PreviewValid = false;
	}
}

void MasterPagesRecvSnapshot(const BYTE* data, int size)
{
	// C1:D3:C2: Page, Used (WORD), Count, then Count x { skill, level }
	if (size < 8 || data[0] != 0xC1)
	{
		return;
	}

	const int page = data[4];
	const int count = data[7];

	if (count > MAX_MASTER_SKILL_DATA || size < (8 + (count * 2)))
	{
		return;
	}

	// An answer for a page that is no longer the one on screen is stale.
	if (page != PgViewed() || page == g_Pg.Active)
	{
		return;
	}

	g_Pg.PreviewUsed = (WORD)(data[5] | (data[6] << 8));
	g_Pg.PreviewRaw.assign(&data[8], &data[8] + (count * 2));
	g_Pg.PreviewValid = true;

#if(NEW_MASTER_SKILL_TREE == 1)
	if (g_pMasterSkillTreeInterface)
	{
		g_pMasterSkillTreeInterface->BuildPagePreview();
	}
#endif
}

void MasterPagesRecvResult(const BYTE* data, int size)
{
	// C1:D3:C3: Code, P1
	if (size < 6 || data[0] != 0xC1)
	{
		return;
	}

	const int code = data[4];
	const int p1 = data[5];
	char text[96];

	const int pending = g_Pg.PendingActivate;

	g_Pg.PendingActivate = 0;

	switch (code)
	{
	case 0:
		if (pending != 0)
		{
			// PAGES_INFO (already handled) says which page is live now; F3:53 rebuilt the tree.
			g_Pg.Viewed = g_Pg.Active;
			g_Pg.PreviewValid = false;
			sprintf(text, "Master page %d activated.", (int)g_Pg.Active);
			PgToast(text);
		}
		else
		{
			PgToast("Page renamed.");
		}
		return;
	case 1:
		PgToast("Switching pages needs a safe zone.");
		return;
	case 2:
		PgToast("That page is locked. Use a Master Page Scroll.");
		return;
	case 3:
		PgToast("That page is already active.");
		return;
	case 4:
		sprintf(text, "Wait %d s before switching again.", p1);
		PgToast(text);
		return;
	case 5:
		PgToast("Close other windows and finish duels or events first.");
		return;
	case 6:
		PgToast("Names: 1-12 letters, numbers, space, - or _.");
		return;
	default:
		PgToast("Master pages are not available right now.");
		return;
	}
}

SEASON3B::CNewUIMasterSkillTree::CNewUIMasterSkillTree()
{
	this->pBitMap[0] = NULL;

	this->pBitMap[1] = NULL;

	this->ConsumePoint = 0;

	this->CurSkillID = 0;

	this->m_TouchSkill = -1;

	this->m_TouchTick = 0;

	this->m_TouchHit = false;

	this->classCode = 0;

	this->CategoryTextIndex = 0;

	this->categoryPos[0] = { 11 + DisplayWinExt,55 };

	this->categoryPos[1] = { 221 + DisplayWinExt,55 };

	this->categoryPos[2] = { 431 + DisplayWinExt,55 };

	this->InitMasterSkillPoint();

	this->ClearSkillTreeInfo();

	this->ClearSkillTreeData();

	this->ClearSkillTooltipData();
}

SEASON3B::CNewUIMasterSkillTree::~CNewUIMasterSkillTree()
{
	this->Release();
}

void SEASON3B::CNewUIMasterSkillTree::ClearSkillTreeInfo()
{
	this->map_skilltreeinfo.clear();
}

BYTE SEASON3B::CNewUIMasterSkillTree::GetConsumePoint()
{
	return this->ConsumePoint;
}

int SEASON3B::CNewUIMasterSkillTree::GetCurSkillID()
{
	return this->CurSkillID;
}

bool SEASON3B::CNewUIMasterSkillTree::Create()
{
	g_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_MASTER_LEVEL, this);

	this->SetPos();

	this->LoadImages();

	this->m_CloseBT.ChangeButtonImgState(1, IMAGE_MASTER_INTERFACE + 5, 0, 0, 0);
	this->m_CloseBT.ChangeButtonInfo(611+ DisplayWinExt, 9, 13, 14);
	this->m_CloseBT.ChangeToolTipText(GlobalText[1002]);

	
	//this->m_ClearBT.ChangeButtonImgState(1, CNewUICharacterInfoWindow::IMAGE_CHAINFO_BTN_STAT_TRU, 0, 0, 0);
	//this->m_ClearBT.ChangeButtonInfo(611 + DisplayWinExt, 24, 13, 14);
	//this->m_ClearBT.ChangeToolTipText("Clear master");


	for (int i = 0; i < MAX_MASTER_SKILL_CATEGORY; i++)
	{
		this->ButtonX[i] = 0;

		this->ButtonY[i] = 0;
	}

	return true;
}

void SEASON3B::CNewUIMasterSkillTree::Release()
{
	this->UnloadImages();

	g_pNewUIMng->RemoveUIObj(this);

	this->ClearSkillTreeInfo();

	this->ClearSkillTreeData();

	this->ClearSkillTooltipData();
}

void SEASON3B::CNewUIMasterSkillTree::SetPos()
{
	this->PosX = DisplayWinExt;

	this->PosY = 0;

	this->width = 640;

	this->height = 428;
}

void SEASON3B::CNewUIMasterSkillTree::OpenMasterSkillTreeData(char* path)
{
	memset(m_stMasterSkillTreeData, 0, sizeof(m_stMasterSkillTreeData));

	FILE* fp = fopen(path, "rb");

	char Text[256];

	if (fp == NULL)
	{
		sprintf(Text, "%s - File not exist.", path);
		g_ErrorReport.Write(Text);
		MessageBox(g_hWnd, Text, NULL, MB_OK);
		SendMessage(g_hWnd, WM_DESTROY, 0, 0);
		return;
	}

	int Size = sizeof(_MASTER_SKILLTREE_DATA);

	BYTE* Buffer = new BYTE[Size * MAX_MASTER_SKILL_DATA];

	fread(Buffer, Size * MAX_MASTER_SKILL_DATA, 1, fp);

	DWORD dwCheckSum;

	fread(&dwCheckSum, sizeof(DWORD), 1u, fp);

	fclose(fp);

	//if (dwCheckSum != GenerateCheckSum2(Buffer, 12288, 0x2BC1))
	//{
	//	sprintf(Text, "%s - File corrupted.", path);
	//	g_ErrorReport.Write(Text);
	//	MessageBox(g_hWnd, Text, NULL, MB_OK);
	//	SendMessage(g_hWnd, WM_DESTROY, 0, 0);
	//	return;
	//}

	BYTE* pSeek = Buffer;

	for (int i = 0; i < MAX_MASTER_SKILL_DATA; i++)
	{
		BuxConvert(pSeek, Size);

		memcpy(&m_stMasterSkillTreeData[i], pSeek, Size);
		pSeek += Size;

		if (pSeek == NULL)
		{
			break;
		}
	}

	delete[] Buffer;
}

void SEASON3B::CNewUIMasterSkillTree::OpenMasterSkillTooltip(char* path)
{
	memset(m_stMasterSkillTooltip, 0, sizeof(m_stMasterSkillTooltip));

	FILE* fp = fopen(path, "rb");

	char Text[256];

	if (fp == NULL)
	{
		sprintf(Text, "%s - File not exist.", path);
		g_ErrorReport.Write(Text);
		MessageBox(g_hWnd, Text, NULL, MB_OK);
		SendMessage(g_hWnd, WM_DESTROY, 0, 0);
		return;
	}

	int Size = sizeof(_MASTER_SKILL_TOOLTIP);

	BYTE* Buffer = new BYTE[Size * MAX_MASTER_SKILL_DATA];

	fread(Buffer, Size * MAX_MASTER_SKILL_DATA, 1, fp);

	DWORD dwCheckSum;

	fread(&dwCheckSum, sizeof(DWORD), 1u, fp);

	fclose(fp);

	//if (dwCheckSum != GenerateCheckSum2(Buffer, 12288, 0x2BC1))
	//{
	//	sprintf(Text, "%s - File corrupted.", path);
	//	g_ErrorReport.Write(Text);
	//	MessageBox(g_hWnd, Text, NULL, MB_OK);
	//	SendMessage(g_hWnd, WM_DESTROY, 0, 0);
	//	return;
	//}

	BYTE* pSeek = Buffer;

	for (int i = 0; i < MAX_MASTER_SKILL_DATA; i++)
	{
		BuxConvert(pSeek, Size);

		memcpy(&m_stMasterSkillTooltip[i], pSeek, Size);

		pSeek += Size;

		if (pSeek == NULL)
		{
			break;
		}
	}

	delete[] Buffer;
}

void SEASON3B::CNewUIMasterSkillTree::InitMasterSkillPoint()
{
	for (int i = 0; i < 3; i++)
	{
		this->CategoryPoint[i] = 0;

		for (int k = 0; k < 10; k++)
		{
			this->skillPoint[i][k] = 0;
		}
	}
}

void SEASON3B::CNewUIMasterSkillTree::SetMasterType(BYTE Class)
{
	switch (Class)
	{
	case CLASS_WIZARD:
	case CLASS_SOULMASTER:
	case CLASS_GRANDMASTER:
		this->classCode = 2;
		break;
	case CLASS_KNIGHT:
	case CLASS_BLADEKNIGHT:
	case CLASS_BLADEMASTER:
		this->classCode = 1;
		break;
	case CLASS_ELF:
	case CLASS_MUSEELF:
	case CLASS_HIGHELF:
		this->classCode = 4;
		break;
	case CLASS_DARK:
	case CLASS_DUELMASTER:
		this->classCode = 16;
		break;
	case CLASS_DARK_LORD:
	case CLASS_LORDEMPEROR:
		this->classCode = 32;
		break;
	case CLASS_SUMMONER:
	case CLASS_BLOODYSUMMONER:
	case CLASS_DIMENSIONMASTER:
		this->classCode = 8;
		break;
	case CLASS_RAGEFIGHTER:
	case CLASS_TEMPLENIGHT:
		this->classCode = 64;
		break;
	default:
		break;
	}

	this->SetMasterSkillTreeData();

	this->SetMasterSkillToolTipData();

	switch (Class)
	{
	case CLASS_WIZARD:
	case CLASS_SOULMASTER:
	case CLASS_GRANDMASTER:
		this->CategoryTextIndex = 1751;
		this->ClassNameTextIndex = 1669;
		break;
	case CLASS_KNIGHT:
	case CLASS_BLADEKNIGHT:
	case CLASS_BLADEMASTER:
		this->CategoryTextIndex = 1755;
		this->ClassNameTextIndex = 1668;
		break;
	case CLASS_ELF:
	case CLASS_MUSEELF:
	case CLASS_HIGHELF:
		this->CategoryTextIndex = 1759;
		this->ClassNameTextIndex = 1670;
		break;
	case CLASS_DARK:
	case CLASS_DUELMASTER:
		this->CategoryTextIndex = 1763;
		this->ClassNameTextIndex = 1671;
		break;
	case CLASS_DARK_LORD:
	case CLASS_LORDEMPEROR:
		this->CategoryTextIndex = 1767;
		this->ClassNameTextIndex = 1672;
		break;
	case CLASS_SUMMONER:
	case CLASS_BLOODYSUMMONER:
	case CLASS_DIMENSIONMASTER:
		this->CategoryTextIndex = 3136;
		this->ClassNameTextIndex = 1689;
		break;
	case CLASS_RAGEFIGHTER:
	case CLASS_TEMPLENIGHT:
		this->CategoryTextIndex = 3330;
		this->ClassNameTextIndex = 3151;
		break;
	default:
		return;
	}
}

void SEASON3B::CNewUIMasterSkillTree::SetMasterSkillTreeData()
{
	this->ClearSkillTreeData();

	for (int i = 0; i < MAX_MASTER_SKILL_DATA; i++)
	{
		if (m_stMasterSkillTreeData[i].Index == 0)
		{
			break;
		}

		if ((this->classCode & m_stMasterSkillTreeData[i].ClassCode) == 0)
		{
			continue;
		}

		if (this->map_masterData.insert(std::pair<BYTE, _MASTER_SKILLTREE_DATA>(m_stMasterSkillTreeData[i].Index, m_stMasterSkillTreeData[i])).second == false)
		{
			break;
		}
	}
}

void SEASON3B::CNewUIMasterSkillTree::SetMasterSkillToolTipData()
{
	this->ClearSkillTooltipData();

	for (int i = 0; i < MAX_MASTER_SKILL_DATA; i++)
	{
		if (m_stMasterSkillTooltip[i].SkillIndex == 0)
		{
			break;
		}

		if ((this->classCode & m_stMasterSkillTooltip[i].ClassCode) == 0)
		{
			continue;
		}

		if (this->map_masterSkillToolTip.insert(std::pair<DWORD, _MASTER_SKILL_TOOLTIP>(m_stMasterSkillTooltip[i].SkillIndex, m_stMasterSkillTooltip[i])).second == false)
		{
			break;
		}
	}
}

bool SEASON3B::CNewUIMasterSkillTree::SetMasterSkillTreeInfo(int index, char SkillPoint, float value, float nextvalue)
{
	//g_ConsoleDebug->Write(MCD_ERROR, "SetMasterSkillTreeInfo Find %d", index);
	std::map<BYTE, _MASTER_SKILLTREE_DATA>::iterator it = this->map_masterData.find(index);

	if (it == this->map_masterData.end())
	{
		//g_ConsoleDebug->Write(MCD_ERROR, "SetMasterSkillTreeInfo END");
		return true;
	}

	CSkillTreeInfo skillInfo;
	

	skillInfo.SetSkillTreeInfo(SkillPoint, value, nextvalue);

	if (this->map_skilltreeinfo.insert(std::pair<DWORD, CSkillTreeInfo>(it->second.Skill, skillInfo)).second == false)
	{
		return false;
	}

	switch (it->second.Group)
	{
	case 0:
		this->CategoryPoint[0] += SkillPoint;
		break;
	case 1:
		this->CategoryPoint[1] += SkillPoint;
		break;
	case 2:
		this->CategoryPoint[2] += SkillPoint;
		break;
	}

	return true;
}

int SEASON3B::CNewUIMasterSkillTree::SetDivideString(char* text, int isItemTollTip, int TextNum, int iTextColor, int iTextBold, bool isPercent)
{
	if (text == NULL)
	{
		return TextNum;
	}

	char alpszDst[10][256];

	memset(alpszDst, 0, sizeof(alpszDst));

	int  nLine = 0;

	if (isItemTollTip == 0)
	{
		nLine = DivideStringByPixel((LPTSTR)alpszDst, 10, 256, text, 150, 1, 35);
	}
	else if (isItemTollTip == 1)
	{
		nLine = DivideStringByPixel((LPTSTR)alpszDst, 10, 256, text, 200, 1, 35);
	}

	for (int i = 0; i < nLine; i++)
	{
		TextListColor[TextNum] = iTextColor;

		TextBold[TextNum] = iTextBold;

		std::string cText = alpszDst[i];

		if (isPercent == true)
		{
			for (int j = cText.find("%", 0); j != -1; j = cText.find("%", j + 2))
			{
				cText.insert(j, "%");
			}
		}

		sprintf(TextList[TextNum], cText.c_str());

		TextNum++;
	}


	return TextNum;
}

bool SEASON3B::CNewUIMasterSkillTree::Render()
{
#if defined(MU_TOUCH_UI)
	// Render only runs while the window is open: a gap means it was closed in
	// between (X, Esc, another window), so start again with nothing selected.
	static DWORD s_LastRenderTick = 0;
	const DWORD renderNow = GetTickCount();

	if ((renderNow - s_LastRenderTick) > 500)
	{
		this->m_TouchSkill = -1;
		this->m_TouchTick = 0;
	}

	s_LastRenderTick = renderNow;
#endif

	// Render only runs while the window is open: a gap means it was just opened, so look at
	// the live page again and ask the server for the page list.
	{
		const DWORD pageNow = GetTickCount();

		if ((pageNow - g_Pg.LastRender) > 500)
		{
			g_Pg.Viewed = 0;
			g_Pg.PreviewValid = false;
			g_Pg.PendingActivate = 0;
			PgCancelRename();
			PgSendReq();

			if (PgOn())
			{
				g_Pg.Viewed = g_Pg.Active;
			}
		}

		g_Pg.LastRender = pageNow;
	}

	EnableAlphaTest();

	glColor4f(1.0, 1.0, 1.0, 1.0);

	SEASON3B::RenderImage(IMAGE_MASTER_INTERFACE, this->PosX, this->PosY, pBitMap[0]->Width, 428); //Fix Den Khung Master Skill

	SEASON3B::RenderImage(IMAGE_MASTER_INTERFACE+1, this->PosX + pBitMap[0]->Width, this->PosY, pBitMap[1]->Width, 428);

	this->RenderIcon();

	this->m_CloseBT.Render();
	//this->m_ClearBT.Render();

	DisableAlphaBlend();

	this->RenderText();

	this->RenderPageStrip();

	DisableAlphaBlend();

	return true;
}

bool SEASON3B::CNewUIMasterSkillTree::IsPagePreview()
{
	return (PgOn() && PgViewed() != (int)g_Pg.Active);
}

// Turns the snapshot (tree index + points) into the same per-skill lookup the window uses
// for the live page, so RenderIcon can draw either one.
void SEASON3B::CNewUIMasterSkillTree::BuildPagePreview()
{
	g_Pg.PreviewPts.clear();

	memset(g_Pg.PreviewCat, 0, sizeof(g_Pg.PreviewCat));

	for (size_t n = 0; (n + 1) < g_Pg.PreviewRaw.size(); n += 2)
	{
		std::map<BYTE, _MASTER_SKILLTREE_DATA>::iterator it = this->map_masterData.find(g_Pg.PreviewRaw[n]);

		if (it == this->map_masterData.end())
		{
			continue;
		}

		g_Pg.PreviewPts[it->second.Skill] = g_Pg.PreviewRaw[n + 1];

		if (it->second.Group < MAX_MASTER_SKILL_CATEGORY)
		{
			g_Pg.PreviewCat[it->second.Group] += g_Pg.PreviewRaw[n + 1];
		}
	}
}

// Mouse on the page strip below the window. True = the event was used.
bool SEASON3B::CNewUIMasterSkillTree::UpdatePageStrip()
{
	if (PgOn() == false)
	{
		return false;
	}

	const int x0 = this->PosX;

	if (SEASON3B::CheckMouseIn(x0, kPgStripY - 2, 640, 50) == false)
	{
		return false;
	}

	if (SEASON3B::IsPress(VK_LBUTTON) == false)
	{
		return true;
	}

	const int viewed = PgViewed();

#if !defined(MU_TOUCH_UI)
	if (g_Pg.Renaming)
	{
		// OK and Cancel sit on the second row, next to the text box.
		if (SEASON3B::CheckMouseIn(x0 + 270, kPgRow2Y - 2, 40, 18))
		{
			PlayBuffer(SOUND_CLICK01);

			char name[kPgNameSize];

			memset(name, 0, sizeof(name));

			if (g_PgRenameBox)
			{
				g_PgRenameBox->GetText(name, kPgNameSize);
			}

			name[kPgNameSize - 1] = 0;

			if (name[0] != 0)
			{
				PgSendRename(g_Pg.RenamePage, name);
			}

			PgCancelRename();

			return true;
		}

		if (SEASON3B::CheckMouseIn(x0 + 314, kPgRow2Y - 2, 56, 18))
		{
			PlayBuffer(SOUND_CLICK01);

			PgCancelRename();

			return true;
		}

		return true;
	}
#endif

	for (int n = 0; n < kPgCount; n++)
	{
		if (SEASON3B::CheckMouseIn(x0 + kPgTabX + (n * kPgTabStep), kPgStripY, kPgTabW, kPgTabH) == false)
		{
			continue;
		}

		PlayBuffer(SOUND_CLICK01);

		if ((n + 1) > g_Pg.Unlocked || (n + 1) > g_Pg.Count)
		{
			PgToast("Locked - use a Master Page Scroll.");
		}
		else if ((n + 1) != viewed)
		{
			g_Pg.Viewed = n + 1;
			g_Pg.PreviewValid = false;
			g_Pg.PreviewPts.clear();
			memset(g_Pg.PreviewCat, 0, sizeof(g_Pg.PreviewCat));

			if ((n + 1) != g_Pg.Active)
			{
				PgSendSelect(0xC1, n + 1);
			}
		}

		return true;
	}

#if !defined(MU_TOUCH_UI)
	if (SEASON3B::CheckMouseIn(x0 + kPgRenameX, kPgStripY, kPgRenameW, kPgTabH))
	{
		PlayBuffer(SOUND_CLICK01);

		g_Pg.Renaming = true;
		g_Pg.RenamePage = viewed;

		if (g_PgRenameBox == NULL)
		{
			g_PgRenameBox = new CUITextInputBox;
			g_PgRenameBox->Init(g_hWnd, 150, 16, kPgNameSize - 1);
			g_PgRenameBox->SetBackColor(255, 25, 25, 25);
			g_PgRenameBox->SetTextColor(255, 255, 255, 255);
			g_PgRenameBox->SetFont((HFONT)g_hFont);
		}

		g_PgRenameBox->SetState(UISTATE_NORMAL);
		g_PgRenameBox->SetText(g_Pg.Names[viewed - 1]);
		g_PgRenameBox->SetPosition(x0 + 110, kPgRow2Y - 1);
		g_PgRenameBox->GiveFocus(1);

		this->SetRelatedWnd(g_PgRenameBox->GetHandle());

		return true;
	}
#endif

	if (SEASON3B::CheckMouseIn(x0 + kPgActivateX, kPgStripY, kPgActivateW, kPgTabH))
	{
		PlayBuffer(SOUND_CLICK01);

		if (viewed == (int)g_Pg.Active)
		{
			PgToast("That page is already active.");
		}
		else if (PgInSafeZone() == false)
		{
			PgToast("Switching pages needs a safe zone.");
		}
		else
		{
			g_Pg.PendingActivate = viewed;

			PgSendSelect(0xC3, viewed);
		}

		return true;
	}

	return true;
}

void SEASON3B::CNewUIMasterSkillTree::RenderPageStrip()
{
	if (PgOn() == false)
	{
		return;
	}

	const int x0 = this->PosX;
	const int viewed = PgViewed();
	const bool preview = (viewed != (int)g_Pg.Active);

	PgRect(x0 + 4, kPgStripY - 3, 632, 49, 0.0f, 0.0f, 0.0f, 0.75f);

	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetBgColor(0, 0, 0, 0);

	for (int n = 0; n < kPgCount; n++)
	{
		const int x = x0 + kPgTabX + (n * kPgTabStep);
		const bool locked = ((n + 1) > g_Pg.Unlocked || (n + 1) > g_Pg.Count);
		const bool isActive = ((n + 1) == (int)g_Pg.Active);
		const bool isViewed = ((n + 1) == viewed);
		const bool hover = SEASON3B::CheckMouseIn(x, kPgStripY, kPgTabW, kPgTabH);

		if (locked)
		{
			PgRect(x, kPgStripY, kPgTabW, kPgTabH, 0.12f, 0.12f, 0.12f, 0.9f);
		}
		else if (isViewed)
		{
			PgRect(x, kPgStripY, kPgTabW, kPgTabH, 0.38f, 0.30f, 0.08f, 0.95f);
		}
		else
		{
			PgRect(x, kPgStripY, kPgTabW, kPgTabH, hover ? 0.30f : 0.20f, hover ? 0.30f : 0.20f, hover ? 0.30f : 0.20f, 0.95f);
		}

		if (isActive && locked == false)
		{
			PgFrame(x, kPgStripY, kPgTabW, kPgTabH, 1.0f, 0.82f, 0.15f, 1.0f);
		}
		else
		{
			PgFrame(x, kPgStripY, kPgTabW, kPgTabH, 0.45f, 0.45f, 0.45f, 0.8f);
		}

		char label[40];

		if (locked)
		{
			sprintf(label, "%s (locked)", g_Pg.Names[n]);
			g_pRenderText->SetTextColor(110, 110, 110, 255);
		}
		else
		{
			sprintf(label, "%s", g_Pg.Names[n]);
			g_pRenderText->SetTextColor(isViewed ? 255 : 210, isViewed ? 230 : 210, isViewed ? 150 : 210, 255);
		}

		g_pRenderText->RenderText(x, kPgStripY + 6, label, kPgTabW, 0, RT3_SORT_CENTER);
	}

	// Rename and Activate.
#if !defined(MU_TOUCH_UI)
	{
		const int x = x0 + kPgRenameX;
		const bool hover = SEASON3B::CheckMouseIn(x, kPgStripY, kPgRenameW, kPgTabH);

		PgRect(x, kPgStripY, kPgRenameW, kPgTabH, hover ? 0.30f : 0.20f, hover ? 0.30f : 0.20f, hover ? 0.30f : 0.20f, 0.95f);
		PgFrame(x, kPgStripY, kPgRenameW, kPgTabH, 0.45f, 0.45f, 0.45f, 0.8f);
		g_pRenderText->SetTextColor(230, 230, 230, 255);
		g_pRenderText->RenderText(x, kPgStripY + 6, "Rename", kPgRenameW, 0, RT3_SORT_CENTER);
	}
#endif

	{
		const int x = x0 + kPgActivateX;
		const bool canActivate = (preview && PgInSafeZone());
		const bool hover = canActivate && SEASON3B::CheckMouseIn(x, kPgStripY, kPgActivateW, kPgTabH);

		PgRect(x, kPgStripY, kPgActivateW, kPgTabH, canActivate ? (hover ? 0.55f : 0.40f) : 0.15f, canActivate ? (hover ? 0.42f : 0.30f) : 0.15f, canActivate ? 0.08f : 0.15f, 0.95f);
		PgFrame(x, kPgStripY, kPgActivateW, kPgTabH, canActivate ? 1.0f : 0.4f, canActivate ? 0.82f : 0.4f, canActivate ? 0.15f : 0.4f, 0.9f);

		if (canActivate)
		{
			g_pRenderText->SetTextColor(255, 235, 160, 255);
		}
		else
		{
			g_pRenderText->SetTextColor(120, 120, 120, 255);
		}

		g_pRenderText->RenderText(x, kPgStripY + 6, "Activate", kPgActivateW, 0, RT3_SORT_CENTER);
	}

	// Second row: points of the viewed page, then what the page is, or the last message.
	char text[128];

	g_pRenderText->SetTextColor(255, 155, 0, 255);

	sprintf(text, "Used %d / %d", (int)g_Pg.Used[viewed - 1], (int)g_Pg.Total);

	g_pRenderText->RenderText(x0 + 14, kPgRow2Y, text, 0, 0, RT3_SORT_LEFT);

#if !defined(MU_TOUCH_UI)
	if (g_Pg.Renaming)
	{
		if (g_PgRenameBox)
		{
			g_PgRenameBox->SetPosition(x0 + 110, kPgRow2Y - 1);
			g_PgRenameBox->Render();
		}

		PgRect(x0 + 270, kPgRow2Y - 2, 40, 18, 0.20f, 0.30f, 0.20f, 0.95f);
		PgRect(x0 + 314, kPgRow2Y - 2, 56, 18, 0.30f, 0.20f, 0.20f, 0.95f);
		g_pRenderText->SetTextColor(230, 230, 230, 255);
		g_pRenderText->RenderText(x0 + 270, kPgRow2Y + 1, "OK", 40, 0, RT3_SORT_CENTER);
		g_pRenderText->RenderText(x0 + 314, kPgRow2Y + 1, "Cancel", 56, 0, RT3_SORT_CENTER);
		g_pRenderText->RenderText(x0 + 380, kPgRow2Y, "Enter to confirm, Esc to cancel", 0, 0, RT3_SORT_LEFT);

		return;
	}
#endif

	if (g_Pg.ToastTick != 0 && (GetTickCount() - g_Pg.ToastTick) < 4000)
	{
		g_pRenderText->SetTextColor(255, 220, 90, 255);
		g_pRenderText->RenderText(x0 + 150, kPgRow2Y, g_Pg.Toast, 0, 0, RT3_SORT_LEFT);
	}
	else if (preview)
	{
		g_pRenderText->SetTextColor(150, 200, 255, 255);

		if (g_Pg.PreviewValid)
		{
			g_pRenderText->RenderText(x0 + 150, kPgRow2Y, "Preview only - activate this page in a safe zone to use it.", 0, 0, RT3_SORT_LEFT);
		}
		else
		{
			g_pRenderText->RenderText(x0 + 150, kPgRow2Y, "Loading page...", 0, 0, RT3_SORT_LEFT);
		}
	}
	else
	{
		g_pRenderText->SetTextColor(255, 210, 40, 255);
		g_pRenderText->RenderText(x0 + 150, kPgRow2Y, "ACTIVE page", 0, 0, RT3_SORT_LEFT);
	}
}

bool SEASON3B::CNewUIMasterSkillTree::Update()
{
	if (g_Pg.Renaming && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MASTER_LEVEL) == false)
	{
		PgCancelRename();
	}

	return 1;
}

bool SEASON3B::CNewUIMasterSkillTree::UpdateMouseEvent()
{
	const POINT& closePos = this->m_CloseBT.GetPos();
	const POINT& closeSize = this->m_CloseBT.GetSize();

#if defined(MU_TOUCH_UI)
	const int closePadX = 12;
	const int closePadY = 10;

	if (SEASON3B::IsRelease(VK_LBUTTON) == true
		&& SEASON3B::CheckMouseIn((closePos.x - closePadX), (closePos.y - closePadY), (closeSize.x + (closePadX * 2)), (closeSize.y + (closePadY * 2))) == true)
	{
		PlayBuffer(SOUND_CLICK01);
		g_pNewUISystem->Hide(SEASON3B::INTERFACE_MASTER_LEVEL);

		return true;
	}
#endif

	if (this->m_CloseBT.UpdateMouseEvent() == true)
	{
		PgCancelRename();

		g_pNewUISystem->Hide(SEASON3B::INTERFACE_MASTER_LEVEL);

		return true;
	}


	//if (this->m_ClearBT.UpdateMouseEvent() == true)
	//{
	//	char szCmd[32];
	//	sprintf(szCmd, "/taymaster");
	//	SendChat(szCmd);
	//	return true;
	//}

	// The page strip sits under the window, outside its rectangle.
	if (this->UpdatePageStrip() == true)
	{
		return false;
	}

	bool result = true;

	if (SEASON3B::IsPress(VK_LBUTTON) == true)
	{
		result = this->CheckMouse(MouseX, MouseY);

		if (result == false)
		{
			PlayBuffer(SOUND_CLICK01);
		}
	}
	
	for (int i = 0; i < MAX_MASTER_SKILL_CATEGORY; i++)
	{
		if (this->ButtonX[i] == 1 && SEASON3B::IsPress(VK_LBUTTON) == true)
		{
			this->ButtonX[i] = 0;

			return true;
		}
	}
	
	this->CheckBtn();
	
	if (SEASON3B::CheckMouseIn(this->PosX, this->PosY, this->width, this->height) == true)
	{
		return false;
	}

	return result;
}

bool SEASON3B::CNewUIMasterSkillTree::UpdateKeyEvent()
{
#if !defined(MU_TOUCH_UI)
	// Typing a page name: Enter confirms, Esc cancels, and no letter (A closes this window)
	// reaches the window hotkeys.
	if (g_Pg.Renaming && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MASTER_LEVEL) == false)
	{
		// Closed while typing (X, or another window took over): put the text box away.
		PgCancelRename();
	}

	if (g_Pg.Renaming && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MASTER_LEVEL))
	{
		if (SEASON3B::IsPress(VK_RETURN))
		{
			char name[kPgNameSize];

			memset(name, 0, sizeof(name));

			if (g_PgRenameBox)
			{
				g_PgRenameBox->GetText(name, kPgNameSize);
			}

			name[kPgNameSize - 1] = 0;

			if (name[0] != 0)
			{
				PgSendRename(g_Pg.RenamePage, name);
			}

			PgCancelRename();
		}
		else if (SEASON3B::IsPress(VK_ESCAPE))
		{
			PgCancelRename();
		}

		return false;
	}
#endif

	if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MASTER_LEVEL) == false || SEASON3B::IsPress(VK_ESCAPE) == false && SEASON3B::IsPress('A') == false)
	{
		return true;
	}

	g_pNewUISystem->Hide(SEASON3B::INTERFACE_MASTER_LEVEL);

	PlayBuffer(SOUND_CLICK01);

	return false;
}

float SEASON3B::CNewUIMasterSkillTree::GetLayerDepth()
{
	return 8.1000004;
}

void SEASON3B::CNewUIMasterSkillTree::LoadImages()
{
	LoadBitmap("Interface\\new_Master_back01.jpg", IMAGE_MASTER_INTERFACE, GL_LINEAR);	//31534

	LoadBitmap("Interface\\new_Master_back02.jpg", IMAGE_MASTER_INTERFACE + 1, GL_LINEAR);	//31535

	this->pBitMap[0] = &Bitmaps[IMAGE_MASTER_INTERFACE];

	this->pBitMap[1] = &Bitmaps[IMAGE_MASTER_INTERFACE + 1];

	LoadBitmap("Interface\\new_Master_Icon.jpg", IMAGE_MASTER_INTERFACE + 2, GL_LINEAR);	//31536

	LoadBitmap("Interface\\new_Master_Non_Icon.jpg", IMAGE_MASTER_INTERFACE + 3, GL_LINEAR);	//31537

	LoadBitmap("Interface\\new_Master_box.tga", IMAGE_MASTER_INTERFACE + 4, GL_LINEAR);	//31538

	LoadBitmap("Interface\\new_Master_exit.jpg", IMAGE_MASTER_INTERFACE + 5, GL_LINEAR);	//31539

	LoadBitmap("Interface\\new_Master_arrow01.tga", IMAGE_MASTER_INTERFACE + 6, GL_LINEAR);	//31540

	LoadBitmap("Interface\\new_Master_arrow02.tga", IMAGE_MASTER_INTERFACE + 7, GL_LINEAR);	//31541

	LoadBitmap("Interface\\new_Master_arrow03.tga", IMAGE_MASTER_INTERFACE + 8, GL_LINEAR);	//31542

	LoadBitmap("Interface\\new_Master_arrow04.tga", IMAGE_MASTER_INTERFACE + 9, GL_LINEAR);	//31543

	LoadBitmap("Interface\\new_Master_arrow05.tga", IMAGE_MASTER_INTERFACE + 10, GL_LINEAR);	//31544

	LoadBitmap("Interface\\new_Master_arrow06.tga", IMAGE_MASTER_INTERFACE + 11, GL_LINEAR);	//31545

	LoadBitmap("Interface\\new_Master_arrow07.tga", IMAGE_MASTER_INTERFACE + 12, GL_LINEAR);	//31546

	LoadBitmap("Interface\\new_Master_arrow08.tga", IMAGE_MASTER_INTERFACE + 13, GL_LINEAR);	//31547
}

void SEASON3B::CNewUIMasterSkillTree::UnloadImages()
{
	for (int i = 0; i < 14; i++)	//estaba en 4, el viejo master tenia 4 asique se deben haber olvidado actualizarlo
	{
		DeleteBitmap(i + IMAGE_MASTER_INTERFACE, 0);
	}
}

void SEASON3B::CNewUIMasterSkillTree::RenderText()
{
	g_pRenderText->SetFont(g_hFont);

	if (SEASON3B::IsPress(VK_LBUTTON) == false && SEASON3B::CheckMouseIn(458, 11, 81, 10) == true)
	{
		TextList[0][0] = 0;
		TextBold[0] = 0;
		TextListColor[0] = 0;
		sprintf(TextList[0], "%I64d / %I64d", Master_Level_Data.lMasterLevel_Experince, Master_Level_Data.lNext_MasterLevel_Experince);
		RenderTipTextList(466 + DisplayWinExt, 26, 1, 0, 3, 0, 1);
	}

	g_pRenderText->SetTextColor(255, 255, 255, 0xFFu);

	g_pRenderText->SetBgColor(0, 0, 0, 1u);

	char Buffer[256];

	memset(Buffer, 0, sizeof(Buffer));

	sprintf(Buffer, GlobalText[1746], Master_Level_Data.nMLevel);

	g_pRenderText->RenderText(275 + DisplayWinExt, 11, Buffer, 0, 0, 1, 0);

	// A previewed page shows what is left on THAT page; the live page shows the live points.
	const bool pagePreview = this->IsPagePreview();
	const BYTE* categoryShown = (pagePreview ? g_Pg.PreviewCat : this->CategoryPoint);

	{
		int pointsShown = Master_Level_Data.nMLevelUpMPoint;

		if (pagePreview)
		{
			pointsShown = (int)g_Pg.Total - (int)g_Pg.Used[PgViewed() - 1];
			pointsShown = (pointsShown < 0) ? 0 : pointsShown;
		}

		sprintf(Buffer, GlobalText[1747], pointsShown);
	}

	g_pRenderText->RenderText(372 + DisplayWinExt, 11, Buffer, 0, 0, 1, 0);

	if (Master_Level_Data.lNext_MasterLevel_Experince != 0)
	{
		__int64 iTotalLevel = Master_Level_Data.nMLevel + 400;				// 종합레벨 - 400렙이 만렙이기 때문에 더해준다.
		__int64 iTOverLevel = iTotalLevel - 255;		// 255레벨 이상 기준 레벨
		__int64 iBaseExperience = 0;					// 레벨 초기 경험치

		__int64 iData_Master =	// A
			(
			(
				(__int64)9 + (__int64)iTotalLevel
				)
				* (__int64)iTotalLevel
				* (__int64)iTotalLevel
				* (__int64)10
				)
			+
			(
			(
				(__int64)9 + (__int64)iTOverLevel
				)
				* (__int64)iTOverLevel
				* (__int64)iTOverLevel
				* (__int64)1000
				);

		iBaseExperience = (iData_Master - (__int64)3892250000) / (__int64)2;	// B

		// 레벨업 경험치
		double fNeedExp = (double)Master_Level_Data.lNext_MasterLevel_Experince - (double)iBaseExperience;

		// 현재 획득한 경험치
		double fExp = (double)Master_Level_Data.lMasterLevel_Experince - (double)iBaseExperience;

		sprintf(Buffer, GlobalText[3335], fExp / fNeedExp * 100.0);

		g_pRenderText->RenderText(466 + DisplayWinExt, 11, Buffer, 0, 0, 1, 0);
	}

	g_pRenderText->RenderText(154 + DisplayWinExt, 11, GlobalText[this->ClassNameTextIndex], 0, 0, 1, 0);

	g_pRenderText->SetTextColor(255, 155, 0, 0xFFu);

	sprintf(Buffer, GlobalText[this->CategoryTextIndex], categoryShown[0]);

	g_pRenderText->RenderText(92 + DisplayWinExt, 40, Buffer, 0, 0, 1, 0);

	sprintf(Buffer, GlobalText[this->CategoryTextIndex + 1], categoryShown[1]);

	g_pRenderText->RenderText(302 + DisplayWinExt, 40, Buffer, 0, 0, 1, 0);

	sprintf(Buffer, GlobalText[this->CategoryTextIndex + 2], categoryShown[2]);

	g_pRenderText->RenderText(513 + DisplayWinExt, 40, Buffer, 0, 0, 1, 0);
}

bool SEASON3B::CNewUIMasterSkillTree::GetXYImgMaster(JCCoord* GetXY, int Skill)
{

	JCCoord BGetXY;
	SKILL_ATTRIBUTE* p = &SkillAttribute[Skill];

	if (!p) { return false; }

	BGetXY.CalcY = (double)(p->SkillGroup / 25) * 0.0546875;
	BGetXY.CalcX = (double)(p->SkillGroup % 25) * 0.0390625;
	*GetXY = BGetXY;
	//g_ConsoleDebug->Write(MCD_RECEIVE, "Skill %d %f ~ %f ", Skill, BGetXY.CalcX, BGetXY.CalcY);
	return true;

}

void SEASON3B::CNewUIMasterSkillTree::RenderIcon()
{
	float v19; // [esp+38h] [ebp-178h]
	float v20; // [esp+40h] [ebp-170h]
	float v21; // [esp+38h] [ebp-178h]
	float v22; // [esp+40h] [ebp-170h]
	float v35; // [esp+84h] [ebp-12Ch]
	float v32; // [esp+78h] [ebp-138h]
	char Buffer[256];
	int recommendedPoints = 0;
	// Another page than the live one: its points come from the snapshot, nothing is
	// recommended and nothing can be bought.
	const bool pagePreview = this->IsPagePreview();
	const int recommended = (pagePreview ? -1 : this->FindRecommendedSkill(&recommendedPoints));
	for (std::map<BYTE, _MASTER_SKILLTREE_DATA>::iterator it = this->map_masterData.begin(); it != this->map_masterData.end(); it++)
	{
		BYTE Group = it->second.Group;

		int Skill = it->second.Skill;

		SKILL_ATTRIBUTE* p = &SkillAttribute[Skill];

		int skillPoint = 0;

		int index = (it->second.Index - 1) % 4;

		BYTE rank = p->SkillRank;

		std::map<DWORD, CSkillTreeInfo>::iterator sit = this->map_skilltreeinfo.find(Skill);

		if (pagePreview)
		{
			std::map<DWORD, BYTE>::iterator pit = g_Pg.PreviewPts.find(Skill);

			if (pit != g_Pg.PreviewPts.end())
			{
				skillPoint = pit->second;
			}
		}
		else if (sit != this->map_skilltreeinfo.end())
		{
			skillPoint = sit->second.GetSkillPoint();
		}

		DWORD textColor = RGBA(255, 255, 255, 255);
		::EnableAlphaTest();
		::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		g_pRenderText->SetBgColor(0);
		g_pRenderText->SetTextColor(textColor);

		if (pagePreview ? (skillPoint == 0) : (this->CheckParentSkill(it->second.RequireSkill[0], it->second.RequireSkill[1]) == false || this->CheckRankPoint(Group, rank, skillPoint) == false || this->CheckBeforeSkill(Skill, skillPoint) == false || this->CheckBeforeSkill(Skill, skillPoint) == false))
		{
			textColor = RGBA(120, 120, 120, 255);

			g_pRenderText->SetTextColor(textColor);
		}
		int CalcX = (int)(index * 49.0f + this->categoryPos[Group].x);
		int CalcY = (int)(this->categoryPos[Group].y + (p->SkillRank - 1) * 41.0f);
		SEASON3B::RenderImage(IMAGE_MASTER_INTERFACE + 4, CalcX, CalcY, 50.0, 38.0, 0.0, 0.0, textColor);
		v22 = (double)(p->SkillGroup / 25) * 0.0546875;
		v21 = (double)(p->SkillGroup % 25) * 0.0390625;
		v20 = CalcY + 5.0;
		v19 = CalcX + 8.5;
		SEASON3B::RenderImage(IMAGE_MASTER_INTERFACE + 2, v19, v20, 20.0, 28.0, v21, v22, 0.0390625, 0.053710938, textColor);

		// The next best pick: a pulsing gold frame and a PICK tag on the skill
		// the top players of this class learned most that this player can
		// raise right now.
		if (Skill == recommended)
		{
			const float pulse = 0.55f + 0.45f * sinf((float)GetTickCount() * 0.006f);

			::EnableAlphaTest();
			glColor4f(1.0f, 0.82f, 0.15f, pulse);
			RenderColor((float)CalcX, (float)CalcY, 50.0f, 2.0f);
			RenderColor((float)CalcX, (float)CalcY + 36.0f, 50.0f, 2.0f);
			RenderColor((float)CalcX, (float)CalcY, 2.0f, 38.0f);
			RenderColor((float)CalcX + 48.0f, (float)CalcY, 2.0f, 38.0f);
			EndRenderColor();
			::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

			g_pRenderText->SetFont(g_hFontBold);
			g_pRenderText->SetBgColor(0, 0, 0, 200);
			g_pRenderText->SetTextColor(255, 210, 40, 255);
			char pickText[16];
			char szPickFormat[32];
			TranslateTemplate("PICK +%d", szPickFormat, sizeof(szPickFormat));
			sprintf(pickText, szPickFormat, recommendedPoints);
			g_pRenderText->RenderText(CalcX + 3, CalcY + 2, pickText, 0, 0, RT3_SORT_LEFT, 0);
			g_pRenderText->SetFont(g_hFont);
			g_pRenderText->SetBgColor(0);
			g_pRenderText->SetTextColor(textColor);
		}
		//===Draw Skill Point

		memset(Buffer, 0, sizeof(Buffer));
		//sprintf(Buffer, "%d", skillPoint);
		float v12 = (int)(CalcY + 25.0);
		float  v11 = (int)(CalcX + 38.0);

		g_pRenderText->RenderText( v11, v12, Buffer, 0, 0, 1, 0);
		RenderNumber(v11, v12, skillPoint, 0.75f);
		int v40 = it->second.ArrowDirection;
		int v34 = 0.0;
		int v39 = 0.0;
		int v33 = 0.0;
		int v37 = 0.0;
		if (InBounds(v40, 1u, 8u))
		{
			switch (v40)
			{
			case 1u:
				v34 = 30.0;
				v39 = 15.0;
				v33 = 28.0;
				v37 = 7.0;
				break;
			case 2u:
				v34 = -21.0;
				v39 = 15.0;
				v33 = 28.0;
				v37 = 7.0;
				break;
			case 3u:
				v34 = 16.0;
				v39 = 35.0;
				v33 = 7.0;
				v37 = 12.0;
				break;
			case 4u:
				v34 = 16.0;
				v39 = 35.0;
				v33 = 7.0;
				v37 = 52.0;
				break;
			case 5u:
				v34 = 35.0;
				v39 = 15.0;
				v33 = 42.0;
				v37 = 31.0;
				break;
			case 6u:
				v34 = -35.0;
				v39 = 15.0;
				v33 = 42.0;
				v37 = 31.0;
				break;
			case 7u:
				v34 = 19.0;
				v39 = 35.0;
				v33 = 40.0;
				v37 = 28.0;
				break;
			case 8u:
				v34 = -21.0;
				v39 = 35.0;
				v33 = 40.0;
				v37 = 28.0;
				break;
			default:
				break;
			}
			int v18 = CalcY + v39;
			int v17 = CalcX + v34;
			SEASON3B::RenderImage(v40 + (IMAGE_MASTER_INTERFACE + 5), v17, v18, v33, v37, 0.0, 0.0, textColor);
		}
	}
	DisableAlphaBlend();
	this->RenderToolTip();
}

void SEASON3B::CNewUIMasterSkillTree::RenderToolTip()
{
	// The effect numbers belong to the live skills; another page is only previewed.
	if (this->IsPagePreview())
	{
		return;
	}

	for (std::map<BYTE, _MASTER_SKILLTREE_DATA>::iterator it = this->map_masterData.begin(); it != this->map_masterData.end(); it++)
	{
		BYTE group = it->second.Group;

		int Skill = it->second.Skill;

		SKILL_ATTRIBUTE* p = &SkillAttribute[Skill];

		if (p == NULL)
		{
			break;
		}

		int index = (it->second.Index - 1) % 4;

		int CalcX = (int)(index * 49.0f + this->categoryPos[group].x);

		int CalcY = (int)(this->categoryPos[group].y + (p->SkillRank - 1) * 41.0f);

#if defined(MU_TOUCH_UI)
		// Touch: the details of the tapped (selected) skill stay up - there is
		// no hover to keep them open.
		if (Skill != this->m_TouchSkill)
		{
			continue;
		}
#else
		if (SEASON3B::IsPress(VK_LBUTTON) == true || SEASON3B::CheckMouseIn(CalcX + 8, CalcY + 5, 20, 28) == false)
		{
			continue;
		}
#endif

		std::map<DWORD, _MASTER_SKILL_TOOLTIP>::iterator mtit = this->map_masterSkillToolTip.find(Skill);

		if (mtit == this->map_masterSkillToolTip.end())
		{
			return;
		}

		int skillPoint = 0;

		float skillValue = 0.0f;

		float skillNextValue = 0.0f;

		std::map<DWORD, CSkillTreeInfo>::iterator sit = this->map_skilltreeinfo.find(Skill);

		if (sit != this->map_skilltreeinfo.end())
		{
			skillPoint = sit->second.GetSkillPoint();

			skillValue = sit->second.GetSkillValue();

			skillNextValue = sit->second.GetSkillNextValue();
		}

		for (int i = 0; i < 30; i++)
		{
			TextList[i][0] = 0;
		}

		memset(TextBold, 0, sizeof(TextBold));

		for (int i = 0; i < 30; i++)
		{
			TextListColor[i] = i == 0 ? TEXT_COLOR_YELLOW : TEXT_COLOR_WHITE;
		}

		int lineCount = 0;

		sprintf(TextList[lineCount], "%s", p->Name);

		TextBold[lineCount] = true;

		lineCount++;

		sprintf(TextList[lineCount], mtit->second.Info1, p->SkillRank, skillPoint, it->second.MaxLevel);

		lineCount++;

		char buffer[512];

		memset(buffer, 0, sizeof(buffer));

		if (it->second.DefValue == -1.0f)
		{
			sprintf(buffer, mtit->second.Info2);
		}
		else
		{
			sprintf(buffer, mtit->second.Info2, skillPoint != 0 ? skillValue : it->second.DefValue);
		}

		lineCount = this->SetDivideString(buffer, 0, lineCount, 0, 0, true);

		if (skillPoint != 0 && skillPoint < it->second.MaxLevel)
		{
			sprintf(buffer, GlobalText[3328]);

			lineCount = this->SetDivideString(buffer, 0, lineCount, 4, 0, true);

			TextBold[lineCount] = 1;

			sprintf(buffer, mtit->second.Info2, skillNextValue);

			lineCount = this->SetDivideString(buffer, 0, lineCount, 0, 0, true);
		}

		if (skillPoint < it->second.MaxLevel)
		{
			sprintf(buffer, GlobalText[3329]);

			lineCount = this->SetDivideString(buffer, 0, lineCount, 1, 0, true);

			TextBold[lineCount] = 1;

			sprintf(buffer, mtit->second.Info3, it->second.RequiredPoints);

			if (it->second.RequiredPoints <= Master_Level_Data.nMLevelUpMPoint)
			{
				lineCount = this->SetDivideString(buffer, 0, lineCount, 0, 0, true);
			}
			else
			{
				lineCount = this->SetDivideString(buffer, 0, lineCount, 2, 0, true);
			}
		}

		int iTextColor = this->CheckBeforeSkill(Skill, skillPoint) == true ? 0 : 2;

		sprintf(buffer, mtit->second.Info4);

		lineCount = this->SetDivideString(buffer, 0, lineCount, iTextColor, 0, true);

		if (skillPoint < it->second.MaxLevel && p->SkillRank != 1)
		{
			iTextColor = this->CheckRankPoint(group, p->SkillRank, skillPoint) == true ? 0 : 2;

			sprintf(buffer, mtit->second.Info5);

			lineCount = this->SetDivideString(buffer, 0, lineCount, iTextColor, 0, true);

			int RequireSkill1 = it->second.RequireSkill[0];

			int RequireSkill2 = it->second.RequireSkill[1];

			if (RequireSkill1 != 0)
			{
				std::map<DWORD, CSkillTreeInfo>::iterator rq1it = this->map_skilltreeinfo.find(RequireSkill1);

				if (rq1it != this->map_skilltreeinfo.end())
				{
					iTextColor = rq1it->second.GetSkillValue() < 10 ? 2 : 0;
				}
				else
				{
					iTextColor = 2;
				}

				sprintf(buffer, mtit->second.Info6);

				lineCount = this->SetDivideString(buffer, 0, lineCount, iTextColor, 0, true);
			}

			if (RequireSkill2 != 0)
			{
				std::map<DWORD, CSkillTreeInfo>::iterator rq1it = this->map_skilltreeinfo.find(RequireSkill2);

				if (rq1it != this->map_skilltreeinfo.end())
				{
					iTextColor = rq1it->second.GetSkillValue() < 10 ? 2 : 0;
				}
				else
				{
					iTextColor = 2;
				}

				sprintf(buffer, mtit->second.Info7);

				lineCount = this->SetDivideString(buffer, 0, lineCount, iTextColor, 0, true);
			}
		}

		// How popular this skill is among the top players of the class.
		const MasterRecommendEntry* recommend = FindMasterRecommend(Skill);
		int pointsToAdd = 0;

		if (Skill == this->FindRecommendedSkill(&pointsToAdd))
		{
			char szRecFormat[160];
			TranslateTemplate("Recommended next pick: add %d point%s", szRecFormat, sizeof(szRecFormat));
			sprintf(buffer, szRecFormat, pointsToAdd, (pointsToAdd == 1) ? "" : "s");
			lineCount = this->SetDivideString(buffer, 0, lineCount, TEXT_COLOR_YELLOW, 1, false);
		}

		if (recommend != NULL && g_MasterRecommendSample > 0)
		{
			char szLearnFormat[192];
			TranslateTemplate("Learned by %d of the top %d players of your class, most to level %d", szLearnFormat, sizeof(szLearnFormat));
			sprintf(buffer, szLearnFormat, recommend->Picks, g_MasterRecommendSample, recommend->Level);
			lineCount = this->SetDivideString(buffer, 0, lineCount, TEXT_COLOR_GREEN, 0, false);
		}

		RenderTipTextList(CalcX + 8, CalcY + 33, lineCount, 0, 3, 0, 1);
	}
}

bool SEASON3B::CNewUIMasterSkillTree::CheckMouse(int posx, int posy)
{
	POINT position[3] = { {185,65},{385,65},{585,65} };

	for (int i = 0; i < MAX_MASTER_SKILL_CATEGORY; i++)
	{
		if (SEASON3B::CheckMouseIn(position[i].x + this->PosX, this->ButtonY[i] + position[i].y + this->PosY, 15, 30) == true && this->ButtonX[i] == 0)
		{
			this->ButtonY[i] = 1;

			return 0;
		}
	}

	return 1;
}

bool SEASON3B::CNewUIMasterSkillTree::CheckBtn()
{
	int posX = 220;

	for (int i = 0; i < MAX_MASTER_SKILL_CATEGORY; i++)
	{
		if (this->ButtonX[i] == 1 && SEASON3B::IsRelease(VK_LBUTTON))
		{
			this->ButtonX[i] = 0;
			return 0;
		}

		if (this->ButtonX[i] == 1)
		{
			this->ButtonY[i] = MouseY - 65;

			if (this->ButtonY[i] <= posX)
			{
				if (*((int*)this + i + 12) <= 0)
					this->ButtonY[i] = 0;
			}
			else
			{
				this->ButtonY[i] = posX;
			}

		}
	}
	
#if defined(MU_TOUCH_UI)
	// This runs every frame, not only on a tap - only a frame with a tap may
	// clear the selection below.
	const bool tappedThisFrame = SEASON3B::IsPress(VK_LBUTTON);
	this->m_TouchHit = false;
#endif

	for (std::map<BYTE, _MASTER_SKILLTREE_DATA>::iterator it = this->map_masterData.begin(); it != this->map_masterData.end(); it++)
	{
		BYTE Group = it->second.Group;

		switch (Group)
		{
		case 0:
			this->CheckAttributeArea(Group, it->second.Index, it->second.RequiredPoints, it->second.RequireSkill[0], it->second.RequireSkill[1], it->second.Skill, it->second.DefValue);
			break;
		case 1:
			this->CheckAttributeArea(Group, it->second.Index, it->second.RequiredPoints, it->second.RequireSkill[0], it->second.RequireSkill[1], it->second.Skill, it->second.DefValue);
			break;
		case 2:
			this->CheckAttributeArea(Group, it->second.Index, it->second.RequiredPoints, it->second.RequireSkill[0], it->second.RequireSkill[1], it->second.Skill, it->second.DefValue);
			break;
		}
	}

#if defined(MU_TOUCH_UI)
	// A tap that landed on no skill puts the pinned details away.
	if (tappedThisFrame && this->m_TouchHit == false)
	{
		this->m_TouchSkill = -1;
		this->m_TouchTick = 0;
	}
#endif

	return true;
}

bool SEASON3B::CNewUIMasterSkillTree::CheckAttributeArea(int group, int index, int reqPoint, unsigned int ReqSkill1, unsigned int ReqSkill2, unsigned int Skill, int defValue)
{
	if (!InBounds(group, 0, 2))
	{
		return 0;
	}

	SKILL_ATTRIBUTE* lpskill = &SkillAttribute[Skill];

	if (lpskill == NULL)
	{
		return 0;
	}

	int tindex = (index - 1) % 4;

	int posX = (int)((double)this->categoryPos[group].x + tindex * 49.0);

	int posY = (int)((double)this->categoryPos[group].y + (lpskill->SkillRank - 1) * 41.0);

	if (SEASON3B::IsPress(1) && SEASON3B::CheckMouseIn(posX + 8, posY + 5, 20, 28))
	{
		// Points are only spent on the live page.
		if (this->IsPagePreview())
		{
			PlayBuffer(SOUND_CLICK01);

			PgToast("Preview only - activate this page to spend points.");

			return 1;
		}

#if defined(MU_TOUCH_UI)
		// No hover on a touch screen, so a tap used to add a point straight
		// away with no way to read the skill first. First tap: select it and
		// show its details (RenderToolTip). A second tap on it within
		// kDoubleTapMs: carry on below and add the point.
		const DWORD kDoubleTapMs = 600;
		const DWORD now = GetTickCount();

		this->m_TouchHit = true;

		if (this->m_TouchSkill != (int)Skill || this->m_TouchTick == 0 || (now - this->m_TouchTick) > kDoubleTapMs)
		{
			this->m_TouchSkill = (int)Skill;
			this->m_TouchTick = now;
			PlayBuffer(SOUND_CLICK01);
			return 1;
		}

		// Used up: the next point needs another double tap, not one more tap.
		this->m_TouchTick = 0;
#endif

		std::map<DWORD, CSkillTreeInfo>::iterator it = this->map_skilltreeinfo.find(Skill);
		std::map<BYTE, _MASTER_SKILLTREE_DATA>::iterator bp = this->map_masterData.find(index);

		int skillPoint = 0;
		int mMaxPoint = 0;
		if (it != this->map_skilltreeinfo.end())
		{
			skillPoint = it->second.GetSkillPoint();
		}
		if (bp != this->map_masterData.end())
		{
			mMaxPoint = bp->second.MaxLevel;
		}
		PlayBuffer(SOUND_CLICK01);


		if (this->CheckSkillPoint(Master_Level_Data.nMLevelUpMPoint, index, mMaxPoint, ReqSkill1, ReqSkill2, Skill, defValue, skillPoint) == false)
		{
			//g_ConsoleDebug->Write(MCD_ERROR, "CheckAttributeArea 1 ");
			return 1;
		}

		if (this->CheckParentSkill(ReqSkill1, ReqSkill2) == false || this->CheckRankPoint(group, lpskill->SkillRank, skillPoint) == false || this->CheckBeforeSkill(Skill, skillPoint) == false)
		{
			//g_ConsoleDebug->Write(MCD_ERROR, "%d %d %d", this->CheckParentSkill(ReqSkill1, ReqSkill2), this->CheckRankPoint(group, lpskill->SkillRank, skillPoint), this->CheckBeforeSkill(Skill, skillPoint));
			SEASON3B::CreateOkMessageBox(GlobalText[3327]);

			return 1;
		}

		if (this->CheckBeforeSkill(Skill, skillPoint) == false/*g_csItemOption.Special_Option_Check(Skill) == false*/)
		{
			//g_ConsoleDebug->Write(MCD_ERROR, "CheckAttributeArea 3");
			SEASON3B::CreateOkMessageBox(GlobalText[3336]);
			return 1;
		}
	//	g_ConsoleDebug->Write(MCD_RECEIVE, "index %d Skill %d reqPoint %d", index, Skill, reqPoint);

		this->SetConsumePoint(reqPoint);

		this->SetCurSkillID(Skill);

		//SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CMaster_Level_Interface));

		SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CMasterSkillPointMsgBoxLayout));

		MouseLButton = 0;
		
		MouseLButtonPop = 0;
		
		MouseLButtonPush = 0;
		
		
	}

	return 1;
}
//#include "CBInterface.h"
bool SEASON3B::CNewUIMasterSkillTree::CheckSkillPoint(WORD mLevelUpPoint, int index, int reqPoint, int ReqSkill1, int ReqSkill2, int Skill, int defValue, WORD skillPoint)
{
	//g_ConsoleDebug->Write(MCD_RECEIVE, "CheckSkillPoint %d ~ %d ", mLevelUpPoint, reqPoint);
	//gInterface.DrawMessage(1, "CheckSkillPoint %d %d", mLevelUpPoint, reqPoint);
	if (skillPoint == reqPoint)
	{
		SEASON3B::CreateOkMessageBox(GlobalText[3326]);
		return 0;
	}

	if (mLevelUpPoint >= 1)
	{
		return 1;
	}

	char Buffer[358];

	memset(Buffer, 0, sizeof(Buffer));

	sprintf(Buffer, GlobalText[3326], reqPoint - mLevelUpPoint);

	SEASON3B::CreateOkMessageBox(Buffer);

	return 0;
}

bool SEASON3B::CNewUIMasterSkillTree::CheckParentSkill(DWORD reqSkill1, DWORD reqSkill2)
{
	if (reqSkill1 == 0)
	{
		return 1;
	}

	std::map<DWORD, CSkillTreeInfo>::iterator it = this->map_skilltreeinfo.find(reqSkill1);

	if (it == this->map_skilltreeinfo.end())
	{
		return 0;
	}

	if (it->second.GetSkillPoint() < 10)
	{
		return 0;
	}

	if (reqSkill2 == 0)
	{
		return 1;
	}

	it = this->map_skilltreeinfo.find(reqSkill2);

	if (it == this->map_skilltreeinfo.end())
	{
		return 0;
	}

	if (it->second.GetSkillPoint() < 10)
	{
		return 0;
	}

	return 1;
}

bool SEASON3B::CNewUIMasterSkillTree::CheckRankPoint(BYTE group, BYTE rank, BYTE skillPoint)
{
	//return 1;
	if (this->skillPoint[group][rank] < skillPoint)
	{
		this->skillPoint[group][rank] = skillPoint;
	}

	if (rank == 1)
	{
		return 1;
	}

	return this->skillPoint[group][rank-1] >= 10;	//PUEDE SER +2 SI HAY ALGUN ERROR
}

bool SEASON3B::CNewUIMasterSkillTree::CheckBeforeSkill(int skill, BYTE skillPoint)
{

	int Index = this->GetBeforeSkillID(skill);

	if (Index == 0)
	{
		return 1;
	}

	SKILL_ATTRIBUTE* lpSkill = &SkillAttribute[Index];

	if (lpSkill == NULL)
	{
		return 0;
	}

	if (lpSkill->SkillUseType == 4)
	{
		return 1;
	}

	if (skillPoint != 0)
	{
		return 1;
	}

	for (int i = 0; i < MAX_MAGIC; i++)
	{
		if (CharacterAttribute->Skill[i] == Index)
		{
			return 1;
		}
	}

	return 0;
}

int SEASON3B::CNewUIMasterSkillTree::GetBeforeSkillID(int index)
{
	return SkillAttribute[index].SkillBrand;
}

int SEASON3B::CNewUIMasterSkillTree::GetBaseSkillID(int index)
{
	if (index >= MAX_SKILLS)
	{
		return 0;
	}

	while (SkillAttribute[index].SkillBrand != 0 && SkillAttribute[index].SkillBrand != 75)
	{
		index = SkillAttribute[index].SkillBrand;
	}

	return index;
}

void SEASON3B::CNewUIMasterSkillTree::SkillUpgrade(int index, char point, float value, float nextvalue)
{
	std::map<BYTE, _MASTER_SKILLTREE_DATA>::iterator it = this->map_masterData.find(index);

	int realSkill = 0;

	if (it != this->map_masterData.end())
	{
		realSkill = it->second.Skill;
	}

	std::map<DWORD, CSkillTreeInfo>::iterator sit = this->map_skilltreeinfo.find(realSkill);

	if (sit != this->map_skilltreeinfo.end())
	{
		this->map_skilltreeinfo.erase(sit);
	}

	CSkillTreeInfo skillinfo;

	skillinfo.SetSkillTreeInfo(point, value, nextvalue);

	if (this->map_skilltreeinfo.insert(std::pair< DWORD, CSkillTreeInfo>(realSkill, skillinfo)).second == 0)
	{
		return;
	}

	this->CategoryPoint[it->second.Group] += it->second.RequiredPoints;
}

int SEASON3B::CNewUIMasterSkillTree::FindRecommendedSkill(int* pointsToAdd)
{
	int points = 0;

	if (pointsToAdd != NULL)
	{
		*pointsToAdd = 0;
	}

	// Nothing to spend, nothing to recommend - the frame comes back with the
	// next master level's points.
	if (Master_Level_Data.nMLevelUpMPoint <= 0)
	{
		return -1;
	}

	// Most popular first; the first one this player has not yet taken to the
	// level the top players did wins.
	for (size_t n = 0; n < g_MasterRecommend.size(); n++)
	{
		const int step = this->ResolveRecommendStep(g_MasterRecommend[n].Skill, g_MasterRecommend[n].Level, 0, &points);

		if (step >= 0)
		{
			if (pointsToAdd != NULL)
			{
				*pointsToAdd = points;
			}

			return step;
		}
	}

	return -1;
}

// The skill to put a point into on the way to `skill`: the skill itself when it
// can be raised now, or - when its parent skills are not at 10 yet - the first
// parent on that path that can. -1 when it is maxed, not in this class's tree,
// or blocked by something a tree click cannot fix (the base skill not learned
// from its book, or not enough points in the rank below).
int SEASON3B::CNewUIMasterSkillTree::ResolveRecommendStep(int skill, int wantLevel, int depth, int* pointsToAdd)
{
	if (skill <= 0 || skill >= MAX_SKILLS || depth > 8)
	{
		return -1;
	}

	for (std::map<BYTE, _MASTER_SKILLTREE_DATA>::iterator it = this->map_masterData.begin(); it != this->map_masterData.end(); it++)
	{
		if (it->second.Skill != skill)
		{
			continue;
		}

		int skillPoint = 0;

		std::map<DWORD, CSkillTreeInfo>::iterator sit = this->map_skilltreeinfo.find(skill);

		if (sit != this->map_skilltreeinfo.end())
		{
			skillPoint = sit->second.GetSkillPoint();
		}

		// Done once it is where the top players took it, rounded up to whole
		// steps of 10 - the step that unlocks the next skill - so the advice is
		// +10 or +20, never +9 (never past the max either).
		int target = wantLevel;

		if (target <= 0)
		{
			target = it->second.MaxLevel;
		}
		else
		{
			target = ((target + 9) / 10) * 10;
		}

		if (target > it->second.MaxLevel)
		{
			target = it->second.MaxLevel;
		}

		if (skillPoint >= target)
		{
			return -1;
		}

		if (this->CheckParentSkill(it->second.RequireSkill[0], it->second.RequireSkill[1]) == false)
		{
			for (int r = 0; r < 2; r++)
			{
				// A parent only has to reach 10 to unlock what is after it.
				const int step = this->ResolveRecommendStep(it->second.RequireSkill[r], 10, depth + 1, pointsToAdd);

				if (step >= 0)
				{
					return step;
				}
			}

			return -1;
		}

		if (this->CheckBeforeSkill(skill, skillPoint) == false || this->CheckRankPoint(it->second.Group, SkillAttribute[skill].SkillRank, skillPoint) == false)
		{
			return -1;
		}

		if (pointsToAdd != NULL)
		{
			*pointsToAdd = target - skillPoint;
		}

		return skill;
	}

	return -1;
}

bool SEASON3B::CNewUIMasterSkillTree::IsLearnSkill(int index)
{
	std::map<DWORD, CSkillTreeInfo>::iterator it = this->map_skilltreeinfo.find(index);

	if (it == this->map_skilltreeinfo.end())
	{
		return false;
	}

	return true;
}

float SEASON3B::CNewUIMasterSkillTree::GetSkillValue(int index)
{
	std::map<DWORD, CSkillTreeInfo>::iterator it = this->map_skilltreeinfo.find(index);

	if (it == this->map_skilltreeinfo.end())
	{
		return 0.0f;
	}

	return it->second.GetSkillValue();
}

float SEASON3B::CNewUIMasterSkillTree::GetSkillCumulativeValue(int index, BYTE damage)
{
	if (!index || index < 300 || index > 608 || damage == 0 || damage >= 5)
	{
		return 0.0;
	}

	float value = 0.0;

	SKILL_ATTRIBUTE* lpSkill;

	for (int i = index; i != 75; i = lpSkill->SkillBrand)
	{
		if (i == 0)
		{
			break;
		}

		lpSkill = &SkillAttribute[i];

		if (damage == lpSkill->IsDamage)
		{
			value += this->GetSkillValue(i);
		}
	}

	return value;
}

BYTE SEASON3B::CNewUIMasterSkillTree::GetSkillLevel(int index)
{
	std::map<DWORD, CSkillTreeInfo>::iterator it = this->map_skilltreeinfo.find(index);

	if (it == this->map_skilltreeinfo.end())
	{
		return 0;
	}

	return it->second.GetSkillPoint();
}

void SEASON3B::CNewUIMasterSkillTree::RegisterSkillInCharacterAttribute(int index)
{
	if (index < 300 || index > 608)
	{
		return;
	}

	SKILL_ATTRIBUTE* lpskill = &SkillAttribute[index];

	if (lpskill->SkillUseType == 4)
	{
		for (int i = 0; i < MAX_MAGIC; i++)
		{
			if (lpskill->SkillBrand != CharacterAttribute->Skill[i])
			{
				continue;
			}

			CharacterAttribute->Skill[i] = index;

			return;
		}

		return;
	}

	for (int i = 0; i < MAX_MAGIC; i++)
	{
		if (CharacterAttribute->Skill[i] != 0)
		{
			continue;
		}

		CharacterAttribute->Skill[i] = index;

		return;
	}
}

void SEASON3B::CNewUIMasterSkillTree::SetConsumePoint(BYTE ConsumePoint)
{
	this->ConsumePoint = ConsumePoint;
}

void SEASON3B::CNewUIMasterSkillTree::SetCurSkillID(int index)
{
	this->CurSkillID = index;
}

void SEASON3B::CNewUIMasterSkillTree::ClearSkillTreeData()
{
	this->map_masterData.clear();
}

void SEASON3B::CNewUIMasterSkillTree::ClearSkillTooltipData()
{
	this->map_masterSkillToolTip.clear();
}

#endif















