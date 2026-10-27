// CB_DuelRanked.cpp: ranked duel overlay (searching chip, match prompt, pause line, result
// banner, toasts) and the code-drawn helpers shared with the Doorkeeper Titus window.
// See CB_DuelRanked.h and Working Project\MD Files\RankedDuel-PLAN.md.
//
// Everything here is drawn with plain GL bars and the bitmap font: no textures, so it works the
// same on PC and phones. Clicks follow the FriendMailWindow pattern (one click per press, the
// first Android finger is already the mouse).
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CB_DuelRanked.h"
#include "CBInterface.h"
#include "NewUIBCustomMenu.h"
#include "NewUISystem.h"
#include "DSPlaySound.h"
#include "DuelMgr.h"
#include "ZzzOpenglUtil.h"
#include "ZzzCharacter.h"
#include "ZzzScene.h"
#include "wsclientinline.h"
#include "MultiLanguage.h"
#include "Input.h"

CB_DuelRanked* gCB_DuelRanked = NULL;

namespace
{
	const DWORD kWhite = RankedUi::Col(255, 255, 255);
	const DWORD kText = RankedUi::Col(236, 228, 208);
	const DWORD kGold = RankedUi::Col(255, 200, 80);
	const DWORD kGreen = RankedUi::Col(120, 230, 130);
	const DWORD kRed = RankedUi::Col(255, 110, 110);
	const DWORD kDim = RankedUi::Col(160, 160, 166);
	const DWORD kOrange = RankedUi::Col(255, 160, 60);

	const DWORD kToastMs = 5000;
	const DWORD kBannerMs = 9000;
	const DWORD kQueuedPollMs = 10000;

	const float kChipW = 222.0f, kChipH = 32.0f, kChipY = 40.0f;
	const float kPromptW = 250.0f, kPromptH = 128.0f, kPromptY = 150.0f;
	const float kBannerW = 250.0f, kBannerH = 112.0f, kBannerY = 168.0f;
	const float kPauseW = 300.0f, kPauseH = 20.0f, kPauseY = 76.0f;
	const float kToastW = 300.0f, kToastH = 16.0f, kToastY = 100.0f;

	bool PrimaryDown()
	{
#if defined(MU_TOUCH_UI)
		return SEASON3B::IsPress(VK_LBUTTON) || CInput::Instance().IsLBtnDn();
#else
		return ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
#endif
	}

	DWORD Shade(DWORD color, float f)
	{
		int r = (int)(GetRed(color) * f);
		int g = (int)(GetGreen(color) * f);
		int b = (int)(GetBlue(color) * f);
		if (r > 255) r = 255;
		if (g > 255) g = 255;
		if (b > 255) b = 255;
		return RankedUi::Col(r, g, b, GetAlpha(color));
	}

	// Where the overlay is centred: the middle of the screen, or the middle of what the Titus
	// window leaves free while it is open.
	float CenterX()
	{
		float cx = (float)MAX_WIN_WIDTH * 0.5f;
		if (g_pNewUISystem != NULL && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_DUELWATCH))
		{
			cx = ((float)MAX_WIN_WIDTH - (float)SEASON3B::CNewUIDuelWatchWindow::WINDOW_WIDTH) * 0.5f;
		}
		return cx;
	}

	void BlockWorldClicks(float x, float y, float w, float h)
	{
		if (RankedUi::Inside(x, y, w, h) && g_pBCustomMenuInfo != NULL)
		{
			g_pBCustomMenuInfo->SetBlockCur(TRUE);
		}
	}

	// Keeps gInterface.Data[id] in step with what is drawn: Android hit-tests taps against these
	// rectangles (IsTouchOverAndroidCustomWindow), so a finger on the chip does not also walk the
	// character. The enum slots sit between eBeginWindowCustom and eEndWindowCustom.
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
}

//////////////////////////////////////////////////////////////////////
// Shared drawing helpers
//////////////////////////////////////////////////////////////////////

void RankedUi::Bar(float x, float y, float w, float h, DWORD rgba)
{
	if (w <= 0.0f || h <= 0.0f)
	{
		return;
	}

	gInterface.DrawBarForm(x, y, w, h, GetRed(rgba) / 255.0f, GetGreen(rgba) / 255.0f, GetBlue(rgba) / 255.0f, GetAlpha(rgba) / 255.0f);
}

void RankedUi::Outline(float x, float y, float w, float h, DWORD rgba)
{
	Bar(x, y, w, 1.0f, rgba);
	Bar(x, y + h - 1.0f, w, 1.0f, rgba);
	Bar(x, y + 1.0f, 1.0f, h - 2.0f, rgba);
	Bar(x + w - 1.0f, y + 1.0f, 1.0f, h - 2.0f, rgba);
}

// Straight to the render text, not through TextDraw: TextDraw splits a line at every '#', and
// "Colosseum # 1" has one.
void RankedUi::Text(HFONT font, float x, float y, float w, int sort, DWORD rgba, const char* text)
{
	if (text == NULL || text[0] == '\0')
	{
		return;
	}

	const DWORD backupText = g_pRenderText->GetTextColor();
	const DWORD backupBack = g_pRenderText->GetBgColor();

	g_pRenderText->SetFont(font);
	g_pRenderText->SetTextColor(rgba);
	g_pRenderText->SetBgColor((DWORD)0);
	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	g_pRenderText->RenderText((int)x, (int)y, text, (int)w, 0, sort);

	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetTextColor(backupText);
	g_pRenderText->SetBgColor(backupBack);
}

