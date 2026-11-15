#include "StdAfx.h"
#include "CB_ActiveInvasions.h"
#include "NewUISystem.h"
#include "CBInterface.h"
#include "CUIController.h"
#include "CharacterManager.h"
#include "Util.h"
#include "Protocol.h"
#include "NewUIBase.h"
#include "Other.h"
#include "ZzzInterface.h"
#include "CB_DuelRanked.h"
#include "NewUIBCustomMenu.h"
#include "Input.h"

#if(CB_ActiveInvasionsD)
CB_ActiveInvasions* gCB_ActiveInvasions;

CB_ActiveInvasions::CB_ActiveInvasions() :  is_update(false)
{
	this->m_data.clear();
}

CB_ActiveInvasions::~CB_ActiveInvasions()
{
}
void CB_ActiveInvasions::list_recv(PMSG_ACTIVE_INVASIONS_RECV* lpMsg)
{

	this->m_data.clear();

	for (int n = 0; n < lpMsg->count; n++)
	{
		auto info = reinterpret_cast<PMSG_ACTIVE_INVASIONS*>(reinterpret_cast<BYTE*>(lpMsg) + sizeof(PMSG_ACTIVE_INVASIONS_RECV) + (sizeof(
			PMSG_ACTIVE_INVASIONS) * n));

		ACTIVE_INVASIONS data;
		data.monster_id = info->monster_id;
		data.count = info->count;
		data.is_update = false;
		data.blink_counter = 0;
		data.blink_type = false;

		this->m_data.push_back(data);
	}

	if (lpMsg->count)
		this->is_update = true;
}
void CB_ActiveInvasions::update_monster_recv(PMSG_INVASION_MONSTER_UPDATE_RECV* lpMsg)
{
	for (auto it = this->m_data.begin(); it != this->m_data.end();)
	{
		if (it->monster_id == lpMsg->monster_id)
		{
			this->is_update = true;
			it->is_update = true;
			it->count._count = lpMsg->count;

			if (it->count._count == 0)
				it = this->m_data.erase(it);

			break;
		}

		++it;
	}
}
extern float NoticeInverse;
int startValue = 0;

