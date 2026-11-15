// CB_ItemReroll.cpp: see CB_ItemReroll.h. Code-drawn window, same helpers as CB_DungeonRace / CB_KingOfKira.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CB_ItemReroll.h"
#include "CB_DuelRanked.h"
#include "CBInterface.h"
#include "NewUIBCustomMenu.h"
#include "NewUISystem.h"
#include "ZzzOpenglUtil.h"
#include "ZzzCharacter.h"
#include "ZzzScene.h"
#include "ZzzInfomation.h"
#include "ZzzInventory.h"
#include "SkillManager.h"
#include "wsclientinline.h"
#include "Input.h"
#include "Other.h"
#include "Util.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

CB_ItemReroll* gCB_ItemReroll = NULL;

// Packet layouts, identical to PMSG_RR_* in the GameServer ItemReroll.h (packed, fixed width only: no `long`).
#pragma pack(push,1)
struct RR_OPEN_RECV { BYTE c, size, head, sub; BYTE PayMask, Pad; WORD JewelItem, JewelCount, JewelPerLock, SpinMs, JewelHave; DWORD PriceWC, PriceGP, LockWC, LockGP; WORD PriceJW, LockJW; };
struct RR_STATE_RECV { BYTE c, size, head, sub; BYTE Code, Stage, Slot, Valid, Old, New, Locks, Flags; WORD ItemIndex, JewelHave; BYTE Weight[6]; BYTE LevelByte, ExtByte; BYTE Mult[6]; };
#pragma pack(pop)

static_assert(sizeof(RR_OPEN_RECV) == 4+2+10+16+4, "RR open");
static_assert(sizeof(RR_STATE_RECV) == 4+8+4+6+6+2, "RR state");

namespace
{
	// result / error codes (eItemRerollCode on the server)
	enum { RR_ROLLED = 2, RR_APPLIED = 3, RR_CLEARED = 4, RR_ERR_OFF = 10, RR_ERR_FAR = 11, RR_ERR_INTERNAL = 26 };

	const DWORD kWhite = RankedUi::Col(255, 255, 255);
	const DWORD kGold = RankedUi::Col(255, 200, 80);
	const DWORD kGreen = RankedUi::Col(120, 230, 130);
	const DWORD kRed = RankedUi::Col(255, 110, 110);
	const DWORD kDim = RankedUi::Col(160, 160, 166);
	const DWORD kBlue = RankedUi::Col(130, 190, 255);

	const float kWinW = 380.0f, kWinH = 392.0f, kWinX = 14.0f, kWinY = 22.0f;
	const float kRowH = 19.0f;

	// English defaults: used when the language file has no [Text_ItemReroll] section (Text_Eng.ini / Text_Vtm.ini / Text_Spn.ini carry the translations)
	const char* const kDefault[48] =
	{
		/* 0 */ "EXCELLENT ITEM REROLLER",
		/* 1 */ "Right-click (long-press) an excellent item in your bag to place it here. It stays safe on the NPC until you claim it.",
		/* 2 */ "Current options",
		/* 3 */ "New options (chance)",
		/* 4 */ "Lock",
		/* 5 */ "Locked",
		/* 6 */ "Jewels (bag + Jewel Bank):",
		/* 7 */ "Pay with:",
		/* 8 */ "WCoin",
		/* 9 */ "Goblin Points",
		/* 10 */ "Reroll (jewels)",
		/* 11 */ "Confirm (coins)",
		/* 12 */ "Close",
		/* 13 */ "Claim",
		/* 14 */ "Spinning...",
		/* 15 */ "Compare, then press Confirm to pay and get the new options (or Reroll again).",
		/* 16 */ "1) REROLL: pay JEWELS to roll new options (nothing changes yet).",
		/* 17 */ "have",
		/* 18 */ "Price",
		/* 19 */ "No options",
		/* 20 */ "Done! The item has its new options. Press Claim to take it back to your bag.",
		/* 21 */ "Item placed. Lock the options you want to keep, then press Reroll.",
		/* 22 */ "The Excellent Item Reroller is switched off.",
		/* 23 */ "Stay next to the NPC and open the window again.",
		/* 24 */ "Close other windows (trade, shop) first.",
		/* 25 */ "This item cannot be rerolled.",
		/* 26 */ "This item has no excellent option.",
		/* 27 */ "This kind of item is not accepted here.",
		/* 28 */ "Ancient items cannot be rerolled.",
		/* 29 */ "Not enough jewels.",
		/* 30 */ "Not enough coins.",
		/* 31 */ "This currency is not accepted.",
		/* 32 */ "Invalid locks: at least one option must stay unlocked.",
		/* 33 */ "No different result could be rolled. Try other locks.",
		/* 34 */ "The item changed. The roll was cancelled.",
		/* 35 */ "Press Reroll first.",
		/* 36 */ "The roll expired. Reroll again.",
		/* 37 */ "Place an item first.",
		/* 38 */ "Something went wrong. Nothing was changed.",
		/* 39 */ "Option %d",
		/* 40 */ "2) CONFIRM: pay WCoin or Goblin Points to get the item with them.",
		/* 41 */ "Locked options cost extra. Jewels are not refunded.",
		/* 42 */ "Jewels", /* 43 */ "No room in your bag! Make space: the item waits on the NPC until you do.", /* 44 */ "Confirm price:", /* 45 */ "Confirm price = %s x the multiplier of the new options (xN)", /* 46 */ "The item is back in your bag.", /* 47 */ ""
	};

