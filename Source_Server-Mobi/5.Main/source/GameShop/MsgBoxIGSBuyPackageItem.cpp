// MsgBoxIGSBuyPackageItem.cpp: implementation of the CMsgBoxIGSBuyPackageItem class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM

#include "MsgBoxIGSBuyPackageItem.h"
#include "MsgBoxIGSCommon.h"	// IGSIsModernSkin, IGSFillRect, IGSRenderModernPanel

#include "UsefulDef.h"
#include "DSPlaySound.h"
#include "NewUISystem.h"
#include "InGameShopSystem.h"
#include "MsgBoxIGSBuyConfirm.h"
#include "MsgBoxIGSSendGift.h"

CMsgBoxIGSBuyPackageItem::CMsgBoxIGSBuyPackageItem()
{
	m_iPackageSeq	= 0;
	m_iDisplaySeq	= 0;
	m_iPriceSeq		= 0;
	m_wItemCode		= -1;
	m_iCashType		= 0;
	m_szPackageName[0]	= '\0';
	m_szPrice[0]		= '\0';
	m_szPeriod[0]		= '\0';

	for(int i=0 ; i<UIMAX_TEXT_LINE ; i++ )
	{
		m_szDescription[i][0] = '\0';
	}
}

CMsgBoxIGSBuyPackageItem::~CMsgBoxIGSBuyPackageItem()
{
	Release();
}

bool CMsgBoxIGSBuyPackageItem::Create(float fPriority)
{
	LoadImages();

	SetAddCallbackFunc();

	CNewUIMessageBoxBase::Create((IMAGE_IGS_WINDOW_WIDTH/2)-(IMAGE_IGS_FRAME_WIDTH/2), (IMAGE_IGS_WINDOW_HEIGHT/2)-(IMAGE_IGS_FRAME_HEIGHT/2),IMAGE_IGS_FRAME_WIDTH, IMAGE_IGS_FRAME_HEIGHT, fPriority);
	
	if(g_pNewUI3DRenderMng)
	{
		g_pNewUI3DRenderMng->Add3DRenderObj(this);
	}

	CreateListBox();
	SetButtonInfo();
	SetMsgBackOpacity();
	return true;
}

