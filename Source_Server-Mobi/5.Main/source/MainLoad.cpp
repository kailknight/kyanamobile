#include "stdafx.h"
#include "Protect.h"
#include ".\\Utilities\\CCRC32.H"
#include "Util.h"
#include "MainLoad.h"
#include "BuffIcon.h"
#include "TrayMode.h"
#include "APICB.h"
#include "Reconnect.h"
#include "ZzzInfomation.h"

// Linked in from MHPClient.lib (the MHP anti-cheat, built as a static library
// instead of MHPClient.dll). Declared by hand rather than by including its
// headers: it has its own CMHPProtect/MAIN_FILE_INFO that would collide with the
// identically named types in Protect.h.
extern "C" void EntryProc();
extern HINSTANCE hins;
bool LIBRARY_LOAD_ATTACH();
bool MEMORY_CHECK_ATTACH();
extern DWORD gUserAccount;
extern DWORD gUserStruct;
extern DWORD gWindowHwnd;
extern HWND g_hWnd;

MainLoad gMainLoad;

MainLoad::MainLoad()
{
	
}
MainLoad::~MainLoad()
{

}
DWORD StartAddress(void* lpThreadParameter)
{
	HANDLE v1;
	HANDLE v2;

	while (TRUE)
	{
		::Sleep(60000);

		v1 = GetCurrentProcess();
		SetProcessWorkingSetSize(v1, 0xFFFFFFFF, 0xFFFFFFFF);

		v2 = GetCurrentProcess();
		SetThreadPriority(v2, -2);
	}

	return 0;
}

bool MainLoad::Load()
{
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)StartAddress, 0, 0, 0);

	if (gProtect.ReadMainFile(".\\Data\\Local\\CBGetMain.bin") == 0)
	{
		MessageBox(0, "Config corrupt! ReadMainFile", "Error", MB_OK | MB_ICONERROR);
		ExitProcess(0);
		return 0;
	}
	if (gProtect.ReadTextFile(".\\Data\\Local\\CBTextInfo.bin") == 0)
	{
		MessageBox(0, "Config corrupt! ReadTextFile", "Error", MB_OK | MB_ICONERROR);
		ExitProcess(0);
		return 0;
	}
	gProtect.LoadEncDec();

	gProtect.CheckPluginFile();
	gProtect.CheckLauncher();
	gProtect.CheckInstance();

	//=== Set IP Serrial 
	szServerIpAddress = gProtect.m_MainInfo.IpAddress;
	g_ServerPort = gProtect.m_MainInfo.IpAddressPort;
	memcpy(Serial, gProtect.m_MainInfo.ClientSerial, sizeof(Serial));
	Version[0] = (BYTE)gProtect.m_MainInfo.ClientVersion[0] + 1;
	Version[1] = (BYTE)gProtect.m_MainInfo.ClientVersion[2] + 2;
	Version[2] = (BYTE)gProtect.m_MainInfo.ClientVersion[3] + 3;
	Version[3] = (BYTE)gProtect.m_MainInfo.ClientVersion[5] + 4;
	Version[4] = (BYTE)gProtect.m_MainInfo.ClientVersion[6] + 5;

	if (gProtect.m_MainInfo.LoadAntihack)
	{
		// MHP used to ship as MHPClient.dll and get loaded a few lines up by
		// gProtect.CheckPluginFile(), which LoadLibrary'd it and called its
		// EntryProc. It is now linked into this exe instead, so no DLL ships and
		// there is nothing loose for a player to swap out or delete.
		//
		// DllMain does not run for a static library, so the two hooks it used to
		// install on DLL_PROCESS_ATTACH have to be placed here by hand, and hins
		// - which the anti-cheat reads for its splash screens and macro check -
		// has to be pointed at this module.
		//
		// EntryProc calls SafeExitProcess() if it cannot read its own config, so
		// the file is checked first: without this a client that has never had
		// kyana.ah deployed would die during startup with no message at all.
		if (GetFileAttributes(".\\Data\\Custom\\Configs\\kyana.ah") != INVALID_FILE_ATTRIBUTES)
		{
			hins = GetModuleHandle(NULL);

			LIBRARY_LOAD_ATTACH();
			MEMORY_CHECK_ATTACH();

			// Hand the anti-cheat this client's own globals. These are normally
			// absolute addresses the server pushes down as MemoryAddress4 and
			// MemoryAddress5, which only works if it has been told the exact layout
			// of this exe - and every rebuild moves them. Left unset, every
			// detection report went out with an empty account and character name,
			// so nothing could be acted on. Now that the anti-cheat is linked into
			// this binary the compiler can supply the addresses directly.
			//
			// gUserAccount is read as 11 bytes with no dereference, so it wants the
			// buffer itself; ReconnectAccount is char[11] and already holds the
			// account for every login path. gUserStruct is dereferenced once and the
			// name taken at +0 - CharacterMachine is a CHARACTER_MACHINE* whose
			// first member is CHARACTER_ATTRIBUTE, itself starting with Name[11], so
			// the address of the pointer is what goes here.
			//
			// Set before EntryProc so a server that does supply MemoryAddress4/5
			// still overrides these.
			gUserAccount = (DWORD)g_pReconnect->s_Data.ReconnectAccount;
			gUserStruct = (DWORD)&CharacterMachine;

			// Window title carries character/level/map and fills CaptionName on every
			// report. Also re-enables MacroCheck, which silently treats an unset
			// handle as "always foreground" and so never fires.
			gWindowHwnd = (DWORD)&g_hWnd;

			EntryProc();
		}
	}
	
	ApplyProtectData();

