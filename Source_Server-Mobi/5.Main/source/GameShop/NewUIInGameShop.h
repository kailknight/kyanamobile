// NewUIInGameShop.h: interface for the NewUIInGameShop class.
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NEWUIINGAMESHOP_H__AE3CE531_70BE_4CBB_9938_0D80B26F21A8__INCLUDED_)
#define AFX_NEWUIINGAMESHOP_H__AE3CE531_70BE_4CBB_9938_0D80B26F21A8__INCLUDED_

#pragma once

#ifdef PBG_ADD_INGAMESHOP_UI_ITEMSHOP

#include "NewUIBase.h"
#include "NewUIManager.h"
#include "NewUIMessageBox.h"
#include "NewUIMyInventory.h"
#include "NewUICommonMessageBox.h"
#include "ZzzInventory.h"
#include "Sprite.h"
#include "InGameShopSystem.h"


namespace SEASON3B
{
	class CNewUIInGameShop : public CNewUIObj 
	{
	public:
		enum LISTBOX_INDEX
		{
			IGS_SAFEKEEPING_LISTBOX = 0,
			IGS_PRESENTBOX_LISTBOX,
			IGS_TOTAL_LISTBOX,
		};

		enum IMAGE_LIST
		{
			IMAGE_IGS_EXIT_BTN	= CNewUIMyInventory::IMAGE_INVENTORY_EXIT_BTN,	// newui_exit_00.tga (36, 58) - 2BtState
			IMAGE_IGS_BACK		= BITMAP_INGAMESHOP_FRAME,	// Ingame_shopback.jpg (640, 429)
			IMAGE_IGS_CATEGORY_BTN,				// Ingame_Bt01.tga (73, 81) - 3BtState
			IMAGE_IGS_CATEGORY_DECO_MIDDLE,		// Ingame_Deco_Center.tga (6, 8)
			IMAGE_IGS_CATEGORY_DECO_DOWN,		// Ingame_Deco_Dn.tga (47, 100)
			IMAGE_IGS_LEFT_TAB,					// Ingame_Tab01.tga (49, 21)
			IMAGE_IGS_RIGHT_TAB,				// Ingame_Tab02.tga (49, 21)
			IMAGE_IGS_ZONE_BTN,					// Ingame_Tab_Up.tga (76, 46) - 2BtState
			IMAGE_IGS_ITEMGIFT_BTN,				// Ingame_Bt_Gift.tga (25, 75) - 3BtState
			IMAGE_IGS_CASHGIFT_BTN,				// Ingame_Bt_Cash.tga (25, 75) - 3BtState 
			IMAGE_IGS_REFRESH_BTN,				// Ingame_Bt_Reset.tga (25, 75) - 3BtState
			IMAGE_IGS_VIEWDETAIL_BTN,			// Ingame_Bt_Bt03.tga (52, 78) - 3BtState
			IMAGE_IGS_ITEMBOX_LOGO,				// Ingame_Itembox_logo.tga (57, 57)
			IMAGE_IGS_PAGE_LEFT,				// Ingame_Bt_page_L.tga (20, 69) - 3BtState
			IMAGE_IGS_PAGE_RIGHT,				// Ingame_Bt_page_R.tga (20, 69) - 3BtState
			IMAGE_IGS_STORAGE_PAGE,				// IGS_Storage_Page.tga (80, 30)
			IMAGE_IGS_STORAGE_PAGE_LEFT,		// IGS_Storage_Page_Left.tga (20, 22) - 3BtState
			IMAGE_IGS_STORAGE_PAGE_RIGHT,		// IGS_Storage_Page_Right.tga (20, 22) - 3BtState

