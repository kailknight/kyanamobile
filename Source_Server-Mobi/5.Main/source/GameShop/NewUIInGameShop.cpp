// NewUIInGameShop.cpp: implementation of the NewUIInGameShop class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#ifdef PBG_ADD_INGAMESHOP_UI_ITEMSHOP
#include "wsclientinline.h"
#include "iexplorer.h"
#include "NewUISystem.h"
#include "NewUIInGameShop.h"
#include "MsgBoxIGSBuyPackageItem.h"
#include "MsgBoxIGSBuySelectItem.h"
#include "MsgBoxIGSCommon.h"
#include "MsgBoxIGSStorageItemInfo.h"
#include "MsgBoxIGSGiftStorageItemInfo.h"
#include "MsgBoxIGSDeleteItemConfirm.h"
#include "MapManager.h"
extern int DisplayWinCDepthBox;
extern int DisplayWin;
extern int DisplayHeight;
extern int DisplayWinMid;
extern int DisplayHeightExt;
extern int DisplayWinExt;
extern int DisplayWinReal;
extern int MouseX, MouseY;
using namespace SEASON3B;

CNewUIInGameShop::CNewUIInGameShop()
{
	Init();
}

CNewUIInGameShop::~CNewUIInGameShop()
{
	Release();
}

void CNewUIInGameShop::Init()
{
	m_ItemAngle = false;
	m_bDraggingWindow = false;
	m_ptDragGrab.x = 0;
	m_ptDragGrab.y = 0;
	m_bLoadBanner = false;
	m_bBannerLink = false;
	m_iBannerTargetPackage = 0;
	m_iStorageTotalItemCnt			= 0;
	m_iStorageCurrentPageItemCnt	= 0;
	m_iStorageTotalPage				= 0;
	m_iStorageCurrentPage			= 0;
	m_iSelectedStorageItemIndex		= 0;
	m_iStorageCurrentPageReceiveItemCnt = 0;
	m_bRequestCurrentPage			= false;

}

void CNewUIInGameShop::Release()
{
	UnloadImages();

	ReleaseBanner();
	
	if(m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);
		m_pNewUIMng = NULL;
	}

	ClearAllStorageItem();
}

bool CNewUIInGameShop::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if(pNewUIMng  == NULL)
		return false;
	
	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_INGAMESHOP, this);
	
	SetPos(x, y);
	LoadImages();
	SetBtnInfo();
	Show(false);	//visible()À» flase·Î

	return true;
}

void CNewUIInGameShop::SetPos( int x, int y )
{

	m_Pos.x = x; 
	m_Pos.y = y;
}

bool CNewUIInGameShop::Render()
{
	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	RenderFrame();
	RenderButtons();	
	RenderTexts();
	RenderBanner();
	RenderListBox();
	RenderDisplayItems();
	DisableAlphaBlend();
	return true;
}

bool CNewUIInGameShop::IsModernSkin()
{
	return (gProtect.m_MainInfo.CustomCashShop != 0);
}

/*
	One flat filled rectangle.

	The modern skin is mostly flat colour, and every one of those fills was
	being written as the same six lines of GL state around a RenderColor call.
	Wrapping it keeps the layout code readable as layout.
*/
void CNewUIInGameShop::RenderFillRect(int x, int y, int w, int h, float r, float g, float b, float a)
{
	EnableAlphaTest(true);
	glColor4f(r, g, b, a);
	RenderColor((float)x, (float)y, (float)w, (float)h, 0.0f, 0);
	EndRenderColor();
	glColor3f(1.0f, 1.0f, 1.0f);
	EnableAlphaTest(false);
}

void CNewUIInGameShop::GetModernBannerRect(int& x, int& y, int& w, int& h)
{
	x = m_Pos.x + MODERN_BANNER_POS_X;
	y = m_Pos.y + MODERN_BANNER_POS_Y;
	w = MODERN_BANNER_WIDTH;
	h = MODERN_BANNER_HEIGHT;
}

void CNewUIInGameShop::RenderFrame()
{
	if( IsModernSkin() )
	{
		RenderModernFrame();
		return;
	}

	RenderImage(IMAGE_IGS_BACK, m_Pos.x, m_Pos.y, IMAGE_IGS_BACK_WIDTH, IMAGE_IGS_BACK_HEIGHT);

	int iSizeCategory = g_InGameShopSystem->GetSizeCategoriesAsSelectedZone();

	if( iSizeCategory < 0 )
		return;

	// Category Deco Middle Render
	POINT CategoryDecoMiddlePos;
	CategoryDecoMiddlePos.x = m_CategoryButton.GetPos(0).x+(IMAGE_IGS_CATEGORY_BTN_WIDTH/2)-(IMAGE_IGS_CATEGORY_DECO_MIDDLE_WIDTH/2);

	for(int i=0 ; i<iSizeCategory-1 ; i++)
	{
		CategoryDecoMiddlePos.y = m_CategoryButton.GetPos(i).y+IMAGE_IGS_CATEGORY_BTN_HEIGHT-1;

		RenderImage(IMAGE_IGS_CATEGORY_DECO_MIDDLE,	CategoryDecoMiddlePos.x, CategoryDecoMiddlePos.y, IMAGE_IGS_CATEGORY_DECO_MIDDLE_WIDTH, IMAGE_IGS_CATEGORY_DECO_MIDDLE_HEIGHT);
	}

	// Category Deco Down Render
	RenderImage(IMAGE_IGS_CATEGORY_DECO_DOWN,m_Pos.x, m_CategoryButton.GetPos(iSizeCategory-1).y-10, IMAGE_IGS_CATEGORY_DECO_DOWN_WIDTH, IMAGE_IGS_CATEGORY_DECO_DOWN_HEIGHT);

	for(int cnt=g_InGameShopSystem->GetSizePackageAsDisplayPackage() ; cnt<INGAMESHOP_DISPLAY_ITEMLIST_SIZE ; cnt++)
	{
		RenderImage(IMAGE_IGS_ITEMBOX_LOGO,	m_Pos.x+IMAGE_IGS_ITEMBOX_LOGO_POS_X+((cnt%IGS_NUM_ITEMS_WIDTH)*IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_X),m_Pos.y+IMAGE_IGS_ITEMBOX_LOGO_POS_Y+((cnt/IGS_NUM_ITEMS_HEIGHT)*IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_Y),IMAGE_IGS_ITEMBOX_LOGO_SIZE, IMAGE_IGS_ITEMBOX_LOGO_SIZE);
	}

	RenderImage(IMAGE_IGS_STORAGE_PAGE, m_Pos.x+IMAGE_IGS_STORAGE_PAGE_POS_X, m_Pos.y+IMAGE_IGS_STORAGE_PAGE_POS_Y, IMGAE_IGS_STORAGE_PAGE_WIDTH, IMGAE_IGS_STORAGE_PAGE_HEIGHT);
}

/*
	The modern shell: header strip, sidebar column, content rules, item cards.

	Everything here is a flat fill except the card plate and the storage page
	frame, so the skin needs no new art beyond what Phase 6 already generated.
*/
void CNewUIInGameShop::RenderModernFrame()
{
	const int X = m_Pos.x;
	const int Y = m_Pos.y;

	RenderFillRect(X, Y, IMAGE_IGS_BACK_WIDTH, IMAGE_IGS_BACK_HEIGHT, 0.07f, 0.07f, 0.08f, 1.0f);

	// Header strip, and the sidebar column that starts under it.
	RenderFillRect(X, Y, IMAGE_IGS_BACK_WIDTH, MODERN_HEADER_HEIGHT, 0.10f, 0.10f, 0.11f, 1.0f);
	RenderFillRect(X, Y+MODERN_HEADER_HEIGHT, MODERN_SIDEBAR_WIDTH,
		IMAGE_IGS_BACK_HEIGHT-MODERN_HEADER_HEIGHT, 0.09f, 0.09f, 0.10f, 1.0f);

	// Hairlines: under the header, down the sidebar edge, under the heading.
	RenderFillRect(X, Y+MODERN_HEADER_HEIGHT-1, IMAGE_IGS_BACK_WIDTH, 1, 0.20f, 0.20f, 0.22f, 1.0f);
	RenderFillRect(X+MODERN_SIDEBAR_WIDTH-1, Y+MODERN_HEADER_HEIGHT, 1,
		IMAGE_IGS_BACK_HEIGHT-MODERN_HEADER_HEIGHT, 0.20f, 0.20f, 0.22f, 1.0f);
	RenderFillRect(X+MODERN_CONTENT_POS_X, Y+MODERN_DIVIDER_POS_Y, MODERN_CONTENT_WIDTH, 1,
		0.20f, 0.20f, 0.22f, 1.0f);

	// Separators between the three balances in the header.
	for(int i=1 ; i<3 ; i++)
	{
		RenderFillRect(X+MODERN_CUR_FIRST_POS_X+(i*MODERN_CUR_COL_WIDTH)-8, Y+6,
			1, MODERN_HEADER_HEIGHT-12, 0.24f, 0.24f, 0.26f, 1.0f);
	}

	/*
		One card per package actually on the shelf.

		The legacy skin drew a logo plate in all nine cells whether or not
		anything was in them; the mockup shows nothing where there is nothing,
		which is also the honest reading of "3 Items".
	*/
	int iCount = g_InGameShopSystem->GetSizePackageAsDisplayPackage();

	for(int i=0 ; i<iCount ; i++)
	{
		int iCardX = X+MODERN_CARD_POS_X+((i%IGS_NUM_ITEMS_WIDTH)*MODERN_CARD_DISTANCE_X);
		int iCardY = Y+MODERN_CARD_POS_Y+((i/IGS_NUM_ITEMS_HEIGHT)*MODERN_CARD_DISTANCE_Y);

		RenderImage(IMAGE_IGS_MODERN_CARD_PANEL, iCardX, iCardY, MODERN_CARD_WIDTH, MODERN_CARD_HEIGHT);

		// The icon well - the lighter plate the 3D model is drawn over.
		RenderFillRect(iCardX+MODERN_CARD_IMG_INSET, iCardY+MODERN_CARD_IMG_INSET,
			MODERN_CARD_WIDTH-(MODERN_CARD_IMG_INSET*2), MODERN_CARD_IMG_HEIGHT,
			0.16f, 0.16f, 0.17f, 1.0f);
	}

	// The storage pager's backing plate. A flat fill rather than a tenth new
	// texture - it is a static rectangle with a number on it.
	RenderFillRect(X+MODERN_STORAGE_PAGE_POS_X, Y+MODERN_STORAGE_PAGE_POS_Y,
		MODERN_STORAGE_PAGE_WIDTH, MODERN_STORAGE_PAGE_HEIGHT, 0.13f, 0.13f, 0.14f, 1.0f);
}

