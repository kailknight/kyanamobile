// RedeemCodeWindow.cpp: implementation of the CB_RedeemCodeWindow class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "RedeemCodeWindow.h"

#if(REDEEMCODE)

#include "NewUISystem.h"
#include "CBInterface.h"
#include "CUIController.h"
#include "Util.h"
#include "Protocol.h"
#include "NewUIBase.h"
#include "Other.h"
#include "ZzzInterface.h"
#include "ZzzInventory.h"
#include "ZzzInfomation.h"

CB_RedeemCodeWindow* gCB_RedeemCodeWindow;

// Doesn't hold the actual text box across frames like the header's other
// members - CUITextInputBox is a live UI control (own focus/blink state),
// same reasoning as CB_DoiMK's own file-scope statics. Lazily created the
// first frame the window is open, torn down when it closes.
static CUITextInputBox* g_CodeInputBox = NULL;
static char g_CodeInputText[33] = { 0, };

// Pulls a code off the Windows clipboard straight into the entry box - codes
// get handed out on Discord/websites, so typing a 20-character dash-separated
// string by hand is the common case this avoids. Same OpenClipboard/CF_TEXT
// shape as ClipboardCheck (UIControls.cpp:3789); on Android the whole family
// is stubbed out to fail (Platform/PlatformDefs.h:2050), so this simply does
// nothing there rather than needing its own #if.
static void PasteCodeFromClipboard()
{
	if (g_CodeInputBox == NULL)
	{
		return;
	}

	if (OpenClipboard(NULL) == FALSE)
	{
		return;
	}

	HGLOBAL hglb = GetClipboardData(CF_TEXT);

	if (hglb != NULL)
	{
		LPSTR lpstr = (LPSTR)GlobalLock(hglb);

		if (lpstr != NULL)
		{
			char cleaned[33];
			int out = 0;

			// Copy only what the server's own whitelist accepts
			// (RedeemCode.cpp, both sides: alphanumeric + dash), so a code
			// pasted with a trailing newline, surrounding quotes or stray
			// spaces still works instead of being rejected as NOT_FOUND.
			for (int i = 0; lpstr[i] != '\0' && out < (int)sizeof(cleaned) - 1; i++)
			{
				char c = lpstr[i];
				bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';

				if (ok)
				{
					cleaned[out++] = c;
				}
			}

			cleaned[out] = '\0';

			if (out > 0)
			{
				g_CodeInputBox->SetText(cleaned);
				memcpy(g_CodeInputText, cleaned, sizeof(cleaned) > sizeof(g_CodeInputText) ? sizeof(g_CodeInputText) : sizeof(cleaned));
				g_CodeInputText[sizeof(g_CodeInputText) - 1] = '\0';
			}

			GlobalUnlock(hglb);
		}
	}

	CloseClipboard();
}

CB_RedeemCodeWindow::CB_RedeemCodeWindow()
{
	this->Reset();
	this->m_LastSendTick = 0;
}

CB_RedeemCodeWindow::~CB_RedeemCodeWindow()
{
}

void CB_RedeemCodeWindow::Reset()
{
	this->m_State = STATE_ENTRY;
	memset(this->m_CodeText, 0, sizeof(this->m_CodeText));
	this->m_Result = 0;
	this->m_ItemCount = 0;
	memset(this->m_Items, 0, sizeof(this->m_Items));
	this->m_RewardWCoinC = 0;
	this->m_RewardWCoinP = 0;
	this->m_RewardGoblinPoint = 0;
	this->m_RewardRuud = 0;
	this->m_HoveredRow = -1;
	this->m_HoveredTipX = 0.0f;
	this->m_HoveredTipLeftX = 0.0f;
	this->m_HoveredTipY = 0.0f;
}

