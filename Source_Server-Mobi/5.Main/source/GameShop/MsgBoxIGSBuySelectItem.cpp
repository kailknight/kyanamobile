// MsgBoxIGSBuySelectItem.cpp: implementation of the CMsgBoxIGSBuySelectItem class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM
#include "MsgBoxIGSBuySelectItem.h"
#include "MsgBoxIGSCommon.h"	// IGSIsModernSkin, IGSFillRect, IGSRenderModernPanel
#include "DSPlaySound.h"
#include "NewUISystem.h"
#include "MsgBoxIGSBuyConfirm.h"
#include "MsgBoxIGSSendGift.h"

CMsgBoxIGSBuySelectItem::CMsgBoxIGSBuySelectItem()
{
	m_iPackageSeq = 0;
	m_iDisplaySeq = 0;
	m_wItemCode = -1;

	m_iDescriptionLine = 0;
	
	m_szPackageName[0] = '\0';
	m_szPrice[0] = '\0';

	for(int i=0 ; i<UIMAX_TEXT_LINE ; i++)
	{
		m_szDescription[i][0] = '\0';
	}
}

CMsgBoxIGSBuySelectItem::~CMsgBoxIGSBuySelectItem()
{
	Release();
}

//--------------------------------------------
// Create
bool CMsgBoxIGSBuySelectItem::Create(float fPriority)
{
	LoadImages();
	
	SetAddCallbackFunc();
	
	CNewUIMessageBoxBase::Create((IMAGE_IGS_WINDOW_WIDTH/2)-(IMAGE_IGS_FRAME_WIDTH/2),(IMAGE_IGS_WINDOW_HEIGHT/2)-(IMAGE_IGS_FRAME_HEIGHT/2), IMAGE_IGS_FRAME_WIDTH, IMAGE_IGS_FRAME_HEIGHT, fPriority);
	
	if(g_pNewUI3DRenderMng)
	{
		g_pNewUI3DRenderMng->Add3DRenderObj(this);
	}

	CreateListBox();
	SetButtonInfo();
	
	SetMsgBackOpacity();
	return true;
}

void CMsgBoxIGSBuySelectItem::Initialize(CShopPackage* pPackage)
{
	int iProductSeq, iPriceSeq;

	m_wItemCode = atoi(pPackage->InGamePackageID);
	
	m_iPackageSeq = pPackage->PackageProductSeq;
	m_iDisplaySeq = pPackage->ProductDisplaySeq;
	
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

	ZeroMemory(m_szDescription, sizeof(unicode::t_char)*UIMAX_TEXT_LINE*MAX_TEXT_LENGTH);
	
	g_pRenderText->SetFont(g_hFont);
	m_iDescriptionLine = ::DivideStringByPixel(&m_szDescription[0][0], UIMAX_TEXT_LINE, MAX_TEXT_LENGTH, pPackage->Description, IGS_TEXT_DISCRIPTION_WIDTH, false, '#');

	pPackage->SetProductSeqFirst();
	if( pPackage->GetProductSeqNext(iProductSeq) == false )
	{
	}

	pPackage->SetPriceSeqFirst();
	while( pPackage->GetPriceSeqNext(iPriceSeq) )
	{
		AddData(pPackage->PackageProductSeq, pPackage->ProductDisplaySeq, iPriceSeq, iProductSeq, pPackage->PricUnitName, pPackage->CashType);
	}
}

void CMsgBoxIGSBuySelectItem::Release()
{
	CNewUIMessageBoxBase::Release();

	if(g_pNewUI3DRenderMng)
		g_pNewUI3DRenderMng->Remove3DRenderObj(this);

	ReleaseListBox();

	UnloadImages();
}

bool CMsgBoxIGSBuySelectItem::Update()
{
	m_BtnBuy.Update();
	m_BtnCancel.Update();
	m_BtnPresent.Update();
	ListBoxDoAction();
	return true;
}

bool CMsgBoxIGSBuySelectItem::Render()
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

