// TradeCoinPanel.cpp: coins in a trade. See TradeCoinPanel.h.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TradeCoinPanel.h"
#include "CBInterface.h"
#include "NewUIBCustomMenu.h"
#include "NewUISystem.h"
#include "NewUITrade.h"
#include "ZzzInventory.h"
#include "wsclientinline.h"
#include "DSPlaySound.h"
#include "Input.h"

extern int DisplayWin;

CTradeCoinPanel* gTradeCoinPanel = NULL;

namespace
{
	const char* kCoinNames[TRADE_COIN_TYPES] = { "WCoin (C)", "WCoin (P)", "Goblin Points" };

	const float kChooserW = 120.0f;
	const float kChooserH = 112.0f;

	bool PrimaryDown()
	{
#if defined(__ANDROID__) || defined(MU_IOS)
		return SEASON3B::IsPress(VK_LBUTTON) || CInput::Instance().IsLBtnDn();
#else
		return ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
#endif
	}
}

CTradeCoinPanel::CTradeCoinPanel()
{
	this->Reset();
	this->m_PendingType = 0;
	this->m_ChooserOpen = false;
	this->m_ChooserTick = 0;
	this->m_WasOpen = false;
	this->m_WasDown = false;
}

void CTradeCoinPanel::Reset()
{
	memset(this->m_Mine,0,sizeof(this->m_Mine));
	memset(this->m_Theirs,0,sizeof(this->m_Theirs));
	this->m_ChooserOpen = false;
}

void CTradeCoinPanel::RecvCoins(PMSG_TRADE_COIN_RECV* lpMsg)
{
	memcpy(this->m_Mine,lpMsg->mine,sizeof(this->m_Mine));
	memcpy(this->m_Theirs,lpMsg->theirs,sizeof(this->m_Theirs));
}

void CTradeCoinPanel::SendOffer(int type,DWORD amount)
{
	if(type < 0 || type >= TRADE_COIN_TYPES)
	{
		return;
	}

	// Changing the offer un-confirms you, as entering zen does
	// (CNewUITrade::SendRequestMyGoldInput) - the server un-confirms you too,
	// and a button left looking pressed made the trade stall until it was
	// toggled again.
	if(g_pTrade->IsMyConfirm())
	{
		g_pTrade->SetMyConfirmOff();
		SendRequestTradeResult(false);
	}

	PMSG_TRADE_COIN_SET pMsg;
	pMsg.header.set(0xD3,0xE8,sizeof(pMsg));
	pMsg.type = (BYTE)type;
	pMsg.amount = amount;
	DataSend((LPBYTE)&pMsg,pMsg.header.size);

	// The server drops the zen offer for a coin one; the bar must not fall
	// back to the old zen figure if the coins are later taken out again.
	if(amount > 0)
	{
		g_pTrade->ClearMyTradeGold();
	}
}

int CTradeCoinPanel::OfferedCoin(bool mine,DWORD* amount) const
{
	const DWORD* coins = mine ? this->m_Mine : this->m_Theirs;

	for(int n = 0; n < TRADE_COIN_TYPES; n++)
	{
		if(coins[n] > 0)
		{
			*amount = coins[n];
			return n;
		}
	}

	*amount = 0;
	return -1;
}

void CTradeCoinPanel::OpenChooser()
{
	this->m_ChooserOpen = !this->m_ChooserOpen;
	this->m_ChooserTick = GetTickCount();
}

// The chooser opens where the zen amount box does (see Draw).
void CTradeCoinPanel::Draw()
{
	bool open = g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_TRADE);

	if(open != this->m_WasOpen)
	{
		// A trade starts or ends: the server starts every trade from zero.
		this->Reset();
		this->m_WasOpen = open;
	}

	bool down = PrimaryDown();
	bool click = down && (this->m_WasDown == false);
	this->m_WasDown = down;

	if(open == false || this->m_ChooserOpen == false)
	{
		return;
	}

	// Exactly where the zen amount box opens (CNewUITextInputMsgBox::Create:
	// centred, y = 100), so the choice and the box it leads to share a spot and
	// neither sits over the chat at the bottom of a phone screen.
	float x = (float)(DisplayWin / 2) - (kChooserW / 2.0f);
	float y = 100.0f;

	bool inside = SEASON3B::CheckMouseIn((int)x,(int)y,(int)kChooserW,(int)kChooserH) == 1;

	if(inside)
	{
		g_pBCustomMenuInfo->SetBlockCur(TRUE);	// keep clicks off the world
	}

	// A click anywhere else closes it - but not the click that opened it.
	if(click && inside == false && (GetTickCount() - this->m_ChooserTick) > 300)
	{
		this->m_ChooserOpen = false;
		return;
	}

	gInterface.DrawBarForm(x,y,kChooserW,kChooserH,0.08f,0.08f,0.1f,0.92f);
	gInterface.DrawBarForm(x,y,kChooserW,18.0f,0.45f,0.32f,0.08f,0.9f);
	TextDraw(g_hFontBold,(int)x,(int)y + 3,0xFFFFFFFF,0x0,(int)kChooserW,0,3,"Trade with");

	const char* labels[1 + TRADE_COIN_TYPES] = { "Zen", kCoinNames[0], kCoinNames[1], kCoinNames[2] };

	for(int n = 0; n < 1 + TRADE_COIN_TYPES; n++)
	{
		float by = y + 24.0f + n * 22.0f;

		if(g_pBCustomMenuInfo->DrawButton(x + 6.0f,by,80.0f,12,(char*)labels[n],kChooserW - 12.0f))
		{
			this->m_ChooserOpen = false;

			if(n == 0)
			{
				SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CTradeZenMsgBoxLayout));
			}
			else
			{
				this->m_PendingType = n - 1;
				SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CTradeCoinMsgBoxLayout));
			}

			return;
		}
	}
}