void CB_RedeemCodeWindow::OpenWindow()
{
	if ((GetTickCount() - gInterface.Data[eWindowRedeemCode].EventTick) > 300)
	{
		gInterface.Data[eWindowRedeemCode].EventTick = GetTickCount();

		if (gInterface.Data[eWindowRedeemCode].OnShow)
		{
			gInterface.Data[eWindowRedeemCode].OnShow = 0;
			return;
		}

		this->Reset();
		gInterface.Data[eWindowRedeemCode].OnShow ^= 1;
	}
}

void CB_RedeemCodeWindow::SendPreviewRequest()
{
	PMSG_REDEEM_CODE_SEND pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0xD3, 0xA0, sizeof(pMsg));
	strncpy_s(pMsg.Code, sizeof(pMsg.Code), this->m_CodeText, sizeof(pMsg.Code) - 1);
	DataSend((LPBYTE)&pMsg, pMsg.header.size);

	this->m_State = STATE_WAITING;
	this->m_LastSendTick = GetTickCount();
}

void CB_RedeemCodeWindow::SendRedeemRequest()
{
	PMSG_REDEEM_CODE_SEND pMsg;
	memset(&pMsg, 0, sizeof(pMsg));
	pMsg.header.set(0xD3, 0xA2, sizeof(pMsg));
	strncpy_s(pMsg.Code, sizeof(pMsg.Code), this->m_CodeText, sizeof(pMsg.Code) - 1);
	DataSend((LPBYTE)&pMsg, pMsg.header.size);

	this->m_State = STATE_WAITING;
	this->m_LastSendTick = GetTickCount();
}

void CB_RedeemCodeWindow::RecvPreviewResult(PMSG_REDEEM_CODE_RECV* lpMsg)
{
	if (this->m_State != STATE_WAITING)
	{
		return;
	}

	this->m_Result = lpMsg->Result;
	this->m_ItemCount = (lpMsg->ItemCount < REDEEM_CODE_BUNDLE_MAX) ? lpMsg->ItemCount : REDEEM_CODE_BUNDLE_MAX;
	memcpy(this->m_Items, lpMsg->Items, sizeof(REDEEM_ITEM_DETAIL) * this->m_ItemCount);
	this->m_RewardWCoinC = lpMsg->RewardWCoinC;
	this->m_RewardWCoinP = lpMsg->RewardWCoinP;
	this->m_RewardGoblinPoint = lpMsg->RewardGoblinPoint;
	this->m_RewardRuud = lpMsg->RewardRuud;
	this->m_HoveredRow = -1;

	// Only REDEEM_OK_TO_REDEEM gets to see the bundle and a Redeem button -
	// every other result (not found, expired, already redeemed, ...) is a
	// final answer for this attempt, shown the same way STATE_RESULT shows a
	// redeem failure.
	this->m_State = (this->m_Result == REDEEM_OK_TO_REDEEM) ? STATE_PREVIEW : STATE_RESULT;
}

void CB_RedeemCodeWindow::RecvRedeemResult(PMSG_REDEEM_CODE_RECV* lpMsg)
{
	if (this->m_State != STATE_WAITING)
	{
		return;
	}

	this->m_Result = lpMsg->Result;
	this->m_ItemCount = (lpMsg->ItemCount < REDEEM_CODE_BUNDLE_MAX) ? lpMsg->ItemCount : REDEEM_CODE_BUNDLE_MAX;
	memcpy(this->m_Items, lpMsg->Items, sizeof(REDEEM_ITEM_DETAIL) * this->m_ItemCount);
	this->m_RewardWCoinC = lpMsg->RewardWCoinC;
	this->m_RewardWCoinP = lpMsg->RewardWCoinP;
	this->m_RewardGoblinPoint = lpMsg->RewardGoblinPoint;
	this->m_RewardRuud = lpMsg->RewardRuud;
	this->m_HoveredRow = -1;
	this->m_State = STATE_RESULT;
}