void CNewUIInGameShop::RenderTexts()	
{
	if( IsModernSkin() )
	{
		RenderModernTexts();
		return;
	}

	unicode::t_char szText[256] = {0,};
	unicode::t_char szValue[256] = {0,};

	g_pRenderText->SetBgColor(0, 0, 0, 0);
	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->SetFont(g_hFontBold);
	sprintf(szText, Hero->ID);
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_CHAR_NAME_POS_X, m_Pos.y+TEXT_IGS_CHAR_NAME_POS_Y,szText, TEXT_IGS_CHAR_NAME_WIDTH, 0, RT3_SORT_CENTER);
	g_pRenderText->SetFont(g_hFont);

	// Display Item
	for(int i=0 ; i<g_InGameShopSystem->GetSizePackageAsDisplayPackage() ; i++)
	{
		CShopPackage* pPackage = g_InGameShopSystem->GetDisplayPackage(i);

		int iCellX  = m_Pos.x+IGS_PACKAGE_NAME_POS_X+((i%IGS_NUM_ITEMS_WIDTH)*IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_X);
		int iNameY  = m_Pos.y+IGS_PACKAGE_NAME_POS_Y+((i/IGS_NUM_ITEMS_HEIGHT)*IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_Y);
		int iPriceY = m_Pos.y+IGS_PACKAGE_PRICE_POS_Y+53+((i/IGS_NUM_ITEMS_HEIGHT)*IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_Y);

		// Package
		//
		// The admin-edited DisplayName overrides the script name when custom mode
		// is on and the server actually sent one for this package - otherwise the
		// IBSPackage.txt name keeps showing, same fallback philosophy as price.
		char szOverrideName[32] = {0};
		const char* pszDisplayName = pPackage->PackageProductName;

		if( gProtect.m_MainInfo.CustomCashShop != 0
			&& g_InGameShopSystem->GetServerPackageName(pPackage->PackageProductSeq, szOverrideName, sizeof(szOverrideName)) )
		{
			pszDisplayName = szOverrideName;
		}

		g_pRenderText->SetTextColor(255, 255, 255, 255);
		g_pRenderText->RenderText(iCellX, iNameY, pszDisplayName, IGS_PACKAGE_NAME_WIDTH, 0, RT3_SORT_CENTER);

		/*
			The price.

			IBSPackage.txt knows a list price and nothing about a sale, so in custom
			mode the shelf shows what the GameServer said it will charge instead.
			Falling back to the script price when the server sent nothing is
			deliberate: an unknown price should read as the old number, not as free.

			Purely cosmetic - the buy request has never carried a price, so what is
			drawn here cannot change what is taken.
		*/
		CInGameShopSystem::IGS_SERVER_PRICE ServerPrice;

		bool bServerPrice = (gProtect.m_MainInfo.CustomCashShop != 0)
			&& g_InGameShopSystem->GetServerPrice(CInGameShopSystem::IGS_PRICE_KIND_PACKAGE, pPackage->PackageProductSeq, ServerPrice);

		if( bServerPrice && ServerPrice.iDiscountPercent > 0 )
		{
			RenderPackageSalePrice(iCellX, iNameY, iPriceY,
				ServerPrice.iListPrice, ServerPrice.iEffectivePrice,
				ServerPrice.iDiscountPercent, pPackage->PricUnitName);
		}
		else
		{
			ConvertGold((bServerPrice ? ServerPrice.iEffectivePrice : pPackage->Price), szValue);
			sprintf(szText, "%s %s", szValue, pPackage->PricUnitName);
			g_pRenderText->SetTextColor(255, 238, 161, 255);
			g_pRenderText->RenderText(iCellX, iPriceY, szText, IGS_PACKAGE_NAME_WIDTH, 0, RT3_SORT_CENTER);
		}
	}
	g_pRenderText->SetTextColor(255, 238, 161, 255);

	//CreditCard
	ConvertGold(g_InGameShopSystem->GetCashCreditCard(), szValue);
	sprintf( szText, GlobalText[2883], "");
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_CASH_POS_X, m_Pos.y+TEXT_IGS_CASH_POS_Y, szText, TEXT_IGS_CASH_WIDTH, 0, RT3_SORT_LEFT);
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_CASH_POS_X+50, m_Pos.y+TEXT_IGS_CASH_POS_Y, szValue, TEXT_IGS_CASH_WIDTH-56, 0, RT3_SORT_RIGHT);
	
	//Prepaid 
	ConvertGold(g_InGameShopSystem->GetCashPrepaid(), szValue);
	sprintf( szText, GlobalText[3145], "");
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_CASH_POS_X, m_Pos.y+TEXT_IGS_MILEAGE_POS_Y, szText, TEXT_IGS_CASH_WIDTH, 0, RT3_SORT_LEFT);
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_CASH_POS_X+50, m_Pos.y+TEXT_IGS_MILEAGE_POS_Y, szValue, TEXT_IGS_CASH_WIDTH-56, 0, RT3_SORT_RIGHT);
	
	ConvertGold(g_InGameShopSystem->GetTotalMileage(), szValue, 1);
	sprintf( szText, GlobalText[2884], "");
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_CASH_POS_X, m_Pos.y+TEXT_IGS_POINT_POS_Y, szText, TEXT_IGS_CASH_WIDTH, 0, RT3_SORT_LEFT);
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_CASH_POS_X+50, m_Pos.y+TEXT_IGS_POINT_POS_Y, szValue, TEXT_IGS_CASH_WIDTH-56, 0, RT3_SORT_RIGHT);

	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->SetFont(g_hFontBold);
	
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_STORAGE_NAME_POS_X, m_Pos.y+TEXT_IGS_STORAGE_NAME_POS_Y,	GlobalText[2951], TEXT_IGS_STORAGE_NAME_WIDTH, 0, RT3_SORT_CENTER);

	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_STORAGE_TIME_POS_X, m_Pos.y+TEXT_IGS_STORAGE_NAME_POS_Y,	GlobalText[2952], TEXT_IGS_STORAGE_TIME_WIDTH, 0, RT3_SORT_CENTER);


	// Page Info
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_PAGE_POS_X+23, m_Pos.y+TEXT_IGS_PAGE_POS_Y, "/", 10, 0, RT3_SORT_CENTER);

	sprintf( szText, "%d", g_InGameShopSystem->GetSelectPage());
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_PAGE_POS_X+5, m_Pos.y+TEXT_IGS_PAGE_POS_Y, szText, 15, 0, RT3_SORT_RIGHT);

	sprintf( szText, "%d", g_InGameShopSystem->GetTotalPages());
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_PAGE_POS_X+36, m_Pos.y+TEXT_IGS_PAGE_POS_Y, szText, 15, 0, RT3_SORT_LEFT);

	// Storage Page Info
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_STORAGE_PAGE_INFO_POS_X+35, m_Pos.y+TEXT_IGS_STORAGE_PAGE_INFO_POS_Y, "/", 10, 0, RT3_SORT_CENTER);
	sprintf( szText, "%d", m_iStorageCurrentPage);
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_STORAGE_PAGE_INFO_POS_X+12, m_Pos.y+TEXT_IGS_STORAGE_PAGE_INFO_POS_Y, szText, 20, 0, RT3_SORT_RIGHT);
	sprintf( szText, "%d", m_iStorageTotalPage);
	g_pRenderText->RenderText(m_Pos.x+TEXT_IGS_STORAGE_PAGE_INFO_POS_X+48, m_Pos.y+TEXT_IGS_STORAGE_PAGE_INFO_POS_Y, szText, 20, 0, RT3_SORT_LEFT);

#ifdef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
#ifdef FOR_WORK
	g_pRenderText->SetTextColor(210, 180, 230, 255);
	g_pRenderText->SetFont(g_hFont);

	// Script Version Info
	CListVersionInfo ScriptVer; 
	ScriptVer = g_InGameShopSystem->GetCurrentScriptVer();
	sprintf( szText, "Script Ver. %d.%d.%d", ScriptVer.Zone, ScriptVer.year, ScriptVer.yearId);
	g_pRenderText->RenderText(m_Pos.x+12, m_Pos.y+396, szText, 150, 0, RT3_SORT_LEFT);

	ScriptVer = g_InGameShopSystem->GetCurrentBannerVer();
	sprintf( szText, "Banner Ver. %d.%d.%d", ScriptVer.Zone, ScriptVer.year, ScriptVer.yearId);
	g_pRenderText->RenderText(m_Pos.x+12, m_Pos.y+408, szText, 150, 0, RT3_SORT_LEFT);
#endif // FOR_WORK
#endif //KJH_MOD_SHOP_SCRIPT_DOWNLOAD
}

/*
	One discounted package cell.

	Layout is squeezed between the item box above (which ends 4px up) and the next
	row's name 36px down, so the two prices stack rather than sitting side by side:
	the struck list price on the line the original shop drew its only price on, and
	the price actually charged directly under it.

	The strike is a 1px bar rather than a font style - RenderText has no such
	thing - so the text is measured first and the bar drawn across what came back.
*/
void CNewUIInGameShop::RenderPackageSalePrice(int iCellX, int iNameY, int iPriceY,
	int iListPrice, int iEffectivePrice, int iDiscountPercent, const char* pszUnitName)
{
	unicode::t_char szText[256] = {0,};
	unicode::t_char szValue[256] = {0,};
	SIZE TextSize = {0, 0};

	// ---- list price, struck through -------------------------------------
	ConvertGold(iListPrice, szValue);
	sprintf(szText, "%s %s", szValue, pszUnitName);

	g_pRenderText->SetTextColor(160, 160, 160, 255);
	g_pRenderText->RenderText(iCellX, iPriceY, szText, IGS_PACKAGE_NAME_WIDTH, 0, RT3_SORT_CENTER, &TextSize);

	if( TextSize.cx > 0 )
	{
		// RenderText centred the string inside the 104px box, so the bar has to be
		// centred the same way rather than started at the cell edge.
		float fStrikeX = (float)(iCellX + ((IGS_PACKAGE_NAME_WIDTH - TextSize.cx) / 2));
		float fStrikeY = (float)(iPriceY + (TextSize.cy / 2));

		EnableAlphaTest(true);
		glColor4f(0.63f, 0.63f, 0.63f, 1.0f);
		RenderColor(fStrikeX, fStrikeY, (float)TextSize.cx, 1.0f, 0.0f, 0);
		EndRenderColor();
		glColor3f(1.0f, 1.0f, 1.0f);
		EnableAlphaTest(false);
	}

	// ---- what it actually costs -----------------------------------------
	ConvertGold(iEffectivePrice, szValue);
	sprintf(szText, "%s %s", szValue, pszUnitName);

	g_pRenderText->SetTextColor(130, 255, 150, 255);
	g_pRenderText->RenderText(iCellX, iPriceY + 12, szText, IGS_PACKAGE_NAME_WIDTH, 0, RT3_SORT_CENTER);

	// ---- percent badge, top-left of the item box -------------------------
	// Over the box rather than beside it: the three columns are 122px apart with
	// no gutter to put it in, and a badge that sits on the art is what a shopper
	// expects anyway.
	float fBadgeX = (float)(iCellX);
	float fBadgeY = (float)(iNameY + 11);

	EnableAlphaTest(true);
	glColor4f(0.72f, 0.11f, 0.13f, 0.88f);
	RenderColor(fBadgeX, fBadgeY, 38.0f, 14.0f, 0.0f, 0);
	EndRenderColor();
	glColor3f(1.0f, 1.0f, 1.0f);
	EnableAlphaTest(false);

	sprintf(szText, "-%d%%", iDiscountPercent);

	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->RenderText((int)fBadgeX, (int)fBadgeY + 1, szText, 38, 0, RT3_SORT_CENTER);
}

