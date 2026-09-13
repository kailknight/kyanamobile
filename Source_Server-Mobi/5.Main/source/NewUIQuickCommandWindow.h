// NewUIQuickCommandWindow.h: interface for the CNewUIQuickCommandWindow class.
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NEWUIQUICKCOMMANDWINDOW_H__3A1D6614_8C41_4066_A831_2954B3C461D5__INCLUDED_)
#define AFX_NEWUIQUICKCOMMANDWINDOW_H__3A1D6614_8C41_4066_A831_2954B3C461D5__INCLUDED_

#pragma once

#include "NewUIManager.h"
#include "NewUIWindowMenu.h"
#include "ZzzBMD.h"
#include "ZzzCharacter.h"

namespace SEASON3B
{
	
	class CNewUIQuickCommandWindow  : public CNewUIObj
	{
	public:
		// The popup's entries, in the order they are drawn. Rows can now be
		// hidden individually (MAIN_FILE_INFO::HidePlayerMenu), so the row a
		// click lands on is NOT the action - m_aVisibleEntry maps one to the
		// other. Anything indexing rows directly is a bug.
		enum QUICK_COMMAND_ENTRY
		{
			QCE_TRADE = 0,
			QCE_BUY,
			QCE_PARTY,
			QCE_FOLLOW,
			QCE_DUEL,
			QCE_VIEWITEM,
			QCE_COUNT
		};

	private:
		enum IMAGE_LIST
		{
			IMAGE_QUICKCOMMAND_BACK = CNewUIMessageBoxMng::IMAGE_MSGBOX_BACK,
			IMAGE_QUICKCOMMAND_FRAME_MIDDLE = CNewUIWindowMenu::IMAGE_WINDOW_MENU_FRAME_MIDDLE,
			IMAGE_QUICKCOMMAND_FRAME_DOWN = CNewUIWindowMenu::IMAGE_WINDOW_MENU_FRAME_DOWN,
			IMAGE_QUICKCOMMAND_LINE = CNewUIWindowMenu::IMAGE_WINDOW_MENU_LINE,
			IMAGE_QUICKCOMMAND_ARROWL = CNewUIWindowMenu::IMAGE_WINDOW_MENU_ARROWL,
			IMAGE_QUICKCOMMAND_ARROWR = CNewUIWindowMenu::IMAGE_WINDOW_MENU_ARROWR,
			IMAGE_QUICKCOMMAND_FRAME_UP = BITMAP_QUICKCOMMAND_BEGIN,
		};

	public:
		CNewUIQuickCommandWindow();
		virtual ~CNewUIQuickCommandWindow();
		
		bool Create(CNewUIManager* pNewUIMng, int x, int y);
		void Release();
		
		void SetPos(int x, int y);
		
		bool UpdateMouseEvent();
		bool UpdateKeyEvent();
		bool Update();
		bool Render();

		float GetLayerDepth();	//. 2.0f
		float GetKeyEventOrder();	// 10.f;
		
		void OpenningProcess();
		void ClosingProcess();
		void OpenQuickCommand(const char* strID, int iIndex, int x, int y);
		void CloseQuickCommand();
		void SetID(const char* strID);
		void SetSelectedCharacterIndex(int iIndex);

	private:
		// Fills m_aVisibleEntry / m_nVisibleCount from HidePlayerMenu and returns
		// how many rows survived. Rebuilt on every open so editing MainInfo.ini
		// (and regenerating CBGetMain.bin) does not need a client restart.
		int BuildVisibleEntries();

		// The popup used to be hardcoded to six rows in five separate places -
		// the back plate, the filler art, the separator lines and both of
		// UpdateMouseEvent's outside-click tests. These are the one authority now,
		// because a hit test that disagrees with the art means clicks landing on
		// nothing (or the menu refusing to close below its own bottom edge).
		int GetVisibleRowCount() const;
		float GetMenuFillerHeight() const;

		void LoadImages();
		void UnloadImages();
		
		void RenderFrame();
		void RenderContents();
		void RenderArrow();

	private:
		CNewUIManager*	m_pNewUIMng;
		POINT			m_Pos;

		// Index into m_aVisibleEntry (a ROW), not a QUICK_COMMAND_ENTRY.
		int m_iSelectedIndex;
		char m_strID[32];
		int m_iSelectedCharacterIndex;

		// Row -> entry map, and how many of it is live.
		int m_aVisibleEntry[QCE_COUNT];
		int m_nVisibleCount;
	};
	
}

#endif // !defined(AFX_NEWUIQUICKCOMMANDWINDOW_H__3A1D6614_8C41_4066_A831_2954B3C461D5__INCLUDED_)