const char* CB_RedeemCodeWindow::ResultMessage(BYTE result) const
{
	switch (result)
	{
	case REDEEM_SUCCESS:          return "Redeemed! The items below are now in your inventory.";
	case REDEEM_OK_TO_REDEEM:     return "Valid - review the rewards below, then tap Redeem.";
	case REDEEM_NOT_FOUND:        return "That code doesn't exist.";
	case REDEEM_INACTIVE:         return "This code is no longer active.";
	case REDEEM_EXPIRED:          return "This code has expired.";
	case REDEEM_CAP_REACHED:      return "This code has reached its usage limit.";
	case REDEEM_ALREADY_REDEEMED: return "You've already redeemed this code.";
	case REDEEM_INVENTORY_FULL:   return "Your inventory doesn't have room for all the rewards.";
	case REDEEM_BUSY:             return "Please wait for your last attempt to finish.";
	default:                      return "Something went wrong - please try again.";
	}
}

// Reconstructs a throwaway ITEM from a REDEEM_ITEM_DETAIL for RenderItem3D
// (icon) and ItemConvert/RenderItemInfo (tooltip) - the same two calls the
// real inventory uses to show an item, just fed from this feature's own
// (Level, Skill, Luck, Option, Excellent) breakdown instead of a live item's
// bytes.
//
// ItemConvert's own three parameters are NOT (Level, Excellent, ExtOption)
// plainly - they are a packed byte layout this codebase already uses
// end-to-end (confirmed against ZzzInfomation.cpp's ItemConvert body and its
// call site at ZzzInventory.cpp:2736, "ItemConvert(ip, ip->Level, ip->
// Option1, ip->ExtOption)" - ip->Level already holds this exact packed byte
// for a live item, this is the same construction from separate fields):
//   Attribute1: bit7=Skill, bits[3:6]=Level(0-15), bit2=Luck, bits[0:1]=
//               Option's low 2 bits
//   Attribute2: bits[0:5]=Excellent(0-63), bit6=Option's high bit (so
//               Option's full 0-7 range is bits[0:1] of Attribute1 plus
//               bit6 of Attribute2, worth 4)
//   Attribute3: ExtOption - always 0 here; this feature's rewards don't use
//               socket/ancient-set-derived ext options (see RedeemCode.cpp,
//               GameServer side, for the matching grant-time default)
static void BuildPreviewItem(ITEM& item, const REDEEM_ITEM_DETAIL& detail)
{
	memset(&item, 0, sizeof(item));
	item.Type = (short)detail.ItemIndex;

	BYTE attribute1 = (BYTE)(((detail.ItemSkill & 1) << 7) | ((detail.ItemLevel & 15) << 3) | ((detail.ItemLuck & 1) << 2) | (detail.ItemOption & 3));
	BYTE attribute2 = (BYTE)((detail.ItemExcellent & 63) | (((detail.ItemOption >> 2) & 1) << 6));
	BYTE attribute3 = 0;

	ItemConvert(&item, attribute1, attribute2, attribute3);

	// option_380 and the Harmony fields are plain, already-unpacked members
	// on ITEM (_struct.h) - RenderItemInfo reads them directly (ZzzInventory
	// .cpp:3183 for 380, :3226 for the Harmony yellow/gray line), no
	// ItemConvert-style bit-packing involved for either.
	item.option_380 = detail.Item380 != 0;
	item.Jewel_Of_Harmony_Option = detail.HarmonyOption;
	item.Jewel_Of_Harmony_OptionLevel = detail.HarmonyOptionLevel;

	// Sockets - unpack the same way the client does for a real item
	// (NewUIItemMng.cpp:79-94): 0xFF truncates SocketCount to this slot,
	// 0xFE means "present but empty" (SocketSeedID = SOCKET_EMPTY), anything
	// else splits into seed id (mod 50) and sphere level (div 50, 1-based).
	// bySocketOption[] keeps the original packed byte alongside the
	// unpacked SocketSeedID/SocketSphereLv the same way a real decoded item
	// would carry both.
	item.SocketCount = 5;
	for (int i = 0; i < 5; i++)
	{
		BYTE raw = detail.SocketOption[i];
		item.bySocketOption[i] = raw;

		if (raw == 0xFF)
		{
			item.SocketCount = (BYTE)i;
			break;
		}
		else if (raw == 0xFE)
		{
			item.SocketSeedID[i] = 0xFF; // SOCKET_EMPTY
			item.SocketSphereLv[i] = 0;
		}
		else
		{
			item.SocketSeedID[i] = raw % 50;
			item.SocketSphereLv[i] = (raw / 50) + 1;
		}
	}
	// SocketSeedSetOption (the "all 5 sockets share one element" bonus
	// display) is deliberately left 0/absent - it indexes a separate lookup
	// table (SocketSystem.cpp's SOT_MIX_SET_BONUS_OPTIONS) this feature
	// doesn't reconstruct, so a genuine 5-matching-socket reward previews
	// without that one bonus line rather than risk showing a wrong one.
}