void RankedUi::TextBox(HFONT font, float x, float y, float w, float h, int sort, DWORD rgba, const char* text)
{
	Text(font, x, y + (h - TextHeight(font)) * 0.5f, w, sort, rgba, text);
}

// The measuring DC works in real pixels; the layout is in the 640x480 virtual space. Results are
// cached (the windows ask for the same few strings every frame) and dropped when the screen
// scale or a font changes.
float RankedUi::TextWidth(HFONT font, const char* text)
{
	if (text == NULL || text[0] == '\0')
	{
		return 0.0f;
	}

	struct ENTRY { HFONT font; float rate; char text[48]; float width; };
	static ENTRY s_cache[24];
	static int s_next = 0;

	const size_t len = strlen(text);
	if (len < sizeof(s_cache[0].text))
	{
		for (int i = 0; i < 24; ++i)
		{
			if (s_cache[i].font == font && s_cache[i].rate == g_fScreenRate_x && strcmp(s_cache[i].text, text) == 0)
			{
				return s_cache[i].width;
			}
		}
	}

	SIZE size = { 0, 0 };
	g_pRenderText->SetFont(font);
	g_pMultiLanguage->_GetTextExtentPoint32(g_pRenderText->GetFontDC(), text, (int)len, &size);
	g_pRenderText->SetFont(g_hFont);

	const float width = (float)size.cx / ((g_fScreenRate_x > 0.0f) ? g_fScreenRate_x : 1.0f);

	if (len < sizeof(s_cache[0].text))
	{
		ENTRY& e = s_cache[s_next];
		s_next = (s_next + 1) % 24;
		e.font = font;
		e.rate = g_fScreenRate_x;
		memcpy(e.text, text, len + 1);
		e.width = width;
	}

	return width;
}

float RankedUi::TextHeight(HFONT font)
{
	static HFONT s_font[2] = { NULL, NULL };
	static float s_rate[2] = { 0.0f, 0.0f };
	static float s_height[2] = { 12.0f, 12.0f };

	for (int i = 0; i < 2; ++i)
	{
		if (s_font[i] == font && s_rate[i] == g_fScreenRate_y)
		{
			return s_height[i];
		}
	}

	SIZE size = { 0, 0 };
	g_pRenderText->SetFont(font);
	g_pMultiLanguage->_GetTextExtentPoint32(g_pRenderText->GetFontDC(), "Ag", 2, &size);
	g_pRenderText->SetFont(g_hFont);

	float h = (float)size.cy / ((g_fScreenRate_y > 0.0f) ? g_fScreenRate_y : 1.0f);
	if (h < 6.0f)
	{
		h = 12.0f;
	}

	static int s_slot = 0;
	s_font[s_slot] = font;
	s_rate[s_slot] = g_fScreenRate_y;
	s_height[s_slot] = h;
	s_slot = (s_slot + 1) % 2;

	return h;
}

bool RankedUi::Inside(float x, float y, float w, float h)
{
	return SEASON3B::CheckMouseIn((int)x, (int)y, (int)w, (int)h) == true;
}

void RankedUi::FormatNumber(int value, char* out, size_t cb)
{
	if (cb < 16)
	{
		if (cb > 0) out[0] = '\0';
		return;
	}

	if (value < 0)
	{
		value = 0;
	}

	char raw[16];
	snprintf(raw, sizeof(raw), "%d", value);

	const int len = (int)strlen(raw);
	size_t o = 0;
	for (int i = 0; i < len && o + 2 < cb; ++i)
	{
		if (i > 0 && ((len - i) % 3) == 0)
		{
			out[o++] = ',';
		}
		out[o++] = raw[i];
	}
	out[o] = '\0';
}

void RankedUi::FormatClock(int seconds, char* out, size_t cb)
{
	if (seconds < 0)
	{
		seconds = 0;
	}
	if (seconds > 99 * 60 + 59)
	{
		seconds = 99 * 60 + 59;
	}

	snprintf(out, cb, "%02d:%02d", seconds / 60, seconds % 60);
}

void RankedUi::RankText(BYTE tier, BYTE div, char* out, size_t cb)
{
	static const char* kNames[7] = { "IRON", "BRONZE", "SILVER", "GOLD", "PLATINUM", "LEGEND", "DUEL MASTER" };

	if (tier > 6)
	{
		snprintf(out, cb, "RANK %d", (int)tier);
		return;
	}

	const char* suffix = "";
	if (div == 3) suffix = " III";
	else if (div == 2) suffix = " II";
	else if (div == 1) suffix = " I";

	snprintf(out, cb, "%s%s", kNames[tier], suffix);
}

DWORD RankedUi::TierColor(BYTE tier)
{
	switch (tier)
	{
	case 0: return Col(128, 128, 138);	// Iron
	case 1: return Col(190, 112, 56);	// Bronze
	case 2: return Col(176, 184, 200);	// Silver
	case 3: return Col(232, 178, 40);	// Gold
	case 4: return Col(70, 206, 196);	// Platinum
	case 5: return Col(178, 92, 235);	// Legend
	case 6: return Col(232, 60, 70);	// Duel Master
	}
	return Col(110, 110, 120);
}

int RankedUi::RankOrder(BYTE tier, BYTE div)
{
	// III = 1, II = 2, I = 3; a tier without divisions (Duel Master) sorts above the lot.
	return (int)tier * 4 + ((div == 0) ? 0 : (4 - (int)div));
}

float RankedUi::PillWidth(BYTE tier, BYTE div)
{
	char text[24];
	RankText(tier, div, text, sizeof(text));

	const float w = TextWidth(g_hFontBold, text) + 18.0f;
	return (w > 56.0f) ? w : 56.0f;
}

