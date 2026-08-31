#include "stdafx.h"
#include "MenuCustom.h"
#include "Util.h"
#include "Protect.h"
#include "CBInterface.h"
#include "NewUISystem.h"
#include "Other.h"
#include "CustomEventTime.h"
#include "Ranking.h"
#include "WindowClass.h"
#include "CBChoTroi.h"
#include "CB_AutoNapGame.h"
#include "CB_DanhHieu.h"
#include "CB_JewelBank.h"
#include "CustomRanking.h"
#include "CBInterfaceVIPChar.h"
#include "CB_HuyDongExc.h"
#include "CB_DoiMK.h"
#include "MocDonate.h"
#include "CB_LockItem.h"
#include "VongQuay.h"
#include "RedeemCodeWindow.h"

cCustomMenu gCustomMenu;

cCustomMenu::cCustomMenu()
	: m_CurrentPage(0)
{

}

cCustomMenu::~cCustomMenu()
{
}


void cCustomMenu::GetCountButton()
{

}
void cCustomMenu::ActionButton(int TypeButton)
{
	switch (TypeButton)
	{
	case eButtonEventTime: //Time Event
	{
		gInterface.Data[eWindowEventTime].OpenClose();
		if (gInterface.Data[eWindowEventTime].OnShow)
		{
			gCustomEventTime.ClearCustomEventTime();

			gCustomEventTime.OpenTestWindow();
		}
	}
	break;
	case eButtonVipShop: //VIP
	{
		gInterface.Data[eVip_MAIN].OpenClose();
	}
	break;
	case eButtonRanking: //Ranking
	{
		gCustomRanking->OpenWindow();
		//gRanking.OpenOnOff();
	}
	break;
	case eButtonChangeClass: //Change Class
	{
		WindowClass.SetVisible(true);
	}
	break;
#if(CUSTOM_BCHOTROI)
	case eButtonMarKet: //cho troi
	{
		gCBChotroi.GetOpenChoTroiWinDow();
	}
	break;
#endif
#if(DANH_HIEU_NEW == 1)
	case eButtonDanhHieu: //danh hieu
	{
		gCBDanhHieu.OpenWindow();
	}
	break;
#endif
#if(CB_AutoBanking)
	case eButtonNapGame: //nut nap game
	{
		gCBAutoNapGame.OpenWindowBanking();
	}
	break;
#endif
	case eButtonJewelBank: //nut jewelbank
	{
		gCBJewelBank.OpenOnOff();
	}
	break;
#if(CB_VIP_CHAR)
	case eButtonVipChar: //
	{
		if (gBInterfaceVIPChar) gBInterfaceVIPChar->CGSendOpenWinwdowVIP();
	}
	break;
#endif
#if(CB_HUYDONGEXC)
	case eButtonHuyDongExc: //nut jewelbank
	{
		gCBHuyDongExc->OpenWindow();
	}
	break;
#endif
#if(DOIMK)
	case eButtonDoiMatKhau:
	{
		gCB_DoiMK->OpenWindow();
	}
	break;
#endif
	case eButtonMocNap:
	{
		gMocDonate.OpenWindowMocNap();
	}
	break;
	
	case eButtonLockItem:
	{
		gCB_LockItem->OpenWindowLock();
	}
	break;


	case eButtonVQ:
	{
		gVongQuay.OpenVongQuay();
	}
	break;

#if(REDEEMCODE)
	case eButtonRedeemCode:
	{
		if (gCB_RedeemCodeWindow) gCB_RedeemCodeWindow->OpenWindow();
	}
	break;
#endif

	default:
		break;
	}
	gInterface.Data[eMenu_MAIN].OnShow = 0;
}