void CB_RedeemCodeWindow::RenderItemRow(int row, float x, float y, float w, float h, const REDEEM_ITEM_DETAIL& detail)
{
	bool hovered = SEASON3B::CheckMouseIn((int)x, (int)y, (int)w, (int)h) == 1;

	if (hovered)
	{
		// Only the row-dependent part here - the X anchors are the window's own
		// edges and are set once per frame in DrawWindow.
		this->m_HoveredRow = row;
		this->m_HoveredTipY = y;
	}

	// Slightly lighter under the cursor so the row the tooltip belongs to is
	// obvious when several are listed.
	float bg = hovered ? 0.30f : 0.55f;
	gInterface.DrawBarForm(x, y, w, h, 0.0f, 0.0f, 0.0f, bg);

	ITEM item;
	BuildPreviewItem(item, detail);

	// Reset the draw colour first: DrawBarForm above leaves it at the bar's own
	// dark translucent value, and RenderItem3D modulates the icon by whatever
	// colour is current - which rendered every icon invisibly black. Every
	// other item-icon caller in the client does this same reset immediately
	// before the call (NewUIMyInventory.cpp:867).
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	const float iconSize = h - 4.0f;

	// RenderItem3D draws the item's real 3D model at a fixed, per-item-type
	// scale - it does NOT scale to the width/height passed in (those only
	// position the model's anchor). So a 1x4 sword renders at its full ~112px
	// inventory footprint and sprawls across the row and over the name text.
	// Shrink it to the icon box using the item's own cell footprint, the same
	// Width/Height the inventory grid lays it out with.
	float scaleMul = 1.0f;

	if (item.Type >= 0 && item.Type < MAX_ITEM)
	{
		const float kInventoryCellPx = 28.0f; // one inventory grid cell
		BYTE cellsW = ItemAttribute[item.Type].Width;
		BYTE cellsH = ItemAttribute[item.Type].Height;
		float naturalPx = ((cellsW > cellsH) ? cellsW : cellsH) * kInventoryCellPx;

		if (naturalPx > 0.0f)
		{
			scaleMul = iconSize / naturalPx;

			// Only ever shrink. A 1x1 item is already smaller than the box and
			// blowing it up looks worse than leaving it at its natural size.
			if (scaleMul > 1.0f)
			{
				scaleMul = 1.0f;
			}
		}
	}

	RenderItem3D(x + 2.0f, y + 2.0f, iconSize, iconSize, item.Type, item.Level, item.Option1, item.ExtOption, false, scaleMul);

	// The client's own name resolver - same call the inventory and shop use, so
	// a reward reads with exactly the name (and "+15" suffix) the player will
	// see once it is actually in their bag.
	//
	// detail.ItemLevel, NOT item.Level: ItemConvert leaves item.Level holding
	// the PACKED attribute byte (skill/level/luck/option bits), so passing it
	// here printed nonsense like "+255" and "+132" instead of "+15".
	char nameText[64];
	memset(nameText, 0, sizeof(nameText));
	GetItemName(item.Type, detail.ItemLevel, nameText);

	float textX = x + iconSize + 8.0f;
	float textW = w - iconSize - 12.0f;

	// Excellent items read gold in the bag; keep that cue here so a plain vs
	// excellent reward is distinguishable at a glance without hovering.
	DWORD nameColor = (detail.ItemExcellent != 0) ? 0xFF60FF60 : 0xFFFFFFFF;
	TextDraw((HFONT)g_hFont, textX, y + 6.0f, nameColor, 0x0, textW, 0, 0, nameText);

	if (detail.Quantity > 1)
	{
		char qtyText[16];
		sprintf_s(qtyText, sizeof(qtyText), "x%d", detail.Quantity);
		TextDraw((HFONT)g_hFont, textX, y + 22.0f, 0xFFC8C8C8, 0x0, textW, 0, 0, qtyText);
	}
}