void RankedUi::DrawPill(float x, float y, float w, float h, BYTE tier, BYTE div)
{
	char text[24];
	RankText(tier, div, text, sizeof(text));

	const DWORD fill = TierColor(tier);
	const int luma = (GetRed(fill) * 3 + GetGreen(fill) * 6 + GetBlue(fill)) / 10;

	// Rounded corners: a full-width bar, a full-height bar, one unit shorter each way.
	Bar(x + 1.0f, y, w - 2.0f, h, Shade(fill, 0.55f));
	Bar(x, y + 1.0f, w, h - 2.0f, Shade(fill, 0.55f));
	Bar(x + 2.0f, y + 1.0f, w - 4.0f, h - 2.0f, fill);
	Bar(x + 1.0f, y + 2.0f, w - 2.0f, h - 4.0f, fill);

	TextBox(g_hFontBold, x, y, w, h, RT3_SORT_CENTER, (luma > 150) ? Col(24, 22, 20) : kWhite, text);
}

void RankedUi::DrawDiamond(float cx, float cy, float r, DWORD rgba)
{
	for (float yy = -r; yy <= r; yy += 1.0f)
	{
		float a = (yy < 0.0f) ? -yy : yy;
		float half = r - a;
		Bar(cx - half, cy + yy, half * 2.0f + 1.0f, 1.0f, rgba);
	}
}

void RankedUi::DrawShield(float x, float y, float w, float h, DWORD rgba)
{
	const float step = 2.0f;
	const float shoulder = h * 0.55f;

	for (float yy = 0.0f; yy < h; yy += step)
	{
		float half = w * 0.5f;
		if (yy > shoulder)
		{
			half = w * 0.5f * (1.0f - (yy - shoulder) / (h - shoulder));
		}
		if (yy < step)
		{
			half -= 2.0f;
		}
		if (half < 1.0f)
		{
			half = 1.0f;
		}

		float rowH = (yy + step > h) ? (h - yy) : step;
		Bar(x + w * 0.5f - half, y + yy, half * 2.0f, rowH, rgba);
	}
}

namespace
{
	const float kBadgeShieldW = 9.0f;
	const float kBadgeShieldH = 11.0f;
	const float kBadgePipW = 3.0f;
}

namespace
{
	// DuelRankBadges.ozt: 256x512, 64x72 tiles. Column 0-2 = division III, II, I; row 0-5 = Iron
	// to Legend; Duel Master (row 6) has only column 0. The division gems are in the art.
	const float kBadgeArtW = 16.0f;
	const float kBadgeArtH = 18.0f;

	bool BadgeArt(BYTE tier)
	{
		return tier <= 6 && Bitmaps.FindTexture(BITMAP_DUELRANK_BADGES) != NULL;
	}
}

float RankedUi::RankBadgeWidth(BYTE div)
{
	return kBadgeShieldW + ((div > 0) ? 2.0f + kBadgePipW : 0.0f);
}

float RankedUi::RankBadgeHeight()
{
	return kBadgeShieldH;
}

float RankedUi::RankBadgeWidth(BYTE tier, BYTE div)
{
	return BadgeArt(tier) ? kBadgeArtW : RankBadgeWidth(div);
}

float RankedUi::RankBadgeHeight(BYTE tier)
{
	return BadgeArt(tier) ? kBadgeArtH : RankBadgeHeight();
}

bool RankedUi::DrawRankBadgeArt(float x, float y, float w, float h, BYTE tier, BYTE div)
{
	if (!BadgeArt(tier))
	{
		return false;
	}

	int col = (div >= 1 && div <= 3) ? (3 - (int)div) : 0;
	int row = (int)tier;

	DisableTexture(false);
	::EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	RenderBitmap(BITMAP_DUELRANK_BADGES, x, y, w, h,
		(float)(col * 64) / 256.0f, (float)(row * 72) / 512.0f, 64.0f / 256.0f, 72.0f / 512.0f);
	return true;
}

void RankedUi::DrawRankBadge(float x, float y, BYTE tier, BYTE div)
{
	if (DrawRankBadgeArt(x, y, kBadgeArtW, kBadgeArtH, tier, div))
	{
		return;
	}

	const DWORD fill = TierColor(tier);

	DrawShield(x, y, kBadgeShieldW, kBadgeShieldH, Shade(fill, 0.35f));
	DrawShield(x + 1.0f, y + 1.0f, kBadgeShieldW - 2.0f, kBadgeShieldH - 3.0f, fill);
	Bar(x + 2.0f, y + 2.0f, kBadgeShieldW - 4.0f, 1.0f, Shade(fill, 1.35f));

	if (div == 0)
	{
		// No divisions (Duel Master): a bright gem on the shield.
		DrawDiamond(x + kBadgeShieldW * 0.5f, y + 5.0f, 2.0f, kWhite);
		return;
	}

	if (div > 3)
	{
		div = 3;
	}

	// One 3x2 pip per division step, stacked from the bottom beside the shield.
	for (int i = 0; i < (int)div; ++i)
	{
		const float py = y + kBadgeShieldH - 3.0f - (float)i * 3.0f;
		Bar(x + kBadgeShieldW + 2.0f, py, kBadgePipW, 2.0f, fill);
	}
}

