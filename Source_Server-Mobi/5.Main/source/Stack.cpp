#include "StdAfx.h"
#include "StackWalker.h"
#include <tchar.h>
#include <time.h>
#include <exception>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")


static TCHAR s_szExceptionLogFileName[_MAX_PATH] = _T("\\exceptions.log");  // default
static BOOL s_bUnhandledExeptionFilterSet = FALSE;

// Log Set Internals
int LogMDay;
int LogMonth;
int LogMYear;
BOOL GetTime = 0;

class StackWalkerToConsole : public StackWalker
{
private:
	FILE* file;
	char szFile[256];
	struct tm* today;
	time_t ltime;

public:
	StackWalkerToConsole()
	{
		if (GetTime == FALSE)
		{
			CreateDirectory("STACK_ERROR", 0);
			time(&ltime);
			today = new tm;
			localtime_s(today, &ltime);
			today->tm_year = today->tm_year + 1900;
			LogMYear = today->tm_year;
			LogMonth = today->tm_mon + 1;
			LogMDay = today->tm_mday;
			wsprintf(szFile, "STACK_ERROR\\stack_%02d%02d%02d_%02d%02d.log", LogMYear, LogMonth, LogMDay, today->tm_hour, today->tm_min);
			GetTime = TRUE;
		}
		fopen_s(&file, szFile, "w");
	}

	~StackWalkerToConsole()
	{
		fclose(file);
	}
	// Writes a line straight into the same log the callstack goes to. Used to
	// record the exception record, which was previously formatted and dropped.
	void WriteLine(const char* fmt, ...)
	{
		if (file == NULL)
		{
			return;
		}

		char buf[512];
		va_list ap;
		va_start(ap, fmt);
		_vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
		va_end(ap);
		fprintf(file, "%s\n", buf);
	}

	char* GetStackLogFileName()
	{
		return szFile;
	}
protected:
	virtual void OnOutput(LPCTSTR szText)
	{
		char szMsg[512]; memset(szMsg, 0, 512);
		//base64.Encrypt(szText,strlen(szText),szMsg);
		wsprintf(szMsg, "%s", szText);
		fprintf(file, szMsg);
		fprintf(file, "\n");
	}
};

LPTOP_LEVEL_EXCEPTION_FILTER WINAPI MyDummySetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
{
	return NULL;
}
//
//static BOOL PreventSetUnhandledExceptionFilter()
//{
//	HMODULE hKernel32 = LoadLibrary(_T("kernel32.dll"));
//	if (hKernel32 == NULL)
//		return FALSE;
//	void* pOrgEntry = GetProcAddress(hKernel32, "SetUnhandledExceptionFilter");
//	if (pOrgEntry == NULL)
//		return FALSE;
//
//#ifdef _M_IX86
//	// Code for x86:
//	// 33 C0                xor         eax,eax
//	// C2 04 00             ret         4
//	unsigned char szExecute[] = { 0x33, 0xC0, 0xC2, 0x04, 0x00 };
//#elif _M_X64
//	// 33 C0                xor         eax,eax
//	// C3                   ret
//	unsigned char szExecute[] = { 0x33, 0xC0, 0xC3 };
//#else
//#error "The following code only works for x86 and x64!"
//#endif
//
//	DWORD dwOldProtect = 0;
//	BOOL  bProt = VirtualProtect(pOrgEntry, sizeof(szExecute), PAGE_EXECUTE_READWRITE, &dwOldProtect);
//
//	SIZE_T bytesWritten = 0;
//	BOOL   bRet = WriteProcessMemory(GetCurrentProcess(), pOrgEntry, szExecute, sizeof(szExecute),
//		&bytesWritten);
//
//	if ((bProt != FALSE) && (dwOldProtect != PAGE_EXECUTE_READWRITE))
//	{
//		DWORD dwBuf;
//		VirtualProtect(pOrgEntry, sizeof(szExecute), dwOldProtect, &dwBuf);
//	}
//	return bRet;
//}


