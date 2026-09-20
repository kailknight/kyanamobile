// ServerDisplayer.h: interface for the CServerDisplayer class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "CriticalSection.h"

// Scrollback capacity for the main log panel. This is the HISTORY size, not the
// number of rows on screen - how many rows are visible is derived from the log
// rect at paint time (GetLogVisibleRows), so growing this does not push text
// off the bottom of the window the way the old fixed 41-line layout did.
// 3000 * sizeof(LOG_DISPLAY_INFO) is about 300 KB of .bss.
#define MAX_LOG_HISTORY 3000
#define MAX_LOG_TEXT_SIZE 100

// Layout constants for the log panel, all verified against what the other
// CServerDisplayer painters occupy:
//   - the header bands (PaintName y[0,60] + the STANDBY/ONLINE/SEASON trio
//     y[60,85]) own everything above LOG_AREA_TOP.
//   - every right-hand panel starts at rect.right - LOG_PANEL_RIGHT_MARGIN,
//     so that x is the layout seam the log must stay left of.
//   - the scrollbar sits just inside that seam; the text stops before it.
#define LOG_LINE_HEIGHT 15
#define LOG_AREA_TOP 85
#define LOG_PANEL_RIGHT_MARGIN 450
#define LOG_SCROLLBAR_WIDTH 17

#define MAX_LOGCONNECT_TEXT_LINE 13
#define MAX_LOGCONNECT_TEXT_SIZE 55

#define MAX_LOGGLOBAL_TEXT_LINE 8
#define MAX_LOGGLOBAL_TEXT_SIZE 100

enum eLogColor
{
	LOG_BLACK = 0,
	LOG_RED = 1,
	LOG_GREEN = 2,
	LOG_BLUE = 3,
	//MC bot
	LOG_BOT = 4,
	LOG_USER = 5,
	LOG_EVENT = 6,
	LOG_ALERT = 7,
	//MC bot
};

// Which view of the main log panel a line belongs to. This is a DISPLAY
// category and is deliberately independent of eLogType (Log.h), which selects
// an on-disk folder - a line can be screen-categorised without having a file
// log, and vice versa.
//
// LOG_CAT_COMMON must stay 0: the rings are memset to 0, so every one of the
// ~595 existing LogAdd() call sites lands in Common with no edit.
//
// Each category owns its own ring, so a high-traffic category (chat, chaos mix)
// can never evict lines from another view - which one shared ring could not
// prevent no matter how the display filtered it.
enum eLogCategory
{
	LOG_CAT_COMMON = 0,
	LOG_CAT_CHAT = 1,
	LOG_CAT_EVENT = 2,
	LOG_CAT_REDEEM = 3,
	LOG_CAT_CHAOSMIX = 4,
	LOG_CAT_COMMAND = 5,
	LOG_CAT_SPINWHEEL = 6,
	LOG_CAT_MARKET = 7,
	LOG_CAT_CASHSHOP = 8,
	LOG_CAT_SLOTMACHINE = 9,
	LOG_CAT_COUNT = 10,
};

struct LOG_DISPLAY_INFO
{
	char text[MAX_LOG_TEXT_SIZE];
	eLogColor color;
};

struct LOGCONNECT_DISPLAY_INFO
{
	char text[MAX_LOGCONNECT_TEXT_SIZE];
	eLogColor color;
};

struct LOGGLOBAL_DISPLAY_INFO
{
	char text[MAX_LOGGLOBAL_TEXT_SIZE];
	eLogColor color;
};

class CServerDisplayer
{
public:
	CServerDisplayer();
	virtual ~CServerDisplayer();
	void Init(HWND hWnd);
	void Run();
	void background();
	void SetWindowName();
	void PaintAllInfo();
	void LogTextPaint();
	void LogTextPaintConnect();
	//void LogTextPaintGlobalMessage();
	void PaintName();
	void PaintEventTime();
	void PaintInvasionTime();
	void PaintCustomArenaTime();
	void LogAddText(eLogColor color, char* text, int size);
	// Ring push WITHOUT the gLog.Output(LOG_GENERAL,...) file write that
	// LogAddText does. Anything already destined for a file (the CLog::Output
	// mirror) must come through here, or every mirrored line would be written
	// into the general log a second time.
	void LogAddTextCat(int category, eLogColor color, char* text, int size);
	void LogAddTextConnect(eLogColor color, char* text, int size);
	void LogAddTextGlobal(eLogColor color, char* text, int size);