void CMsgBoxIGSBuySelectItem::RenderFrame()
{
	if( IGSIsModernSkin() )
	{
		/*
			Same flat treatment as the single-price confirm box, at this
			dialog's own 215x346: panel, 1px border, red title bar, and wells
			under the item, the description, the duration list and the price.
			The duration list keeps its own rows - only the plate behind it
			changes.
		*/
		const int X = GetPos().x;
		const int Y = GetPos().y;
		const int W = IMAGE_IGS_FRAME_WIDTH;
		const int H = IMAGE_IGS_FRAME_HEIGHT;

		IGSFillRect(X, Y, W, H, 0.10f, 0.10f, 0.11f, 1.0f);

		IGSFillRect(X, Y, W, 1, 0.24f, 0.24f, 0.27f, 1.0f);
		IGSFillRect(X, Y+H-1, W, 1, 0.24f, 0.24f, 0.27f, 1.0f);
		IGSFillRect(X, Y, 1, H, 0.24f, 0.24f, 0.27f, 1.0f);
		IGSFillRect(X+W-1, Y, 1, H, 0.24f, 0.24f, 0.27f, 1.0f);

		IGSFillRect(X+1, Y+1, W-2, 22, 0.80f, 0.15f, 0.12f, 1.0f);

		IGSFillRect(X+8, Y+28, W-16, 68, 0.15f, 0.15f, 0.16f, 1.0f);	// item
		IGSFillRect(X+8, Y+114, W-16, 34, 0.13f, 0.13f, 0.14f, 1.0f);	// attributes
		IGSFillRect(X+8, Y+154, W-16, 126, 0.13f, 0.13f, 0.14f, 1.0f);	// duration list
		IGSFillRect(X+8, Y+283, W-16, 22, 0.28f, 0.10f, 0.09f, 1.0f);	// price chip
	}
	else
	{
		RenderImage(IMAGE_IGS_MGSBOX_BACK, GetPos().x, GetPos().y, IMAGE_IGS_FRAME_WIDTH, IMAGE_IGS_FRAME_HEIGHT);
	}
}

void CMsgBoxIGSBuySelectItem::RenderTexts()
{
	g_pRenderText->SetBgColor(0, 0, 0, 0);
	g_pRenderText->SetTextColor(255, 255, 255, 255);
	g_pRenderText->SetFont(g_hFontBold);
	g_pRenderText->RenderText(GetPos().x, GetPos().y+IGS_TEXT_TITLE_POS_Y,GlobalText[2890], IMAGE_IGS_FRAME_WIDTH, 0, RT3_SORT_CENTER);
	g_pRenderText->SetTextColor(255, 255, 0, 255);
	g_pRenderText->RenderText(GetPos().x, GetPos().y+IGS_TEXT_NAME_POS_Y,m_szPackageName, IMAGE_IGS_FRAME_WIDTH, 0, RT3_SORT_CENTER);
	g_pRenderText->SetFont(g_hFont);
	g_pRenderText->SetTextColor(255, 255, 255, 255);

	for(int i=0 ; i<m_iDescriptionLine ; i++)
	{
		g_pRenderText->RenderText(GetPos().x+IGS_TEXT_ATTR_POS_X, GetPos().y+IGS_TEXT_ATTR_POS_Y+(i*10), m_szDescription[i], IGS_TEXT_ATTR_WIDTH, 0, RT3_SORT_LEFT);
	}
	
	g_pRenderText->RenderText(GetPos().x+IGS_TEXT_PRICE_POS_X, GetPos().y+IGS_TEXT_PRICE_POX_Y, m_szPrice, IGS_TEXT_PRICE_WIDTH, 0, RT3_SORT_RIGHT);

#ifdef FOR_WORK
	if( IGSIsModernSkin() == false )
	{
	// debug
	unicode::t_char szText[256] = {0,};
	g_pRenderText->SetTextColor(255, 0, 0, 255);
	if( m_wItemCode == 65535 )
	{
		sprintf(szText, "Bad item index.");
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
	sprintf(szText, "Price Seq : %d", m_SelectBuyListBox.GetSelectedText()->m_iPriceSeq);
	g_pRenderText->RenderText(GetPos().x+IMAGE_IGS_FRAME_WIDTH, GetPos().y+40, szText, 200, 0, RT3_SORT_LEFT);
	sprintf(szText, "Cash Type : %d", m_SelectBuyListBox.GetSelectedText()->m_iCashType);
	g_pRenderText->RenderText(GetPos().x+IMAGE_IGS_FRAME_WIDTH, GetPos().y+50, szText, 200, 0, RT3_SORT_LEFT);
	}
#endif // FOR_WORK

}

void CMsgBoxIGSBuySelectItem::RenderButtons()
{
	m_BtnPresent.Render();
	m_BtnBuy.Render();
	m_BtnCancel.Render();
}

void CMsgBoxIGSBuySelectItem::Render3D()
{
	if( m_wItemCode==65535 )
		return;

	RenderItem3D(GetPos().x+IGS_3DITEM_POS_X, GetPos().y+IGS_3DITEM_POS_Y, IGS_3DITEM_WIDTH, IGS_3DITEM_HEIGHT, m_wItemCode, 0, 0, 0, true);
}

bool CMsgBoxIGSBuySelectItem::IsVisible() const
{
	return true;
}

void CMsgBoxIGSBuySelectItem::SetAddCallbackFunc()
{
	AddCallbackFunc(CMsgBoxIGSBuySelectItem::LButtonUp, MSGBOX_EVENT_MOUSE_LBUTTON_UP);
	AddCallbackFunc(CMsgBoxIGSBuySelectItem::BuyBtnDown, MSGBOX_EVENT_PRESSKEY_RETURN);
	AddCallbackFunc(CMsgBoxIGSBuySelectItem::BuyBtnDown, MSGBOX_EVENT_USER_COMMON_OK);
	AddCallbackFunc(CMsgBoxIGSBuySelectItem::PresentBtnDown, MSGBOX_EVENT_USER_CUSTOM_INGAMESHOP_PRESENT);
	AddCallbackFunc(CMsgBoxIGSBuySelectItem::CancelBtnDown, MSGBOX_EVENT_USER_COMMON_CANCEL);
}

CALLBACK_RESULT CMsgBoxIGSBuySelectItem::LButtonUp(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam)
{
	CMsgBoxIGSBuySelectItem* pOwnMsgBox = dynamic_cast<CMsgBoxIGSBuySelectItem*>(pOwner);

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

CALLBACK_RESULT CMsgBoxIGSBuySelectItem::BuyBtnDown(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam)
{
	CMsgBoxIGSBuySelectItem* pOwnMsgBox = dynamic_cast<CMsgBoxIGSBuySelectItem*>(pOwner);
	IGS_SelectBuyItem* pItem = pOwnMsgBox->m_SelectBuyListBox.GetSelectedText();
	
	CMsgBoxIGSBuyConfirm* pMsgBox = NULL;
	CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSBuyConfirmLayout), &pMsgBox);
	pMsgBox->Initialize(pOwnMsgBox->m_wItemCode, pOwnMsgBox->m_iPackageSeq, pOwnMsgBox->m_iDisplaySeq, pItem->m_iPriceSeq, pItem->m_iCashType, pItem->m_szItemName, pItem->m_szItemPrice, pItem->m_szItemPeriod);

	PlayBuffer(SOUND_CLICK01);
	g_MessageBox->SendEvent(pOwner, MSGBOX_EVENT_DESTROY);
	
	return CALLBACK_BREAK;
}