// C1:D3:C4 from the GameServer: [head 4][count 1] then count * { WORD index, BYTE tier + 1, BYTE div }.
// The icon belongs to the character object with that key; no object, no icon.
void RecvDuelRankView(const BYTE* lpMsg, int size)
{
	if (lpMsg == NULL || size < 5 || lpMsg[0] != 0xC1)
	{
		return;
	}

	int count = lpMsg[4];
	if (count > (size - 5) / 4)
	{
		count = (size - 5) / 4;
	}

	for (int i = 0; i < count; ++i)
	{
		const BYTE* e = lpMsg + 5 + i * 4;
		const int key = (int)(e[0] | (e[1] << 8));
		const int ci = FindCharacterIndex(key);

		if (ci < 0 || ci >= MAX_CHARACTERS_CLIENT)
		{
			continue;
		}

		CHARACTER* c = &CharactersClient[ci];

		if (e[2] > 16 || e[3] > 3)
		{
			c->DuelRankTier = 0;
			c->DuelRankDiv = 0;
			continue;
		}

		c->DuelRankTier = e[2];
		c->DuelRankDiv = e[3];
	}
}

void RankedUi::DrawButton(float x, float y, float w, float h, const char* label, int style, bool enabled, bool bold)
{
	const bool hover = enabled && Inside(x, y, w, h);
	const bool down = hover && MouseLButton;

	DWORD border, fill, text;

	if (!enabled)
	{
		border = Col(64, 64, 66);
		fill = Col(26, 26, 28);
		text = Col(120, 120, 122);
	}
	else if (style == 1)
	{
		border = Col(96, 200, 110);
		fill = down ? Col(20, 60, 30) : (hover ? Col(40, 100, 56) : Col(30, 76, 42));
		text = Col(235, 255, 235);
	}
	else if (style == 2)
	{
		border = Col(220, 96, 96);
		fill = down ? Col(70, 20, 20) : (hover ? Col(120, 40, 40) : Col(94, 30, 30));
		text = Col(255, 235, 235);
	}
	else if (style == 3)
	{
		border = Col(214, 170, 70);
		fill = down ? Col(60, 44, 14) : (hover ? Col(104, 78, 26) : Col(82, 60, 20));
		text = Col(255, 226, 150);
	}
	else
	{
		border = Col(160, 140, 96);
		fill = down ? Col(28, 26, 22) : (hover ? Col(62, 58, 48) : Col(44, 42, 36));
		text = Col(236, 226, 200);
	}

	Bar(x, y, w, h, border);
	Bar(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fill);
	if (enabled && !down)
	{
		Bar(x + 2.0f, y + 2.0f, w - 4.0f, 1.0f, Shade(fill, 1.6f));
	}

	TextBox(bold ? g_hFontBold : g_hFont, x, y + (down ? 1.0f : 0.0f), w, h, RT3_SORT_CENTER, text, label);
}

//////////////////////////////////////////////////////////////////////
// The overlay
//////////////////////////////////////////////////////////////////////

CB_DuelRanked::CB_DuelRanked()
{
	m_Click = false;
	m_WasDown = false;
	m_ActionTick = 0;
	m_PollTick = 0;
	m_ToastCount = 0;
	memset(m_Toast, 0, sizeof(m_Toast));
}

void CB_DuelRanked::HideTitusWindow()
{
	if (g_pNewUISystem != NULL && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_DUELWATCH))
	{
		g_pNewUISystem->Hide(SEASON3B::INTERFACE_DUELWATCH);
	}
}

void CB_DuelRanked::AddToast(const char* text, DWORD rgba)
{
	if (text == NULL || text[0] == '\0')
	{
		return;
	}

	if (m_ToastCount >= MAX_TOASTS)
	{
		for (int i = 1; i < MAX_TOASTS; ++i)
		{
			m_Toast[i - 1] = m_Toast[i];
		}
		m_ToastCount = MAX_TOASTS - 1;
	}

	TOAST& t = m_Toast[m_ToastCount++];
	snprintf(t.text, sizeof(t.text), "%s", text);
	t.color = rgba;
	t.tick = GetTickCount();
}

// One tap is one action: the Titus window and the chip both come through here.
static bool ActionAllowed(DWORD& last)
{
	const DWORD now = GetTickCount();
	// Unsigned: with last == 0 and an uptime past ~24.8 days, (int)(now - last) is
	// negative and every tap was dropped forever.
	if (last != 0 && (DWORD)(now - last) < 400)
	{
		return false;
	}
	last = now;
	return true;
}

void CB_DuelRanked::StartSearch()
{
	if (ActionAllowed(m_ActionTick) == false)
	{
		return;
	}

	PlayBuffer(SOUND_CLICK01);
	SendRankedQueue(1);

	// The tap registered and the request is on its way; the server's answer follows (also in chat).
	if (g_pChatListBox) g_pChatListBox->AddText("", "Ranked: looking for a match...", SEASON3B::TYPE_SYSTEM_MESSAGE);
}

void CB_DuelRanked::CancelSearch()
{
	if (ActionAllowed(m_ActionTick) == false)
	{
		return;
	}

	PlayBuffer(SOUND_CLICK01);
	SendRankedQueue(0);

	// Follow the tap at once; the STATUS asked for below corrects it if the server disagrees.
	g_DuelMgr.SetRankedLocalState(0);
	g_DuelMgr.ScheduleRankedStatus(900);
}

void CB_DuelRanked::AnswerPrompt(bool accept)
{
	RANKED_PROMPT_INFO& prompt = g_DuelMgr.GetRankedPrompt();
	if (prompt.m_bActive == FALSE || prompt.m_bAnswered)
	{
		return;
	}

	if (ActionAllowed(m_ActionTick) == false)
	{
		return;
	}

	PlayBuffer(SOUND_CLICK01);
	SendRankedAccept(accept ? 1 : 0, prompt.m_dwMatchId);

	if (accept)
	{
		g_DuelMgr.SetRankedPromptAnswered();
	}
	else
	{
		g_DuelMgr.ClearRankedPrompt();
		g_DuelMgr.SetRankedLocalState(0);
		g_DuelMgr.ScheduleRankedStatus(900);
	}
}