	const char* T(int i)
	{
		if (i < 0 || i >= 48) return "";
		const char* s = gOther.Text_ItemReroll[i];
		if (s[0] == '\0' || strcmp(s, "Null") == 0) return kDefault[i];
		return s;
	}

	bool PrimaryDown()
	{
#if defined(MU_TOUCH_UI)
		return SEASON3B::IsPress(VK_LBUTTON) || CInput::Instance().IsLBtnHeldDn();   // held, so a touch drag keeps going
#else
		return ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
#endif
	}

	void PublishRect(int id, bool show, float x, float y, float w, float h)
	{
		InterfaceObject& d = gInterface.Data[id];
		d.OnShow = show;
		if (show)
		{
			d.X = x;
			d.Y = y;
			d.Width = w;
			d.Height = h;
			d.FirstLoad = true;
		}
	}

	int BitCount(BYTE v)
	{
		int c = 0;
		for (int n = 0; n < 8; ++n) if (v & (1 << n)) ++c;
		return c;
	}

	// the Nth set bit (0 based), -1 when there is none
	int NthBit(BYTE mask, int n)
	{
		for (int b = 0; b < 6; ++b)
		{
			if (mask & (1 << b))
			{
				if (n == 0) return b;
				--n;
			}
		}
		return -1;
	}

	DWORD Capped(DWORD a, DWORD b, int mul)
	{
		const unsigned long long t = (unsigned long long)a + (unsigned long long)b * (unsigned)mul;
		return (DWORD)((t > 0x7FFFFFFFULL) ? 0x7FFFFFFFULL : t);
	}

	void CollectSpecials(const ITEM& src, int excBits, int* ids, int* vals, int* count)
	{
		ITEM t = src;
		t.Option1 = (BYTE)((src.Option1 & ~0x3F) | excBits);
		t.ExtOption = 0;
		t.SpecialNum = 0;
		ItemConvert(&t, (BYTE)src.Level, t.Option1, 0);
		*count = (t.SpecialNum > 16) ? 16 : t.SpecialNum;
		for (int i = 0; i < *count; ++i)
		{
			ids[i] = t.Special[i];
			vals[i] = t.SpecialValue[i];
		}
	}
}

CB_ItemReroll::CB_ItemReroll()
{
	m_Click = false;
	m_WasDown = false;
	m_Open = false;
	m_OpenTick = 0;
	m_PayMask = 3;
	m_PriceJW = m_LockJW = 0;
	m_JewelItem = m_JewelCount = m_JewelPerLock = m_SpinMs = m_JewelHave = 0;
	m_PriceWC = m_PriceGP = m_LockWC = m_LockGP = 0;
	memset(&m_Item, 0, sizeof(m_Item));
	m_PosX = kWinX; m_PosY = kWinY; m_DragDX = m_DragDY = 0; m_Dragging = false;
	m_RequestTick = 0;
	m_Msg[0] = '\0';
	m_MsgGood = true;
	m_MsgTick = 0;
	Reset();
}

void CB_ItemReroll::Reset()
{
	m_HasItem = false;
	m_Stage = m_Slot = m_Valid = m_Old = m_New = m_RollLocks = 0;
	m_ItemIndex = 0;
	m_Locks = 0;
	m_Flags = 1;
	memset(m_Weight, 10, sizeof(m_Weight));
	memset(m_Mult, 1, sizeof(m_Mult));
	m_Pay = 0;
	m_Spinning = false;
	m_SpinStart = 0;
	for (int i = 0; i < 6; ++i) m_Label[i][0] = '\0';
}

