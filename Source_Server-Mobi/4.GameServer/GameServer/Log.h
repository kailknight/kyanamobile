// Log.h: interface for the CLog class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#define MAX_LOG 20

enum eLogType
{
	LOG_GENERAL = 0,
	LOG_CHAT = 1,
	LOG_COMMAND = 2,
	LOG_TRADE = 3,
	LOG_CONNECT = 4,
	LOG_HACK = 5,
	LOG_CASH_SHOP = 6,
	LOG_CHAOS_MIX = 7,
	LOG_ANTIFLOOD = 8,
	LOG_BANK_JEWEL = 9,
	LOG_THUMUA = 10,
	LOG_ITEMBAGDROP = 11,
	LOG_CONG_HUONG = 12,
	LOG_ITEM_CHARACTER = 13,
	LOG_SHOP = 14,
	LOG_WC = 15,
	LOG_MocNap = 16,
	LOG_DEBUG = 17,
	LOG_ITEM_HomDo = 18,
	LOG_TRADEBOT = 19,
};

struct LOG_INFO
{
	BOOL Active;
	char Directory[256];
	int Day;
	int Month;
	int Year;
	char Filename[256];
	HANDLE File;
	DWORD Size;
};

class CLog
{
public:
	CLog();
	virtual ~CLog();
	void AddLog(BOOL active,char* directory);
	// File policy (call before AddLog). overwrite=1: one fixed file per category
	// (<Directory><Name>.txt) that is emptied when the day changes and when it
	// passes maxMB (0 = no size limit). overwrite=0: the old one-dated-file-per-day.
	void SetPolicy(BOOL overwrite,DWORD maxMB);
	void Output(eLogType type,char* text,...);
	// Writes the file exactly like Output but never mirrors to the log panel.
	// For high-frequency forensic dumps that are worth keeping on disk and
	// would drown a category's on-screen view.
	void OutputNoMirror(eLogType type,char* text,...);
private:
	void WriteFileLine(eLogType type,char* composed);
	bool OpenFile(LOG_INFO* lpInfo,const SYSTEMTIME& today);
	LOG_INFO m_LogInfo[MAX_LOG];
	int m_count;
	BOOL m_overwrite;
	DWORD m_maxBytes;
	CRITICAL_SECTION m_lock;
};

// 1 = LOG_ITEMBAGDROP only records the failures (items that could not be dropped),
// not one block per successful drop. Set from [GameServerInfo] LogItemDropErrorsOnly.
extern int gLogItemDropErrorsOnly;

extern CLog gLog;