// -----------------------------------------------------------------------------
// hooks for NewUITrade.cpp
// -----------------------------------------------------------------------------

void TradeCoinOpenChooser()
{
	if(gTradeCoinPanel != NULL)
	{
		gTradeCoinPanel->OpenChooser();
		return;
	}

	SEASON3B::CreateMessageBox(MSGBOX_LAYOUT_CLASS(SEASON3B::CTradeZenMsgBoxLayout));
}

// One money bar: the currency's name after the coin picture, the amount at the
// right edge (where the zen figure always was).
void TradeCoinRenderOfferBar(bool mine,int x,int y,int zen)
{
	unicode::t_char amountText[64];
	const char* label = "Zen";
	DWORD coins = 0;
	int coin = (gTradeCoinPanel != NULL) ? gTradeCoinPanel->OfferedCoin(mine,&coins) : -1;

	if(coin >= 0)
	{
		label = kCoinNames[coin];
		::ConvertGold((double)coins,amountText);
		g_pRenderText->SetTextColor(255,210,80,255);
	}
	else
	{
		::ConvertGold((double)zen,amountText);
		g_pRenderText->SetTextColor(::getGoldColor((DWORD)zen));
	}

	g_pRenderText->RenderText(x + 170,y + 8,amountText,0,0,RT3_WRITE_RIGHT_TO_LEFT);

	g_pRenderText->SetTextColor(230,200,120,255);
	g_pRenderText->RenderText(x + 50,y + 8,label);
}

// -----------------------------------------------------------------------------
// the amount box for a coin
// -----------------------------------------------------------------------------

bool SEASON3B::CTradeCoinMsgBoxLayout::SetLayout()
{
	CNewUITextInputMsgBox* pMsgBox = GetMsgBox();

	if(0 == pMsgBox)
	{
		return false;
	}

	if(false == pMsgBox->Create(MSGBOX_COMMON_TYPE_OKCANCEL,INPUTBOX_TYPE_NUMBER,INPUTBOX_WIDTH,INPUTBOX_HEIGHT,10))
	{
		return false;
	}

	int type = (gTradeCoinPanel != NULL) ? gTradeCoinPanel->m_PendingType : 0;

	if(type < 0 || type >= TRADE_COIN_TYPES)
	{
		type = 0;
	}

	static char text[128];
	sprintf_s(text,sizeof(text),"Enter the amount of %s you would like to trade.",kCoinNames[type]);

	pMsgBox->SetInputBoxOption(UIOPTION_NUMBERONLY|UIOPTION_PAINTBACK);
	pMsgBox->AddMsg(text);
	pMsgBox->AddCallbackFunc(CTradeCoinMsgBoxLayout::ReturnDown,MSGBOX_EVENT_PRESSKEY_RETURN);
	pMsgBox->AddCallbackFunc(CTradeCoinMsgBoxLayout::OkBtnDown,MSGBOX_EVENT_USER_COMMON_OK);
	pMsgBox->AddCallbackFunc(CTradeCoinMsgBoxLayout::CancelBtnDown,MSGBOX_EVENT_USER_COMMON_CANCEL);

	return true;
}

CALLBACK_RESULT SEASON3B::CTradeCoinMsgBoxLayout::ProcessOk(class CNewUIMessageBoxBase* pOwner)
{
	CNewUITextInputMsgBox* pMsgBox = dynamic_cast<CNewUITextInputMsgBox*>(pOwner);
	unicode::t_char strText[MAX_TEXT_LENGTH] = { 0, };
	pMsgBox->GetInputBoxText(strText);

	if(unicode::_strlen(strText) == 0)
	{
		return CALLBACK_CONTINUE;
	}

	// 0 is allowed here: it takes this coin back out of the offer.
	DWORD amount = 0;

	for(int n = 0; strText[n] != 0; n++)
	{
		if(strText[n] < '0' || strText[n] > '9' || amount > 100000000)
		{
			return CALLBACK_CONTINUE;
		}

		amount = amount * 10 + (DWORD)(strText[n] - '0');
	}

	if(gTradeCoinPanel != NULL)
	{
		gTradeCoinPanel->SendOffer(gTradeCoinPanel->m_PendingType,amount);
	}

	PlayBuffer(SOUND_CLICK01);
	g_MessageBox->SendEvent(pOwner,MSGBOX_EVENT_DESTROY);

	return CALLBACK_BREAK;
}

CALLBACK_RESULT SEASON3B::CTradeCoinMsgBoxLayout::ReturnDown(class CNewUIMessageBoxBase* pOwner,const leaf::xstreambuf& xParam)
{
	return ProcessOk(pOwner);
}

CALLBACK_RESULT SEASON3B::CTradeCoinMsgBoxLayout::OkBtnDown(class CNewUIMessageBoxBase* pOwner,const leaf::xstreambuf& xParam)
{
	return ProcessOk(pOwner);
}

CALLBACK_RESULT SEASON3B::CTradeCoinMsgBoxLayout::CancelBtnDown(class CNewUIMessageBoxBase* pOwner,const leaf::xstreambuf& xParam)
{
	PlayBuffer(SOUND_CLICK01);
	g_MessageBox->SendEvent(pOwner,MSGBOX_EVENT_DESTROY);

	return CALLBACK_BREAK;
}
