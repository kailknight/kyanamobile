// CB_DuelRanked.h: ranked duel (cross-server matchmaking) overlay, plus the drawing helpers
// the Doorkeeper Titus window shares with it.
//
// The Titus window (NewUIDuelWatchWindow.cpp) is the ranked hub. This file owns what has to stay
// on screen after that window is closed: the "Searching..." chip with its cancel button, the
// Accept/Decline prompt for a found match, the "Opponent disconnected" pause line, the result
// banner and short toast lines. The state lives in g_DuelMgr (DuelMgr.h); the packets (AA:10-14)
// are parsed in WSclient.cpp. Everything is code-drawn (no textures).
// See Working Project\MD Files\RankedDuel-PLAN.md sections 5.3, 5.4 and 7.
//
//////////////////////////////////////////////////////////////////////

#pragma once

class CB_DuelRanked
{
public:
	CB_DuelRanked();

	// Every frame, from Interface::Work().
	void DrawWindow();

	// Called by the AA:10-14 handlers in WSclient.cpp after g_DuelMgr holds the new data.
	void OnStatus();
	void OnPrompt();
	void OnResult();
	void OnMessage(BYTE code, BYTE p1);

	// What the buttons do. The Titus window calls the first two as well.
	void StartSearch();
	void CancelSearch();
	void AnswerPrompt(bool accept);

private:
	void AddToast(const char* text, DWORD rgba);
	void HideTitusWindow();

	void DrawToasts(DWORD now);
	void DrawPauseLine();
	void DrawChip(DWORD now);
	void DrawPrompt(DWORD now);
	void DrawBanner();

	bool  m_Click;			// the primary button went down this frame
	bool  m_WasDown;
	DWORD m_ActionTick;		// last button action, so one tap is one action
	DWORD m_PollTick;		// last STATUS poll while queued

	struct TOAST
	{
		char  text[100];
		DWORD color;
		DWORD tick;
	};
	enum { MAX_TOASTS = 3 };
	TOAST m_Toast[MAX_TOASTS];
	int   m_ToastCount;
};

extern CB_DuelRanked* gCB_DuelRanked;

// Drawing helpers shared with the Titus window. Colours are packed like RGBA() in
// _GlobalFunctions.h: red in the low byte, alpha in the high byte.
namespace RankedUi
{
	inline DWORD Col(unsigned r, unsigned g, unsigned b, unsigned a = 255)
	{
		return (DWORD)(r | (g << 8) | (b << 16) | (a << 24));
	}

	void  Bar(float x, float y, float w, float h, DWORD rgba);
	void  Outline(float x, float y, float w, float h, DWORD rgba);
	// w is the clip/align box width in virtual units (0 = draw unclipped from x);
	// sort is RT3_SORT_LEFT / RT3_SORT_CENTER / RT3_SORT_RIGHT.
	void  Text(HFONT font, float x, float y, float w, int sort, DWORD rgba, const char* text);
	// Same, centred vertically in a box of height h.
	void  TextBox(HFONT font, float x, float y, float w, float h, int sort, DWORD rgba, const char* text);
	float TextWidth(HFONT font, const char* text);
	float TextHeight(HFONT font);
	bool  Inside(float x, float y, float w, float h);

	void  FormatNumber(int value, char* out, size_t cb);			// 1250 -> "1,250"
	void  FormatClock(int seconds, char* out, size_t cb);			// 75 -> "01:15"
	void  RankText(BYTE tier, BYTE div, char* out, size_t cb);		// "GOLD III", "DUEL MASTER", "RANK 9"
	DWORD TierColor(BYTE tier);
	int   RankOrder(BYTE tier, BYTE div);							// bigger = higher rank

	float PillWidth(BYTE tier, BYTE div);
	void  DrawPill(float x, float y, float w, float h, BYTE tier, BYTE div);
	void  DrawShield(float x, float y, float w, float h, DWORD rgba);
	void  DrawDiamond(float cx, float cy, float r, DWORD rgba);

	// The small tier badge beside a character's name (shield in the tier colour, one pip per
	// division step: III = 3 pips, I = 1, none for a tier without divisions). Tier is 0 based.
	float RankBadgeWidth(BYTE div);
	float RankBadgeHeight();
	float RankBadgeWidth(BYTE tier, BYTE div);	// the badge art when DuelRankBadges.ozt is loaded, else the code shield
	float RankBadgeHeight(BYTE tier);
	void  DrawRankBadge(float x, float y, BYTE tier, BYTE div);
	// The badge art at any size (false = no art loaded, the caller draws its own).
	bool  DrawRankBadgeArt(float x, float y, float w, float h, BYTE tier, BYTE div);

	// style: 0 neutral, 1 green, 2 red, 3 gold
	void  DrawButton(float x, float y, float w, float h, const char* label, int style, bool enabled, bool bold);
}
