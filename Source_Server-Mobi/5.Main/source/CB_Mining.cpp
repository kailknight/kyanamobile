// CB_Mining.cpp: see CB_Mining.h. Code-drawn window + event line, same helpers as CB_DungeonRace.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CB_Mining.h"
#include "CB_DuelRanked.h"
#include "CBInterface.h"
#include "NewUIBCustomMenu.h"
#include "NewUISystem.h"
#include "NewUIMessageBox.h"
#include "NewUICustomMessageBox.h"
#include "ZzzOpenglUtil.h"
#include "ZzzCharacter.h"
#include "ZzzInfomation.h"
#include "ZzzScene.h"
#include "DSPlaySound.h"
#include "wsclientinline.h"
#include "Input.h"
#include "Other.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

CB_Mining* gCB_Mining = NULL;

void PlayMiningAnimation(); // ZzzInterface.cpp

// Packet layouts, identical to PMSG_CM_* in the GameServer CustomMining.h (packed, fixed width only).
#pragma pack(push,1)
struct CM_STATE_RECV { BYTE c, size, head, sub; BYTE Code, Class, Step, Steps, Err; WORD JewelItem, Pending, NextTotal, Chance, AttemptMs; BYTE DurCost, PickDur; };
struct CM_HUD_RECV { BYTE c, size, head, sub; BYTE Phase, Pad; WORD RemainSec; };
#pragma pack(pop)

static_assert(sizeof(CM_STATE_RECV) == 4+5+10+2, "CM state");
static_assert(sizeof(CM_HUD_RECV) == 4+4, "CM hud");

enum { CMC_ASK = 0, CMC_CONTINUE = 1, CMC_MINING = 2, CMC_FAIL = 3, CMC_CASHED = 4, CMC_ERROR = 5, CMC_CLOSE = 6 };

// The progress bar is the Castle Siege "crown switch" bar (CProgressMsgBox): it fills over the attempt and closes itself.
static DWORD g_MiningBarMs = 3600;
static char g_MiningBarText[96] = "Mining...";

namespace SEASON3B
{
	class CMiningProgressLayout : public TMsgBoxLayout<CProgressMsgBox>
	{
	public:
		bool SetLayout()
		{
			CProgressMsgBox* pMsgBox = GetMsgBox();
			if (0 == pMsgBox)
				return false;

			if (false == pMsgBox->Create(g_MiningBarMs))
				return false;

			pMsgBox->AddMsg(g_MiningBarText);

			return true;
		}
	};
}

namespace
{
	const DWORD kWhite = RankedUi::Col(255, 255, 255);
	const DWORD kGold = RankedUi::Col(255, 200, 80);
	const DWORD kGreen = RankedUi::Col(120, 230, 130);
	const DWORD kRed = RankedUi::Col(255, 110, 110);
	const DWORD kDim = RankedUi::Col(160, 160, 166);
	const DWORD kStone[3] = { RankedUi::Col(90, 170, 255), RankedUi::Col(195, 120, 255), RankedUi::Col(255, 205, 70) };

	const float kWinW = 310.0f, kWinH = 206.0f, kWinY = 110.0f;
	const float kHudW = 250.0f, kHudH = 44.0f, kHudY = 6.0f;
	const DWORD kHudStaleMs = 3500;

	// English defaults: used when the language file has no [Text_Mining] section (Text_Eng.ini carries them, other languages can translate)
	const char* const kDefault[32] =
	{
		/* 0 */ "TARKAN MINING",
		/* 1 */ "Mine this stone?",
		/* 2 */ "A win raises the jewels, a loss loses them all.",
		/* 3 */ "Mine",
		/* 4 */ "Cancel",
		/* 5 */ "Continue",
		/* 6 */ "Cash out",
		/* 7 */ "OK",
		/* 8 */ "Blue Mining Stone",
		/* 9 */ "Purple Mining Stone",
		/* 10 */ "Gold Mining Stone",
		/* 11 */ "Jewels won: %d x %s",
		/* 12 */ "Step %d of %d",
		/* 13 */ "Next success: %d jewels",
		/* 14 */ "Chance: %.2f%%",
		/* 15 */ "Pick Axe durability: -%d per try (you have %d)",
		/* 16 */ "First success gives %d jewels",
		/* 17 */ "Mining the %s...",
		/* 18 */ "The stone crumbled! You lost %d x %s.",
		/* 19 */ "The stone crumbled!",
		/* 20 */ "You received %d x %s.",
		/* 21 */ "Last step reached: paid out.",
		/* 22 */ "Cash out now or press Continue for more.",
		/* 23 */ "Blue stones",
		/* 24 */ "Purple stones",
		/* 25 */ "Gold stones",
		/* 26 */ "TARKAN MINING EVENT",
		/* 27 */ "Mine the stones on Tarkan with your Pick Axe!",
		/* 28 */ "",
		/* 29 */ "",
		/* 30 */ "",
		/* 31 */ ""
	};