int CB_RedeemCodeWindow::CurrencyRewardCount() const
{
	int n = 0;
	if (this->m_RewardWCoinC > 0)      n++;
	if (this->m_RewardWCoinP > 0)      n++;
	if (this->m_RewardGoblinPoint > 0) n++;
	if (this->m_RewardRuud > 0)        n++;
	return n;
}

// Currency has no item icon or tooltip, so these are plain labelled rows drawn
// above the item list rather than anything routed through RenderItemRow.
int CB_RedeemCodeWindow::RenderCurrencyRows(float x, float y, float w, float rowH, float stride)
{
	struct CurrencyRow { DWORD amount; const char* label; DWORD color; };

	const CurrencyRow rows[4] =
	{
		{ this->m_RewardWCoinC,      "W Coin (C)",  0xFF7FD4FF },
		{ this->m_RewardWCoinP,      "W Coin (P)",  0xFFFFD24F },
		{ this->m_RewardGoblinPoint, "Goblin Point", 0xFF9CFF7F },
		{ this->m_RewardRuud,        "Ruud",        0xFFFF9CE0 },
	};

	int drawn = 0;

	for (int i = 0; i < 4; i++)
	{
		if (rows[i].amount == 0)
		{
			continue;
		}

		float rowY = y + (drawn * stride);
		gInterface.DrawBarForm(x, rowY, w, rowH, 0.0f, 0.0f, 0.0f, 0.55f);

		TextDraw((HFONT)g_hFont, x + 10.0f, rowY + 6.0f, rows[i].color, 0x0, w - 20.0f, 0, 0, (char*)rows[i].label);

		// Thousands-separated through the same helper the rest of the UI uses
		// for coin amounts, so a big reward reads the same way it does on the
		// Features menu's own coin line.
		char amountText[32];
		sprintf_s(amountText, sizeof(amountText), "%s", gInterface.NumberFormat(rows[i].amount));
		TextDraw((HFONT)g_hFont, x + 10.0f, rowY + 22.0f, 0xFFFFFFFF, 0x0, w - 20.0f, 0, 0, amountText);

		drawn++;
	}

	return drawn;
}

void CB_RedeemCodeWindow::RenderHoveredTooltip()
{
	if (this->m_HoveredRow < 0 || this->m_HoveredRow >= this->m_ItemCount)
	{
		return;
	}

	ITEM item;
	BuildPreviewItem(item, this->m_Items[this->m_HoveredRow]);

	// Anchored outside the window entirely, not just past the row's right edge -
	// the row is only ~9px narrower than the window, so the old placement put
	// the tooltip straight over the Redeem/Back buttons. Flips to the left side
	// when there isn't room on the right.
	const float kTipWidthEstimate = 260.0f;
	float tipX = this->m_HoveredTipX;

	if (tipX + kTipWidthEstimate > (float)MAX_WIN_WIDTH)
	{
		tipX = this->m_HoveredTipLeftX - kTipWidthEstimate;
	}

	if (tipX < 0.0f)
	{
		tipX = 0.0f;
	}

	// The client's own real tooltip - name, stats, requirements, excellent /
	// 380 / harmony / socket lines - exactly as it reads in the bag.
	RenderItemInfo((int)tipX, (int)this->m_HoveredTipY, &item, false);
}

