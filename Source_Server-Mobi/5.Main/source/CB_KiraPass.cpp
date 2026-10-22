// CB_KiraPass.cpp: Kira Pass (monthly WCoin) panel. See CB_KiraPass.h.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CB_KiraPass.h"
#include "CBInterface.h"
#include "NewUIBCustomMenu.h"
#include "NewUISystem.h"
#include "DSPlaySound.h"
#include "FriendMailWindow.h"
#include "MultiLanguage.h"

void TranslateTemplate(const char* pszEnglish, char* pszOut, size_t cbOut);	// MultiLanguage.cpp
#include <cstddef>
#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM
#include "GameShop/NewUIInGameShop.h"
#endif

// A phone with an in-app store: Google Play on Android, the App Store on
// iPhone/iPad. PC and Mac buy on the website (same rule as the cash shop).
#if defined(__ANDROID__) || (defined(MU_IOS) && !defined(MU_MACOS))
#define KIRA_PASS_STORE_BILLING 1
#include "Platform/MobilePlatform.h"
#endif

extern char LogInID[];

CB_KiraPass* gCB_KiraPass = NULL;

CB_KiraPass::CB_KiraPass()
{
	this->m_Known = false;
	this->m_DaysLeft = 0;
	this->m_PaidToday = 0;
	this->m_Days = 30;
	this->m_Instant = 50;
	this->m_Daily = 25;
}

void CB_KiraPass::OpenWindow()
{
	if ((GetTickCount() - gInterface.Data[eWindowKiraPass].EventTick) < 300)
	{
		return;
	}

	gInterface.Data[eWindowKiraPass].EventTick = GetTickCount();
	gInterface.Data[eWindowKiraPass].OnShow ^= 1;

#if defined(KIRA_PASS_STORE_BILLING)
	if (gInterface.Data[eWindowKiraPass].OnShow)
	{
		// Connects the store and loads the product (its local price).
		MU_MobileBilling("account", LogInID, NULL, 0);
		MU_MobileBilling("init", "", NULL, 0);
	}
#endif
}

void CB_KiraPass::RecvStatus(const BYTE* lpMsg, int size)
{
	// An older server sends the packet without OpenMailbox.
	if (size < (int)offsetof(PMSG_MONTHLYCARD_STATUS_RECV, OpenMailbox))
	{
		return;
	}

	const PMSG_MONTHLYCARD_STATUS_RECV* lpStatus = (const PMSG_MONTHLYCARD_STATUS_RECV*)lpMsg;

	this->m_Known = true;
	this->m_DaysLeft = lpStatus->DaysLeft;
	this->m_PaidToday = lpStatus->PaidToday;
	this->m_Days = (lpStatus->Days > 0) ? lpStatus->Days : 30;
	this->m_Instant = lpStatus->Instant;
	this->m_Daily = lpStatus->Daily;

	// Kira Pass holder logging in: open the Friends window on the mailbox,
	// where the daily WCoin letter waits.
	if (size >= (int)sizeof(PMSG_MONTHLYCARD_STATUS_RECV) && lpStatus->OpenMailbox != 0 && gFriendMailWindow != NULL)
	{
		gFriendMailWindow->OpenMailbox();
	}
}

void CB_KiraPass::Buy()
{
	PlayBuffer(SOUND_CLICK01);

#if defined(KIRA_PASS_STORE_BILLING)
	MU_MobileBilling("account", LogInID, NULL, 0);
	if (!MU_MobileBilling("buy", KIRA_PASS_PRODUCT_ID, NULL, 0) && g_pChatListBox != NULL)
	{
		g_pChatListBox->AddText("", "The store is not available on this device right now.", SEASON3B::TYPE_ERROR_MESSAGE);
	}
#elif defined(_WIN32) && !defined(__ANDROID__) && !defined(MU_IOS)
	// PC: the website store (PayMongo) - the pass is a package there.
	ShellExecuteA(0, "open", KIRA_PASS_STORE_URL, 0, 0, SW_SHOW);
#else
	if (g_pChatListBox != NULL)
	{
		g_pChatListBox->AddText("", "Buy the Kira Pass on worldofkira.com.", SEASON3B::TYPE_SYSTEM_MESSAGE);
	}
#endif
}