void CB_ItemReroll::SetMessage(int textIndex, bool good)
{
	snprintf(m_Msg, sizeof(m_Msg), "%s", T(textIndex));
	m_MsgGood = good;
	m_MsgTick = GetTickCount();
}

void CB_ItemReroll::SendAction(BYTE action, BYTE slot, BYTE locks, BYTE pay)
{
	// C1:D3:69, UNENCRYPTED like every 0xD3 packet (see wsclientinline.h SendClientBuild). Fixed-width bytes only.
	CStreamPacketEngine spe;
	spe.Init(0xC1, 0xD3);
	spe << (BYTE)0x69;
	spe << (BYTE)action;
	spe << (BYTE)slot;
	spe << (BYTE)locks;
	spe << (BYTE)pay;
	spe.Send();
	if (action == 1 || action == 2 || action == 0) m_RequestTick = GetTickCount();
}

void CB_ItemReroll::Close(bool tellServer)
{
	if (tellServer && m_Open) SendAction(5, 0, 0, 0);
	m_Open = false;
	Reset();
	m_Spinning = false;
	PublishRect(eWindowItemReroll, false, 0, 0, 0, 0);
}

void CB_ItemReroll::RecvOpen(const BYTE* lpMsg, int size)
{
	if (size < (int)sizeof(RR_OPEN_RECV)) return;

	const RR_OPEN_RECV* p = (const RR_OPEN_RECV*)lpMsg;

	Reset();
	m_Open = true;
	m_OpenTick = GetTickCount();
	m_PayMask = p->PayMask;
	m_JewelItem = p->JewelItem;
	m_JewelCount = p->JewelCount;
	m_JewelPerLock = p->JewelPerLock;
	m_SpinMs = p->SpinMs;
	m_JewelHave = p->JewelHave;
	m_PriceWC = p->PriceWC;
	m_PriceGP = p->PriceGP;
	m_LockWC = p->LockWC;
	m_LockGP = p->LockGP;
	m_PriceJW = p->PriceJW;
	m_LockJW = p->LockJW;
	m_Pay = (m_PayMask & 1) ? 1 : ((m_PayMask & 2) ? 2 : 3);
	m_RequestTick = 0;
	m_Msg[0] = '\0';

	// the bag must be open to pick the item from
	if (g_pNewUISystem && !g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY))
	{
		g_pNewUISystem->Show(SEASON3B::INTERFACE_INVENTORY);
	}
}

// One label per excellent bit, taken from the client's own item conversion (so it reads exactly like the tooltip, for weapons,
// armour, wings and accessories alike): the line an item gains when only that bit is set.
void CB_ItemReroll::BuildLabels()
{
	for (int b = 0; b < 6; ++b) m_Label[b][0] = '\0';
	if (!m_HasItem) return;

	int baseIds[16], baseVals[16], baseN = 0;
	CollectSpecials(m_Item, 0, baseIds, baseVals, &baseN);

	for (int b = 0; b < 6; ++b)
	{
		if (!(m_Valid & (1 << b))) continue;

		int ids[16], vals[16], n = 0;
		CollectSpecials(m_Item, 1 << b, ids, vals, &n);

		bool found = false;
		for (int i = 0; i < n && !found; ++i)
		{
			bool known = false;
			for (int j = 0; j < baseN; ++j) if (baseIds[j] == ids[i] && baseVals[j] == vals[i]) known = true;
			if (known) continue;

			int iMana = 0;
			gSkillManager.GetSkillInformation(ids[i], 1, NULL, &iMana, NULL);

			char text[128] = { 0 };
			GetSpecialOptionText(m_Item.Type, text, (WORD)ids[i], (BYTE)vals[i], iMana);

			const char* s = text;
			while (*s == ' ') ++s;
			if (*s)
			{
				snprintf(m_Label[b], sizeof(m_Label[b]), "%s", s);
				found = true;
			}
		}

		if (!found) snprintf(m_Label[b], sizeof(m_Label[b]), T(39), b + 1);
	}
}