			// Modern flat-dark skin (CustomCashShop != 0 only - see LoadImages()).
			// Only loaded in custom mode, so legacy-mode players pay zero extra
			// load/VRAM cost. Full window backdrop is a plain RenderColor fill,
			// not a texture - the mockup's background has no visual detail to
			// justify one.
			IMAGE_IGS_MODERN_CATEGORY_BTN,		// Modern_Sidebar.tga (73, 81) - 3BtState
			IMAGE_IGS_MODERN_VIEWDETAIL_BTN,	// Modern_BuyBtn.tga (52, 78) - 3BtState
			IMAGE_IGS_MODERN_CARD_PANEL,		// Modern_CardPanel.tga (116, 112)
			IMAGE_IGS_MODERN_BANNER_BG,			// Modern_BannerBG.tga (159, 69)
			IMAGE_IGS_MODERN_TAB_LEFT,			// Modern_TabLeft.tga (49, 21) - single frame
			IMAGE_IGS_MODERN_TAB_RIGHT,			// Modern_TabRight.tga (49, 21) - single frame

			/*
				The flat replacements for the last of the ornate widget art.

				Frame counts and per-frame sizes match the legacy art each one
				stands in for, because the button widgets slice a texture by its
				own height - a 3-state file where the original had 2 would show
				the wrong third of the image.

				These live in the shop's own list because the shop loads them
				once at CNewUIInGameShop::Create and holds them until Release,
				which is the whole session - so the confirm dialogs can draw with
				them without owning any texture lifetime of their own.
			*/
			IMAGE_IGS_MODERN_BTN,				// Modern_Btn.tga (64, 72) - 3BtState
			IMAGE_IGS_MODERN_BTN_RED,			// Modern_BtnRed.tga (64, 72) - 3BtState
			IMAGE_IGS_MODERN_ZONE_BTN,			// Modern_ZoneBtn.tga (76, 46) - 2BtState
			IMAGE_IGS_MODERN_CLOSE_BTN,			// Modern_Close.tga (36, 58) - 2BtState
			IMAGE_IGS_MODERN_ARROW_LEFT,		// Modern_ArrowL.tga (20, 69) - 3BtState
			IMAGE_IGS_MODERN_ARROW_RIGHT,		// Modern_ArrowR.tga (20, 69) - 3BtState
			IMAGE_IGS_MODERN_ICON_GIFT,			// Modern_IconGift.tga (25, 75) - 3BtState
			IMAGE_IGS_MODERN_ICON_CASH,			// Modern_IconCash.tga (25, 75) - 3BtState
			IMAGE_IGS_MODERN_ICON_REFRESH,		// Modern_IconRefresh.tga (25, 75) - 3BtState

			IMAGE_IGS_BANNER	= BITMAP_INGAMESHOP_BANNER
		};
		
	private:
		enum INGAMESHOP_TEXT_INFO
		{
			TEXT_IGS_CHAR_NAME_POS_X	= 498,
			TEXT_IGS_CHAR_NAME_POS_Y	= 23,
			TEXT_IGS_CHAR_NAME_WIDTH	= 122,
			TEXT_IGS_CASH_POS_X		= 498,
			TEXT_IGS_CASH_POS_Y		= 50,
			TEXT_IGS_CASH_WIDTH		= 130,
			TEXT_IGS_MILEAGE_POS_Y	= 65,
			TEXT_IGS_POINT_POS_Y	= 80,
			TEXT_IGS_STORAGE_NAME_POS_X	= 492,
			TEXT_IGS_STORAGE_NAME_POS_Y = 233,
			TEXT_IGS_STORAGE_NAME_WIDTH	= 96,
			TEXT_IGS_STORAGE_TIME_POS_X	= 592,
			TEXT_IGS_STORAGE_TIME_WIDTH = 34,
			TEXT_IGS_PAGE_POS_X		= 251,
			TEXT_IGS_PAGE_POS_Y		= 404,
			TEXT_IGS_STORAGE_PAGE_INFO_POS_X	= 518,
			TEXT_IGS_STORAGE_PAGE_INFO_POS_Y	= 376,
		};
		
