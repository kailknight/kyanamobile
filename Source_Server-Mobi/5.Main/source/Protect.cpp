// Protect.cpp: implementation of the CProtect class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Protect.h"
#include ".\\Utilities\\CCRC32.H"
#include "Util.h"

CProtect gProtect;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CProtect::CProtect() // OK
{
	ZeroMemory(&this->m_MainInfo, sizeof(this->m_MainInfo));
	ZeroMemory(&this->m_TextInfo, sizeof(this->m_TextInfo));
}

CProtect::~CProtect() // OK
{

}

BYTE EncDecKey1;
BYTE EncDecKey2;
bool CProtect::CheckSocketPort(SOCKET s) // OK1
{
	SOCKADDR_IN addr;
	int addr_len = sizeof(addr);

	if (getpeername(s, (SOCKADDR*)& addr, &addr_len) == SOCKET_ERROR)
	{
		return 0;
	}

	int port = ntohs(addr.sin_port);
	if ((port < gProtect.m_MainInfo.GSPortMin || port > gProtect.m_MainInfo.GSPortMax))
	{
		return 0;
	}

	return 1;
}
void CProtect::DecryptData(BYTE* lpMsg, int size) // OK
{
	for (int n = 0; n < size; n++)
	{
		lpMsg[n] ^= EncDecKey1;
		lpMsg[n] -= EncDecKey2;
	}
}

void CProtect::EncryptData(BYTE* lpMsg, int size) // OK
{
	for (int n = 0; n < size; n++)
	{
		lpMsg[n] += EncDecKey2;
		lpMsg[n] ^= EncDecKey1;
	}
}
void CProtect::LoadEncDec()
{
#if(ENCRYPT_STATE==1)
	//==Init EncDec
	WORD EncDecKey = 0;

	for (int n = 0; n < sizeof(gProtect.m_MainInfo.CustomerName); n++)
	{
		EncDecKey += (BYTE)(gProtect.m_MainInfo.CustomerName[n] ^ gProtect.m_MainInfo.ClientSerial[(n % sizeof(gProtect.m_MainInfo.ClientSerial))]);
	}
	////==VietPlus
	//EncDecKey1 = (BYTE)0x01;
	//EncDecKey2 = (BYTE)0x02; //default 76
	//===DO
	EncDecKey1 = BEncDecKey1;
	EncDecKey2 = BEncDecKey2; //default 76

	EncDecKey1 += LOBYTE(EncDecKey);
	EncDecKey2 += HIBYTE(EncDecKey);
#endif
}
bool CProtect::ReadMainFile(char* name) // OK
{
	CCRC32 CRC32;

	if (CRC32.FileCRC(name, reinterpret_cast<unsigned long*>(&this->m_ClientFileCRC), 1024) == 0)
	{
		return 0;
	}

	HANDLE file = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	if (GetFileSize(file, 0) != sizeof(MAIN_FILE_INFO))
	{
		CloseHandle(file);
		return 0;
	}

	DWORD OutSize = 0;

	if (ReadFile(file, &this->m_MainInfo, sizeof(MAIN_FILE_INFO), &OutSize, 0) == 0)
	{
		CloseHandle(file);
		return 0;
	}

	for (int n = 0; n < sizeof(MAIN_FILE_INFO); n++)
	{
		((BYTE*)& this->m_MainInfo)[n] -= (BYTE)(0x95 ^ HIBYTE(n));
		((BYTE*)& this->m_MainInfo)[n] ^= (BYTE)(0xCA ^ LOBYTE(n));
	}

	CloseHandle(file);


	return 1;
}