/*
	Every string the modern skin draws.

	Kept whole rather than sprinkled through RenderTexts as more skin branches:
	the two layouts disagree about where almost every string goes, so a shared
	function would have been a branch per line.
*/
void CNewUIInGameShop::RenderModernTexts()
{
	unicode::t_char szText[256] = {0,};
	unicode::t_char szValue[256] = {0,};

	const int X = m_Pos.x;
	const int Y = m_Pos.y;

	g_pRenderText->SetBgColor(0, 0, 0, 0);

	// ---- header: the shop's own name -------------------------------------
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->RenderText(X+MODERN_TITLE_POS_X, Y+MODERN_TITLE_POS_Y, "CASH SHOP", 160, 0, RT3_SORT_LEFT);

	/*
		The three balances, label over value.

		GlobalText carries these as format strings ("WCoin: %s"), so they go
		through sprintf with an empty argument exactly as the legacy skin does -
		rendering GlobalText[2883] directly would print the %s.
	*/
	int iBalance[3];
	iBalance[0] = g_InGameShopSystem->GetCashCreditCard();
	iBalance[1] = g_InGameShopSystem->GetCashPrepaid();
	iBalance[2] = g_InGameShopSystem->GetTotalMileage();

	for(int i=0 ; i<3 ; i++)
	{
		int iColX = X+MODERN_CUR_FIRST_POS_X+(i*MODERN_CUR_COL_WIDTH);

		sprintf(szText, (i == 0) ? GlobalText[2883] : ((i == 1) ? GlobalText[3145] : GlobalText[2884]), "");
		g_pRenderText->SetFont(g_hFont);
		g_pRenderText->SetTextColor(150, 150, 158, 255);
		g_pRenderText->RenderText(iColX, Y+MODERN_CUR_LABEL_POS_Y, szText, MODERN_CUR_COL_WIDTH-12, 0, RT3_SORT_LEFT);

		ConvertGold(iBalance[i], szValue, (i == 2) ? 1 : 0);
		g_pRenderText->SetFont(g_hFontBold);
		g_pRenderText->SetTextColor(255, 255, 255, 255);
		g_pRenderText->RenderText(iColX, Y+MODERN_CUR_VALUE_POS_Y, szValue, MODERN_CUR_COL_WIDTH-12, 0, RT3_SORT_LEFT);
	}

	// ---- content heading: selected category, then how much is in it -------
	type_listName CategoryNameList = g_InGameShopSystem->GetCategoryName();
	int iSelectedCategory = m_CategoryButton.GetCurButtonIndex();
	int iWalk = 0;

	szText[0] = 0;

	for(type_listName::iterator it = CategoryNameList.begin() ; it != CategoryNameList.end() ; ++it, ++iWalk)
	{
		if( iWalk == iSelectedCategory )
		{
			sprintf(szText, "%s", (*it).c_str());
			break;
		}
	}

	int iCount = g_InGameShopSystem->GetSizePackageAsDisplayPackage();

	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->RenderText(X+MODERN_CONTENT_POS_X, Y+MODERN_HEADING_POS_Y, szText, 200, 0, RT3_SORT_LEFT);

	sprintf(szText, "%d Items", iCount);
	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetTextColor(140, 140, 148, 255);
	g_pRenderText->RenderText(X+MODERN_ITEMCOUNT_POS_X, Y+MODERN_HEADING_POS_Y+3, szText, MODERN_ITEMCOUNT_WIDTH, 0, RT3_SORT_RIGHT);

	// Page counter, between the two paging buttons in the heading row.
	g_pRenderText->RenderText(X+MODERN_PAGE_TEXT_POS_X, Y+MODERN_HEADING_POS_Y+3, "/", MODERN_PAGE_TEXT_WIDTH, 0, RT3_SORT_CENTER);
	sprintf(szText, "%d", g_InGameShopSystem->GetSelectPage());
	g_pRenderText->RenderText(X+MODERN_PAGE_TEXT_POS_X-4, Y+MODERN_HEADING_POS_Y+3, szText, MODERN_PAGE_TEXT_WIDTH, 0, RT3_SORT_RIGHT);
	sprintf(szText, "%d", g_InGameShopSystem->GetTotalPages());
	g_pRenderText->RenderText(X+MODERN_PAGE_TEXT_POS_X+4, Y+MODERN_HEADING_POS_Y+3, szText, MODERN_PAGE_TEXT_WIDTH, 0, RT3_SORT_LEFT);

	// ---- the shelf --------------------------------------------------------
	for(int i=0 ; i<iCount ; i++)
	{
		RenderModernCardText(i,
			X+MODERN_CARD_POS_X+((i%IGS_NUM_ITEMS_WIDTH)*MODERN_CARD_DISTANCE_X),
			Y+MODERN_CARD_POS_Y+((i/IGS_NUM_ITEMS_HEIGHT)*MODERN_CARD_DISTANCE_Y));
	}

	// ---- right panel ------------------------------------------------------
	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetTextColor(150, 150, 158, 255);
	g_pRenderText->RenderText(X+TEXT_IGS_CHAR_NAME_POS_X, Y+MODERN_CHAR_LABEL_POS_Y, "CHARACTER", TEXT_IGS_CHAR_NAME_WIDTH, 0, RT3_SORT_LEFT);

	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(255, 255, 255, 255);
	sprintf(szText, "%s", Hero->ID);
	g_pRenderText->RenderText(X+TEXT_IGS_CHAR_NAME_POS_X, Y+MODERN_CHAR_NAME_POS_Y, szText, TEXT_IGS_CHAR_NAME_WIDTH, 0, RT3_SORT_LEFT);
	g_pRenderText->SetFont(g_hFont);

	for(int i=0 ; i<3 ; i++)
	{
		int iRowY = Y+MODERN_PANEL_CASH_POS_Y+(i*MODERN_PANEL_ROW_HEIGHT);

		sprintf(szText, (i == 0) ? GlobalText[2883] : ((i == 1) ? GlobalText[3145] : GlobalText[2884]), "");
		g_pRenderText->SetTextColor(150, 150, 158, 255);
		g_pRenderText->RenderText(X+TEXT_IGS_CASH_POS_X, iRowY, szText, TEXT_IGS_CASH_WIDTH, 0, RT3_SORT_LEFT);

		ConvertGold(iBalance[i], szValue, (i == 2) ? 1 : 0);
		g_pRenderText->SetTextColor(255, 255, 255, 255);
		g_pRenderText->RenderText(X+TEXT_IGS_CASH_POS_X, iRowY, szValue, TEXT_IGS_CASH_WIDTH, 0, RT3_SORT_RIGHT);
	}

	// ---- storage list headers and its own pager ---------------------------
	g_pRenderText->SetTextColor(150, 150, 158, 255);
	g_pRenderText->RenderText(X+TEXT_IGS_STORAGE_NAME_POS_X, Y+TEXT_IGS_STORAGE_NAME_POS_Y, GlobalText[2951], TEXT_IGS_STORAGE_NAME_WIDTH, 0, RT3_SORT_CENTER);
	g_pRenderText->RenderText(X+TEXT_IGS_STORAGE_TIME_POS_X, Y+TEXT_IGS_STORAGE_NAME_POS_Y, GlobalText[2952], TEXT_IGS_STORAGE_TIME_WIDTH, 0, RT3_SORT_CENTER);

	// Centred on the smaller plate rather than the legacy 80x30 one.
	int iStoragePageMidX = X + MODERN_STORAGE_PAGE_POS_X + (MODERN_STORAGE_PAGE_WIDTH/2);
	int iStoragePageY = Y + MODERN_STORAGE_PAGE_POS_Y + 3;

	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->RenderText(iStoragePageMidX-5, iStoragePageY, "/", 10, 0, RT3_SORT_CENTER);
	sprintf(szText, "%d", m_iStorageCurrentPage);
	g_pRenderText->RenderText(iStoragePageMidX-28, iStoragePageY, szText, 20, 0, RT3_SORT_RIGHT);
	sprintf(szText, "%d", m_iStorageTotalPage);
	g_pRenderText->RenderText(iStoragePageMidX+8, iStoragePageY, szText, 20, 0, RT3_SORT_LEFT);

	// The Script/Banner version stamps the legacy skin prints bottom-left are
	// developer plumbing, and the mockup has no place for them.
}

/*
	One card's text: name under the icon, then the price.

	Undiscounted prices get the mockup's filled chip. Discounted ones keep the
	Phase 4 treatment instead - struck list price with the charged price under
	it, and a percent badge over the icon well - because a chip cannot show two
	numbers and the saving is the whole reason for drawing two.
*/
void CNewUIInGameShop::RenderModernCardText(int iIndex, int iCardX, int iCardY)
{
	unicode::t_char szText[256] = {0,};
	unicode::t_char szValue[256] = {0,};

	CShopPackage* pPackage = g_InGameShopSystem->GetDisplayPackage(iIndex);

	if( pPackage == NULL )
		return;

	char szOverrideName[32] = {0};
	const char* pszDisplayName = pPackage->PackageProductName;

	if( g_InGameShopSystem->GetServerPackageName(pPackage->PackageProductSeq, szOverrideName, sizeof(szOverrideName)) )
		pszDisplayName = szOverrideName;

	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->RenderText(iCardX+2, iCardY+MODERN_CARD_NAME_POS_Y, pszDisplayName, MODERN_CARD_WIDTH-4, 0, RT3_SORT_CENTER);
	g_pRenderText->SetFont(g_hFont);

	CInGameShopSystem::IGS_SERVER_PRICE ServerPrice;

	bool bServerPrice = g_InGameShopSystem->GetServerPrice(CInGameShopSystem::IGS_PRICE_KIND_PACKAGE, pPackage->PackageProductSeq, ServerPrice);

	bool bOnSale = (bServerPrice && ServerPrice.iDiscountPercent > 0);

	if( bOnSale )
	{
		// The saving goes on the icon well - a badge bottom-left and the struck
		// list price bottom-right - so the chip below can stay a single line.
		RenderModernSaleMarks(iCardX, iCardY, ServerPrice.iListPrice,
			ServerPrice.iDiscountPercent, pPackage->PricUnitName);
	}

	// The mockup's price chip: a dark red plate across the card's inner column
	// with the price centred on it, instead of the legacy loose gold text.
	RenderFillRect(iCardX+MODERN_CARD_IMG_INSET, iCardY+MODERN_CARD_PILL_POS_Y,
		MODERN_CARD_WIDTH-(MODERN_CARD_IMG_INSET*2), MODERN_CARD_PILL_HEIGHT,
		0.28f, 0.10f, 0.09f, 1.0f);

	ConvertGold((bServerPrice ? ServerPrice.iEffectivePrice : pPackage->Price), szValue);
	sprintf(szText, "%s %s", szValue, pPackage->PricUnitName);

	// Green when it is a saving, the shop's warm gold when it is just the price.
	if( bOnSale )
		g_pRenderText->SetTextColor(130, 255, 150, 255);
	else
		g_pRenderText->SetTextColor(255, 205, 150, 255);

	g_pRenderText->RenderText(iCardX+2, iCardY+MODERN_CARD_PILL_POS_Y+1, szText, MODERN_CARD_WIDTH-4, 0, RT3_SORT_CENTER);
}

/*
	The two marks that say a card is on sale: the percent badge and the struck
	list price, both laid over the bottom of the icon well.

	They live on the artwork rather than under the name because the card no
	longer has a spare text row - and a shopper reads a badge on the picture
	before they read anything underneath it.
*/
void CNewUIInGameShop::RenderModernSaleMarks(int iCardX, int iCardY, int iListPrice, int iDiscountPercent, const char* pszUnitName)
{
	unicode::t_char szText[256] = {0,};
	unicode::t_char szValue[256] = {0,};
	SIZE TextSize = {0, 0};

	int iMarkY = iCardY + MODERN_CARD_IMG_INSET + MODERN_CARD_IMG_HEIGHT - 14;

	RenderFillRect(iCardX+MODERN_CARD_IMG_INSET, iMarkY, 32, 13, 0.72f, 0.11f, 0.13f, 0.92f);

	sprintf(szText, "-%d%%", iDiscountPercent);
	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->RenderText(iCardX+MODERN_CARD_IMG_INSET, iMarkY+1, szText, 32, 0, RT3_SORT_CENTER);

	ConvertGold(iListPrice, szValue);
	sprintf(szText, "%s", szValue);

	int iStrikeBoxX = iCardX + MODERN_CARD_WIDTH - MODERN_CARD_IMG_INSET - 60;

	g_pRenderText->SetTextColor(190, 190, 190, 255);
	g_pRenderText->RenderText(iStrikeBoxX, iMarkY+1, szText, 58, 0, RT3_SORT_RIGHT, &TextSize);

	if( TextSize.cx <= 0 )
		return;

	// RenderText right-aligned inside a 58px box, so the bar starts where the
	// string actually did rather than at the box edge.
	float fStrikeX = (float)(iStrikeBoxX + 58 - TextSize.cx);

	EnableAlphaTest(true);
	glColor4f(0.75f, 0.75f, 0.75f, 1.0f);
	RenderColor(fStrikeX, (float)(iMarkY + 1 + (TextSize.cy / 2)), (float)TextSize.cx, 1.0f, 0.0f, 0);
	EndRenderColor();
	glColor3f(1.0f, 1.0f, 1.0f);
	EnableAlphaTest(false);
}

void CNewUIInGameShop::RenderButtons()
{
	m_ZoneButton.Render();
	m_CategoryButton.Render();
 	m_ListBoxTabButton.Render();

	for(int i=0 ; i<g_InGameShopSystem->GetSizePackageAsDisplayPackage() ; i++)
	{
		m_ViewDetailButton[i].Render();	
	}

	m_CashGiftButton.Render();
	m_CashChargeButton.Render();
	m_CashRefreshButton.Render();
	m_UseButton.Render();
	m_PrevButton.Render();
	m_NextButton.Render();
	m_StoragePrevButton.Render();
	m_StorageNextButton.Render();
	m_CloseButton.Render();


}

