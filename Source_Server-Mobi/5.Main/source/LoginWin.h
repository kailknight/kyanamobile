//*****************************************************************************
// File: LoginWin.h
//*****************************************************************************
#pragma once

#include "Win.h"

#include "Button.h"

class CUITextInputBox;

// Scales a design-space (329x245) login layout constant for the Android/iOS
// login window, which is itself enlarged by the same factor (see CLoginWin::Create).
// Shared with CB_AutoLogin so the save-password checkbox and saved-account list it
// draws on top of this window scale with it instead of drifting off as the window
// grows on taller phones - both are defined once in LoginWin.cpp.
int ScaleLoginMetric(int value);

class CLoginWin : public CWin
{
protected:
	CSprite		m_asprInputBox[2];
	CButton		m_aBtn[2];
	CUITextInputBox*	m_pIDInputBox, * m_pPassInputBox;

#if(CB_DANGKYINGAME)
	CButton		m_DangKy;
#endif
public:
	CLoginWin();
	virtual ~CLoginWin();
	void Create();
	void SetPosition(int nXCoord, int nYCoord);
	void Show(bool bShow);
	bool CursorInWin(int nArea);

	void ConnectConnectionServer();

	CUITextInputBox* GetIDInputBox() const { return m_pIDInputBox; }
	CUITextInputBox* GetPassInputBox() const { return m_pPassInputBox; }
	bool FocusInputAt(float uiX, float uiY);

private:
	int FirstLoad = 0;

protected:
	void PreRelease();
	void UpdateWhileActive(double dDeltaTick);
	void UpdateWhileShow(double dDeltaTick);
	void RenderControls();
	void RequestLogin();
	void CancelLogin();
};