void CB_ItemReroll::RecvState(const BYTE* lpMsg, int size)
{
	if (size < (int)sizeof(RR_STATE_RECV)) return;

	const RR_STATE_RECV* p = (const RR_STATE_RECV*)lpMsg;

	m_RequestTick = 0;
	m_JewelHave = p->JewelHave;
	m_Flags = p->Flags;
	memcpy(m_Weight, p->Weight, 6);
	memcpy(m_Mult, p->Mult, 6);

	const bool wasStage2 = (m_Stage == 2);

	if (p->Stage == 0)
	{
		m_HasItem = false;
		m_Stage = 0;
		m_Spinning = false;
		m_Locks = 0;
	}
	else
	{
		// The item on the NPC is the server's: when it is not the one just clicked (a window reopened with an item that was saved here
		// earlier) build it from the item bytes the server sent.
		if (!m_HasItem || m_Item.Type != (short)p->ItemIndex)
		{
			memset(&m_Item, 0, sizeof(m_Item));
			m_Item.Type = (short)p->ItemIndex;
			m_HasItem = true;
			m_Stage = 0;
		}

		m_Item.Level = p->LevelByte;
		m_Item.ExtOption = p->ExtByte;
		m_Item.Option1 = (BYTE)((m_Item.Option1 & ~0x3F) | (p->Old & 0x3F));

		const bool newItem = (m_Stage == 0 || m_Slot != p->Slot || m_Old != p->Old || m_Valid != p->Valid);

		m_Stage = p->Stage;
		m_Slot = p->Slot;
		m_Valid = p->Valid;
		m_Old = p->Old;
		m_New = p->New;
		m_RollLocks = p->Locks;

		if (newItem)
		{
			m_Locks = 0;
			BuildLabels();
		}
		else
		{
			m_Locks &= m_Old;
		}

		if (p->Stage == 2 && p->Code == RR_ROLLED)
		{
			m_Spinning = true;
			m_SpinStart = GetTickCount();
		}
		else if (p->Stage != 2)
		{
			m_Spinning = false;
		}
	}

	switch (p->Code)
	{
	case 1: SetMessage(21, true); break;
	case RR_ROLLED: SetMessage(15, true); break;
	case RR_APPLIED:
		SetMessage(20, true);
		m_Locks = 0;
		m_Spinning = false;
		PlayBuffer(SOUND_GET_ITEM01);
		break;
	case RR_CLEARED: SetMessage(46, true); PlayBuffer(SOUND_GET_ITEM01); break;
	case 0:
		break;
	case 27: SetMessage(43, false); break;
	default:
		if (p->Code >= RR_ERR_OFF && p->Code <= RR_ERR_INTERNAL) SetMessage(22 + (p->Code - RR_ERR_OFF), false);
		break;
	}

	if (p->Code == RR_ERR_FAR || p->Code == RR_ERR_OFF)
	{
		// window no longer valid on the server: close it here too (without telling the server back)
		Close(false);
		SetMessage(p->Code == RR_ERR_OFF ? 22 : 23, false);
	}

	if (p->Code == RR_ERR_FAR || p->Code == RR_ERR_OFF)
	{
		gInterface.DrawMessage(1, "%s", m_Msg);
	}
}

bool CB_ItemReroll::SendItem(ITEM* item, int slot)
{
	if (!m_Open) return false;

	if (item == NULL || item->Type < 0) return true;

	// a request in flight / the reel is spinning: swallow the click so the item is not used by accident
	if (m_Spinning || (m_RequestTick != 0 && (GetTickCount() - m_RequestTick) < 1500)) return true;

	if (slot < 0 || slot > 255) return true;

	m_Item = *item;
	m_HasItem = true;
	m_Stage = 0;
	m_Locks = 0;
	m_Spinning = false;

	PlayBuffer(SOUND_GET_ITEM01);
	SendAction(0, (BYTE)slot, 0, 0);
	return true;
}

// Chance of an option to come up per pick, in %: its weight over the weights of every option that can still be drawn.
int CB_ItemReroll::Percent(int bit, BYTE locks) const
{
	BYTE pool = (BYTE)(m_Valid & ~locks);
	if (m_Flags & 1) pool &= (BYTE)~m_Old;

	int sum = 0;
	for (int b = 0; b < 6; ++b) if ((pool & (1 << b)) && m_Weight[b] > 0) sum += m_Weight[b];

	if (!(pool & (1 << bit)) || sum <= 0 || m_Weight[bit] == 0) return 0;
	return (m_Weight[bit] * 100 + sum / 2) / sum;
}

