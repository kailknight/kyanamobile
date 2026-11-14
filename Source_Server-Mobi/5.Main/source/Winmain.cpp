///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN

#include <locale.h>
#include <zmouse.h>
#include "UIWindows.h"
#include "UIManager.h"
#include "Reconnect.h"
#include "ZzzOpenglUtil.h"
#include "ZzzTexture.h"
#include "ZzzOpenData.h"
#include "Other.h"
#include "MultiLanguage.h"
#include "ZzzScene.h"
#include "ZzzBMD.h"
#include "ZzzInfomation.h"
#include "ZzzObject.h"
#include "ZzzCharacter.h"
#include "ZzzInterface.h"
#include "ZzzInventory.h"
#include "zzzLodTerrain.h"
#include "DSPlaySound.h"
#include "wsclientinline.h"
#include "Resource.h"
#include <imm.h>
#include "zzzpath.h"
#include "Nprotect.h"
#include "Local.h"
#include "PersonalShopTitleImp.h"
#include "./Utilities/Log/ErrorReport.h"
#include "UIMapName.h"		// rozy
#include "./ExternalObject/leaf/ExceptionHandler.h"
#include "./Utilities/Dump/CrashReporter.h"
#include "./Utilities/Log/muConsoleDebug.h"
#include "ProtocolSend.h"
#include "ProtectSysKey.h"

#include "CBTMessageBox.h"
#include "./ExternalObject/leaf/regkey.h"

#include "CSChaosCastle.h"
#include "GMHellas.h"
#include <io.h>
#include "Input.h"
#include "./Time/Timer.h"
#include "UIMng.h"
//==Load BCustom
#include "Protect.h"
//Nvidia Update
#include "nvapi.h"
#include "NvApiDriverSettings.h"
#ifdef MOVIE_DIRECTSHOW
#include <dshow.h>
#include "MovieScene.h"
#endif // MOVIE_DIRECTSHOW
#include "GameCensorship.h"

#include "w_MapHeaders.h"

#include "w_PetProcess.h"

#include <ThemidaInclude.h>

#include "MultiLanguage.h"

#include "PacketManager.h"
#include "BuffIcon.h"
#if(UseStackLog)
#include "Stack.h"
#endif
#include "MainLoad.h"
#include "TrayMode.h"
#include "CB_MUHelper.h"


CUIMercenaryInputBox * g_pMercenaryInputBox = NULL;
CUITextInputBox * g_pSingleTextInputBox = NULL;
CUITextInputBox * g_pSinglePasswdInputBox = NULL;
int g_iChatInputType = 1;
extern BOOL g_bIMEBlock;

CChatRoomSocketList * g_pChatRoomSocketList = NULL;

CMultiLanguage *pMultiLanguage = NULL;

extern DWORD g_dwTopWindow;

#ifdef MOVIE_DIRECTSHOW
CMovieScene*	g_pMovieScene = NULL;
#endif // MOVIE_DIRECTSHOW

CUIManager* g_pUIManager = NULL;
CUIMapName* g_pUIMapName = NULL;		// rozy

int Time_Effect = 0;
bool ashies = false;
int weather = rand()%3;

HWND      g_hWnd  = NULL;
HINSTANCE g_hInst = NULL;
HDC       g_hDC   = NULL;
HGLRC     g_hRC   = NULL;
HFONT     g_hFont = NULL;
HFONT     g_hFontBold = NULL;
HFONT     g_hFontBoldName = NULL;
HFONT     g_hFontBig = NULL;
HFONT     g_hFixFont = NULL;
HFONT     g_hFontMini= NULL;
CTimer*		g_pTimer = NULL;	// performance counter.
bool      Destroy = false;
bool      ActiveIME = false;

BYTE*				RendomMemoryDump;
ITEM_ATTRIBUTE*		ItemAttRibuteMemoryDump;
CHARACTER*			CharacterMemoryDump;

int       RandomTable[100];

char TextMu[]       = "mu.exe";

CErrorReport g_ErrorReport;

BOOL g_bMinimizedEnabled = FALSE;
int g_iScreenSaverOldValue = 60*15;

extern float g_fScreenRate_x;	// ¡Ø
extern float g_fScreenRate_y;
extern int DisplayWinCDepthBox;
extern int DisplayWin;
extern int DisplayHeight;
extern int DisplayWinMid;
extern int DisplayHeightExt;
extern int DisplayWinExt;
extern int DisplayWinReal;

#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
BOOL g_bUseWindowMode = TRUE;
#endif

char Mp3FileName[256];

#pragma comment(lib, "wzAudio.lib")
#include <wzAudio.h>

// Defined further down this file, next to g_bDirectSoundCreated. Declared here
// because every function below has to check it: wzAudio keeps its state in an
// instance it allocates in wzAudioCreate, and calling into the DLL when that
// call failed - or after wzAudioDestroy has released it - dereferences null
// inside the DLL. Seen as an access violation reading 0x00000004 at
// wzAudio+0x1D9C on a machine whose audio device would not initialise: startup
// creates wzAudio unconditionally now, but nothing checked whether it worked
// before the login screen's first PlayMp3 went in. The answers for "no audio"
// mirror the Android implementations of these same four functions
// (android_main.cpp), which have always reported "nothing is playing".
extern bool g_bWzAudioCreated;

void StopMp3(char *Name, BOOL bEnforce)
{
	if(!g_bWzAudioCreated) return;

    if(!m_MusicOnOff && !bEnforce) return;

	if(Mp3FileName[0] != NULL)
	{
		if(strcmp(Name, Mp3FileName) == 0) {
			wzAudioStop();
			Mp3FileName[0] = NULL;
		}
	}
}

void PlayMp3(char *Name, BOOL bEnforce )
{
	if(!g_bWzAudioCreated) return;

	if(Destroy) return;
    if(!m_MusicOnOff && !bEnforce) return;

	if(strcmp(Name,Mp3FileName) == 0)
	{
		return;
	}
	else
	{
		wzAudioPlay(Name, 1);
		strcpy(Mp3FileName,Name);
	}
}

bool IsEndMp3()
{
	// "Nothing is playing" rather than "still playing": callers use this to stop
	// a track or move an event on, and a permanent false leaves those waiting.
	if(!g_bWzAudioCreated) return true;

	if (100 == wzAudioGetStreamOffsetRange())
		return true;
	return false;
}

int GetMp3PlayPosition()
{
	// 100 is the value IsEndMp3 reads as finished.
	if(!g_bWzAudioCreated) return 100;

	return wzAudioGetStreamOffsetRange();
}

extern int  LogIn;
extern char LogInID[];

void CheckHack( void)
{
	#ifdef NEW_PROTOCOL_SYSTEM
		gProtocolSend.SendCheckOnline();
	#else
		SendCheck();
	#endif
}

GLvoid KillGLWindow(GLvoid)								
{
	//if (g_hRC)
	//{
	//	if (!wglMakeCurrent(NULL,NULL))
	//	{
	//		g_ErrorReport.Write( "GL - Release Of DC And RC Failed\r\n");
	//		MessageBox(NULL,"Release Of DC And RC Failed.","Error",MB_OK | MB_ICONINFORMATION);
	//	}

	//	if (!wglDeleteContext(g_hRC))
	//	{
	//		g_ErrorReport.Write( "GL - Release Rendering Context Failed\r\n");
	//		MessageBox(NULL,"Release Rendering Context Failed.","Error",MB_OK | MB_ICONINFORMATION);
	//	}

	//	g_hRC=NULL;
	//}
	if (g_hRC)
	{
		wglMakeCurrent(nullptr, nullptr);
		if (!wglDeleteContext(g_hRC))
		{
			g_ErrorReport.Write("GL - Release Rendering Context Failed\r\n");
			MessageBox(NULL, "Release Rendering Context Failed.", "Error", MB_OK | MB_ICONINFORMATION);
		}

		g_hRC = NULL;
	}
	if (g_hDC && !ReleaseDC(g_hWnd,g_hDC))
	{
		g_ErrorReport.Write( "GL - OpenGL Release Error\r\n");
		MessageBox(NULL,"OpenGL Release Error.","Error",MB_OK | MB_ICONINFORMATION);
		g_hDC=NULL;
	}

#if (defined WINDOWMODE)
	if (g_bUseWindowMode == FALSE)
	{
		ChangeDisplaySettings(NULL,0);
		ShowCursor(TRUE);
	}
#else
#ifdef ENABLE_FULLSCREEN
#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
	if (g_bUseWindowMode == FALSE)
#endif	// USER_WINDOW_MODE
	{
		ChangeDisplaySettings(NULL,0);
		ShowCursor(TRUE);
	}
#endif //ENABLE_FULLSCREEN
#endif	//WINDOWMODE(#else)
}


BOOL GetFileNameOfFilePath( char *lpszFile, char *lpszPath)
{
	int iFind = ( int)'\\';
	char *lpFound = lpszPath;
	char *lpOld = lpFound;
	while ( lpFound)
	{
		lpOld = lpFound;
		lpFound = strchr( lpFound + 1, iFind);
	}

	if ( strchr( lpszPath, iFind))
	{
		strncpy( lpszFile, lpOld + 1, MAX_PATH - 1);
	}
	else
	{
		strncpy( lpszFile, lpOld, MAX_PATH - 1);
	}
	lpszFile[MAX_PATH - 1] = '\0';

	BOOL bCheck = TRUE;
	for ( char *lpTemp = lpszFile; bCheck; ++lpTemp)
	{
		switch ( *lpTemp)
		{
		case '\"':
		case '\\':
		case '/':
		case ' ':
			*lpTemp = '\0';
		case '\0':
			bCheck = FALSE;
			break;
		}
	}

	return ( TRUE);
}

HANDLE g_hMainExe = INVALID_HANDLE_VALUE;

BOOL OpenMainExe( void)
{
#ifdef _DEBUG
	return ( TRUE);
#endif
	char lpszFile[MAX_PATH];
	char *lpszCommandLine = GetCommandLine();
	GetFileNameOfFilePath( lpszFile, lpszCommandLine);

	g_hMainExe = CreateFile( ( char*)lpszFile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL, 0);
	
	return ( INVALID_HANDLE_VALUE != g_hMainExe);
}

void CloseMainExe( void)
{
	CloseHandle( g_hMainExe);
}

WORD DecryptCheckSumKey( WORD wSource)
{
	WORD wAcc = wSource ^ 0xB479;
	return ( ( wAcc >> 10) << 4) | ( wAcc & 0xF);
}

DWORD GenerateCheckSum( BYTE *pbyBuffer, DWORD dwSize, WORD wKey)
{
	DWORD dwKey = ( DWORD)wKey;
	DWORD dwResult = dwKey << 9;
	for ( DWORD dwChecked = 0; dwChecked <= dwSize - 4; dwChecked += 4)
	{
		DWORD dwTemp;
		memcpy( &dwTemp, pbyBuffer + dwChecked, sizeof ( DWORD));

		switch ( ( dwChecked / 4 + wKey) % 3)
		{
		case 0:
			dwResult ^= dwTemp;
			break;
		case 1:
			dwResult += dwTemp;
			break;
		case 2:
			dwResult <<= ( dwTemp % 11);
			dwResult ^= dwTemp;
			break;
		}

		if ( 0 == ( dwChecked % 4))
		{
			dwResult ^= ( ( dwKey + dwResult) >> ( ( dwChecked / 4) % 16 + 3));
		}
	}

	return ( dwResult);
}