	int GetLogView();
	void SetLogView(int category);
	static char* GetLogCategoryName(int category);

	// Log panel geometry + scrollback control. Everything here runs on the
	// message thread only (the WM_TIMER_2000 paint, WM_VSCROLL, WM_MOUSEWHEEL
	// and the Logs menu all arrive there), so the scroll state itself needs no
	// lock - only the shared m_log buffer does, and that is handled internally.
	void GetLogRect(RECT* lpRect);
	void GetLogScrollBarRect(RECT* lpRect);
	int GetLogVisibleRows();
	int GetLogScrollMax();
	int GetLogScrollPos();
	void LogScrollToPos(int pos);
	void LogScrollLines(int delta);
	void LogScrollToLatest();
	void LogClear();
	int GetLogAutoScroll();
	void SetLogAutoScroll(int enable);
	void UpdateLogScrollBar();
	// Repaints ONLY the log rect + scrollbar, leaving the header and the
	// right-hand panels untouched. Run()'s LogTextPaint still whitewashes the
	// whole client first (the panels have small unpainted gaps that rely on
	// it), so this narrow version is what scroll input calls to get an
	// immediate response instead of waiting up to 2s for the next timer tick.
	void LogTextPaintNow();
	void PaintOnline();
	void PaintPremium();
	void PaintSeason();

	int EventBc;
	int EventDs;
	int EventCc;
	int EventIt;
	int EventCustomLottery;
	int EventCustomBonus;
	//int EventCustomArena;
	int EventCustomQuiz;
	int EventMoss;
	int EventKing;
	int EventDrop;
	int EventTvT;
	int EventInvasion[30];
	int EventCustomArena[30];
	int EventCs;
	int EventCsState;
	int EventCastleDeep;
	int EventCryWolf;
	int EventCryWolfState;
	int EventBossGuild;
	int EventCTCMini;
	int EventLoanChien;
	int EventSelupan;
	int EventKanturu;
	void InitTimeEvent();
	void ClearTimeDisplayer();
private:
	void DrawLogArea(HDC hdc);
	HWND m_hwnd;
	HFONT m_font;
	HFONT m_font2;
	HFONT m_font3;
	HFONT m_font4;
	HFONT m_font5;
	HBRUSH m_brush[5];
	// One independent ring per view. ~2.8 MB of .bss at 9 x 3000 x 104 bytes,
	// which buys complete flood isolation between categories.
	LOG_DISPLAY_INFO m_log[LOG_CAT_COUNT][MAX_LOG_HISTORY];
	LOGCONNECT_DISPLAY_INFO m_logConnect[MAX_LOGCONNECT_TEXT_LINE];
	LOGGLOBAL_DISPLAY_INFO m_logGlobal[MAX_LOGGLOBAL_TEXT_LINE];
	int m_count[LOG_CAT_COUNT];
	int m_countConnect;
	int m_countGlobal;
	// m_log is written from several threads (the message thread, the packet
	// queue thread and the timer-queue callbacks) and read by the paint code on
	// the message thread. It carried no lock at all before the scrollback
	// change; the old fixed-array layout made the race survivable by accident,
	// which is no longer something to rely on.
	CCriticalSection m_logCritical;
	int m_logTotal[LOG_CAT_COUNT];		// valid lines, saturating at MAX_LOG_HISTORY
	// Scroll state is per view so switching away and back keeps your place.
	// Touched only from the message thread (paint, wheel, scrollbar, menu), so
	// unlike the rings it needs no lock.
	int m_logScrollPos[LOG_CAT_COUNT];	// 0 = oldest line at the top of the panel
	int m_logAutoScroll[LOG_CAT_COUNT];	// 1 = keep the newest line pinned to the bottom
	int m_logView;						// category currently shown in the panel
	char m_DisplayerText[2][64];
	int CountTimeEventS;
};

extern CServerDisplayer gServerDisplayer;
