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
#define	LMW_BTN_OPT0		4
#define	LMW_BTN_MAX			7
#else	// MOVIE_DIRECTSHOW
#define	LMW_BTN_LANG		2
#define	LMW_BTN_OPT0		3
#define	LMW_BTN_MAX			6
#endif	// MOVIE_DIRECTSHOW

// Height of the language button row. The window is this much taller than Menu/Credit alone;
// CUIMng::CreateLoginScene derives the window's y from its height, so Menu and Credit stay
// exactly where they were and the language button sits on the row above them.
#define	LMW_LANG_ROW_H		36

// The language button opens a drop-down list of the three languages (LMW_BTN_OPT0 + language index).
// The list opens upward, so the window is this much taller; being anchored at the bottom, Menu and
// Credit still do not move.
#define	LMW_DROP_ROW_H		30
#define	LMW_DROP_H			(LMW_DROP_ROW_H * 3)

class CLoginMainWin : public CWin  
{
protected:
	CButton	m_aBtn[LMW_BTN_MAX];
	CSprite	m_sprDeco;
	bool	m_bLangOpen;	// the drop-down list is showing
	int		m_iLangWanted;	// language index (MultiLanguage.h) chosen this session, -1 = none yet; it applies on restart

public:
	CLoginMainWin();
	virtual ~CLoginMainWin();

	void Create();
	void SetPosition(int nXCoord, int nYCoord);
	void RefreshLanguageButton();
	void ShowLanguageList(bool bOpen);
	void Show(bool bShow);
	bool CursorInWin(int nArea);

protected:
	void PreRelease();
	void UpdateWhileActive(double dDeltaTick);
	void RenderControls();
};

#endif // !defined(AFX_LOGINMAINWIN_H__96B05A69_6360_4C8E_BD9C_20FC72EBE1C6__INCLUDED_)
