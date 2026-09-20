// NewUISlotMachine.cpp: implementation of the CNewUISlotMachine class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "NewUISlotMachine.h"
#include "NewUISystem.h"
#include "NewUIBCustomMenu.h"
#include "CBInterface.h"
#include "DSPlaySound.h"
#include "ZzzInfomation.h"
#include "ZzzOpenglUtil.h"
#include "wsclientinline.h"

// The client-wide resolution index, used to cancel the item renderer's built-in
// per-resolution size bonus. Declared rather than pulling in Winmain.h.
extern int m_Resolution;

using namespace SEASON3B;

namespace
{
	// ---- Animation timing ---------------------------------------------------
	// Milliseconds, driven from GetTickCount deltas. A frame counter would tie reel
	// speed to frame rate, so the reels would crawl on a slow machine and blur on a
	// fast one.
	const float kReelSpeedPxPerSec = 900.0f;

	// How long after the answer arrives each reel stops. Left to right, 220ms apart,
	// so the last reel lands about a second after the first - that stagger is what
	// makes a near-miss readable instead of everything resolving at once.
	const DWORD kLandBaseMs = 260;
	const DWORD kLandStepMs = 220;

	// A hitch (alt-tab, a map load) must not teleport a reel.
	const DWORD kMaxFrameDeltaMs = 500;
	const DWORD kAssumedFrameMs = 16;

	// How often the cosmetic pre-answer scroll swaps in a new symbol.
	const DWORD kRollAdvanceMs = 60;

	// ---- Item icon size ----------------------------------------------------
	// The "Scale" argument of RenderItem3DInBatch is a field-of-view in degrees, not
	// a multiplier: a SMALLER number is a narrower lens and a BIGGER icon.
	//
	// RenderItem3DInBatch then ADDS a resolution bonus of up to +0.80 to it, so one
	// hardcoded constant renders at wildly different sizes on different monitors.
	// SlotIconFov below cancels that bonus out, which is why these are expressed as
	// the field of view actually wanted. Lower them to make icons bigger.
	const float kIconFovReel = 0.42f;		// a 64-unit cell
	const float kIconFovClaim = 1.20f;		// a 22-unit reward recess

	// Where the icon lands inside its box, in UI units.
	//
	// RenderItem3D positions the model at (sx + W*0.5, sy + H*0.6) for every item in
	// sections 12 and 14 - they all fall through to its final else. The 0.5 centres
	// horizontally; the 0.6 does not centre vertically, and these cancel the
	// difference. Positive Y moves down.
	const float kIconNudgeX = 0.0f;
	const float kIconNudgeY = -6.4f;		// -(0.6 - 0.5) * 64
	const float kClaimNudgeY = -2.2f;		// -(0.6 - 0.5) * 22

	// The resolution bonus RenderItem3DInBatch is about to add, so it can be
	// subtracted here first. Mirrors its switch exactly; if that ever changes, this
	// has to change with it.
	float SlotIconFov(float wanted)
	{
		float bonus = 0.0f;

		switch(m_Resolution)
		{
			case 3: bonus = 0.30f; break;	// 1280x1024
			case 4: bonus = 0.40f; break;	// 1366x768
			case 5: bonus = 0.50f; break;	// 1440x900
			case 6: bonus = 0.60f; break;	// 1600x900
			case 7: bonus = 0.70f; break;	// 1680x1050
			case 8: bonus = 0.80f; break;	// 1920x1080
		}

		const float fov = (wanted - bonus);

		// A zero or negative field of view is not a valid projection.
		return ((fov < 0.05f) ? 0.05f : fov);
	}

	// ---- Art rectangles ----------------------------------------------------
	// The textures are power-of-two canvases with the art in a corner. These are the
	// measured pixel rectangles of the art itself, so the transparent padding is
	// never stretched onto the screen.
	const float kBtnSrcX = 7.0f, kBtnSrcY = 89.0f, kBtnSrcW = 241.0f, kBtnSrcH = 78.0f;

	// slot_btn_small: three round buttons stacked (normal / hover / pressed), each
	// 75px, 79px apart.
	const float kSmallSrcX = 26.0f, kSmallSrcW = 75.0f, kSmallSrcH = 75.0f;
	const float kSmallSrcY[3] = { 10.0f, 89.0f, 168.0f };

	// Colours. TextDraw takes 0xAABBGGRR - so 0xFFFFD040 comes out LIGHT BLUE, which
	// is how the first build's multiplier labels ended up cyan instead of gold.
	// DrawInfoBox takes 0xRRGGBBAA. They are different orderings; do not swap them.
	const DWORD kTxtWhite   = 0xFFFFFFFF;
	const DWORD kTxtGold    = 0xFF40D0FF;
	const DWORD kTxtSoft    = 0xFFC0C0C0;
	const DWORD kTxtDim     = 0xFF707070;
	const DWORD kTxtRed     = 0xFF5050FF;
	const DWORD kTxtCost    = 0xFF80FFFF;
	const DWORD kTxtTitle   = 0xFF3CB4F0;

	const DWORD kBoxBacking = 0x1A1226F0;	// dark purple behind the transparent frame opening
	const DWORD kBoxWinLine = 0x40FF80A0;
}

//////////////////////////////////////////////////////////////////////
// Layout, in window-local units. Read straight off slot_bg.ozt: the frame's
// opening is x 40..360, y 62..319; the bottom bar's recesses are at y 356..386.
//////////////////////////////////////////////////////////////////////

namespace
{
	// Close: the carved X in the frame's top-right corner. The art is ~20 wide, so
	// the hit rect is padded well past it - a finger needs more than 20 units.
	const float kCloseX = 372.0f, kCloseY = 2.0f, kCloseW = 28.0f, kCloseH = 36.0f;

	const float kStatusY = 258.0f;

	// Stake buttons: one row inside the opening, under the reels.
	// Stakes are laid out in up to TWO rows of four. One row of four was the first
	// version and silently hid every stake past the fourth - the config had six.
	const int   kStakeCols = 4;
	const int   kStakeMaxShown = 8;
	const float kStakeY = 268.0f, kStakeH = 22.0f, kStakeRowStep = 24.0f;
	const float kStakeGap = 8.0f;

	// Bet row.
	// Below the frame opening, on the wooden bar: two rows of stakes use the space
	// the bet row used to have.
	const float kBetY = 330.0f, kBetH = 16.0f;
	const float kMinusX = 76.0f, kPlusX = 140.0f;
	const float kMaxX = 164.0f, kMaxW = 40.0f;

	// The two big actions sit over the bar's recesses.
	const float kSpinX = 156.0f, kSpinY = 358.0f, kSpinW = 90.0f, kSpinH = 28.0f;
	const float kClaimX = 318.0f, kClaimY = 360.0f, kClaimW = 68.0f, kClaimH = 24.0f;

	// Reward icons sit in three of the bar's recesses: the 2nd, 3rd and 5th.
	const float kRewardIconX[3] = { 42.0f, 96.0f, 258.0f };
	const float kRewardIconY = 360.0f, kRewardIconSize = 22.0f;
	const int   kRewardSlotsShown = 3;

