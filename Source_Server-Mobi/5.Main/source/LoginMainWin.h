//*****************************************************************************
// File: LoginMainWin.h
//*****************************************************************************

#if !defined(AFX_LOGINMAINWIN_H__96B05A69_6360_4C8E_BD9C_20FC72EBE1C6__INCLUDED_)
#define AFX_LOGINMAINWIN_H__96B05A69_6360_4C8E_BD9C_20FC72EBE1C6__INCLUDED_

#pragma once

#include "Win.h"
#include "Button.h"

#define	LMW_BTN_MENU		0
#define	LMW_BTN_CREDIT		1
#ifdef MOVIE_DIRECTSHOW
#define	LMW_BTN_MOVIE		2
#define	LMW_BTN_LANG		3
#define	LMW_BTN_MAX			4
#else	// MOVIE_DIRECTSHOW
#define	LMW_BTN_LANG		2
#define	LMW_BTN_MAX			3
#endif	// MOVIE_DIRECTSHOW

// Height of the language button row. The window is this much taller than Menu/Credit alone;
// CUIMng::CreateLoginScene derives the window's y from its height, so Menu and Credit stay
// exactly where they were and the language button sits on the row above them.
#define	LMW_LANG_ROW_H		36

class CLoginMainWin : public CWin  
{
protected:
	CButton	m_aBtn[LMW_BTN_MAX];
	CSprite	m_sprDeco;
	bool	m_bLangChanged;	// a different language was chosen this session; it applies on restart

public:
	CLoginMainWin();
	virtual ~CLoginMainWin();

	void Create();
	void SetPosition(int nXCoord, int nYCoord);
	void RefreshLanguageButton();
	void Show(bool bShow);
	bool CursorInWin(int nArea);

protected:
	void PreRelease();
	void UpdateWhileActive(double dDeltaTick);
	void RenderControls();
};

#endif // !defined(AFX_LOGINMAINWIN_H__96B05A69_6360_4C8E_BD9C_20FC72EBE1C6__INCLUDED_)
