// MsgBoxIGSCommon.h: interface for the CMsgBoxIGSCommon class.
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MSGBOXIGSCOMMON_H__5E2E9B11_C6F5_411E_BC68_1CF1D1DAE3C0__INCLUDED_)
#define AFX_MSGBOXIGSCOMMON_H__5E2E9B11_C6F5_411E_BC68_1CF1D1DAE3C0__INCLUDED_

#pragma once

#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM

#include "UIControls.h"
#include "NewUIMessageBox.h"
#include "NewUICommonMessageBox.h"
#include "NewUIOptionWindow.h"

using namespace SEASON3B;

class CMsgBoxIGSCommon : public CNewUIMessageBoxBase
{
public:
	enum IMAGE_IGS_COMMON
	{
		IMAGE_IGS_BUTTON	= BITMAP_IGS_MSGBOX_BUTTON,
		IMAGE_IGS_BACK		= CNewUIOptionWindow::IMAGE_OPTION_FRAME_BACK,
		IMAGE_IGS_UP		= CNewUIOptionWindow::IMAGE_OPTION_FRAME_UP,
		IMAGE_IGS_DOWN		= CNewUIOptionWindow::IMAGE_OPTION_FRAME_DOWN,
		IMAGE_IGS_LEFTLINE	= CNewUIOptionWindow::IMAGE_OPTION_FRAME_LEFT,
		IMAGE_IGS_RIGHTLINE = CNewUIOptionWindow::IMAGE_OPTION_FRAME_RIGHT,
	};
	
	enum IMAGESIZE_IGS_COMMON
	{
		IMAGE_IGS_WINDOW_WIDTH	= 640,
		IMAGE_IGS_WINDOW_HEIGHT = 429,
		IMAGE_IGS_FRAME_WIDTH	= 190,
		IMAGE_IGS_FRAME_HEIGHT	= 109,
		IMAGE_IGS_UP_HEIGHT		= 64,
		IMAGE_IGS_DOWN_HEIGHT	= 45,
		IMAGE_IGS_LINE_WIDTH	= 21,
		IMAGE_IGS_LINE_HEIGHT	= 10,
		IMAGE_IGS_BTN_WIDTH		= 52,
		IMAGE_IGS_BTN_HEIGHT	= 26,	
	};
	
	enum IGS_COMMON_POS
	{
		IGS_TEXT_TITLE_POS_Y		= 10,
		IGS_TEXT_ITEM_INFO_POS_X	= 10,
		IGS_TEXT_ITEM_INFO_POS_Y	= 40,
		IGS_TEXT_ITEM_INFO_WIDTH	= 170,
		IGS_BTN_POS_Y				= 5,
	};

	enum IGS_COMMON_ETC
	{
		IGS_NUM_TEXT_LIMIT_RENDER_MIDDLE_LINE = 3,
	};
	
public:
	CMsgBoxIGSCommon();
	~CMsgBoxIGSCommon();
	
	bool Create(float fPriority = 3.f);
	void Release();
	
	bool Update();
	bool Render();
	
	void Initialize(const unicode::t_char* pszTitle, const unicode::t_char* pszText);
	
	static CALLBACK_RESULT LButtonUp(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam);
	static CALLBACK_RESULT OKButtonDown(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam);
	static CALLBACK_RESULT CancelButtonDown(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam);
	
private:
	void SetAddCallbackFunc();
	void SetButtonInfo();
	
	void RenderFrame();
	void RenderTexts();
	void RenderButtons();
	
	void LoadImages();
	void UnloadImages();
	
private:
	CNewUIMessageBoxButton m_BtnOk;

	unicode::t_char		m_szTitle[MAX_TEXT_LENGTH];
	unicode::t_char		m_szText[NUM_LINE_CMB][MAX_TEXT_LENGTH];

	int	m_iMsgBoxWidth;
	int m_iMsgBoxHeight;
	int m_iMiddleCount;
	int	m_iNumTextLine;
	
public:
};

/*
	One flat filled rectangle, shared by every cash shop dialog.

	The modern skin builds its dialog frames out of these instead of the ornate
	plates, and four separate message boxes needed the same six lines of GL state
	around a RenderColor call.
*/
void IGSFillRect(int x, int y, int w, int h, float r, float g, float b, float a);

/*
	True when the shop is drawing its modern skin.

	Reads the same client-side CustomCashShop flag the shop itself does, so a
	dialog never disagrees with the window that opened it.
*/
bool IGSIsModernSkin();

/*
	A whole dialog panel in the modern skin: flat ground, 1px border, red title
	bar.

	Six message boxes draw the same thing at different sizes, four of them out of
	a three-slice top/middle/bottom frame whose real height depends on how many
	middle rows the text needed - so callers pass the height they actually drew
	rather than the enum's nominal one.
*/
void IGSRenderModernPanel(int x, int y, int w, int h);

/*
	Which texture a dialog button should draw with.

	Returns one of the shop's own modern button textures in the modern skin, and
	iLegacyImage otherwise. Deliberately NOT a swap of what gets loaded into
	BITMAP_IGS_MSGBOX_BUTTON: the MU Helper window shares that exact slot
	(NewUIMuHelper.h:38), loads its own art into it and deletes it on close, so
	whatever the shop puts there the helper draws too.

	Safe to call any time - CNewUIInGameShop loads these at Create and holds them
	until Release, which is the whole session.
*/
int IGSDialogButtonImage(bool bPrimary, int iLegacyImage);

class CMsgBoxIGSCommonLayout : public TMsgBoxLayout<CMsgBoxIGSCommon>
{
public:
	bool SetLayout();
};

#endif // KJH_ADD_INGAMESHOP_UI_SYSTEM
#endif // !defined(AFX_MSGBOXIGSCOMMON_H__5E2E9B11_C6F5_411E_BC68_1CF1D1DAE3C0__INCLUDED_)