	const char* T(int i)
	{
		if (i < 0 || i >= 32) return "";
		const char* s = gOther.Text_Mining[i];
		if (s[0] == '\0' || strcmp(s, "Null") == 0) return kDefault[i];
		return s;
	}

	bool PrimaryDown()
	{
#if defined(MU_TOUCH_UI)
		return SEASON3B::IsPress(VK_LBUTTON) || CInput::Instance().IsLBtnDn();
#else
		return ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
#endif
	}

	void PublishRect(int id, bool show, float x, float y, float w, float h)
	{
		InterfaceObject& d = gInterface.Data[id];
		d.OnShow = show;
		if (show)
		{
			d.X = x;
			d.Y = y;
			d.Width = w;
			d.Height = h;
			d.FirstLoad = true;
		}
	}

	void Clock(int seconds, char* out, size_t cb)
	{
		if (seconds < 0) seconds = 0;
		snprintf(out, cb, "%02d:%02d", seconds / 60, seconds % 60);
	}

	const char* JewelName(int item)
	{
		if (item > 0 && item < MAX_ITEM && ItemAttribute != NULL && ItemAttribute[item].Name[0] != '\0')
		{
			return ItemAttribute[item].Name;
		}
		return "jewels";
	}

	// Gemstone (14,41) has its own pick-up sound, like the other jewels have theirs (WSclient.cpp, item received)
	void PlayJewelSound(int item)
	{
		PlayBuffer((item == ITEM_POTION + 41) ? SOUND_JEWEL02 : SOUND_JEWEL01);
	}

	void ChatError(const char* text)
	{
		if (g_pChatListBox != NULL)
		{
			g_pChatListBox->AddText("", text, SEASON3B::TYPE_ERROR_MESSAGE);
		}
	}
}

CB_Mining::CB_Mining()
{
	m_Click = false;
	m_WasDown = false;
	m_Open = false;
	m_Code = m_Class = m_Step = m_Steps = 0;
	m_JewelItem = m_Pending = m_NextTotal = m_Chance = m_AttemptMs = 0;
	m_DurCost = m_PickDur = 0;
	m_Mining = false;
	m_MineEnd = 0;
	m_WaitResult = false;
	m_WaitTick = 0;
	m_HudTick = 0;
	m_HudPhase = 0;
	m_HudRemain = 0;
}

void CB_Mining::StartBar()
{
	g_MiningBarMs = (m_AttemptMs >= 500) ? (DWORD)m_AttemptMs : 3600;

	const char* stone = T(8 + (m_Class > 2 ? 0 : m_Class));
	snprintf(g_MiningBarText, sizeof(g_MiningBarText), T(17), stone);

	SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CMiningProgressLayout));
}