DWORD GetCheckSum( WORD wKey)
{
	wKey = DecryptCheckSumKey( wKey);

	char lpszFile[MAX_PATH];

	strcpy( lpszFile, "data\\local\\Gameguard.csr");

	HANDLE hFile = CreateFile( ( char*)lpszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if ( INVALID_HANDLE_VALUE == hFile)
	{
		return ( 0);
	}
	
	DWORD dwSize = GetFileSize( hFile, NULL);
	BYTE *pbyBuffer = new BYTE [dwSize];
	DWORD dwNumber;
	ReadFile( hFile, pbyBuffer, dwSize, &dwNumber, 0);
	CloseHandle( hFile);
	
	DWORD dwCheckSum = GenerateCheckSum(pbyBuffer, dwSize, wKey);
	delete [] pbyBuffer;
	
	return (dwCheckSum);
}


BOOL GetFileVersion( char *lpszFileName, WORD *pwVersion)
{
	DWORD dwHandle;
	DWORD dwLen = GetFileVersionInfoSize( lpszFileName, &dwHandle);
	if ( dwLen <= 0)
	{
		return ( FALSE);
	}

	BYTE *pbyData = new BYTE [dwLen];
	if ( !GetFileVersionInfo( lpszFileName, dwHandle, dwLen, pbyData))
	{
		delete [] pbyData;
		return ( FALSE);
	}

	VS_FIXEDFILEINFO *pffi;
	UINT uLen;
	if ( !VerQueryValue( pbyData, "\\", ( LPVOID*)&pffi, &uLen))
	{
		delete [] pbyData;
		return ( FALSE);
	}

	pwVersion[0] = HIWORD( pffi->dwFileVersionMS);
	pwVersion[1] = LOWORD( pffi->dwFileVersionMS);
	pwVersion[2] = HIWORD( pffi->dwFileVersionLS);
	pwVersion[3] = LOWORD( pffi->dwFileVersionLS);

	delete [] pbyData;
	return ( TRUE);
}

extern PATH     *path;

void DestroyWindow()
{
	LogOut = true;
	//. save volume level
	leaf::CRegKey regkey;
	regkey.SetKey(leaf::CRegKey::_HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu\\Config");
	regkey.WriteDword("VolumeLevel", g_pOption->GetVolumeLevel());
	char string[10];
	WritePrivateProfileStringA("Custom", "ShowName", itoa(mShowName, string, 10) , "./config.ini");
	WritePrivateProfileStringA("Custom", "ShowHPBar", itoa(mShowHPBar, string, 10), "./config.ini");
	WritePrivateProfileStringA("Custom", "ShowMiniMap", itoa(mShowMiniMap, string, 10), "./config.ini");
	WritePrivateProfileStringA("Custom", "ShowDanhHieu", itoa(mShowDanhHieu, string, 10), "./config.ini");
	WritePrivateProfileStringA("Custom", "HideMarketNotice", itoa(mHideMarketNotice, string, 10), "./config.ini");
	// Music/Sound/Resolution live in the registry instead (see OpenInitFile),
	// and are written the moment they change rather than only on a clean
	// exit - SaveConfigDword(), called from the Config System window.
	// Unlike the four writes just above, CNewUINameWindow isn't built until
	// LoadMainSceneInterface() (main scene load, well after this early
	// bootstrap) - null here means the client exited before ever loading a
	// character (e.g. closed at server select), so there's nothing to save.
	if(g_pNewUISystem->GetUI_NewNameWindow() != NULL)
	{
		WritePrivateProfileStringA("Custom", "ShowItemNames", itoa(g_pNewUISystem->GetUI_NewNameWindow()->IsShowItemName(), string, 10), "./config.ini");
	}
	//===
	WritePrivateProfileStringA("Graphics", "GlowEffect", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eGlowEffect], string, 10), "./config.ini");
	WritePrivateProfileStringA("Graphics", "EffectDynamic", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eEffectDynamic], string, 10), "./config.ini");
	WritePrivateProfileStringA("Graphics", "EffectStatic", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eEffectStatic], string, 10), "./config.ini");
	WritePrivateProfileStringA("Graphics", "BMDPlayer", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eBMDPlayer], string, 10), "./config.ini");
	WritePrivateProfileStringA("Graphics", "BMDWings", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eBMDWings], string, 10), "./config.ini");
	WritePrivateProfileStringA("Graphics", "BMDWeapons", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eBMDWeapons], string, 10), "./config.ini");
	WritePrivateProfileStringA("Graphics", "BMDImg", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eBMDImg], string, 10), "./config.ini");
	WritePrivateProfileStringA("Graphics", "BMDMonter", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eBMDMonter], string, 10), "./config.ini");
	WritePrivateProfileStringA("Graphics", "RenderObjects", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eRenderObjects], string, 10), "./config.ini");
	WritePrivateProfileStringA("Graphics", "RenderTerrain", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eRenderTerrain], string, 10), "./config.ini");
	WritePrivateProfileStringA("Graphics", "ExcellentEffect", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eExcellentEffect], string, 10), "./config.ini");
	WritePrivateProfileStringA("Graphics", "BMDZen", itoa(g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eBMDZen], string, 10), "./config.ini");


	CUIMng::Instance().Release();

#ifdef MOVIE_DIRECTSHOW
	if(g_pMovieScene)
	{
		g_pMovieScene->Destroy();
	}
#endif // MOVIE_DIRECTSHOW

	//. release font handle
	if(g_hFont)::DeleteObject((HGDIOBJ)g_hFont);

	if(g_hFontBold)::DeleteObject((HGDIOBJ)g_hFontBold);

	if(g_hFontBig)::DeleteObject((HGDIOBJ)g_hFontBig);

	if(g_hFixFont)::DeleteObject((HGDIOBJ)g_hFixFont);

	if (g_hFontMini)::DeleteObject((HGDIOBJ)g_hFixFont);

	if (g_hFontBoldName)::DeleteObject((HGDIOBJ)g_hFontBoldName);
	
	ReleaseCharacters();

    if ( path!=NULL )
    {
	    delete path;
    }
	SAFE_DELETE(GateAttribute);

	//for ( int i = 0; i < MAX_SKILLS; ++i)
	//{
	//}
	SAFE_DELETE(SkillAttribute);

	SAFE_DELETE(CharacterMachine);

    DeleteWaterTerrain ();

#ifdef MOVIE_DIRECTSHOW
	if(SceneFlag != MOVIE_SCENE)
#endif // MOVIE_DIRECTSHOW
	{
		gMapManager.DeleteObjects();

		// Object.
		for(int i=MODEL_LOGO;i<MAX_MODELS;i++)
		{
			Models[i].Release();
		}

		// Bitmap
		Bitmaps.UnloadAllImages();
	}

	SAFE_DELETE_ARRAY( CharacterMemoryDump );
	SAFE_DELETE_ARRAY( ItemAttRibuteMemoryDump );
	SAFE_DELETE_ARRAY( RendomMemoryDump );
	SAFE_DELETE_ARRAY( ModelsDump );
	
#ifdef DYNAMIC_FRUSTRUM
	DeleteAllFrustrum();
#endif //DYNAMIC_FRUSTRUM

	SAFE_DELETE(g_pMercenaryInputBox);
	SAFE_DELETE(g_pSingleTextInputBox);
	SAFE_DELETE(g_pSinglePasswdInputBox);

	SAFE_DELETE(g_pChatRoomSocketList);
	SAFE_DELETE(g_pUIMapName);	// rozy
	SAFE_DELETE( g_pTimer );
	SAFE_DELETE(g_pUIManager);
	 
#ifdef MOVIE_DIRECTSHOW
	SAFE_DELETE(g_pMovieScene);
#endif // MOVIE_DIRECTSHOW

	SAFE_DELETE(pMultiLanguage);
	BoostRest( g_BuffSystem );
	BoostRest( g_MapProcess );
	BoostRest( g_petProcess );

	g_ErrorReport.Write( "Destroy" );
	 
	HWND shWnd = FindWindow(NULL, "MuPlayer");
	if(shWnd)
		SendMessage(shWnd, WM_DESTROY, 0, 0);
}
void DestroySound()
{
	for(int i=0;i<MAX_BUFFER;i++)
		ReleaseBuffer(i);

	FreeDirectSound();

	// Clearing the flag is the point. This runs from WM_DESTROY, and the process
	// does not end until several calls later (DestroyWindow, KillGLWindow,
	// CloseMainExe, ExitProcess) - every other thread is still running through
	// all of it. One of them calling PlayMp3/StopMp3 in that window goes into the
	// DLL with the instance already released, which is the same null dereference
	// a failed wzAudioCreate produces, and it shows up as a crash on a thread
	// that has nothing to do with shutdown.
	if(g_bWzAudioCreated)
	{
		wzAudioDestroy();
		g_bWzAudioCreated = false;
	}
}

int g_iInactiveTime = 0;
int g_iNoMouseTime = 0;
int g_iInactiveWarning = 0;
bool g_bWndActive = false;
bool HangulDelete = false;
int Hangul = 0;
bool g_bEnterPressed = false;

int g_iMousePopPosition_x = 0;
int g_iMousePopPosition_y = 0;

extern int TimeRemain;
extern bool EnableFastInput;
void MainScene(HDC hDC);

//////////////////////////////////////////////////////////////////////
// Screen size + real fullscreen (PC)
//
// Resolution (registry SOFTWARE\Webzen\Mu\Config, value "Resolution"):
//   0-10  the original table (unchanged, so saved settings keep working)
//   11 1280x720  12 1600x900  13 1920x1080  14 2560x1440  15 3840x2160  16 1024x768  17 1280x1024  18 1600x1200
//   99    the monitor's own (desktop) resolution
// WindowMode = 0: real fullscreen - the monitor is switched to that resolution (no stretching, no black bars);
// Alt+Tab gives the desktop back and the mode is switched again when the game gets the focus back.
// WindowMode = 1: a fixed window of that size (made smaller when it does not fit on the screen).
//////////////////////////////////////////////////////////////////////

extern int m_Resolution;

static DEVMODE g_FsMode;
static bool g_FsHave = false;

// The UI is laid out on a virtual screen at least 640x480: the scale keeps both sides inside it.
static float ScreenScaleFor(int w,int h)
{
	float sx = (float)w / 640.0f;
	float sy = (float)h / 480.0f;
	return (sx < sy) ? sx : sy;
}

void ApplyScreenSize(int w,int h,float rate)
{
	WindowWidth = w;
	WindowHeight = h;

	g_fScreenRate_x = rate;
	g_fScreenRate_y = rate;

	DisplayWinCDepthBox = (int)WindowWidth / g_fScreenRate_y - 640;
	DisplayWin = (int)WindowWidth / g_fScreenRate_y;
	DisplayWinMid = (int)WindowWidth / g_fScreenRate_y / 2;
	DisplayWinExt = (int)((DisplayWin * 0.5) - 320.0);
	DisplayWinReal = (int)DisplayWin / 640.0;

	DisplayHeight = (int)((double)WindowHeight / g_fScreenRate_y);
	DisplayHeightExt = (int)((double)DisplayHeight - 480.0);
}

// Picks the display mode for w x h: 32 bit first, the desktop's refresh rate when the mode has it, else the highest.
static bool FullscreenFindMode(int w,int h)
{
	DEVMODE cur;
	memset(&cur,0,sizeof(cur));
	cur.dmSize = sizeof(cur);
	EnumDisplaySettings(NULL,ENUM_CURRENT_SETTINGS,&cur);

	DEVMODE dm;
	DEVMODE best;
	bool found = false;

	memset(&dm,0,sizeof(dm));
	dm.dmSize = sizeof(dm);

	for(int n=0;EnumDisplaySettings(NULL,n,&dm);n++)
	{
		if((int)dm.dmPelsWidth != w || (int)dm.dmPelsHeight != h || dm.dmBitsPerPel < 16)
		{
			continue;
		}

		if(found == false)
		{
			best = dm;
			found = true;
			continue;
		}

		bool better = false;

		if(dm.dmBitsPerPel != best.dmBitsPerPel)
		{
			better = (dm.dmBitsPerPel > best.dmBitsPerPel);
		}
		else if(dm.dmDisplayFrequency == cur.dmDisplayFrequency)
		{
			better = (best.dmDisplayFrequency != cur.dmDisplayFrequency);
		}
		else if(best.dmDisplayFrequency != cur.dmDisplayFrequency)
		{
			better = (dm.dmDisplayFrequency > best.dmDisplayFrequency);
		}

		if(better)
		{
			best = dm;
		}
	}

	if(found)
	{
		best.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
		g_FsMode = best;
		g_FsHave = true;
	}

	return found;
}

