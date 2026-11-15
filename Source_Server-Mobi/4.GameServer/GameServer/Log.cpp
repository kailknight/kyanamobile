// Log.cpp: implementation of the CLog class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Log.h"
#include "ServerDisplayer.h"
#include "Util.h"

CLog gLog;
int gLogItemDropErrorsOnly = 0;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLog::CLog() // OK
{
	this->m_count = 0;

	this->m_overwrite = 1;

	this->m_maxBytes = 50*1024*1024;
	this->m_keepDays = 0;

	InitializeCriticalSection(&this->m_lock);
}

void CLog::SetPolicy(BOOL overwrite,DWORD maxMB) // OK
{
	this->m_overwrite = overwrite;

	this->m_maxBytes = ((maxMB>=4096)?0xFFFFFFFF:(maxMB*1024*1024));
}

void CLog::SetKeepDays(int days) // OK
{
	this->m_keepDays = ((days < 0)?0:days);
}

// Days since a fixed date, for comparing the dates in the file names.
static int LogDayNumber(int year,int month,int day) // OK
{
	SYSTEMTIME st;
	memset(&st,0,sizeof(st));
	st.wYear = (WORD)year;
	st.wMonth = (WORD)month;
	st.wDay = (WORD)day;

	FILETIME ft;

	if(SystemTimeToFileTime(&st,&ft) == 0)
	{
		return -1;
	}

	ULARGE_INTEGER value;
	value.LowPart = ft.dwLowDateTime;
	value.HighPart = ft.dwHighDateTime;

	return (int)(value.QuadPart/(10000000ULL*86400ULL));
}

// Deletes this category's dated files (YYYY-MM-DD.txt and YYYY-MM-DD.N.txt) that are more than m_keepDays old. Anything whose
// name does not start with a date is left alone (the category's own fixed file, other tools' files).
void CLog::DeleteOldFiles(LOG_INFO* lpInfo,const SYSTEMTIME& today) // OK
{
	if(this->m_keepDays <= 0)
	{
		return;
	}

	const int todayNumber = LogDayNumber(today.wYear,today.wMonth,today.wDay);

	if(todayNumber < 0)
	{
		return;
	}

	char pattern[300];
	wsprintf(pattern,".\\%s\\*.txt",lpInfo->Directory);

	WIN32_FIND_DATA data;
	HANDLE find = FindFirstFile(pattern,&data);

	if(find == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		int year = 0,month = 0,day = 0;

		if(strlen(data.cFileName) < 14 || data.cFileName[4] != '-' || data.cFileName[7] != '-' || sscanf_s(data.cFileName,"%4d-%2d-%2d",&year,&month,&day) != 3)
		{
			continue;
		}

		const int number = LogDayNumber(year,month,day);

		if(number >= 0 && (todayNumber-number) > this->m_keepDays)
		{
			char path[400];
			wsprintf(path,".\\%s\\%s",lpInfo->Directory,data.cFileName);
			DeleteFile(path);
		}
	}
	while(FindNextFile(find,&data) != 0);

	FindClose(find);
}