// ---- packet entry points (called from WSclient.cpp after g_DuelMgr is updated) ----

void CB_DuelRanked::OnStatus()
{
	// A fresh STATUS is as good as a poll. The Titus window stays open whatever the state: its
	// Find Match button already says why it is disabled, and a state stuck on the server side
	// must never lock a player out of the hub.
	m_PollTick = GetTickCount();
}

void CB_DuelRanked::OnPrompt()
{
	g_DuelMgr.SetRankedLocalState(2);
	g_DuelMgr.EndRankedPause();
	HideTitusWindow();
	PlayBuffer(SOUND_OPEN_DUELWINDOW);
}

void CB_DuelRanked::OnResult()
{
	const RANKED_RESULT_INFO& r = g_DuelMgr.GetRankedResult();

	g_DuelMgr.EndRankedPause();
	g_DuelMgr.ClearRankedPrompt();

	if (r.m_byOutcome == 0)
	{
		PlayBuffer(SOUND_START_DUEL);
	}

	// Fresh MMR, rank and win/loss counts for the Titus window.
	g_DuelMgr.ScheduleRankedStatus(2500);
}

void CB_DuelRanked::OnMessage(BYTE code, BYTE p1)
{
	char text[100];
	text[0] = '\0';
	DWORD color = kWhite;
	bool refresh = false;			// the server state changed: ask for a fresh STATUS

	switch (code)
	{
	case 1:		// QUEUED
		g_DuelMgr.SetRankedLocalState(1);
		snprintf(text, sizeof(text), "Searching for a ranked opponent...");
		color = kGreen;
		refresh = true;
		break;
	case 2:		// LEFT_QUEUE
		g_DuelMgr.SetRankedLocalState(0);
		snprintf(text, sizeof(text), "You left the ranked queue.");
		refresh = true;
		break;
	case 3:		// RANKED_DISABLED
		snprintf(text, sizeof(text), "Ranked duels are disabled.");
		color = kRed;
		break;
	case 4:		// ARENA_UNAVAILABLE
		snprintf(text, sizeof(text), "The duel arena is unavailable right now.");
		color = kRed;
		refresh = true;
		break;
	case 5:		// ALREADY_QUEUED
		g_DuelMgr.SetRankedLocalState(1);
		snprintf(text, sizeof(text), "You are already in the ranked queue.");
		color = kGold;
		refresh = true;
		break;
	case 6:		// IN_MATCH
		snprintf(text, sizeof(text), "You are already in a ranked match.");
		color = kGold;
		refresh = true;
		break;
	case 7:		// COOLDOWN
		snprintf(text, sizeof(text), "Ranked cooldown: wait %d s.", (int)p1);
		color = kRed;
		break;
	case 8:		// LEVEL_TOO_LOW
		snprintf(text, sizeof(text), "Ranked duels need level %d.", (int)p1);
		color = kRed;
		break;
	case 9:		// NOT_ALLOWED_NOW
		snprintf(text, sizeof(text), "You cannot join ranked duels right now.");
		color = kRed;
		break;
	case 10:	// OPEN_TITUS_FIRST
		snprintf(text, sizeof(text), "Talk to Doorkeeper Titus first.");
		color = kGold;
		break;
	case 11:	// OPPONENT_DECLINED: still queued
		g_DuelMgr.ClearRankedPrompt();
		g_DuelMgr.SetRankedLocalState(1);
		snprintf(text, sizeof(text), "Your opponent declined. Still searching...");
		color = kGold;
		refresh = true;
		break;
	case 12:	// MATCH_CANCELLED: still queued
		g_DuelMgr.ClearRankedPrompt();
		g_DuelMgr.SetRankedLocalState(1);
		snprintf(text, sizeof(text), "The match was cancelled. Still searching...");
		color = kGold;
		refresh = true;
		break;
	case 13:	// REMOVED_FROM_QUEUE
		g_DuelMgr.ClearRankedPrompt();
		g_DuelMgr.SetRankedLocalState(0);
		snprintf(text, sizeof(text), "You were removed from the ranked queue.");
		color = kRed;
		refresh = true;
		break;
	case 14:	// MATCH_VOID
		g_DuelMgr.ClearRankedPrompt();
		g_DuelMgr.SetRankedLocalState(0);
		g_DuelMgr.EndRankedPause();
		snprintf(text, sizeof(text), "The match was voided. No MMR changed.");
		color = kGold;
		refresh = true;
		break;
	case 15:	// OPPONENT_DISCONNECTED: drawn as the pause line (it repeats, so no toast)
		g_DuelMgr.StartRankedPause((int)p1);
		break;
	case 16:	// OPPONENT_RETURNED
		g_DuelMgr.EndRankedPause();
		snprintf(text, sizeof(text), "Your opponent is back. The fight resumes.");
		color = kGreen;
		break;
	case 17:	// OPPONENT_TIMED_OUT
		g_DuelMgr.EndRankedPause();
		snprintf(text, sizeof(text), "Your opponent timed out. You win!");
		color = kGreen;
		break;
	case 18:	// YOU_TIMED_OUT
		g_DuelMgr.EndRankedPause();
		snprintf(text, sizeof(text), "You timed out. You lose.");
		color = kRed;
		break;
	case 19:	// WATCH_NOT_FOUND
		snprintf(text, sizeof(text), "That fight is no longer available.");
		color = kRed;
		g_DuelMgr.InvalidateRankedList();
		break;
	case 20:	// WATCH_FULL
		snprintf(text, sizeof(text), "That arena is full.");
		color = kRed;
		break;
	case 21:	// PARTY_BLOCK
		snprintf(text, sizeof(text), "Leave your party before joining ranked.");
		color = kRed;
		break;
	case 22:	// HELPER_BLOCK
		snprintf(text, sizeof(text), "Turn off the MU Helper first.");
		color = kRed;
		break;
	case 23:	// MOVING_TO_ARENA
		HideTitusWindow();
		g_DuelMgr.ClearRankedPrompt();
		g_DuelMgr.SetRankedLocalState(3);
		snprintf(text, sizeof(text), "Moving to the arena...");
		break;
	case 24:	// FIGHT_STARTING
		g_DuelMgr.ClearRankedPrompt();
		g_DuelMgr.SetRankedLocalState(4);
		snprintf(text, sizeof(text), "The fight is starting!");
		color = kGreen;
		break;
	case 25:	// RESUMING
		g_DuelMgr.EndRankedPause();
		snprintf(text, sizeof(text), "Resuming the fight...");
		color = kGreen;
		break;
	case 26:	// SPECTATING
		HideTitusWindow();
		g_DuelMgr.SetRankedWatching(TRUE);
		snprintf(text, sizeof(text), "Watching a ranked fight.");
		break;
	case 27:	// GOING_HOME
		g_DuelMgr.SetRankedWatching(FALSE);
		g_DuelMgr.EndRankedPause();
		snprintf(text, sizeof(text), "Going back to where you came from.");
		break;
	default:
		return;		// a code from a newer server: ignore it
	}

	AddToast(text, color);

	// The toast is easy to miss: the same line goes to the chat box.
	if (g_pChatListBox) g_pChatListBox->AddText("", text, SEASON3B::TYPE_SYSTEM_MESSAGE);

	if (refresh)
	{
		g_DuelMgr.ScheduleRankedStatus(700);
	}
}