void CB_KiraPass::DrawWindow()
{
	if (!gInterface.Data[eWindowKiraPass].OnShow)
	{
		return;
	}

	if (gInterface.CheckWindow(Interface::MoveList) || gInterface.CheckWindow(Interface::ObjWindow::CashShop) || gInterface.CheckWindow(Interface::ObjWindow::SkillTree) || gInterface.CheckWindow(Interface::ObjWindow::FullMap))
	{
		gInterface.Data[eWindowKiraPass].OnShow = false;
		return;
	}

#if defined(KIRA_PASS_STORE_BILLING) && defined(KJH_ADD_INGAMESHOP_UI_SYSTEM)
	// A purchase made from this panel reports back through the cash shop's
	// store poll, which otherwise only runs while the shop is open.
	if (g_pInGameShop != NULL)
	{
		g_pInGameShop->PollStoreResults();
	}
#endif

	float WindowW = 250;
	float WindowH = 230;
	float StartX = (MAX_WIN_WIDTH / 2) - (WindowW / 2);
	float StartY = 40;

	// Vietnamese text for the English templates below. Falls back to the English when the
	// language is English or the template has no translation (TranslateTemplate decides).
	auto T = [](const char* en) -> const char* {
		static char s_buf[8][256];
		static int s_next = 0;
		char* out = s_buf[s_next];
		s_next = (s_next + 1) % 8;
		TranslateTemplate(en, out, 256);
		return out;
	};

	g_pBCustomMenuInfo->gDrawWindowCustom(&StartX, &StartY, WindowW, WindowH, eWindowKiraPass, T("Kira Pass"));

	int x = (int)StartX + 15;

	int y = (int)StartY + 40;
	int w = (int)WindowW - 30;

	char price[48] = "PHP 500";
#if defined(KIRA_PASS_STORE_BILLING)
	char storePrice[48] = { 0 };
	if (MU_MobileBilling("price", KIRA_PASS_PRODUCT_ID, storePrice, sizeof(storePrice)) && storePrice[0] != '\0')
	{
		// Not strncpy_s(..., _TRUNCATE): on Android PlatformDefs.h maps that to
		// strncpy(dst, src, _TRUNCATE), and strncpy zero-fills up to the count -
		// (size_t)-1 bytes - which smashed the stack the moment Play returned
		// the price.
		strncpy(price, storePrice, sizeof(price) - 1);
		price[sizeof(price) - 1] = '\0';
	}
#endif

	y = gInterface.DrawFormat(eGold, x, y, w, 3, T("%s for %d days"), price, (int)this->m_Days);
	y += 6;
	y = gInterface.DrawFormat(eWhite, x, y, w, 1, T("- %d WCoin (C) right away"), (int)this->m_Instant);
	y = gInterface.DrawFormat(eWhite, x, y, w, 1, T("- %d WCoin (C) every day you log in"), (int)this->m_Daily);
	y = gInterface.DrawFormat(eWhite, x, y, w, 1, T("- Rewards arrive in your mailbox (Friends)"));
	y = gInterface.DrawFormat(eWhite, x, y, w, 1, T("- Claim them within 14 days"));
	y = gInterface.DrawFormat(eRed, x, y, w, 1, T("A day you don't log in is lost."));
	y = gInterface.DrawFormat(eWhite, x, y, w, 1, T("Buying again adds %d more days."), (int)this->m_Days);
	y += 8;

	if (!this->m_Known)
	{
		gInterface.DrawFormat(eWhite, x, y, w, 3, T("Status: checking..."));
	}
	else if (this->m_DaysLeft > 0)
	{
		y = gInterface.DrawFormat(eExcellent, x, y, w, 3, T("Active - %d day(s) left"), (int)this->m_DaysLeft);
		gInterface.DrawFormat(this->m_PaidToday ? eExcellent : eGold, x, y, w, 3,
			this->m_PaidToday ? T("Today's reward: sent to your mailbox") : T("Today's reward: on your next login"));
	}
	else
	{
		gInterface.DrawFormat(eWhite, x, y, w, 3, T("Status: not active"));
	}

	const float BtnW = 100;
	const float BtnY = StartY + WindowH - 40;

	if (g_pBCustomMenuInfo->DrawButton(StartX + 15, BtnY, 170, 12, (char*)(this->m_DaysLeft > 0 ? T("Extend") : T("Buy")), BtnW))
	{
		this->Buy();
	}

	if (g_pBCustomMenuInfo->DrawButton(StartX + WindowW - 15 - BtnW, BtnY, 170, 12, (char*)T("Close"), BtnW))
	{
		gInterface.Data[eWindowKiraPass].OnShow = 0;
		PlayBuffer(SOUND_CLICK01);
	}
}