		enum INGAMESHOP_IMAGES_POS
		{
			IMAGE_IGS_EXIT_BTN_POS_X	= 484,			// Exit Button
			IMAGE_IGS_EXIT_BTN_POS_Y	= 392,
			IMAGE_IGS_BACK_POS_X		= 0,			// InGameShop Back
			IMAGE_IGS_BACK_POS_Y		= 0,
			IMAGE_IGS_CATEGORY_BTN_POS_X	= 13,		// Category Button
			IMAGE_IGS_CATEGORY_BTN_POS_Y	= 31,
			IMAGE_IGS_CATEGORY_BTN_DISTANCE	= 6,
			IMAGE_IGS_TAB_BTN_POS_X			= 486,		// Tab Button
			IMAGE_IGS_TAB_BTN_POS_Y			= 208,
			IMAGE_IGS_TAB_BTN_DISTANCE		= -2,
			IMAGE_IGS_ZONE_BTN_POS_X		= 95,		// Zone Button
			IMAGE_IGS_ZONE_BTN_POS_Y		= 0,
			IMAGE_IGS_VIEWDETAIL_BTN_POS_X	= 162,		// View Detail Button
			IMAGE_IGS_VIEWDETAIL_BTN_POS_Y	= 126,
			IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_X	= 122,
			IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_Y	= 121,
			IMAGE_IGS_ITEMGIFT_BTN_POS_X	= 519,		// Item Gift Button
			IMAGE_IGS_CASHGIFT_BTN_POS_X	= 546,		// Cash Gift Button
			IMAGE_IGS_REFRESH_BTN_POS_X		= 573,		// Refresh Button
			IMAGE_IGS_ICON_BTN_POS_Y		= 94,
			IMAGE_IGS_USE_BTN_POS_X			= 572,
			IMAGE_IGS_USE_BTN_POS_Y			= 396,
			IMAGE_IGS_ITEMBOX_LOGO_POS_X	= 128,
			IMAGE_IGS_ITEMBOX_LOGO_POS_Y	= 52,
			IMAGE_IGS_PAGE_LEFT_POS_X		= 231,		// Page Left Button
			IMAGE_IGS_PAGE_RIGHT_POS_X		= 307,		// Page Right Button
			IMAGE_IGS_PAGE_BUTTON_POS_Y		= 397,
 			IMAGE_IGS_BANNER_POS_X			= 482,		// Banner
 			IMAGE_IGS_BANNER_POS_Y			= 133,
			IMAGE_IGS_STORAGE_PAGE_POS_X		= 518,		// Storage Page
			IMAGE_IGS_STORAGE_PAGE_POS_Y		= 366,
			IMAGE_IGS_STORAGE_PAGE_LEFT_POS_X	= 512,		// Storage Page Left
			IMAGE_IGS_STORAGE_PAGE_RIGHT_POS_X	= 586,		// Storage Page Right
			IMAGE_IGS_STORAGE_PAGE_BTN_POS_Y	= 372,
		};
		