void cCustomMenu::Draw()
{

	if (!gInterface.Data[eMenu_MAIN].OnShow) return;

	float MainWidth = 230;
	float MainHeight = 290;
#if defined(__ANDROID__) || defined(MU_IOS)
	// Nudged down from dead-center - the top edge (and its close button) used to
	// sit right under the Android top bar's icon row.
	float StartY = ((DisplayHeight - 51) / 2) - (MainHeight / 2) + 50.0f;
#else
	float StartY = ((DisplayHeight - 51) / 2) - (MainHeight / 2);
#endif
	float StartX = (DisplayWin / 2) - (MainWidth / 2);

	//--
	g_pBCustomMenuInfo->gDrawWindowCustom(&StartX, &StartY, MainWidth, MainHeight, eMenu_MAIN, "Menu");

	gInterface.DrawFormat(eGold, (int)StartX + 10, (int)StartY + 35, 210, 3, "%s : %s", gCustomMessage.GetMessage(50), gInterface.NumberFormat(CharacterMachine->Gold));

	gInterface.DrawFormat(eWhite, (int)StartX + 25, (int)StartY + 55, 40, 1, gCustomMessage.GetMessage(51));
	gInterface.DrawFormat(eGold, (int)StartX + 50, (int)StartY + 55, 40, 1, "%s", gInterface.NumberFormat(CharacterAttribute->PrintPlayer.Coin1));

	gInterface.DrawFormat(eWhite, (int)StartX + 90, (int)StartY + 55, 40, 1, gCustomMessage.GetMessage(52));
	gInterface.DrawFormat(eGold, (int)StartX + 115, (int)StartY + 55, 40, 1, "%s", gInterface.NumberFormat(CharacterAttribute->PrintPlayer.Coin2));

	gInterface.DrawFormat(eWhite, (int)StartX + 153, (int)StartY + 55, 40, 1, gCustomMessage.GetMessage(53));
	gInterface.DrawFormat(eGold, (int)StartX + 176, (int)StartY + 55, 40, 1, "%s", gInterface.NumberFormat(CharacterAttribute->PrintPlayer.Coin3));


	// Collect enabled slots first, then paginate that filtered list - some
	// slots are off (Menu[i]==0) on any given server, so pagination has to
	// be based on how many buttons actually show, not raw slot index range.
	int enabledSlots[15];
	int enabledCount = 0;
	for (int i = 0; i < 15; i++)
	{
		if (gProtect.m_MainInfo.Menu[i])
		{
			enabledSlots[enabledCount++] = i;
		}
	}

	const int pageCount = (enabledCount == 0) ? 1 : ((enabledCount - 1) / kCustomMenuButtonsPerPage) + 1;
	if (this->m_CurrentPage >= pageCount)
	{
		// Self-correcting rather than needing an explicit reset when the menu
		// opens/closes - covers a server that used to have more buttons
		// enabled than it does now, and the ordinary case of the window
		// simply being reopened from whatever page it was last left on.
		this->m_CurrentPage = 0;
	}

	int BtStartX = StartX + 8;
	int BtStartY = StartY + 78;
	float BtnW = (MainWidth - 17) / 2;
	float KhoangCachX = BtnW + 3;
	float KhoangCachY = 32;
	int cX = 0;
	int cY = 0;

	const int pageStart = this->m_CurrentPage * kCustomMenuButtonsPerPage;
	int pageEnd = pageStart + kCustomMenuButtonsPerPage;
	if (pageEnd > enabledCount)
	{
		pageEnd = enabledCount;
	}

	for (int n = pageStart; n < pageEnd; n++)
	{
		int i = enabledSlots[n];

		// CommonManager\MenuName.txt, once an operator has migrated a slot
		// to it (GetMainInfo.cpp/MenuName.h) - an empty slot there (not
		// migrated yet) falls back to the old Text_Button/Text_<lang>.ini
		// label, so nothing changes for a server that hasn't set this up.
		char* label = (gProtect.m_MainInfo.MenuName[i][0] != '\0')
			? gProtect.m_MainInfo.MenuName[i]
			: gOther.Text_Button[i];

		if (g_pBCustomMenuInfo->DrawButton(BtStartX + (cX * KhoangCachX), BtStartY + (cY * KhoangCachY), 170, 12, label, BtnW))
		{
			this->ActionButton(i);
		}
		cX++;
		if (cX > 1)
		{
			cX = 0;
			cY++;
		}
	}

	if (pageCount > 1)
	{
		// Bottom row, below the last button row (6 rows max at 12/page) and
		// clear of the window's own bottom edge.
		float NavY = StartY + MainHeight - 24;

		if (this->m_CurrentPage > 0)
		{
			if (g_pBCustomMenuInfo->DrawButton(StartX + 8, NavY, 170, 12, "< Prev", BtnW))
			{
				this->m_CurrentPage--;
			}
		}

		char PageLabel[16];
		sprintf_s(PageLabel, sizeof(PageLabel), "%d / %d", this->m_CurrentPage + 1, pageCount);
		gInterface.DrawFormat(eWhite, (int)(StartX + (MainWidth / 2) - 15), (int)NavY + 2, 40, 3, "%s", PageLabel);

		if (this->m_CurrentPage < pageCount - 1)
		{
			if (g_pBCustomMenuInfo->DrawButton(StartX + 8 + KhoangCachX, NavY, 170, 12, "Next >", BtnW))
			{
				this->m_CurrentPage++;
			}
		}
	}

}