// ---- drawing ----

void CB_DuelRanked::DrawToasts(DWORD now)
{
	// Drop the expired ones (they expire in order).
	while (m_ToastCount > 0 && (DWORD)(now - m_Toast[0].tick) > (DWORD)kToastMs)
	{
		for (int i = 1; i < m_ToastCount; ++i)
		{
			m_Toast[i - 1] = m_Toast[i];
		}
		--m_ToastCount;
	}

	const float cx = CenterX();
	for (int i = 0; i < m_ToastCount; ++i)
	{
		const float y = kToastY + (float)i * (kToastH + 2.0f);
		RankedUi::Bar(cx - kToastW * 0.5f, y, kToastW, kToastH, RankedUi::Col(10, 10, 12, 190));
		RankedUi::TextBox(g_hFont, cx - kToastW * 0.5f, y, kToastW, kToastH, RT3_SORT_CENTER, m_Toast[i].color, m_Toast[i].text);
	}
}

void CB_DuelRanked::DrawPauseLine()
{
	char text[64];
	snprintf(text, sizeof(text), "Opponent disconnected - waiting %d s", g_DuelMgr.GetRankedPauseSecLeft());

	const float cx = CenterX();
	RankedUi::Bar(cx - kPauseW * 0.5f, kPauseY, kPauseW, kPauseH, RankedUi::Col(70, 24, 16, 215));
	RankedUi::Outline(cx - kPauseW * 0.5f, kPauseY, kPauseW, kPauseH, RankedUi::Col(220, 120, 60));
	RankedUi::TextBox(g_hFontBold, cx - kPauseW * 0.5f, kPauseY, kPauseW, kPauseH, RT3_SORT_CENTER, kOrange, text);
}

void CB_DuelRanked::DrawChip(DWORD now)
{
	const float cx = CenterX();
	const float x = cx - kChipW * 0.5f;
	const float y = kChipY;

	PublishRect(eWindowDuelRankedChip, true, x, y, kChipW, kChipH);
	BlockWorldClicks(x, y, kChipW, kChipH);

	RankedUi::Bar(x, y, kChipW, kChipH, RankedUi::Col(10, 18, 12, 230));
	RankedUi::Outline(x, y, kChipW, kChipH, RankedUi::Col(80, 180, 100));

	// A dot that pulses, so a stalled client is visibly different from a waiting one.
	const int phase = (int)((now / 6) % 200);
	const int alpha = 110 + (phase < 100 ? phase : 200 - phase);
	RankedUi::Bar(x + 8.0f, y + (kChipH - 8.0f) * 0.5f, 8.0f, 8.0f, RankedUi::Col(120, 230, 130, alpha));

	char clock[16];
	char text[40];
	RankedUi::FormatClock(g_DuelMgr.GetRankedQueuedSec(), clock, sizeof(clock));
	snprintf(text, sizeof(text), "Searching... %s", clock);
	RankedUi::TextBox(g_hFontBold, x + 22.0f, y, 118.0f, kChipH, RT3_SORT_LEFT, kGreen, text);

	const float bx = x + kChipW - 80.0f;
	const float by = y + 2.0f;
	RankedUi::DrawButton(bx, by, 76.0f, 28.0f, "Cancel", 2, true, false);

	if (m_Click && RankedUi::Inside(bx, by, 76.0f, 28.0f))
	{
		CancelSearch();
	}
}