void CMsgBoxIGSBuyPackageItem::Initialize(CShopPackage* pPackage)
{
	int iProductSeq;
	int iValue = 0;
	unicode::t_char szText[MAX_TEXT_LENGTH] = {'\0', };

	m_iPackageSeq	= pPackage->PackageProductSeq;
	m_iDisplaySeq	= pPackage->ProductDisplaySeq;
	m_iCashType		= pPackage->CashType;

	/*
		Gifting.

		IBSPackage.txt field 10 is the gift flag (184 allowed / 185 not), and
		every row in the shipped script says 185 - which is why the Gift button
		has always been dead. In custom mode the database is the authority for
		what the shop sells and the script is only a fallback, and the whole
		server-side gift path already exists and works
		(CGCashShopItemGifRecv -> item insert -> in-game mail), so the script's
		blanket "no" is not a decision anyone made about these packages.

		Enabled for everything in custom mode. There is no per-package control
		yet: that needs a GiftEnabled column on CustomCashShopPackages and a
		field on the name/price round trip to carry it, the same shape as the
		DisplayName override.
	*/
	if( pPackage->GiftFlag == 184 || gProtect.m_MainInfo.CustomCashShop != 0 )
	{
		m_BtnPresent.SetEnable(true);
	}
	else
	{
		m_BtnPresent.SetEnable(false);
	}

	// Same admin-edited DisplayName override as the shelf card, so the confirm
	// dialog agrees with what the player just clicked on.
	char szOverrideName[32] = {0};

	if( gProtect.m_MainInfo.CustomCashShop != 0
		&& g_InGameShopSystem->GetServerPackageName(pPackage->PackageProductSeq, szOverrideName, sizeof(szOverrideName)) )
	{
		strncpy(m_szPackageName, szOverrideName, MAX_TEXT_LENGTH);
	}
	else
	{
		strncpy(m_szPackageName, pPackage->PackageProductName, MAX_TEXT_LENGTH);
	}

	/*
		The price.

		This used to read pPackage->Price unconditionally - the IBSPackage.txt
		list price - so a discounted package showed its sale price on the shelf
		and its full price here, in the box the player confirms. The shelf was
		right and this was wrong; nothing was ever mischarged, because the buy
		request has never carried a price and the server works it out from the
		same views, but the two disagreed on screen.

		Same fallback rule as the shelf: no server price means show the script
		price, because an unknown price should read as the old number.
	*/
	CInGameShopSystem::IGS_SERVER_PRICE ServerPrice;

	bool bServerPrice = (gProtect.m_MainInfo.CustomCashShop != 0)
		&& g_InGameShopSystem->GetServerPrice(CInGameShopSystem::IGS_PRICE_KIND_PACKAGE, pPackage->PackageProductSeq, ServerPrice);

	ConvertGold((bServerPrice ? ServerPrice.iEffectivePrice : pPackage->Price), szText);
	sprintf(m_szPrice, "%s %s", szText, pPackage->PricUnitName);

	// Period
	pPackage->SetProductSeqFirst();
	pPackage->GetProductSeqNext(iProductSeq);
	
	g_InGameShopSystem->GetProductInfoFromProductSeq(iProductSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_USE_LIMIT_PERIOD, iValue, szText);

	if( iValue > 0 )
	{
		sprintf(m_szPeriod, "%d %s", iValue, szText);
	}
	else
	{
		sprintf(m_szPeriod, "-");
	}

	// PriceSeq was never set here (always sent as the literal 0 in
	// BuyBtnDown below), so every single-price ("Buy" without a size/price
	// picker) purchase asked the server to buy price-sequence 0, which the
	// server correctly refuses as an invalid/sold-out offer.
	g_InGameShopSystem->GetProductInfoFromProductSeq(iProductSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_PRICE_SEQUENCE, m_iPriceSeq, szText);

	m_wItemCode = atoi(pPackage->InGamePackageID);

	ZeroMemory(m_szDescription, sizeof(unicode::t_char)*UIMAX_TEXT_LINE*MAX_TEXT_LENGTH);

	g_pRenderText->SetFont(g_hFont);

	// A description saved with real newlines instead of '#' wrapped as one line
	// and drew the CR/LF as notdef glyphs, running its sentences together. The
	// admin tool and the DataServer both store/send '#' now; this covers a
	// catalog that has not been re-pushed, and hand-edited IBSPackage.txt rows.
	unicode::t_char szDesc[SHOPLIST_LENGTH_PACKAGEDESC] = {'\0', };
	::IGSNormalizeShopText(pPackage->Description, szDesc, sizeof(szDesc));

	int nLine = ::DivideStringByPixel(&m_szDescription[0][0], UIMAX_TEXT_LINE, MAX_TEXT_LENGTH, szDesc, IGS_LISTBOX_WIDTH, false, '#');

	for (int i=0; i < nLine; ++i)
	{
		m_PackageInfo.AddText(m_szDescription[i]);
	}
}

void CMsgBoxIGSBuyPackageItem::Release()
{
	CNewUIMessageBoxBase::Release();

	if(g_pNewUI3DRenderMng)
		g_pNewUI3DRenderMng->Remove3DRenderObj(this);

	ReleaseListBox();

	UnloadImages();
}

bool CMsgBoxIGSBuyPackageItem::Update()
{
	m_BtnBuy.Update();
	m_BtnCancel.Update();
	m_BtnPresent.Update();
	ListBoxDoAction();
	return true;
}

bool CMsgBoxIGSBuyPackageItem::Render()
{
	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	RenderMsgBackColor(true);
	RenderFrame();
	RenderTexts();
	RenderButtons();
	RenderListBox();
	DisableAlphaBlend();
	return true;
}

