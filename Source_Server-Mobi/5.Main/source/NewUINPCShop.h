// NewUINPCShop.h: interface for the CNewUINPCShop class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NEWUINPCSHOP_H__EEE639A8_C89E_47B3_8DBA_22560F102D98__INCLUDED_)
#define AFX_NEWUINPCSHOP_H__EEE639A8_C89E_47B3_8DBA_22560F102D98__INCLUDED_

#pragma once

#include "NewUIBase.h"
#include "NewUIInventoryCtrl.h"
#include "NewUIMessageBox.h"
#include "NewUIMyInventory.h"
#include "NewUIButton.h"

namespace SEASON3B
{
	class CNewUINPCShop : public CNewUIObj
	{
	public:
		enum IMAGE_LIST
		{
			IMAGE_NPCSHOP_BACK = CNewUIMessageBoxMng::IMAGE_MSGBOX_BACK,	// Reference
			IMAGE_NPCSHOP_TOP = CNewUIMyInventory::IMAGE_INVENTORY_BACK_TOP2,
			IMAGE_NPCSHOP_LEFT = CNewUIMyInventory::IMAGE_INVENTORY_BACK_LEFT,
			IMAGE_NPCSHOP_RIGHT= CNewUIMyInventory::IMAGE_INVENTORY_BACK_RIGHT,
			IMAGE_NPCSHOP_BOTTOM= CNewUIMyInventory::IMAGE_INVENTORY_BACK_BOTTOM,
			IMAGE_NPCSHOP_BTN_REPAIR = CNewUIMyInventory::IMAGE_INVENTORY_REPAIR_BTN,
			IMAGE_NPCSHOP_REPAIR_MONEY = BITMAP_INTERFACE_NEW_NPCSHOP_BEGIN,
		};
		
		enum
		{
			NPCSHOP_POS_X = 260,
			NPCSHOP_POS_Y = 0,
			SHOP_STATE_BUYNSELL = 1,
			SHOP_STATE_REPAIR = 2,
		};
		
	private:
		enum
		{
			NPCSHOP_WIDTH = 190,
			NPCSHOP_HEIGHT = 429,
		};

		CNewUIManager*			m_pNewUIMng;
		CNewUIInventoryCtrl*	m_pNewInventoryCtrl;
		POINT m_Pos;

		DWORD m_dwShopState;
		int m_iTaxRate;
		bool m_bRepairShop;
		bool m_bIsNPCShopOpen;

		CNewUIButton m_BtnRepair;
		CNewUIButton m_BtnRepairAll;
		
		DWORD m_dwStandbyItemKey;

		bool m_bSellingItem;

		// Shop pages (server: CustomShopPage in CustomConfig.ini). m_iPageCount stays 0
		// for an ordinary one-screen shop, which is what keeps every other shop looking
		// and behaving exactly as before - no arrows, no label, no lock.
		int m_iPage;
		int m_iPageCount;

		// A page was asked for and the server has not answered yet. While set, buying is
		// locked: the buy packet carries only a slot number, and the SERVER decides which
		// page that slot is on. A click in the gap between asking and the new list arriving
		// would buy from the new page while the player is still looking at the old one.
		bool m_bPagePending;
		DWORD m_dwPageRequestMs;

	public:
		CNewUINPCShop();
		virtual ~CNewUINPCShop();

		bool Create(CNewUIManager* pNewUIMng, int x, int y);
		void Release();

		void SetPos(int x, int y);

		bool UpdateMouseEvent();
		bool UpdateKeyEvent();
		bool Update();
		bool Render();

		float GetLayerDepth();	//. 2.5f

		void SetTaxRate(int iTaxRate);
		int GetTaxRate();

		bool InsertItem(int iIndex, BYTE* pbyItemPacket);

		// C1:D3:B7 - the item list that follows is this page of that many. Empties the
		// grid; the ordinary C2:31 list that comes next refills it.
		void RecvPageInfo(BYTE* lpMsg);

		void OpenningProcess();

		void ClosingProcess();
		void SetRepairShop(bool bRepair);
		bool IsRepairShop();
		void ToggleState();
		DWORD GetShopState();
		
		int GetPointedItemIndex();

		//. Exporting Functions
		void SetStandbyItemKey(DWORD dwItemKey);
		DWORD GetStandbyItemKey() const;
		int GetStandbyItemIndex();
		ITEM* GetStandbyItem();

		void SetSellingItem(bool bFlag);
		bool IsSellingItem();

		CNewUIInventoryCtrl* GetInventoryCtrl() const { return m_pNewInventoryCtrl; }

	private:
		void Init();
		void SetButtonInfo();
		
		void LoadImages();
		void UnloadImages();

		bool InventoryProcess();
		bool BtnProcess();

		// Page arrows (in the header, so they never collide with the repair row or the
		// repair-money bar at the bottom) and the "Page 2 / 5" label above the grid.
		bool PageButtonProcess();
		void RequestPage(int iPage);
		void RenderPageBar();
		bool PageButtonRect(bool bNext, int* px, int* py, int* pw, int* ph);

		void RenderFrame();
		void RenderTexts();
		void RenderButton();
		void RenderRepairMoney();
	};
}

#endif // !defined(AFX_NEWUINPCSHOP_H__EEE639A8_C89E_47B3_8DBA_22560F102D98__INCLUDED_)
