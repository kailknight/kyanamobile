#include "StdAfx.h"
#include "CB_AutoResetInfo.h"
#include "NewUISystem.h"
#include "CBInterface.h"
#include "CUIController.h"
#include "CharacterManager.h"
#include "Util.h"
#include "Protocol.h"
#include "NewUIBase.h"
#include "Other.h"
#include "ZzzInterface.h"


#if(CB_AUTORESETINFO)
CBAutoResetInfo* gCBAutoResetInfo = nullptr;

CBAutoResetInfo::CBAutoResetInfo()
{
	this->ViewInfoResetWindow.Clear();
	//====AutoReset Info========//
	this->TypeReset = 0;
	this->GHRSReset = 0;
	this->GHRSRelife = 0;
	this->LvlReset = 0;
	this->LoadingInfo = 0;
	//==========//

	this->m_LevelRelife = 0;
	this->m_RelifeCong = 0;
	this->m_MaxRelife = 0;
	this->m_LevelSauKhiRL = 0;
	this->m_ResetSauKhiRL = 0;
	this->m_PointRelife = 0;
}


CBAutoResetInfo::~CBAutoResetInfo()
{

}

void CBAutoResetInfo::GCCharacterAutoResetRecv(BYTE* Recv) // OK
{
	PMSG_AUTORESET_INFO_RECV* lpMsg = (PMSG_AUTORESET_INFO_RECV*)Recv;
	this->m_LevelRelife = lpMsg->LevelRelife;
	this->m_RelifeCong = lpMsg->RelifeCong;
	this->m_MaxRelife = lpMsg->MaxRelife;
	this->m_LevelSauKhiRL = lpMsg->LevelSauKhiRL;
	this->m_ResetSauKhiRL = lpMsg->ResetSauKhiRL;
	this->m_PointRelife = lpMsg->PointRelife;


	if (!gCBAutoResetInfo) return;
	if (GetTickCount() < gInterface.Data[eWindowRelife].EventTick + 300) return;
	gInterface.Data[eWindowRelife].EventTick = GetTickCount();
	if (gInterface.Data[eWindowRelife].OnShow)
	{
		gInterface.Data[eWindowRelife].OnShow = 0;
		return;
	}
	gInterface.Data[eWindowRelife].OnShow = 1;
}
void CBAutoResetInfo::DrawWindowRelife()
{
	if (gInterface.CheckWindow(Interface::MoveList) || gInterface.CheckWindow(Interface::CashShop) || gInterface.CheckWindow(Interface::SkillTree) || gInterface.CheckWindow(Interface::FullMap)
		|| (gInterface.CheckWindow(Interface::Inventory)
			&& gInterface.CheckWindow(Interface::ExpandInventory)
			&& gInterface.CheckWindow(Interface::Store))
		|| (gInterface.CheckWindow(Interface::Inventory)
			&& gInterface.CheckWindow(Interface::Warehouse)
			&& gInterface.CheckWindow(Interface::ExpandWarehouse)) ||
		//gCBDauGiaItem.SelectItemDauGia == -1 ||
		GetKeyState(VK_ESCAPE) < 0)
	{
		gInterface.Data[eWindowRelife].OnShow = false;
		return;
	}

	if (!gInterface.Data[eWindowRelife].OnShow)
	{
		return;
	}
	float WindowW = 280;
	float WindowH = 260;
	gInterface.Data[eWindowRelife].Width = WindowW;
	float StartX = (MAX_WIN_WIDTH / 2) - (WindowW / 2);
	float StartY = 25.0;
	//if (!gCBUtil.gDrawWindowCustom(&StartX, &StartY, WindowW, WindowH, eWindowAutoReset, "Auto Reset Info"))
	g_pBCustomMenuInfo->gDrawWindowCustom(&StartX, &StartY, WindowW, WindowH, eWindowRelife, "Relife Info");
	DWORD Color = eGray100;
	float CuaSoW = WindowW;
	//====================================
	float TextStartY = StartY;

	int ct = 4;
	TextDraw(g_hFont, (StartX + (CuaSoW / 10)), TextStartY + (12 * ct++), 0xEBFFFBFF, 0x0, CuaSoW, 0, 1, "• You have reached the Reset limit, Relife is required to continue Reset"); //
	TextDraw(g_hFont, (StartX + (CuaSoW / 10)), TextStartY + (12 * ct++), 0xEBFFFBFF, 0x0, CuaSoW, 0, 1, "• After Relife you will receive %d Point", this->m_PointRelife); //
	TextDraw(g_hFont, (StartX + (CuaSoW / 10)), TextStartY + (12 * ct++), 0xEBFFFBFF, 0x0, CuaSoW, 0, 1, "• Reset returns to %d, Level returns to %d!", this->m_ResetSauKhiRL, this->m_LevelSauKhiRL); //
	ct += 1;
	TextDraw(g_hFont, StartX, TextStartY + (12 * ct++), 0xFFD51CFF, 0x0, CuaSoW, 0, 3, "Requires Level %d to Relife", this->m_LevelRelife); //
	TextDraw(g_hFont, StartX, TextStartY + (12 * ct++), 0xFFD51CFF, 0x0, CuaSoW, 0, 3, "You will get +%d Relife for the next time", this->m_RelifeCong); //
	TextDraw(g_hFont, StartX, TextStartY + (12 * ct++), 0xFFD51CFF, 0x0, CuaSoW, 0, 3, "You can Relife a maximum of %d times!", this->m_MaxRelife); //



	if (g_pBCustomMenuInfo->DrawButton(StartX + (WindowW / 2) - (120 / 2), StartY + (WindowH - 60), 150, 12, "Perform Relife", 120) && (GetTickCount() - gInterface.Data[eWindowRelife].EventTick) > 300)
	{
		XULY_CGPACKET pMsg;
		pMsg.header.set(0xD3, 0x36, sizeof(pMsg));
		pMsg.ThaoTac = 3; //
		DataSend((LPBYTE)& pMsg, pMsg.header.size);
	}
}
void CBAutoResetInfo::RecvProtocol(BYTE* Recv)
{
	if (!Recv) return;
	DKRS_COUNTLIST* mRecv = (DKRS_COUNTLIST*)Recv;
	gCBAutoResetInfo->ViewInfoResetWindow.Clear();
	this->LoadingInfo = false;

	gCBAutoResetInfo->ViewInfoResetWindow.Level = mRecv->Level;
	gCBAutoResetInfo->ViewInfoResetWindow.Point = mRecv->Point;
	gCBAutoResetInfo->ViewInfoResetWindow.Zen = mRecv->Zen;
	gCBAutoResetInfo->ViewInfoResetWindow.WC = mRecv->WC;
	gCBAutoResetInfo->ViewInfoResetWindow.WP = mRecv->WP;
	gCBAutoResetInfo->ViewInfoResetWindow.GP = mRecv->GP;
	gCBAutoResetInfo->ViewInfoResetWindow.ResetView = mRecv->ResetView;
	gCBAutoResetInfo->ViewInfoResetWindow.ResetDay = mRecv->ResetDay;
	gCBAutoResetInfo->ViewInfoResetWindow.MaxGHRS = mRecv->MaxGHRS;
	gCBAutoResetInfo->ViewInfoResetWindow.OnResetType = mRecv->OnResetType;
	gCBAutoResetInfo->ViewInfoResetWindow.LoaiReset = mRecv->LoaiReset;
	gCBAutoResetInfo->ViewInfoResetWindow.AutoResetEnable = mRecv->AutoResetEnable;
	if (gCBAutoResetInfo->ViewInfoResetWindow.LoaiReset == 0)gCBAutoResetInfo->ViewInfoResetWindow.Point = 0;
	gCBAutoResetInfo->ViewInfoResetWindow.TypeReset = mRecv->TypeReset;
	gCBAutoResetInfo->ViewInfoResetWindow.CodeReset = mRecv->CodeReset;
	gCBAutoResetInfo->ViewInfoResetWindow.RewardWC = mRecv->RewardWC;
	gCBAutoResetInfo->ViewInfoResetWindow.RewardWP = mRecv->RewardWP;
	gCBAutoResetInfo->ViewInfoResetWindow.RewardGP = mRecv->RewardGP;
	gCBAutoResetInfo->ViewInfoResetWindow.KeepStats = mRecv->KeepStats;
	gCBAutoResetInfo->ViewInfoResetWindow.NoLevelUpPoint = mRecv->NoLevelUpPoint;
	gCBAutoResetInfo->ViewInfoResetWindow.ResetMove = mRecv->ResetMove;
	
	//==Get point def
	for (int i = 0; i < 5; i++)
	{
		gCBAutoResetInfo->ViewInfoResetWindow.DefStat[i] = mRecv->DefStat[i];
	}
	gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck.clear();
	for (int n = 0; n < mRecv->Count; n++)
	{
		DKRS_ITEMINFO* lpInfo = (DKRS_ITEMINFO*)(((BYTE*)Recv) + sizeof(DKRS_COUNTLIST) + (sizeof(DKRS_ITEMINFO) * n));

		gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck.push_back(*lpInfo);
	}
	if (gCBAutoResetInfo->ViewInfoResetWindow.OnResetType > 0)
	{
		if (!gInterface.Data[eWindowAutoReset].OnShow) gInterface.Data[eWindowAutoReset].OnShow = 1;
	}
	this->LoadingInfo = 1;
	//gInterface.DrawMessage(1, "Open Window Auto Reset %d", gCBAutoResetInfo->ViewInfoResetWindow.OnResetType);
}
void CBAutoResetInfo::OpenWindow()
{
	if (!gCBAutoResetInfo) return;
	if (GetTickCount() < gInterface.Data[eWindowAutoReset].EventTick + 300) return;
	gInterface.Data[eWindowAutoReset].EventTick = GetTickCount();
	if (gInterface.Data[eWindowAutoReset].OnShow)
	{
		gInterface.Data[eWindowAutoReset].OnShow = 0;
		return;
	}

	XULY_CGPACKET pMsg;
	pMsg.header.set(0xD3, 0x36, sizeof(pMsg));
	pMsg.ThaoTac = 1; //
	DataSend((LPBYTE)& pMsg, pMsg.header.size);

}
char* CBAutoResetInfo::GetItemName(int ItemType, int Level)
{

	return BGetItemName(ItemType, Level);

}
CUITextInputBox* InputAddPointReset[5] = { nullptr };
CUITextInputBox* InputCodeReset = { nullptr };
char TextInputAddPoint[4][6] = { 0 };
char TextInputCodeReset[11] = { 0 };
char* NameTypeReset[] = { "Keep Point", "Point*Reset", "Keep Point + Point" };