void CB_Mining::RecvState(const BYTE* lpMsg, int size)
{
	if (size < (int)sizeof(CM_STATE_RECV))
	{
		return;
	}

	const CM_STATE_RECV* p = (const CM_STATE_RECV*)lpMsg;

	if (p->Code == CMC_ERROR || p->Code == CMC_CLOSE)
	{
		m_Open = false;
		m_Mining = false;
		m_WaitResult = false;
		return;
	}

	m_Code = p->Code;
	m_Class = p->Class;
	m_Step = p->Step;
	m_Steps = p->Steps;
	m_JewelItem = p->JewelItem;
	m_Pending = p->Pending;
	m_NextTotal = p->NextTotal;
	m_Chance = p->Chance;
	m_AttemptMs = p->AttemptMs;
	m_DurCost = p->DurCost;
	m_PickDur = p->PickDur;

	char line[160];

	switch (p->Code)
	{
	case CMC_ASK:
	case CMC_CONTINUE:
		m_Open = true;
		m_Mining = false;

		if (p->Code == CMC_CONTINUE && m_WaitResult)
		{
			PlayJewelSound(m_JewelItem); // the attempt won
		}

		m_WaitResult = false;
		break;

	case CMC_MINING:
		m_Open = false;
		m_Mining = true;
		m_WaitResult = false;
		m_MineEnd = GetTickCount() + ((m_AttemptMs >= 500) ? (DWORD)m_AttemptMs : 3600);
		PlayMiningAnimation();
		StartBar();
		break;

	case CMC_FAIL:
		m_Open = true;
		m_Mining = false;
		m_WaitResult = false;
		PlayBuffer(SOUND_MIX01); // the Chaos machine failure sounds
		PlayBuffer(SOUND_BREAK01);

		if (m_Pending > 0)
		{
			snprintf(line, sizeof(line), T(18), (int)m_Pending, JewelName(m_JewelItem));
		}
		else
		{
			snprintf(line, sizeof(line), "%s", T(19));
		}

		ChatError(line);
		break;

	case CMC_CASHED:
		m_Open = true;
		m_Mining = false;

		m_WaitResult = false;
		PlayJewelSound(m_JewelItem);
		snprintf(line, sizeof(line), T(20), (int)m_Pending, JewelName(m_JewelItem));
		if (g_pChatListBox != NULL)
		{
			g_pChatListBox->AddText("", line, SEASON3B::TYPE_SYSTEM_MESSAGE);
		}
		break;
	}
}

void CB_Mining::RecvHud(const BYTE* lpMsg, int size)
{
	if (size < (int)sizeof(CM_HUD_RECV))
	{
		return;
	}

	const CM_HUD_RECV* p = (const CM_HUD_RECV*)lpMsg;

	m_HudTick = (p->RemainSec > 0) ? GetTickCount() : 0; // 0 seconds = the event is over: hide the line
	m_HudPhase = p->Phase;
	m_HudRemain = p->RemainSec;
}

void CB_Mining::SendAction(BYTE action)
{
	// C1:D3:6D, UNENCRYPTED like every 0xD3 packet (see wsclientinline.h SendClientBuild).
	CStreamPacketEngine spe;
	spe.Init(0xC1, 0xD3);
	spe << (BYTE)0x6D;
	spe << (BYTE)action;
	spe.Send();
}