void CB_DuelRanked::DrawPrompt(DWORD now)
{
	RANKED_PROMPT_INFO& prompt = g_DuelMgr.GetRankedPrompt();

	const float cx = (float)MAX_WIN_WIDTH * 0.5f;
	const float x = cx - kPromptW * 0.5f;
	const float y = kPromptY;

	PublishRect(eWindowDuelRankedPrompt, true, x, y, kPromptW, kPromptH);
	BlockWorldClicks(x, y, kPromptW, kPromptH);

	RankedUi::Bar(x, y, kPromptW, kPromptH, RankedUi::Col(12, 12, 16, 240));
	RankedUi::Outline(x, y, kPromptW, kPromptH, RankedUi::Col(214, 170, 70));
	RankedUi::Outline(x + 2.0f, y + 2.0f, kPromptW - 4.0f, kPromptH - 4.0f, RankedUi::Col(70, 58, 34));
	RankedUi::DrawDiamond(x + 16.0f, y + 15.0f, 4.0f, kGold);
	RankedUi::DrawDiamond(x + kPromptW - 16.0f, y + 15.0f, 4.0f, kGold);
	RankedUi::Text(g_hFontBold, x, y + 9.0f, kPromptW, RT3_SORT_CENTER, kGold, "MATCH FOUND");

	// Opponent: name, then rank pill and MMR on the next row.
	RankedUi::Text(g_hFontBold, x, y + 28.0f, kPromptW, RT3_SORT_CENTER, kWhite, prompt.m_szOpp);

	char mmr[24];
	char mmrText[40];
	RankedUi::FormatNumber((int)prompt.m_wOppMmr, mmr, sizeof(mmr));
	snprintf(mmrText, sizeof(mmrText), "%s MMR", mmr);
	const float pillW = RankedUi::PillWidth(prompt.m_byOppTier, prompt.m_byOppDiv);
	const float mmrW = RankedUi::TextWidth(g_hFont, mmrText);
	const float rowW = pillW + 8.0f + mmrW;
	const float rowX = cx - rowW * 0.5f;
	RankedUi::DrawPill(rowX, y + 44.0f, pillW, 16.0f, prompt.m_byOppTier, prompt.m_byOppDiv);
	RankedUi::TextBox(g_hFont, rowX + pillW + 8.0f, y + 44.0f, mmrW + 4.0f, 16.0f, RT3_SORT_LEFT, kText, mmrText);

	// Countdown.
	int msLeft = (int)(prompt.m_dwEndTick - now);
	if (msLeft < 0)
	{
		msLeft = 0;
	}
	const int secLeft = (msLeft + 999) / 1000;
	float frac = (prompt.m_dwLength > 0) ? ((float)msLeft / (float)prompt.m_dwLength) : 0.0f;
	if (frac > 1.0f) frac = 1.0f;

	const float barX = x + 16.0f, barW = kPromptW - 32.0f;
	RankedUi::Bar(barX, y + 68.0f, barW, 8.0f, RankedUi::Col(60, 56, 50));
	RankedUi::Bar(barX + 1.0f, y + 69.0f, barW - 2.0f, 6.0f, RankedUi::Col(16, 16, 18));
	RankedUi::Bar(barX + 1.0f, y + 69.0f, (barW - 2.0f) * frac, 6.0f, (frac > 0.3f) ? RankedUi::Col(110, 210, 120) : RankedUi::Col(230, 90, 80));

	char timeText[32];
	snprintf(timeText, sizeof(timeText), "Time left: %d s", secLeft);
	RankedUi::Text(g_hFont, x, y + 80.0f, kPromptW, RT3_SORT_CENTER, kDim, timeText);

	const float btnY = y + kPromptH - 38.0f;
	const float btnW = (kPromptW - 32.0f - 10.0f) * 0.5f;

	if (prompt.m_bAnswered)
	{
		RankedUi::Text(g_hFontBold, x, btnY + 8.0f, kPromptW, RT3_SORT_CENTER, kGreen, "Waiting for your opponent...");
		return;
	}

	const float acceptX = x + 16.0f;
	const float declineX = x + 16.0f + btnW + 10.0f;
	RankedUi::DrawButton(acceptX, btnY, btnW, 30.0f, "Accept", 1, true, true);
	RankedUi::DrawButton(declineX, btnY, btnW, 30.0f, "Decline", 2, true, true);

	if (m_Click)
	{
		if (RankedUi::Inside(acceptX, btnY, btnW, 30.0f))
		{
			AnswerPrompt(true);
		}
		else if (RankedUi::Inside(declineX, btnY, btnW, 30.0f))
		{
			AnswerPrompt(false);
		}
	}
}

