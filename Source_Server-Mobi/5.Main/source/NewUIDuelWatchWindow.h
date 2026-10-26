// NewUIDuelWatchWindow.h: interface for the CNewUIDuelWatchWindow class.
//
// Doorkeeper Titus. Two tabs: RANKED (the ranked matchmaking hub: profile card, Find Match,
// the live ranked fights with a Watch button) and NORMAL (the four local duel channels, as the
// window always was). Drawn entirely in code. See CB_DuelRanked.h and
// Working Project\MD Files\RankedDuel-PLAN.md section 7.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "NewUIBase.h"
#include "NewUIButton.h"
#include "NewUIMessageBox.h"
#include "NewUIMyInventory.h"
#include "NewUIMyQuestInfoWindow.h"

namespace SEASON3B
{
	class CNewUIDuelWatchWindow : public CNewUIObj
	{
	public:
		enum
		{
			WINDOW_WIDTH = 264,
			WINDOW_HEIGHT = 429,	// stays clear of the main frame bar along the bottom
		};

		enum TAB
		{
			TAB_RANKED = 0,
			TAB_NORMAL = 1,
		};

	private:
		CNewUIManager*			m_pNewUIMng;
		POINT m_Pos;

		int   m_Tab;
		DWORD m_dwListTick;		// last AA:13 (ranked fight list) request
		DWORD m_dwStatusTick;	// last AA:10 (status) request while no status has arrived

	public:
		CNewUIDuelWatchWindow();
		virtual ~CNewUIDuelWatchWindow();

		bool Create(CNewUIManager* pNewUIMng, int x, int y);
		void Release();

		void SetPos(int x, int y);

		bool UpdateMouseEvent();
		bool UpdateKeyEvent();
		bool Update();
		bool Render();

		void OpeningProcess();
		void ClosingProcess();

		float GetLayerDepth();	//. 5.0f

	private:
		void DrawFrame();
		void DrawTabs();
		void DrawRankedTab();
		void DrawNormalTab();
	};
}