int PointReAdd = 0;

void WindowDieuKienResetInfo(int X, int Y, int W)
{
	float StartX = X;
	float StartY = Y;
	float WindowW = W;
	StartY += 35;
	//TextDraw((HFONT)g_hFontBold, StartX + 10, StartY, 0xEB8213FF, 0x0, WindowW, 0, 1, "**Coin:");
	TextDraw((HFONT)g_hFontBold, StartX + 10, StartY, 0xEB8213FF, 0x0, WindowW, 0, 1, "- Required Materials:");
	StartY += 15;

	float XText = 25;
	float BGW = 35;
	float BGH = 12;
	float cStartX = StartX + 10;
	float kkW = BGW + XText + 10;
	int GetCountLine = 0;
	if (gCBAutoResetInfo->ViewInfoResetWindow.Zen)
	{
		gInterface.DrawBarForm(cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0), StartY + (GetCountLine > 1 ? 15 : 0), BGW + XText, BGH, 1, 0.2667, 0, 0.8);
		TextDraw((HFONT)g_hFontBold, cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0), StartY + (GetCountLine > 1 ? 15 : 0) + 1.5, 0xFFD000A9, 0x0, XText, 0, 3, "Zen");
		gInterface.DrawBarForm(cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0) + XText, StartY + (GetCountLine > 1 ? 15 : 0), BGW, BGH, 0.0, 0.0, 0.0, 0.8);
		TextDraw((HFONT)g_hFont, cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0) + XText, StartY + (GetCountLine > 1 ? 15 : 0) + 1.5, 0xFFFFFFFF, 0x0, BGW, 0, 3, "%s", gInterface.NumberFormat(gCBAutoResetInfo->ViewInfoResetWindow.Zen));
		GetCountLine++;
	}

	if (gCBAutoResetInfo->ViewInfoResetWindow.WC)
	{
		gInterface.DrawBarForm(cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0), StartY + (GetCountLine > 1 ? 15 : 0), BGW + XText, BGH, 1, 0.2667, 0, 0.8);
		TextDraw((HFONT)g_hFontBold, cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0), StartY + (GetCountLine > 1 ? 15 : 0) + 1.5, 0xFFD000A9, 0x0, XText, 0, 3, "WC");
		gInterface.DrawBarForm(cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0) + XText, StartY + (GetCountLine > 1 ? 15 : 0), BGW, BGH, 0.0, 0.0, 0.0, 0.8);
		TextDraw((HFONT)g_hFont, cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0) + XText, StartY + (GetCountLine > 1 ? 15 : 0) + 1.5, 0xFFFFFFFF, 0x0, BGW, 0, 3, "%s", gInterface.NumberFormat(gCBAutoResetInfo->ViewInfoResetWindow.WC));
		GetCountLine++;
	}
	if (gCBAutoResetInfo->ViewInfoResetWindow.WP)
	{
		gInterface.DrawBarForm(cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0), StartY + (GetCountLine > 1 ? 15 : 0), BGW + XText, BGH, 1, 0.2667, 0, 0.8);
		TextDraw((HFONT)g_hFontBold, cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0), StartY + (GetCountLine > 1 ? 15 : 0) + 1.5, 0xFFD000A9, 0x0, XText, 0, 3, "WP");
		gInterface.DrawBarForm(cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0) + XText, StartY + (GetCountLine > 1 ? 15 : 0), BGW, BGH, 0.0, 0.0, 0.0, 0.8);
		TextDraw((HFONT)g_hFont, cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0) + XText, StartY + (GetCountLine > 1 ? 15 : 0) + 1.5, 0xFFFFFFFF, 0x0, BGW, 0, 3, "%s", gInterface.NumberFormat(gCBAutoResetInfo->ViewInfoResetWindow.WP));
		GetCountLine++;
	}
	if (gCBAutoResetInfo->ViewInfoResetWindow.GP)
	{
		gInterface.DrawBarForm(cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0), StartY + (GetCountLine > 1 ? 15 : 0), BGW + XText, BGH, 1, 0.2667, 0, 0.8);
		TextDraw((HFONT)g_hFontBold, cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0), StartY + (GetCountLine > 1 ? 15 : 0) + 1.5, 0xFFD000A9, 0x0, XText, 0, 3, "GP");
		gInterface.DrawBarForm(cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0) + XText, StartY + (GetCountLine > 1 ? 15 : 0), BGW, BGH, 0.0, 0.0, 0.0, 0.8);
		TextDraw((HFONT)g_hFont, cStartX + ((GetCountLine == 1 || GetCountLine == 3) ? kkW : 0) + XText, StartY + (GetCountLine > 1 ? 15 : 0) + 1.5, 0xFFFFFFFF, 0x0, BGW, 0, 3, "%s", gInterface.NumberFormat(gCBAutoResetInfo->ViewInfoResetWindow.GP));
		GetCountLine++;
	}
	if (!gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck.empty())
	{
		StartY += 35;

		TextDraw((HFONT)g_hFontBold, StartX + 10, StartY, 0xEB8213FF, 0x0, WindowW, 0, 1, "**Items:");
		//===Draw List
		StartY += 15;
		float InfoListX = StartX + 10;
		float InfoListY = StartY;
		float InfoListW = W - 20;
		int Count = 0;
		DWORD ColorSelect = 0x0;
		for (int n = 0; n < gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck.size(); n++)
		{


			ITEM* CTItem = g_pNewItemMng->CreateItem(gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck[n].Item);
			int CTItemIndex = CTItem->Type;

			//BYTE m_Skill = (gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck[n].Item[1] / 128) & 1;
			//BYTE m_Luck = (gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck[n].Item[1] / 4) & 1;
			//BYTE m_Opt = (gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck[n].Item[1] & 3) + ((gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck[n].Item[7] & 64) / 16);
			if (Count >= 5) break;
			if (SEASON3B::CheckMouseIn(InfoListX + 5, (InfoListY + 1) + (Count * 12), InfoListW + 15, 13) == 1)
			{
				ColorSelect = 0xABABAB64;
				//	gPostInterface.item_post_ = CTItem;

				//gInterface.DrawItemToolTipText(CTItem, *(int*)0x879340C, *(int*)0x8793410 + 25);
				//glColor3f(1, 1, 1);
				//pSetBlend(false);
				RenderItemInfo(MouseX + 75, MouseY, CTItem, 0, 0, false, false);
			}
			else
			{
				ColorSelect = 0x0;
			}


			//TextDraw((HFONT)g_hFont, InfoListX + 5, (InfoListY + 5) + (Count * 12), (gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck[n].Status == 1) ? 0xFFDD00FF : 0xFC4023FF, ColorSelect, InfoListW + 15, 0, 1, "[x%d] %s", gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck[n].SL, gCBAutoResetInfo->GetItemName(CTItemIndex, CTItem->Level));//
			TextDraw((HFONT)g_hFont, InfoListX + 5, (InfoListY + 5) + (Count * 12), (gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck[n].Status == 1) ? 0xFFDD00FF : 0xFC4023FF, ColorSelect, InfoListW + 15, 0, 1, "[x%d] %s", gCBAutoResetInfo->ViewInfoResetWindow.m_ItemCheck[n].SL, gCBAutoResetInfo->GetItemName(CTItemIndex, CTItem->Level));//
			Count++;

		}
	}
}
void CBAutoResetInfo::DrawWindow()
{

	if (!gCBAutoResetInfo) return;

	this->DrawWindowRelife();

	//if (GetKeyState(VK_F3) & 0x4000)
	//{
	//	gCBAutoResetInfo->OpenWindow();
	//}
	if (gInterface.CheckWindow(Interface::MoveList) || gInterface.CheckWindow(Interface::CashShop) || gInterface.CheckWindow(Interface::SkillTree) || gInterface.CheckWindow(Interface::FullMap)
		|| (gInterface.CheckWindow(Interface::Inventory)
			&& gInterface.CheckWindow(Interface::ExpandInventory)
			&& gInterface.CheckWindow(Interface::Store))
		|| (gInterface.CheckWindow(Interface::Inventory)
			&& gInterface.CheckWindow(Interface::Warehouse)
			&& gInterface.CheckWindow(Interface::ExpandWarehouse)) ||
		//gCBDauGiaItem.SelectItemDauGia == -1 ||
		GetKeyState(VK_ESCAPE) < 0)
	{
		gInterface.Data[eWindowAutoReset].OnShow = false;
		return;
	}

	if (!gInterface.Data[eWindowAutoReset].OnShow || this->LoadingInfo == 0 || gCBAutoResetInfo->ViewInfoResetWindow.LoaiReset < 0 || gCBAutoResetInfo->ViewInfoResetWindow.LoaiReset > 2)
	{
		return;
	}
	// Laid out for this server's reset: stats are kept, every reset pays a coin
	// reward and levelling gives no points (CustomConfig.ini ResetKeepStats /
	// ResetReward* / ResetNoLevelUpPoint, sent with the reset info). Colours
	// are TextDraw's 0xAABBGGRR.
	const VIEWINFO_RESETLIST& info = gCBAutoResetInfo->ViewInfoResetWindow;

	float WindowW = 300;
	float WindowH = 235;
	gInterface.Data[eWindowAutoReset].Width = WindowW;
	gInterface.Data[eWindowAutoReset].Height = WindowH;
	float StartX = (MAX_WIN_WIDTH / 2) - (WindowW / 2);
	float StartY = 25.0;
	g_pBCustomMenuInfo->gDrawWindowCustom(&StartX, &StartY, WindowW, WindowH, eWindowAutoReset, "Reset");

	const DWORD colorTitle = 0xFF40D7FF;  // gold
	const DWORD colorLabel = 0xFFFFFFFF;  // white
	const DWORD colorValue = 0xFF40D7FF;  // gold
	const DWORD colorOk = 0xFF60E060;     // green
	const DWORD colorNo = 0xFF5050FF;     // red
	const DWORD colorReward = 0xFFFFE040; // cyan
	const DWORD colorNote = 0xFF50B0FF;   // soft orange

	// The reward, from CustomConfig.ini's ResetReward*.
	char reward[128] = { 0 };
	{
		char part[48];
		if (info.RewardWC > 0)
		{
			sprintf(part, "%s WCoinC", gInterface.NumberFormat(info.RewardWC));
			strcat(reward, part);
		}
		if (info.RewardWP > 0)
		{
			sprintf(part, "%s%s WCoinP", reward[0] ? ", " : "", gInterface.NumberFormat(info.RewardWP));
			strcat(reward, part);
		}
		if (info.RewardGP > 0)
		{
			sprintf(part, "%s%s GP", reward[0] ? ", " : "", gInterface.NumberFormat(info.RewardGP));
			strcat(reward, part);
		}
		if (reward[0] == 0)
		{
			strcpy(reward, "None");
		}
	}

	const float boxX = StartX + 10;
	const float boxW = WindowW - 20;
	float TextY = StartY + 35;

	g_pBCustomMenuInfo->DrawInfoBox(boxX, TextY, boxW - 2, 75, 0x00000096, 0, 0);
	TextY += 3;
	TextDraw((HFONT)g_hFontBold, boxX, TextY, colorTitle, 0x0, boxW, 0, 3, "[%s] Next Reset: %d", CharacterAttribute->Name, info.ResetView + 1);
	TextY += 7;
	TextDraw((HFONT)g_hFontBold, boxX, TextY, colorLabel, 0x0, boxW + 2, 0, 3, "------------------------------------------------------------------");
	TextY += 12;

	TextDraw((HFONT)g_hFont, boxX + 5, TextY, colorLabel, 0x0, boxW, 0, 1, "- Required Level:");
	TextDraw((HFONT)g_hFontBold, boxX - 5, TextY, (CharacterAttribute->Level >= info.Level) ? colorOk : colorNo, 0x0, boxW, 0, 4, "%d / %d", CharacterAttribute->Level, info.Level);
	TextY += 12;

	TextDraw((HFONT)g_hFont, boxX + 5, TextY, colorLabel, 0x0, boxW, 0, 1, "- Reset Type:");
	TextDraw((HFONT)g_hFontBold, boxX - 5, TextY, colorValue, 0x0, boxW, 0, 4, "%s", info.KeepStats ? "Keep Stats" : NameTypeReset[info.LoaiReset]);
	TextY += 12;

	TextDraw((HFONT)g_hFont, boxX + 5, TextY, colorLabel, 0x0, boxW, 0, 1, "- Reset Reward:");
	TextDraw((HFONT)g_hFontBold, boxX - 5, TextY, colorReward, 0x0, boxW, 0, 4, "%s", reward);
	TextY += 12;

	TextDraw((HFONT)g_hFont, boxX + 5, TextY, colorLabel, 0x0, boxW, 0, 1, "%s", info.ResetMove ? "- Moved to town after the reset" : "- Reset on spot");
	TextY += 24;

	// The disclaimer.
	if (info.NoLevelUpPoint)
	{
		TextDraw((HFONT)g_hFont, boxX, TextY, colorNote, 0x0, boxW, 0, 3, "Disclaimer: the Reset System only gives %s.", reward);
		TextY += 12;
		TextDraw((HFONT)g_hFont, boxX, TextY, colorNote, 0x0, boxW, 0, 3, "Leveling up doesn't give any LevelPoints.");
		TextY += 12;
	}

	// Anything the reset costs, only when the reset table asks for something.
	if (info.Zen != 0 || info.WC != 0 || info.WP != 0 || info.GP != 0 || !info.m_ItemCheck.empty())
	{
		WindowDieuKienResetInfo(boxX - 10, TextY - 30, boxW);
	}

	float ButtonW = 80;
	if (info.AutoResetEnable)
	{
		if (g_pBCustomMenuInfo->DrawButton(StartX + (WindowW / 4) - (ButtonW / 2), StartY + (WindowH - 40), 150, 12, "<Turn Off Auto Reset>", ButtonW) && (GetTickCount() - gInterface.Data[eWindowAutoReset].EventTick) > 300)
		{
			gInterface.Data[eWindowAutoReset].EventTick = GetTickCount();
			RESETCODE_SEND pMsg;
			pMsg.header.set(0xD3, 0x37, sizeof(pMsg));
			pMsg.TypeReset = -1;

			for (int i = 0; i < 5; i++)
			{
				pMsg.AutoResetStats[i] = 0;
			}
			memset(&pMsg.CodeReset, 0, sizeof(pMsg.CodeReset));
			DataSend((LPBYTE)& pMsg, pMsg.header.size);
		}
	}

	const float performX = info.AutoResetEnable ? (StartX + (WindowW / 2) + (ButtonW / 2)) : (StartX + (WindowW / 2) - (ButtonW / 2));

	if (g_pBCustomMenuInfo->DrawButton(performX, StartY + (WindowH - 40), 150, 12, "Perform Reset", ButtonW) && (GetTickCount() - gInterface.Data[eWindowAutoReset].EventTick) > 300)
	{
		gInterface.Data[eWindowAutoReset].EventTick = GetTickCount();
		RESETCODE_SEND pMsg;
		pMsg.header.set(0xD3, 0x37, sizeof(pMsg));
		pMsg.TypeReset = info.TypeReset;
		for (int i = 0; i < 5; i++)
		{
			pMsg.AutoResetStats[i] = 0;
		}
		memset(&pMsg.CodeReset, 0, sizeof(pMsg.CodeReset));
		memcpy(&pMsg.CodeReset, TextInputCodeReset, sizeof(pMsg.CodeReset));
		DataSend((LPBYTE)& pMsg, pMsg.header.size);
	}


}
#endif