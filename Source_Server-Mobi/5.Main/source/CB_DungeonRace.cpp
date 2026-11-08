// CB_DungeonRace.cpp: see CB_DungeonRace.h. Code-drawn window + counter, same helpers as CB_KingOfKira.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CB_DungeonRace.h"
#include "CB_DuelRanked.h"
#include "CBInterface.h"
#include "NewUIBCustomMenu.h"
#include "NewUISystem.h"
#include "ZzzOpenglUtil.h"
#include "ZzzCharacter.h"
#include "ZzzScene.h"
#include "wsclientinline.h"
#include "Input.h"
#include <stdarg.h>
#include <stdio.h>

CB_DungeonRace* gCB_DungeonRace = NULL;

// Proves this Main.exe is the Dungeon Race build the moment the process starts (before login / any click).
struct DungeonRaceStartLog { DungeonRaceStartLog() { DungeonRaceLog("Main.exe started (Dungeon Race client build)"); } };
static DungeonRaceStartLog s_DungeonRaceStartLog;

// Packet layouts, identical to PMSG_DR_* in the GameServer DungeonRace.h (packed, fixed width only).
#pragma pack(push,1)
struct DR_OPEN_RECV { BYTE c, size, head, sub; BYTE State, Mine, Winners, Pad; WORD RemainSec, Players, MinPlayers, MaxPlayers; };
struct DR_HUD_RECV { BYTE c, size, head, sub; BYTE State, Mine, Place, Finished; WORD RemainSec, Players; };
#pragma pack(pop)

static_assert(sizeof(DR_OPEN_RECV) == 4+4+8, "DR open");
static_assert(sizeof(DR_HUD_RECV) == 4+4+4, "DR hud");

namespace
{
	const DWORD kWhite = RankedUi::Col(255, 255, 255);
	const DWORD kGold = RankedUi::Col(255, 200, 80);
	const DWORD kGreen = RankedUi::Col(120, 230, 130);
	const DWORD kRed = RankedUi::Col(255, 110, 110);
	const DWORD kDim = RankedUi::Col(160, 160, 166);

	const float kWinW = 290.0f, kWinH = 190.0f, kWinY = 110.0f;
	const float kHudW = 230.0f, kHudH = 44.0f, kHudY = 6.0f;
	const DWORD kHudStaleMs = 3500;

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
}

CB_DungeonRace::CB_DungeonRace()
{
	m_Click = false;
	m_WasDown = false;
	m_Open = false;
	m_State = m_Mine = m_Winners = 0;
	m_OpenRemain = m_Players = m_MinPlayers = m_MaxPlayers = 0;
	m_OpenTick = 0;
	m_HudTick = 0;
	m_HudState = m_HudMine = m_HudPlace = m_HudFinished = 0;
	m_HudRemain = m_HudPlayers = 0;
	DungeonRaceLog("client with Dungeon Race window started");
}

void CB_DungeonRace::RecvOpen(const BYTE* lpMsg, int size)
{
	if (size < (int)sizeof(DR_OPEN_RECV))
	{
		return;
	}

	const DR_OPEN_RECV* p = (const DR_OPEN_RECV*)lpMsg;
	DungeonRaceLog("RECV window packet: state %d mine %d players %d remain %d", (int)p->State, (int)p->Mine, (int)p->Players, (int)p->RemainSec);

	m_Open = true;
	m_State = p->State;
	m_Mine = p->Mine;
	m_Winners = p->Winners;
	m_OpenRemain = p->RemainSec;
	m_Players = p->Players;
	m_MinPlayers = p->MinPlayers;
	m_MaxPlayers = p->MaxPlayers;
	m_OpenTick = GetTickCount();
}

void CB_DungeonRace::RecvHud(const BYTE* lpMsg, int size)
{
	if (size < (int)sizeof(DR_HUD_RECV))
	{
		return;
	}

	const DR_HUD_RECV* p = (const DR_HUD_RECV*)lpMsg;
	if (m_HudTick == 0 || (DWORD)(GetTickCount() - m_HudTick) > kHudStaleMs) DungeonRaceLog("RECV counter packet: state %d mine %d place %d finished %d remain %d", (int)p->State, (int)p->Mine, (int)p->Place, (int)p->Finished, (int)p->RemainSec);

	// the first counter after joining: the server moved us to the start, the window is done
	if (p->Mine != 0 && (m_HudTick == 0 || (DWORD)(GetTickCount() - m_HudTick) > kHudStaleMs))
	{
		m_Open = false;
	}

	m_HudTick = GetTickCount();
	m_HudState = p->State;
	m_HudMine = p->Mine;
	m_HudPlace = p->Place;
	m_HudFinished = p->Finished;
	m_HudRemain = p->RemainSec;
	m_HudPlayers = p->Players;
}