BOOL PreventSetUnhandledExceptionFilter()
{
	HMODULE hKernel32 = LoadLibrary(_T("kernel32.dll"));
	if (hKernel32 == NULL)
	{
		return FALSE;
	}

	void* pOrgEntry = GetProcAddress(hKernel32, "SetUnhandledExceptionFilter");
	if (pOrgEntry == NULL)
	{
		return FALSE;
	}

	DWORD dwOldProtect = 0;
	SIZE_T jmpSize = 5;
#ifdef _M_X64
	jmpSize = 13;
#endif
	BOOL bProt = VirtualProtect(pOrgEntry, jmpSize,
		PAGE_EXECUTE_READWRITE, &dwOldProtect);
	BYTE newJump[20];
	void* pNewFunc = &MyDummySetUnhandledExceptionFilter;
#ifdef _M_IX86
	DWORD dwOrgEntryAddr = (DWORD)pOrgEntry;
	dwOrgEntryAddr += jmpSize; // add 5 for 5 op-codes for jmp rel32
	DWORD dwNewEntryAddr = (DWORD)pNewFunc;
	DWORD dwRelativeAddr = dwNewEntryAddr - dwOrgEntryAddr;
	// JMP rel32: Jump near, relative, displacement relative to next instruction.
	newJump[0] = 0xE9;  // JMP rel32
	memcpy(&newJump[1], &dwRelativeAddr, sizeof(pNewFunc));
#elif _M_X64
	newJump[0] = 0x49;  // MOV R15, ...
	newJump[1] = 0xBF;  // ...
	memcpy(&newJump[2], &pNewFunc, sizeof(pNewFunc));
	//pCur += sizeof (ULONG_PTR);
	newJump[10] = 0x41;  // JMP R15, ...
	newJump[11] = 0xFF;  // ...
	newJump[12] = 0xE7;  // ...
#endif
	SIZE_T bytesWritten;
	BOOL bRet = WriteProcessMemory(GetCurrentProcess(), pOrgEntry, newJump, jmpSize, &bytesWritten);

	if (bProt != FALSE)
	{
		DWORD dwBuf;
		VirtualProtect(pOrgEntry, jmpSize, dwOldProtect, &dwBuf);
	}
	return bRet;
}

// dbghelp symbols never load on player machines, so every callstack frame prints
// "module-name not available". These helpers name the module and offset straight
// from the loaded-module list; Main.exe offsets resolve against its .map file.
static void DescribeAddress(DWORD Address, char* Out, size_t OutSize)
{
	HMODULE Module = NULL;
	char Path[MAX_PATH] = "?";

	if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)Address, &Module) && Module != NULL)
	{
		GetModuleFileName(Module, Path, MAX_PATH);
		const char* Name = strrchr(Path, '\\');
		sprintf_s(Out, OutSize, "%s+0x%X", Name != NULL ? Name + 1 : Path, Address - (DWORD)Module);
	}
	else
	{
		sprintf_s(Out, OutSize, "0x%08X (no module)", Address);
	}
}