	// ONE definition of where stake button n sits, shared by the drawing and the
	// hit-testing. They were separate copies of the same arithmetic before, which is
	// how a four-button cap ended up in both and a six-stake config lost two of them
	// silently.
	void StakeButtonRect(int n, int total, float* x, float* y, float* w, float* h)
	{
		const int shown = ((total > kStakeMaxShown) ? kStakeMaxShown : total);

		// One row while they fit, two once they do not, so three stakes stay full
		// width instead of being squeezed into half a row.
		const int cols = ((shown > kStakeCols) ? kStakeCols : shown);
		const int row = (n / kStakeCols);
		const int col = (n % kStakeCols);

		// The last row may be short; it is centred rather than left-aligned so six
		// stakes read as 4 + 2 centred instead of 4 + 2 hanging off the left.
		const int inThisRow = ((((row + 1) * kStakeCols) <= shown) ? kStakeCols : (shown - (row * kStakeCols)));

		const float full = (320.0f - ((cols - 1) * kStakeGap));
		const float bw = (full / (float)cols);
		const float rowWidth = ((bw * inThisRow) + (kStakeGap * (inThisRow - 1)));

		(*w) = bw;
		(*h) = kStakeH;
		(*x) = (40.0f + ((320.0f - rowWidth) / 2.0f) + (col * (bw + kStakeGap)));
		(*y) = (kStakeY + (row * kStakeRowStep));
	}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CNewUISlotMachine::CNewUISlotMachine() : m_pNewUIMng(NULL)
{
	m_Pos.x = m_Pos.y = 0;

	m_Ready = false;
	m_SymbolCount = 0;
	m_LineCount = 0;
	m_StakeCount = 0;
	m_MultiCap = 10;
	m_SelectedStake = 0;
	m_Bet = 1;
	m_MaxBet = 1;

	memset(m_Symbol, 0, sizeof(m_Symbol));
	memset(m_Line, 0, sizeof(m_Line));
	memset(m_Stake, 0, sizeof(m_Stake));

	memset(m_Cell, 0, sizeof(m_Cell));
	memset(m_ShownCell, 0, sizeof(m_ShownCell));
	memset(m_RollCell, 0, sizeof(m_RollCell));
	memset(m_WinLine, 0, sizeof(m_WinLine));

	m_WinLineCount = 0;
	m_ScatterCount = 0;
	m_TotalPay = 0;
	m_FreeSpinsLeft = 0;
	m_FreeSpinAwarded = 0;
	m_StakeItemIndex = -1;
	m_StakeLevel = 0;
	m_WasFreeSpin = false;
	m_LastResultCode = 0;

	memset(m_Claim, 0, sizeof(m_Claim));
	m_ClaimCount = 0;
	m_ClaimFree = SLOTUI_CLAIM_SIZE;

	m_SpinState = SPIN_IDLE;
	m_SpinStartMs = 0;
	m_LastFrameMs = 0;
	m_LandStartMs = 0;
	m_RollAdvanceMs = 0;

	m_Dragging = false;
	m_DragDX = 0;
	m_DragDY = 0;

	for(int n = 0; n < SLOTUI_REELS; n++)
	{
		m_ReelOffset[n] = 0.0f;
		m_ReelStopMs[n] = 0;
		m_ReelLanded[n] = true;		// nothing is spinning, so nothing is pending
	}
}

CNewUISlotMachine::~CNewUISlotMachine()
{
	Release();
}

bool CNewUISlotMachine::Create(CNewUIManager* pNewUIMng, int x, int y)
{
	if(NULL == pNewUIMng || NULL == g_pNewUI3DRenderMng || NULL == g_pNewItemMng)
		return false;

	LoadImages();

	SetPos(x, y);

	m_pNewUIMng = pNewUIMng;
	m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_SLOTMACHINE, this);

	Show(false);

	return true;
}

void CNewUISlotMachine::Release()
{
	if(m_pNewUIMng)
	{
		m_pNewUIMng->RemoveUIObj(this);
		m_pNewUIMng = NULL;

		// Only when it was actually created: Release is also reached from the
		// destructor of a window whose Create never ran, and deleting ids that were
		// never loaded is not something to leave to the bitmap table.
		UnloadImages();
	}
}

void CNewUISlotMachine::LoadImages()
{
	// The file NAMES do not match what is in them for two of the cells: the green
	// "win" cell was exported as slot_cell_scatter and the star-framed scatter cell
	// as slot_cell_win. Mapped here rather than asking for a re-export.
	LoadBitmap("Custom\\SlotMachine\\slot_bg.tga",			IMAGE_SLOT_BG,			GL_LINEAR);
	LoadBitmap("Custom\\SlotMachine\\slot_cell.tga",		IMAGE_SLOT_CELL,		GL_LINEAR);
	LoadBitmap("Custom\\SlotMachine\\slot_cell_scatter.tga",IMAGE_SLOT_CELL_WIN,	GL_LINEAR);
	LoadBitmap("Custom\\SlotMachine\\slot_cell_win.tga",	IMAGE_SLOT_CELL_SCATTER,GL_LINEAR);
	LoadBitmap("Custom\\SlotMachine\\slot_btn_normal.tga",	IMAGE_SLOT_BTN_NORMAL,	GL_LINEAR);
	LoadBitmap("Custom\\SlotMachine\\slot_btn_hover.tga",	IMAGE_SLOT_BTN_HOVER,	GL_LINEAR);
	LoadBitmap("Custom\\SlotMachine\\slot_btn_pressed.tga",	IMAGE_SLOT_BTN_PRESSED,	GL_LINEAR);
	LoadBitmap("Custom\\SlotMachine\\slot_btn_small.tga",	IMAGE_SLOT_SMALL,		GL_LINEAR);
	LoadBitmap("Custom\\SlotMachine\\slot_plate.tga",		IMAGE_SLOT_PLATE,		GL_LINEAR);
	LoadBitmap("Custom\\SlotMachine\\slot_banner.tga",		IMAGE_SLOT_BANNER,		GL_LINEAR);
}

void CNewUISlotMachine::UnloadImages()
{
	// All ten are this window's own, unlike the earlier build that aliased other
	// windows' art and so must not delete anything.
	for(int id = IMAGE_SLOT_BG; id < IMAGE_SLOT_COUNT_END; id++)
	{
		DeleteBitmap(id);
	}
}

void CNewUISlotMachine::CenterWindow()
{
	float sw = 640.0f;
	float sh = 480.0f;

#if(WIDE_SCREEN)
	// The visible UI area, which on a widescreen client is wider than 640.
	sw = (float)GetWindowsX();
	sh = (float)GetWindowsY();
#endif

	m_Pos.x = (int)((sw - (float)WINDOW_WIDTH) / 2.0f);
	m_Pos.y = (int)((sh - (float)WINDOW_HEIGHT) / 2.0f);

	ClampToScreen();
}