void CNewUIInGameShop::RenderListBox()
{	
	m_StorageItemListBox.Render();
}

bool CNewUIInGameShop::IsInGameShopRect(float _x,float _y)
{
	if(!g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INGAMESHOP))
		return false;

	RECT _TempRT;

	// m_Pos-relative since the window became draggable - hard-coded 0,0 here
	// would keep claiming the cursor at the window's original spot.
	_TempRT.top = m_Pos.y;
	_TempRT.bottom = m_Pos.y + IMAGE_IGS_BACK_HEIGHT;
	_TempRT.left = m_Pos.x;
	_TempRT.right = m_Pos.x + IMAGE_IGS_BACK_WIDTH;

	if(_x >= _TempRT.left && _x < _TempRT.right && _y < _TempRT.bottom && _y >= _TempRT.top)
		return true;
	else
		return false;

	return false;
}

void CNewUIInGameShop::SetConvertInvenCoord(WORD _ItemType, float _Width, float _Height)
{
	ITEM_ATTRIBUTE* pItemAttr = &ItemAttribute[_ItemType];
	float _TempWidth = pItemAttr->Width*20.0f;
	float _TempHeight = pItemAttr->Height*20.0f;
	float _fCoodX=0,_fCoodY=0;

	if(_ItemType == ITEM_WING+36)
	{
		_fCoodY = 5.0f;
	}

	else if(pItemAttr->Height >= 4)
	{
		_fCoodY = -10.0f;
	}

	m_fRePos.x = (_Width/2) - (_TempWidth/2) + _fCoodX;
	m_fRePos.y = (_Height/2) - (_TempHeight/2) + _fCoodY;
	m_fReSize.x = _TempWidth;
	m_fReSize.y = _TempHeight;
}
void CNewUIInGameShop::SetRateScale(int _ItemType)
{
	const float _fRate_Value = 0.703f;
	ITEM_ATTRIBUTE* pItemAttr = &ItemAttribute[_ItemType];

	if(_ItemType == ITEM_WING+36)
	{
		m_fRate_Scale = _fRate_Value*0.7f;
	}
	else if(_ItemType == ITEM_STAFF+10)
	{
		m_fRate_Scale = _fRate_Value*0.7f;
	}
	else if(_ItemType >= ITEM_HELPER+117 && _ItemType <= ITEM_HELPER+120)
	{
		m_fRate_Scale = _fRate_Value*1.6f;
	}
	else if(pItemAttr->Height >= 4)
	{
		m_fRate_Scale = _fRate_Value*0.7f;
	}
	else
	{
		m_fRate_Scale = _fRate_Value;
	}
}

void CNewUIInGameShop::RenderDisplayItems()
{		
	EndBitmap();
	
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glViewport2(0,0,WindowWidth,WindowHeight);
	//DAT Fix Size Item Wide XSHOP
	float SizeItem = 1.6f;
	//if (m_Resolution >= 3) { SizeItem += 0.70f; } //Gốc
	switch ((int)m_Resolution)
	{
	case 3: SizeItem += 0.30f; break;	//1280x1024
	case 4: SizeItem += 0.40f; break;	//1366x768
	case 5: SizeItem += 0.50f; break;	//1440x900
	case 6: SizeItem += 0.60f; break;	//1600x900
	case 7: SizeItem += 0.70f; break;	//1680x1050
	case 8: SizeItem += 0.80f; break;	//1920x1080
	}
	gluPerspective2(SizeItem, (float)(WindowWidth)/(float)(WindowHeight), RENDER_ITEMVIEW_NEAR, RENDER_ITEMVIEW_FAR);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	GetOpenGLMatrix(CameraMatrix);
	EnableDepthTest();
	EnableDepthMask();
	
	glClear(GL_DEPTH_BUFFER_BIT);

	for(int i=0 ; i<g_InGameShopSystem->GetSizePackageAsDisplayPackage() ; i++ )
	{
 		//int iPosX = IGS_ITEMRENDER_POS_X_STANDAD + DisplayWinExt +(IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_X*(i%IGS_NUM_ITEMS_WIDTH));
 		int iPosX = IGS_ITEMRENDER_POS_X_STANDAD + DisplayWinExt +(IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_X*(i%IGS_NUM_ITEMS_WIDTH));
  		int iPosY = IGS_ITEMRENDER_POS_Y_STANDAD+(IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_Y*(i/IGS_NUM_ITEMS_HEIGHT));
		int iRenderW = IGS_ITEMRENDER_POS_WIDTH;
		int iRenderH = IGS_ITEMRENDER_POS_HEIGHT;
		int iSwordNudge = -25;

		if( IsModernSkin() )
		{
			// The model belongs in the card's icon well, which is both shorter
			// and higher up than the legacy item box. The sword nudge scales
			// with it - at -25 the model would hang off the side of the card.
			iPosX = m_Pos.x + MODERN_CARD_POS_X + 4 + (MODERN_CARD_DISTANCE_X*(i%IGS_NUM_ITEMS_WIDTH));
			iPosY = m_Pos.y + MODERN_CARD_POS_Y + MODERN_CARD_IMG_INSET + (MODERN_CARD_DISTANCE_Y*(i/IGS_NUM_ITEMS_HEIGHT));
			iRenderW = MODERN_CARD_WIDTH - 8;
			iRenderH = MODERN_CARD_IMG_HEIGHT;
			iSwordNudge = -12;
		}

		if (g_InGameShopSystem->GetPackageItemCode(i) <= MODEL_SWORD)
		{
			RenderItem3D(iPosX+iSwordNudge, iPosY, iRenderW, iRenderH, g_InGameShopSystem->GetPackageItemCode(i), 0, 0, 0, true);
		}
		else
		{
			RenderItem3D(iPosX, iPosY, iRenderW, iRenderH, g_InGameShopSystem->GetPackageItemCode(i), 0, 0, 0, true);
		}
	}

	UpdateMousePositionn();
	
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	
	BeginBitmap();
}
	
bool CNewUIInGameShop::BtnProcess()
{	
	if( g_InGameShopSystem->IsRequestEventPackge() == true )
	{
		if( m_ZoneButton.UpdateMouseEvent() != -1 )
		{
			g_InGameShopSystem->SelectZone(m_ZoneButton.GetCurButtonIndex());
			InitCategoryBtn();
			g_InGameShopSystem->SelectCategory(m_CategoryButton.GetCurButtonIndex());
			return true;
		}
		
		if( m_CategoryButton.UpdateMouseEvent() != -1 )
		{
			g_InGameShopSystem->SelectCategory(m_CategoryButton.GetCurButtonIndex());
			return true;
		}
	}
	
	if( m_ListBoxTabButton.UpdateMouseEvent() != -1 )
	{
		char szCode = GetCurrentStorageCode();
		m_iSelectedStorageItemIndex = 0;
		m_bRequestCurrentPage = true;
		SendRequestIGS_ItemStorageList(1, &szCode);
		return true;
	}

	for(int i=0 ; i<g_InGameShopSystem->GetSizePackageAsDisplayPackage() ; i++)
	{
		if( m_ViewDetailButton[i].UpdateMouseEvent() )
		{
			CShopPackage* pPackage = g_InGameShopSystem->GetDisplayPackage(i);
			
			if( pPackage->PriceCount == 1 )
			{
				CMsgBoxIGSBuyPackageItem* pMsgBox = NULL;
				CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxBuyPackageItemLayout), &pMsgBox);
				pMsgBox->Initialize(pPackage);
			}
			else if(pPackage->PriceCount > 1)
			{
				CMsgBoxIGSBuySelectItem* pMsgBox = NULL;
				CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSBuySelectItemLayout), &pMsgBox);
				pMsgBox->Initialize(pPackage);
			}

			return true;
		}
	}

	if( m_CashGiftButton.UpdateMouseEvent() == true )
	{
		CMsgBoxIGSCommon* pMsgBox = NULL;
		CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSCommonLayout), &pMsgBox);
		pMsgBox->Initialize(GlobalText[2937], GlobalText[2938]);
		return true;
	}

	if( m_CashChargeButton.UpdateMouseEvent() == true )
	{
		CMsgBoxIGSCommon* pMsgBox = NULL;
		CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSCommonLayout), &pMsgBox);
		pMsgBox->Initialize(GlobalText[2937], GlobalText[2938]);
		return true;
	}

	if( m_CashRefreshButton.UpdateMouseEvent() == true )
	{
		SendRequestIGS_CashPointInfo();

		return true;
	}

	if( m_UseButton.UpdateMouseEvent() == true )
	{
		if( m_StorageItemListBox.GetLineNum() <= 0 )
		{
			CMsgBoxIGSCommon* pMsgBox = NULL;
			CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSCommonLayout), &pMsgBox);
			pMsgBox->Initialize(GlobalText[3028], GlobalText[3033]);
			return true;
		}

		int iStorageIndex = m_ListBoxTabButton.GetCurButtonIndex();


		IGS_StorageItem* pSelectItem = m_StorageItemListBox.GetSelectedText();
		
		if( iStorageIndex == IGS_SAFEKEEPING_LISTBOX )					// º¸°üÇÔ
		{
			CMsgBoxIGSStorageItemInfo* pMsgBox = NULL;
			CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSStorageItemInfoLayout), &pMsgBox);
			pMsgBox->Initialize(pSelectItem->m_iStorageSeq, pSelectItem->m_iStorageItemSeq, pSelectItem->m_wItemCode, pSelectItem->m_szType, 
				pSelectItem->m_szName, pSelectItem->m_szNum, pSelectItem->m_szPeriod);
		}
		else if( iStorageIndex == IGS_PRESENTBOX_LISTBOX )				// ¼±¹° º¸°üÇÔ
		{
			CMsgBoxIGSGiftStorageItemInfo* pMsgBox = NULL;
			CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSGiftStorageItemInfoLayout), &pMsgBox);
			pMsgBox->Initialize(pSelectItem->m_iStorageSeq, pSelectItem->m_iStorageItemSeq, pSelectItem->m_wItemCode, 
				pSelectItem->m_szType, pSelectItem->m_szSendUserName, pSelectItem->m_szMessage, 
				pSelectItem->m_szName, pSelectItem->m_szNum, pSelectItem->m_szPeriod);
		}
		return true;
	}

	// Prev Button
	if(m_PrevButton.UpdateMouseEvent())
	{
		g_InGameShopSystem->PrePage();
	 	return true;
	}
	
	// Next Button
	if(m_NextButton.UpdateMouseEvent())
	{
		g_InGameShopSystem->NextPage();
		return true;
 	}

	// Storage Prev Button
	if(m_StoragePrevButton.UpdateMouseEvent())
	{
		StoragePrevPage();
		return true;
	}
	
	// Next Button
	if(m_StorageNextButton.UpdateMouseEvent())
	{
		StorageNextPage();
		return true;
 	}

	if(m_CloseButton.UpdateMouseEvent() == true)
	{	
		if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INGAMESHOP) == true)
		{	
			SendRequestIGS_CashShopOpen(1);		// ¼¥ Close¿äÃ»
			g_pNewUISystem->Hide(SEASON3B::INTERFACE_INGAMESHOP);

			return true;
		}
		return false;
	}

	return false;
}

