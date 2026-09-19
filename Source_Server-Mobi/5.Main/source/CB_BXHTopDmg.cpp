#include "StdAfx.h"
#include "CB_BXHTopDmg.h"
#include "NewUISystem.h"
#include "CBInterface.h"
#include "CUIController.h"
#include "CharacterManager.h"
#include "Util.h"
#include "Protocol.h"
#include "NewUIBase.h"
#include "Other.h"
#include "ZzzInterface.h"

// Defined in NewUIBCustomMenu.cpp with no header declaration of its own.
// This is the drag handler every other custom window uses.
extern void MoveWindows(int eNumWindow);

CB_BXHTopDmg* gCB_BXHTopDmg;

CB_BXHTopDmg::CB_BXHTopDmg()
{
	this->m_DataTopBXHDMG.clear();
	this->ClassMonter = -1;
	this->Life = 0;
	this->MaxLife = 0;
	this->LastRecvTick = 0;
	this->RecvWorld = -1;
}

CB_BXHTopDmg::~CB_BXHTopDmg()
{
}

void CB_BXHTopDmg::RecvProtocol(BYTE* Recv)
{
	if (!Recv) return;
	this->m_DataTopBXHDMG.clear();
	this->ClassMonter = -1;
	PMSG_COUNT_SEND_DATA* lpMsg = (PMSG_COUNT_SEND_DATA*)Recv;
	this->ClassMonter = lpMsg->ClassMonter;
	this->Life = lpMsg->Life;
	this->MaxLife = lpMsg->MaxLife;
	this->LastRecvTick = GetTickCount();
	this->RecvWorld = gMapManager.WorldActive;
	for (int n = 0; n < lpMsg->count; n++)
	{
		DATA_BXH_SEND* lpInfo = (CB_BXHTopDmg::DATA_BXH_SEND*)(((BYTE*)lpMsg) + sizeof(PMSG_COUNT_SEND_DATA) + (sizeof(DATA_BXH_SEND) * n));
		this->m_DataTopBXHDMG.push_back(*lpInfo);
	}
	//gInterface.DrawMessage(1, "BXHDmg Recv Size %d %s %I64u", this->m_DataTopBXHDMG.size(), this->m_DataTopBXHDMG[0].Name, this->m_DataTopBXHDMG[0].damage);

	if (!this->m_DataTopBXHDMG.empty()) gInterface.Data[eWindowBXHDmg].OnShow = 1;
}

namespace
{
	// A one-pixel gold outline. This interface has no frame primitive, only
	// filled rectangles, so it is four thin bars.
	void DrawGoldFrame(float x, float y, float w, float h)
	{
		const GLfloat r = 0.62f, g = 0.48f, b = 0.20f, a = 1.0f;

		gInterface.DrawBarForm(x, y, w, 1.0f, r, g, b, a);
		gInterface.DrawBarForm(x, y + h - 1.0f, w, 1.0f, r, g, b, a);
		gInterface.DrawBarForm(x, y, 1.0f, h, r, g, b, a);
		gInterface.DrawBarForm(x + w - 1.0f, y, 1.0f, h, r, g, b, a);
	}
}

bool CB_BXHTopDmg::IsBossHpBarActive()
{
	if (this->MaxLife == 0 || this->m_DataTopBXHDMG.empty())
	{
		return false;
	}

	// Gone the moment you leave. The server only sends this packet to players
	// on the boss's map, inside 20 tiles, who are in its hit list - so walking
	// off or warping just stops the packets, and nothing announces that. Two
	// guards cover the two ways out:
	//
	//   - a map change is known instantly, so a warp clears the bar at once;
	//   - otherwise the bar lives a few seconds past the last packet, which is
	//     long enough to ride out a lull in a fight (packets only flow while
	//     the boss is actually being damaged) without leaving a dead bar up.
	if (gMapManager.WorldActive != this->RecvWorld)
	{
		return false;
	}

	const DWORD kBossHpHoldMs = 8000;

	if ((GetTickCount() - this->LastRecvTick) > kBossHpHoldMs)
	{
		return false;
	}

	return true;
}

// Free wrapper, for callers outside the custom-UI layer.
//
// CB_BXHTopDmg.h carries "using namespace SEASON3B", so including it from a
// core file drags that namespace in wholesale - in ZzzInterface.cpp that makes
// INTERFACE_NPCGUILDMASTER ambiguous against its own enum. A plain extern
// declaration at the call site avoids the whole problem.
bool BossHpBarShownFor(int MonsterClass)
{
	return (gCB_BXHTopDmg != NULL) && gCB_BXHTopDmg->IsBossHpBarShownFor(MonsterClass);
}

bool CB_BXHTopDmg::IsBossHpBarShownFor(int MonsterClass)
{
	if (MonsterClass < 0 || MonsterClass != this->ClassMonter)
	{
		return false;
	}

	return this->IsBossHpBarActive();
}

