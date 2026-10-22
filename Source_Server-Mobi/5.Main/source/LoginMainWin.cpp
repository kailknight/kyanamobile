//*****************************************************************************
// File: LoginMainWin.cpp
//*****************************************************************************

#include "stdafx.h"
#include "LoginMainWin.h"

#include "Input.h"
#include "UIMng.h"

#include "ZzzBMD.h"
#include "ZzzInfomation.h"
#include "ZzzObject.h"
#include "DSPlaySound.h"

#include "ZzzCharacter.h"
#include "wsclientinline.h"
#include "MultiLanguage.h"
#if defined(MU_IOS)
#include "GameConfig/GameConfig.h"
#endif

extern void SetSelectedLanguage(const char* pszLanguage);
#if !defined(__ANDROID__) && !defined(MU_IOS)
extern bool AskRestartForLanguage(bool bWantVi);	// Winmain.cpp
#elif defined(__ANDROID__) || defined(MU_IOS)
extern void MU_MobileAskRestartForLanguage(bool vietnamese);	// Platform/MobilePlatform.cpp
#endif

#ifdef MOVIE_DIRECTSHOW
#include <dshow.h>
#include "MovieScene.h"

extern CMovieScene* g_pMovieScene;
#endif // MOVIE_DIRECTSHOW

extern char* g_lpszMp3[NUM_MUSIC];

CLoginMainWin::CLoginMainWin()
{
	m_bLangChanged = false;
}

CLoginMainWin::~CLoginMainWin()
{

}

void CLoginMainWin::Create()
{
	for (int i = 0; i <= LMW_BTN_CREDIT; ++i)
		m_aBtn[i].Create(54, 30, BITMAP_LOG_IN+4 + i, 3, 2, 1);

	// Language switch: the same plain text button the system menu uses (SysMenuWin.cpp).
	m_aBtn[LMW_BTN_LANG].Create(108, 30, BITMAP_TEXT_BTN, 4, 2, 1);
	RefreshLanguageButton();
#ifdef MOVIE_DIRECTSHOW
	m_aBtn[LMW_BTN_MOVIE].Create(54, 30, BITMAP_LOG_IN+15, 3, 2, 1);
#endif	// MOVIE_DIRECTSHOW

	CWin::Create(CInput::Instance().GetScreenWidth() - 30 * 2,
		m_aBtn[0].GetHeight() + LMW_LANG_ROW_H, -2);

	for (int i = 0; i < LMW_BTN_MAX; ++i)
		CWin::RegisterButton(&m_aBtn[i]);

	m_sprDeco.Create(189, 103, BITMAP_LOG_IN+6, 0, NULL, 105, 59);
}

void CLoginMainWin::PreRelease()
{
	m_sprDeco.Release();
}

void CLoginMainWin::RefreshLanguageButton()
{
	static DWORD s_adwClr[4] = { CLRDW_BR_GRAY, CLRDW_BR_GRAY, CLRDW_WHITE, 0 };
	const bool bVi = IsVietnameseLanguage();
#if defined(MU_IOS)
	// Both language controls share the persisted choice, not separate toggle flags.
	m_bLangChanged = (GameConfig::GetInstance().GetLanguageSelection() == L"Vtm") != bVi;
#endif
	const char* pszLabel;
	if (!m_bLangChanged)
	{
		pszLabel = bVi ? "English" : "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t";
	}
	else
	{
		// bVi is still the language this run STARTED in. The saved choice is the other one.
		pszLabel = bVi ? "Restart game" : "Kh\xE1\xBB\x9Fi \xC4\x91\xE1\xBB\x99ng l\xE1\xBA\xA1i";
	}
	m_aBtn[LMW_BTN_LANG].SetText(pszLabel, s_adwClr);
}