void CNewUIInGameShop::SetBtnInfo()
{
	bool bModernSkin = IsModernSkin();

	/*
		Every widget below picks its texture off the same switch.

		Phase 6 and 8 reskinned the surfaces and left these on their original
		art, which against a flat dark shop read as leftovers rather than as a
		deliberate mix - the close button, the Use button, the paging arrows,
		the zone tab and the three icon buttons were the last ornate pieces on
		screen.
	*/
	m_CloseButton.ChangeButtonImgState(true, bModernSkin ? IMAGE_IGS_MODERN_CLOSE_BTN : IMAGE_IGS_EXIT_BTN, false);
	m_CloseButton.ChangeButtonInfo(
		m_Pos.x + (bModernSkin ? MODERN_CLOSE_POS_X : IMAGE_IGS_EXIT_BTN_POS_X),
		m_Pos.y + (bModernSkin ? MODERN_CLOSE_POS_Y : IMAGE_IGS_EXIT_BTN_POS_Y),
		bModernSkin ? MODERN_CLOSE_WIDTH : IMAGE_IGS_EXIT_BTN_WIDTH,
		bModernSkin ? MODERN_CLOSE_HEIGHT : IMAGE_IGS_EXIT_BTN_HEIGHT);
	m_CloseButton.ChangeToolTipText(GlobalText[1002], true);
	// Modern skin: same asymmetric UP=blank/DOWN=pill setup, red pill textures
	// instead of the legacy tan ones. The unselected tab still draws nothing
	// (BITMAP_UNKNOWN), so only the two DOWN-state images need swapping.
	bool bModernTabSkin = bModernSkin;
	int iTabLeftImg = bModernTabSkin ? IMAGE_IGS_MODERN_TAB_LEFT : IMAGE_IGS_LEFT_TAB;
	int iTabRightImg = bModernTabSkin ? IMAGE_IGS_MODERN_TAB_RIGHT : IMAGE_IGS_RIGHT_TAB;

	m_ListBoxTabButton.UnRegisterRadioButton();
	m_ListBoxTabButton.CreateRadioGroup(IGS_TOTAL_LISTBOX, iTabLeftImg);
	m_ListBoxTabButton.ChangeRadioButtonInfo(true, m_Pos.x+IMAGE_IGS_TAB_BTN_POS_X, m_Pos.y+IMAGE_IGS_TAB_BTN_POS_Y,IMAGE_IGS_TAB_BTN_WIDTH, IMAGE_IGS_TAB_BTN_HEIGHT, IMAGE_IGS_TAB_BTN_DISTANCE);
	m_ListBoxTabButton.ChangeButtonState( SEASON3B::BUTTON_STATE_DOWN, 0 );
	m_ListBoxTabButton.ChangeButtonState( IGS_SAFEKEEPING_LISTBOX, BITMAP_UNKNOWN, SEASON3B::BUTTON_STATE_UP, 0 );
	m_ListBoxTabButton.ChangeButtonState( IGS_PRESENTBOX_LISTBOX, BITMAP_UNKNOWN, SEASON3B::BUTTON_STATE_UP, 0 );
	m_ListBoxTabButton.ChangeButtonState( IGS_PRESENTBOX_LISTBOX, iTabRightImg, SEASON3B::BUTTON_STATE_DOWN, 0);

	unicode::t_string strText;
	std::list<unicode::t_string> TextList;
	strText = GlobalText[2888];
	TextList.push_back(strText);
	strText = GlobalText[2889];
	TextList.push_back(strText);
	
	m_ListBoxTabButton.ChangeRadioText(TextList);
	m_ListBoxTabButton.ChangeFrame(IGS_SAFEKEEPING_LISTBOX);

	// Modern skin: same widget, different texture - "BUY" is baked into
	// IMAGE_IGS_MODERN_VIEWDETAIL_BTN's own art, so the engine-drawn caption
	// that the legacy icon needs is suppressed rather than drawn on top of it.
	int iViewDetailBtnImg = bModernSkin ? IMAGE_IGS_MODERN_VIEWDETAIL_BTN : IMAGE_IGS_VIEWDETAIL_BTN;

	for(int i=0 ; i<INGAMESHOP_DISPLAY_ITEMLIST_SIZE ; i++)
	{
		m_ViewDetailButton[i].ChangeButtonImgState(true, iViewDetailBtnImg, true, false, true);

		if( bModernSkin )
		{
			// A bar across the foot of the card, the way the mockup draws it,
			// rather than the legacy 52x26 stub floating under the item box.
			m_ViewDetailButton[i].ChangeButtonInfo(
				m_Pos.x+MODERN_CARD_POS_X+MODERN_CARD_IMG_INSET+((i%IGS_NUM_ITEMS_WIDTH)*MODERN_CARD_DISTANCE_X),
				m_Pos.y+MODERN_CARD_POS_Y+MODERN_CARD_BUY_POS_Y+((i/IGS_NUM_ITEMS_HEIGHT)*MODERN_CARD_DISTANCE_Y),
				MODERN_CARD_WIDTH-(MODERN_CARD_IMG_INSET*2), MODERN_CARD_BUY_HEIGHT);
		}
		else
		{
			m_ViewDetailButton[i].ChangeButtonInfo((IMAGE_IGS_VIEWDETAIL_BTN_POS_X+ (DisplayWinExt-30)) +((i%IGS_NUM_ITEMS_WIDTH)*IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_X),IMAGE_IGS_VIEWDETAIL_BTN_POS_Y+((i/IGS_NUM_ITEMS_HEIGHT)*IMAGE_IGS_VIEWDETAIL_BTN_DISTANCE_Y),IMAGE_IGS_VIEWDETAIL_BTN_WIDTH, IMAGE_IGS_VIEWDETAIL_BTN_HEIGHT);
		}

		m_ViewDetailButton[i].MoveTextPos(0, -1);

		// The caption is engine-drawn in both skins now. Modern_BuyBtn.tga used
		// to carry "BUY" baked in, which only worked while the button was drawn
		// at the texture's own 52x26; at card width the lettering would stretch.
		m_ViewDetailButton[i].ChangeText(GlobalText[2886]);
	}

	/*
		Gift / charge / refresh.

		The mockup has no equivalent row, but these are live functions and
		dropping them would cost the player features to gain a resemblance, so
		they move down under the taller balance block instead of disappearing.
	*/
	int iIconBtnY = bModernSkin ? MODERN_ICON_BTN_POS_Y : IMAGE_IGS_ICON_BTN_POS_Y;

	m_CashGiftButton.ChangeButtonImgState(true, bModernSkin ? IMAGE_IGS_MODERN_ICON_GIFT : IMAGE_IGS_ITEMGIFT_BTN, true);
	m_CashGiftButton.ChangeButtonInfo(m_Pos.x+IMAGE_IGS_ITEMGIFT_BTN_POS_X, m_Pos.y+iIconBtnY, IMAGE_IGS_ICON_BTN_WIDTH, IMAGE_IGS_ICON_BTN_HEIGHT);
	m_CashGiftButton.ChangeToolTipText(GlobalText[2939]);
	m_CashChargeButton.ChangeButtonImgState(true, bModernSkin ? IMAGE_IGS_MODERN_ICON_CASH : IMAGE_IGS_CASHGIFT_BTN, true);
	m_CashChargeButton.ChangeButtonInfo(m_Pos.x+IMAGE_IGS_CASHGIFT_BTN_POS_X, m_Pos.y+iIconBtnY, IMAGE_IGS_ICON_BTN_WIDTH, IMAGE_IGS_ICON_BTN_HEIGHT);
	m_CashChargeButton.ChangeToolTipText(GlobalText[2940]);

	m_CashRefreshButton.ChangeButtonImgState(true, bModernSkin ? IMAGE_IGS_MODERN_ICON_REFRESH : IMAGE_IGS_REFRESH_BTN, true);
	m_CashRefreshButton.ChangeButtonInfo(m_Pos.x+IMAGE_IGS_REFRESH_BTN_POS_X, m_Pos.y+iIconBtnY, IMAGE_IGS_ICON_BTN_WIDTH, IMAGE_IGS_ICON_BTN_HEIGHT);
	m_CashRefreshButton.ChangeToolTipText(GlobalText[2941]);

	m_UseButton.ChangeButtonImgState(true, bModernSkin ? IMAGE_IGS_MODERN_BTN : IMAGE_IGS_VIEWDETAIL_BTN, true, false, true);
	m_UseButton.ChangeButtonInfo(
		m_Pos.x + (bModernSkin ? MODERN_USE_POS_X : IMAGE_IGS_USE_BTN_POS_X),
		m_Pos.y + (bModernSkin ? MODERN_USE_POS_Y : IMAGE_IGS_USE_BTN_POS_Y),
		bModernSkin ? MODERN_USE_WIDTH : IMAGE_IGS_VIEWDETAIL_BTN_WIDTH,
		bModernSkin ? MODERN_USE_HEIGHT : IMAGE_IGS_VIEWDETAIL_BTN_HEIGHT);
	m_UseButton.MoveTextPos(0, -1);
	m_UseButton.ChangeText(GlobalText[2887]);

	// Paging sits in the heading row in the modern skin - see the comment on
	// MODERN_PAGE_BTN_POS_Y for why the shelf needs the bottom of the frame.
	int iPagePrevX = bModernSkin ? MODERN_PAGE_PREV_POS_X : IMAGE_IGS_PAGE_LEFT_POS_X;
	int iPageNextX = bModernSkin ? MODERN_PAGE_NEXT_POS_X : IMAGE_IGS_PAGE_RIGHT_POS_X;
	int iPageBtnY  = bModernSkin ? MODERN_PAGE_BTN_POS_Y  : IMAGE_IGS_PAGE_BUTTON_POS_Y;

	m_PrevButton.ChangeButtonImgState(true, bModernSkin ? IMAGE_IGS_MODERN_ARROW_LEFT : IMAGE_IGS_PAGE_LEFT, true);
	m_PrevButton.ChangeButtonInfo(m_Pos.x+iPagePrevX, m_Pos.y+iPageBtnY,
		bModernSkin ? MODERN_PAGE_BTN_WIDTH : IMAGE_IGS_PAGE_BTN_WIDTH,
		bModernSkin ? MODERN_PAGE_BTN_HEIGHT : IMAGE_IGS_PAGE_BTN_HEIGHT);

	// next
	m_NextButton.ChangeButtonImgState(true, bModernSkin ? IMAGE_IGS_MODERN_ARROW_RIGHT : IMAGE_IGS_PAGE_RIGHT, true);
	m_NextButton.ChangeButtonInfo(m_Pos.x+iPageNextX, m_Pos.y+iPageBtnY,
		bModernSkin ? MODERN_PAGE_BTN_WIDTH : IMAGE_IGS_PAGE_BTN_WIDTH,
		bModernSkin ? MODERN_PAGE_BTN_HEIGHT : IMAGE_IGS_PAGE_BTN_HEIGHT);

	// Storage Page prev
	m_StoragePrevButton.ChangeButtonImgState(true, bModernSkin ? IMAGE_IGS_MODERN_ARROW_LEFT : IMAGE_IGS_STORAGE_PAGE_LEFT, true);
	m_StoragePrevButton.ChangeButtonInfo(
		m_Pos.x + (bModernSkin ? MODERN_STORAGE_PREV_POS_X : IMAGE_IGS_STORAGE_PAGE_LEFT_POS_X-12),
		m_Pos.y + (bModernSkin ? MODERN_STORAGE_ARROW_POS_Y : IMAGE_IGS_STORAGE_PAGE_BTN_POS_Y-3),
		bModernSkin ? MODERN_STORAGE_ARROW_WIDTH : IMGAE_IGS_STORAGE_PAGE_BTN_WIDTH,
		bModernSkin ? MODERN_STORAGE_ARROW_HEIGHT : IMGAE_IGS_STORAGE_PAGE_BTN_HEIGHT);
	
	// Storage Page next
	m_StorageNextButton.ChangeButtonImgState(true, bModernSkin ? IMAGE_IGS_MODERN_ARROW_RIGHT : IMAGE_IGS_STORAGE_PAGE_RIGHT, true);
	m_StorageNextButton.ChangeButtonInfo(
		m_Pos.x + (bModernSkin ? MODERN_STORAGE_NEXT_POS_X : IMAGE_IGS_STORAGE_PAGE_RIGHT_POS_X+10),
		m_Pos.y + (bModernSkin ? MODERN_STORAGE_ARROW_POS_Y : IMAGE_IGS_STORAGE_PAGE_BTN_POS_Y-3),
		bModernSkin ? MODERN_STORAGE_ARROW_WIDTH : IMGAE_IGS_STORAGE_PAGE_BTN_WIDTH,
		bModernSkin ? MODERN_STORAGE_ARROW_HEIGHT : IMGAE_IGS_STORAGE_PAGE_BTN_HEIGHT);
}

