// Log.h: interface for the CLog class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#define MAX_LOG 1

enum eLogType
{
	LOG_GENERAL = 0,
};

// One file per log (<Directory>\DataServer.txt) that is emptied when the day
// changes, rather than a new dated file every day: the log never holds more
// than today, plus a size cap for a runaway day.
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
	void Output(eLogType type,char* text,...);
private:
	LOG_INFO m_LogInfo[MAX_LOG];
	int m_count;
};

extern CLog gLog;
