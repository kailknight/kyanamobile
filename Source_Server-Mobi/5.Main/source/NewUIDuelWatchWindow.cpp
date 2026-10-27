// NewUIDuelWatchWindow.cpp: Doorkeeper Titus, the ranked duel hub.
//
// RANKED tab: profile card (rank badge, MMR, rank pill, progress to the next rank, Find Match) and
// one card per arena room with the live ranked fight and a Watch button. NORMAL tab: the four
// local duel channels, exactly as the window always behaved. Everything is drawn in code (see
// CB_DuelRanked.h for the helpers); no textures. Hit areas are finger sized (28+ virtual units).
// See Working Project\MD Files\RankedDuel-PLAN.md section 7.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUIDuelWatchWindow.h"
#include "NewUISystem.h"
#include "NewUICustomMessageBox.h"
#include "CBInterface.h"
#include "ZzzBMD.h"
#include "ZzzEffect.h"
#include "ZzzObject.h"
#include "ZzzInventory.h"
#include "ZzzOpenglUtil.h"
#include "ZzzInterface.h"
#include "ZzzInfomation.h"
#include "ZzzCharacter.h"
#include "wsclientinline.h"
#include "DSPlaySound.h"
#include "DuelMgr.h"
#include "CB_DuelRanked.h"
extern int DisplayWinCDepthBox;
using namespace SEASON3B;

namespace
{
	const float kW = (float)CNewUIDuelWatchWindow::WINDOW_WIDTH;
	const float kH = (float)CNewUIDuelWatchWindow::WINDOW_HEIGHT;
	const float kMargin = 8.0f;
	const float kInnerW = kW - kMargin * 2.0f;

	// Window-local layout.
	const float kCloseX = kW - 36.0f, kCloseY = 2.0f, kCloseW = 32.0f, kCloseH = 30.0f;
	const float kTabY = 38.0f, kTabH = 28.0f, kTabGap = 6.0f;
	const float kHeadY = 72.0f;
	const float kProfY = 90.0f, kProfH = 106.0f;
	const float kRoomY = 200.0f, kRoomH = 53.0f, kRoomGap = 3.0f;
	const float kChanY = 88.0f, kChanH = 76.0f, kChanGap = 6.0f;
	const float kWatchW = 68.0f, kWatchH = 28.0f;

	const DWORD kPollMs = 3000;			// ranked list (and status until one arrives)
	const DWORD kUnavailableMs = 3000;	// no STATUS this long after opening = ranked unavailable

	const DWORD kWhite = RankedUi::Col(255, 255, 255);
	const DWORD kText = RankedUi::Col(236, 228, 208);
	const DWORD kGold = RankedUi::Col(255, 214, 80);
	const DWORD kGoldDim = RankedUi::Col(214, 170, 70);
	const DWORD kDim = RankedUi::Col(150, 148, 142);
	const DWORD kRed = RankedUi::Col(255, 96, 96);
	const DWORD kGreen = RankedUi::Col(120, 230, 130);

	enum { FA_NONE = 0, FA_SEARCH, FA_CANCEL };

	void CardPanel(float x, float y, float w, float h)
	{
		RankedUi::Bar(x, y, w, h, RankedUi::Col(14, 14, 18, 235));
		RankedUi::Outline(x, y, w, h, RankedUi::Col(66, 58, 44));
		RankedUi::Bar(x + 1.0f, y + 1.0f, w - 2.0f, 1.0f, RankedUi::Col(40, 36, 30));
	}

	void TabRect(int i, float& x, float& y, float& w, float& h)
	{
		w = (kInnerW - kTabGap) * 0.5f;
		x = kMargin + (float)i * (w + kTabGap);
		y = kTabY;
		h = kTabH;
	}