void CNewUISlotMachine::ClampToScreen()
{
	float sw = 640.0f;
	float sh = 480.0f;

#if(WIDE_SCREEN)
	sw = (float)GetWindowsX();
	sh = (float)GetWindowsY();
#endif

	// The whole window is kept on screen, not just its title bar. A window dragged
	// half off the edge would hide the buttons with no way to pull it back.
	int maxX = (int)(sw - (float)WINDOW_WIDTH);
	int maxY = (int)(sh - (float)WINDOW_HEIGHT);

	if(maxX < 0)
		maxX = 0;

	if(maxY < 0)
		maxY = 0;

	if(m_Pos.x < 0)
		m_Pos.x = 0;

	if(m_Pos.y < 0)
		m_Pos.y = 0;

	if(m_Pos.x > maxX)
		m_Pos.x = maxX;

	if(m_Pos.y > maxY)
		m_Pos.y = maxY;
}

void CNewUISlotMachine::OpeningProcess()
{
	m_SpinState = SPIN_IDLE;
	m_LastFrameMs = GetTickCount();

	// Spawns in the middle every time it opens. A position dragged to last session
	// is deliberately not remembered: the window can only be dragged while open, so
	// remembering it would just be a way to open somewhere the player forgot.
	m_Dragging = false;

	CenterWindow();
}

void CNewUISlotMachine::ClosingProcess()
{
	// The machine's own state (free spins, held winnings) lives on the server, so
	// nothing here needs preserving. Only the animation is reset, so re-opening does
	// not resume a spin that already finished.
	m_SpinState = SPIN_IDLE;

	for(int n = 0; n < SLOTUI_REELS; n++)
	{
		m_ReelOffset[n] = 0.0f;
	}

	m_Ready = false;

	// Tells the server the machine is closed, which is what clears its open flag and
	// releases the interface lock. Without this the player could walk away from the
	// NPC and keep spinning, because the server's only test for "standing at the
	// machine" is that flag. 0xC1:0x31 is the same close every NPC window sends.
	SendExitInventory();
}

//////////////////////////////////////////////////////////////////////
// Symbol helpers
//////////////////////////////////////////////////////////////////////

int CNewUISlotMachine::SymbolItemIndex(int symbol)
{
	if(symbol < 0 || symbol >= m_SymbolCount)
		return -1;

	return (int)m_Symbol[symbol].ItemIndex;
}

int CNewUISlotMachine::SymbolLevel(int symbol)
{
	if(symbol < 0 || symbol >= m_SymbolCount)
		return 0;

	return (int)m_Symbol[symbol].Level;
}