		enum INGAMESHOP_IMAGES_SIZE
		{
			IMAGE_IGS_EXIT_BTN_WIDTH		= 36,		// Exit Button
			IMAGE_IGS_EXIT_BTN_HEIGHT		= 29,
			IMAGE_IGS_BACK_WIDTH			= 640,		// InGameShop Back
			IMAGE_IGS_BACK_HEIGHT			= 429,
			IMAGE_IGS_CATEGORY_BTN_WIDTH	= 73,		// Category Button
			IMAGE_IGS_CATEGORY_BTN_HEIGHT	= 27,
			IMAGE_IGS_CATEGORY_DECO_MIDDLE_WIDTH	= 4,	// Category Deco Middle
			IMAGE_IGS_CATEGORY_DECO_MIDDLE_HEIGHT	= 8,
			IMAGE_IGS_CATEGORY_DECO_DOWN_WIDTH		= 47,	// Category Deco Down
			IMAGE_IGS_CATEGORY_DECO_DOWN_HEIGHT		= 100,
			IMAGE_IGS_TAB_BTN_WIDTH			= 49,		// Tab Button
			IMAGE_IGS_TAB_BTN_HEIGHT		= 20,
			IMAGE_IGS_ZONE_BTN_WIDTH		= 76,		// Zone Button
			IMAGE_IGS_ZONE_BTN_HEIGHT		= 23, 
			IMAGE_IGS_VIEWDETAIL_BTN_WIDTH	= 52,		// View Detail Button
			IMAGE_IGS_VIEWDETAIL_BTN_HEIGHT	= 26,
			IMAGE_IGS_ICON_BTN_WIDTH		= 25,
			IMAGE_IGS_ICON_BTN_HEIGHT		= 25,
			IMAGE_IGS_ITEMBOX_LOGO_SIZE		= 57,
			IMAGE_IGS_PAGE_BTN_WIDTH		= 20,		// Page
			IMAGE_IGS_PAGE_BTN_HEIGHT		= 23,
			IMAGE_IGS_BANNER_WIDTH			= 153,		// Banner
			IMAGE_IGS_BANNER_HEIGHT			= 63,
			IMGAE_IGS_STORAGE_PAGE_WIDTH	= 80,		// Storage Page
			IMGAE_IGS_STORAGE_PAGE_HEIGHT	= 30,
			IMGAE_IGS_STORAGE_PAGE_BTN_WIDTH	= 20,	// Storage Page Btn
			IMGAE_IGS_STORAGE_PAGE_BTN_HEIGHT	= 22,
		};
		
		enum INGAMESHOP_DISPLAY_ITEMS
		{
			IGS_WIDTH_POS_X		= 129,
			IGS_HEIGHT_POS_Y	= 59,		
			IGS_SIZE_WIDTH		= 60,
			IGS_SIZE_HEIGHT		= 49,		
			IGS_NUM_ITEMS_WIDTH		= 3,
			IGS_NUM_ITEMS_HEIGHT	= 3,	
			IGS_PACKAGE_NAME_POS_X	= 105,
			IGS_PACKAGE_NAME_POS_Y	= 40,
			IGS_PACKAGE_NAME_WIDTH	= 104,
			IGS_PACKAGE_PRICE_POS_Y	= 60,
			IGS_ITEMRENDER_POS_X_STANDAD	= 102,
			IGS_ITEMRENDER_POS_WIDTH		= 108,
			IGS_ITEMRENDER_POS_Y_STANDAD	= 51,
			IGS_ITEMRENDER_POS_HEIGHT		= 58,
			IGS_STORAGE_TOTAL_ITEM_PER_PAGE	= 9,
		};
		
		/*
			Modern-skin layout - the mockup's proportions expressed in the shop's
			own 640x429 virtual space.

			The 3x3 grid is not negotiable. INGAMESHOP_DISPLAY_ITEMLIST_SIZE is the
			wire length of PMSG_CASHSHOP_EVENTITEM_LIST (WSclient.h:3630), so a page
			holds exactly 9 packages no matter what the skin does. The mockup's card
			proportions assume a single visible row; three rows of those plus a
			header, a heading and a full-width banner do not fit inside 429px, so
			these cards keep the mockup's internal order and styling at a compressed
			height rather than its exact pixel ratios.
		*/
		enum INGAMESHOP_MODERN_LAYOUT
		{
			MODERN_HEADER_HEIGHT	= 36,		// Top bar - shop title + currency strip
			MODERN_CATEGORY_POS_Y	= 40,		// Sidebar starts below the header, not at the legacy 31
			MODERN_SIDEBAR_WIDTH	= 90,
			MODERN_TITLE_POS_X		= 14,
			MODERN_TITLE_POS_Y		= 9,
			MODERN_CUR_COL_WIDTH	= 108,		// Three currency columns, right aligned
			MODERN_CUR_FIRST_POS_X	= 302,		// 640 - 14 - (3 * MODERN_CUR_COL_WIDTH)
			/*
				Label over value, 15px apart inside a 36px strip.

				The first cut used a 30px strip with 3/15, and the label's descenders
				ran into the value - the two rows need a full line box each, and the
				in-game font is taller than the 12px the arithmetic assumed. Sizing
				the strip to the content rather than shaving the gap is what fixed it.
			*/
			MODERN_CUR_LABEL_POS_Y	= 4,
			MODERN_CUR_VALUE_POS_Y	= 19,