#if (CB_ANTIHACKGGNEW)
	gAPICB.Init();
#endif
	SetTargetFps(gProtect.m_MainInfo.FpsLimit);

	return 1;
}

void MainLoad::ApplyProtectData()
{
	gCustomMessage.LoadEng(gProtect.m_MainInfo.EngCustomMessageInfo);
	gCustomMessage.LoadVtm(gProtect.m_MainInfo.VtmCustomMessageInfo);

	gCustomBattleGloves.Load(gProtect.m_MainInfo.CustomGloves);
	gCustomJewel.Load(gProtect.m_MainInfo.CustomJewelInfo);
	gCustomWing.Load(gProtect.m_MainInfo.CustomWingInfo);
	gCustomItem.Load(gProtect.m_MainInfo.CustomItemInfo);
	gCustomItem.LoadRingPen(gProtect.m_MainInfo.CustomRingPenInfo);

	gCloak.Load(gProtect.m_MainInfo.m_CustomCloak);
	gCloak.LoadCEffect(gProtect.m_MainInfo.m_CustomCEffect);

	gCustomWingEffect.Load(gProtect.m_MainInfo.CustomWingEffectInfo);
	gDynamicWingEffect.Load(gProtect.m_MainInfo.DynamicWingEffectInfo);


	gCustomBow.Load(gProtect.m_MainInfo.CustomBowInfo);
	ItemTRSData.Load(gProtect.m_MainInfo.CustomPosition);

	gCustomMonster.Load(gProtect.m_MainInfo.CustomMonsters);
	gCustomMonster.LoadBossClass(gProtect.m_MainInfo.CustomBossClass);
	gNPCName.Load(gProtect.m_MainInfo.CustomNPCName);
	gCustomMonsterGlow.LoadGlow(gProtect.m_MainInfo.m_CustomMonsterGlow);
	gCustomMonsterGlow.LoadBrightness(gProtect.m_MainInfo.m_CustomMonsterbrightness);

	gCustomMap.OpenScritp(gProtect.m_MainInfo.m_MapInfo);
	gCustomPet2.Load(gProtect.m_MainInfo.CustomPetInfo);
	gCustomCEffectPet.Load(gProtect.m_MainInfo.m_PetCEffectBMD);
	gCustomCEffectPet.LoadGlow(gProtect.m_MainInfo.RenderMeshPet);
	gggJCEffectMonster.Load(gProtect.m_MainInfo.m_CustomMonsterEffect);
	//=============

	//==Text FIle
	gIconBuff.LoadEng(gProtect.m_TextInfo.m_TooltipTRSDataEng);
	gIconBuff.LoadVTM(gProtect.m_TextInfo.m_TooltipTRSDataVTM);

	GInfo.loadnInformation(gProtect.m_TextInfo.m_TRSTooltipData);
	GInfo.loadnInformationSet(gProtect.m_TextInfo.m_TRSTooltipSetData);
	GInfo.loadnText(gProtect.m_TextInfo.m_TRSTooltipText);

	gCustomBuyVip.Load(gProtect.m_MainInfo.CustomBuyVipInfo);

	gCustomCommandInfo.Load(gProtect.m_MainInfo.CustomCommandInfo);
	gCustomServerName.Load(gProtect.m_MainInfo.DefaultServerName, gProtect.m_MainInfo.CustomServerName);
	gCustomDmgColor.Load(gProtect.m_MainInfo.CustomDmgColor); //Dmg Color
}