CALLBACK_RESULT CMsgBoxIGSBuySelectItem::PresentBtnDown(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam)
{
	CMsgBoxIGSBuySelectItem* pOwnMsgBox = dynamic_cast<CMsgBoxIGSBuySelectItem*>(pOwner);
	IGS_SelectBuyItem* pItem = pOwnMsgBox->m_SelectBuyListBox.GetSelectedText();
	
	CMsgBoxIGSSendGift* pMsgBox = NULL;
	CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSSendGiftLayout), &pMsgBox);
	pMsgBox->Initialize(pOwnMsgBox->m_iPackageSeq, pOwnMsgBox->m_iDisplaySeq, pItem->m_iPriceSeq, pItem->m_wItemCode, pItem->m_iCashType, pOwnMsgBox->m_szPackageName, pItem->m_szItemPrice, pItem->m_szItemPeriod);

	PlayBuffer(SOUND_CLICK01);
	g_MessageBox->SendEvent(pOwner, MSGBOX_EVENT_DESTROY);
	
	return CALLBACK_BREAK;
}

CALLBACK_RESULT CMsgBoxIGSBuySelectItem::CancelBtnDown(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam)
{
	PlayBuffer(SOUND_CLICK01);
	g_MessageBox->SendEvent(pOwner, MSGBOX_EVENT_DESTROY);

	return CALLBACK_BREAK;
}

void CMsgBoxIGSBuySelectItem::SetButtonInfo()
{
	m_BtnBuy.SetInfo(IGSDialogButtonImage(true, IMAGE_IGS_BUTTON), GetPos().x+IGS_BTN_BUY_POS_X, GetPos().y+IGS_BTN_POS_Y, IMAGE_IGS_BTN_WIDTH, IMAGE_IGS_BTN_HEIGHT, CNewUIMessageBoxButton::MSGBOX_BTN_CUSTOM, true);
	m_BtnBuy.MoveTextPos(-1, -1);
	m_BtnBuy.SetText(GlobalText[2891]);	
	
	m_BtnPresent.SetInfo(IGSDialogButtonImage(false, IMAGE_IGS_BUTTON), GetPos().x+IGS_BTN_PRESENT_POS_X, GetPos().y+IGS_BTN_POS_Y,IMAGE_IGS_BTN_WIDTH, IMAGE_IGS_BTN_HEIGHT, CNewUIMessageBoxButton::MSGBOX_BTN_CUSTOM, true);
	m_BtnPresent.MoveTextPos(-1, -1);
	m_BtnPresent.SetText(GlobalText[2892]);	
	
	m_BtnCancel.SetInfo(IGSDialogButtonImage(false, IMAGE_IGS_BUTTON), GetPos().x+IGS_BTN_CANCEL_POS_X, GetPos().y+IGS_BTN_POS_Y, IMAGE_IGS_BTN_WIDTH, IMAGE_IGS_BTN_HEIGHT, CNewUIMessageBoxButton::MSGBOX_BTN_CUSTOM, true);
	m_BtnCancel.MoveTextPos(-1, -1);
	m_BtnCancel.SetText(GlobalText[229]);	
}