void CMsgBoxIGSBuyPackageItem::RenderFrame()
{
	if( gProtect.m_MainInfo.CustomCashShop == 0 )
	{
		RenderImage(IMAGE_IGS_FRAME, GetPos().x, GetPos().y, IMAGE_IGS_FRAME_WIDTH, IMAGE_IGS_FRAME_HEIGHT);
		return;
	}

	/*
		Modern skin: the ornate 198x291 scrollwork plate is replaced by flat
		fills in the shop's own palette. No new art - the same reasoning as the
		shop background, which this dialog now sits on top of and was clashing
		with badly.

		The three wells (item, description, price) are drawn here so they sit
		under the 3D model, the description list and the price text, all of
		which keep their existing offsets.
	*/
	const int X = GetPos().x;
	const int Y = GetPos().y;
	const int W = IMAGE_IGS_FRAME_WIDTH;
	const int H = IMAGE_IGS_FRAME_HEIGHT;

	IGSFillRect(X, Y, W, H, 0.10f, 0.10f, 0.11f, 1.0f);

	// 1px border
	IGSFillRect(X, Y, W, 1, 0.24f, 0.24f, 0.27f, 1.0f);
	IGSFillRect(X, Y+H-1, W, 1, 0.24f, 0.24f, 0.27f, 1.0f);
	IGSFillRect(X, Y, 1, H, 0.24f, 0.24f, 0.27f, 1.0f);
	IGSFillRect(X+W-1, Y, 1, H, 0.24f, 0.24f, 0.27f, 1.0f);

	// Title bar - the shop's red, so the dialog reads as part of the same UI.
	IGSFillRect(X+1, Y+1, W-2, 22, 0.80f, 0.15f, 0.12f, 1.0f);

	IGSFillRect(X+8, Y+28, W-16, 70, 0.15f, 0.15f, 0.16f, 1.0f);		// item well
	IGSFillRect(X+8, Y+120, W-16, 98, 0.13f, 0.13f, 0.14f, 1.0f);		// description
	IGSFillRect(X+8, Y+224, W-16, 22, 0.28f, 0.10f, 0.09f, 1.0f);		// price chip
}

bool CMsgBoxIGSBuyPackageItem::IsVisible() const
{
	return true;
}

void CMsgBoxIGSBuyPackageItem::SetButtonInfo()
{
	m_BtnBuy.SetInfo(IGSDialogButtonImage(true, IMAGE_IGS_BUTTON), GetPos().x+IGS_BTN_BUY_POS_X, GetPos().y+IGS_BTN_POS_Y, IMAGE_IGS_BTN_WIDTH, IMAGE_IGS_BTN_HEIGHT, CNewUIMessageBoxButton::MSGBOX_BTN_CUSTOM, true);
	m_BtnBuy.MoveTextPos(-1, -1);
	m_BtnBuy.SetText(GlobalText[2891]);	 

	m_BtnPresent.SetInfo(IGSDialogButtonImage(false, IMAGE_IGS_BUTTON), GetPos().x+IGS_BTN_PRESENT_POS_X, GetPos().y+IGS_BTN_POS_Y, IMAGE_IGS_BTN_WIDTH, IMAGE_IGS_BTN_HEIGHT, CNewUIMessageBoxButton::MSGBOX_BTN_CUSTOM, true);
	m_BtnPresent.MoveTextPos(-1, -1);
	m_BtnPresent.SetText(GlobalText[2892]);	
	
	m_BtnCancel.SetInfo(IGSDialogButtonImage(false, IMAGE_IGS_BUTTON), GetPos().x+IGS_BTN_CANCEL_POS_X, GetPos().y+IGS_BTN_POS_Y, IMAGE_IGS_BTN_WIDTH, IMAGE_IGS_BTN_HEIGHT, CNewUIMessageBoxButton::MSGBOX_BTN_CUSTOM, true);
	m_BtnCancel.MoveTextPos(-1, -1);
	m_BtnCancel.SetText(GlobalText[229]);	
}