void CB_DuelRanked::DrawBanner()
{
	RANKED_RESULT_INFO& r = g_DuelMgr.GetRankedResult();

	const float cx = CenterX();
	const float x = cx - kBannerW * 0.5f;
	const float y = kBannerY;

	PublishRect(eWindowDuelRankedBanner, true, x, y, kBannerW, kBannerH);
	BlockWorldClicks(x, y, kBannerW, kBannerH);

	const char* title = "NO RESULT";
	DWORD accent = kDim;
	switch (r.m_byOutcome)
	{
	case 0: title = "WIN"; accent = kGreen; break;
	case 1: title = "LOSS"; accent = kRed; break;
	case 2: title = "DRAW"; accent = kGold; break;
	}

	RankedUi::Bar(x, y, kBannerW, kBannerH, RankedUi::Col(12, 12, 16, 240));
	RankedUi::Outline(x, y, kBannerW, kBannerH, accent);
	RankedUi::Outline(x + 2.0f, y + 2.0f, kBannerW - 4.0f, kBannerH - 4.0f, Shade(accent, 0.45f));
	RankedUi::Text((g_hFontBig != NULL) ? g_hFontBig : g_hFontBold, x, y + 8.0f, kBannerW, RT3_SORT_CENTER, accent, title);

	char line[80];
	float ly = y + 44.0f;

	if (r.m_byOutcome <= 2)
	{
		char oldMmr[24], newMmr[24];
		RankedUi::FormatNumber((int)r.m_wOldMmr, oldMmr, sizeof(oldMmr));
		RankedUi::FormatNumber((int)r.m_wNewMmr, newMmr, sizeof(newMmr));

		snprintf(line, sizeof(line), "%+d MMR", (int)r.m_sDelta);
		RankedUi::Text(g_hFontBold, x, ly, kBannerW, RT3_SORT_CENTER, (r.m_sDelta > 0) ? kGreen : ((r.m_sDelta < 0) ? kRed : kText), line);
		ly += 16.0f;

		snprintf(line, sizeof(line), "MMR %s -> %s", oldMmr, newMmr);
		RankedUi::Text(g_hFont, x, ly, kBannerW, RT3_SORT_CENTER, kText, line);
		ly += 15.0f;

		char oldRank[24], newRank[24];
		RankedUi::RankText(r.m_byOldTier, r.m_byOldDiv, oldRank, sizeof(oldRank));
		RankedUi::RankText(r.m_byNewTier, r.m_byNewDiv, newRank, sizeof(newRank));

		const int before = RankedUi::RankOrder(r.m_byOldTier, r.m_byOldDiv);
		const int after = RankedUi::RankOrder(r.m_byNewTier, r.m_byNewDiv);

		if (after > before)
		{
			snprintf(line, sizeof(line), "RANK UP! %s -> %s", oldRank, newRank);
			RankedUi::Text(g_hFontBold, x, ly, kBannerW, RT3_SORT_CENTER, kGold, line);
		}
		else if (after < before)
		{
			snprintf(line, sizeof(line), "Rank down: %s -> %s", oldRank, newRank);
			RankedUi::Text(g_hFont, x, ly, kBannerW, RT3_SORT_CENTER, kRed, line);
		}
		else
		{
			snprintf(line, sizeof(line), "Rank: %s", newRank);
			RankedUi::Text(g_hFont, x, ly, kBannerW, RT3_SORT_CENTER, RankedUi::TierColor(r.m_byNewTier), line);
		}
	}
	else
	{
		RankedUi::Text(g_hFont, x, ly + 8.0f, kBannerW, RT3_SORT_CENTER, kText, "No MMR changed.");
	}

	RankedUi::Text(g_hFont, x, y + kBannerH - 16.0f, kBannerW, RT3_SORT_CENTER, RankedUi::Col(110, 110, 116), "Tap to close");

	if (m_Click && RankedUi::Inside(x, y, kBannerW, kBannerH))
	{
		g_DuelMgr.ClearRankedResult();
	}
}

void CB_DuelRanked::DrawWindow()
{
	const DWORD now = GetTickCount();

	// One click per press, not one per frame the button is held.
	const bool down = PrimaryDown();
	m_Click = down && (m_WasDown == false);
	m_WasDown = down;

	// A deferred STATUS request (after entering a world, or after the queue changed). Nothing is
	// asked for before the world is up; the request just waits.
	const bool inWorld = (Hero != NULL && SceneFlag == MAIN_SCENE);

	if (inWorld && g_DuelMgr.TakeRankedStatusDue())
	{
		SendRankedStatusRequest();
	}

	// A search outlives a DataServer restart (the queue is memory-only there) and a lost packet,
	// so while queued the state is re-read now and then instead of trusting the chip forever.
	if (inWorld && g_DuelMgr.IsRankedQueued() && (DWORD)(now - m_PollTick) >= (DWORD)kQueuedPollMs)
	{
		m_PollTick = now;
		SendRankedStatusRequest();
	}

	RANKED_PROMPT_INFO& prompt = g_DuelMgr.GetRankedPrompt();
	RANKED_RESULT_INFO& result = g_DuelMgr.GetRankedResult();

	// The server decides a prompt's end (it allows 2 s of grace); this only clears one it never
	// ended, e.g. after a lost packet.
	if (prompt.m_bActive && (int)(now - prompt.m_dwEndTick) > 4000)
	{
		g_DuelMgr.ClearRankedPrompt();
	}
	if (result.m_bActive && (int)(now - result.m_dwTick) > (int)kBannerMs)
	{
		g_DuelMgr.ClearRankedResult();
	}

	const bool titusOpen = (g_pNewUISystem != NULL && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_DUELWATCH));
	const bool showPrompt = (prompt.m_bActive != FALSE);
	const bool showChip = g_DuelMgr.IsRankedQueued() && !showPrompt && !titusOpen;
	const bool showBanner = (result.m_bActive != FALSE);
	const bool showPause = (g_DuelMgr.IsRankedPaused() != FALSE);

	// Closed overlays stop claiming screen space (the Android tap test reads these).
	if (showChip == false) PublishRect(eWindowDuelRankedChip, false, 0, 0, 0, 0);
	if (showPrompt == false) PublishRect(eWindowDuelRankedPrompt, false, 0, 0, 0, 0);
	if (showBanner == false) PublishRect(eWindowDuelRankedBanner, false, 0, 0, 0, 0);

	if (showChip == false && showPrompt == false && showBanner == false && showPause == false && m_ToastCount == 0)
	{
		return;
	}

	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (showPause)
	{
		DrawPauseLine();
	}
	DrawToasts(now);
	if (showChip)
	{
		DrawChip(now);
	}
	if (showBanner)
	{
		DrawBanner();
	}
	if (showPrompt)
	{
		DrawPrompt(now);
	}

	// Same state the other windows in this chain (CB_KiraPass) leave behind.
	EnableAlphaTest(false);
}