	// What the Find Match button says and does right now. Shared by Render and the click code so
	// the two can never disagree.
	int FindButton(char* label, size_t cb, int& style, bool& enabled)
	{
		const RANKED_STATUS_INFO& st = g_DuelMgr.GetRankedStatus();

		style = 3;
		enabled = false;

		// A MSG can say "queued" before the STATUS that confirms it.
		if (g_DuelMgr.IsRankedQueued())
		{
			char clock[16];
			RankedUi::FormatClock(g_DuelMgr.GetRankedQueuedSec(), clock, sizeof(clock));
			snprintf(label, cb, "Cancel (%s searching)", clock);
			style = 2;
			enabled = true;
			return FA_CANCEL;
		}

		if (st.m_bValid == FALSE)
		{
			const bool waited = (int)(GetTickCount() - g_DuelMgr.GetRankedOpenTick()) > (int)kUnavailableMs;
			snprintf(label, cb, "%s", waited ? "Unavailable" : "Loading...");
			return FA_NONE;
		}

		if (st.m_byState == 2)
		{
			snprintf(label, cb, "Match found...");
			return FA_NONE;
		}
		if (st.m_byState == 3)
		{
			snprintf(label, cb, "Moving to the arena...");
			return FA_NONE;
		}
		if (st.m_byState == 4)
		{
			snprintf(label, cb, "In a ranked match");
			return FA_NONE;
		}
		if (st.m_byState == 5)
		{
			snprintf(label, cb, "Watching a fight");
			return FA_NONE;
		}
		if (st.m_byAccepting == 0)
		{
			snprintf(label, cb, "Arena closed");
			return FA_NONE;
		}

		snprintf(label, cb, "Find Match");
		enabled = true;
		return FA_SEARCH;
	}

	bool CanWatch()
	{
		const RANKED_STATUS_INFO& st = g_DuelMgr.GetRankedStatus();
		return !(st.m_byState >= 1 && st.m_byState <= 4);
	}

	bool CanJoinNormal(int i)
	{
		return g_DuelMgr.IsDuelChannelEnabled(i) && g_DuelMgr.IsDuelChannelJoinable(i);
	}

	void DrawTab(float x, float y, float w, float h, const char* label, bool active)
	{
		const bool hover = RankedUi::Inside(x, y, w, h);
		RankedUi::Bar(x, y, w, h, active ? RankedUi::Col(214, 170, 70) : RankedUi::Col(70, 62, 48));
		RankedUi::Bar(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, active ? RankedUi::Col(70, 54, 18) : (hover ? RankedUi::Col(46, 42, 34) : RankedUi::Col(28, 26, 22)));
		RankedUi::TextBox(g_hFontBold, x, y, w, h, RT3_SORT_CENTER, active ? RankedUi::Col(255, 226, 150) : RankedUi::Col(176, 166, 146), label);
	}
}

CNewUIDuelWatchWindow::CNewUIDuelWatchWindow()
{
	m_pNewUIMng = NULL;
	m_Pos.x = m_Pos.y = 0;
	m_Tab = TAB_RANKED;
	m_dwListTick = 0;
	m_dwStatusTick = 0;
}

CNewUIDuelWatchWindow::~CNewUIDuelWatchWindow()
{
	Release();
}

bool CNewUIDuelWatchWindow::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if(NULL == pNewUIMng)
		return false;

	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_DUELWATCH, this);
	x = x + DisplayWinCDepthBox;
	SetPos(x, y);

	Show(false);

	return true;
}

void CNewUIDuelWatchWindow::Release()
{
	if(m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);
		m_pNewUIMng = NULL;
	}
}

void CNewUIDuelWatchWindow::SetPos(int x, int y)
{
	m_Pos.x = x;
	m_Pos.y = y;
}