void CMsgBoxIGSBuyPackageItem::RenderTexts()
{
	g_pRenderText->SetBgColor(0, 0, 0, 0);
	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->SetFont(g_hFontBold);

	g_pRenderText->RenderText(GetPos().x, GetPos().y+IGS_TEXT_TITLE_POS_Y, GlobalText[2890], IMAGE_IGS_FRAME_WIDTH, 0, RT3_SORT_CENTER);

	g_pRenderText->SetTextColor(247, 186, 0, 255);

	g_pRenderText->RenderText(GetPos().x, GetPos().y+IGS_TEXT_NAME_POS_Y, m_szPackageName, IMAGE_IGS_FRAME_WIDTH, 0, RT3_SORT_CENTER);

	g_pRenderText->SetFont(g_hFont);

	g_pRenderText->SetTextColor(255, 238, 161, 255);

	g_pRenderText->RenderText(GetPos().x+IGS_TEXT_PRICE_POS_X, GetPos().y+IGS_TEXT_PRICE_POX_Y,	m_szPrice, IGS_TEXT_PRICE_WIDTH, 0, RT3_SORT_RIGHT);
	
#ifdef FOR_WORK
	if( IGSIsModernSkin() == false )
	{
	unicode::t_char szText[256] = {'\0', };
	g_pRenderText->SetTextColor(255, 0, 0, 255);
	if( m_wItemCode == 65535 )
	{
		sprintf(szText, "�������ڵ尡 �����ϴ�.");
	}
	else
	{
		sprintf(szText, "ItemCode : %d (%d, %d)", m_wItemCode, m_wItemCode/MAX_ITEM_INDEX, m_wItemCode%MAX_ITEM_INDEX);
	}
	g_pRenderText->RenderText(GetPos().x+IMAGE_IGS_FRAME_WIDTH, GetPos().y+10, szText, 200, 0, RT3_SORT_LEFT);
	sprintf(szText, "Package Seq : %d", m_iPackageSeq);
	g_pRenderText->RenderText(GetPos().x+IMAGE_IGS_FRAME_WIDTH, GetPos().y+20, szText, 200, 0, RT3_SORT_LEFT);
	sprintf(szText, "Display Seq : %d", m_iDisplaySeq);
	g_pRenderText->RenderText(GetPos().x+IMAGE_IGS_FRAME_WIDTH, GetPos().y+30, szText, 200, 0, RT3_SORT_LEFT);
	sprintf(szText, "Price Seq : 0");
	g_pRenderText->RenderText(GetPos().x+IMAGE_IGS_FRAME_WIDTH, GetPos().y+40, szText, 200, 0, RT3_SORT_LEFT);
	sprintf(szText, "CashType : %d", m_iCashType);
	g_pRenderText->RenderText(GetPos().x+IMAGE_IGS_FRAME_WIDTH, GetPos().y+50, szText, 200, 0, RT3_SORT_LEFT);
	}
#endif // FOR_WORK
	
}

void CMsgBoxIGSBuyPackageItem::RenderButtons()
{
	m_BtnBuy.Render();
	m_BtnPresent.Render();
	m_BtnCancel.Render();
}

void CMsgBoxIGSBuyPackageItem::Render3D()
{
	if( m_wItemCode==65535 )
		return;

	RenderItem3D(GetPos().x+IGS_3DITEM_POS_X, GetPos().y+IGS_3DITEM_POS_Y,IGS_3DITEM_WIDTH, IGS_3DITEM_HEIGHT, m_wItemCode, 0, 0, 0, true);
}

void CMsgBoxIGSBuyPackageItem::SetAddCallbackFunc()
{
	AddCallbackFunc(CMsgBoxIGSBuyPackageItem::LButtonUp, MSGBOX_EVENT_MOUSE_LBUTTON_UP);
	AddCallbackFunc(CMsgBoxIGSBuyPackageItem::BuyBtnDown, MSGBOX_EVENT_PRESSKEY_RETURN);
	AddCallbackFunc(CMsgBoxIGSBuyPackageItem::BuyBtnDown, MSGBOX_EVENT_USER_COMMON_OK);
	AddCallbackFunc(CMsgBoxIGSBuyPackageItem::PresentBtnDown, MSGBOX_EVENT_USER_CUSTOM_INGAMESHOP_PRESENT);
	AddCallbackFunc(CMsgBoxIGSBuyPackageItem::CancelBtnDown, MSGBOX_EVENT_USER_COMMON_CANCEL);
}

CALLBACK_RESULT CMsgBoxIGSBuyPackageItem::LButtonUp(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam)
{
	CMsgBoxIGSBuyPackageItem* pOwnMsgBox = dynamic_cast<CMsgBoxIGSBuyPackageItem*>(pOwner);
	if(pOwnMsgBox)
	{
		if(pOwnMsgBox->m_BtnBuy.IsMouseIn() == true)
		{
			g_MessageBox->SendEvent(pOwner, MSGBOX_EVENT_USER_COMMON_OK);
			return CALLBACK_BREAK;
		}

		if(pOwnMsgBox->m_BtnPresent.IsMouseIn() == true)
		{
			g_MessageBox->SendEvent(pOwner, MSGBOX_EVENT_USER_CUSTOM_INGAMESHOP_PRESENT);
			return CALLBACK_BREAK;
		}

		if(pOwnMsgBox->m_BtnCancel.IsMouseIn() == true)
		{
			g_MessageBox->SendEvent(pOwner, MSGBOX_EVENT_USER_COMMON_CANCEL);
			return CALLBACK_BREAK;
		}
	}
	return CALLBACK_CONTINUE;
}