bool CProtect::ReadTextFile(char* name) // OK
{
	HANDLE file = CreateFile(name,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_ARCHIVE,0);

	if(file == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	if(GetFileSize(file,0) != sizeof(TEXT_FILE_INFO))
	{
		CloseHandle(file);
		MessageBox(0, "ReadTextFile 1", "Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	DWORD OutSize = 0;

	if(ReadFile(file,&this->m_TextInfo,sizeof(TEXT_FILE_INFO),&OutSize,0) == 0)
	{
		CloseHandle(file);
		MessageBox(0, "ReadTextFile 2", "Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	for(int n=0;n < sizeof(TEXT_FILE_INFO);n++)
	{
		((BYTE*)&this->m_TextInfo)[n] -= (BYTE)(0x95^HIBYTE(n));
		((BYTE*)&this->m_TextInfo)[n] ^= (BYTE)(0xCA^LOBYTE(n));
	}

	CloseHandle(file);


	return 1;
}

void CProtect::CheckLauncher() // OK
{
	if ((this->m_MainInfo.LauncherType & 1) == 0)
	{
		return;
	}

	if (FindWindow(0, gProtect.m_MainInfo.LauncherName) == 0)
	{
		//int result = system(gProtect.m_MainInfo.FileLauncherName);
		WinExec(gProtect.m_MainInfo.LauncherFile, 0);
		ExitProcess(0);
	}


}

void CProtect::CheckInstance() // OK
{
	if ((this->m_MainInfo.LauncherType & 2) == 0)
	{
		return;
	}

	HANDLE hMutex = CreateMutex(NULL, FALSE, gProtect.m_MainInfo.LauncherName);
	//
	if (GetLastError() != ERROR_ALREADY_EXISTS)
	{
		WinExec(gProtect.m_MainInfo.LauncherFile, 0);	//ten launcher
		ExitProcess(0);
	}
	return;
}

void CProtect::CheckInstanceLimit() // OK
{
#if defined(__ANDROID__) || defined(MU_IOS)
	/*
		Windows-only by nature. The OS already runs a single instance of an app, so
		there is nothing to cap - and the two APIs this needs are not available:
		MessageBoxA does not exist here, and CreateMutex is stubbed to return
		(HANDLE)1 unconditionally (Platform/PlatformDefs.h), so the slot loop below
		would always claim the first slot and mean nothing.
	*/
	return;
#else
	const DWORD limit = this->m_MainInfo.MaxClientInstance;

	if (limit == 0)
	{
		return;
	}

	/*
		One named mutex per slot; the first slot we can create exclusively is ours,
		and its handle is deliberately never closed - it is released when the
		process dies.

		A counting semaphore looks like the obvious primitive and is the wrong one:
		closing a handle does NOT give back a count that was acquired with a wait,
		so a client that crashed would leak its slot until the machine rebooted.
		A named mutex is destroyed with its last handle, so a crash frees the slot
		by itself.

		No prefix, so these live in the caller's session namespace - the cap is
		per logged-in Windows user, which is what "how many clients can one person
		run" means. A "Global\\" prefix would make it machine-wide across sessions.
	*/
	for (DWORD n = 0; n < limit; n++)
	{
		char szSlotName[128] = {0};
		wsprintf(szSlotName, "MuClientInstance_%s_%u", this->m_MainInfo.CustomerName, n);

		HANDLE hSlot = CreateMutex(NULL, TRUE, szSlotName);

		if (hSlot == NULL)
		{
			// Cannot tell whether this slot is free - do not burn it, try the next.
			continue;
		}

		if (GetLastError() != ERROR_ALREADY_EXISTS)
		{
			// Slot n is ours. Keep hSlot open for the lifetime of the process.
			return;
		}

		CloseHandle(hSlot);
	}

	char szMessage[256] = {0};
	wsprintf(szMessage, "You can only run %u copies of the game at the same time.", limit);

	::MessageBoxA(NULL, szMessage, this->m_MainInfo.WindowName,
		MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND | MB_TOPMOST);

	::ExitProcess(0);
#endif
}

void CProtect::CheckClientFile() // OK
{
	if(this->m_MainInfo.ClientCRC32 == 0)
	{
		return;
	}

	char name[MAX_PATH] = {0};

	if(GetModuleFileName(0,name,sizeof(name)) == 0)
	{
		ExitProcess(0);
	}

	if(_stricmp(ConvertModuleFileName(name),this->m_MainInfo.ClientName) != 0)
	{
		ExitProcess(0);
	}

	CCRC32 CRC32;

	DWORD ClientCRC32;

	if(CRC32.FileCRC(this->m_MainInfo.ClientName,reinterpret_cast<unsigned long*>(&ClientCRC32),1024) == 0)
	{
		ExitProcess(0);
	}

	if(this->m_MainInfo.ClientCRC32 != ClientCRC32)
	{
		ExitProcess(0);
	}
}

void CProtect::CheckPluginFile() // OK
{
	if(this->m_MainInfo.PluginCRC32 == 0)
	{
		return;
	}

	CCRC32 CRC32;

	DWORD PluginCRC32;

	if(CRC32.FileCRC(this->m_MainInfo.PluginName,reinterpret_cast<unsigned long*>(&PluginCRC32),1024) == 0)
	{
		ExitProcess(0);
	}

	if(this->m_MainInfo.PluginCRC32 != PluginCRC32)
	{
		ExitProcess(0);
	}

	HMODULE module = LoadLibrary(this->m_MainInfo.PluginName);

	if(module == 0)
	{
		ExitProcess(0);
	}

	void (*EntryProc)() = (void(*)())GetProcAddress(module,"EntryProc");

	if(EntryProc != 0)
	{
		EntryProc();
	}
}

void CProtect::CheckCameraFile() // OK
{
	if(this->m_MainInfo.CameraCRC32 == 0)
	{
		return;
	}

	CCRC32 CRC32;

	DWORD CameraCRC32;

	if(CRC32.FileCRC(this->m_MainInfo.CameraName,reinterpret_cast<unsigned long*>(&CameraCRC32),1024) == 0)
	{
		ExitProcess(0);
	}

	if(this->m_MainInfo.CameraCRC32 != CameraCRC32)
	{
		ExitProcess(0);
	}

	HMODULE module = LoadLibrary(this->m_MainInfo.CameraName);

	if(module == 0)
	{
		ExitProcess(0);
	}

	void (*EntryProc)() = (void(*)())GetProcAddress(module,"EntryProc");

	if(EntryProc != 0)
	{
		EntryProc();
	}
}