bool CNewUIDuelWatchWindow::UpdateMouseEvent()
{
	if(CheckMouseIn(m_Pos.x, m_Pos.y, WINDOW_WIDTH, WINDOW_HEIGHT) == false)
		return true;

	if(SEASON3B::IsPress(VK_LBUTTON) == false)
		return false;

	const float px = (float)m_Pos.x;
	const float py = (float)m_Pos.y;

	// Close.
	if(RankedUi::Inside(px + kCloseX, py + kCloseY, kCloseW, kCloseH))
	{
		PlayBuffer(SOUND_CLICK01);
		g_pNewUISystem->Hide(SEASON3B::INTERFACE_DUELWATCH);
		return false;
	}

	// Tabs.
	for(int t = 0; t < 2; ++t)
	{
		float tx, ty, tw, th;
		TabRect(t, tx, ty, tw, th);
		if(RankedUi::Inside(px + tx, py + ty, tw, th))
		{
			if(m_Tab != t)
			{
				PlayBuffer(SOUND_CLICK01);
				m_Tab = t;
				if(t == TAB_RANKED && Hero != NULL)
				{
					SendRankedListRequest();
					m_dwListTick = GetTickCount();
				}
			}
			return false;
		}
	}

	if(m_Tab == TAB_RANKED)
	{
		char label[48];
		int style;
		bool enabled;
		const int action = FindButton(label, sizeof(label), style, enabled);

		if(enabled && RankedUi::Inside(px + kMargin + 10.0f, py + kProfY + 74.0f, kInnerW - 20.0f, 28.0f))
		{
			if(gCB_DuelRanked != NULL)
			{
				if(action == FA_SEARCH)
					gCB_DuelRanked->StartSearch();
				else if(action == FA_CANCEL)
					gCB_DuelRanked->CancelSearch();
			}
			return false;
		}

		if(CanWatch())
		{
			for(int room = 0; room < MAX_RANKED_FIGHTS; ++room)
			{
				const RANKED_FIGHT_INFO* fight = g_DuelMgr.FindRankedFightByRoom(room);
				const float cy = kRoomY + (float)room * (kRoomH + kRoomGap);
				if(fight != NULL && RankedUi::Inside(px + kMargin + kInnerW - kWatchW - 8.0f, py + cy + 4.0f, kWatchW, kWatchH))
				{
					PlayBuffer(SOUND_CLICK01);
					SendRankedWatch(fight->m_byRoom, fight->m_dwMatchId);
					return false;
				}
			}
		}
	}
	else
	{
		for(int i = 0; i < MAX_DUEL_CHANNELS; ++i)
		{
			const float cy = kChanY + (float)i * (kChanH + kChanGap);
			if(CanJoinNormal(i) && RankedUi::Inside(px + kMargin + (kInnerW - 124.0f) * 0.5f, py + cy + 42.0f, 124.0f, 28.0f))
			{
				PlayBuffer(SOUND_CLICK01);
				SendRequestJoinChannel(i);
				return false;
			}
		}
	}

	return false;
}

bool CNewUIDuelWatchWindow::UpdateKeyEvent()
{
	if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_DUELWATCH) == true)
	{
		if(SEASON3B::IsPress(VK_ESCAPE) == true)
		{
			g_pNewUISystem->Hide(SEASON3B::INTERFACE_DUELWATCH);
			PlayBuffer(SOUND_CLICK01);
			return false;
		}
	}
	return true;
}

bool CNewUIDuelWatchWindow::Update()
{
	if(IsVisible() == false || Hero == NULL)
		return true;

	const DWORD now = GetTickCount();

	// The fight list is polled while the ranked tab is up; the status only until the first one
	// arrives (after that the server's own messages keep it fresh).
	if(m_Tab == TAB_RANKED && (DWORD)(now - m_dwListTick) >= (DWORD)kPollMs)
	{
		m_dwListTick = now;
		SendRankedListRequest();
	}

	if(g_DuelMgr.GetRankedStatus().m_bValid == FALSE && (DWORD)(now - m_dwStatusTick) >= (DWORD)kPollMs)
	{
		m_dwStatusTick = now;
		SendRankedStatusRequest();
	}

	return true;
}