//--------------------------------------------
// LoadImages
void CMsgBoxIGSBuySelectItem::LoadImages()
{
	LoadBitmap("Interface\\InGameShop\\Ingame_pack_back03.tga", IMAGE_IGS_MGSBOX_BACK, GL_LINEAR);
	LoadBitmap("Interface\\InGameShop\\Ingame_Bt03.tga",		IMAGE_IGS_BUTTON, GL_LINEAR);
}


void CMsgBoxIGSBuySelectItem::UnloadImages()
{
	DeleteBitmap(IMAGE_IGS_MGSBOX_BACK);
	DeleteBitmap(IMAGE_IGS_BUTTON);
}

void CMsgBoxIGSBuySelectItem::CreateListBox()
{
	m_SelectBuyListBox.SetPosition(GetPos().x+IGS_LISTBOX_POS_X, GetPos().y+IGS_LISTBOX_POS_Y);
}

void CMsgBoxIGSBuySelectItem::RenderListBox()
{
	if(m_SelectBuyListBox.GetLineNum() != 0)
		m_SelectBuyListBox.Render();
}

void CMsgBoxIGSBuySelectItem::ReleaseListBox()
{
	m_SelectBuyListBox.Clear();
}

void CMsgBoxIGSBuySelectItem::ListBoxDoAction()
{
	m_SelectBuyListBox.DoAction();
	
	if( m_SelectBuyListBox.IsChangeLine() == TRUE )
	{
		IGS_SelectBuyItem* pItem = m_SelectBuyListBox.GetSelectedText();
		strcpy( m_szPrice, pItem->m_szItemPrice);
		m_wItemCode = pItem->m_wItemCode;
	}
}

void CMsgBoxIGSBuySelectItem::AddData(int iPackageSeq, int iDisplaySeq, int iPriceSeq, int iProductSeq, unicode::t_char* pszPriceUnit, int iCashType)
{
	int iValue;
	unicode::t_char szText[MAX_TEXT_LENGTH] = {'\0', };
	
	IGS_SelectBuyItem Item;
	memset(&Item, 0, sizeof(IGS_SelectBuyItem));

	Item.m_bIsSelected = FALSE;
	Item.m_iPackageSeq = iPackageSeq;
	Item.m_iDisplaySeq = iDisplaySeq;
	Item.m_iPriceSeq = iPriceSeq;
	Item.m_iCashType = iCashType;

	g_InGameShopSystem->GetProductInfoFromPriceSeq(iProductSeq, iPriceSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_ITEMCODE, iValue, szText);
	Item.m_wItemCode = iValue;
	
	g_InGameShopSystem->GetProductInfoFromPriceSeq(iProductSeq, iPriceSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_ITEMNAME, iValue, szText);
	strcpy(Item.m_szItemName, szText);
	
	g_InGameShopSystem->GetProductInfoFromPriceSeq(iProductSeq, iPriceSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_PRICE, iValue, szText);
	sprintf(Item.m_szItemPrice, "%s %s", szText, pszPriceUnit);
	
	g_InGameShopSystem->GetProductInfoFromPriceSeq(iProductSeq, iPriceSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_USE_LIMIT_PERIOD, iValue, szText);
	if( iValue > 0 )
	{
		sprintf(Item.m_szItemPeriod, "%d %s", iValue, szText);
	}
	else
	{
		sprintf(Item.m_szItemPeriod, "-");
	}
	
	g_InGameShopSystem->GetProductInfoFromPriceSeq(iProductSeq, iPriceSeq, CInGameShopSystem::IGS_PRODUCT_ATT_TYPE_NUM, iValue, szText);
	if( iValue > 0 )
	{
		sprintf(Item.m_szAttribute, GlobalText[3045], iValue, Item.m_szItemPeriod);
	}
	else
	{
		sprintf(Item.m_szAttribute, GlobalText[3039], Item.m_szItemPeriod);
	}
	
	m_SelectBuyListBox.AddText(Item);
}

bool CMsgBoxIGSBuySelectItemLayout::SetLayout()
{
	CMsgBoxIGSBuySelectItem* pMsgBox = GetMsgBox();
	if(nullptr == pMsgBox)
		return false;
	
	if(false == pMsgBox->Create())
		return false;
	
	return true;
}

#endif // KJH_ADD_INGAMESHOP_UI_SYSTEM