// Opens (or reopens) a category's file for the given day.
// Overwrite mode: <Directory>\<last folder name>.txt, kept only if it was last
// written today and is under the size limit, otherwise emptied.
// Dated mode: <Directory>\YYYY-MM-DD.txt, appended to (the original behaviour).
bool CLog::OpenFile(LOG_INFO* lpInfo,const SYSTEMTIME& today)
{
	if(this->m_keepDays > 0)
	{
		if(lpInfo->Part <= 1)
		{
			wsprintf(lpInfo->Filename,".\\%s\\%04d-%02d-%02d.txt",lpInfo->Directory,today.wYear,today.wMonth,today.wDay);
		}
		else
		{
			wsprintf(lpInfo->Filename,".\\%s\\%04d-%02d-%02d.%d.txt",lpInfo->Directory,today.wYear,today.wMonth,today.wDay,lpInfo->Part);
		}
	}
	else if(this->m_overwrite == 0)
	{
		wsprintf(lpInfo->Filename,".\\%s\\%04d-%02d-%02d.txt",lpInfo->Directory,today.wYear,today.wMonth,today.wDay);
	}
	else
	{
		const char* name = strrchr(lpInfo->Directory,'\\');

		name = ((name==0)?lpInfo->Directory:(name+1));

		wsprintf(lpInfo->Filename,".\\%s\\%s.txt",lpInfo->Directory,name);
	}

	lpInfo->File = CreateFile(lpInfo->Filename,GENERIC_WRITE,FILE_SHARE_READ,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);

	if(lpInfo->File == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	lpInfo->Size = 0;

	if(this->m_keepDays > 0)
	{
		// appended to (a restart the same day continues the day's file); the size is the current length
		DWORD existing = GetFileSize(lpInfo->File,0);
		lpInfo->Size = ((existing == INVALID_FILE_SIZE)?0:existing);
	}
	else if(this->m_overwrite != 0)
	{
		FILETIME written;
		SYSTEMTIME utc,local;
		bool keep = false;

		if(GetFileTime(lpInfo->File,0,0,&written) != 0 && FileTimeToSystemTime(&written,&utc) != 0 && SystemTimeToTzSpecificLocalTime(0,&utc,&local) != 0)
		{
			keep = (local.wDay == today.wDay && local.wMonth == today.wMonth && local.wYear == today.wYear);
		}

		DWORD size = GetFileSize(lpInfo->File,0);

		if(keep == 0 || size == INVALID_FILE_SIZE || (this->m_maxBytes != 0 && size >= this->m_maxBytes))
		{
			SetFilePointer(lpInfo->File,0,0,FILE_BEGIN);
			SetEndOfFile(lpInfo->File);
			return 1;
		}

		lpInfo->Size = size;
	}

	if(SetFilePointer(lpInfo->File,0,0,FILE_END) == INVALID_SET_FILE_POINTER)
	{
		CloseHandle(lpInfo->File);
		return 0;
	}

	return 1;
}

CLog::~CLog() // OK
{
	for(int n=0;n < this->m_count;n++)
	{
		if(this->m_LogInfo[n].Active != 0)
		{
			CloseHandle(this->m_LogInfo[n].File);
		}
	}
}

void CLog::AddLog(BOOL active,char* directory) // OK
{
	if(this->m_count < 0 || this->m_count >= MAX_LOG)
	{
		return;
	}

	LOG_INFO* lpInfo = &this->m_LogInfo[this->m_count++];

	lpInfo->Active = active;

	strcpy_s(lpInfo->Directory,directory);

	if(lpInfo->Active != 0)
	{
		CreateDirectory(lpInfo->Directory,0);

		SYSTEMTIME time;

		GetLocalTime(&time);

		lpInfo->Day = time.wDay;

		lpInfo->Month = time.wMonth;

		lpInfo->Year = time.wYear;
		lpInfo->Part = 1;

		if(this->OpenFile(lpInfo,time) == 0)
		{
			lpInfo->Active = 0;
			return;
		}

		this->DeleteOldFiles(lpInfo,time);
	}
}

// Maps an on-disk log category to a log-panel view, or -1 for "do not show on
// screen". Only the four categories that have a corresponding menu view are
// mapped; LOG_GENERAL must never appear here or CServerDisplayer::LogAddText's
// own gLog.Output(LOG_GENERAL,...) call would recurse.
static int GetLogScreenCategory(eLogType type) // OK
{
	switch(type)
	{
		case LOG_CHAT:		return LOG_CAT_CHAT;
		case LOG_COMMAND:	return LOG_CAT_COMMAND;
		case LOG_CHAOS_MIX:	return LOG_CAT_CHAOSMIX;
		case LOG_CASH_SHOP:	return LOG_CAT_CASHSHOP;
		default:			break;
	}

	return -1;
}

void CLog::Output(eLogType type,char* text,...) // OK
{
	char composed[1024] = {0};

	va_list arg;
	va_start(arg,text);
	vsprintf_s(composed,text,arg);
	va_end(arg);

	// Mirror to the log panel BEFORE WriteFileLine's early returns. Those fire
	// when a category is switched off in [Log Settings], when its folder cannot
	// be created, or when a midnight file reopen fails - none of which should
	// silently empty an on-screen view. The screen view is a separate feature
	// from the disk file and must not depend on it.
	int screenCategory = GetLogScreenCategory(type);

	if(screenCategory >= 0)
	{
		// "%s" - composed is data now, not a format string.
		LogAddCat(screenCategory,LOG_BLACK,"%s",composed);
	}

	this->WriteFileLine(type,composed);
}

void CLog::OutputNoMirror(eLogType type,char* text,...) // OK
{
	char composed[1024] = {0};

	va_list arg;
	va_start(arg,text);
	vsprintf_s(composed,text,arg);
	va_end(arg);

	this->WriteFileLine(type,composed);
}

void CLog::WriteFileLine(eLogType type,char* composed) // OK
{
	if(type < 0 || type >= this->m_count)
	{
		return;
	}

	LOG_INFO* lpInfo = &this->m_LogInfo[type];

	if(lpInfo->Active == 0)
	{
		return;
	}

	SYSTEMTIME time;

	GetLocalTime(&time);

	EnterCriticalSection(&this->m_lock);

	if(time.wDay != lpInfo->Day || time.wMonth != lpInfo->Month || time.wYear != lpInfo->Year)
	{
		CloseHandle(lpInfo->File);

		lpInfo->Day = time.wDay;

		lpInfo->Month = time.wMonth;

		lpInfo->Year = time.wYear;
		lpInfo->Part = 1;

		// A new day: overwrite mode empties the same file, dated mode opens the new day's.
		if(this->OpenFile(lpInfo,time) == 0)
		{
			lpInfo->Active = 0;
			LeaveCriticalSection(&this->m_lock);
			return;
		}

		this->DeleteOldFiles(lpInfo,time);
	}
	else if(this->m_keepDays > 0 && this->m_maxBytes != 0 && lpInfo->Size >= this->m_maxBytes)
	{
		// Keep-days mode never empties a file: past the size limit the day continues in YYYY-MM-DD.2.txt, .3.txt ...
		CloseHandle(lpInfo->File);
		lpInfo->Part++;

		if(this->OpenFile(lpInfo,time) == 0)
		{
			lpInfo->Active = 0;
			LeaveCriticalSection(&this->m_lock);
			return;
		}
	}
	else if(this->m_overwrite != 0 && this->m_maxBytes != 0 && lpInfo->Size >= this->m_maxBytes)
	{
		// Past the size limit: start the file again from empty.
		SetFilePointer(lpInfo->File,0,0,FILE_BEGIN);
		SetEndOfFile(lpInfo->File);
		lpInfo->Size = 0;
	}

	char buff[1024] = {0};

	wsprintf(buff,"%02d:%02d:%02d %s\r\n",time.wHour,time.wMinute,time.wSecond,composed);

	DWORD OutSize = 0;

	if(WriteFile(lpInfo->File,buff,strlen(buff),&OutSize,0) != 0)
	{
		lpInfo->Size += OutSize;
	}

	LeaveCriticalSection(&this->m_lock);
}