void CNewUIDuelWatchWindow::DrawFrame()
{
	const float px = (float)m_Pos.x;
	const float py = (float)m_Pos.y;

	// Body and the double gold-brown border.
	RankedUi::Bar(px, py, kW, kH, RankedUi::Col(18, 18, 22, 248));
	RankedUi::Outline(px, py, kW, kH, RankedUi::Col(104, 86, 54));
	RankedUi::Outline(px + 1.0f, py + 1.0f, kW - 2.0f, kH - 2.0f, RankedUi::Col(60, 50, 34));
	RankedUi::Outline(px + 4.0f, py + 4.0f, kW - 8.0f, kH - 8.0f, RankedUi::Col(40, 34, 26));

	// Title band.
	RankedUi::Bar(px + 2.0f, py + 2.0f, kW - 4.0f, 30.0f, RankedUi::Col(34, 30, 22));
	RankedUi::Bar(px + 2.0f, py + 31.0f, kW - 4.0f, 1.0f, RankedUi::Col(150, 122, 70));

	const char* title = GlobalText[2698];
	if(title == NULL)
		title = "Doorkeeper Titus";
	const float tw = RankedUi::TextWidth(g_hFontBold, title);
	RankedUi::TextBox(g_hFontBold, px, py + 2.0f, kW, 30.0f, RT3_SORT_CENTER, RankedUi::Col(240, 232, 208), title);
	RankedUi::DrawDiamond(px + kW * 0.5f - tw * 0.5f - 12.0f, py + 17.0f, 4.0f, kGoldDim);
	RankedUi::DrawDiamond(px + kW * 0.5f + tw * 0.5f + 12.0f, py + 17.0f, 4.0f, kGoldDim);

	// Close: a round gold button with a cross.
	const float cx = px + kCloseX + 2.0f, cy = py + kCloseY + 3.0f;
	const bool hover = RankedUi::Inside(px + kCloseX, py + kCloseY, kCloseW, kCloseH);
	RankedUi::DrawDiamond(cx + 13.0f, cy + 12.0f, 12.0f, hover ? RankedUi::Col(255, 214, 90) : RankedUi::Col(222, 168, 40));
	RankedUi::DrawDiamond(cx + 13.0f, cy + 12.0f, 9.0f, RankedUi::Col(54, 36, 8));
	RankedUi::TextBox(g_hFontBold, cx, cy, 26.0f, 24.0f, RT3_SORT_CENTER, RankedUi::Col(255, 226, 150), "X");

	// Corner studs.
	RankedUi::DrawDiamond(px + 7.0f, py + kH - 8.0f, 3.0f, kGoldDim);
	RankedUi::DrawDiamond(px + kW - 8.0f, py + kH - 8.0f, 3.0f, kGoldDim);
}

void CNewUIDuelWatchWindow::DrawTabs()
{
	const char* labels[2] = { "RANKED", "NORMAL" };

	for(int t = 0; t < 2; ++t)
	{
		float x, y, w, h;
		TabRect(t, x, y, w, h);
		DrawTab((float)m_Pos.x + x, (float)m_Pos.y + y, w, h, labels[t], m_Tab == t);
	}
}