void CB_DungeonRace::SendAction(BYTE action)
{
	DungeonRaceLog("SEND action %d (0 join, 1 leave)", (int)action);
	// C1:D3:67, UNENCRYPTED like every 0xD3 packet (see wsclientinline.h SendClientBuild).
	CStreamPacketEngine spe;
	spe.Init(0xC1, 0xD3);
	spe << (BYTE)0x67;
	spe << (BYTE)action;
	spe.Send();
}

void CB_DungeonRace::DrawOpenWindow()
{
	const float x = (float)MAX_WIN_WIDTH * 0.5f - kWinW * 0.5f;
	const float y = kWinY;

	PublishRect(eWindowDungeonRace, true, x, y, kWinW, kWinH);

	if (RankedUi::Inside(x, y, kWinW, kWinH) && g_pBCustomMenuInfo != NULL)
	{
		g_pBCustomMenuInfo->SetBlockCur(TRUE);
	}

	RankedUi::Bar(x, y, kWinW, kWinH, RankedUi::Col(12, 12, 16, 240));
	RankedUi::Outline(x, y, kWinW, kWinH, RankedUi::Col(214, 170, 70));
	RankedUi::Outline(x + 2.0f, y + 2.0f, kWinW - 4.0f, kWinH - 4.0f, RankedUi::Col(70, 58, 34));
	RankedUi::DrawDiamond(x + 16.0f, y + 15.0f, 4.0f, kGold);
	RankedUi::DrawDiamond(x + kWinW - 16.0f, y + 15.0f, 4.0f, kGold);
	RankedUi::Text(g_hFontBold, x, y + 9.0f, kWinW, RT3_SORT_CENTER, kGold, "DUNGEON RACE");

	char clock[16];
	char line[120];
	const int left = (int)m_OpenRemain - (int)((GetTickCount() - m_OpenTick) / 1000);

	const char* status = "No race is scheduled.";
	DWORD color = kDim;

	if (m_State == 2)
	{
		Clock(left, clock, sizeof(clock));
		snprintf(line, sizeof(line), "Registration is open - closes in %s", clock);
		status = line;
		color = kGreen;
	}
	else if (m_State == 3)
	{
		status = "Registration closed - the race is about to start.";
		color = kRed;
	}
	else if (m_State == 4)
	{
		status = "The race is running.";
		color = kRed;
	}
	else if (m_State == 1 && m_OpenRemain > 0)
	{
		Clock(left, clock, sizeof(clock));
		snprintf(line, sizeof(line), "Next race opens in %s", clock);
		status = line;
	}

	RankedUi::Text(g_hFont, x, y + 32.0f, kWinW, RT3_SORT_CENTER, color, status);

	char info[120];
	snprintf(info, sizeof(info), "Racers: %d  (min %d, max %d)", (int)m_Players, (int)m_MinPlayers, (int)m_MaxPlayers);
	RankedUi::Text(g_hFont, x, y + 52.0f, kWinW, RT3_SORT_CENTER, kWhite, info);

	snprintf(info, sizeof(info), "Reach Dungeon 3 first - the top %d win a prize.", (int)m_Winners);
	RankedUi::Text(g_hFont, x, y + 70.0f, kWinW, RT3_SORT_CENTER, kDim, info);

	RankedUi::Text(g_hFont, x + 12.0f, y + 90.0f, kWinW - 24.0f, RT3_SORT_CENTER, kDim, "Wings, weapons, rings, pendants, Uniria and Dinorant");
	RankedUi::Text(g_hFont, x + 12.0f, y + 104.0f, kWinW - 24.0f, RT3_SORT_CENTER, kDim, "come off for the race (kept in your bag). No warping!");

	if (m_Mine != 0)
	{
		RankedUi::Text(g_hFontBold, x, y + 124.0f, kWinW, RT3_SORT_CENTER, kGreen, "You are registered. Wait at the start.");
	}

	const float btnY = y + kWinH - 40.0f;
	const float btnW = (kWinW - 32.0f - 20.0f) / 3.0f;
	const float joinX = x + 16.0f;
	const float leaveX = joinX + btnW + 10.0f;
	const float closeX = leaveX + btnW + 10.0f;

	const bool canJoin = (m_State == 2 && m_Mine == 0);
	const bool canLeave = (m_State == 2 && m_Mine != 0);

	RankedUi::DrawButton(joinX, btnY, btnW, 30.0f, "Join", 1, canJoin, true);
	RankedUi::DrawButton(leaveX, btnY, btnW, 30.0f, "Leave", 2, canLeave, true);
	RankedUi::DrawButton(closeX, btnY, btnW, 30.0f, "Close", 0, true, true);

	if (m_Click)
	{
		if (canJoin && RankedUi::Inside(joinX, btnY, btnW, 30.0f))
		{
			SendAction(0);
		}
		else if (canLeave && RankedUi::Inside(leaveX, btnY, btnW, 30.0f))
		{
			SendAction(1);
		}
		else if (RankedUi::Inside(closeX, btnY, btnW, 30.0f))
		{
			m_Open = false;
		}
	}
}

