// Log.cpp: implementation of the CLog class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Log.h"

CLog gLog;

namespace
{
	// A single day's log past this is emptied and starts again, so even a
	// day of error spam cannot fill the disk.
	const DWORD kMaxLogBytes = 64 * 1024 * 1024;

	void EmptyLog(LOG_INFO* lpInfo)
	{
		SetFilePointer(lpInfo->File,0,0,FILE_BEGIN);
		SetEndOfFile(lpInfo->File);
		lpInfo->Size = 0;
	}

	// Opens the log, keeping what is already there only if it was written
	// today (a restart during the day carries on the same log).
	bool OpenLog(LOG_INFO* lpInfo,const SYSTEMTIME& today)
	{
		wsprintf(lpInfo->Filename,".\\%s\\DataServer.txt",lpInfo->Directory);

		lpInfo->File = CreateFile(lpInfo->Filename,GENERIC_WRITE,FILE_SHARE_READ,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);

		if(lpInfo->File == INVALID_HANDLE_VALUE)
		{
			return 0;
		}

		FILETIME written;
		SYSTEMTIME utc,local;
		bool keep = false;

		if(GetFileTime(lpInfo->File,0,0,&written) != 0 && FileTimeToSystemTime(&written,&utc) != 0 && SystemTimeToTzSpecificLocalTime(0,&utc,&local) != 0)
		{
			keep = (local.wDay == today.wDay && local.wMonth == today.wMonth && local.wYear == today.wYear);
		}

		DWORD size = GetFileSize(lpInfo->File,0);

		if(keep == 0 || size == INVALID_FILE_SIZE || size >= kMaxLogBytes)
		{
			EmptyLog(lpInfo);
			return 1;
		}

		if(SetFilePointer(lpInfo->File,0,0,FILE_END) == INVALID_SET_FILE_POINTER)
		{
			CloseHandle(lpInfo->File);
			return 0;
		}

		lpInfo->Size = size;
		return 1;
	}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLog::CLog() // OK
{
	this->m_count = 0;
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

		if(OpenLog(lpInfo,time) == 0)
		{
			lpInfo->Active = 0;
			return;
		}
	}
}

void CLog::Output(eLogType type,char* text,...) // OK
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

	if(time.wDay != lpInfo->Day || time.wMonth != lpInfo->Month || time.wYear != lpInfo->Year)
	{
		// A new day: the same file starts empty instead of a new dated one.
		lpInfo->Day = time.wDay;

		lpInfo->Month = time.wMonth;

		lpInfo->Year = time.wYear;

		EmptyLog(lpInfo);
	}
	else if(lpInfo->Size >= kMaxLogBytes)
	{
		EmptyLog(lpInfo);
	}

	char temp[1024] = {0};

	va_list arg;
	va_start(arg,text);
	vsprintf_s(temp,text,arg);
	va_end(arg);

	char buff[1024] = {0};

	wsprintf(buff,"%02d:%02d:%02d %s\r\n",time.wHour,time.wMinute,time.wSecond,temp);

	DWORD OutSize;

	if(WriteFile(lpInfo->File,buff,strlen(buff),&OutSize,0) != 0)
	{
		lpInfo->Size += OutSize;
	}
}