static bool IsExecutableAddress(DWORD Address)
{
	MEMORY_BASIC_INFORMATION Info;

	if (VirtualQuery((LPCVOID)Address, &Info, sizeof(Info)) == 0 || Info.State != MEM_COMMIT)
	{
		return false;
	}

	return (Info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

// 0xE06D7363 is a C++ throw. ExceptionInformation[1] is the thrown object and [2] the
// ThrowInfo, whose first catchable type is the thrown type's decorated name.
static void LogCppException(StackWalkerToConsole& sw, EXCEPTION_RECORD* lpRecord)
{
	if (lpRecord->NumberParameters < 3)
	{
		return;
	}

	__try
	{
		DWORD* ThrowInfo = (DWORD*)lpRecord->ExceptionInformation[2];
		DWORD* TypeArray = (DWORD*)ThrowInfo[3];
		int Count = (int)TypeArray[0];
		bool IsStdException = false;

		for (int i = 0; i < Count && i < 8; i++)
		{
			DWORD* Catchable = (DWORD*)TypeArray[1 + i];
			const char* TypeName = (const char*)(((DWORD*)Catchable[1]) + 2);

			sw.WriteLine("CppException type[%d]: %s", i, TypeName);

			if (strcmp(TypeName, ".?AVexception@std@@") == 0)
			{
				IsStdException = true;
			}
		}

		if (IsStdException)
		{
			sw.WriteLine("CppException what(): %s", ((std::exception*)lpRecord->ExceptionInformation[1])->what());
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		sw.WriteLine("CppException: could not read the throw info");
	}
}

static void LogProcessState(StackWalkerToConsole& sw)
{
	MEMORYSTATUSEX Mem;
	Mem.dwLength = sizeof(Mem);

	if (GlobalMemoryStatusEx(&Mem))
	{
		sw.WriteLine("Memory: address space free %u MB of %u MB, physical free %u MB",
					 (DWORD)(Mem.ullAvailVirtual >> 20), (DWORD)(Mem.ullTotalVirtual >> 20), (DWORD)(Mem.ullAvailPhys >> 20));
	}

	PROCESS_MEMORY_COUNTERS Counters;

	if (GetProcessMemoryInfo(GetCurrentProcess(), &Counters, sizeof(Counters)))
	{
		sw.WriteLine("Process: working set %u MB, peak %u MB, pagefile %u MB",
					 (DWORD)(Counters.WorkingSetSize >> 20), (DWORD)(Counters.PeakWorkingSetSize >> 20), (DWORD)(Counters.PagefileUsage >> 20));
	}

	DWORD HandleCount = 0;

	if (GetProcessHandleCount(GetCurrentProcess(), &HandleCount))
	{
		sw.WriteLine("Handle count: %u", HandleCount);
	}
}

static void LogModuleCallstack(StackWalkerToConsole& sw, CONTEXT* Context)
{
	char Text[MAX_PATH + 32];

	DescribeAddress(Context->Eip, Text, sizeof(Text));
	sw.WriteLine("Fault   : %s", Text);

	// FPO makes the EBP chain unreliable, so scan the stack for return addresses.
	// A word that points into executable memory and just after a call is a caller.
	DWORD* Stack = (DWORD*)Context->Esp;
	int Printed = 0;

	__try
	{
		for (int i = 0; i < 4096 && Printed < 40; i++)
		{
			DWORD Value = Stack[i];

			if (Value < 0x10000 || !IsExecutableAddress(Value))
			{
				continue;
			}

			BYTE* Code = (BYTE*)Value;

			// E8 rel32 (call) ends 5 bytes before; FF /2 forms end 2 or 6 bytes before.
			bool AfterCall = (IsExecutableAddress(Value - 5) && Code[-5] == 0xE8)
				|| (IsExecutableAddress(Value - 2) && Code[-2] == 0xFF)
				|| (IsExecutableAddress(Value - 6) && Code[-6] == 0xFF);

			if (!AfterCall)
			{
				continue;
			}

			DescribeAddress(Value, Text, sizeof(Text));
			sw.WriteLine("Stack[%4d]: %s", i, Text);
			Printed++;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

static LONG __stdcall CrashHandlerExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
#ifdef _M_IX86
	if (pExPtrs->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
	{
		static char MyStack[1024 * 128];  // be sure that we have enought space...
		// it assumes that DS and SS are the same!!! (this is the case for Win32)
		// change the stack only if the selectors are the same (this is the case for Win32)
		//__asm push offset MyStack[1024*128];
		//__asm pop esp;
		__asm mov eax, offset MyStack[1024 * 128];
		__asm mov esp, eax;
	}
#endif

	StackWalkerToConsole sw;  // output to console

	// The exception record used to be formatted into a string and then dropped -
	// FatalAppExit below is commented out, so nothing ever saw it. Without the
	// code and the faulting address, an optimised build's line numbers are the
	// only evidence, and those name the nearest attributable line rather than the
	// instruction that actually faulted. Log it.
	{
		EXCEPTION_RECORD* lpRecord = pExPtrs->ExceptionRecord;

		sw.WriteLine("ExceptionCode: 0x%08X  Flags: 0x%08X  Address: 0x%08X",
					 lpRecord->ExceptionCode,
					 lpRecord->ExceptionFlags,
					 (DWORD)lpRecord->ExceptionAddress);

		if (lpRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && lpRecord->NumberParameters >= 2)
		{
			const char* Access = "read";

			if (lpRecord->ExceptionInformation[0] == 1)
			{
				Access = "write";
			}
			else if (lpRecord->ExceptionInformation[0] == 8)
			{
				Access = "execute";
			}

			sw.WriteLine("AccessViolation: %s of 0x%08X", Access, (DWORD)lpRecord->ExceptionInformation[1]);
		}

		if (pExPtrs->ContextRecord != NULL)
		{
			sw.WriteLine("EIP: 0x%08X  ESP: 0x%08X  EBP: 0x%08X",
						 pExPtrs->ContextRecord->Eip,
						 pExPtrs->ContextRecord->Esp,
						 pExPtrs->ContextRecord->Ebp);
		}

		if (lpRecord->ExceptionCode == 0xE06D7363)
		{
			LogCppException(sw, lpRecord);
		}

		LogProcessState(sw);

		if (pExPtrs->ContextRecord != NULL)
		{
			LogModuleCallstack(sw, pExPtrs->ContextRecord);
		}
	}
	sw.ShowCallstack(GetCurrentThread(), pExPtrs->ContextRecord);
	TCHAR lString[500];
	/*_stprintf_s(lString,
		_T("*** Unhandled Exception! See console output for more infos!\n")
		_T("   ExpCode: 0x%8.8X\n")
		_T("   ExpFlags: %d\n")
		_T("   ExpAddress: 0x%8.8X\n")
		_T("   Please report!"),
		pExPtrs->ExceptionRecord->ExceptionCode,
		pExPtrs->ExceptionRecord->ExceptionFlags,
		pExPtrs->ExceptionRecord->ExceptionAddress);*/

	_stprintf_s(lString, "*** Please Send %s to Server Admin! ***", sw.GetStackLogFileName());
	//FatalAppExit(-1, lString);
	return EXCEPTION_CONTINUE_SEARCH;
}

static void InitUnhandledExceptionFilter()
{
	TCHAR szModName[_MAX_PATH];
	if (GetModuleFileName(NULL, szModName, sizeof(szModName) / sizeof(TCHAR)) != 0)
	{
		_tcscpy_s(s_szExceptionLogFileName, szModName);
		_tcscat_s(s_szExceptionLogFileName, _T(".exp.log"));
	}
	if (s_bUnhandledExeptionFilterSet == FALSE)
	{
		// set global exception handler (for handling all unhandled exceptions)
		SetUnhandledExceptionFilter(CrashHandlerExceptionFilter);
#if defined _M_X64 || defined _M_IX86
		PreventSetUnhandledExceptionFilter();
#endif
		s_bUnhandledExeptionFilterSet = TRUE;
	}
}

void StartStackLogging()
{
	InitUnhandledExceptionFilter();
}