void CB_Mining::DrawStateWindow()
{
	const float x = (float)MAX_WIN_WIDTH * 0.5f - kWinW * 0.5f;
	const float y = kWinY;

	PublishRect(eWindowMining, true, x, y, kWinW, kWinH);

	if (RankedUi::Inside(x, y, kWinW, kWinH) && g_pBCustomMenuInfo != NULL)
	{
		g_pBCustomMenuInfo->SetBlockCur(TRUE);
	}

	const int tier = (m_Class > 2) ? 0 : (int)m_Class;
	const DWORD tierColor = kStone[tier];

	RankedUi::Bar(x, y, kWinW, kWinH, RankedUi::Col(12, 12, 16, 240));
	RankedUi::Outline(x, y, kWinW, kWinH, tierColor);
	RankedUi::Outline(x + 2.0f, y + 2.0f, kWinW - 4.0f, kWinH - 4.0f, RankedUi::Col(70, 58, 34));
	RankedUi::DrawDiamond(x + 16.0f, y + 15.0f, 4.0f, tierColor);
	RankedUi::DrawDiamond(x + kWinW - 16.0f, y + 15.0f, 4.0f, tierColor);
	RankedUi::Text(g_hFontBold, x, y + 9.0f, kWinW, RT3_SORT_CENTER, tierColor, T(8 + tier));

	// the jewel this stone gives, drawn as the real item model (once the first win has rolled it)
	if (m_JewelItem > 0 && m_Code != CMC_ASK && g_pNewUISystem != NULL)
	{
		g_pNewUISystem->RenderItem3DFree(x + 14.0f, y + 28.0f, 44.0f, 44.0f, m_JewelItem, 0, 0, 0, false, 1.0f);
		EnableAlphaTest();
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	}

	char line[160];
	const float btnY = y + kWinH - 40.0f;
	const float btnW = (kWinW - 32.0f - 10.0f) / 2.0f;
	const float leftX = x + 16.0f;
	const float rightX = leftX + btnW + 10.0f;

	bool leftEnabled = true;
	const char* leftLabel = "";
	const char* rightLabel = "";

	if (m_Code == CMC_ASK)
	{
		RankedUi::Text(g_hFontBold, x, y + 34.0f, kWinW, RT3_SORT_CENTER, kWhite, T(1));
		RankedUi::Text(g_hFont, x + 10.0f, y + 56.0f, kWinW - 20.0f, RT3_SORT_CENTER, kDim, T(2));

		snprintf(line, sizeof(line), T(16), (int)m_NextTotal);
		RankedUi::Text(g_hFont, x, y + 82.0f, kWinW, RT3_SORT_CENTER, kGold, line);
		snprintf(line, sizeof(line), T(14), (float)m_Chance / 100.0f);
		RankedUi::Text(g_hFont, x, y + 102.0f, kWinW, RT3_SORT_CENTER, kWhite, line);
		snprintf(line, sizeof(line), T(15), (int)m_DurCost, (int)m_PickDur);
		RankedUi::Text(g_hFont, x, y + 122.0f, kWinW, RT3_SORT_CENTER, (m_PickDur >= m_DurCost) ? kDim : kRed, line);

		leftLabel = T(3);
		rightLabel = T(4);
		leftEnabled = (m_PickDur >= m_DurCost);
	}
	else if (m_Code == CMC_CONTINUE)
	{
		snprintf(line, sizeof(line), T(11), (int)m_Pending, JewelName(m_JewelItem));
		RankedUi::Text(g_hFontBold, x, y + 34.0f, kWinW, RT3_SORT_CENTER, kGold, line);
		snprintf(line, sizeof(line), T(12), (int)m_Step, (int)m_Steps);
		RankedUi::Text(g_hFont, x, y + 56.0f, kWinW, RT3_SORT_CENTER, kDim, line);

		if (m_NextTotal > 0)
		{
			snprintf(line, sizeof(line), T(13), (int)m_NextTotal);
			RankedUi::Text(g_hFont, x, y + 82.0f, kWinW, RT3_SORT_CENTER, kGreen, line);
			snprintf(line, sizeof(line), T(14), (float)m_Chance / 100.0f);
			RankedUi::Text(g_hFont, x, y + 102.0f, kWinW, RT3_SORT_CENTER, kWhite, line);
			snprintf(line, sizeof(line), T(15), (int)m_DurCost, (int)m_PickDur);
			RankedUi::Text(g_hFont, x, y + 122.0f, kWinW, RT3_SORT_CENTER, (m_PickDur >= m_DurCost) ? kDim : kRed, line);
			RankedUi::Text(g_hFont, x + 10.0f, y + 146.0f, kWinW - 20.0f, RT3_SORT_CENTER, kDim, T(2));
		}
		else
		{
			RankedUi::Text(g_hFont, x + 10.0f, y + 90.0f, kWinW - 20.0f, RT3_SORT_CENTER, kGreen, T(21));
		}

		leftLabel = T(5);
		rightLabel = T(6);
		leftEnabled = (m_NextTotal > 0 && m_PickDur >= m_DurCost);
	}
	else // FAIL / CASHED: the result, one OK button
	{
		if (m_Code == CMC_FAIL)
		{
			if (m_Pending > 0)
			{
				snprintf(line, sizeof(line), T(18), (int)m_Pending, JewelName(m_JewelItem));
			}
			else
			{
				snprintf(line, sizeof(line), "%s", T(19));
			}
			RankedUi::Text(g_hFontBold, x + 10.0f, y + 70.0f, kWinW - 20.0f, RT3_SORT_CENTER, kRed, line);
		}
		else
		{
			snprintf(line, sizeof(line), T(20), (int)m_Pending, JewelName(m_JewelItem));
			RankedUi::Text(g_hFontBold, x + 10.0f, y + 70.0f, kWinW - 20.0f, RT3_SORT_CENTER, kGreen, line);
		}

		const float okW = 110.0f;
		const float okX = x + kWinW * 0.5f - okW * 0.5f;

		RankedUi::DrawButton(okX, btnY, okW, 30.0f, T(7), 1, true, true);

		if (m_Click && RankedUi::Inside(okX, btnY, okW, 30.0f))
		{
			m_Open = false;
		}

		return;
	}

	RankedUi::DrawButton(leftX, btnY, btnW, 30.0f, leftLabel, 1, leftEnabled, true);
	RankedUi::DrawButton(rightX, btnY, btnW, 30.0f, rightLabel, 2, true, true);

	if (m_Click)
	{
		if (leftEnabled && RankedUi::Inside(leftX, btnY, btnW, 30.0f))
		{
			SendAction(0); // mine / continue: the server answers with the MINING state (animation + bar)
			m_Open = false;
		}
		else if (RankedUi::Inside(rightX, btnY, btnW, 30.0f))
		{
			if (m_Code == CMC_CONTINUE)
			{
				SendAction(1); // cash out: the server answers with CASHED
				m_Open = false;
			}
			else
			{
				SendAction(2); // cancel
				m_Open = false;
			}
		}
	}
}

