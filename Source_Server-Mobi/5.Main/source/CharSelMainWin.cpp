//*****************************************************************************
// File: CharSelMainWin.cpp
//*****************************************************************************

#include "stdafx.h"
#include "CharSelMainWin.h"
#include "Input.h"
#include "UIMng.h"
#include "ZzzBMD.h"
#include "ZzzInfomation.h"
#include "ZzzObject.h"
#include "ZzzCharacter.h"
#include "ZzzInterface.h"
#include "UIGuildInfo.h"
#include "ZzzOpenData.h"
#include "ZzzOpenglUtil.h"
#include "ServerListManager.h"
#include "LoginWin.h"
extern int DisplayWinCDepthBox;
extern int DisplayWin;
extern int DisplayWinMid;
extern int DisplayWinExt;
extern int DisplayWinReal;

extern float g_fScreenRate_x;
extern float g_fScreenRate_y;

CCharSelMainWin::CCharSelMainWin()
{

}

CCharSelMainWin::~CCharSelMainWin()
{

}

void CCharSelMainWin::Create()
{
	m_asprBack[CSMW_SPR_DECO].Create(189, 103, BITMAP_LOG_IN+2);
	m_asprBack[CSMW_SPR_INFO].Create(
	CInput::Instance().GetScreenWidth() - 266, 21);
	m_asprBack[CSMW_SPR_INFO].SetColor(0, 0, 0);
	m_asprBack[CSMW_SPR_INFO].SetAlpha(143);


	m_aBtn[CSMW_BTN_CREATE].Create(54, 30, BITMAP_LOG_IN+3, 4, 2, 1, 3);
	m_aBtn[CSMW_BTN_MENU].Create(54, 30, BITMAP_LOG_IN+4, 3, 2, 1);
	m_aBtn[CSMW_BTN_CONNECT].Create(54, 30, BITMAP_LOG_IN+5, 4, 2, 1, 3);
	m_aBtn[CSMW_BTN_DELETE].Create(54, 30, BITMAP_LOG_IN+6, 4, 2, 1, 3);

#if defined(__ANDROID__) || defined(MU_IOS)
	// Create() above builds each button's sprite frames off the native 54x30
	// art, so the SetSize calls below only inflate the render/click box (same
	// two-step pattern LoginWin.cpp uses for its own buttons) - a real 54x30
	// px button is a couple percent of a modern phone's screen height and all
	// but untappable, which is exactly why Create/Menu/Connect/Delete could
	// not be hit.
	for (int i = 0; i < CSMW_BTN_MAX; ++i)
	{
		m_aBtn[i].SetSize(ScaleLoginMetric(54), ScaleLoginMetric(30));
	}
#endif

	CWin::Create(
		m_aBtn[0].GetWidth() * CSMW_BTN_MAX + m_asprBack[CSMW_SPR_INFO].GetWidth() + 6,
		m_aBtn[0].GetHeight(), -2);

	for (int i = 0; i < CSMW_BTN_MAX; ++i)
		CWin::RegisterButton(&m_aBtn[i]);

	m_bAccountBlockItem = false;

	for (int i = 0; i < 5; ++i)
	{
		if (CharactersClient[i].Object.Live)
		{
			if (CharactersClient[i].CtlCode & CTLCODE_10ACCOUNT_BLOCKITEM)
			{
				m_bAccountBlockItem = true;
				break;
			}
		}
	}
}

void CCharSelMainWin::PreRelease()
{
	for (int i = 0; i < CSMW_SPR_MAX; ++i)
		m_asprBack[i].Release();
}

void CCharSelMainWin::SetPosition(int nXCoord, int nYCoord)
{
	CWin::SetPosition(nXCoord, nYCoord);

	int nBtnWidth = m_aBtn[0].GetWidth();

	m_aBtn[CSMW_BTN_CREATE].SetPosition(nXCoord, nYCoord);
	m_aBtn[CSMW_BTN_MENU].SetPosition(nXCoord + nBtnWidth + 1, nYCoord);
	m_asprBack[CSMW_SPR_INFO].SetPosition(m_aBtn[CSMW_BTN_MENU].GetXPos() + nBtnWidth + 2, nYCoord + 5);

	int nWinRPosX = nXCoord + CWin::GetWidth();
	int nConnectGap = 1;

#if defined(__ANDROID__) || defined(MU_IOS)
	// Anchor the right-hand pair to the real screen edge, and put a finger-sized
	// gap between Connect and Delete.
	//
	// CWin::GetWidth() is not usable for this on mobile. Create() builds it from
	// (GetScreenWidth() - 266), which mixes a RENDER-pixel screen width with a
	// 266 that is a logical-640 constant, while the buttons scale by
	// ScaleLoginMetric (~1.11) rather than by g_fScreenRate_x (~2.0). The right
	// edge therefore lands at
	//     WindowWidth + (4 * ScaleLoginMetric(54) - 238)
	// instead of the intended WindowWidth - 22. At a 1860px-wide render target
	// that put Delete 66px off-screen (only ~10% of it tappable, which is why it
	// felt broken); at 1280 it lands 2px over with a ONE pixel gap to Connect,
	// which is how aiming at Connect deletes a character instead.
	//
	// On desktop the original expression is already correct (ScaleLoginMetric is
	// identity, 216 + (640-266) + 6 + 22 == 640 - 22), so it is left alone.
	nWinRPosX = CInput::Instance().GetScreenWidth() - ScaleLoginMetric(22);
	nConnectGap = ScaleLoginMetric(14);
#endif

	m_asprBack[CSMW_SPR_DECO].SetPosition(nWinRPosX - (m_asprBack[CSMW_SPR_DECO].GetWidth() - 22), nYCoord - 59);
	m_aBtn[CSMW_BTN_DELETE].SetPosition(nWinRPosX - nBtnWidth, nYCoord);
	m_aBtn[CSMW_BTN_CONNECT].SetPosition(nWinRPosX - (nBtnWidth * 2 + nConnectGap), nYCoord);
}