			MODERN_CONTENT_POS_X	= 98,		// Flush with the first card column
			MODERN_CONTENT_WIDTH	= 360,		// Exactly 3 cards at MODERN_CARD_DISTANCE_X
			MODERN_HEADING_POS_Y	= 42,		// Category name, item count, paging
			MODERN_DIVIDER_POS_Y	= 65,

			/*
				Paging moves up into the heading row in this skin. The legacy
				buttons sit at y=397, which is exactly where the third row of
				mockup-proportioned cards has to reach - and a shelf that ends
				above its own pager is the reason the cards had no room. The
				mockup shows no pager at all (three items, one page), so the
				heading row is the closest honest home for it.
			*/
			MODERN_ITEMCOUNT_POS_X	= 260,		// Right aligned, ends at 378
			MODERN_ITEMCOUNT_WIDTH	= 118,
			MODERN_PAGE_BTN_POS_Y	= 42,
			MODERN_PAGE_BTN_WIDTH	= 18,		// Modern_ArrowL/R frame size
			MODERN_PAGE_BTN_HEIGHT	= 18,
			MODERN_PAGE_PREV_POS_X	= 384,
			MODERN_PAGE_NEXT_POS_X	= 432,
			MODERN_PAGE_TEXT_POS_X	= 406,
			MODERN_PAGE_TEXT_WIDTH	= 24,

			MODERN_BANNER_POS_X		= 98,		// Full content width, above the shelf
			MODERN_BANNER_POS_Y		= 70,
			MODERN_BANNER_WIDTH		= 360,
			MODERN_BANNER_HEIGHT	= 60,

			MODERN_CARD_POS_X		= 98,
			MODERN_CARD_POS_Y		= 136,
			MODERN_CARD_WIDTH		= 116,
			MODERN_CARD_HEIGHT		= 94,
			MODERN_CARD_DISTANCE_X	= 122,
			MODERN_CARD_DISTANCE_Y	= 97,		// Row 3 ends at 424, inside the 429 frame

			MODERN_CARD_IMG_INSET	= 3,		// Icon well inside the card
			MODERN_CARD_IMG_HEIGHT	= 40,
			MODERN_CARD_NAME_POS_Y	= 46,		// Name under the icon, per the mockup

			/*
				One price row, discounted or not.

				The taller banner left the card 10px shorter, and two stacked
				price lines no longer fit above the Buy bar. A discount now reads
				as the badge and the struck list price on the icon well, with the
				charged price alone in the chip - which also means every card has
				the same shape whether or not it is on sale.
			*/
			MODERN_CARD_PILL_POS_Y	= 60,
			MODERN_CARD_PILL_HEIGHT	= 14,
			MODERN_CARD_BUY_POS_Y	= 77,
			MODERN_CARD_BUY_HEIGHT	= 14,

			/*
				The bottom-right cluster - storage pager, close and Use.

				The legacy sizes were drawn for ornate art with its own carved
				border; as flat plates at the same dimensions they read as far
				too heavy against the rest of the skin.
			*/
			MODERN_STORAGE_PAGE_POS_X	= 524,
			MODERN_STORAGE_PAGE_POS_Y	= 372,
			MODERN_STORAGE_PAGE_WIDTH	= 58,
			MODERN_STORAGE_PAGE_HEIGHT	= 18,
			/*
				Arrows and the close button are drawn at exactly their texture's
				own frame size.

				Everything else in this skin is a flat plate, and scaling a plate
				is invisible. These two carry a glyph: drawn smaller than the
				cell it was painted into, the chevron ends up small and off
				centre inside its own plate. Modern_ArrowL/R are 18x18 per frame
				and Modern_Close is 24x20, so these constants must match.
			*/
			MODERN_STORAGE_ARROW_WIDTH	= 18,
			MODERN_STORAGE_ARROW_HEIGHT	= 18,
			MODERN_STORAGE_PREV_POS_X	= 502,
			MODERN_STORAGE_NEXT_POS_X	= 586,
			MODERN_STORAGE_ARROW_POS_Y	= 372,
			MODERN_CLOSE_POS_X			= 506,
			MODERN_CLOSE_POS_Y			= 400,
			MODERN_CLOSE_WIDTH			= 24,		// Modern_Close frame size
			MODERN_CLOSE_HEIGHT			= 20,
			MODERN_USE_POS_X			= 578,
			MODERN_USE_POS_Y			= 401,
			MODERN_USE_WIDTH			= 44,
			MODERN_USE_HEIGHT			= 18,