void CNewUIDuelWatchWindow::DrawRankedTab()
{
	const float px = (float)m_Pos.x;
	const float py = (float)m_Pos.y;
	const DWORD now = GetTickCount();
	const RANKED_STATUS_INFO& st = g_DuelMgr.GetRankedStatus();

	// Header band.
	{
		const char* head = "RANKED MATCHMAKING";
		const float hw = RankedUi::TextWidth(g_hFontBold, head);
		RankedUi::TextBox(g_hFontBold, px, py + kHeadY, kW, 14.0f, RT3_SORT_CENTER, kGold, head);
		RankedUi::DrawDiamond(px + kW * 0.5f - hw * 0.5f - 12.0f, py + kHeadY + 7.0f, 3.0f, kGoldDim);
		RankedUi::DrawDiamond(px + kW * 0.5f + hw * 0.5f + 12.0f, py + kHeadY + 7.0f, 3.0f, kGoldDim);
	}

	// ---- profile card ----
	const float cx = px + kMargin;
	const float cy = py + kProfY;
	CardPanel(cx, cy, kInnerW, kProfH);

	const bool haveStatus = (st.m_bValid != FALSE);
	const DWORD tierColor = haveStatus ? RankedUi::TierColor(st.m_byTier) : RankedUi::Col(90, 90, 98);

	// Badge: a shield in the tier colour with a diamond in it.
	// With a rank, the same badge art as the name plates.
	if (!(haveStatus && RankedUi::DrawRankBadgeArt(cx + 10.0f, cy + 8.0f, 42.0f, 47.0f, st.m_byTier, st.m_byDiv)))
	{
		RankedUi::DrawShield(cx + 12.0f, cy + 10.0f, 38.0f, 46.0f, tierColor);
		RankedUi::DrawShield(cx + 16.0f, cy + 14.0f, 30.0f, 38.0f, RankedUi::Col(22, 22, 28));
		RankedUi::DrawDiamond(cx + 31.0f, cy + 31.0f, 8.0f, tierColor);
	}

	const float tx = cx + 62.0f;

	if(haveStatus)
	{
		char mmr[24];
		RankedUi::FormatNumber((int)st.m_wMmr, mmr, sizeof(mmr));

		const char* label = "Your MMR:";
		const float lw = RankedUi::TextWidth(g_hFontBold, label);
		RankedUi::Text(g_hFontBold, tx, cy + 8.0f, 0.0f, RT3_SORT_LEFT, kText, label);
		RankedUi::Text(g_hFontBold, tx + lw + 6.0f, cy + 8.0f, 0.0f, RT3_SORT_LEFT, kGold, mmr);

		const float pw = RankedUi::PillWidth(st.m_byTier, st.m_byDiv);
		RankedUi::DrawPill(tx, cy + 28.0f, pw, 18.0f, st.m_byTier, st.m_byDiv);

		char record[40];
		snprintf(record, sizeof(record), "W %d   L %d", (int)st.m_wWins, (int)st.m_wLosses);
		RankedUi::TextBox(g_hFont, tx + pw + 10.0f, cy + 28.0f, 100.0f, 18.0f, RT3_SORT_LEFT, kDim, record);

		// Progress inside the current rank step.
		const float barW = kInnerW - 62.0f - 12.0f;
		float frac = 1.0f;
		if(st.m_wStepNext > st.m_wStepLo)
		{
			frac = ((float)st.m_wMmr - (float)st.m_wStepLo) / ((float)st.m_wStepNext - (float)st.m_wStepLo);
			if(frac < 0.0f) frac = 0.0f;
			if(frac > 1.0f) frac = 1.0f;
		}
		RankedUi::Bar(tx, cy + 52.0f, barW, 8.0f, RankedUi::Col(70, 64, 54));
		RankedUi::Bar(tx + 1.0f, cy + 53.0f, barW - 2.0f, 6.0f, RankedUi::Col(14, 14, 16));
		RankedUi::Bar(tx + 1.0f, cy + 53.0f, (barW - 2.0f) * frac, 6.0f, tierColor);

		// Names the rank being climbed to. Tiers 0-5 have III/II/I, the top tier
		// (Duel Master) has none and is the last rank: past it the MMR just keeps
		// climbing, there is no cap.
		char next[64];
		if(st.m_wStepNext > st.m_wStepLo)
		{
			BYTE nextTier = st.m_byTier;
			BYTE nextDiv = 0;
			if(st.m_byDiv == 3) nextDiv = 2;
			else if(st.m_byDiv == 2) nextDiv = 1;
			else { nextTier = (BYTE)(st.m_byTier + 1); nextDiv = (nextTier >= 6) ? 0 : 3; }

			char nextName[24];
			RankedUi::RankText(nextTier, nextDiv, nextName, sizeof(nextName));
			snprintf(next, sizeof(next), "%d MMR to %s", (int)st.m_wStepNext - (int)st.m_wMmr, nextName);
		}
		else
		{
			char rankName[24];
			RankedUi::RankText(st.m_byTier, st.m_byDiv, rankName, sizeof(rankName));
			snprintf(next, sizeof(next), "%s - top rank, MMR has no limit", rankName);
		}
		RankedUi::Text(g_hFont, tx, cy + 59.0f, barW, RT3_SORT_LEFT, kDim, next);
	}
	else
	{
		const bool waited = (int)(now - g_DuelMgr.GetRankedOpenTick()) > (int)kUnavailableMs;
		RankedUi::TextBox(g_hFontBold, tx, cy + 8.0f, kInnerW - 62.0f - 8.0f, 20.0f, RT3_SORT_LEFT, kDim, waited ? "Ranked unavailable" : "Loading...");
		if(waited)
		{
			RankedUi::Text(g_hFont, tx, cy + 32.0f, kInnerW - 62.0f - 8.0f, RT3_SORT_LEFT, kDim, "The server does not offer");
			RankedUi::Text(g_hFont, tx, cy + 45.0f, kInnerW - 62.0f - 8.0f, RT3_SORT_LEFT, kDim, "ranked duels right now.");
		}
	}

	// Find Match / Cancel.
	{
		char label[48];
		int style;
		bool enabled;
		FindButton(label, sizeof(label), style, enabled);
		RankedUi::DrawButton(cx + 10.0f, cy + 74.0f, kInnerW - 20.0f, 28.0f, label, style, enabled, true);
	}

	// ---- one card per arena room ----
	const bool listKnown = (g_DuelMgr.IsRankedListValid() != FALSE);
	const bool listWaited = (int)(now - g_DuelMgr.GetRankedOpenTick()) > (int)kUnavailableMs;
	const bool canWatch = CanWatch();

	for(int room = 0; room < MAX_RANKED_FIGHTS; ++room)
	{
		const float x = px + kMargin;
		const float y = py + kRoomY + (float)room * (kRoomH + kRoomGap);
		const RANKED_FIGHT_INFO* f = g_DuelMgr.FindRankedFightByRoom(room);

		CardPanel(x, y, kInnerW, kRoomH);

		const float leftW = kInnerW - kWatchW - 8.0f - 10.0f - 6.0f;	// text area beside the Watch button

		char title[40];
		snprintf(title, sizeof(title), "Colosseum # %d", room + 1);
		RankedUi::Text(g_hFontBold, x + 10.0f, y + 6.0f, leftW, RT3_SORT_LEFT, kGold, title);

		if(f != NULL)
		{
			char clock[16];
			RankedUi::FormatClock((int)f->m_wRemainSec, clock, sizeof(clock));
			RankedUi::Text(g_hFont, x + 10.0f, y + 7.0f, leftW, RT3_SORT_RIGHT, kDim, clock);

			const float mid = x + 10.0f + leftW * 0.5f;
			const float half = leftW * 0.5f - 18.0f;
			char score[24];
			snprintf(score, sizeof(score), "%d - %d", (int)f->m_byScore1, (int)f->m_byScore2);
			RankedUi::Text(g_hFont, mid - half - 18.0f, y + 21.0f, half, RT3_SORT_RIGHT, kWhite, f->m_szName1);
			RankedUi::Text(g_hFontBold, mid - 18.0f, y + 21.0f, 36.0f, RT3_SORT_CENTER, kGold, score);
			RankedUi::Text(g_hFont, mid + 18.0f, y + 21.0f, half, RT3_SORT_LEFT, kWhite, f->m_szName2);

			// Rank and MMR of each fighter in the tier colour, across the full card width.
			char r1[40], r2[40], t1[24], t2[24], m1[24], m2[24];
			RankedUi::RankText(f->m_byTier1, f->m_byDiv1, t1, sizeof(t1));
			RankedUi::RankText(f->m_byTier2, f->m_byDiv2, t2, sizeof(t2));
			RankedUi::FormatNumber((int)f->m_wMmr1, m1, sizeof(m1));
			RankedUi::FormatNumber((int)f->m_wMmr2, m2, sizeof(m2));
			snprintf(r1, sizeof(r1), "%s  %s", t1, m1);
			snprintf(r2, sizeof(r2), "%s  %s", t2, m2);

			const float cw = kInnerW * 0.5f - 6.0f;
			RankedUi::Text(g_hFont, x + 4.0f, y + 36.0f, cw, RT3_SORT_RIGHT, RankedUi::TierColor(f->m_byTier1), r1);
			RankedUi::Text(g_hFont, x + kInnerW * 0.5f + 2.0f, y + 36.0f, cw, RT3_SORT_LEFT, RankedUi::TierColor(f->m_byTier2), r2);
		}
		else
		{
			const char* idle = (listKnown || listWaited) ? "No fight" : "Loading...";
			RankedUi::Text(g_hFont, x + 10.0f, y + 25.0f, leftW, RT3_SORT_CENTER, kDim, idle);
		}

		RankedUi::DrawButton(x + kInnerW - kWatchW - 8.0f, y + 4.0f, kWatchW, kWatchH, "Watch", 0, (f != NULL && canWatch), false);
	}
}