void CB_ItemReroll::DrawWindow()
{
	const DWORD now = GetTickCount();

	const bool down = PrimaryDown();
	m_Click = down && (m_WasDown == false);
	m_WasDown = down;

	const bool inWorld = (Hero != NULL && SceneFlag == MAIN_SCENE);

	if (!inWorld)
	{
		if (m_Open) Close(false);
		return;
	}

	if (!m_Open)
	{
		PublishRect(eWindowItemReroll, false, 0, 0, 0, 0);
		return;
	}

	// the reel stops
	if (m_Spinning && (now - m_SpinStart) >= (DWORD)m_SpinMs + 250)
	{
		m_Spinning = false;
		PlayBuffer(SOUND_JEWEL01); // the roll is done: jewel success sound
	}

	// drag the window by its title bar
	if (m_Click && RankedUi::Inside(m_PosX, m_PosY, kWinW - 30.0f, 28.0f))
	{
		m_Dragging = true;
		m_DragDX = (float)MouseX - m_PosX;
		m_DragDY = (float)MouseY - m_PosY;
	}
	if (!down) m_Dragging = false;
	if (m_Dragging)
	{
		m_PosX = (float)MouseX - m_DragDX;
		m_PosY = (float)MouseY - m_DragDY;
	}
	if (m_PosX < -kWinW + 60.0f) m_PosX = -kWinW + 60.0f;
	if (m_PosX > (float)MAX_WIN_WIDTH - 60.0f) m_PosX = (float)MAX_WIN_WIDTH - 60.0f;
	if (m_PosY < 0.0f) m_PosY = 0.0f;
	if (m_PosY > (float)MAX_WIN_HEIGHT - 80.0f) m_PosY = (float)MAX_WIN_HEIGHT - 80.0f;

	const float x = m_PosX, y = m_PosY;

	PublishRect(eWindowItemReroll, true, x, y, kWinW, kWinH);

	if (RankedUi::Inside(x, y, kWinW, kWinH) && g_pBCustomMenuInfo != NULL)
	{
		g_pBCustomMenuInfo->SetBlockCur(TRUE);
	}

	// the bag stays open next to the window
	if (g_pNewUISystem && !g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY))
	{
		g_pNewUISystem->Show(SEASON3B::INTERFACE_INVENTORY);
	}

	EnableAlphaTest();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	RankedUi::Bar(x, y, kWinW, kWinH, RankedUi::Col(12, 12, 16, 240));
	RankedUi::Outline(x, y, kWinW, kWinH, RankedUi::Col(214, 170, 70));
	RankedUi::Outline(x + 2.0f, y + 2.0f, kWinW - 4.0f, kWinH - 4.0f, RankedUi::Col(70, 58, 34));
	RankedUi::DrawDiamond(x + 16.0f, y + 15.0f, 4.0f, kGold);
	RankedUi::DrawDiamond(x + kWinW - 16.0f, y + 15.0f, 4.0f, kGold);
	RankedUi::Text(g_hFontBold, x, y + 9.0f, kWinW, RT3_SORT_CENTER, kGold, T(0));

	// ---- item slot + info -------------------------------------------------
	const float boxX = x + 10.0f, boxY = y + 30.0f, boxW = 100.0f, boxH = 90.0f;
	RankedUi::Bar(boxX, boxY, boxW, boxH, RankedUi::Col(0, 0, 0, 150));
	RankedUi::Outline(boxX, boxY, boxW, boxH, m_HasItem ? RankedUi::Col(120, 140, 70) : RankedUi::Col(90, 90, 96));

	const float infoX = boxX + boxW + 8.0f, infoW = kWinW - (infoX - x) - 10.0f;
	RankedUi::Bar(infoX, boxY, infoW, boxH, RankedUi::Col(0, 0, 0, 110));
	RankedUi::Outline(infoX, boxY, infoW, boxH, RankedUi::Col(60, 60, 66));

	const bool busy = (m_RequestTick != 0 && (now - m_RequestTick) < 1500);

	if (m_HasItem && m_Stage != 0)
	{
		g_pNewUISystem->RenderItem3DFree(boxX + 25.0f, boxY - 35.0f, 50.0f, 90.0f, m_Item.Type, m_Item.Level, m_Item.Option1, m_Item.ExtOption, false, 1.3f);

		RankedUi::Text(g_hFontBold, infoX + 6.0f, boxY + 6.0f, infoW - 80.0f, RT3_SORT_LEFT, kWhite, BGetItemName(m_Item.Type, m_Item.Level));

		RankedUi::Text(g_hFont, infoX + 6.0f, boxY + 30.0f, infoW - 12.0f, RT3_SORT_LEFT, kGold, T(16));
		RankedUi::Text(g_hFont, infoX + 6.0f, boxY + 46.0f, infoW - 12.0f, RT3_SORT_LEFT, kGold, T(40));
		RankedUi::Text(g_hFont, infoX + 6.0f, boxY + 66.0f, infoW - 12.0f, RT3_SORT_LEFT, kDim, T(41));

		const float rmW = 66.0f, rmH = 20.0f, rmX = infoX + infoW - rmW - 5.0f, rmY = boxY + 4.0f;
		const bool canRemove = (!m_Spinning && !busy);
		RankedUi::DrawButton(rmX, rmY, rmW, rmH, T(13), 0, canRemove, false);
		if (m_Click && canRemove && RankedUi::Inside(rmX, rmY, rmW, rmH))
		{
			SendAction(4, 0, 0, 0);
			m_HasItem = false;
			m_Stage = 0;
			m_Locks = 0;
		}
	}
	else
	{
		RankedUi::Text(g_hFont, infoX + 8.0f, boxY + 10.0f, infoW - 16.0f, RT3_SORT_LEFT, kDim, T(1));
	}

	// ---- option lists -----------------------------------------------------
	const float colL = x + 10.0f, colR = x + 200.0f, colW = 180.0f, listY = y + 150.0f;
	RankedUi::Text(g_hFontBold, colL, y + 130.0f, colW, RT3_SORT_LEFT, kGold, T(2));
	RankedUi::Text(g_hFontBold, colR, y + 130.0f, colW, RT3_SORT_LEFT, kGold, T(3));
	RankedUi::Bar(colL, listY - 2.0f, colW, kRowH * 6.0f + 2.0f, RankedUi::Col(0, 0, 0, 90));
	RankedUi::Bar(colR, listY - 2.0f, colW, kRowH * 6.0f + 2.0f, RankedUi::Col(0, 0, 0, 90));

	if (m_HasItem && m_Stage != 0)
	{
		const bool canToggle = (!m_Spinning && !busy);
		const float lockW = 44.0f;

		int row = 0;
		for (int b = 0; b < 6; ++b)
		{
			if (!(m_Old & (1 << b))) continue;

			const float ry = listY + row * kRowH;
			const bool locked = (m_Locks & (1 << b)) != 0;

			RankedUi::DrawButton(colL + 2.0f, ry, lockW, kRowH - 2.0f, locked ? T(5) : T(4), locked ? 3 : 0, canToggle, false);
			RankedUi::Text(g_hFont, colL + lockW + 8.0f, ry + 3.0f, colW - lockW - 10.0f, RT3_SORT_LEFT, locked ? kGold : kWhite, m_Label[b]);

			if (m_Click && canToggle && RankedUi::Inside(colL + 2.0f, ry, lockW, kRowH - 2.0f))
			{
				m_Locks ^= (BYTE)(1 << b);
				PlayBuffer(SOUND_CLICK01);
			}
			++row;
		}

		if (m_Stage == 2)
		{
			// Rows line up with "Current options": a locked option stays in its row (marked Locked, no spin); only the
			// unlocked rows spin and land on the rolled options.
			int rowBit[6], nRows = 0;
			bool rowKept[6];
			BYTE fresh = (BYTE)(m_New & ~m_RollLocks);
			int freshBits[6], nFresh = 0;
			for (int b2 = 0; b2 < 6; ++b2) if (fresh & (1 << b2)) freshBits[nFresh++] = b2;
			int nextFresh = 0;

			for (int b2 = 0; b2 < 6; ++b2)
			{
				if (!(m_Old & (1 << b2))) continue;
				if (m_RollLocks & (1 << b2)) { rowBit[nRows] = b2; rowKept[nRows++] = true; }
				else if (nextFresh < nFresh) { rowBit[nRows] = freshBits[nextFresh++]; rowKept[nRows++] = false; }
				else { rowBit[nRows] = -1; rowKept[nRows++] = false; }
			}
			while (nextFresh < nFresh && nRows < 6) { rowBit[nRows] = freshBits[nextFresh++]; rowKept[nRows++] = false; }

			int nSpin = 0;
			for (int r = 0; r < nRows; ++r) if (!rowKept[r] && rowBit[r] >= 0) ++nSpin;

			const DWORD elapsed = now - m_SpinStart;
			int spinIdx = 0;

			for (int r = 0; r < nRows; ++r)
			{
				const float ry = listY + r * kRowH;
				const int bit = rowBit[r];
				if (bit < 0) continue;

				if (rowKept[r])
				{
					RankedUi::Text(g_hFont, colR + 6.0f, ry + 3.0f, colW - 76.0f, RT3_SORT_LEFT, kGold, m_Label[bit]);
					RankedUi::Text(g_hFont, colR + colW - 74.0f, ry + 3.0f, 70.0f, RT3_SORT_RIGHT, kGold, T(5));
					continue;
				}

				const DWORD stopAt = (DWORD)m_SpinMs * (spinIdx + 1) / (DWORD)(nSpin > 0 ? nSpin : 1);
				++spinIdx;
				const bool stopped = (!m_Spinning) || (elapsed >= stopAt);

				if (stopped)
				{
					const bool isNew = (m_Old & (1 << bit)) == 0;
					RankedUi::Text(g_hFont, colR + 6.0f, ry + 3.0f, colW - 76.0f, RT3_SORT_LEFT, isNew ? kGreen : kWhite, m_Label[bit]);
					char pc[16];
					snprintf(pc, sizeof(pc), "%d%% x%d", Percent(bit, m_RollLocks), (int)m_Mult[bit]);
					RankedUi::Text(g_hFont, colR + colW - 74.0f, ry + 3.0f, 70.0f, RT3_SORT_RIGHT, kDim, pc);
				}
				else
				{
					// the reel: a new option from the item's own list every 70 ms, highlighted strip
					RankedUi::Bar(colR + 2.0f, ry, colW - 4.0f, kRowH - 2.0f, RankedUi::Col(70, 58, 34, 200));
					const int pick = (int)(((elapsed / 70) * 7 + r * 5 + r * r) % (DWORD)BitCount(m_Valid));
					const int vb = NthBit(m_Valid, pick);
					RankedUi::Text(g_hFont, colR + 6.0f, ry + 3.0f, colW - 8.0f, RT3_SORT_LEFT, kBlue, vb >= 0 ? m_Label[vb] : "?");
				}
			}
		}
		else if (m_Stage == 1)
		{
			// before the roll: every option that can come up, with its chance per pick
			int r = 0;
			for (int b = 0; b < 6; ++b)
			{
				if (!(m_Valid & (1 << b))) continue;

				const int pct = Percent(b, m_Locks);
				const bool keep = (m_Locks & (1 << b)) != 0;
				if (pct == 0 && !keep) continue;

				const float ry = listY + r * kRowH;
				const DWORD col = keep ? kGold : kDim;
				RankedUi::Text(g_hFont, colR + 6.0f, ry + 3.0f, colW - 76.0f, RT3_SORT_LEFT, col, m_Label[b]);
				char pc[16];
				if (keep) snprintf(pc, sizeof(pc), "%s", T(5));
				else snprintf(pc, sizeof(pc), "%d%% x%d", pct, (int)m_Mult[b]);
				RankedUi::Text(g_hFont, colR + colW - 74.0f, ry + 3.0f, 70.0f, RT3_SORT_RIGHT, col, pc);
				++r;
			}
		}
	}

	// ---- cost -------------------------------------------------------------
	const int locksNext = BitCount(m_Locks);
	const int locksRoll = (m_Stage == 2) ? BitCount(m_RollLocks) : locksNext;
	const int jewelNeed = (int)m_JewelCount + (int)m_JewelPerLock * locksNext;

	char line[200];
	const char* jewelName = (m_JewelItem < MAX_ITEM) ? ItemAttribute[m_JewelItem].Name : "?";
	snprintf(line, sizeof(line), "%s %s x%d  (%s %d)", T(6), jewelName, jewelNeed, T(17), (int)m_JewelHave);
	const bool jewelOk = ((int)m_JewelHave >= jewelNeed);
	RankedUi::Text(g_hFont, x + 12.0f, y + 266.0f, kWinW - 24.0f, RT3_SORT_LEFT, jewelOk ? kWhite : kRed, line);

	// ONE payment method, chosen in the server config (PayWith). The final price = base price x the multipliers of the options the
	// new item ends up with + the lock surcharge; it is known once the roll is done.
	const int pm = (m_PayMask & 1) ? 1 : ((m_PayMask & 2) ? 2 : 3);
	m_Pay = (BYTE)pm;

	const DWORD baseP = (pm == 1) ? m_PriceWC : ((pm == 2) ? m_PriceGP : (DWORD)m_PriceJW);
	const DWORD lockP = (pm == 1) ? m_LockWC : ((pm == 2) ? m_LockGP : (DWORD)m_LockJW);
	const DWORD have = (pm == 1) ? (CharacterAttribute ? CharacterAttribute->PrintPlayer.Coin1 : 0) : ((pm == 2) ? (CharacterAttribute ? CharacterAttribute->PrintPlayer.Coin3 : 0) : (DWORD)m_JewelHave);
	const char* payName = (pm == 1) ? T(8) : ((pm == 2) ? T(9) : T(42));

	int units = 0;
	if (m_Stage == 2) for (int b2 = 0; b2 < 6; ++b2) if (m_New & (1 << b2)) units += m_Mult[b2];

	char num[24];
	RankedUi::Text(g_hFontBold, x + 12.0f, y + 286.0f, 120.0f, RT3_SORT_LEFT, kWhite, T(44));

	if (m_Stage == 2)
	{
		const DWORD price = Capped(baseP * (DWORD)units, lockP, locksRoll);
		RankedUi::FormatNumber((int)price, num, sizeof(num));
		snprintf(line, sizeof(line), "%s %s", num, payName);
		RankedUi::Text(g_hFontBold, x + 130.0f, y + 286.0f, kWinW - 140.0f, RT3_SORT_LEFT, (have >= price) ? kGold : kRed, line);

		RankedUi::FormatNumber((int)have, num, sizeof(num));
		snprintf(line, sizeof(line), "%s: %s %s", T(17), num, payName);
		RankedUi::Text(g_hFont, x + 12.0f, y + 306.0f, kWinW - 24.0f, RT3_SORT_LEFT, (have >= price) ? kDim : kRed, line);
	}
	else
	{
		RankedUi::FormatNumber((int)baseP, num, sizeof(num));
		snprintf(line, sizeof(line), "%s %s", num, payName);
		RankedUi::Text(g_hFontBold, x + 130.0f, y + 286.0f, kWinW - 140.0f, RT3_SORT_LEFT, kGold, line);

		snprintf(line, sizeof(line), T(45), payName);
		RankedUi::Text(g_hFont, x + 12.0f, y + 306.0f, kWinW - 24.0f, RT3_SORT_LEFT, kDim, line);
	}

	// ---- message (above the buttons) -------------------------------------
	if (m_Spinning)
	{
		RankedUi::Text(g_hFontBold, x + 12.0f, y + 346.0f, kWinW - 24.0f, RT3_SORT_LEFT, kBlue, T(14));
	}
	else if (m_Msg[0] != '\0')
	{
		RankedUi::Text(g_hFont, x + 12.0f, y + 346.0f, kWinW - 24.0f, RT3_SORT_LEFT, m_MsgGood ? kGreen : kRed, m_Msg);
	}

	// ---- buttons ----------------------------------------------------------
	const float btnY = y + kWinH - 4.0f - 26.0f - 2.0f;
	const float btnW = (kWinW - 20.0f - 20.0f) / 3.0f;
	const float rerollX = x + 10.0f, confirmX = rerollX + btnW + 10.0f, closeX = confirmX + btnW + 10.0f;
	const float btnH = 26.0f;

	const bool canRoll = (m_HasItem && m_Stage != 0 && !m_Spinning && !busy);
	const bool canConfirm = (m_HasItem && m_Stage == 2 && !m_Spinning && !busy);

	RankedUi::DrawButton(rerollX, btnY, btnW, btnH, T(10), 1, canRoll, true);
	RankedUi::DrawButton(confirmX, btnY, btnW, btnH, T(11), 3, canConfirm, true);
	RankedUi::DrawButton(closeX, btnY, btnW, btnH, T(12), 0, true, true);

	if (m_Click)
	{
		if (canRoll && RankedUi::Inside(rerollX, btnY, btnW, btnH))
		{
			SendAction(1, m_Slot, m_Locks, 0);
		}
		else if (canConfirm && RankedUi::Inside(confirmX, btnY, btnW, btnH))
		{
			SendAction(2, m_Slot, 0, m_Pay);
		}
		else if (RankedUi::Inside(closeX, btnY, btnW, btnH))
		{
			Close(true);
		}
	}
}