void CB_Mining::DrawHud()
{
	const float x = (float)MAX_WIN_WIDTH * 0.5f - kHudW * 0.5f;
	const float y = kHudY;

	RankedUi::Bar(x, y, kHudW, kHudH, RankedUi::Col(12, 12, 16, 200));
	RankedUi::Outline(x, y, kHudW, kHudH, kStone[(m_HudPhase >= 1 && m_HudPhase <= 3) ? m_HudPhase - 1 : 2]);

	const int left = (int)m_HudRemain - (int)((GetTickCount() - m_HudTick) / 1000);

	char clock[16];
	Clock(left, clock, sizeof(clock));

	char title[96];
	snprintf(title, sizeof(title), "%s  %s", T(26), clock);
	RankedUi::Text(g_hFontBold, x, y + 4.0f, kHudW, RT3_SORT_CENTER, kGold, title);

	const char* sub = T(27);
	DWORD color = kDim;

	if (m_HudPhase >= 1 && m_HudPhase <= 3)
	{
		sub = T(22 + m_HudPhase);
		color = kStone[m_HudPhase - 1];
	}

	RankedUi::Text(g_hFont, x, y + 24.0f, kHudW, RT3_SORT_CENTER, color, sub);
}

void CB_Mining::DrawWindow()
{
	const DWORD now = GetTickCount();

	const bool down = PrimaryDown();
	m_Click = down && (m_WasDown == false);
	m_WasDown = down;

	const bool inWorld = (Hero != NULL && SceneFlag == MAIN_SCENE);

	if (inWorld == false)
	{
		m_Open = false;
		m_Mining = false;
		m_WaitResult = false;
		m_HudTick = 0;
	}

	// the bar has filled: tell the server, which then applies the result it rolled when the attempt started
	if (m_Mining && (int)(now - m_MineEnd) >= 0)
	{
		m_Mining = false;
		m_WaitResult = true;
		m_WaitTick = now;
		SendAction(3);
	}

	if (m_WaitResult && (DWORD)(now - m_WaitTick) > 15000)
	{
		m_WaitResult = false; // no answer (lag / old server): give up quietly
	}

	const bool showHud = (m_HudTick != 0 && (DWORD)(now - m_HudTick) < kHudStaleMs && inWorld);

	if (m_Open == false)
	{
		PublishRect(eWindowMining, false, 0, 0, 0, 0);
	}

	if (m_Open == false && showHud == false)
	{
		return;
	}

	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (showHud)
	{
		DrawHud();
	}

	if (m_Open)
	{
		DrawStateWindow();
	}
}