/*
	Push m_Pos out to every child widget.

	The shop was written on the assumption that it never moves: the buttons are
	placed once in SetBtnInfo, and several sites used DisplayWinExt - the
	window's own x - directly instead of m_Pos. Dragging turns all of that into
	state that has to be refreshed, so this is the one place that does it.

	The radio groups are repositioned with ChangeRadioButtonInfo rather than
	rebuilt with Init*Btn, because rebuilding calls ChangeFrame(0) and would
	snap the player back to the first zone and category mid-drag.

	The storage list sets its own position in its constructor
	(UIControls.cpp:6901) and knows nothing about the window, so it needs telling
	explicitly or it stays behind.
*/
void CNewUIInGameShop::ApplyWindowPos()
{
	// SetBtnInfo rebuilds the storage tab group and selects Storage, so the
	// player's tab is saved across it - dragging the window should not quietly
	// throw them off Gift Inventory.
	int iTabSel = m_ListBoxTabButton.GetCurButtonIndex();

	SetBtnInfo();

	if( iTabSel >= 0 )
		m_ListBoxTabButton.ChangeFrame(iTabSel);

	bool bModern = IsModernSkin();

	m_ZoneButton.ChangeRadioButtonInfo(true,
		m_Pos.x + (bModern ? 110 : IMAGE_IGS_ZONE_BTN_POS_X),
		m_Pos.y + (bModern ? 4 : IMAGE_IGS_ZONE_BTN_POS_Y),
		IMAGE_IGS_ZONE_BTN_WIDTH, IMAGE_IGS_ZONE_BTN_HEIGHT);

	m_CategoryButton.ChangeRadioButtonInfo(false,
		m_Pos.x + (bModern ? 0 : IMAGE_IGS_CATEGORY_BTN_POS_X),
		m_Pos.y + (bModern ? MODERN_CATEGORY_POS_Y : IMAGE_IGS_CATEGORY_BTN_POS_Y),
		bModern ? MODERN_SIDEBAR_WIDTH : IMAGE_IGS_CATEGORY_BTN_WIDTH,
		IMAGE_IGS_CATEGORY_BTN_HEIGHT, bModern ? 0 : IMAGE_IGS_CATEGORY_BTN_DISTANCE);

	m_StorageItemListBox.SetPosition(m_Pos.x + 490, m_Pos.y + 360);
}

/*
	Drag the shop around by its header strip.

	Modern skin only. The legacy frame has no bar that reads as a grab handle,
	and its layout constants assume the original origin in more places than are
	worth chasing for a skin nobody is asking to move.

	The zone tabs live inside the header now, so their band is carved out of the
	handle - otherwise grabbing a currency tab would drag the window instead of
	switching zones. Everything else in the strip (the title, the balances) is
	inert, so it all grabs.

	Returns true while it owns the mouse, which keeps UpdateMouseEvent from
	handing the same click to a button underneath.
*/
bool CNewUIInGameShop::UpdateWindowDrag()
{
	if( IsModernSkin() == false )
		return false;

	if( m_bDraggingWindow )
	{
		if( SEASON3B::IsPress(VK_LBUTTON) == false && SEASON3B::IsRepeat(VK_LBUTTON) == false )
		{
			m_bDraggingWindow = false;
			return true;
		}

		int iNewX = MouseX - m_ptDragGrab.x;
		int iNewY = MouseY - m_ptDragGrab.y;

		/*
			Always leave a strip of the header on screen.

			Without this the window can be dragged far enough off an edge that
			the only thing that could drag it back - the header - is unreachable,
			and the shop is stuck until the client restarts.
		*/
		const int iKeepVisible = 80;
		int iMaxX = DisplayWin - iKeepVisible;
		int iMinX = iKeepVisible - IMAGE_IGS_BACK_WIDTH;
		int iMaxY = DisplayHeight - MODERN_HEADER_HEIGHT;

		if( iNewX < iMinX ) iNewX = iMinX;
		if( iNewX > iMaxX ) iNewX = iMaxX;
		if( iNewY < 0 ) iNewY = 0;
		if( iNewY > iMaxY ) iNewY = iMaxY;

		if( iNewX != m_Pos.x || iNewY != m_Pos.y )
		{
			SetPos(iNewX, iNewY);
			ApplyWindowPos();
		}

		return true;
	}

	if( SEASON3B::IsPress(VK_LBUTTON) == false )
		return false;

	if( SEASON3B::CheckMouseIn(m_Pos.x, m_Pos.y, IMAGE_IGS_BACK_WIDTH, MODERN_HEADER_HEIGHT) == false )
		return false;

	int iZoneCount = g_InGameShopSystem->GetSizeZones();

	if( iZoneCount > 0
		&& SEASON3B::CheckMouseIn(m_Pos.x+110, m_Pos.y+4,
			IMAGE_IGS_ZONE_BTN_WIDTH*iZoneCount, IMAGE_IGS_ZONE_BTN_HEIGHT) )
		return false;

	m_bDraggingWindow = true;
	m_ptDragGrab.x = MouseX - m_Pos.x;
	m_ptDragGrab.y = MouseY - m_Pos.y;

	return true;
}

bool CNewUIInGameShop::Update()
{
	if( IsVisible() == false )
		return true;

	// A banner fetched on the worker thread is applied here, because InitBanner
	// loads a texture and that has to happen on the render thread.
	unicode::t_char szBannerPath[MAX_PATH] = {0};
	int iBannerId = -1;

	if( g_InGameShopSystem->PollCustomBannerDownload(szBannerPath, sizeof(szBannerPath), iBannerId) )
	{
		g_InGameShopSystem->SetLastAppliedBannerId(iBannerId);
		InitBanner(szBannerPath, "#");
	}

	return true;
}

bool CNewUIInGameShop::UpdateMouseEvent()
{
	if(IsVisible() == false)
		return true;

	if( UpdateWindowDrag() )
		return false;

	if( BtnProcess() )
		return false;

	if( UpdateBanner() )
		return false;
		
	if(SEASON3B::CheckMouseIn(m_Pos.x, m_Pos.y, IMAGE_IGS_BACK_WIDTH, IMAGE_IGS_BACK_HEIGHT))
	{
		m_StorageItemListBox.DoAction();

		if(SEASON3B::IsPress(VK_RBUTTON))
		{
			MouseRButton = false;
			MouseRButtonPop = false;
			MouseRButtonPush = false;
			return false;
		}
		
		if(SEASON3B::IsNone(VK_LBUTTON) == false)
		{
			return false;
		}

		return false;
	}

	return true;
}

bool CNewUIInGameShop::UpdateKeyEvent()
{
	if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INGAMESHOP) == true)
	{
		if(SEASON3B::IsPress(VK_ESCAPE) == true)
		{
			SendRequestIGS_CashShopOpen(1);
			g_pNewUISystem->Hide(SEASON3B::INTERFACE_INGAMESHOP);

			return false;
		}
	}
	return true;
}

bool CNewUIInGameShop::IsInGameShopOpen()
{
	//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt CallStack - CNewUIInGameShop::IsInGameShopOpen()");
	if( Hero->Movement )
		return false;

	if( !(Hero->SafeZone) && !(WD_0LORENCIA == gMapManager.WorldActive && WD_3NORIA == gMapManager.WorldActive && WD_2DEVIAS == gMapManager.WorldActive && WD_51HOME_6TH_CHAR == gMapManager.WorldActive))
	{
		CMsgBoxIGSCommon* pMsgBox = NULL;
		CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSCommonLayout), &pMsgBox);
		pMsgBox->Initialize(GlobalText[3028], GlobalText[3051]);
		//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt Return - false <%s>", GlobalText[3051]);
		return false;
	}

	if( g_InGameShopSystem->IsShopOpen() == false)
	{
		CMsgBoxIGSCommon* pMsgBox = NULL;
		CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSCommonLayout), &pMsgBox);
		pMsgBox->Initialize(GlobalText[3028], GlobalText[3035]);
		//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt Return - false <%s>", GlobalText[3035]);
		return false;
	}
	//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt Return - true");
	return true;
}

bool CNewUIInGameShop::IsInGameShop()
{
	if(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INGAMESHOP))
		return true;
	else
		return false;
}

void CNewUIInGameShop::InitBanner(unicode::t_char* pszFileName, unicode::t_char* pszBannerURL)
{
	ReleaseBanner();

	if(pszFileName == NULL) 
		return;

	if(pszBannerURL[0] != '#')
	{
		m_bBannerLink = true;
	}

	if( Bitmaps.Convert_Format(pszFileName) == false)
		return;

	if( LoadBitmap(pszFileName, IMAGE_IGS_BANNER, GL_LINEAR, GL_CLAMP, true, true) == true)
	{
		m_bLoadBanner = true;

		strcpy(m_szBannerURL, pszBannerURL);
	}
}

void CNewUIInGameShop::RenderBanner()
{
	if( m_bLoadBanner == false )
		return;

	if( IsModernSkin() )
	{
		/*
			The mockup's promo strip: full content width, directly above the
			shelf, where Phase 6 had kept the legacy 153x63 box on the right rail.

			Uploaded art is whatever aspect the operator chose - the one in use is
			nearly square - so it is fitted inside the strip rather than stretched
			across it. A logo squashed to 9:1 would be worse than the old box.
		*/
		int bx, by, bw, bh;
		GetModernBannerRect(bx, by, bw, bh);

		RenderFillRect(bx, by, bw, bh, 0.16f, 0.07f, 0.07f, 1.0f);
		RenderFillRect(bx, by, 3, bh, 0.85f, 0.16f, 0.13f, 1.0f);

		int iArtX = bx+8;
		int iArtY = by+3;
		int iArtW = bw-16;
		int iArtH = bh-6;

		BITMAP_t* pBannerArt = Bitmaps.FindTexture(IMAGE_IGS_BANNER);

		if( pBannerArt != NULL && pBannerArt->Width > 0.0f && pBannerArt->Height > 0.0f )
		{
			// The image's own size, falling back to the texture's when a loader
			// did not record it. Fitting by the padded size would leave the art
			// smaller than the space and off-centre.
			float fSrcW = (pBannerArt->SourceWidth > 0.0f) ? pBannerArt->SourceWidth : pBannerArt->Width;
			float fSrcH = (pBannerArt->SourceHeight > 0.0f) ? pBannerArt->SourceHeight : pBannerArt->Height;

			float fScale = (float)iArtH / fSrcH;

			if( (fSrcW * fScale) > (float)iArtW )
				fScale = (float)iArtW / fSrcW;

			int iFitW = (int)(fSrcW * fScale);
			int iFitH = (int)(fSrcH * fScale);

			iArtX += (iArtW - iFitW) / 2;
			iArtY += (iArtH - iFitH) / 2;
			iArtW = iFitW;
			iArtH = iFitH;
		}

		/*
			Only the image, not the padding around it.

			The loader rounds a texture up to a power of two - a 153x63 banner
			lives in a 256x64 texture - so drawing the full 0..1 UV range draws
			40% margin as well. SourceWidth/Height are the real pixels; they are
			0 for anything an older loader made, and the full range is the right
			answer then.
		*/
		float fUW = 1.0f;
		float fVH = 1.0f;

		if( pBannerArt != NULL && pBannerArt->SourceWidth > 0.0f && pBannerArt->SourceHeight > 0.0f
			&& pBannerArt->Width > 0.0f && pBannerArt->Height > 0.0f )
		{
			fUW = pBannerArt->SourceWidth / pBannerArt->Width;
			fVH = pBannerArt->SourceHeight / pBannerArt->Height;
		}

		/*
			V is passed inverted (start at fVH, span -fVH) - that is what puts the
			uploaded banner the right way up.

			MU's own .OZJ assets are stored PRE-FLIPPED vertically: they were made by
			converting bottom-up TGAs to JPEG without re-ordering the rows, so
			OpenJpeg's straight top-down copy renders them upright, and every other
			OZJ in the game therefore looks correct.

			An operator-uploaded banner is an ordinary, natural-orientation JPEG.
			Convert_Format only prepends 24 dummy header bytes to make it an .OZJ
			(GlobalBitmap.cpp) - it does not, and cheaply cannot, re-order the rows -
			so this one texture arrives upside down relative to every other OZJ and
			needs the flip here instead.

			Done at draw time rather than in the loader because IMAGE_IGS_BANNER only
			ever holds uploaded art: every InitBanner caller passes a downloaded path.
			Flipping inside OpenJpeg would invert the entire game's textures.

			NOT an EXIF problem, and the EXIF path cannot fix it: the banner in use
			reports Orientation = 1, so ApplyExifOrientationRGB correctly does nothing.

			Mobile only. The PC build draws the same upload upright with the plain
			0..fVH range, and the inverted range turned it upside down there - so
			whatever makes mobile need the flip is not shared with desktop.
		*/
#if defined(__ANDROID__) || defined(MU_IOS)
		RenderImage(IMAGE_IGS_BANNER, (float)iArtX, (float)iArtY, (float)iArtW, (float)iArtH, 0.0f, fVH, fUW, -fVH);
#else
		RenderImage(IMAGE_IGS_BANNER, (float)iArtX, (float)iArtY, (float)iArtW, (float)iArtH, 0.0f, 0.0f, fUW, fVH);
#endif
	}
	else
	{
		RenderImage(IMAGE_IGS_BANNER, IMAGE_IGS_BANNER_POS_X + DisplayWinExt, IMAGE_IGS_BANNER_POS_Y, IMAGE_IGS_BANNER_WIDTH, IMAGE_IGS_BANNER_HEIGHT);
	}

	/*
		UpdateBanner has always opened m_szBannerURL on a click, but nothing on
		screen said the banner was clickable - a banner with a # URL and one with a
		real one looked identical. A hover highlight is the missing half of that.

		Only in custom mode: the original shop is left exactly as it was.
	*/
	if( IsModernSkin() == false || (m_bBannerLink == false && m_iBannerTargetPackage <= 0) )
		return;

	int hx, hy, hw, hh;
	GetModernBannerRect(hx, hy, hw, hh);

	if( SEASON3B::CheckMouseIn(hx, hy, hw, hh) == false )
		return;

	float fX = (float)hx;
	float fY = (float)hy;
	float fW = (float)hw;
	float fH = (float)hh;

	// A 1px frame rather than a wash over the art - the banner is the message,
	// and tinting it would fight whatever it was drawn to say.
	EnableAlphaTest(true);
	glColor4f(1.0f, 0.90f, 0.55f, 0.85f);
	RenderColor(fX, fY, fW, 1.0f, 0.0f, 0);
	RenderColor(fX, fY + fH - 1.0f, fW, 1.0f, 0.0f, 0);
	RenderColor(fX, fY, 1.0f, fH, 0.0f, 0);
	RenderColor(fX + fW - 1.0f, fY, 1.0f, fH, 0.0f, 0);
	EndRenderColor();
	glColor3f(1.0f, 1.0f, 1.0f);
	EnableAlphaTest(false);
}