void CLoginMainWin::SetPosition(int nXCoord, int nYCoord)
{
	CWin::SetPosition(nXCoord, nYCoord);

	// Menu and Credit sit on the bottom row of the (taller) window; the language button is on
	// the row above Menu, flush left.
	const int nRowY = nYCoord + LMW_LANG_ROW_H;
	m_aBtn[LMW_BTN_LANG].SetPosition(nXCoord, nYCoord);
	m_aBtn[LMW_BTN_MENU].SetPosition(nXCoord, nRowY);
	m_aBtn[LMW_BTN_CREDIT].SetPosition(
		nXCoord + CWin::GetWidth() - m_aBtn[LMW_BTN_CREDIT].GetWidth(),
		nRowY);
#ifdef MOVIE_DIRECTSHOW
	m_aBtn[LMW_BTN_MOVIE].SetPosition(m_aBtn[LMW_BTN_CREDIT].GetXPos()
		- 10 - m_aBtn[LMW_BTN_MOVIE].GetWidth(), nYCoord);
#endif	// MOVIE_DIRECTSHOW
	m_sprDeco.SetPosition(
		m_aBtn[LMW_BTN_CREDIT].GetXPos(), m_aBtn[LMW_BTN_CREDIT].GetYPos());
}

void CLoginMainWin::Show(bool bShow)
{
#if defined(MU_IOS)
	if (bShow)
		RefreshLanguageButton();
#endif
	CWin::Show(bShow);

	for (int i = 0; i < LMW_BTN_MAX; ++i)
		m_aBtn[i].Show(bShow);
	m_sprDeco.Show(bShow);
}

bool CLoginMainWin::CursorInWin(int nArea)
{
	if (!CWin::m_bShow)
		return false;

	switch (nArea)
	{
	case WA_MOVE:
		return false;
	}

	return CWin::CursorInWin(nArea);
}

void CLoginMainWin::UpdateWhileActive(double dDeltaTick)
{
	if (m_aBtn[LMW_BTN_MENU].IsClick())
	{
		CUIMng& rUIMng = CUIMng::Instance();
		rUIMng.ShowWin(&rUIMng.m_SysMenuWin);
		rUIMng.SetSysMenuWinShow(true);
	}
	else if (m_aBtn[LMW_BTN_LANG].IsClick())
	{
		// Saved at once; read at the next start (the language data files load once at startup).
		// Clicking again before restarting switches back.
		const bool bStartedVi = IsVietnameseLanguage();
#if defined(MU_IOS)
		RefreshLanguageButton();
#endif
		m_bLangChanged = !m_bLangChanged;
		const bool bWantVi = m_bLangChanged ? !bStartedVi : bStartedVi;
		SetSelectedLanguage(bWantVi ? "Vtm" : "Eng");
		RefreshLanguageButton();
#if !defined(__ANDROID__) && !defined(MU_IOS)
		// Only when it differs from the language this run started in; going back is silent.
		if (m_bLangChanged)
		{
			AskRestartForLanguage(bWantVi);
		}
#elif defined(__ANDROID__) || defined(MU_IOS)
		if (m_bLangChanged)
		{
			MU_MobileAskRestartForLanguage(bWantVi);
		}
#endif
	}
	else if (m_aBtn[LMW_BTN_CREDIT].IsClick())
	{
		SendRequestServerList();

		CUIMng& rUIMng = CUIMng::Instance();
		rUIMng.ShowWin(&rUIMng.m_CreditWin);

		::StopMp3(g_lpszMp3[MUSIC_MAIN_THEME]);
		::PlayMp3(g_lpszMp3[MUSIC_MUTHEME]);
	}
#ifdef MOVIE_DIRECTSHOW
	else if (m_aBtn[LMW_BTN_MOVIE].IsClick())
	{
		g_pMovieScene = new CMovieScene;
		g_pMovieScene->Initialize_DirectShow(g_hWnd, MOVIE_FILE_WMV);
		if(g_pMovieScene->IsFile() == FALSE || g_pMovieScene->IsFailDirectShow() == TRUE)
		{
			g_pMovieScene->Destroy();
			SAFE_DELETE(g_pMovieScene);
			return;	
		}
		::StopMp3(g_lpszMp3[MUSIC_MAIN_THEME]);
		CUIMng& rUIMng = CUIMng::Instance();
		rUIMng.SetMoving(true);
	}
#endif	// MOVIE_DIRECTSHOW
}

void CLoginMainWin::RenderControls()
{
	m_sprDeco.Render();
	CWin::RenderButtons();
}