void CCharSelMainWin::Show(bool bShow)
{
	CWin::Show(bShow);

	int i;
	for (i = 0; i < CSMW_SPR_MAX; ++i)
		m_asprBack[i].Show(bShow);
	for (i = 0; i < CSMW_BTN_MAX; ++i)
		m_aBtn[i].Show(bShow);
}

bool CCharSelMainWin::CursorInWin(int nArea)
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

void CCharSelMainWin::UpdateDisplay()
{
	m_aBtn[CSMW_BTN_CREATE].SetEnable(false);
	int i=0;

	for (i = 0; i < 5; ++i)
	{
		if (!CharactersClient[i].Object.Live)
		{
			m_aBtn[CSMW_BTN_CREATE].SetEnable(true);
			break;
		}
	}

	if (SelectedHero > -1)
	{
		m_aBtn[CSMW_BTN_CONNECT].SetEnable(true);
		m_aBtn[CSMW_BTN_DELETE].SetEnable(true);
	}
	else
	{
		m_aBtn[CSMW_BTN_CONNECT].SetEnable(false);
		m_aBtn[CSMW_BTN_DELETE].SetEnable(false);
	}


	bool bNobodyCharacter = true;

	for (int i = 0; i < 5; ++i)
	{
		if (CharactersClient[i].Object.Live == true)
		{
			bNobodyCharacter = false;
			break;
		}
	}
	
	if (bNobodyCharacter == true)
	{
		CUIMng& rUIMng = CUIMng::Instance();
		rUIMng.ShowWin(&rUIMng.m_CharMakeWin);
	}
}

void CCharSelMainWin::UpdateWhileActive(double dDeltaTick)
{
	if (m_aBtn[CSMW_BTN_CONNECT].IsClick())
		::StartGame();
	else if (m_aBtn[CSMW_BTN_MENU].IsClick())
	{
		CUIMng& rUIMng = CUIMng::Instance();
		rUIMng.ShowWin(&rUIMng.m_SysMenuWin);
		rUIMng.SetSysMenuWinShow(true);
	}
	else if (m_aBtn[CSMW_BTN_CREATE].IsClick())
	{
		CUIMng& rUIMng = CUIMng::Instance();
		rUIMng.ShowWin(&rUIMng.m_CharMakeWin);

	}
	else if (m_aBtn[CSMW_BTN_DELETE].IsClick())
		DeleteCharacter();
}

void CCharSelMainWin::RenderControls()
{
	for (int i = 0; i < CSMW_SPR_MAX; ++i)
		m_asprBack[i].Render();

	::EnableAlphaTest();
	::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	
	g_pRenderText->SetFont(g_hFixFont);
	g_pRenderText->SetTextColor(CLRDW_WHITE);
	g_pRenderText->SetBgColor(0);

	if (m_bAccountBlockItem)
	{
		g_pRenderText->SetTextColor(0, 0, 0, 255);
		g_pRenderText->SetBgColor(255, 255, 0, 128);
		g_pRenderText->RenderText(DisplayWinMid, 330, GlobalText[436], 0, 0, RT3_WRITE_CENTER);
		g_pRenderText->RenderText(DisplayWinMid, 348, GlobalText[437], 0, 0, RT3_WRITE_CENTER);
	}
	CWin::RenderButtons();
}

void CCharSelMainWin::DeleteCharacter()
{
    if (CharactersClient[SelectedHero].GuildStatus != G_NONE)
		CUIMng::Instance().PopUpMsgWin(MESSAGE_DELETE_CHARACTER_GUILDWARNING);
	else if (CharactersClient[SelectedHero].CtlCode	& (CTLCODE_02BLOCKITEM | CTLCODE_10ACCOUNT_BLOCKITEM))
		CUIMng::Instance().PopUpMsgWin(MESSAGE_DELETE_CHARACTER_ID_BLOCK);
	else
		CUIMng::Instance().PopUpMsgWin(MESSAGE_DELETE_CHARACTER_CONFIRM);
}