// Modern look (code drawn, same helpers as the event windows): a small "Invasion" tab at the top centre that only exists while an
// invasion is going on, and a list panel (monster, killed/total) under the top-right corner when the tab is clicked.
void CB_ActiveInvasions::DrawWindow()
{
	if (this->m_data.empty())
	{
		// no invasion running: nothing is shown, and the panel starts closed for the next one
		gInterface.Data[eActiveInvasionsMain].OnShow = false;
		startValue = 0;
		return;
	}

	if (gInterface.CheckWindow(Interface::MoveList) || gInterface.CheckWindow(Interface::ObjWindow::CashShop) || gInterface.CheckWindow(Interface::ObjWindow::SkillTree) || gInterface.CheckWindow(Interface::ObjWindow::FullMap)
		|| (gInterface.CheckWindow(Interface::Inventory)
			&& gInterface.CheckWindow(Interface::ExpandInventory)
			&& gInterface.CheckWindow(Interface::Store))
		|| (gInterface.CheckWindow(Interface::Inventory)
			&& gInterface.CheckWindow(Interface::Warehouse)
			&& gInterface.CheckWindow(Interface::ExpandWarehouse)))
	{
		gInterface.Data[eActiveInvasionsMain].OnShow = false;
		return;
	}

	const DWORD kGold = RankedUi::Col(255, 200, 80);
	const DWORD kWhite = RankedUi::Col(255, 255, 255);
	const DWORD kNew = RankedUi::Col(190, 130, 255);

	// the tab
	char tab[48];
	sprintf_s(tab, "INVASION  (%d)", (int)this->m_data.size());
	const float tabW = 130.0f, tabH = 20.0f;
	const float tabX = (float)MAX_WIN_WIDTH * 0.5f - tabW * 0.5f;
	const float tabY = 0.0f;
	const bool pulse = ((int)NoticeInverse % 20 < 5);

	RankedUi::Bar(tabX, tabY, tabW, tabH, RankedUi::Col(12, 12, 16, 215));
	RankedUi::Outline(tabX, tabY, tabW, tabH, pulse ? RankedUi::Col(255, 235, 150) : RankedUi::Col(214, 170, 70));
	RankedUi::DrawDiamond(tabX + 10.0f, tabY + tabH * 0.5f, 3.0f, kGold);
	RankedUi::TextBox(g_hFontBold, tabX, tabY, tabW, tabH, RT3_SORT_CENTER, kGold, tab);

	if (RankedUi::Inside(tabX, tabY, tabW, tabH) && SEASON3B::IsPress(VK_LBUTTON))
	{
		gInterface.Data[eActiveInvasionsMain].OnShow ^= 1;
	}

	if (!gInterface.Data[eActiveInvasionsMain].OnShow)
	{
		return;
	}

	// the list
	const int perPage = 12;
	const int total = (int)this->m_data.size();

	if (startValue >= total) startValue = 0;

	const int rows = min(perPage, total - startValue);
	const float rowH = 16.0f;
	const bool paged = (total > perPage);
	const float W = 200.0f;
	const float H = 30.0f + rows * rowH + (paged ? 26.0f : 8.0f);

	// Opens in the middle of the screen, then the player can drag it by its title bar (the position is kept).
	static float s_X = 0.0f, s_Y = 0.0f, s_DX = 0.0f, s_DY = 0.0f;
	static bool s_Set = false, s_Dragging = false, s_WasDown = false;

#if defined(MU_TOUCH_UI)
	// The phone's MouseLButton / IsLBtnHeldDn flags stay set between touches (the debug log showed them never dropping), so
	// the held state comes from the key state of the left button only: a new press, then repeat while it is held.
	const bool down = SEASON3B::IsPress(VK_LBUTTON) || SEASON3B::IsRepeat(VK_LBUTTON);
#else
	const bool down = ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
#endif
	const bool clickEdge = SEASON3B::IsPress(VK_LBUTTON);
	s_WasDown = down;

	if (!s_Set)
	{
		s_X = ((float)MAX_WIN_WIDTH - W) * 0.5f;
		s_Y = ((float)MAX_WIN_HEIGHT - H) * 0.5f;
		s_Set = true;
	}

	if (clickEdge && RankedUi::Inside(s_X, s_Y, W, 24.0f))
	{
		s_Dragging = true;
		s_DX = (float)MouseX - s_X;
		s_DY = (float)MouseY - s_Y;
	}
	if (!down) s_Dragging = false;
	if (s_Dragging)
	{
		s_X = (float)MouseX - s_DX;
		s_Y = (float)MouseY - s_DY;
	}
	if (s_X < 0.0f) s_X = 0.0f;
	if (s_X > (float)MAX_WIN_WIDTH - W) s_X = (float)MAX_WIN_WIDTH - W;
	if (s_Y < 0.0f) s_Y = 0.0f;
	if (s_Y > (float)MAX_WIN_HEIGHT - H) s_Y = (float)MAX_WIN_HEIGHT - H;

	const float X = s_X;
	const float Y = s_Y;

	if (RankedUi::Inside(X, Y, W, H) && g_pBCustomMenuInfo != NULL)
	{
		g_pBCustomMenuInfo->SetBlockCur(TRUE);
	}

	InterfaceObject& d = gInterface.Data[eActiveInvasionsMain];
	d.X = X; d.Y = Y; d.Width = W; d.Height = H; d.FirstLoad = true;

	RankedUi::Bar(X, Y, W, H, RankedUi::Col(12, 12, 16, 225));
	RankedUi::Outline(X, Y, W, H, RankedUi::Col(214, 170, 70));
	RankedUi::Outline(X + 2.0f, Y + 2.0f, W - 4.0f, H - 4.0f, RankedUi::Col(70, 58, 34));
	RankedUi::DrawDiamond(X + 12.0f, Y + 13.0f, 3.5f, kGold);
	RankedUi::DrawDiamond(X + W - 12.0f, Y + 13.0f, 3.5f, kGold);
	RankedUi::Text(g_hFontBold, X, Y + 6.0f, W, RT3_SORT_CENTER, kGold, "INVASION  (drag to move)");

	for (int c = 0; c < rows; c++)
	{
		ACTIVE_INVASIONS& e = this->m_data[startValue + c];

		char count[32];
		sprintf_s(count, "%d/%d", e.count._count, e.count._max_count);

		const float ry = Y + 28.0f + c * rowH;
		const DWORD color = e.is_update ? kNew : kWhite;

		if (c % 2 == 0)
		{
			RankedUi::Bar(X + 5.0f, ry - 1.0f, W - 10.0f, rowH - 1.0f, RankedUi::Col(255, 255, 255, 14));
		}

		RankedUi::Text(g_hFont, X + 10.0f, ry + 1.0f, W - 80.0f, RT3_SORT_LEFT, color, getMonsterName(e.monster_id));
		RankedUi::Text(g_hFontBold, X + W - 70.0f, ry + 1.0f, 60.0f, RT3_SORT_RIGHT, color, count);

		if (RankedUi::Inside(X + 5.0f, ry - 1.0f, W - 10.0f, rowH - 1.0f))
		{
			e.is_update = false;
		}
	}

	if (paged)
	{
		const float by = Y + H - 24.0f;
		char page[24];
		sprintf_s(page, "%d / %d", startValue / perPage + 1, (total + perPage - 1) / perPage);
		RankedUi::Text(g_hFont, X, by + 5.0f, W, RT3_SORT_CENTER, kGold, page);

		RankedUi::DrawButton(X + 10.0f, by, 30.0f, 20.0f, "<", 0, startValue > 0, true);
		RankedUi::DrawButton(X + W - 40.0f, by, 30.0f, 20.0f, ">", 0, startValue + perPage < total, true);

		if (SEASON3B::IsPress(VK_LBUTTON))
		{
			if (startValue > 0 && RankedUi::Inside(X + 10.0f, by, 30.0f, 20.0f)) startValue -= perPage;
			else if (startValue + perPage < total && RankedUi::Inside(X + W - 40.0f, by, 30.0f, 20.0f)) startValue += perPage;
		}
	}
}
#endif