static bool FullscreenApply()
{
	if(g_FsHave == false)
	{
		return false;
	}

	return (ChangeDisplaySettings(&g_FsMode,CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL);
}

//////////////////////////////////////////////////////////////////////
// Live display change (PC)
//////////////////////////////////////////////////////////////////////

#define KOK_GL_FRAMEBUFFER 0x8D40
#define KOK_GL_RENDERBUFFER 0x8D41
#define KOK_GL_COLOR_ATTACHMENT0 0x8CE0
#define KOK_GL_DEPTH_ATTACHMENT 0x8D00
#define KOK_GL_STENCIL_ATTACHMENT 0x8D20
#define KOK_GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define KOK_GL_DEPTH24_STENCIL8 0x88F0
#define KOK_GL_RGBA8 0x8058
#define KOK_GL_CLAMP_TO_EDGE 0x812F

typedef void (APIENTRY *PFN_GenFramebuffers)(GLsizei, GLuint*);
typedef void (APIENTRY *PFN_BindFramebuffer)(GLenum, GLuint);
typedef void (APIENTRY *PFN_FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef void (APIENTRY *PFN_GenRenderbuffers)(GLsizei, GLuint*);
typedef void (APIENTRY *PFN_BindRenderbuffer)(GLenum, GLuint);
typedef void (APIENTRY *PFN_RenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
typedef void (APIENTRY *PFN_FramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
typedef GLenum (APIENTRY *PFN_CheckFramebufferStatus)(GLenum);

static PFN_GenFramebuffers s_glGenFramebuffers = NULL;
static PFN_BindFramebuffer s_glBindFramebuffer = NULL;
static PFN_FramebufferTexture2D s_glFramebufferTexture2D = NULL;
static PFN_GenRenderbuffers s_glGenRenderbuffers = NULL;
static PFN_BindRenderbuffer s_glBindRenderbuffer = NULL;
static PFN_RenderbufferStorage s_glRenderbufferStorage = NULL;
static PFN_FramebufferRenderbuffer s_glFramebufferRenderbuffer = NULL;
static PFN_CheckFramebufferStatus s_glCheckFramebufferStatus = NULL;

static bool s_PresentScaled = false;   // the engine renders offscreen, the frame is scaled onto the window
static bool s_PresentCheck = false;    // the window changed: decide again after the next frame
static GLuint s_PresentFbo = 0;
static GLuint s_PresentTex = 0;
static GLuint s_PresentDepth = 0;
static int s_PhysW = 0, s_PhysH = 0;   // the window's client size
static int s_DstX = 0, s_DstY = 0, s_DstW = 0, s_DstH = 0;   // where the frame lands in it (letterboxed)

static bool PresentLoadGl()
{
	if (s_glBindFramebuffer != NULL)
	{
		return true;
	}

	s_glGenFramebuffers = (PFN_GenFramebuffers)wglGetProcAddress("glGenFramebuffersEXT");
	s_glBindFramebuffer = (PFN_BindFramebuffer)wglGetProcAddress("glBindFramebufferEXT");
	s_glFramebufferTexture2D = (PFN_FramebufferTexture2D)wglGetProcAddress("glFramebufferTexture2DEXT");
	s_glGenRenderbuffers = (PFN_GenRenderbuffers)wglGetProcAddress("glGenRenderbuffersEXT");
	s_glBindRenderbuffer = (PFN_BindRenderbuffer)wglGetProcAddress("glBindRenderbufferEXT");
	s_glRenderbufferStorage = (PFN_RenderbufferStorage)wglGetProcAddress("glRenderbufferStorageEXT");
	s_glFramebufferRenderbuffer = (PFN_FramebufferRenderbuffer)wglGetProcAddress("glFramebufferRenderbufferEXT");
	s_glCheckFramebufferStatus = (PFN_CheckFramebufferStatus)wglGetProcAddress("glCheckFramebufferStatusEXT");

	if (s_glGenFramebuffers == NULL || s_glBindFramebuffer == NULL || s_glFramebufferTexture2D == NULL || s_glGenRenderbuffers == NULL
		|| s_glBindRenderbuffer == NULL || s_glRenderbufferStorage == NULL || s_glFramebufferRenderbuffer == NULL || s_glCheckFramebufferStatus == NULL)
	{
		s_glBindFramebuffer = NULL;
		g_ErrorReport.Write("> Live display change: this graphics driver has no framebuffer objects.\r\n");
		return false;
	}

	return true;
}

static bool PresentCreateTarget()
{
	if (s_PresentFbo != 0)
	{
		return true;
	}

	if (PresentLoadGl() == false)
	{
		return false;
	}

	extern int CachTexture;

	glGenTextures(1, &s_PresentTex);
	glBindTexture(GL_TEXTURE_2D, s_PresentTex);
	glTexImage2D(GL_TEXTURE_2D, 0, KOK_GL_RGBA8, (int)WindowWidth, (int)WindowHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, KOK_GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, KOK_GL_CLAMP_TO_EDGE);
	CachTexture = -1;

	s_glGenRenderbuffers(1, &s_PresentDepth);
	s_glBindRenderbuffer(KOK_GL_RENDERBUFFER, s_PresentDepth);
	s_glRenderbufferStorage(KOK_GL_RENDERBUFFER, KOK_GL_DEPTH24_STENCIL8, (int)WindowWidth, (int)WindowHeight);

	s_glGenFramebuffers(1, &s_PresentFbo);
	s_glBindFramebuffer(KOK_GL_FRAMEBUFFER, s_PresentFbo);
	s_glFramebufferTexture2D(KOK_GL_FRAMEBUFFER, KOK_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_PresentTex, 0);
	s_glFramebufferRenderbuffer(KOK_GL_FRAMEBUFFER, KOK_GL_DEPTH_ATTACHMENT, KOK_GL_RENDERBUFFER, s_PresentDepth);
	s_glFramebufferRenderbuffer(KOK_GL_FRAMEBUFFER, KOK_GL_STENCIL_ATTACHMENT, KOK_GL_RENDERBUFFER, s_PresentDepth);

	const GLenum status = s_glCheckFramebufferStatus(KOK_GL_FRAMEBUFFER);
	s_glBindFramebuffer(KOK_GL_FRAMEBUFFER, 0);

	if (status != KOK_GL_FRAMEBUFFER_COMPLETE)
	{
		g_ErrorReport.Write("> Live display change: offscreen frame not complete (0x%04X).\r\n", (unsigned)status);
		return false;
	}

	g_ErrorReport.Write("> Live display change: offscreen frame %dx%d ready.\r\n", (int)WindowWidth, (int)WindowHeight);
	return true;
}

// After the window changed: scale the frame when the window's client area is not the engine's size.
static void PresentDecide()
{
	RECT rc;

	if (g_hWnd == NULL || GetClientRect(g_hWnd, &rc) == FALSE || rc.right <= 0 || rc.bottom <= 0)
	{
		return;
	}

	s_PhysW = rc.right;
	s_PhysH = rc.bottom;

	// fit the engine frame inside the client area, keeping its shape (black bars where the shapes differ)
	const float sx = (float)s_PhysW / (float)WindowWidth;
	const float sy = (float)s_PhysH / (float)WindowHeight;
	const float s = (sx < sy) ? sx : sy;
	s_DstW = (int)((float)WindowWidth * s + 0.5f);
	s_DstH = (int)((float)WindowHeight * s + 0.5f);
	s_DstX = (s_PhysW - s_DstW) / 2;
	s_DstY = (s_PhysH - s_DstH) / 2;

	const bool want = (s_PhysW != (int)WindowWidth || s_PhysH != (int)WindowHeight);

	if (want && PresentCreateTarget())
	{
		s_PresentScaled = true;
		s_glBindFramebuffer(KOK_GL_FRAMEBUFFER, s_PresentFbo);
		glViewport(0, 0, WindowWidth, WindowHeight);
	}
	else
	{
		if (s_PresentScaled && s_glBindFramebuffer != NULL)
		{
			s_glBindFramebuffer(KOK_GL_FRAMEBUFFER, 0);
		}

		s_PresentScaled = false;
		glViewport(0, 0, WindowWidth, WindowHeight);
	}
}

void PcPresentSwap(HDC hDC)
{
	if (s_PresentScaled)
	{
		extern int CachTexture;

		s_glBindFramebuffer(KOK_GL_FRAMEBUFFER, 0);

		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0, s_PhysW, 0, s_PhysH, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glViewport(0, 0, s_PhysW, s_PhysH);
		glDisable(GL_SCISSOR_TEST);
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_FOG);
		glDisable(GL_LIGHTING);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, s_PresentTex);
		glColor4f(1.f, 1.f, 1.f, 1.f);

		const float x0 = (float)s_DstX, x1 = (float)(s_DstX + s_DstW);
		const float y0 = (float)(s_PhysH - s_DstY - s_DstH), y1 = (float)(s_PhysH - s_DstY);
		glBegin(GL_QUADS);
		glTexCoord2f(0.f, 0.f); glVertex2f(x0, y0);
		glTexCoord2f(1.f, 0.f); glVertex2f(x1, y0);
		glTexCoord2f(1.f, 1.f); glVertex2f(x1, y1);
		glTexCoord2f(0.f, 1.f); glVertex2f(x0, y1);
		glEnd();

		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glPopAttrib();
		CachTexture = -1;

		::SwapBuffers(hDC);

		// back to the offscreen frame for the next one
		s_glBindFramebuffer(KOK_GL_FRAMEBUFFER, s_PresentFbo);
		glViewport(0, 0, WindowWidth, WindowHeight);
	}
	else
	{
		::SwapBuffers(hDC);
	}

	if (s_PresentCheck)
	{
		s_PresentCheck = false;
		PresentDecide();
	}
}

void PcPresentMapMouse(float* x, float* y)
{
	if (s_PresentScaled && s_DstW > 0 && s_DstH > 0)
	{
		*x = (*x - (float)s_DstX) * (float)WindowWidth / (float)s_DstW;
		*y = (*y - (float)s_DstY) * (float)WindowHeight / (float)s_DstH;
		return;
	}

	RECT rcClient;

	if (g_hWnd != NULL && GetClientRect(g_hWnd, &rcClient) && rcClient.right > 0 && rcClient.bottom > 0)
	{
		*x = *x * (float)WindowWidth / (float)rcClient.right;
		*y = *y * (float)WindowHeight / (float)rcClient.bottom;
	}
}

bool GetResolutionSize(int resolution, int* w, int* h)
{
	static const int table[][3] = {
		{0,640,480}, {1,800,600}, {2,1024,640}, {3,1280,800}, {4,1366,768}, {5,1440,900}, {6,1536,960}, {7,1680,1050}, {8,1920,1200},
		{9,2560,1600}, {10,3840,2400}, {11,1280,720}, {12,1600,900}, {13,1920,1080}, {14,2560,1440}, {15,3840,2160}, {16,1024,768},
		{17,1280,1024}, {18,1600,1200} };

	if (resolution == 99)
	{
		*w = GetSystemMetrics(SM_CXSCREEN);
		*h = GetSystemMetrics(SM_CYSCREEN);
		return true;
	}

	for (int n = 0; n < (int)(sizeof(table) / sizeof(table[0])); n++)
	{
		if (table[n][0] == resolution)
		{
			*w = table[n][1];
			*h = table[n][2];
			return true;
		}
	}

	return false;
}

bool ApplyDisplaySettings(int resolution, int windowMode)
{
	int w = 0, h = 0;

	if (g_hWnd == NULL || GetResolutionSize(resolution, &w, &h) == false)
	{
		return false;
	}

	if (windowMode == 0)
	{
		bool ok = FullscreenFindMode(w, h) && FullscreenApply();

		if (ok == false)
		{
			w = GetSystemMetrics(SM_CXSCREEN);
			h = GetSystemMetrics(SM_CYSCREEN);
			ok = FullscreenFindMode(w, h) && FullscreenApply();
		}

		if (ok == false)
		{
			g_ErrorReport.Write("> Live display change: fullscreen %dx%d failed.\r\n", w, h);
			return false;
		}

		w = (int)g_FsMode.dmPelsWidth;
		h = (int)g_FsMode.dmPelsHeight;

		SetWindowLong(g_hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowLong(g_hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_APPWINDOW);
		SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, w, h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	}
	else
	{
		// Window mode first, then give the desktop its resolution back: the mode change sends activation messages,
		// and the fullscreen Alt+Tab handler must not take them as "back to fullscreen" (it re-applied the mode and
		// the borderless top-left popup).
		const bool wasFullscreen = (g_bUseWindowMode == FALSE);

		g_bUseWindowMode = TRUE;
		g_FsHave = false;

		if (wasFullscreen)
		{
			ChangeDisplaySettings(NULL, 0);
		}

		ShowWindow(g_hWnd, SW_RESTORE);

		const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_CLIPCHILDREN;

		// a window must fit on the screen
		RECT wa;
		SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
		RECT frame = { 0, 0, 100, 100 };
		AdjustWindowRect(&frame, style, FALSE);
		const int extraW = (frame.right - frame.left) - 100;
		const int extraH = (frame.bottom - frame.top) - 100;
		const int maxW = (wa.right - wa.left) - extraW;
		const int maxH = (wa.bottom - wa.top) - extraH;

		if (w > maxW || h > maxH)
		{
			const float s1 = (float)maxW / (float)w;
			const float s2 = (float)maxH / (float)h;
			const float s = (s1 < s2) ? s1 : s2;
			w = (int)(w * s);
			h = (int)(h * s);
		}

		RECT rc = { 0, 0, w, h };
		AdjustWindowRect(&rc, style, FALSE);
		const int ww = rc.right - rc.left;
		const int wh = rc.bottom - rc.top;

		SetWindowLong(g_hWnd, GWL_STYLE, style | WS_VISIBLE);
		SetWindowLong(g_hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW);
		SetWindowPos(g_hWnd, HWND_NOTOPMOST, wa.left + ((wa.right - wa.left) - ww) / 2, wa.top + ((wa.bottom - wa.top) - wh) / 2, ww, wh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

		RECT check;
		GetWindowRect(g_hWnd, &check);
		g_ErrorReport.Write("> Live display change: window style %08X at %d,%d size %dx%d.\r\n", (unsigned)GetWindowLong(g_hWnd, GWL_STYLE), (int)check.left, (int)check.top, (int)(check.right - check.left), (int)(check.bottom - check.top));
	}

	g_bUseWindowMode = (windowMode != 0) ? TRUE : FALSE;
	m_Resolution = resolution;

	SaveConfigDword("Resolution", resolution);
	SaveConfigDword("WindowMode", (windowMode != 0) ? 1 : 0);

	s_PresentCheck = true;   // switch to / from the scaled frame after this frame

	g_ErrorReport.Write("> Live display change: %s %dx%d (the game renders at %dx%d until the next start).\r\n", (windowMode == 0) ? "fullscreen" : "window", w, h, (int)WindowWidth, (int)WindowHeight);
	return true;
}

LONG FAR PASCAL WndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	MOUSEHOOKSTRUCTEX* Mouse = (MOUSEHOOKSTRUCTEX*)lParam;
    switch (msg)
    {
	case WM_SYSKEYDOWN:
		{
			return 0;
		}
		break;
#if defined PROTECT_SYSTEMKEY && defined NDEBUG
#ifndef FOR_WORK
	case WM_SYSCOMMAND:
		{
			if(wParam == SC_KEYMENU || wParam == SC_SCREENSAVE)
			{
				return 0;
			}
		}
		break;
#endif // !FOR_WORK
#endif // PROTECT_SYSTEMKEY && NDEBUG
#ifdef ENABLE_FULLSCREEN
	case WM_ACTIVATEAPP:
		// Real fullscreen: leaving the game gives the desktop its own resolution back and minimizes the game; coming
		// back switches the monitor again. (g_bMinimizedEnabled: this minimize is ours, not a cheat.)
		if (g_bUseWindowMode == FALSE && g_FsHave)
		{
			if (wParam == FALSE)
			{
				ChangeDisplaySettings(NULL,0);
				g_bMinimizedEnabled = TRUE;
				ShowWindow(hwnd,SW_MINIMIZE);
				g_bMinimizedEnabled = FALSE;
			}
			else
			{
				FullscreenApply();
				ShowWindow(hwnd,SW_RESTORE);
				SetWindowPos(hwnd,HWND_TOPMOST,0,0,(int)g_FsMode.dmPelsWidth,(int)g_FsMode.dmPelsHeight,SWP_SHOWWINDOW);
			}
		}
		break;
#endif
    case WM_ACTIVATE:
		if(LOWORD(wParam) == WA_INACTIVE)
		{
#ifdef ACTIVE_FOCUS_OUT
			if (g_bUseWindowMode == FALSE)
#endif	// ACTIVE_FOCUS_OUT
				g_bWndActive = false;
#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
			if (g_bUseWindowMode == TRUE)
			{
				MouseLButton = false;
				MouseLButtonPop = false;
				//MouseLButtonPush = false;
				MouseRButton = false;
				MouseRButtonPop = false;
				MouseRButtonPush = false;
				MouseLButtonDBClick = false;
				MouseMButton = false;
				MouseMButtonPop = false;
				MouseMButtonPush = false;
				MouseWheel = 0;
			}
#endif
		}
		else
		{
			g_bWndActive = true;
		}
		break;
	case WM_TIMER :
		//MessageBox(NULL,GlobalText[16],"Error",MB_OK);
		switch( wParam )
		{
		case HACK_TIMER:
			// PKD_ADD_BINARY_PROTECTION
			VM_START
			CheckHack();
			VM_END
			break;
		case WINDOWMINIMIZED_TIMER:
			PostMessage(g_hWnd, WM_CLOSE, 0, 0);
			break;
		case CHATCONNECT_TIMER:
			g_pFriendMenu->SendChatRoomConnectCheck();
			break;
		case SLIDEHELP_TIMER:
			if(g_bWndActive)
			{
				if(g_pSlideHelpMgr)
					g_pSlideHelpMgr->CreateSlideText();
			}
			break;
		}
		break;
	case WM_USER_MEMORYHACK:
		//SetTimer( g_hWnd, WINDOWMINIMIZED_TIMER, 1*1000, NULL);
		KillGLWindow();
		break;
	case WM_NPROTECT_EXIT_TWO:
		SendHackingChecked( 0x04, 0);
		SetTimer( g_hWnd, WINDOWMINIMIZED_TIMER, 1*1000, NULL);
		MessageBox(NULL,GlobalText[16],"Error",MB_OK);
		break;
	case WM_ASYNCSELECTMSG :
		switch( WSAGETSELECTEVENT( lParam ) )
		{
		case FD_CONNECT :
			break;
		case FD_READ :
			SocketClient.nRecv();
			break;
		case FD_WRITE :
			SocketClient.FDWriteSend();
			break;
		case FD_CLOSE :
			g_pChatListBox->AddText("", GlobalText[3], SEASON3B::TYPE_SYSTEM_MESSAGE);
#ifdef CONSOLE_DEBUG
			switch(WSAGETSELECTERROR(lParam))
			{
			case WSAECONNRESET:
				g_ConsoleDebug->Write(MCD_ERROR, "The connection was reset by the remote side.");
				g_ErrorReport.Write("The connection was reset by the remote side.\r\n");
				g_ErrorReport.WriteCurrentTime();
				break;
			case WSAECONNABORTED:
				g_ConsoleDebug->Write(MCD_ERROR, "The connection was terminated due to a time-out or other failure.");
				g_ErrorReport.Write("The connection was terminated due to a time-out or other failure.\r\n");
				g_ErrorReport.WriteCurrentTime();
				break;
			}
#endif // CONSOLE_DEBUG
			SocketClient.Close();

			#ifdef NEW_PROTOCOL_SYSTEM
				gProtocolSend.DisconnectServer();
			#endif	

			// SocketClient.Close() above hands off to ReconnectOnCloseSocket(),
			// which puts the client into RECONNECT status. This popup used to
			// fire unconditionally, so the "disconnected from the server" box
			// appeared even while a reconnect was running - the other reconnect
			// hooks were added around it but this one was missed. Only report
			// the lost server when we are not about to reconnect.
#if(UseReconnect)
			if (g_pReconnect->s_Data.ReconnectStatus != CReconnect::RECONNECT_STATUS_RECONNECT)
#endif
			{
				CUIMng::Instance().PopUpMsgWin(MESSAGE_SERVER_LOST);
			}
			break;
		}
		break;
	case WM_CTLCOLOREDIT:
		SetBkColor((HDC)wParam, RGB(0, 0, 0));
		SetTextColor((HDC)wParam, RGB(255, 255, 255));
		return (LRESULT)GetStockObject(BLACK_BRUSH);
		break;
	case WM_ERASEBKGND:
		return TRUE;
		break;
	case WM_PAINT:
		{
			PAINTSTRUCT ps ;
			HDC hDC = BeginPaint(hwnd, &ps);
			EndPaint(hwnd, &ps) ;
		}
		return 0;
		break;
	case WM_DESTROY:
		{
			Destroy = true;
			SocketClient.Close();

			#ifdef NEW_PROTOCOL_SYSTEM
				gProtocolSend.DisconnectServer();
			#endif	

			DestroySound();
			DestroyWindow();
			KillGLWindow();	
			CloseMainExe();
			ExitProcess(0);
			//PostQuitMessage(0);
		}
		break;
    case WM_SETCURSOR:
        ShowCursor(false);
		break;
#if (defined WINDOWMODE)
	case WM_SIZE:
		if ( SIZE_MINIMIZED == wParam && g_bUseWindowMode == FALSE )
		{
			if ( !( g_bMinimizedEnabled))
			{
				DWORD dwMess[SIZE_ENCRYPTION_KEY];
				for ( int i = 0; i < SIZE_ENCRYPTION_KEY; ++i)
				{
					dwMess[i] = GetTickCount();
				}
				g_SimpleModulusCS.LoadKeyFromBuffer( ( BYTE*)dwMess, FALSE, FALSE, FALSE, TRUE);
			}
		}
		break;
#else
#ifdef NDEBUG
#ifndef FOR_WORK
#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
	case WM_SIZE:
		if ( SIZE_MINIMIZED == wParam
#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
			&& g_bUseWindowMode == FALSE
#endif
			)
		{
			if ( !( g_bMinimizedEnabled))
			{
				SendHackingChecked( 0x05, 0);
				DWORD dwMess[SIZE_ENCRYPTION_KEY];
				for ( int i = 0; i < SIZE_ENCRYPTION_KEY; ++i)
				{
					dwMess[i] = GetTickCount();
				}
				g_SimpleModulusCS.LoadKeyFromBuffer( ( BYTE*)dwMess, FALSE, FALSE, FALSE, TRUE);
			}
		}
		break;
#endif
#endif
#endif
#endif	//WINDOWMODE(#else)
//-----------------------------
	default:
		if (msg >= WM_CHATROOMMSG_BEGIN && msg < WM_CHATROOMMSG_END)
			g_pChatRoomSocketList->ProcessSocketMessage(msg - WM_CHATROOMMSG_BEGIN, WSAGETSELECTEVENT(lParam));
		break;
	}

	MouseLButtonDBClick = false;
	if (MouseLButtonPop == true && (g_iMousePopPosition_x != MouseX || g_iMousePopPosition_y != MouseY)) 
		MouseLButtonPop = false;
    switch (msg)
	{
	case WM_MOUSEMOVE:
		{
			// the client area can differ from the render size (window resized / scaled by Windows): map through it
			float px = (float)(short)LOWORD(lParam);
			float py = (float)(short)HIWORD(lParam);
			PcPresentMapMouse(&px, &py);
			MouseX = px / g_fScreenRate_x; //g_fScreenRate_x
			MouseY = py / g_fScreenRate_y;
			if(MouseX < 0) 
				MouseX = 0;
			if(MouseX > DisplayWin) //Fix Wide
				MouseX = DisplayWin;
			if(MouseY < 0) 
				MouseY = 0;
			if(MouseY > DisplayHeight)
				MouseY = DisplayHeight;
		}
		break;
	case WM_LBUTTONDOWN:
		g_iNoMouseTime = 0;
		MouseLButtonPop = false;
		if(!MouseLButton) 
			MouseLButtonPush = true;
		MouseLButton = true;
#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
		SetCapture(g_hWnd);
#endif
		break;
	case WM_LBUTTONUP: //514
		g_iNoMouseTime = 0;
		MouseLButtonPush = false;
		//if(MouseLButton) MouseLButtonPop = true;
		MouseLButtonPop = true;
		MouseLButton = false;
		g_iMousePopPosition_x = MouseX;
		g_iMousePopPosition_y = MouseY;
#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
		ReleaseCapture();
#endif
		break;
	case WM_RBUTTONDOWN:
		g_iNoMouseTime = 0;
		MouseRButtonPop = false;
		if(!MouseRButton) MouseRButtonPush = true;
		MouseRButton = true;
#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
		SetCapture(g_hWnd);
#endif
		break;
	case WM_RBUTTONUP:
		g_iNoMouseTime = 0;
		MouseRButtonPush = false;
		if(MouseRButton) MouseRButtonPop = true;
		MouseRButton = false;
#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
		ReleaseCapture();
#endif
		break;
    case WM_LBUTTONDBLCLK:
		g_iNoMouseTime = 0;
		MouseLButtonDBClick = true;
		break;
	case WM_MBUTTONDOWN:
		Camera3DSetMove = true;
		g_iNoMouseTime = 0;
		MouseMButtonPop = false;
		if(!MouseMButton) MouseMButtonPush = true;
		MouseMButton = true;
#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
		SetCapture(g_hWnd);
#endif
		break;
	case WM_MBUTTONUP:
		Camera3DSetMove = false;
		g_iNoMouseTime = 0;
		MouseMButtonPush = false;
		if(MouseMButton) MouseMButtonPop = true;
		MouseRButton = false;
#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
		ReleaseCapture();
#endif
		break;
	case WM_MOUSEWHEEL:
        {
            
			//==Camera 3D

			//if(!Camera3DEnable)
			
			MouseWheel = (short)HIWORD(wParam) / WHEEL_DELTA;
			
			if (Camera3DEnable && Zoom3D == 0 && GetTickCount() > BlockMouseWheel)
			{
				if (((short)HIWORD(wParam) / WHEEL_DELTA) > 0)
				{
					//if(CameraFOV < 95.f) Zoom3D += 5;
					if (CameraFOV < gProtect.m_MainInfo.ZoomMax) Zoom3D += 5;
				}
				else
				{
					if (CameraFOV > gProtect.m_MainInfo.ZoomMin) Zoom3D -= 5;
				}
			}
        }
        break;
	case WM_IME_NOTIFY:
		{
			if (g_iChatInputType == 1)
			{
				switch (wParam)
				{
				case IMN_SETCONVERSIONMODE:
					if (GetFocus() == g_hWnd)
					{
						CheckTextInputBoxIME(IME_CONVERSIONMODE);
					}
					break;
				case IMN_SETSENTENCEMODE:
					if (GetFocus() == g_hWnd)
					{
						CheckTextInputBoxIME(IME_SENTENCEMODE);
					}
					break;
				default:
					break;
				}
			}
		}
		break;
	case WM_CHAR:
		{
			switch(wParam)
			{
			case VK_RETURN:
				{
					SetEnterPressed( true );
				}
				break;
			}
		}
		break;
    }
	if( g_BuffSystem ) {
		LRESULT result;
		TheBuffStateSystem().HandleWindowMessage( msg, wParam, lParam, result );
	}

    return DefWindowProc(hwnd,msg,wParam,lParam);
}

bool CreateOpenglWindow()
{
    PIXELFORMATDESCRIPTOR pfd;

    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 16;
	pfd.cDepthBits   = 16;

	if (!(g_hDC=GetDC(g_hWnd)))
	{
		g_ErrorReport.Write( "OpenGL Get DC Error - ErrorCode : %d\r\n", GetLastError());
		KillGLWindow();
		MessageBox(NULL,GlobalText[4],"OpenGL Get DC Error.",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;
	}

	GLuint PixelFormat;

	if (!(PixelFormat=ChoosePixelFormat(g_hDC,&pfd)))
	{
		g_ErrorReport.Write( "OpenGL Choose Pixel Format Error - ErrorCode : %d\r\n", GetLastError());
		KillGLWindow();
		MessageBox(NULL,GlobalText[4],"OpenGL Choose Pixel Format Error.",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;
	}

	if(!SetPixelFormat(g_hDC,PixelFormat,&pfd))
	{
		g_ErrorReport.Write( "OpenGL Set Pixel Format Error - ErrorCode : %d\r\n", GetLastError());
		KillGLWindow();
		MessageBox(NULL,GlobalText[4],"OpenGL Set Pixel Format Error.",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;
	}

	if (!(g_hRC=wglCreateContext(g_hDC)))
	{
		g_ErrorReport.Write( "OpenGL Create Context Error - ErrorCode : %d\r\n", GetLastError());
		KillGLWindow();
		MessageBox(NULL,GlobalText[4],"OpenGL Create Context Error.",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;
	}

	if(!wglMakeCurrent(g_hDC,g_hRC))
	{
		g_ErrorReport.Write( "OpenGL Make Current Error - ErrorCode : %d\r\n", GetLastError());
		KillGLWindow();
		MessageBox(NULL,GlobalText[4],"OpenGL Make Current Error.",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;
	}
	////===GLew
	//if (glewInit() != GLEW_OK) {
	//	KillGLWindow();
	//	MessageBox(NULL, GlobalText[4], "glewInit() Make Current Error.", MB_OK | MB_ICONEXCLAMATION);
	//	return FALSE;
	//}
	//InitVBO();
	//====
	ShowWindow(g_hWnd,SW_SHOW);
	SetForegroundWindow(g_hWnd);
	SetFocus(g_hWnd);
	return true;
}
#include <shobjidl.h>
void SetUniqueAppID() {
	
	srand(static_cast<unsigned int>(time(nullptr)));

	
	int randomID = rand();
	std::wstringstream appIDStream;
	appIDStream << L"MyApp.UniqueWindowID" << randomID;

	
	SetCurrentProcessExplicitAppUserModelID(appIDStream.str().c_str());
}
HWND StartWindow(HINSTANCE hCurrentInst,int nCmdShow)
{

   char *windowName = gProtect.m_MainInfo.WindowName;
   //SetUniqueAppID(); //

    WNDCLASS wndClass;
    HWND hWnd;
	
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc   = WndProc;
    wndClass.cbClsExtra    = 0;
    wndClass.cbWndExtra    = 0;
    wndClass.hInstance     = hCurrentInst;
    wndClass.hIcon		   = LoadIcon(hCurrentInst, (LPCTSTR)IDI_ICON1);
    wndClass.hCursor	   = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wndClass.lpszMenuName  = NULL;
    wndClass.lpszClassName = windowName;
    RegisterClass(&wndClass);

	gTrayMode.Init(g_hInst);

#ifdef ENABLE_FULLSCREEN
	// Window mode: a fixed, captioned window, centred on the work area. Fullscreen: a topmost popup covering the
	// (already switched) screen. Not tied to FOR_WORK any more - that switch used to force the borderless popup always.
	if (g_bUseWindowMode == TRUE)
	{
		const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_CLIPCHILDREN;
		RECT rc = { 0, 0, (LONG)WindowWidth, (LONG)WindowHeight };
		AdjustWindowRect(&rc, style, FALSE);
		const int ww = rc.right - rc.left;
		const int wh = rc.bottom - rc.top;

		RECT wa;
		SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
		int x = wa.left + ((wa.right - wa.left) - ww) / 2;
		int y = wa.top + ((wa.bottom - wa.top) - wh) / 2;
		if (x < wa.left) x = wa.left;
		if (y < wa.top) y = wa.top;

		hWnd = CreateWindow(windowName, windowName, style, x, y, ww, wh, NULL, NULL, hCurrentInst, NULL);
	}
	else
	{
		hWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_APPWINDOW, windowName, windowName, WS_POPUP, 0, 0, WindowWidth, WindowHeight, NULL, NULL, hCurrentInst, NULL);
	}
#else //ENABLE_FULLSCREEN
	{
		RECT rc = { 0, 0, WindowWidth, WindowHeight };
#if defined WINDOWMODE
		if (g_bUseWindowMode == TRUE)
			AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_CLIPCHILDREN, NULL);
		else
#endif
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, NULL);
		rc.right -= rc.left;
		rc.bottom -= rc.top;
#if defined WINDOWMODE
		if (g_bUseWindowMode == TRUE)
		{
		hWnd = CreateWindow(
			windowName, windowName,
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_CLIPCHILDREN,
			(GetSystemMetrics(SM_CXSCREEN) - rc.right) / 2,
			(GetSystemMetrics(SM_CYSCREEN) - rc.bottom) / 2,
			rc.right,
			rc.bottom,
			NULL, NULL, hCurrentInst, NULL);
		}
		else
#endif
		{
		hWnd = CreateWindow(
			windowName, windowName,
			WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
			(GetSystemMetrics(SM_CXSCREEN) - rc.right) / 2,
			(GetSystemMetrics(SM_CYSCREEN) - rc.bottom) / 2,
			rc.right,
			rc.bottom,
			NULL, NULL, hCurrentInst, NULL);
		}
	}
#endif //ENABLE_FULLSCREEN
	
    return hWnd;
}

char m_ID[11];
char m_Pass[21];
char m_Version[11];
char m_ExeVersion[11];
int  m_SavePassOnOff;
int  m_SoundOnOff;
int  m_MusicOnOff;
int  m_Resolution;
int	m_nColorDepth;
int	g_iRenderTextType = 0;

char m_FontName[30];
int m_FontSizePlus = 0;

int mShowName;
int mShowHPBar;
int mShowMiniMap;
int mShowDanhHieu;
int mHideMarketNotice;

// wzAudioCreate is gated on m_MusicOnOff at startup (below), which reads as
// false on every run - nothing in this codebase ever writes the "MusicOnOff"
// registry value the read at the bottom of this file checks, so that read
// always takes its != ERROR_SUCCESS branch and defaults to false. wzAudio
// has therefore never actually been created by this code path. Anything
// that turns music on later (e.g. NewUIOptionWindow.cpp's checkbox) has to
// check this and create it lazily first, or wzAudioPlay crashes into an
// instance that was never set up.
bool g_bWzAudioCreated = false;

// Same gap, same shape, different subsystem: InitDirectSound (below) only
// runs if m_SoundOnOff was already true at startup. Unlike MusicOnOff,
// nothing here forces that false - the registry read defaults it to true
// when the (also never-written) "SoundOnOff" key is missing - but a machine
// that happens to have that key set to 0 from an unrelated, older install
// (this is a real, historically-used Webzen registry path) hits the exact
// same never-initialized gap SetEnableSound() can walk into. Confirmed via
// a crash inside CreateStaticBuffer (DSplaysound.cpp) the first time a
// monster's sound buffer loaded after the Sound checkbox was turned on with
// DirectSound never created - g_lpDS was still null.
bool g_bDirectSoundCreated = false;

void SaveConfigDword(const char* lpszName, int iValue)
{
	leaf::CRegKey regkey;
	regkey.SetKey(leaf::CRegKey::_HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu\\Config");
	regkey.WriteDword(lpszName, (DWORD)iValue);
}


char g_aszMLSelection[MAX_LANGUAGE_NAME_LENGTH] = {'\0'};
std::string g_strSelectedML = "";

// Saves the language the player picked ("Eng" or "Vtm"). It takes effect on the next start:
// the data files are read once at startup (see LocalLanguageFile). config.ini is ours and is
// read first; the registry value is also written because the older launcher / .reg scripts
// use it, and a stale one there must not disagree with what the player chose.
void SetSelectedLanguage(const char* pszLanguage)
{
	char szCode[8];
	NormalizeLanguageCode(pszLanguage, szCode);

	WritePrivateProfileStringA("LOGIN", "Language", szCode, "./config.ini");

	HKEY hKey;
	if (ERROR_SUCCESS == RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu\\Config", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL))
	{
		RegSetValueExA(hKey, "LangSelection", 0, REG_SZ, (const BYTE*)szCode, (DWORD)strlen(szCode) + 1);
		RegCloseKey(hKey);
	}
}

// Starts this exe again, with the command line of this run, a few seconds from now. The delay is
// done by a plain cmd.exe that holds no handle to this process (so the anti-cheat's handle scan
// sees nothing new) and gives the old client time to close first.
static void LaunchGameAfterDelay()
{
	char szExe[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, szExe, MAX_PATH);

	char szDir[MAX_PATH] = { 0 };
	strncpy(szDir, szExe, MAX_PATH - 1);
	char* pSlash = strrchr(szDir, '\\');
	if (pSlash != NULL)
	{
		*pSlash = '\0';
	}

	// Everything after the program name in the command line.
	const char* pszArgs = GetCommandLineA();
	if (*pszArgs == '"')
	{
		++pszArgs;
		while (*pszArgs != '\0' && *pszArgs != '"') ++pszArgs;
		if (*pszArgs == '"') ++pszArgs;
	}
	else
	{
		while (*pszArgs != '\0' && *pszArgs != ' ') ++pszArgs;
	}
	while (*pszArgs == ' ') ++pszArgs;

	char szCommand[2048];
	_snprintf(szCommand, sizeof(szCommand) - 1, "cmd.exe /C \"ping -n 6 127.0.0.1 >nul & start \"\" /D \"%s\" \"%s\" %s\"", szDir, szExe, pszArgs);
	szCommand[sizeof(szCommand) - 1] = '\0';

	// CREATE_NO_WINDOW alone: cmd gets a hidden console that ping inherits. With DETACHED_PROCESS
	// (which overrides CREATE_NO_WINDOW) cmd has no console and ping opens its own visible one.
	STARTUPINFOA si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	if (CreateProcessA(NULL, szCommand, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, szDir, &si, &pi))
	{
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
}

// After the player changed the language (SetSelectedLanguage already saved it): asks, in the language
// just chosen, whether to restart the game now. The data files are read once at startup, so the
// change only shows after a restart. Returns true when the game is closing to restart; false when the
// player said no (the choice stays saved for the next start).
bool AskRestartForLanguage(int iWantLang)
{
	const wchar_t* pszTitle = L"Language";
	const wchar_t* pszText = L"Language changed to English.\nRestart the game now?";
	if (iWantLang == LANG_INDEX_VIETNAMESE)
	{
		pszTitle = L"Ngôn ngữ";
		pszText = L"Đã đổi ngôn ngữ sang Tiếng Việt.\nKhởi động lại game ngay bây giờ?";
	}
	else if (iWantLang == LANG_INDEX_SPANISH)
	{
		pszTitle = L"Idioma";
		pszText = L"Idioma cambiado a Español.\n¿Reiniciar el juego ahora?";
	}

	// The game hides the system cursor (ShowCursor(false) on every WM_SETCURSOR, so the counter is far
	// below zero) and may clip it to its window: bring the cursor back for the box, then put the counter
	// back where it was.
	ReleaseCapture();
	ClipCursor(NULL);
	int iShown = 0;
	int iCount = ShowCursor(TRUE);
	++iShown;
	while (iCount < 0 && iShown < 2000)
	{
		iCount = ShowCursor(TRUE);
		++iShown;
	}

	// No owner window on purpose: a box owned by the game window passes its WM_SETCURSOR up to the
	// game's WndProc, which calls ShowCursor(false) on every mouse move and keeps the cursor hidden.
	// MB_TASKMODAL still blocks the game's own windows while it is open.
	const int iAnswer = MessageBoxW(NULL, pszText, pszTitle, MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND | MB_TOPMOST | MB_TASKMODAL);

	while (iShown-- > 0)
	{
		ShowCursor(FALSE);
	}

	if (iAnswer != IDYES)
	{
		return false;
	}

	g_ErrorReport.Write("> Language changed, restarting the game.");

	if (SceneFlag == MAIN_SCENE)
	{
		// In the world: leave the way the Exit Game button does, so the server saves and closes the
		// session. The server's reply ends this process.
		if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MIXINVENTORY))
		{
			g_pChatListBox->AddText("", GlobalText[592], SEASON3B::TYPE_ERROR_MESSAGE);
			return false;
		}

		LaunchGameAfterDelay();
		SaveOptions();
		SaveMacro("Data\\Macro.txt");
		SendRequestLogOut(0);
	}
	else
	{
		LaunchGameAfterDelay();
		SendMessage(g_hWnd, WM_DESTROY, 0, 0);
	}
	return true;
}

BOOL OpenInitFile()
{
	//char szTemp[50];
	char szIniFilePath[256+20]="";
	char szCurrentDir[256];

	GetCurrentDirectory(256, szCurrentDir);

	strcpy(szIniFilePath, szCurrentDir);
	if( szCurrentDir[strlen(szCurrentDir)-1] == '\\' ) 
		strcat(szIniFilePath, "config.ini");
	else strcat(szIniFilePath, "\\config.ini");

	GetPrivateProfileString ("LOGIN", "Version", "", m_Version, 11, szIniFilePath);

	char *lpszCommandLine = GetCommandLine();
	char lpszFile[MAX_PATH];
	if ( GetFileNameOfFilePath( lpszFile, lpszCommandLine))
	{
		WORD wVersion[4];
		if ( GetFileVersion( lpszFile, wVersion))
		{
			sprintf( m_ExeVersion, "%d.%02d", wVersion[0], wVersion[1]);
			if ( wVersion[2] > 0)
			{
                char lpszMinorVersion[3] = "a";
                if ( wVersion[2]>26 )
                {
                    lpszMinorVersion[0] = 'A';
				    lpszMinorVersion[0] += ( wVersion[2] - 27 );
                    lpszMinorVersion[1] = '+';
                }
                else
                {
				    lpszMinorVersion[0] += ( wVersion[2] - 1);
                }
				strcat( m_ExeVersion, lpszMinorVersion);
			}
		}
		else
		{
			strcpy( m_ExeVersion, m_Version);
		}
	}
	else
	{
		strcpy( m_ExeVersion, m_Version);
	}
	//==== Custom Font
	GetPrivateProfileString("FONT", "FontName", "", m_FontName, 30, szIniFilePath);
	m_FontSizePlus = GetPrivateProfileInt("FONT", "FontSizeBonus", 0, szIniFilePath);

	//===Custom Config
	mShowName = GetPrivateProfileInt("Custom", "ShowName", 1, szIniFilePath);
	mShowHPBar = GetPrivateProfileInt("Custom", "ShowHPBar", 0, szIniFilePath);
	mShowMiniMap = GetPrivateProfileInt("Custom", "ShowMiniMap", 0, szIniFilePath);
	mShowDanhHieu = GetPrivateProfileInt("Custom", "ShowDanhHieu", 0, szIniFilePath);
	mHideMarketNotice = GetPrivateProfileInt("Custom", "HideMarketNotice", 0, szIniFilePath);



	g_bGMObservation = mShowName;
//#ifdef _DEBUG

	m_ID[0] = '\0';
	m_Pass[0] = '\0';
	m_SavePassOnOff = 0;
	m_SoundOnOff = 1;
	m_MusicOnOff = 1;
	m_Resolution = 0;
	m_nColorDepth = 0;

	HKEY hKey;
	DWORD dwDisp;
	DWORD dwSize;
	if ( ERROR_SUCCESS == RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu\\Config", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, & hKey, &dwDisp))
	{
		dwSize = 11;
		if ( RegQueryValueEx (hKey, "ID", 0, NULL, (LPBYTE)m_ID, & dwSize) != ERROR_SUCCESS)
		{
		}

		dwSize = sizeof(int);
		if ( RegQueryValueEx (hKey, "SoundOnOff", 0, NULL, (LPBYTE) & m_SoundOnOff, &dwSize) != ERROR_SUCCESS)
		{
			m_SoundOnOff = true;
		}
		dwSize = sizeof ( int);
		// Defaults on, like SoundOnOff above. It defaulted *off*, which meant
		// a fresh install with no registry value never created wzAudio at all
		// (see the audio init in WinMain) and so had no music by default.
		if ( RegQueryValueEx (hKey, "MusicOnOff", 0, NULL, (LPBYTE) & m_MusicOnOff, &dwSize) != ERROR_SUCCESS)
		{
			m_MusicOnOff = true;
		}
		dwSize = sizeof ( int);
		if ( RegQueryValueEx (hKey, "Resolution", 0, NULL, (LPBYTE) & m_Resolution, &dwSize) != ERROR_SUCCESS)
			m_Resolution = 1;

		if (0 == m_Resolution)
			m_Resolution = 3;

	    if ( RegQueryValueEx (hKey, "ColorDepth", 0, NULL, (LPBYTE) & m_nColorDepth, &dwSize) != ERROR_SUCCESS)
	    {
		    m_nColorDepth = 0;
	    }
		dwSize = sizeof ( int);
		if ( RegQueryValueEx (hKey, "TextOut", 0, NULL, (LPBYTE) & g_iRenderTextType, &dwSize) != ERROR_SUCCESS)
		{
			g_iRenderTextType = 0;
		}

		g_iChatInputType = 1;

#if defined USER_WINDOW_MODE || (defined WINDOWMODE)
		dwSize = sizeof ( int);
		if ( RegQueryValueEx (hKey, "WindowMode", 0, NULL, (LPBYTE) & g_bUseWindowMode, &dwSize) != ERROR_SUCCESS)
		{
			g_bUseWindowMode = FALSE;
		}
#endif // USER_WINDOW_MODE

		dwSize = MAX_LANGUAGE_NAME_LENGTH;
		if ( RegQueryValueEx (hKey, "LangSelection", 0, NULL, (LPBYTE)g_aszMLSelection, &dwSize) != ERROR_SUCCESS)
		{
			strcpy(g_aszMLSelection, "Eng");
		}
		g_strSelectedML = g_aszMLSelection;
	}
	RegCloseKey( hKey);

	{
		char szLanguage[16] = "";
		GetPrivateProfileString("LOGIN", "Language", "", szLanguage, sizeof(szLanguage), szIniFilePath);
		if (szLanguage[0] != '\0')
		{
			g_strSelectedML = szLanguage;
		}
		char szCode[8];
		NormalizeLanguageCode(g_strSelectedML.c_str(), szCode);
		g_strSelectedML = szCode;
		strcpy(g_aszMLSelection, szCode);
		gOther.Load();
	}

	// MusicOnOff / SoundOnOff / VolumeLevel stay in the registry under
	// SOFTWARE\Webzen\Mu\Config - the same values Scripts\*.reg toggles
	// externally - so the in-game checkboxes and those scripts drive one
	// shared setting instead of disagreeing. The reads above are unchanged;
	// what was missing was any *write*, which is why the checkboxes could not
	// survive a restart. SaveConfigDword() below is now called whenever one
	// is changed.

#if(WIDE_SCREEN)
	float GetPos = 0.0f;
	int fsW = 0, fsH = 0;
	switch (m_Resolution) //Resolution
	{
	case 0:WindowWidth = 640; WindowHeight = 480;	GetPos = 1.25f; break;
	case 1:WindowWidth = 800; WindowHeight = 600;	GetPos = 1.25f; break;
	case 2:WindowWidth = 1024; WindowHeight = 640;	GetPos = 1.25f; break;
	case 3:WindowWidth = 1280; WindowHeight = 800;	GetPos = 1.50f; break;
	case 4:WindowWidth = 1366; WindowHeight = 768;	GetPos = 1.60f; break;
	case 5:WindowWidth = 1440; WindowHeight = 900;	GetPos = 1.69f; break;
	case 6:WindowWidth = 1536; WindowHeight = 960;	GetPos = 1.87f; break;
	case 7:WindowWidth = 1680; WindowHeight = 1050; GetPos = 2.10f; break;
	case 8:WindowWidth = 1920; WindowHeight = 1200; GetPos = 2.10f; break;
	case 9:WindowWidth = 2560; WindowHeight = 1600; GetPos = 2.10f; break;
	case 10:WindowWidth = 3840; WindowHeight = 2400; GetPos = 2.10f; break;
	// 16:9 and 4:3/5:4 sizes (the UI scale follows the screen: the same layout as 1366x768 for 16:9)
	case 11:WindowWidth = 1280; WindowHeight = 720; break;
	case 12:WindowWidth = 1600; WindowHeight = 900; break;
	case 13:WindowWidth = 1920; WindowHeight = 1080; break;
	case 14:WindowWidth = 2560; WindowHeight = 1440; break;
	case 15:WindowWidth = 3840; WindowHeight = 2160; break;
	case 16:WindowWidth = 1024; WindowHeight = 768; break;
	case 17:WindowWidth = 1280; WindowHeight = 1024; break;
	case 18:WindowWidth = 1600; WindowHeight = 1200; break;
	case 99:WindowWidth = GetSystemMetrics(SM_CXSCREEN); WindowHeight = GetSystemMetrics(SM_CYSCREEN); break; // the monitor's own resolution
	default:WindowWidth = 1920; WindowHeight = 1200; GetPos = 1.20f; break;
		//case 3:WindowWidth=1920;WindowHeight=1440;break;
		//case 4:WindowWidth=1600;WindowHeight=1200;break;
	}

	if (GetPos <= 0.0f)
	{
		GetPos = ScreenScaleFor((int)WindowWidth, (int)WindowHeight);
	}

	// A window must fit on the screen: when the chosen size is bigger than the work area, use the biggest size that fits.
	if (g_bUseWindowMode == TRUE)
	{
		RECT wa;
		SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);

		RECT frame = { 0, 0, 100, 100 };
		AdjustWindowRect(&frame, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_CLIPCHILDREN, FALSE);
		const int extraW = (frame.right - frame.left) - 100;
		const int extraH = (frame.bottom - frame.top) - 100;
		const int maxW = (wa.right - wa.left) - extraW;
		const int maxH = (wa.bottom - wa.top) - extraH;

		if ((int)WindowWidth > maxW || (int)WindowHeight > maxH)
		{
			static const int sizes[][2] = { {2560,1440}, {1920,1080}, {1680,1050}, {1600,900}, {1440,900}, {1366,768}, {1280,720}, {1024,768}, {800,600} };

			for (int n = 0; n < (int)(sizeof(sizes) / sizeof(sizes[0])); n++)
			{
				if (sizes[n][0] <= maxW && sizes[n][1] <= maxH)
				{
					g_ErrorReport.Write("> Window %dx%d does not fit the screen: using %dx%d.\r\n", (int)WindowWidth, (int)WindowHeight, sizes[n][0], sizes[n][1]);
					WindowWidth = sizes[n][0];
					WindowHeight = sizes[n][1];
					GetPos = ScreenScaleFor(sizes[n][0], sizes[n][1]);
					break;
				}
			}
		}
	}

	ApplyScreenSize((int)WindowWidth, (int)WindowHeight, GetPos);

#else
	switch (m_Resolution)
	{
	case 0:
		WindowWidth = 640;
		WindowHeight = 480;
		break;
	case 1:
		WindowWidth = 800;
		WindowHeight = 600;
		break;
	case 2:
		WindowWidth = 1024;
		WindowHeight = 768;
		break;
	case 3:
		WindowWidth = 1280;
		WindowHeight = 768;
		break;
	case 4:
		WindowWidth = 1366;
		WindowHeight = 768;
		break;
	case 5:
		WindowWidth = 1440;
		WindowHeight = 900;
		break;
	case 6:
		WindowWidth = 1600;
		WindowHeight = 900;
		break;
	case 7:
		WindowWidth = 1600;
		WindowHeight = 1280;
		break;
	case 8:
		WindowWidth = 1680;
		WindowHeight = 1050;
		break;
	case 9:
		WindowWidth = 1920;
		WindowHeight = 1080;
		break;
	case 10:
		WindowWidth = 2560;
		WindowHeight = 1440;
		break;
	default:
		WindowWidth = 640;
		WindowHeight = 480;
		break;
	}

	g_fScreenRate_x = (float)WindowWidth / 640;
	g_fScreenRate_y = (float)WindowHeight / 480;
	DisplayWin = 640;
	DisplayHeight = 480;

	DisplayWinReal = 640;
	DisplayWinMid = 320;
	DisplayWinCDepthBox = 0;

	DisplayHeight = 480;
	DisplayHeightExt = 0;
#endif


	g_ConsoleDebug->Write(MCD_NORMAL, "[IP] %s Port %d", szServerIpAddress, g_ServerPort);
	g_ConsoleDebug->Write(MCD_NORMAL, "[Resolusion] %d %dx%d", m_Resolution, WindowWidth,WindowHeight);
	return TRUE;
}

BOOL Util_CheckOption( char *lpszCommandLine, unsigned char cOption, char *lpszString)
{
	unsigned char cComp[2];
	cComp[0] = cOption; cComp[1] = cOption;
	if ( islower( ( int)cOption))
	{
		cComp[1] = toupper( ( int)cOption);
	}
	else if ( isupper( ( int)cOption))
	{
		cComp[1] = tolower( ( int)cOption);
	}

	int nFind = ( int)'/';
	unsigned char *lpFound = ( unsigned char*)lpszCommandLine;
	while ( lpFound)
	{
		lpFound = ( unsigned char*)strchr( ( char*)( lpFound + 1), nFind);
		if ( lpFound && ( *( lpFound + 1) == cComp[0] || *( lpFound + 1) == cComp[1]))
		{	// ¹ß°ß
			if ( lpszString)
			{
				int nCount = 0;
				for ( unsigned char *lpSeek = lpFound + 2; *lpSeek != ' ' && *lpSeek != '\0'; lpSeek++)
				{
					nCount++;
				}

				memcpy( lpszString, lpFound + 2, nCount);
				lpszString[nCount] = '\0';
			}
			return ( TRUE);
		}
	}

	return ( FALSE);
}

BOOL UpdateFile( char *lpszOld, char *lpszNew)
{
	SetFileAttributes(lpszOld, FILE_ATTRIBUTE_NORMAL);
	SetFileAttributes(lpszNew, FILE_ATTRIBUTE_NORMAL);

	DWORD dwStartTickCount = ::GetTickCount();
	while(::GetTickCount() - dwStartTickCount < 5000) {
		if ( CopyFile( lpszOld, lpszNew, FALSE))
		{	// ¼º°ø
			DeleteFile( lpszOld);
			return ( TRUE);
		}
	}
	g_ErrorReport.Write("%s to %s CopyFile Error : %d\r\n", lpszNew, lpszOld, GetLastError());
	return ( FALSE);
}

#include <tlhelp32.h>

BOOL KillExeProcess( char *lpszExe)
{
	HANDLE hProcessSnap = NULL; 
    BOOL bRet = FALSE; 
    PROCESSENTRY32 pe32 = { 0 }; 
 
    //  Take a snapshot of all processes in the system. 

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); 

    if (hProcessSnap == INVALID_HANDLE_VALUE) 
        return (FALSE); 
 
    //  Fill in the size of the structure before using it. 

    pe32.dwSize = sizeof(PROCESSENTRY32); 
 
    //  Walk the snapshot of the processes, and for each process, 
    //  display information. 

    if (Process32First(hProcessSnap, &pe32)) 
    {
        do 
        { 
			if(stricmp(pe32.szExeFile, lpszExe) == 0)
			{
				HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID);

				if(process)
				{
					TerminateProcess(process, 0);
				}
			}
        } while (Process32Next(hProcessSnap, &pe32)); 
        bRet = TRUE; 
    } 
    else 
        bRet = FALSE;    // could not walk the list of processes 
 
    // Do not forget to clean up the snapshot object. 

    CloseHandle (hProcessSnap);

	return bRet;
}

char g_lpszCmdURL[50];
BOOL GetConnectServerInfo( PSTR szCmdLine, char *lpszURL, WORD *pwPort)
{
	char lpszTemp[256] = {0, };
	if( Util_CheckOption( szCmdLine, 'y', lpszTemp))
	{
		BYTE bySuffle[] = { 0x0C, 0x07, 0x03, 0x13 };

		for(int i=0; i<(int)strlen(lpszTemp); i++)
			lpszTemp[i] -= bySuffle[i%4];
		strcpy(lpszURL, lpszTemp);

		if( Util_CheckOption( szCmdLine, 'z', lpszTemp)) 
		{
			for(int j=0; j<(int)strlen(lpszTemp); j++)
				lpszTemp[j] -= bySuffle[j%4];
			*pwPort = atoi( lpszTemp);
		}
		g_ErrorReport.Write("[Virtual Connection] Connect IP : %s, Port : %d\r\n", lpszURL, *pwPort);
		return (TRUE);
	}
	if ( !Util_CheckOption( szCmdLine, 'u', lpszTemp))
	{
		return ( FALSE);
	}
	strcpy( lpszURL, lpszTemp);
	if ( !Util_CheckOption( szCmdLine, 'p', lpszTemp))
	{
		return ( FALSE);
	}
	*pwPort = atoi( lpszTemp);

	return ( TRUE);
}


extern int TimeRemain;
BOOL g_bInactiveTimeChecked = FALSE;
void MoveObject(OBJECT *o);
#if(!UseStackLog)
//=== MuError.dmp
#include <dbghelp.h>
#pragma comment(lib,"dbghelp.lib")
//===
bool ExceptionCallback(_EXCEPTION_POINTERS* pExceptionInfo )
{

	char path[MAX_PATH];

	SYSTEMTIME SystemTime;

	GetLocalTime(&SystemTime);

	wsprintf(path, "%d-%d-%d_%dh%dm%ds.dmp", SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay, SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);

	HANDLE file = CreateFile(path, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	if (file != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION mdei;

		mdei.ThreadId = GetCurrentThreadId();

		mdei.ExceptionPointers = pExceptionInfo;

		mdei.ClientPointers = 0;

		if (MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, (MINIDUMP_TYPE)(MiniDumpScanMemory + MiniDumpWithIndirectlyReferencedMemory), &mdei, 0, 0) != 0)
		{
			CloseHandle(file);
			g_ErrorReport.Write("Save DumpFile complete %s" , path);
			return EXCEPTION_EXECUTE_HANDLER;
		}
	}

	CloseHandle(file);

#ifdef ENABLE_FULLSCREEN
	if (g_bUseWindowMode == FALSE)
	{
		ChangeDisplaySettings(NULL,0); // a crash must not leave the desktop in the game's resolution
	}
#endif //ENABLE_FULLSCREEN

	return EXCEPTION_CONTINUE_SEARCH;
	return true;
}
#endif
char* szServerIpAddress = "139.99.24.220";
WORD g_ServerPort = 63000;
BYTE Version[SIZE_PROTOCOLVERSION] = { '1' + 1, '0' + 2, '4' + 3, '0' + 4, '5' + 5 };
BYTE Serial[SIZE_PROTOCOLSERIAL + 1] = { "TbYehR2hFUPBKgZj" };
#if (GetGPUUse)
#if defined(_M_X64) || defined(__amd64__)
#define NVAPI_DLL "nvapi64.dll"
#else
#define NVAPI_DLL "nvapi.dll"
#endif

#include <windows.h>
// magic numbers, do not change them
#define NVAPI_MAX_PHYSICAL_GPUS   64
#define NVAPI_MAX_USAGES_PER_GPU  34

// function pointer types
typedef int* (*NvAPI_QueryInterface_t)(unsigned int offset);
typedef int (*NvAPI_Initialize_t)();
typedef int (*NvAPI_EnumPhysicalGPUs_t)(int** handles, int* count);
typedef int (*NvAPI_GPU_GetUsages_t)(int* handle, unsigned int* usages);
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
extern "C" __declspec(dllexport) int GetNvOptimusEnablement() { return NvOptimusEnablement; }
extern "C" __declspec(dllexport) int GetAmdPowerXpressRequest() { return AmdPowerXpressRequestHighPerformance; }
#endif

void SleepForMilliseconds(int milliseconds) {
	LARGE_INTEGER start, end, frequency;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);

	do {
		QueryPerformanceCounter(&end);
	} while ((end.QuadPart - start.QuadPart) * 1000 / frequency.QuadPart < milliseconds);
}
MSG MainLoop()
{
	MSG msg;
	while (1)
	{
	
		if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			// CustomMessage.h does "#undef GetMessage" so its class can declare a
			// GetMessage() method, which removes the Win32 GetMessage ->
			// GetMessageA macro for every file that includes it. Call the ANSI
			// entry point directly, matching PeekMessage/DispatchMessage above.
			if (!GetMessageA(&msg, NULL, 0, 0))
			{
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{

			//Scene
#if (defined WINDOWMODE)
			if (g_bUseWindowMode || g_bWndActive)
			{
				Scene(g_hDC);
			}
			else
			{
				// fullscreen game in the background (Alt+Tab): it waits, minimized, until the player comes back
				Sleep(20);
			}
#else//WINDOWMODE
			if (g_bWndActive)
				Scene(g_hDC);

#endif	//WINDOWMODE(#else)
		}
#ifdef NEW_PROTOCOL_SYSTEM
		if (SceneFlag < CHARACTER_SCENE)
			ProtocolCompiler();

		g_pChatRoomSocketList->ProtocolCompile();
		gProtocolSend.RecvMessage();
#else
		//=== Fix Fps Test
		ProtocolCompiler();
		g_pChatRoomSocketList->ProtocolCompile();
#endif

	} // while( 1 )

	return msg;
}

inline SpinLock* g_MUHelper_lock = new SpinLock();
void CALLBACK MUHelperTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	if (SceneFlag == 5)
	{
		g_MUHelper_lock->lock();
		if (gCB_MUHelper) gCB_MUHelper->Work();
		g_MUHelper_lock->unlock();
	}
}
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int nCmdShow)
{
	
#if(UseStackLog)
	StartStackLogging();
#endif
#if (GetGPUUse)
	HMODULE hmod = LoadLibraryA(NVAPI_DLL);
	if (hmod == NULL)
	{

		MessageBox(0, "Couldn't find nvapi.dll", "Error", MB_OK | MB_ICONERROR);
		ExitProcess(0);
	}

	// nvapi.dll internal function pointers
	NvAPI_QueryInterface_t      NvAPI_QueryInterface = NULL;
	NvAPI_Initialize_t          NvAPI_Initialize = NULL;
	NvAPI_EnumPhysicalGPUs_t    NvAPI_EnumPhysicalGPUs = NULL;
	NvAPI_GPU_GetUsages_t       NvAPI_GPU_GetUsages = NULL;

	// nvapi_QueryInterface is a function used to retrieve other internal functions in nvapi.dll
	NvAPI_QueryInterface = (NvAPI_QueryInterface_t)GetProcAddress(hmod, "nvapi_QueryInterface");

	// some useful internal functions that aren't exported by nvapi.dll
	NvAPI_Initialize = (NvAPI_Initialize_t)(*NvAPI_QueryInterface)(0x0150E828);
	NvAPI_EnumPhysicalGPUs = (NvAPI_EnumPhysicalGPUs_t)(*NvAPI_QueryInterface)(0xE5AC921F);
	NvAPI_GPU_GetUsages = (NvAPI_GPU_GetUsages_t)(*NvAPI_QueryInterface)(0x189A1FDF);

	if (NvAPI_Initialize == NULL || NvAPI_EnumPhysicalGPUs == NULL ||
		NvAPI_EnumPhysicalGPUs == NULL || NvAPI_GPU_GetUsages == NULL)
	{
		MessageBox(0, "Couldn't get functions in nvapi.dll", "Error", MB_OK | MB_ICONERROR);
		ExitProcess(0);
	}

	// initialize NvAPI library, call it once before calling any other NvAPI functions
	(*NvAPI_Initialize)();

	int          gpuCount = 0;
	int* gpuHandles[NVAPI_MAX_PHYSICAL_GPUS] = { NULL };
	unsigned int gpuUsages[NVAPI_MAX_USAGES_PER_GPU] = { 0 };

	// gpuUsages[0] must be this value, otherwise NvAPI_GPU_GetUsages won't work
	gpuUsages[0] = (NVAPI_MAX_USAGES_PER_GPU * 4) | 0x10000;

	(*NvAPI_EnumPhysicalGPUs)(gpuHandles, &gpuCount);

	// print GPU usage every second
	//int usages[NVAPI_MAX_PHYSICAL_GPUS];
	//while (TRUE)
	//{
	//	for (int j = 0; j < gpuCount; j++)
	//	{
	//		(*NvAPI_GPU_GetUsages)(gpuHandles[j], gpuUsages);
	//		usages[j] = gpuUsages[3];
	//		//g_Console.AddMessage(1, "GPU Usage: %d", usages[j]);
	//	}
	//	Sleep(1000);
	//}
#endif
#if(!UseStackLog)
	leaf::AttachExceptionHandler(ExceptionCallback);
#endif
	char lpszExeVersion[256] = "unknown";

	char *lpszCommandLine = GetCommandLine();
	char lpszFile[MAX_PATH];
	WORD wVersion[4] = { 0,};
	if ( GetFileNameOfFilePath( lpszFile, lpszCommandLine))
	{
		if ( GetFileVersion( lpszFile, wVersion))
		{
			sprintf( lpszExeVersion, "%d.%02d", wVersion[0], wVersion[1]);
			if ( wVersion[2] > 0)
			{
				char lpszMinorVersion[2] = "a";
				lpszMinorVersion[0] += ( wVersion[2] - 1);
				strcat( lpszExeVersion, lpszMinorVersion);
			}
		}
	}

	g_ErrorReport.Write( "\r\n");
	g_ErrorReport.WriteLogBegin();
	g_ErrorReport.AddSeparator();
	g_ErrorReport.Write( "Mu online %s (%s) executed. (%d.%d.%d.%d)\r\n", lpszExeVersion, "Eng", wVersion[0], wVersion[1], wVersion[2], wVersion[3]);

	g_ConsoleDebug->Write(MCD_NORMAL, "Mu Online (Version: %d.%d.%d.%d)", wVersion[0], wVersion[1], wVersion[2], wVersion[3]);

	g_ErrorReport.WriteCurrentTime();
	ER_SystemInfo si;
	ZeroMemory( &si, sizeof ( ER_SystemInfo));
	GetSystemInfo( &si);
	g_ErrorReport.AddSeparator();
	g_ErrorReport.WriteSystemInfo( &si);
	g_ErrorReport.AddSeparator();

	if (!gMainLoad.Load())
	{
		return false;
	}


	// PKD_ADD_BINARY_PROTECTION
	VM_START
	WORD wPortNumber;	
	if ( GetConnectServerInfo( szCmdLine, g_lpszCmdURL, &wPortNumber))
	{
		szServerIpAddress = g_lpszCmdURL;
		g_ServerPort = wPortNumber;
	}
	VM_END

	if ( !OpenMainExe())
	{
		return false;
	}

	// PKD_ADD_BINARY_PROTECTION
	VM_START
	g_SimpleModulusCS.LoadEncryptionKey( "Data\\Enc1.dat");
	g_SimpleModulusSC.LoadDecryptionKey( "Data\\Dec2.dat");

	VM_END

	g_ErrorReport.Write( "> To read config.ini.\r\n");
	if( OpenInitFile() == FALSE )
	{
		g_ErrorReport.Write( "config.ini read error\r\n");
		return false;
	}

	pMultiLanguage = new CMultiLanguage(g_strSelectedML);

	if (g_iChatInputType == 1)
		ShowCursor(FALSE);

	g_ErrorReport.Write( "> Enum display settings.\r\n");
	DEVMODE DevMode;
	DEVMODE* pDevmodes;
	int nModes = 0;
	while (EnumDisplaySettings(NULL, nModes, &DevMode)) nModes++;
	pDevmodes = new DEVMODE[nModes+1];
	nModes = 0;
	while (EnumDisplaySettings(NULL, nModes, &pDevmodes[nModes])) nModes++;

	DWORD dwBitsPerPel = 16;
	for(int n1=0; n1<nModes; n1++)
	{
		if(pDevmodes[n1].dmBitsPerPel == 16 && m_nColorDepth == 0) {
			dwBitsPerPel = 16; break;
		}
		if(pDevmodes[n1].dmBitsPerPel == 24 && m_nColorDepth == 1) {
			dwBitsPerPel = 24; break;
		}
		if(pDevmodes[n1].dmBitsPerPel == 32 && m_nColorDepth == 1) {
			dwBitsPerPel = 32; break;
		}
	}

#ifdef ENABLE_FULLSCREEN
	if (g_bUseWindowMode == FALSE)
	{
		// Real fullscreen: switch the monitor to the chosen resolution. A resolution the monitor does not have falls back
		// to the desktop's own resolution (still fullscreen, still no stretching).
		bool ok = FullscreenFindMode((int)WindowWidth, (int)WindowHeight) && FullscreenApply();

		if (ok == false)
		{
			const int dw = GetSystemMetrics(SM_CXSCREEN);
			const int dh = GetSystemMetrics(SM_CYSCREEN);

			g_ErrorReport.Write("> Fullscreen %dx%d is not available on this monitor: using %dx%d.\r\n", (int)WindowWidth, (int)WindowHeight, dw, dh);

			ApplyScreenSize(dw, dh, ScreenScaleFor(dw, dh));
			ok = FullscreenFindMode(dw, dh) && FullscreenApply();
		}

		if (ok)
		{
			g_ErrorReport.Write("> Fullscreen %dx%d, %d bit, %d Hz.\r\n", (int)g_FsMode.dmPelsWidth, (int)g_FsMode.dmPelsHeight, (int)g_FsMode.dmBitsPerPel, (int)g_FsMode.dmDisplayFrequency);
		}
		else
		{
			g_ErrorReport.Write("> Fullscreen failed: starting in a window.\r\n");
			g_FsHave = false;
			g_bUseWindowMode = TRUE;
		}
	}
#endif //ENABLE_FULLSCREEN

	delete [] pDevmodes;

	g_ErrorReport.Write( "> Screen size = %d x %d.\r\n", WindowWidth, WindowHeight);

    g_hInst = hInstance;
    g_hWnd = StartWindow(hInstance,nCmdShow);
	g_ErrorReport.Write( "> Start window success.\r\n");

    if ( !CreateOpenglWindow())
	{
		return FALSE;
	}

	

	g_ErrorReport.Write( "> OpenGL init success.\r\n");
	g_ErrorReport.AddSeparator();
	//g_ErrorReport.WriteOpenGLInfo();
	g_ErrorReport.AddSeparator();
	g_ErrorReport.WriteSoundCardInfo(); 

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

	//g_ErrorReport.WriteImeInfo( g_hWnd);
	g_ErrorReport.AddSeparator();


	//InitVSync();
	//if (IsVSyncAvailable())
	//{
	//	EnableVSync();
	//	//SetTargetFps(-1); // unlimited
	//}
	
	switch(WindowWidth)
	{
		case 640 :FontHeight = 12;break;
		case 800 :FontHeight = 13;break;
		case 1024:FontHeight = 14;break;
		case 1280:FontHeight = 15;break;
		default:FontHeight = 14; break;

	}
	
	int nFixFontHeight = 13 + m_FontSizePlus;
	int nFixFontSize;
	int iFontSize;
	FontHeight = FontHeight + m_FontSizePlus;
	iFontSize = FontHeight - 1;
	nFixFontSize = nFixFontHeight - 1;

	g_hFont		= CreateFont(iFontSize,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE, m_FontName);
	g_hFontBold = CreateFont(iFontSize,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE, m_FontName);
	g_hFontBoldName = CreateFont(15, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, m_FontName);
	g_hFontBig	= CreateFont(iFontSize*2,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE, m_FontName);
	g_hFixFont	= CreateFont(nFixFontSize,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH | FF_DONTCARE, m_FontName);
	g_hFontMini = CreateFont(10, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, m_FontName);
	setlocale( LC_ALL, "english");

	CInput::Instance().Create(g_hWnd, WindowWidth, WindowHeight);

	g_pNewUISystem->Create();

	// Both audio engines are created unconditionally now, and the on/off
	// flags are applied afterwards as mutes. They used to gate creation
	// itself, which turned each flag into a one-shot startup decision that
	// could not be undone from the new Config System checkboxes: with sound
	// off at launch the wave table never loaded at all (see LoadWaveFile,
	// DSplaysound.cpp) so enabling it later stayed silent, and with music
	// off wzAudio was never created so the first PlayMp3 crashed into it.
	// Creating both up front costs one device init plus the wave table's
	// memory - exactly what an ordinary sound-on launch already paid.
	if(wzAudioCreate(g_hWnd) == 0)
	{
		g_bWzAudioCreated = true;
		wzAudioOption(WZAOPT_STOPBEFOREPLAY, 1);
	}

	if(SUCCEEDED(InitDirectSound(g_hWnd)))
	{
		g_bDirectSoundCreated = true;

		leaf::CRegKey regkey;
		regkey.SetKey(leaf::CRegKey::_HKEY_CURRENT_USER, "SOFTWARE\\Webzen\\Mu\\Config");
		DWORD value;
		if(!regkey.ReadDword("VolumeLevel", value))
		{
			value = SOUND_VOLUME_FULL;
			regkey.WriteDword("VolumeLevel", value);
		}

		// 0 is total silence - SetEffectVolumeLevel(0) calls
		// SetMasterVolume(-10000) - and a stale VolumeLevel=0 in the registry
		// is exactly what silenced every skill/UI effect while music kept
		// playing, since wzAudio has its own separate volume. Now that the
		// draggable volume bar is gone and Sound is a plain on/off checkbox,
		// silence is the checkbox's job, so 0 here means "unset" and gets the
		// full level. The old clamp also turned a perfectly valid 10 (what
		// Scripts\Sound&MusicON.reg writes, and the actual maximum) into 5.
		if(value < 1 || value > SOUND_VOLUME_FULL)
			value = SOUND_VOLUME_FULL;

		g_pOption->SetVolumeLevel(int(value));
		SetEffectVolumeLevel(g_pOption->GetVolumeLevel());
	}

	// InitDirectSound sets g_EnableSound true on success, so apply the
	// user's actual preference now that the engine is up. Kept false if the
	// device failed to initialise - SetEnableSound only assigns the flag, so
	// letting it go true with no DirectSound behind it is the exact
	// null-g_lpDS state the LoadWaveFile guard now also refuses.
	SetEnableSound(g_bDirectSoundCreated && m_SoundOnOff != 0);

	SetTimer(g_hWnd, HACK_TIMER, 20*1000, NULL);

	gCB_MUHelper = new CB_MUHelper;
	SetTimer(g_hWnd, MU_HELPERAUTO, 100, MUHelperTimerProc); // was 250; the attack rate itself comes from attack speed (CB_MUHelper.cpp)

	srand((unsigned)time(NULL));
	for(int i=0;i<100;i++)
	{
		RandomTable[i] = rand()%360;
	}

	//memorydump[0]
	RendomMemoryDump = new BYTE [rand()%100+1];


	GateAttribute				= new GATE_ATTRIBUTE [MAX_GATES];
	SkillAttribute				= new SKILL_ATTRIBUTE[MAX_SKILLS];

	//memorydump[1]
	ItemAttRibuteMemoryDump		= new ITEM_ATTRIBUTE [MAX_ITEM+ 1024];
	ItemAttribute				= ((ITEM_ATTRIBUTE*)ItemAttRibuteMemoryDump)+rand()% 1024;

	//memorydump[2]
	CharacterMemoryDump			= new CHARACTER      [MAX_CHARACTERS_CLIENT+1+128];
	CharactersClient			= ((CHARACTER*)CharacterMemoryDump)+rand()%128;
	CharacterMachine			= new CHARACTER_MACHINE;

	memset(GateAttribute       ,0,sizeof(GATE_ATTRIBUTE   )*(MAX_GATES              ));
	//memset(ItemAttribute       ,0,sizeof(ITEM_ATTRIBUTE   )*(MAX_ITEM     + 2048));
	memset(ItemAttRibuteMemoryDump, 0, sizeof(ITEM_ATTRIBUTE)* (MAX_ITEM + 1024)); //Fix RenderItemName
	memset(SkillAttribute      ,0,sizeof(SKILL_ATTRIBUTE  )*(MAX_SKILLS             ));
	memset(CharacterMachine    ,0,sizeof(CHARACTER_MACHINE));

    CharacterAttribute   = &CharacterMachine->Character;
    CharacterMachine->Init();
	Hero = &CharactersClient[0];	

	if (g_iChatInputType == 1)
	{
		 g_pMercenaryInputBox = new CUIMercenaryInputBox;
		 g_pSingleTextInputBox = new CUITextInputBox;
		 g_pSinglePasswdInputBox = new CUITextInputBox;
	}
	
	g_pChatRoomSocketList = new CChatRoomSocketList;
	g_pUIManager = new CUIManager;
	g_pUIMapName = new CUIMapName;	// rozy
	g_pTimer = new CTimer();

#ifdef MOVIE_DIRECTSHOW
	g_pMovieScene = new CMovieScene;
#endif // MOVIE_DIRECTSHOW
	
	g_BuffSystem = BuffStateSystem::Make();

	g_MapProcess = MapProcess::Make();

	g_petProcess = PetProcess::Make();

	CUIMng::Instance().Create();

	if (g_iChatInputType == 1)
	{
		g_pMercenaryInputBox->Init(g_hWnd);
		g_pSingleTextInputBox->Init(g_hWnd, 200, 20);
		g_pSinglePasswdInputBox->Init(g_hWnd, 200, 20, 9, TRUE);
		g_pSingleTextInputBox->SetState(UISTATE_HIDE);
		g_pSinglePasswdInputBox->SetState(UISTATE_HIDE);

		g_pMercenaryInputBox->SetFont(g_hFont);
		g_pSingleTextInputBox->SetFont(g_hFont);
		g_pSinglePasswdInputBox->SetFont(g_hFont);

		g_bIMEBlock = FALSE;
		HIMC  hIMC = ImmGetContext(g_hWnd);
		ImmSetConversionStatus(hIMC, IME_CMODE_ALPHANUMERIC, IME_SMODE_NONE );
		ImmReleaseContext(g_hWnd, hIMC);
		SaveIMEStatus();
		g_bIMEBlock = TRUE;
	}
#if (defined WINDOWMODE)
	if (g_bUseWindowMode == FALSE)
	{
		int nOldVal;
		SystemParametersInfo(SPI_SCREENSAVERRUNNING, 1, &nOldVal, 0);
		SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &g_iScreenSaverOldValue, 0);
		SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, 300*60, NULL, 0);
	}
#else
#ifdef NDEBUG
#ifndef FOR_WORK
#ifdef ACTIVE_FOCUS_OUT
	if (g_bUseWindowMode == FALSE)
	{
#endif	// ACTIVE_FOCUS_OUT
		int nOldVal; // °ªÀÌ µé¾î°¥ ÇÊ¿ä°¡ ¾øÀ½
		SystemParametersInfo(SPI_SCREENSAVERRUNNING, 1, &nOldVal, 0);  // ´ÜÃàÅ°¸¦ ¸ø¾²°Ô ÇÔ
		SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &g_iScreenSaverOldValue, 0);  // ½ºÅ©¸°¼¼ÀÌ¹ö Â÷´Ü
		SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, 300*60, NULL, 0);  // ½ºÅ©¸°¼¼ÀÌ¹ö Â÷´Ü
#ifdef ACTIVE_FOCUS_OUT
	}
#endif	// ACTIVE_FOCUS_OUT
#endif
#endif
#endif	//WINDOWMODE(#else)

#ifdef SAVE_PACKET
	DeleteFile( PACKET_SAVE_FILE);
#endif

#if defined PROTECT_SYSTEMKEY && defined NDEBUG
#ifndef FOR_WORK
	ProtectSysKey::AttachProtectSysKey(g_hInst, g_hWnd);
#endif // !FOR_WORK
#endif // PROTECT_SYSTEMKEY && NDEBUG

	const MSG msg = MainLoop();
	DestroyWindow();
	
    return msg.wParam;
}