bool CNewUIInGameShop::UpdateBanner()
{
	if( m_bLoadBanner == false || (m_bBannerLink == false && m_iBannerTargetPackage <= 0) )
		return false;

	int cx = IMAGE_IGS_BANNER_POS_X + DisplayWinExt;
	int cy = IMAGE_IGS_BANNER_POS_Y;
	int cw = IMAGE_IGS_BANNER_WIDTH;
	int ch = IMAGE_IGS_BANNER_HEIGHT;

	// The clickable area has to follow the banner, or the modern skin would
	// open the link from the empty rail slot the banner no longer occupies.
	if( IsModernSkin() )
		GetModernBannerRect(cx, cy, cw, ch);

	if( (SEASON3B::IsPress(VK_LBUTTON))
		&& (SEASON3B::CheckMouseIn(cx, cy, cw, ch)))
	{
		/*
			A banner that names a package jumps to it; one that names a URL still
			opens the browser, which is all this ever did.

			The package wins when both are set: pulling someone out of the game to
			a web page is the more disruptive of the two, so it needs to be the
			deliberate choice rather than the default.
		*/
		if( m_iBannerTargetPackage > 0 && JumpToPackage(m_iBannerTargetPackage) )
			return true;

		leaf::OpenExplorer(m_szBannerURL);
		return true; 
	}
	return false;
}

/*
	Select the tab a package lives on, so a banner click lands the player in
	front of what it was advertising.

	Category buttons are addressed by position, not by category id, so the id
	has to be walked back to an index - GetCategorySeqIndexByIndex is the only
	mapping there is. Selecting the tab is as far as this goes: the shelf has no
	notion of a highlighted card, and inventing one would be a bigger change
	than the banner is worth.
*/
bool CNewUIInGameShop::JumpToPackage(int iPackageSeq)
{
	int iIndex = g_InGameShopSystem->GetCategoryButtonIndexOfPackage(iPackageSeq);

	if( iIndex < 0 )
		return false;

	m_CategoryButton.ChangeFrame(iIndex);
	g_InGameShopSystem->SelectCategory(iIndex);

	return true;
}

void CNewUIInGameShop::ReleaseBanner()
{
	if( m_bLoadBanner == false )
		return;

	DeleteBitmap(IMAGE_IGS_BANNER);

	m_bLoadBanner = false;
}

void CNewUIInGameShop::OpeningProcess()
{

	//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt CallStack - CNewUIInGameShop::OpeningProcess()");
	PlayBuffer(SOUND_CLICK01);
	g_InGameShopSystem->Initalize();
	g_InGameShopSystem->SelectZone(0);
	InitZoneBtn();
	g_InGameShopSystem->SelectCategory(0);
	InitCategoryBtn();
	g_InGameShopSystem->SetRequestEventPackge();
} 

void CNewUIInGameShop::ClosingProcess()
{
	PlayBuffer(SOUND_CLICK01);
	m_ListBoxTabButton.ChangeFrame(IGS_SAFEKEEPING_LISTBOX);
	ClearAllStorageItem();
}

void CNewUIInGameShop::InitZoneBtn()
{
	m_ZoneButton.UnRegisterRadioButton();

	if (g_InGameShopSystem->GetSizeZones() == 0)
		return;

	m_ZoneButton.UnRegisterRadioButton();
	/*
		The currency-zone tabs.

		At their legacy y=0 they would sit on top of the modern header strip, so
		in that skin they move into the header proper, between the title and the
		balances. There is room for two at 76px each before they would reach
		MODERN_CUR_FIRST_POS_X; a third zone would run under the balances.
	*/
	bool bModernZone = IsModernSkin();
	int iZoneX = bModernZone ? 110 : IMAGE_IGS_ZONE_BTN_POS_X;
	int iZoneY = bModernZone ? 4 : IMAGE_IGS_ZONE_BTN_POS_Y;

	m_ZoneButton.CreateRadioGroup(g_InGameShopSystem->GetSizeZones(), bModernZone ? IMAGE_IGS_MODERN_ZONE_BTN : IMAGE_IGS_ZONE_BTN);
	m_ZoneButton.ChangeRadioButtonInfo(true, m_Pos.x+iZoneX, m_Pos.y+iZoneY,IMAGE_IGS_ZONE_BTN_WIDTH, IMAGE_IGS_ZONE_BTN_HEIGHT);
	m_ZoneButton.SetFont(g_hFontBold);
	type_listName ZoneNameList = g_InGameShopSystem->GetZoneName();
	m_ZoneButton.ChangeRadioText(ZoneNameList);
	m_ZoneButton.ChangeFrame(0);
}

void CNewUIInGameShop::InitCategoryBtn()
{
	m_CategoryButton.UnRegisterRadioButton();

	if( g_InGameShopSystem->GetSizeCategoriesAsSelectedZone() == 0 )
		return;

	m_CategoryButton.UnRegisterRadioButton();

	// Modern skin: same button, different texture. The red "selected" look is
	// baked into IMAGE_IGS_MODERN_CATEGORY_BTN's own 3rd frame, same technique
	// the legacy Ingame_Bt01.tga already uses for its own selected state.
	int iCategoryBtnImg = (gProtect.m_MainInfo.CustomCashShop != 0) ? IMAGE_IGS_MODERN_CATEGORY_BTN : IMAGE_IGS_CATEGORY_BTN;
	// The modern sidebar is a full-bleed column: rows run edge to edge like the
	// mockup instead of floating inset the way the ornate plates did.
	bool bModernSidebar = IsModernSkin();
	int iCategoryX = bModernSidebar ? 0 : IMAGE_IGS_CATEGORY_BTN_POS_X;
	int iCategoryW = bModernSidebar ? MODERN_SIDEBAR_WIDTH : IMAGE_IGS_CATEGORY_BTN_WIDTH;

	m_CategoryButton.CreateRadioGroup(g_InGameShopSystem->GetSizeCategoriesAsSelectedZone(), iCategoryBtnImg, true);
	int iCategoryY = bModernSidebar ? MODERN_CATEGORY_POS_Y : IMAGE_IGS_CATEGORY_BTN_POS_Y;

	// Flush rows in the modern sidebar. The legacy 6px gap let the background
	// through between the ornate plates, which was the point of the deco pieces
	// that filled it; on a flat column it just reads as a broken list.
	int iCategoryGap = bModernSidebar ? 0 : IMAGE_IGS_CATEGORY_BTN_DISTANCE;

	m_CategoryButton.ChangeRadioButtonInfo(false, m_Pos.x+iCategoryX, m_Pos.y+iCategoryY,iCategoryW, IMAGE_IGS_CATEGORY_BTN_HEIGHT, iCategoryGap);
	m_CategoryButton.ChangeButtonState( SEASON3B::BUTTON_STATE_DOWN, 2 );
	m_CategoryButton.SetFont(g_hFontBold);
	type_listName CategoryNameList = g_InGameShopSystem->GetCategoryName();
	m_CategoryButton.ChangeRadioText(CategoryNameList);
	m_CategoryButton.ChangeFrame(0);
}

void CNewUIInGameShop::AddStorageItem(int iStorageSeq, int iStorageItemSeq, int iStorageGroupCode, int iProductSeq, int iPriceSeq, int iCashPoint,unicode::t_char chItemType, unicode::t_char* pszUserName /* = NULL */, unicode::t_char* pszMessage /* = NULL */)
{
	int iValue = -1;
	unicode::t_char szText[MAX_TEXT_LENGTH] = {'\0', };
	IGS_StorageItem Item;

	Item.m_bIsSelected = FALSE;
	Item.m_iStorageSeq = iStorageSeq;
	Item.m_iStorageItemSeq = iStorageItemSeq;
	Item.m_iStorageGroupCode = iStorageGroupCode;
	Item.m_iProductSeq = iProductSeq;
	Item.m_iPriceSeq = iPriceSeq;
	Item.m_iCashPoint = iCashPoint;
	Item.m_iNum = 1;
	Item.m_szType = chItemType;
	Item.m_wItemCode = -1;

	if( pszUserName == NULL )
	{
		Item.m_szSendUserName[0] = '\0';
	}
	else 
	{
		strcpy(Item.m_szSendUserName, pszUserName);
	}

	if( pszMessage == NULL )
	{
		Item.m_szMessage[0] = '\0';
	}
	else
	{
		strcpy(Item.m_szMessage, pszMessage);
	}
	
	if( chItemType == 'C' || chItemType == 'c')
	{
		unicode::t_char szValue[MAX_TEXT_LENGTH] = {'\0', };
		ConvertGold(iCashPoint, szValue);
		// Name
		sprintf(Item.m_szName, GlobalText[3050], szValue);
		
		// Num
		sprintf(Item.m_szNum, GlobalText[3043], szValue);
		Item.m_iNum = iCashPoint;
		
		// Period
		sprintf(Item.m_szPeriod, "-");
	}
	else if(chItemType == 'P' || chItemType == 'p')
	{
		if( iPriceSeq > 0)
		{
			// Name
			if( g_InGameShopSystem->GetProductInfoFromPriceSeq(iProductSeq, iPriceSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_ITEMNAME, iValue, Item.m_szName) == false)
			{
				sprintf(Item.m_szName, "aaa");
			}
			
			g_InGameShopSystem->GetProductInfoFromPriceSeq(iProductSeq, iPriceSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_NUM, iValue, szText);
			if( iValue > 0 )
			{
				sprintf(Item.m_szNum, "%d %s", iValue, szText);
				Item.m_iNum = iValue;
			}
			else
			{
				sprintf(Item.m_szNum, "-");
			}
			
			// Period
			g_InGameShopSystem->GetProductInfoFromPriceSeq(iProductSeq, iPriceSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_USE_LIMIT_PERIOD, iValue, szText);
			if( iValue > 0 )
			{
				sprintf(Item.m_szPeriod, "%d %s", iValue, szText);
			}
			else
			{
				sprintf(Item.m_szPeriod, "-");
			}
			
			g_InGameShopSystem->GetProductInfoFromPriceSeq(iProductSeq, iPriceSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_ITEMCODE, iValue, szText);
			Item.m_wItemCode = iValue;	
		}
		else
		{
			if( g_InGameShopSystem->GetProductInfoFromProductSeq(iProductSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_ITEMNAME, iValue, Item.m_szName) == false	)
				return;
			
			// Num
			g_InGameShopSystem->GetProductInfoFromProductSeq(iProductSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_NUM, iValue, szText);
			if( iValue > 0 )
			{
				sprintf(Item.m_szNum, "%d %s", iValue, szText);
				Item.m_iNum = iValue;
			}
			else
			{
				sprintf(Item.m_szNum, "-");
			}
			
			// Period
			g_InGameShopSystem->GetProductInfoFromProductSeq(iProductSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_USE_LIMIT_PERIOD, iValue, szText);
			if( iValue > 0 )
			{
				sprintf(Item.m_szPeriod, "%d %s", iValue, szText);
			}
			else
			{
				sprintf(Item.m_szPeriod, "-");
			}
			
			g_InGameShopSystem->GetProductInfoFromProductSeq(iProductSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_ITEMCODE, iValue, szText);
			Item.m_wItemCode = iValue;
		}
	}
	else 
	{
		return;
	}

	m_iStorageCurrentPageReceiveItemCnt++;

	m_StorageItemListBox.AddText(Item);

	if( m_iStorageCurrentPageReceiveItemCnt >= m_iStorageCurrentPageItemCnt )
	{
		if( m_iSelectedStorageItemIndex > m_iStorageCurrentPageItemCnt )
		{
			m_StorageItemListBox.SLSetSelectLine(m_iStorageCurrentPageItemCnt);
		}
		else
		{
			m_StorageItemListBox.SLSetSelectLine(m_iSelectedStorageItemIndex);
		}
	}
}