void CB_RedeemCodeWindow::DrawWindow()
{
	if (gInterface.CheckWindow(Interface::MoveList) || gInterface.CheckWindow(Interface::ObjWindow::CashShop) || gInterface.CheckWindow(Interface::ObjWindow::SkillTree) || gInterface.CheckWindow(Interface::ObjWindow::FullMap))
	{
		gInterface.Data[eWindowRedeemCode].OnShow = false;
		return;
	}

	if (!gInterface.Data[eWindowRedeemCode].OnShow)
	{
		if (g_CodeInputBox)
		{
			g_CodeInputBox = nullptr;
		}
		return;
	}

	// Rows are laid out from StartY+70 down, at RowH+4 each, with ~50px of
	// footer for the buttons - so the frame has to grow with the bundle rather
	// than stay at a fixed 280. An 8-item bundle used to draw its last rows
	// straight through the bottom edge and out over the game world.
	const float RowH = 40.0f;
	const float RowStride = RowH + 4.0f;

	bool listState = (this->m_State == STATE_PREVIEW || this->m_State == STATE_RESULT);
	int listRows = 0;

	if (listState)
	{
		// STATE_RESULT only lists the bundle on success; a failure message has
		// no rows under it, so the window shouldn't reserve space for any.
		bool showsRows = (this->m_State == STATE_PREVIEW) || (this->m_Result == REDEEM_SUCCESS);
		listRows = showsRows ? (this->m_ItemCount + this->CurrencyRewardCount()) : 0;
	}

	float WindowW = 260;
	float WindowH = listState ? (120.0f + (listRows * RowStride)) : 150.0f;

	if (WindowH < 150.0f)
	{
		WindowH = 150.0f;
	}

	float StartX = (MAX_WIN_WIDTH / 2) - (WindowW / 2);
	float StartY = 30;

	g_pBCustomMenuInfo->gDrawWindowCustom(&StartX, &StartY, WindowW, WindowH, eWindowRedeemCode, "Redeem Code");

	float TextX = StartX + 15;
	float TextY = StartY + 45;

	this->m_HoveredRow = -1;
	this->m_HoveredTipX = StartX + WindowW + 8.0f;
	this->m_HoveredTipLeftX = StartX - 8.0f;

	if (this->m_State == STATE_ENTRY || this->m_State == STATE_WAITING)
	{
		TextDraw((HFONT)g_hFont, TextX, TextY, 0xFFFFFFFF, 0x0, 0, 0, 1, "Enter your code:");

		float BoxX = TextX;
		float BoxY = TextY + 20;
		gInterface.DrawBarForm(BoxX - 3, BoxY - 2.5f, 200, 13, 0.0f, 0.0f, 0.0f, 0.8f);

		if (!g_CodeInputBox)
		{
			g_CodeInputBox = new CUITextInputBox;
			// Init is (hWnd, iWidth, iHeight, iMaxLength, bIsPassword) - this
			// used to pass BoxX/BoxY as the width/height and TRUE as
			// bIsPassword, which is why the typed code came back masked as
			// "*****". A redemption code isn't a secret and the player needs to
			// see what they typed to catch a typo.
			g_CodeInputBox->Init(pGameWindow, 200, 13, 32, FALSE);
			g_CodeInputBox->SetBackColor(0, 0, 0, 0);
			g_CodeInputBox->SetTextColor(255, 255, 255, 0);
			g_CodeInputBox->SetFont((HFONT)g_hFont);
			g_CodeInputBox->SetState(UISTATE_NORMAL);
			g_CodeInputBox->SetOption(UIOPTION_NOLOCALIZEDCHARACTERS);
			g_CodeInputBox->SetPosition(BoxX, BoxY);
		}
		else
		{
			g_CodeInputBox->SetPosition(BoxX, BoxY);
			g_CodeInputBox->Render();
			g_CodeInputBox->GetText(g_CodeInputText, sizeof(g_CodeInputText) - 1);

			if (SEASON3B::CheckMouseIn((int)(BoxX - 5), (int)(BoxY - 5), 200, 24) == 1 && (GetKeyState(VK_LBUTTON) & 0x8000))
			{
				g_CodeInputBox->GiveFocus(1);
				PlayBuffer(25, 0, 0);
			}
		}

		if (this->m_State == STATE_WAITING)
		{
			TextDraw((HFONT)g_hFont, TextX, TextY + 45, 0xFFC0C0C0, 0x0, 0, 0, 1, "Checking...");

			// A round trip that never replies (disconnect mid-flight, e.g.)
			// would otherwise leave this window stuck showing "Checking..."
			// forever - fall back to the entry state after a few seconds
			// rather than trap the player here with no way out but closing
			// the window.
			if ((GetTickCount() - this->m_LastSendTick) > 8000)
			{
				this->m_State = STATE_ENTRY;
			}
		}
		else
		{
			if (g_pBCustomMenuInfo->DrawButton(StartX + 20, StartY + WindowH - 35, 70, 12, "Paste", 70))
			{
				PasteCodeFromClipboard();
			}

			if (g_pBCustomMenuInfo->DrawButton(StartX + WindowW - 20 - 110, StartY + WindowH - 35, 110, 12, "Check Code", 110))
			{
				strncpy_s(this->m_CodeText, sizeof(this->m_CodeText), g_CodeInputText, sizeof(this->m_CodeText) - 1);

				if (this->m_CodeText[0] != '\0')
				{
					this->SendPreviewRequest();
				}
			}
		}
	}
	else if (this->m_State == STATE_PREVIEW)
	{
		TextDraw((HFONT)g_hFont, TextX, TextY, 0xFF90FFB0, 0x0, 230, 0, 1, this->ResultMessage(this->m_Result));

		float RowY = TextY + 25;

		int currencyRows = this->RenderCurrencyRows(TextX, RowY, 230, RowH, RowStride);
		float ItemsY = RowY + (currencyRows * RowStride);

		if (this->m_ItemCount == 0 && currencyRows == 0)
		{
			TextDraw((HFONT)g_hFont, TextX, RowY, 0xFFC0C0C0, 0x0, 0, 0, 1, "(no rewards)");
		}
		else
		{
			for (int n = 0; n < this->m_ItemCount; n++)
			{
				this->RenderItemRow(n, TextX, ItemsY + (n * RowStride), 230, RowH, this->m_Items[n]);
			}

			this->RenderHoveredTooltip();
		}

		if (g_pBCustomMenuInfo->DrawButton(StartX + 15, StartY + WindowH - 35, 90, 12, "Back", 90))
		{
			this->Reset();
		}
		if (g_pBCustomMenuInfo->DrawButton(StartX + WindowW - 15 - 100, StartY + WindowH - 35, 100, 12, "Redeem", 100))
		{
			this->SendRedeemRequest();
		}
	}
	else if (this->m_State == STATE_RESULT)
	{
		DWORD color = (this->m_Result == REDEEM_SUCCESS) ? 0xFF90FFB0 : 0xFFFF8080;
		TextDraw((HFONT)g_hFont, TextX, TextY, color, 0x0, 230, 0, 1, this->ResultMessage(this->m_Result));

		float RowY = TextY + 25;

		if (this->m_Result == REDEEM_SUCCESS)
		{
			int currencyRows = this->RenderCurrencyRows(TextX, RowY, 230, RowH, RowStride);
			float ItemsY = RowY + (currencyRows * RowStride);

			for (int n = 0; n < this->m_ItemCount; n++)
			{
				this->RenderItemRow(n, TextX, ItemsY + (n * RowStride), 230, RowH, this->m_Items[n]);
			}

			this->RenderHoveredTooltip();
		}

		if (g_pBCustomMenuInfo->DrawButton(StartX + (WindowW / 2) - (100 / 2), StartY + WindowH - 35, 100, 12, "Close", 100))
		{
			gInterface.Data[eWindowRedeemCode].OnShow = false;
		}
	}
}

#endif