			// Right panel - pushed down clear of the header strip.
			MODERN_CHAR_LABEL_POS_Y	= 44,
			MODERN_CHAR_NAME_POS_Y	= 57,
			MODERN_PANEL_CASH_POS_Y	= 86,
			MODERN_PANEL_ROW_HEIGHT	= 18,
			MODERN_ICON_BTN_POS_Y	= 146,
		};
		
	public:
		CNewUIInGameShop();
		virtual ~CNewUIInGameShop();
		
		bool Create(CNewUIManager* pNewUIMng, int x, int y);
		
		void SetPos(int x, int y);
		const POINT& GetPos() { return m_Pos; }
		
		bool Render();
		bool Update();
		bool UpdateMouseEvent();
		bool UpdateKeyEvent();
		bool BtnProcess();
		void SetBtnInfo();
		float GetLayerDepth() { return 10.08f; }
		
		bool GetItemRotation() { return m_ItemAngle; }
		void SetItemRotation(bool _bInput) { m_ItemAngle = _bInput; }
		
		void OpeningProcess();
		void ClosingProcess(); 
		
		void Release();
		
		bool IsInGameShopOpen();
		bool IsInGameShop();
		
		void InitZoneBtn();
		void InitCategoryBtn();
		
		void AddStorageItem(int iStorageSeq, int iStorageItemSeq, int iStorageGroupCode, int iProductSeq, int iPriceSeq,int iCashPoint, unicode::t_char chItemType, unicode::t_char* pszUserName = NULL, unicode::t_char* pszMessage = NULL);

		void ClearAllStorageItem();

		void InitStorage(int iTotalItemCnt, int iCurrentPageItemCnt, int iTotalPage, int iCurrentPage );
		char GetCurrentStorageCode();

		void StoragePrevPage();
		void StorageNextPage();
		void UpdateStorageItemList();
		
		void InitBanner(unicode::t_char* pszFileName, unicode::t_char* pszBannerURL);
		void ReleaseBanner();
		
		void SetRateScale(int _ItemType);
		float GetRateScale() { return m_fRate_Scale; }
		void SetConvertInvenCoord(WORD _ItemType, float _Width, float _Height);
		POINT GetConvertPos()	{ return m_fRePos; }
		POINT GetConvertSize()	{ return m_fReSize; }
		bool IsInGameShopRect(float _x,float _y);

	private:
		float m_fRate_Scale;
		POINT m_fRePos;
		POINT m_fReSize;
		
	private:
		void Init();
		void LoadImages();
		void UnloadImages();
		void RenderFrame();
		void RenderTexts();
		void RenderButtons();
		void RenderListBox();
		void RenderDisplayItems();

		// One package cell drawn the custom way: the list price struck through, the
		// price actually charged under it, and a percent badge on the item box.
		// Only reached when MainInfo CustomCashShop is on AND the server sent a
		// discount for this package - everything else keeps the original line.
		void RenderPackageSalePrice(int iCellX, int iNameY, int iPriceY,
			int iListPrice, int iEffectivePrice, int iDiscountPercent, const char* pszUnitName);

		void RenderBanner();
		