void CNewUIInGameShop::ClearAllStorageItem()
{
	m_iStorageTotalItemCnt			= 0;
	m_iStorageCurrentPageItemCnt	= 0;
	m_iStorageTotalPage				= 0;
	m_iStorageCurrentPage			= 0;
	m_iStorageCurrentPageReceiveItemCnt = 0;
	m_StorageItemListBox.Clear();
}

void CNewUIInGameShop::InitStorage( int iTotalItemCnt, int iCurrentPageItemCnt, int iTotalPage, int iCurrentPage )
{
	ClearAllStorageItem();
	
	m_iStorageTotalItemCnt			= iTotalItemCnt;
	m_iStorageCurrentPageItemCnt	= iCurrentPageItemCnt;
	m_iStorageTotalPage				= iTotalPage;

	if( m_iStorageTotalPage > 0 )
	{
		m_iStorageCurrentPage		= iCurrentPage;
	}
	else
	{
		m_iStorageCurrentPage		= 0;
	}

	if( m_iSelectedStorageItemIndex == 0 || m_bRequestCurrentPage == false)
	{
		m_iSelectedStorageItemIndex = iCurrentPageItemCnt;
	}

	m_bRequestCurrentPage = false;
}

char CNewUIInGameShop::GetCurrentStorageCode()
{
	char szCode;
	switch(m_ListBoxTabButton.GetCurButtonIndex())
	{
	case IGS_SAFEKEEPING_LISTBOX:
		szCode = 'S';
		break;
	case IGS_PRESENTBOX_LISTBOX:
		szCode = 'G';
		break;
	default:
		szCode = 'Z';
		break;
	}
	return szCode;
}

void CNewUIInGameShop::StoragePrevPage()
{
	if( m_iStorageCurrentPage > 1 )
	{
		char szCode = GetCurrentStorageCode();
		m_iSelectedStorageItemIndex = 0;
		m_bRequestCurrentPage = true;
		SendRequestIGS_ItemStorageList(m_iStorageCurrentPage-1, &szCode);
	}
}

void CNewUIInGameShop::StorageNextPage()
{
	if( m_iStorageCurrentPage < m_iStorageTotalPage )
	{
		char szCode = GetCurrentStorageCode();
		m_iSelectedStorageItemIndex = 0;
		m_bRequestCurrentPage = true;
		SendRequestIGS_ItemStorageList(m_iStorageCurrentPage+1, &szCode);
	}
}

void CNewUIInGameShop::UpdateStorageItemList()
{
	char szCode = GetCurrentStorageCode();
	int iSelectLineIndex = m_StorageItemListBox.SLGetSelectLineNum();
	m_bRequestCurrentPage = true;

	if( (m_iStorageCurrentPageItemCnt == 1) && (m_iStorageTotalPage > 1))
	{
		m_iSelectedStorageItemIndex = 1;
		SendRequestIGS_ItemStorageList(m_iStorageCurrentPage-1, &szCode);
	}
	else if(iSelectLineIndex == 1)
	{
		m_iSelectedStorageItemIndex = iSelectLineIndex;
		SendRequestIGS_ItemStorageList(m_iStorageCurrentPage, &szCode);
	}
	else if( m_iStorageCurrentPageItemCnt < IGS_STORAGE_TOTAL_ITEM_PER_PAGE )
	{
		m_iSelectedStorageItemIndex = (iSelectLineIndex-1);
		SendRequestIGS_ItemStorageList(m_iStorageCurrentPage, &szCode);
	}
	else 
	{
		m_iSelectedStorageItemIndex = iSelectLineIndex;
		SendRequestIGS_ItemStorageList(m_iStorageCurrentPage, &szCode);
	}
}

void CNewUIInGameShop::LoadImages()
{
	LoadBitmap("Interface\\newui_exit_00.tga", IMAGE_IGS_EXIT_BTN, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_shopback.jpg", IMAGE_IGS_BACK, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_Bt01.tga", IMAGE_IGS_CATEGORY_BTN, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_Deco_Center.tga", IMAGE_IGS_CATEGORY_DECO_MIDDLE, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_Deco_Dn.tga", IMAGE_IGS_CATEGORY_DECO_DOWN, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\ingame_Tab01.tga", IMAGE_IGS_LEFT_TAB, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\ingame_Tab02.tga", IMAGE_IGS_RIGHT_TAB, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_Tab_Up.tga", IMAGE_IGS_ZONE_BTN, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_Bt_Gift.tga", IMAGE_IGS_ITEMGIFT_BTN, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_Bt_Cash.tga", IMAGE_IGS_CASHGIFT_BTN, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_Bt_Reset.tga", IMAGE_IGS_REFRESH_BTN, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_Bt03.tga", IMAGE_IGS_VIEWDETAIL_BTN, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_Itembox_logo.tga", IMAGE_IGS_ITEMBOX_LOGO, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\ingame_Bt_page_L.tga", IMAGE_IGS_PAGE_LEFT, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\ingame_Bt_page_R.tga", IMAGE_IGS_PAGE_RIGHT, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\IGS_Storage_Page.tga", IMAGE_IGS_STORAGE_PAGE, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\IGS_Storage_Page_Left.tga", IMAGE_IGS_STORAGE_PAGE_LEFT, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\IGS_Storage_Page_Right.tga", IMAGE_IGS_STORAGE_PAGE_RIGHT, GL_LINEAR);

	// Modern flat-dark skin. Only loaded when custom mode is on, so legacy-mode
	// players pay zero extra load/VRAM cost and the legacy branch elsewhere in
	// this class stays byte-for-byte unchanged.
	if( gProtect.m_MainInfo.CustomCashShop != 0 )
	{
		LoadBitmap("Interface\\InGameShop\\Modern_Sidebar.tga", IMAGE_IGS_MODERN_CATEGORY_BTN, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_BuyBtn.tga", IMAGE_IGS_MODERN_VIEWDETAIL_BTN, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_CardPanel.tga", IMAGE_IGS_MODERN_CARD_PANEL, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_BannerBG.tga", IMAGE_IGS_MODERN_BANNER_BG, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_TabLeft.tga", IMAGE_IGS_MODERN_TAB_LEFT, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_TabRight.tga", IMAGE_IGS_MODERN_TAB_RIGHT, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_Btn.tga", IMAGE_IGS_MODERN_BTN, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_BtnRed.tga", IMAGE_IGS_MODERN_BTN_RED, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_ZoneBtn.tga", IMAGE_IGS_MODERN_ZONE_BTN, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_Close.tga", IMAGE_IGS_MODERN_CLOSE_BTN, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_ArrowL.tga", IMAGE_IGS_MODERN_ARROW_LEFT, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_ArrowR.tga", IMAGE_IGS_MODERN_ARROW_RIGHT, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_IconGift.tga", IMAGE_IGS_MODERN_ICON_GIFT, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_IconCash.tga", IMAGE_IGS_MODERN_ICON_CASH, GL_LINEAR);
		LoadBitmap("Interface\\InGameShop\\Modern_IconRefresh.tga", IMAGE_IGS_MODERN_ICON_REFRESH, GL_LINEAR);
	}
}

void CNewUIInGameShop::UnloadImages()
{
	DeleteBitmap(IMAGE_IGS_EXIT_BTN);
	DeleteBitmap(IMAGE_IGS_BACK);
	DeleteBitmap(IMAGE_IGS_CATEGORY_BTN);
	DeleteBitmap(IMAGE_IGS_CATEGORY_DECO_MIDDLE);
	DeleteBitmap(IMAGE_IGS_CATEGORY_DECO_DOWN);
	DeleteBitmap(IMAGE_IGS_LEFT_TAB);
	DeleteBitmap(IMAGE_IGS_RIGHT_TAB);
	DeleteBitmap(IMAGE_IGS_ZONE_BTN);
	DeleteBitmap(IMAGE_IGS_ITEMGIFT_BTN);
	DeleteBitmap(IMAGE_IGS_CASHGIFT_BTN);
	DeleteBitmap(IMAGE_IGS_REFRESH_BTN);
	DeleteBitmap(IMAGE_IGS_VIEWDETAIL_BTN);
	DeleteBitmap(IMAGE_IGS_ITEMBOX_LOGO);
	DeleteBitmap(IMAGE_IGS_PAGE_LEFT);
	DeleteBitmap(IMAGE_IGS_PAGE_RIGHT);
	DeleteBitmap(IMAGE_IGS_STORAGE_PAGE);
	DeleteBitmap(IMAGE_IGS_STORAGE_PAGE_LEFT);
	DeleteBitmap(IMAGE_IGS_STORAGE_PAGE_RIGHT);

	// Safe even in legacy mode - UnloadImage() no-ops on an index that was
	// never loaded (map lookup miss), so these never need their own gate.
	DeleteBitmap(IMAGE_IGS_MODERN_CATEGORY_BTN);
	DeleteBitmap(IMAGE_IGS_MODERN_VIEWDETAIL_BTN);
	DeleteBitmap(IMAGE_IGS_MODERN_CARD_PANEL);
	DeleteBitmap(IMAGE_IGS_MODERN_BANNER_BG);
	DeleteBitmap(IMAGE_IGS_MODERN_TAB_LEFT);
	DeleteBitmap(IMAGE_IGS_MODERN_TAB_RIGHT);
	DeleteBitmap(IMAGE_IGS_MODERN_BTN);
	DeleteBitmap(IMAGE_IGS_MODERN_BTN_RED);
	DeleteBitmap(IMAGE_IGS_MODERN_ZONE_BTN);
	DeleteBitmap(IMAGE_IGS_MODERN_CLOSE_BTN);
	DeleteBitmap(IMAGE_IGS_MODERN_ARROW_LEFT);
	DeleteBitmap(IMAGE_IGS_MODERN_ARROW_RIGHT);
	DeleteBitmap(IMAGE_IGS_MODERN_ICON_GIFT);
	DeleteBitmap(IMAGE_IGS_MODERN_ICON_CASH);
	DeleteBitmap(IMAGE_IGS_MODERN_ICON_REFRESH);
}

#endif //PBG_ADD_INGAMESHOP_UI_ITEMSHOP