void CB_BXHTopDmg::DrawBossHpBar()
{
	if (this->IsBossHpBarActive() == false)
	{
		return;
	}

	QWORD Life = this->Life;

	if (Life > this->MaxLife)
	{
		Life = this->MaxLife;
	}

	// Long division, not float: boss HP runs to millions and a float loses
	// whole thousands at that size, so the bar would stick near full through
	// the opening hits.
	const int Percent = (int)((100 * (double)Life) / (double)this->MaxLife);

	// 40% smaller than the first pass: 420x22 -> 252x13.
	const float BarW = 252.0f;
	const float BarH = 13.0f;
	const float NameH = 20.0f;
	const float PercentW = 34.0f;

	// Name and bar move as one block, so the group's box is what gets dragged
	// and what gets clamped to the screen - not the bar alone.
	const float GroupW = BarW + PercentW;
	const float GroupH = NameH + BarH;

	InterfaceObject& Win = gInterface.Data[eWindowBossHpBar];

	if (!Win.FirstLoad)
	{
		// First run only. After this the position is whatever the player
		// dragged it to, which is why the default is applied here and not
		// every frame.
		//
		// CB_ActiveInvasions' "[Boss Info]" strip owns y 0..19 in the centre
		// column, so the default sits just below it.
		Win.X = (float)(MAX_WIN_WIDTH / 2) - (GroupW / 2);
		Win.Y = 22.0f;
		Win.AllowMove = true;
		Win.FirstLoad = true;
	}

	Win.Width = GroupW;
	Win.Height = GroupH;

	if (Win.AllowMove)
	{
		MoveWindows(eWindowBossHpBar);
	}

	const float X = Win.X;
	const float Y = Win.Y;

	// --- boss name --------------------------------------------------------
	TextDraw((HFONT)g_hFontBig, X, Y, 0xFFD27AFF, 0x0, BarW, 0, 3,
		"%s", getMonsterName(this->ClassMonter));

	// --- bar --------------------------------------------------------------
	const float BarX = X;
	const float BarY = Y + NameH;

	gInterface.DrawBarForm(BarX, BarY, BarW, BarH, 0.04f, 0.02f, 0.02f, 0.9f);

	const float FillW = (float)((BarW * (double)Life) / (double)this->MaxLife);

	if (FillW > 0.0f)
	{
		gInterface.DrawBarForm(BarX, BarY, FillW, BarH, 0.85f, 0.18f, 0.22f, 1.0f);
	}

	// Segment dividers, drawn over the fill so they read as notches cut out of
	// it rather than as marks floating on the empty track.
	const int SegmentCount = 10;

	for (int n = 1; n < SegmentCount; n++)
	{
		const float SegX = BarX + ((BarW / SegmentCount) * n);

		if (SegX < BarX + FillW)
		{
			gInterface.DrawBarForm(SegX - 1.0f, BarY, 1.0f, BarH, 0.10f, 0.02f, 0.03f, 1.0f);
		}
	}

	DrawGoldFrame(BarX, BarY, BarW, BarH);

	// --- percentage -------------------------------------------------------
	TextDraw((HFONT)g_hFontBold, BarX + BarW + 6.0f, BarY + 1.0f, 0xFF4D5AFF, 0x0, PercentW, 0, 1,
		"%d%%", Percent);
}

void CB_BXHTopDmg::DrawWindowMini()
{
	if (gInterface.CheckWindow(Interface::MoveList)
		|| gInterface.CheckWindow(Interface::ObjWindow::CreateGuild)
		|| gInterface.CheckWindow(Interface::ObjWindow::BloodCastle)
		|| gInterface.CheckWindow(Interface::ObjWindow::DevilSquare)
		|| gInterface.CheckWindow(Interface::ObjWindow::Character)
		|| gInterface.CheckWindow(Interface::ObjWindow::FastMenu)
		|| gInterface.CheckWindow(Interface::ObjWindow::CashShop)
		|| gInterface.CheckWindow(Interface::ObjWindow::SkillTree) || gInterface.CheckWindow(Interface::ObjWindow::FullMap)
		|| (gInterface.CheckWindow(Interface::Inventory)
			&& gInterface.CheckWindow(Interface::ExpandInventory)
			&& gInterface.CheckWindow(Interface::Store))
		|| (gInterface.CheckWindow(Interface::Inventory)
			&& gInterface.CheckWindow(Interface::Warehouse)
			&& gInterface.CheckWindow(Interface::ExpandWarehouse)) || gInterface.CheckWindow(Interface::Inventory))
	{

		return;
	}

	if (!gInterface.Data[eWindowBXHDmg].OnShow || this->m_DataTopBXHDMG.empty())
	{
		gInterface.Data[eWindowBXHDmg].OnShow = false;
		return;
	}
	float WindowW = 150;
	float WindowH = 120;
	float StartX = (MAX_WIN_WIDTH - WindowW) - (10);
	float StartY = (MAX_WIN_HEIGHT - 350);

	g_pBCustomMenuInfo->DrawWindowCustomMini(&StartX, &StartY, WindowW, WindowH, eWindowBXHDmg, "%s", getMonsterName(this->ClassMonter));

	if (gInterface.Data[eWindowBXHDmg].BActiveHiden) return;

	TextDraw((HFONT)g_hFontBold, StartX + 3, StartY + 25, 0xFFC421FF, 0x0, 60, 0, 3, "Top");
	TextDraw((HFONT)g_hFontBold, StartX + 5 + 50, StartY + 25, 0xFFC421FF, 0x0, 85, 0, 3, "Damage");
	int TextY = StartY + 25 + 13;
	for (int i = 0; i < this->m_DataTopBXHDMG.size(); i++)
	{
		TextDraw((HFONT)g_hFont, StartX + 3, TextY + (11*i), 0xFFFFFFFF, 0x0, 60, 0, 3, this->m_DataTopBXHDMG[i].Name);
		TextDraw((HFONT)g_hFont, StartX + 5 + 50, TextY +(11 * i), 0xFFFFFFFF, 0x0, 85, 0, 3, "%s",gInterface.QNumberFormat(this->m_DataTopBXHDMG[i].damage));
	}

}