CALLBACK_RESULT CMsgBoxIGSBuyPackageItem::BuyBtnDown(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam)
{
	CMsgBoxIGSBuyPackageItem* pOwnMsgBox = dynamic_cast<CMsgBoxIGSBuyPackageItem*>(pOwner);
	CMsgBoxIGSBuyConfirm* pMsgBox = NULL;
	CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSBuyConfirmLayout), &pMsgBox);

	pMsgBox->Initialize(pOwnMsgBox->m_wItemCode, pOwnMsgBox->m_iPackageSeq, pOwnMsgBox->m_iDisplaySeq, pOwnMsgBox->m_iPriceSeq, pOwnMsgBox->m_iCashType,pOwnMsgBox->m_szPackageName, pOwnMsgBox->m_szPrice, pOwnMsgBox->m_szPeriod);

	PlayBuffer(SOUND_CLICK01);
	g_MessageBox->SendEvent(pOwner, MSGBOX_EVENT_DESTROY);

	return CALLBACK_BREAK;
}

CALLBACK_RESULT CMsgBoxIGSBuyPackageItem::PresentBtnDown(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam)
{
	CMsgBoxIGSBuyPackageItem* pOwnMsgBox = dynamic_cast<CMsgBoxIGSBuyPackageItem*>(pOwner);
	
	CMsgBoxIGSSendGift* pMsgBox = NULL;
	CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSSendGiftLayout), &pMsgBox);

	pMsgBox->Initialize(pOwnMsgBox->m_iPackageSeq, pOwnMsgBox->m_iDisplaySeq, 0, pOwnMsgBox->m_wItemCode, pOwnMsgBox->m_iCashType,pOwnMsgBox->m_szPackageName, pOwnMsgBox->m_szPrice, pOwnMsgBox->m_szPeriod);

	PlayBuffer(SOUND_CLICK01);
	g_MessageBox->SendEvent(pOwner, MSGBOX_EVENT_DESTROY);
	return CALLBACK_BREAK;
}

CALLBACK_RESULT CMsgBoxIGSBuyPackageItem::CancelBtnDown(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam)
{
	PlayBuffer(SOUND_CLICK01);
	g_MessageBox->SendEvent(pOwner, MSGBOX_EVENT_DESTROY);

	return CALLBACK_BREAK;
}

void CMsgBoxIGSBuyPackageItem::LoadImages()
{
	LoadBitmap("Interface\\InGameShop\\Ingame_pack_back01.tga", IMAGE_IGS_FRAME, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_Bt03.tga",		IMAGE_IGS_BUTTON, GL_LINEAR);
}

void CMsgBoxIGSBuyPackageItem::UnloadImages()
{
	DeleteBitmap(IMAGE_IGS_FRAME);
	DeleteBitmap(IMAGE_IGS_BUTTON);
}

void CMsgBoxIGSBuyPackageItem::CreateListBox()
{
	m_PackageInfo.SetPosition(GetPos().x+IGS_LISTBOX_POS_X, GetPos().y+IGS_LISTBOX_POS_Y);
	m_PackageInfo.SetLineColorRender(false);
}

void CMsgBoxIGSBuyPackageItem::RenderListBox()
{
	if(m_PackageInfo.GetLineNum() != 0)
		m_PackageInfo.Render();
}

void CMsgBoxIGSBuyPackageItem::ListBoxDoAction()
{
	m_PackageInfo.DoAction();
}

void CMsgBoxIGSBuyPackageItem::ReleaseListBox()
{
	m_PackageInfo.Clear();
}

bool CMsgBoxBuyPackageItemLayout::SetLayout()
{
	CMsgBoxIGSBuyPackageItem* pMsgBox = GetMsgBox();
	if(nullptr == pMsgBox)
		return false;
	
	if(false == pMsgBox->Create())
		return false;
	
	return true;
}

#endif // KJH_ADD_INGAMESHOP_UI_SYSTEM