void CB_DungeonRace::DrawHud()
{
	const float x = (float)MAX_WIN_WIDTH * 0.5f - kHudW * 0.5f;
	const float y = kHudY;

	RankedUi::Bar(x, y, kHudW, kHudH, RankedUi::Col(12, 12, 16, 200));
	RankedUi::Outline(x, y, kHudW, kHudH, RankedUi::Col(214, 170, 70));

	char clock[16];
	Clock((int)m_HudRemain, clock, sizeof(clock));

	char title[64];
	const char* what = "DUNGEON RACE";

	if (m_HudState == 2) what = "DUNGEON RACE - registration";
	else if (m_HudState == 3) what = "DUNGEON RACE - starts in";
	else if (m_HudState == 4) what = "DUNGEON RACE - time left";

	snprintf(title, sizeof(title), "%s  %s", what, clock);
	RankedUi::Text(g_hFontBold, x, y + 4.0f, kHudW, RT3_SORT_CENTER, kGold, title);

	char sub[80];
	DWORD color = kDim;

	if (m_HudMine == 2)
	{
		snprintf(sub, sizeof(sub), "You finished in place %d!", (int)m_HudPlace);
		color = kGreen;
	}
	else if (m_HudState == 4)
	{
		snprintf(sub, sizeof(sub), "Finished: %d   Racers: %d", (int)m_HudFinished, (int)m_HudPlayers);
		color = kWhite;
	}
	else
	{
		snprintf(sub, sizeof(sub), "Racers: %d - stay at the start", (int)m_HudPlayers);
	}

	RankedUi::Text(g_hFont, x, y + 24.0f, kHudW, RT3_SORT_CENTER, color, sub);
}

void CB_DungeonRace::DrawWindow()
{
	const DWORD now = GetTickCount();

	const bool down = PrimaryDown();
	m_Click = down && (m_WasDown == false);
	m_WasDown = down;

	const bool inWorld = (Hero != NULL && SceneFlag == MAIN_SCENE);

	if (inWorld == false)
	{
		m_Open = false;
		m_HudTick = 0;
	}

	const bool showHud = (m_HudTick != 0 && (DWORD)(now - m_HudTick) < kHudStaleMs && inWorld);

	if (m_Open == false)
	{
		PublishRect(eWindowDungeonRace, false, 0, 0, 0, 0);
	}

	static bool lastOpen = false;
	if (m_Open != lastOpen) { lastOpen = m_Open; DungeonRaceLog("window %s", m_Open ? "SHOWN" : "closed"); }

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
		DrawOpenWindow();
	}
}

void DungeonRaceLog(const char* fmt, ...)
{
#if defined(_WIN32) && !defined(__ANDROID__)
	// PC client only: a plain file next to Main.exe, so it works without the debug console.
	char buff[512];

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buff, sizeof(buff), fmt, arg);
	va_end(arg);
	buff[sizeof(buff) - 1] = '\0';

	SYSTEMTIME st;
	GetLocalTime(&st);

	// next to the exe (not the working directory, which the launcher may change); fall back to the working dir
	char path[MAX_PATH + 32] = {0};
	DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
	FILE* f = NULL;

	if (n > 0 && n < MAX_PATH)
	{
		char* slash = strrchr(path, '\\');

		if (slash != NULL)
		{
			strcpy(slash + 1, "DungeonRaceClient.log");
			f = fopen(path, "a");
		}
	}

	if (f == NULL)
	{
		f = fopen("DungeonRaceClient.log", "a");
	}

	if (f != NULL)
	{
		fprintf(f, "%02d:%02d:%02d.%03d %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buff);
		fclose(f);
	}
#else
	(void)fmt;
#endif
}