		// Modern skin only. Kept apart from RenderFrame/RenderTexts so the legacy
		// path stays byte-for-byte the code it always was.
		void RenderModernFrame();
		void RenderModernTexts();
		void RenderModernCardText(int iIndex, int iCardX, int iCardY);
		void RenderModernSaleMarks(int iCardX, int iCardY, int iListPrice, int iDiscountPercent, const char* pszUnitName);
		void RenderFillRect(int x, int y, int w, int h, float r, float g, float b, float a);
		bool IsModernSkin();

		// Window dragging. Only the header strip is a grab handle, and only in
		// the modern skin - the legacy frame has no bar that reads as one.
		bool UpdateWindowDrag();
		void ApplyWindowPos();
		void GetModernBannerRect(int& x, int& y, int& w, int& h);

		// Jump the shelf to whatever the banner advertises.
		bool JumpToPackage(int iPackageSeq);

	public:
		void SetBannerTargetPackage(int iPackageSeq) { m_iBannerTargetPackage = iPackageSeq; }

	private:
		bool UpdateBanner();
		
	private:
		CNewUIManager* m_pNewUIMng;
		POINT m_Pos;
		bool m_ItemAngle;
		
		CNewUIRadioGroupButton	m_ZoneButton;
		CNewUIRadioGroupButton	m_CategoryButton;
		CNewUIRadioGroupButton	m_ListBoxTabButton;
		CNewUIButton	m_ViewDetailButton[INGAMESHOP_DISPLAY_ITEMLIST_SIZE];
		CNewUIButton	m_CashGiftButton;
		CNewUIButton	m_CashChargeButton;
		CNewUIButton	m_CashRefreshButton;
		CNewUIButton	m_UseButton;
		CNewUIButton	m_PrevButton;
		CNewUIButton	m_NextButton;
		CNewUIButton	m_CloseButton;
		CNewUIButton	m_StoragePrevButton;
		CNewUIButton	m_StorageNextButton;
		
		bool m_bDraggingWindow;
		POINT m_ptDragGrab;		// Cursor offset inside the window when the drag started

		bool m_bLoadBanner;
		bool m_bBannerLink;

		// The package a banner click jumps to, as a PackageProductSeq. 0 = the
		// banner is decoration. Translated from PackageId by the DataServer,
		// which is the only side that can do that lookup.
		int m_iBannerTargetPackage;
		unicode::t_char m_szBannerURL[INTERNET_MAX_URL_LENGTH];

		int		m_iStorageTotalItemCnt;
		int		m_iStorageCurrentPageItemCnt;
		int		m_iStorageTotalPage;
		int		m_iStorageCurrentPage;
		int		m_iSelectedStorageItemIndex;

		int		m_iStorageCurrentPageReceiveItemCnt;
		bool	m_bRequestCurrentPage;

		CUIInGameShopListBox	m_StorageItemListBox;
	};
}
	
#elif defined(__ANDROID__)

namespace SEASON3B
{
	class CNewUIInGameShop
	{
	public:
		enum IMAGE_LIST
		{
			IMAGE_IGS_CATEGORY_BTN = BITMAP_ANDROID_IGS_CATEGORY_BTN,
			IMAGE_IGS_PAGE_LEFT = BITMAP_ANDROID_IGS_PAGE_LEFT,
			IMAGE_IGS_PAGE_RIGHT = BITMAP_ANDROID_IGS_PAGE_RIGHT,
			IMAGE_IGS_STORAGE_PAGE = BITMAP_ANDROID_IGS_STORAGE_PAGE,
			IMAGE_IGS_STORAGE_PAGE_LEFT = BITMAP_ANDROID_IGS_STORAGE_PAGE_LEFT,
			IMAGE_IGS_STORAGE_PAGE_RIGHT = BITMAP_ANDROID_IGS_STORAGE_PAGE_RIGHT,
		};
	};
}

#endif //PBG_ADD_INGAMESHOP_UI_ITEMSHOP
#endif // !defined(AFX_NEWUIINGAMESHOP_H__AE3CE531_70BE_4CBB_9938_0D80B26F21A8__INCLUDED_)