bool CNewUISlotMachine::IsCellOnWinningLine(int reel, int row)
{
	// Only true once the reveal has happened. Highlighting a winning cell while the
	// reels are still turning would give the result away early.
	if(m_SpinState != SPIN_REVEAL)
		return false;

	for(int n = 0; n < m_WinLineCount; n++)
	{
		int line = m_WinLine[n].line;

		if(line < 0 || line >= m_LineCount)
			continue;

		// Only cells inside the winning run count - the line continues past where
		// the run broke, and those cells did not win anything.
		if(reel >= m_WinLine[n].count)
			continue;

		if(m_Line[line].Row[reel] == row)
			return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////
// Packets
//////////////////////////////////////////////////////////////////////

void CNewUISlotMachine::RecvOpen(BYTE* lpMsg)
{
	PMSG_SLOT_OPEN_RECV* pRecv = (PMSG_SLOT_OPEN_RECV*)lpMsg;

	// A server built for a different grid would mean every cell index below lands
	// somewhere else. Refuse rather than draw nonsense.
	if(pRecv->Reels != SLOTUI_REELS || pRecv->Rows != SLOTUI_ROWS)
	{
		g_pChatListBox->AddText("", "Slot machine: this client does not match the server's layout.", SEASON3B::TYPE_ERROR_MESSAGE);
		return;
	}

	m_SymbolCount = ((pRecv->SymbolCount > SLOTUI_MAX_SYMBOL) ? SLOTUI_MAX_SYMBOL : pRecv->SymbolCount);
	m_LineCount = ((pRecv->LineCount > SLOTUI_MAX_LINE) ? SLOTUI_MAX_LINE : pRecv->LineCount);
	m_StakeCount = ((pRecv->StakeCount > SLOTUI_MAX_STAKE) ? SLOTUI_MAX_STAKE : pRecv->StakeCount);
	m_MultiCap = pRecv->MultiCap;
	m_FreeSpinsLeft = pRecv->FreeSpinsLeft;

	m_MaxBet = ((pRecv->MaxBet < 1) ? 1 : pRecv->MaxBet);

	// Clamped rather than reset, so re-opening the machine keeps the bet the player
	// last chose - unless this machine allows less than that one did.
	if(m_Bet > m_MaxBet)
		m_Bet = m_MaxBet;

	if(m_Bet < 1)
		m_Bet = 1;

	// Walked with the counts the header declares, so a short packet cannot make this
	// read past the buffer.
	BYTE* p = (lpMsg + sizeof(PMSG_SLOT_OPEN_RECV));

	for(int n = 0; n < m_SymbolCount; n++)
	{
		memcpy(&m_Symbol[n], p, sizeof(PMSG_SLOT_OPEN_SYMBOL));
		p += sizeof(PMSG_SLOT_OPEN_SYMBOL);
	}

	for(int n = 0; n < m_LineCount; n++)
	{
		memcpy(&m_Line[n], p, sizeof(PMSG_SLOT_OPEN_LINE));
		p += sizeof(PMSG_SLOT_OPEN_LINE);
	}

	for(int n = 0; n < m_StakeCount; n++)
	{
		memcpy(&m_Stake[n], p, sizeof(PMSG_SLOT_OPEN_STAKE));
		p += sizeof(PMSG_SLOT_OPEN_STAKE);

		// The name came off the wire and is drawn as a C string.
		m_Stake[n].Name[sizeof(m_Stake[n].Name) - 1] = '\0';
	}

	if(m_SelectedStake >= m_StakeCount)
		m_SelectedStake = 0;

	// Start with something on the reels rather than a grid of symbol 0, so the
	// window does not look broken before the first spin.
	for(int n = 0; n < SLOTUI_CELLS; n++)
	{
		m_ShownCell[n] = (BYTE)((m_SymbolCount > 0) ? (rand() % m_SymbolCount) : 0);
		m_Cell[n] = m_ShownCell[n];
	}

	m_WinLineCount = 0;
	m_TotalPay = 0;
	m_Ready = true;
	m_SpinState = SPIN_IDLE;

	g_pNewUISystem->Show(SEASON3B::INTERFACE_SLOTMACHINE);
}

void CNewUISlotMachine::RecvResult(BYTE* lpMsg)
{
	PMSG_SLOT_RESULT_RECV* pRecv = (PMSG_SLOT_RESULT_RECV*)lpMsg;

	m_LastResultCode = pRecv->result;
	m_FreeSpinsLeft = pRecv->FreeSpinsLeft;

	// A refusal carries no grid. Stop the cosmetic roll and put the previous result
	// back, rather than landing on fifteen zeroes.
	if(pRecv->result != 0)
	{
		m_SpinState = SPIN_IDLE;
		return;
	}

	memcpy(m_Cell, pRecv->Cell, sizeof(m_Cell));

	m_ScatterCount = pRecv->ScatterCount;
	m_TotalPay = pRecv->TotalPay;
	m_FreeSpinAwarded = pRecv->FreeSpinAwarded;
	m_StakeItemIndex = pRecv->StakeItemIndex;
	m_StakeLevel = pRecv->StakeLevel;
	m_WasFreeSpin = ((pRecv->WasFreeSpin != 0) ? true : false);

	// Taken from the reply, not left as what was asked for. During free spins the
	// server overrides the bet with the one that triggered the round, and the status
	// line has to show what was actually staked.
	if(pRecv->BetCount >= 1)
		m_Bet = pRecv->BetCount;

	m_WinLineCount = ((pRecv->WinLineCount > SLOTUI_MAX_LINE) ? SLOTUI_MAX_LINE : pRecv->WinLineCount);

	BYTE* p = (lpMsg + sizeof(PMSG_SLOT_RESULT_RECV));

	for(int n = 0; n < m_WinLineCount; n++)
	{
		memcpy(&m_WinLine[n], p, sizeof(PMSG_SLOT_RESULT_LINE));
		p += sizeof(PMSG_SLOT_RESULT_LINE);
	}

	// The answer is in hand but NOTHING is shown yet. Everything downstream reads
	// m_SpinState, and only the landing animation may set SPIN_REVEAL - that single
	// gate is what stops the win being visible the moment the packet lands.
	const DWORD now = GetTickCount();

	m_SpinState = SPIN_LANDING;

	// None of the reels has landed yet - UpdateReels lands them one at a time.
	for(int reel = 0; reel < SLOTUI_REELS; reel++)
	{
		m_ReelLanded[reel] = false;
	}
	m_LandStartMs = now;

	for(int reel = 0; reel < SLOTUI_REELS; reel++)
	{
		m_ReelStopMs[reel] = now + kLandBaseMs + (kLandStepMs * reel);
	}
}

void CNewUISlotMachine::RecvClaimList(BYTE* lpMsg)
{
	PMSG_SLOT_CLAIM_LIST_RECV* pRecv = (PMSG_SLOT_CLAIM_LIST_RECV*)lpMsg;

	m_ClaimCount = ((pRecv->count > SLOTUI_CLAIM_SIZE) ? SLOTUI_CLAIM_SIZE : pRecv->count);
	m_ClaimFree = pRecv->free;

	BYTE* p = (lpMsg + sizeof(PMSG_SLOT_CLAIM_LIST_RECV));

	for(int n = 0; n < m_ClaimCount; n++)
	{
		memcpy(&m_Claim[n], p, sizeof(PMSG_SLOT_CLAIM_ROW));
		p += sizeof(PMSG_SLOT_CLAIM_ROW);
	}
}

void CNewUISlotMachine::SendSpin()
{
	PMSG_SLOT_SPIN_SEND pMsg;

	// 0xD3 must go unencrypted - HackPacketCheck lists head 211 with Encrypt = 0 and
	// closes the connection on a mismatch before the packet is even dispatched.
	// DataSend ends in a plain send, so this is the correct path.
	pMsg.header.set(0xD3, 0x90, sizeof(pMsg));

	pMsg.stake = (BYTE)m_SelectedStake;
	pMsg.bet = (BYTE)m_Bet;

	DataSend((LPBYTE)&pMsg, pMsg.header.size);
}

void CNewUISlotMachine::SendClaim(int slot)
{
	PMSG_SLOT_CLAIM_SEND pMsg;

	pMsg.header.set(0xD3, 0x92, sizeof(pMsg));

	pMsg.slot = (BYTE)slot;

	DataSend((LPBYTE)&pMsg, pMsg.header.size);
}

//////////////////////////////////////////////////////////////////////
// Animation
//////////////////////////////////////////////////////////////////////

bool CNewUISlotMachine::IsCellSpinning(int reel)
{
	if(m_SpinState == SPIN_ROLLING)
		return true;

	if(m_SpinState != SPIN_LANDING)
		return false;

	// A reel keeps spinning until UpdateReels COMMITS it, not until a clock reads
	// past its stop time. Testing the clock here as well let Render see a reel as
	// stopped a few milliseconds before Update had put its real symbols in, which
	// showed one frame of stale symbols on every stop.
	return ((m_ReelLanded[reel] == false) ? true : false);
}

void CNewUISlotMachine::UpdateReels()
{
	const DWORD now = GetTickCount();

	DWORD dt = ((m_LastFrameMs != 0) ? (now - m_LastFrameMs) : kAssumedFrameMs);

	// A hitch must not advance the reels by a whole second's worth of travel.
	if(dt > kMaxFrameDeltaMs)
		dt = kAssumedFrameMs;

	m_LastFrameMs = now;

	if(m_SpinState != SPIN_ROLLING && m_SpinState != SPIN_LANDING)
		return;

	const float advance = kReelSpeedPxPerSec * ((float)dt / 1000.0f);

	// Land reels ONE AT A TIME, 1 -> 2 -> 3 -> 4 -> 5, each with its own real
	// result. The first version copied the whole grid only when the LAST reel
	// stopped, so reels 1-4 sat there showing the previous spin's symbols until reel
	// 5 landed and every cell then changed at once - which read as the result being
	// random.
	if(m_SpinState == SPIN_LANDING)
	{
		for(int reel = 0; reel < SLOTUI_REELS; reel++)
		{
			if(m_ReelLanded[reel] != false)
				continue;

			if(now < m_ReelStopMs[reel])
				continue;

			for(int row = 0; row < SLOTUI_ROWS; row++)
			{
				const int idx = ((reel * SLOTUI_ROWS) + row);

				m_ShownCell[idx] = m_Cell[idx];
			}

			m_ReelLanded[reel] = true;
			m_ReelOffset[reel] = 0.0f;

			PlayBuffer(SOUND_JEWEL01);
		}
	}

	bool allStopped = true;

	for(int reel = 0; reel < SLOTUI_REELS; reel++)
	{
		if(IsCellSpinning(reel) == false)
		{
			m_ReelOffset[reel] = 0.0f;
			continue;
		}

		allStopped = false;

		m_ReelOffset[reel] += advance;

		while(m_ReelOffset[reel] >= (float)CELL_SIZE)
		{
			m_ReelOffset[reel] -= (float)CELL_SIZE;
		}
	}

	// Swap the cosmetic symbols periodically so a spinning reel actually looks like
	// different symbols going past rather than one image sliding.
	if(now >= m_RollAdvanceMs)
	{
		m_RollAdvanceMs = now + kRollAdvanceMs;

		for(int n = 0; n < SLOTUI_CELLS; n++)
		{
			if(m_SymbolCount > 0)
				m_RollCell[n] = (BYTE)(rand() % m_SymbolCount);
		}
	}

	if(m_SpinState == SPIN_LANDING && allStopped != false)
	{
		// The single moment the WIN becomes visible. The symbols themselves are
		// already on screen by now, landed reel by reel above; what waits for the last
		// reel is everything that announces a result - the highlighted cells, the win
		// lines, the payout text - all gated on SPIN_REVEAL.
		m_SpinState = SPIN_REVEAL;

		if(m_TotalPay > 0)
		{
			PlayBuffer(SOUND_GET_ITEM01);
		}

		// Fired on the SCATTERS, not on the free spins they usually award. Those two
		// came apart once the retrigger cap existed: scatters landing during a
		// capped-out bonus grant nothing, and staying silent for them made the
		// biggest symbol on the grid look like it had not registered.
		if(m_ScatterCount >= 3)
		{
			PlayBuffer(SOUND_FIRECRACKER1);
		}
	}
}

//////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////

int CNewUISlotMachine::ButtonState(float x, float y, float w, float h)
{
	if(SEASON3B::CheckMouseIn((int)(m_Pos.x + x), (int)(m_Pos.y + y), (int)w, (int)h) == false)
		return 0;

	return ((MouseLButton != false) ? 2 : 1);
}

bool CNewUISlotMachine::UpdateMouseEvent()
{
	const bool busy = ((m_SpinState == SPIN_ROLLING || m_SpinState == SPIN_LANDING) ? true : false);

	// An active drag owns the mouse until the button is released, wherever the
	// cursor has wandered to - otherwise moving fast enough to outrun the window
	// would drop it mid-drag.
	if(m_Dragging != false)
	{
		if(MouseLButton != false)
		{
			m_Pos.x = MouseX - m_DragDX;
			m_Pos.y = MouseY - m_DragDY;

			ClampToScreen();

			return false;
		}

		m_Dragging = false;
	}

	if(SEASON3B::IsPress(VK_LBUTTON))
	{
		// Close: the X carved into the frame.
		if(SEASON3B::CheckMouseIn(m_Pos.x + (int)kCloseX, m_Pos.y + (int)kCloseY, (int)kCloseW, (int)kCloseH))
		{
			PlayBuffer(SOUND_CLICK01);
			g_pNewUISystem->Hide(SEASON3B::INTERFACE_SLOTMACHINE);
			return false;
		}

		// Drag handle: the title bar across the top of the frame. Only here, so the
		// reels and buttons underneath stay ordinary clicks. Checked AFTER the close
		// X, which sits inside the same strip.
		if(SEASON3B::CheckMouseIn(m_Pos.x, m_Pos.y, WINDOW_WIDTH, 46))
		{
			m_Dragging = true;
			m_DragDX = MouseX - m_Pos.x;
			m_DragDY = MouseY - m_Pos.y;

			return false;
		}

		if(m_Ready != false)
		{
			// SPIN.
			if(SEASON3B::CheckMouseIn(m_Pos.x + (int)kSpinX, m_Pos.y + (int)kSpinY, (int)kSpinW, (int)kSpinH))
			{
				if(busy == false)
				{
					PlayBuffer(SOUND_CLICK01);

					SendSpin();

					// The reels start turning immediately, before the server answers.
					// If they waited, a press would look like it did nothing for a
					// whole round trip.
					m_SpinState = SPIN_ROLLING;
					m_SpinStartMs = GetTickCount();
					m_WinLineCount = 0;
					m_TotalPay = 0;
					m_FreeSpinAwarded = 0;
					m_LastResultCode = 0;
				}

				return false;
			}

			// CLAIM ALL.
			if(SEASON3B::CheckMouseIn(m_Pos.x + (int)kClaimX, m_Pos.y + (int)kClaimY, (int)kClaimW, (int)kClaimH))
			{
				if(m_ClaimCount > 0)
				{
					PlayBuffer(SOUND_CLICK01);
					SendClaim(0xFF);
				}

				return false;
			}

			// Stake buttons, laid out the same way DrawStakeRow lays them out.
			const int shown = ((m_StakeCount > kStakeMaxShown) ? kStakeMaxShown : m_StakeCount);

			for(int n = 0; n < shown; n++)
			{
				float bx, by, bw, bh;

				StakeButtonRect(n, m_StakeCount, &bx, &by, &bw, &bh);

				if(SEASON3B::CheckMouseIn(m_Pos.x + (int)bx, m_Pos.y + (int)by, (int)bw, (int)bh))
				{
					if(busy == false)
					{
						m_SelectedStake = n;
						PlayBuffer(SOUND_CLICK01);
					}

					return false;
				}
			}

			// Bet stepper. Blocked while the reels turn and while free spins are
			// running, because the server locks the bet for the whole round - letting
			// it be changed here would show one figure and stake another.
			const bool betLocked = ((busy != false || m_FreeSpinsLeft > 0) ? true : false);

			if(SEASON3B::CheckMouseIn(m_Pos.x + (int)kMinusX, m_Pos.y + (int)kBetY, (int)kBetH, (int)kBetH))
			{
				if(betLocked == false && m_Bet > 1)
				{
					m_Bet--;
					PlayBuffer(SOUND_CLICK01);
				}

				return false;
			}

			if(SEASON3B::CheckMouseIn(m_Pos.x + (int)kPlusX, m_Pos.y + (int)kBetY, (int)kBetH, (int)kBetH))
			{
				if(betLocked == false && m_Bet < m_MaxBet)
				{
					m_Bet++;
					PlayBuffer(SOUND_CLICK01);
				}

				return false;
			}

			if(SEASON3B::CheckMouseIn(m_Pos.x + (int)kMaxX, m_Pos.y + (int)kBetY, (int)kMaxW, (int)kBetH))
			{
				if(betLocked == false)
				{
					m_Bet = m_MaxBet;
					PlayBuffer(SOUND_CLICK01);
				}

				return false;
			}

			// A reward row can also be claimed on its own, so a big win that will not
			// fit does not block a small one.
			const int rows = ((m_ClaimCount > kRewardSlotsShown) ? kRewardSlotsShown : m_ClaimCount);

			for(int n = 0; n < rows; n++)
			{
				if(SEASON3B::CheckMouseIn(m_Pos.x + (int)kRewardIconX[n], m_Pos.y + (int)kRewardIconY, (int)kRewardIconSize, (int)kRewardIconSize))
				{
					PlayBuffer(SOUND_CLICK01);
					SendClaim(m_Claim[n].slot);
					return false;
				}
			}
		}
	}

	// Swallow everything inside the window, so a click on the machine never also
	// reaches the world behind it.
	if(SEASON3B::CheckMouseIn(m_Pos.x, m_Pos.y, WINDOW_WIDTH, WINDOW_HEIGHT))
	{
		if(SEASON3B::IsPress(VK_RBUTTON))
		{
			MouseRButton = false;
			MouseRButtonPop = false;
			MouseRButtonPush = false;
			return false;
		}

		if(SEASON3B::IsNone(VK_LBUTTON) == false)
		{
			return false;
		}
	}

	return true;
}

bool CNewUISlotMachine::UpdateKeyEvent()
{
	return true;
}

bool CNewUISlotMachine::Update()
{
	UpdateReels();

	return true;
}

//////////////////////////////////////////////////////////////////////
// Render
//////////////////////////////////////////////////////////////////////

// Every draw group starts from this. The item renders, DrawInfoBox and TextDraw all
// leave GL state this window did not set: a raw glDisable(GL_TEXTURE_2D) leaves
// ZzzOpenglUtil's shadow cache saying "texture on" while GL has it off, so the next
// EnableAlphaTest is a no-op and the textured quad comes out solid white.
// DisableTexture drives the shadow and GL back into agreement so the
// EnableAlphaTest after it does a real glEnable.
static void ResetSlotGLState()
{
	DisableTexture(false);
	::EnableAlphaTest();
	glColor4f(1.f, 1.f, 1.f, 1.f);
}

void CNewUISlotMachine::DrawArt(int image, float x, float y, float w, float h, float srcX, float srcY, float srcW, float srcH)
{
	BITMAP_t* pImage = &Bitmaps[image];

	if(pImage->Width <= 0 || pImage->Height <= 0)
		return;

	const float tw = (float)pImage->Width;
	const float th = (float)pImage->Height;

	// A quarter-texel inset on every edge. With GL_LINEAR, sampling exactly on the
	// rectangle's border blends in the transparent padding beside it and draws a
	// faint halo around every button.
	const float e = 0.25f;

	RenderBitmap(image, m_Pos.x + x, m_Pos.y + y, w, h,
		(srcX + e) / tw, (srcY + e) / th, (srcW - (2.0f * e)) / tw, (srcH - (2.0f * e)) / th);
}

void CNewUISlotMachine::DrawWideButton(float x, float y, float w, float h, int state, const char* label, DWORD color)
{
	int image = IMAGE_SLOT_BTN_NORMAL;

	if(state == 1)
		image = IMAGE_SLOT_BTN_HOVER;
	else if(state == 2)
		image = IMAGE_SLOT_BTN_PRESSED;

	DrawArt(image, x, y, w, h, kBtnSrcX, kBtnSrcY, kBtnSrcW, kBtnSrcH);

	if(label != NULL && label[0] != '\0')
	{
		// A pressed button sits one unit lower, which is what sells the press with
		// no second frame of art.
		const float nudge = ((state == 2) ? 1.0f : 0.0f);

		TextDraw((HFONT)g_hFontBold, (int)(m_Pos.x + x), (int)(m_Pos.y + y + ((h - 12.0f) / 2.0f) + nudge), color, 0x0, (int)w, 0, 3, (char*)label);

		// TextDraw leaves the font atlas bound; the next art draw must not inherit it.
		ResetSlotGLState();
	}
}

void CNewUISlotMachine::DrawRoundButton(float x, float y, float size, int state)
{
	const int row = ((state < 0) ? 0 : ((state > 2) ? 2 : state));

	DrawArt(IMAGE_SLOT_SMALL, x, y, size, size, kSmallSrcX, kSmallSrcY[row], kSmallSrcW, kSmallSrcH);
}

void CNewUISlotMachine::DrawFrame()
{
	// The frame's opening is TRANSPARENT, so the game world would show through the
	// reels. A dark panel goes behind it first; the frame art then covers this box's
	// own border, which is why it can safely overrun the opening by a couple of units.
	g_pBCustomMenuInfo->DrawInfoBox((float)(m_Pos.x + 38), (float)(m_Pos.y + 60), 322.0f, 260.0f, kBoxBacking, 0, 0);

	ResetSlotGLState();

	DrawArt(IMAGE_SLOT_BG, 0.0f, 0.0f, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT, 0.0f, 0.0f, 1024.0f, 1024.0f);
}

void CNewUISlotMachine::DrawOneCell(float x, float y, int symbol, bool winning)
{
	int image = IMAGE_SLOT_CELL;

	if(winning != false)
	{
		image = IMAGE_SLOT_CELL_WIN;
	}
	else if(symbol >= 0 && symbol < m_SymbolCount && m_Symbol[symbol].Kind == SLOTUI_KIND_SCATTER)
	{
		// Scatters get the star frame even when they did not form a line, because
		// they pay from anywhere - a player needs to be able to count them at a
		// glance.
		image = IMAGE_SLOT_CELL_SCATTER;
	}

	DrawArt(image, x, y, (float)CELL_SIZE, (float)CELL_SIZE, 0.0f, 0.0f, 128.0f, 128.0f);
}

void CNewUISlotMachine::DrawReels()
{
	const float gx = (float)GRID_X;
	const float gy = (float)GRID_Y;

	for(int reel = 0; reel < SLOTUI_REELS; reel++)
	{
		for(int row = 0; row < SLOTUI_ROWS; row++)
		{
			const int idx = ((reel * SLOTUI_ROWS) + row);
			const bool spinning = IsCellSpinning(reel);
			const int symbol = ((spinning != false) ? m_RollCell[idx] : m_ShownCell[idx]);

			DrawOneCell(gx + (reel * CELL_SIZE), gy + (row * CELL_SIZE), symbol, IsCellOnWinningLine(reel, row));
		}
	}

	// One batch for all fifteen icons. RenderItem3DFree's viewport and alpha-test
	// setup is otherwise paid fifteen times a frame.
	g_pNewUISystem->BeginItem3DFreeBatch();

	for(int reel = 0; reel < SLOTUI_REELS; reel++)
	{
		const bool spinning = IsCellSpinning(reel);

		// While a reel turns its icons are drawn offset, which is what reads as
		// motion. The offset is deliberately NOT applied once it has stopped, so a
		// landed reel sits exactly in its cells.
		const float slide = ((spinning != false) ? m_ReelOffset[reel] : 0.0f);

		for(int row = 0; row < SLOTUI_ROWS; row++)
		{
			const int idx = ((reel * SLOTUI_ROWS) + row);
			const int symbol = ((spinning != false) ? m_RollCell[idx] : m_ShownCell[idx]);
			const int item = SymbolItemIndex(symbol);

			if(item < 0)
				continue;

			// The quad is the WHOLE cell, and FixY is on. RenderItem3D places the model
			// at a per-item-type fraction of the quad (0.5, 0.55, 0.65, 0.8... in
			// ZzzInventory.cpp) that is tuned to centre it in a box of exactly this
			// shape, and GetItem3DFixYPosition corrects the vertical for the types
			// that sit low. The previous build used a 52px quad inset by 6 with FixY
			// off, which is why the icons rode low in their cells.
			const float ix = m_Pos.x + gx + (reel * CELL_SIZE) + kIconNudgeX;
			const float iy = m_Pos.y + gy + (row * CELL_SIZE) + slide + kIconNudgeY;

			// Clipped by hand rather than with a scissor rect: a sliding icon would
			// otherwise draw over the frame below the grid.
			if(iy > (m_Pos.y + gy + (CELL_SIZE * SLOTUI_ROWS) - 8.0f))
				continue;

			g_pNewUISystem->RenderItem3DInBatch(ix, iy, (float)CELL_SIZE, (float)CELL_SIZE, item, SymbolLevel(symbol), 0, 0, false, SlotIconFov(kIconFovReel), false);
		}
	}

	g_pNewUISystem->EndItem3DFreeBatch();

	ResetSlotGLState();

	// The multiplier value drawn on the coin, as text, so a x7 symbol added to the
	// config later needs nothing new. The coin is the left half of slot_plate.
	for(int reel = 0; reel < SLOTUI_REELS; reel++)
	{
		if(IsCellSpinning(reel) != false)
			continue;

		for(int row = 0; row < SLOTUI_ROWS; row++)
		{
			const int symbol = m_ShownCell[(reel * SLOTUI_ROWS) + row];

			if(symbol < 0 || symbol >= m_SymbolCount)
				continue;

			if(m_Symbol[symbol].Kind != SLOTUI_KIND_MULTIPLIER)
				continue;

			const float px = gx + (reel * CELL_SIZE) + CELL_SIZE - 26.0f;
			const float py = gy + (row * CELL_SIZE) + CELL_SIZE - 26.0f;

			DrawArt(IMAGE_SLOT_PLATE, px, py, 24.0f, 24.0f, 0.0f, 0.0f, 64.0f, 64.0f);

			char szMulti[16];
			sprintf(szMulti, "x%d", (int)m_Symbol[symbol].Value);

			TextDraw((HFONT)g_hFontBold, (int)(m_Pos.x + px), (int)(m_Pos.y + py + 7.0f), kTxtWhite, 0x0, 24, 0, 3, szMulti);

			ResetSlotGLState();
		}
	}
}

void CNewUISlotMachine::DrawWinLines()
{
	if(m_SpinState != SPIN_REVEAL || m_WinLineCount <= 0)
		return;

	const float gx = (float)GRID_X;
	const float gy = (float)GRID_Y;

	// Drawn as short segments across the centres of the winning cells - a quad per
	// step rather than one stretched image, so no artwork is involved.
	for(int n = 0; n < m_WinLineCount; n++)
	{
		const int line = m_WinLine[n].line;

		if(line < 0 || line >= m_LineCount)
			continue;

		for(int reel = 0; reel < m_WinLine[n].count; reel++)
		{
			const int row = m_Line[line].Row[reel];

			const float cx = m_Pos.x + gx + (reel * CELL_SIZE);
			const float cy = m_Pos.y + gy + (row * CELL_SIZE) + (CELL_SIZE / 2) - 1.0f;

			g_pBCustomMenuInfo->DrawInfoBox(cx, cy, (float)CELL_SIZE, 2.0f, kBoxWinLine, 0, 0);
		}
	}

	ResetSlotGLState();
}

void CNewUISlotMachine::DrawStakeRow()
{
	// Up to eight, in two rows of four. Geometry comes from StakeButtonRect so this
	// and the hit-testing cannot disagree about where a button is.
	const int shown = ((m_StakeCount > kStakeMaxShown) ? kStakeMaxShown : m_StakeCount);

	if(shown <= 0)
		return;

	const bool busy = ((m_SpinState == SPIN_ROLLING || m_SpinState == SPIN_LANDING) ? true : false);

	for(int n = 0; n < shown; n++)
	{
		float bx, by, bw, bh;

		StakeButtonRect(n, m_StakeCount, &bx, &by, &bw, &bh);

		const bool selected = ((n == m_SelectedStake) ? true : false);

		// The chosen stake wears the HOVER art - the cyan glow - as its "selected"
		// look, so no fourth state had to be drawn. Real hover only shows on the
		// others, and not at all while the reels are turning.
		int state = 0;

		if(selected != false)
			state = 1;
		else if(busy == false)
			state = ButtonState(bx, by, bw, bh);

		DrawWideButton(bx, by, bw, bh, state, m_Stake[n].Name, ((selected != false) ? kTxtWhite : kTxtSoft));
	}
}

void CNewUISlotMachine::DrawBetRow()
{
	const bool busy = ((m_SpinState == SPIN_ROLLING || m_SpinState == SPIN_LANDING) ? true : false);
	const bool betLocked = ((busy != false || m_FreeSpinsLeft > 0) ? true : false);

	TextDraw((HFONT)g_hFont, (int)(m_Pos.x + 44), (int)(m_Pos.y + kBetY + 2.0f), kTxtSoft, 0x0, 30, 0, 1, "Bet");
	ResetSlotGLState();

	// - [ n ] +  MAX. Stepper rather than preset buttons, because MaxBet is
	// configurable and presets would either overshoot it or under-use it.
	DrawRoundButton(kMinusX, kBetY, kBetH, ((betLocked == false && m_Bet > 1) ? ButtonState(kMinusX, kBetY, kBetH, kBetH) : 2));

	TextDraw((HFONT)g_hFontBold, (int)(m_Pos.x + 74), (int)(m_Pos.y + kBetY + 2.0f), kTxtWhite, 0x0, 20, 0, 3, "-");

	char szBet[32];
	sprintf(szBet, "%d", m_Bet);

	TextDraw((HFONT)g_hFontBold, (int)(m_Pos.x + 96), (int)(m_Pos.y + kBetY + 2.0f), kTxtGold, 0x0, 40, 0, 3, szBet);

	ResetSlotGLState();

	DrawRoundButton(kPlusX, kBetY, kBetH, ((betLocked == false && m_Bet < m_MaxBet) ? ButtonState(kPlusX, kBetY, kBetH, kBetH) : 2));

	TextDraw((HFONT)g_hFontBold, (int)(m_Pos.x + kPlusX - 2.0f), (int)(m_Pos.y + kBetY + 2.0f), kTxtWhite, 0x0, 20, 0, 3, "+");

	ResetSlotGLState();

	DrawWideButton(kMaxX, kBetY, kMaxW, kBetH, ((betLocked == false) ? ButtonState(kMaxX, kBetY, kMaxW, kBetH) : 2), "MAX", ((betLocked == false) ? kTxtWhite : kTxtDim));

	// What this spin actually costs, spelled out.
	char szCost[128];

	if(m_FreeSpinsLeft > 0)
	{
		sprintf(szCost, "free spin - bet locked at %d", m_Bet);
	}
	else if(m_SelectedStake >= 0 && m_SelectedStake < m_StakeCount)
	{
		// Amount is how many items or coins one unit of bet costs, so the real cost
		// is bet x amount. Showing only the bet would understate a 100-coin stake
		// by a factor of a hundred.
		const DWORD cost = ((DWORD)m_Bet * m_Stake[m_SelectedStake].Amount);

		sprintf(szCost, "= %u %s", cost, m_Stake[m_SelectedStake].Name);
	}
	else
	{
		sprintf(szCost, " ");
	}

	TextDraw((HFONT)g_hFont, (int)(m_Pos.x + 210), (int)(m_Pos.y + kBetY + 2.0f), kTxtCost, 0x0, 150, 0, 1, szCost);

	ResetSlotGLState();
}

void CNewUISlotMachine::DrawRewardBox()
{
	// The standing "Rewards: n held" line is gone - the bet row now occupies that
	// strip, and the icons with their counts already say what is held. Only the two
	// cases the player has to ACT on are worth the space.
	if(m_ClaimFree <= 0 || m_ClaimCount > kRewardSlotsShown)
	{
		char szLabel[96];

		if(m_ClaimFree <= 0)
		{
			// This one blocks spinning, so it is stated in red.
			sprintf(szLabel, "REWARD BOX FULL - claim to keep playing");
		}
		else
		{
			sprintf(szLabel, "+%d more held", m_ClaimCount - kRewardSlotsShown);
		}

		TextDraw((HFONT)g_hFont, (int)(m_Pos.x + 200), (int)(m_Pos.y + 348), ((m_ClaimFree <= 0) ? kTxtRed : kTxtSoft), 0x0, 160, 0, 2, szLabel);

		ResetSlotGLState();
	}

	const int rows = ((m_ClaimCount > kRewardSlotsShown) ? kRewardSlotsShown : m_ClaimCount);

	if(rows <= 0)
		return;

	g_pNewUISystem->BeginItem3DFreeBatch();

	for(int n = 0; n < rows; n++)
	{
		g_pNewUISystem->RenderItem3DInBatch(m_Pos.x + kRewardIconX[n], m_Pos.y + kRewardIconY + kClaimNudgeY, kRewardIconSize, kRewardIconSize, (int)m_Claim[n].ItemIndex, (int)m_Claim[n].Level, 0, 0, false, SlotIconFov(kIconFovClaim), false);
	}

	g_pNewUISystem->EndItem3DFreeBatch();

	ResetSlotGLState();

	// The count is the whole point of a row, so it sits right beside the icon.
	for(int n = 0; n < rows; n++)
	{
		char szCount[16];
		sprintf(szCount, "%u", m_Claim[n].Count);

		TextDraw((HFONT)g_hFontBold, (int)(m_Pos.x + kRewardIconX[n] + kRewardIconSize + 1.0f), (int)(m_Pos.y + kRewardIconY + 7.0f), kTxtGold, 0x0, 30, 0, 1, szCount);
	}

	ResetSlotGLState();
}

void CNewUISlotMachine::DrawStatusText()
{
	// The plaque at the top of the frame is blank wood. It carries the title, or -
	// while free spins run - the banner and their count, because they persist across
	// spins and closing the window and the player needs to see them even on a
	// losing spin.
	if(m_FreeSpinsLeft > 0)
	{
		DrawArt(IMAGE_SLOT_BANNER, 98.0f, 6.0f, 204.0f, 38.0f, 1.0f, 1.0f, 510.0f, 126.0f);

		char szFree[64];
		sprintf(szFree, "FREE SPINS: %d", m_FreeSpinsLeft);

		TextDraw((HFONT)g_hFontBold, (int)(m_Pos.x + 98), (int)(m_Pos.y + 20), 0xFF102060, 0x0, 204, 0, 3, szFree);
	}
	else
	{
		TextDraw((HFONT)g_hFontBold, (int)(m_Pos.x + 98), (int)(m_Pos.y + 18), kTxtTitle, 0x0, 204, 0, 3, "SLOT MACHINE");
	}

	ResetSlotGLState();

	char szLine[160];

	// One status line, whose content is entirely decided by m_SpinState - which is
	// what keeps "what the player is told" and "what the reels are doing" from ever
	// disagreeing.
	if(m_Ready == false)
	{
		sprintf(szLine, "Waiting for the machine...");
	}
	else if(m_SpinState == SPIN_ROLLING)
	{
		sprintf(szLine, "Spinning...");
	}
	else if(m_SpinState == SPIN_LANDING)
	{
		sprintf(szLine, "Stopping...");
	}
	else if(m_LastResultCode != 0)
	{
		// The refusal codes, in the server's SLOT_RESULT_* order.
		const char* reason = "The machine refused the spin.";

		switch(m_LastResultCode)
		{
			case 1: reason = "The machine is switched off."; break;
			case 2: reason = "Stand at the machine to play."; break;
			case 3: reason = "Slow down."; break;
			case 4: reason = "That stake is not accepted."; break;
			case 5: reason = "You do not have that item to stake."; break;
			case 6: reason = "Reward box is full - claim a prize first."; break;
			case 7: reason = "Finish what you are doing first."; break;
			case 8: reason = "Not enough inventory space to claim."; break;
			case 9: reason = "No inventory space, and no bank slot for it."; break;
		}

		sprintf(szLine, "%s", reason);
	}
	else if(m_SpinState == SPIN_REVEAL && m_TotalPay > 0)
	{
		// The bet is included because the payout is a multiple of it - without it a
		// win of 60 on a bet of 3 looks like the paytable is wrong.
		sprintf(szLine, "WON %u  -  %d line%s, %d scatter, bet %d", m_TotalPay, m_WinLineCount, ((m_WinLineCount == 1) ? "" : "s"), m_ScatterCount, m_Bet);
	}
	else if(m_SpinState == SPIN_REVEAL)
	{
		sprintf(szLine, "No win.  %d scatter", m_ScatterCount);
	}
	else
	{
		sprintf(szLine, "Pick a stake and press SPIN.");
	}

	TextDraw((HFONT)g_hFontBold, (int)(m_Pos.x + 40), (int)(m_Pos.y + kStatusY), ((m_SpinState == SPIN_REVEAL && m_TotalPay > 0) ? kTxtGold : kTxtWhite), 0x0, 320, 0, 3, szLine);

	ResetSlotGLState();
}

bool CNewUISlotMachine::Render()
{
	EnableAlphaTest();
	glColor4f(1.f, 1.f, 1.f, 1.f);

	DrawFrame();

	DrawReels();

	DrawWinLines();

	DrawStatusText();

	DrawStakeRow();

	DrawBetRow();

	DrawRewardBox();

	// The two big actions, over the bar's recesses. SPIN is disabled-looking while
	// the reels turn; CLAIM ALL while there is nothing to claim.
	const bool busy = ((m_SpinState == SPIN_ROLLING || m_SpinState == SPIN_LANDING) ? true : false);

	DrawWideButton(kSpinX, kSpinY, kSpinW, kSpinH, ((busy != false || m_Ready == false) ? 2 : ButtonState(kSpinX, kSpinY, kSpinW, kSpinH)), "SPIN", ((busy != false) ? kTxtDim : kTxtWhite));

	DrawWideButton(kClaimX, kClaimY, kClaimW, kClaimH, ((m_ClaimCount <= 0) ? 2 : ButtonState(kClaimX, kClaimY, kClaimW, kClaimH)), "CLAIM ALL", ((m_ClaimCount <= 0) ? kTxtDim : kTxtWhite));

	DisableAlphaBlend();

	return true;
}