void CNewUIDuelWatchWindow::DrawNormalTab()
{
	const float px = (float)m_Pos.x;
	const float py = (float)m_Pos.y;

	// The pre-ranked window's own captions, so a translation of them keeps working.
	const char* caption = GlobalText[2699];
	RankedUi::TextBox(g_hFont, px + kMargin, py + kTabY + kTabH + 2.0f, kInnerW, 14.0f, RT3_SORT_CENTER, kDim, (caption != NULL) ? caption : "Choose a duel to watch");

	for(int i = 0; i < MAX_DUEL_CHANNELS; ++i)
	{
		const float x = px + kMargin;
		const float y = py + kChanY + (float)i * (kChanH + kChanGap);
		const bool active = (g_DuelMgr.IsDuelChannelEnabled(i) != FALSE);

		CardPanel(x, y, kInnerW, kChanH);

		char title[64];
		const char* titleFormat = GlobalText[2700];
		snprintf(title, sizeof(title), (titleFormat != NULL) ? titleFormat : "Colosseum # %d", i + 1);
		RankedUi::Text(g_hFontBold, x, y + 8.0f, kInnerW, RT3_SORT_CENTER, kGold, title);

		if(active)
		{
			const float mid = x + kInnerW * 0.5f;
			RankedUi::Text(g_hFontBold, mid - 14.0f, y + 26.0f, 28.0f, RT3_SORT_CENTER, kRed, "VS");
			RankedUi::Text(g_hFont, mid - 14.0f - 100.0f, y + 26.0f, 100.0f, RT3_SORT_RIGHT, kWhite, g_DuelMgr.GetDuelChannelUserID1(i));
			RankedUi::Text(g_hFont, mid + 14.0f, y + 26.0f, 100.0f, RT3_SORT_LEFT, kWhite, g_DuelMgr.GetDuelChannelUserID2(i));
		}
		else
		{
			RankedUi::Text(g_hFont, x, y + 26.0f, kInnerW, RT3_SORT_CENTER, kText, GlobalText[2705]);
		}

		RankedUi::DrawButton(x + (kInnerW - 124.0f) * 0.5f, y + 42.0f, 124.0f, 28.0f, GlobalText[2701], 0, CanJoinNormal(i), false);
	}
}

bool CNewUIDuelWatchWindow::Render()
{
	EnableAlphaTest();
	glColor4f(1.f, 1.f, 1.f, 1.f);

	DrawFrame();
	DrawTabs();

	if(m_Tab == TAB_RANKED)
		DrawRankedTab();
	else
		DrawNormalTab();

	DisableAlphaBlend();

	return true;
}

void CNewUIDuelWatchWindow::OpeningProcess()
{
	const DWORD now = GetTickCount();

	m_Tab = TAB_RANKED;
	m_dwListTick = now;
	m_dwStatusTick = now;

	g_DuelMgr.SetRankedOpenTick(now);
	g_DuelMgr.InvalidateRankedList();

	// An old server never answers these (it ignores unknown AA sub-codes): the window then
	// shows "Ranked unavailable" after a few seconds.
	if(Hero != NULL)
	{
		SendRankedStatusRequest();
		SendRankedListRequest();
	}
}

void CNewUIDuelWatchWindow::ClosingProcess()
{
	SendExitInventory();
}

float CNewUIDuelWatchWindow::GetLayerDepth()
{ return 5.0f; }
