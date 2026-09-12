

#include "stdafx.h"
#include "VongQuay.h"
#include "Util.h"
#include "Interface.h"
#include "CBInterface.h"
#include "Other.h"
#include "CBNewUiEx.h"
#include <cmath>
#include "NewUISystem.h"

CVongQuay gVongQuay;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CVongQuay::CVongQuay()
{
	this->Init();
}


CVongQuay::~CVongQuay()
{
}

void CVongQuay::Init()
{
	this->ListItemVongQuay.clear();
	this->DanhSachVongQuay.clear();
}


void CVongQuay::OpenVongQuay()
{
	if ((GetTickCount() - gInterface.Data[eWindowVongQuay].EventTick) > 300)
	{
		gInterface.Data[eWindowVongQuay].EventTick = GetTickCount();

		if (gInterface.Data[eWindowVongQuay].OnShow)
		{
			gInterface.Data[eWindowVongQuay].OnShow = 0;

			return;
		}

		XULY_CGPACKET pMsg;
		pMsg.header.set(0xD3, 0x8C, sizeof(pMsg));
		pMsg.ThaoTac = 1;
		DataSend((LPBYTE)&pMsg, pMsg.header.size);

		pMsg.header.set(0xD3, 0x8B, sizeof(pMsg));
		pMsg.ThaoTac = 1; //
		DataSend((LPBYTE)&pMsg, pMsg.header.size);
		
	}
}

CNewUIScrollBar* ListVongQuay = nullptr;

int SelectTypeVQ = 1;
int Chay = -1;
int PageQuay = 0;
// quay10lan() (the multi-spin repeat sender) and the spin-count input box are
// gone - the wheel is single-spin only. That sender was also malformed: it sent
// the 8-byte XULY_CGPACKET while the server casts 0xD3/0x8A to the 12-byte
// XULY_CGPACKET_SOLAN, so the spin count came from 4 bytes past the payload and
// repeat-spin timing was decided by whatever was left in the receive buffer.
bool UpdateMaxPosSVQ= false;

// ---------------------------------------------------------------------------
// Wheel pointer
//
// Replaces the old marquee, which walked an int index between the 12 boxes and
// tinted whichever one it landed on. That had no arrow, stepped at a rate that
// depended on frame rate and a GetTickCount parity bucket, and its CountVong
// "speed" was dead code (int += float truncates to +1), so it could neither
// accelerate nor decelerate.
//
// Now: a real angle in degrees, driven by elapsed milliseconds, that free-spins
// while the server is deciding and then eases onto the winning segment.
// ---------------------------------------------------------------------------
namespace
{
	const float kPointerSpinDegPerSec = 900.0f;	// free-spin speed
	// The server sends the winning slot in the spin-START packet, so the whole
	// window until the prize is handed out can be spent decelerating onto it.
	//
	// Sizing matters, because the golden "received X from Spin Wheel" notice is
	// sent by the SERVER at grant time and the client cannot hold it back. The
	// grant fires on the first per-second tick after 5000ms, i.e. somewhere in
	// (5000, 6000] ms after the spin. Landing at 4500ms leaves roughly half a
	// second of margin even after the spin-start packet's own network latency, so
	// the pointer always comes to rest BEFORE the message appears.
	const float kPointerLandMsSingle = 4500.0f;
	// Extra whole turns folded into the landing, so most of the eased travel is
	// spent decelerating rather than covering the gap to the target.
	const int kPointerLandTurns = 4;
	// arrow.ozt is 150x150. OpenTga rounds up to the next power of two (256) and
	// does NOT clear the padding - the buffer is a plain new BYTE[] and only the
	// first 150 rows/columns are written - so drawing with u,v = 0..1 samples
	// uninitialised heap for ~66% of the quad.
	const float kArrowUV = 150.0f / 256.0f;

	float g_PointerAngleDeg = 0.0f;		// current angle, degrees, CCW positive
	int   g_PointerTarget = -1;			// segment being landed on, -1 = free
	DWORD g_PointerLandStart = 0;
	float g_PointerLandFrom = 0.0f;
	float g_PointerLandTo = 0.0f;
	DWORD g_PointerLastMs = 0;

	bool g_PointerSettled = false;

	// Reveal state. The prize is NOT drawn off the packet - the server sends the
	// winning slot at spin start so the wheel can aim at it, so reading that
	// directly would show the answer the moment the wheel began turning. It is
	// published by exactly one event, fired once when the pointer comes to rest.
	bool g_RewardRevealed = false;
	int  g_RewardSlot = -1;

	// ---- Deferred claim-list application --------------------------------
	// The Rewards panel must show prizes the player is already holding the moment
	// the window opens, but it must NOT show the prize from a spin that is still
	// turning. Those two pull in opposite directions, because the server pushes
	// the updated claim list at grant time - about 4.5s before the pointer
	// finishes its landing animation.
	//
	// So the display is no longer gated on the pointer being settled (that made
	// held prizes invisible until the player spun again). Instead a list that
	// arrives mid-spin is parked here and swapped in by OnWheelStopped, which
	// keeps the reveal on the single stop event where it belongs.
	bool g_SpinInFlight = false;	// Spin pressed, pointer not yet settled.

	// Tracks the window's open/closed edge so the held-prize list is freed once on
	// close instead of on every closed frame. See the closed branch of
	// DrawWindowVQ for why the difference matters.
	bool g_WheelWindowWasOpen = false;

	std::vector<CVongQuay::INFO_SPINCLAIM_LOCAL> g_PendingClaim;
	int  g_PendingClaimFree = 0;
	bool g_HasPendingClaim = false;

	// CreateItem hands out refcounted ITEMs, so a bare clear() leaks every row.
	void FreeClaimVector(std::vector<CVongQuay::INFO_SPINCLAIM_LOCAL>& rows)
	{
		for (unsigned int n = 0; n < rows.size(); n++)
		{
			if (rows[n].Item)
			{
				g_pNewItemMng->DeleteItem(rows[n].Item);
				rows[n].Item = NULL;
			}
		}

		rows.clear();
	}

	void DropPendingClaim()
	{
		FreeClaimVector(g_PendingClaim);
		g_HasPendingClaim = false;
	}

	void ApplyPendingClaim()
	{
		if (!g_HasPendingClaim)
		{
			return;
		}

		FreeClaimVector(gVongQuay.ListClaim);
		gVongQuay.ListClaim.swap(g_PendingClaim);
		gVongQuay.ClaimFree = g_PendingClaimFree;

		g_PendingClaim.clear();
		g_HasPendingClaim = false;
	}

	// Angle to pass RenderBitmapRotate so the arrow points at segment n.
	//
	// The art points RIGHT at rest, so no extra quarter-turn is needed. Two
	// conventions have to be composed:
	//  - the segments are placed at (cos, +sin) in y-DOWN screen space, i.e.
	//    bearing measured CLOCKWISE from +x, while RenderBitmapRotate's positive
	//    angle is COUNTER-CLOCKWISE - hence the negation.
	//  - the boxes are positioned in logical 640x480 units that the 2D path
	//    scales by g_fScreenRate_x horizontally and g_fScreenRate_y vertically,
	//    INDEPENDENTLY. On a 2400x1080 phone that is 3.75 vs 2.25, so the ring
	//    is an ellipse in pixels, not a circle, and aiming at the logical
	//    bearing misses the segment by up to a third of a segment width.
	//    Aim at the real pixel bearing instead.
	float SegmentPointerAngleDeg(int segment, int segmentCount)
	{
		const float kPi = 3.14159265358979323846f;
		const float phi = (2.0f * kPi * (float)segment) / (float)segmentCount;
		const float psi = atan2f(g_fScreenRate_y * sinf(phi), g_fScreenRate_x * cosf(phi));
		return -(psi * 180.0f / kPi);
	}

	// Which segment the arrow is currently over - drives the tinted highlight
	// during the free spin so the two can never disagree.
	int PointerSegment(int segmentCount)
	{
		int best = 0;
		float bestDiff = 1.0e9f;

		for (int n = 0; n < segmentCount; n++)
		{
			float d = g_PointerAngleDeg - SegmentPointerAngleDeg(n, segmentCount);

			while (d > 180.0f) d -= 360.0f;
			while (d < -180.0f) d += 360.0f;

			if (fabsf(d) < bestDiff)
			{
				bestDiff = fabsf(d);
				best = n;
			}
		}

		return best;
	}

	void ResetWheelPointer()
	{
		g_PointerAngleDeg = 0.0f;
		g_PointerTarget = -1;
		g_PointerLandStart = 0;
		g_PointerLastMs = 0;
		g_PointerSettled = false;
		g_RewardRevealed = false;
		g_RewardSlot = -1;

		g_SpinInFlight = false;
		DropPendingClaim();
	}

	// Fires ONCE, at the instant the wheel stops. Everything the player is
	// allowed to learn about the prize hangs off this - nothing reads the
	// winning slot out of the packet directly.
	void OnWheelStopped(int slot)
	{
		g_RewardRevealed = true;
		g_RewardSlot = slot;

		// The won prize becomes visible in the Rewards panel here and nowhere
		// else - the packet that carried it landed seconds ago.
		g_SpinInFlight = false;
		ApplyPendingClaim();

		PlayBuffer(25, 0, 0);
	}

	bool WheelPointerSettled()
	{
		return g_PointerSettled;
	}

	int RevealedRewardSlot()
	{
		return (g_RewardRevealed ? g_RewardSlot : -1);
	}

	void UpdateWheelPointer(int segmentCount, int startRoll, int winIndex)
	{
		const DWORD nowMs = GetTickCount();

		DWORD dt = ((g_PointerLastMs != 0) ? (nowMs - g_PointerLastMs) : 16);

		// Guard the first frame and any hitch (alt-tab, load spike) so the
		// pointer never teleports a full turn on one frame.
		if (dt > 500) dt = 16;

		g_PointerLastMs = nowMs;

		const bool canLand = (winIndex >= 0 && winIndex < segmentCount);

		if (canLand)
		{
			const float landMs = kPointerLandMsSingle;

			if (g_PointerTarget != winIndex)
			{
				g_PointerTarget = winIndex;
				g_PointerLandStart = nowMs;
				g_PointerLandFrom = g_PointerAngleDeg;
				g_PointerSettled = false;

				float to = SegmentPointerAngleDeg(winIndex, segmentCount);

				// Keep travelling the way the free spin was going (clockwise on
				// screen = decreasing angle), plus whole extra turns so the eased
				// curve has real distance to decelerate over.
				while (to >= g_PointerLandFrom) to -= 360.0f;
				to -= 360.0f * (float)kPointerLandTurns;

				g_PointerLandTo = to;
			}

			float t = (float)(nowMs - g_PointerLandStart) / landMs;

			if (t >= 1.0f)
			{
				t = 1.0f;

				if (!g_PointerSettled)
				{
					g_PointerSettled = true;
					OnWheelStopped(winIndex);
				}
			}

			// Ease-out quart. Covers ~68% of the travel in the first quarter of
			// the time and ~94% by halfway, so the last stretch into the winning
			// segment is a visible crawl rather than an abrupt stop.
			const float inv = 1.0f - t;
			const float e = 1.0f - (inv * inv * inv * inv);

			g_PointerAngleDeg = g_PointerLandFrom + ((g_PointerLandTo - g_PointerLandFrom) * e);
			return;
		}

		g_PointerTarget = -1;

		// No target yet - the server has not answered the spin request.
		if (startRoll >= 1)
		{
			g_PointerSettled = false;

			g_PointerAngleDeg -= kPointerSpinDegPerSec * ((float)dt / 1000.0f);

			while (g_PointerAngleDeg <= -360.0f) g_PointerAngleDeg += 360.0f;
		}
	}

	// Called when Spin is pressed: clear the previous result so the old prize
	// stops being displayed the moment a new spin starts.
	void BeginWheelSpin()
	{
		g_PointerTarget = -1;
		g_PointerSettled = false;
		g_RewardRevealed = false;
		g_RewardSlot = -1;

		// Set here rather than keying off StartRollSau/g_PointerTarget: this has to
		// cover the whole window from the click to the pointer settling, including
		// the gap before the server's answer arrives, and it must not depend on
		// which of the server's two replies (spin result vs claim list) lands first.
		g_SpinInFlight = true;
	}
}

void CVongQuay::DrawWindowVQ()
{
	if (gInterface.CheckWindow(Interface::ObjWindow::MoveList) || gInterface.CheckWindow(Interface::ObjWindow::CashShop) || gInterface.CheckWindow(Interface::ObjWindow::SkillTree) || gInterface.CheckWindow(Interface::ObjWindow::FullMap)
		|| (gInterface.CheckWindow(Interface::Inventory)
			&& gInterface.CheckWindow(Interface::ExpandInventory)
			&& gInterface.CheckWindow(Interface::Store))
		|| (gInterface.CheckWindow(Interface::Inventory)
			&& gInterface.CheckWindow(Interface::Warehouse)
			&& gInterface.CheckWindow(Interface::ExpandWarehouse))
		|| gInterface.CheckWindow(Interface::ChaosBox)
		)
	{
		gInterface.Data[eWindowVongQuay].OnShow = 0;

		return;
	}
	if (!gInterface.Data[eWindowVongQuay].OnShow)
	{
		if (ListVongQuay) ListVongQuay = nullptr;
		if (SelectTypeVQ != 1) SelectTypeVQ = 1;
		gVongQuay.StartRollSau = -1;
		gVongQuay.IndexItemSau = -1;
		ResetWheelPointer();

		// DeleteItem before clearing - CreateItem hands out refcounted ITEMs and
		// a bare clear() leaks all 12 every time the window closes or the player
		// switches wheel.
		for (unsigned int n = 0; n < this->ListItemVongQuay.size(); n++)
		{
			if (this->ListItemVongQuay[n].Item)
			{
				g_pNewItemMng->DeleteItem(this->ListItemVongQuay[n].Item);
				this->ListItemVongQuay[n].Item = NULL;
			}
		}

		this->ListItemVongQuay.clear();

		// Clear the held-prize list ONCE, on the open->closed transition, never on
		// every closed frame.
		//
		// This branch runs every frame while the window is shut, and OpenVongQuay
		// sends 0xD3 0x8C (which the server answers with the claim list) BEFORE
		// anything sets OnShow true. A per-frame clear here therefore destroyed
		// that reply the same frame it arrived, every single time - so a prize won
		// before closing the window looked gone, and only reappeared after a fresh
		// spin pushed the list again. That is the "can't claim my previous reward
		// without spinning" bug.
		if (g_WheelWindowWasOpen)
		{
			this->ClearClaimList();
			g_WheelWindowWasOpen = false;
		}

		return;
	}

	g_WheelWindowWasOpen = true;
	int MaxListVQInPage = 5;
	int MaxListItemVQInPage = 9;
	// 480, was 380: the Rewards panel needs a column of its own. Drawing it
	// outside the frame would overlap the game world and would not hit-test
	// reliably. Only this literal changes - the
	// gInterface.Data[eWindowVongQuay].Width assignment just below reads it.
	float WindowW = 480;
	float WindowH = 310;
	gInterface.Data[eWindowVongQuay].Width = WindowW;
	float StartX = (MAX_WIN_WIDTH / 2) - (WindowW / 2);
	float StartY = ((MAX_WIN_HEIGHT - 51) / 2) - (WindowH / 2);
	if (g_pBCustomMenuInfo->gDrawWindowCustom( &StartX, &StartY, WindowW, WindowH, eWindowVongQuay, "New Spin Wheel"))
	{

		EnableAlphaBlend();
		glColor3f(1.0, 1.0, 1.0);
		//===Info Yeu Cau Moc Nap
		float InfoMocNapX = (StartX + 10) + 3;
		float InfoMocNapY = (StartY + 15);
		float TyleInfoYeuCau = 7.5f;
		float WInfo = (WindowW - 20) / 10;
		float WProcess = (WInfo * (TyleInfoYeuCau - 2.7));
		float HInfo = WindowH - 160;

		// ---- Right column ------------------------------------------------
		// Rewards, the wheel list and Requirement share ONE 130px strip, stacked,
		// and all three read their X from ColX and their Y from the block
		// constants below.
		//
		// They each used to hardcode their own (StartX + WindowW) - N offset, and
		// that is exactly how the Rewards panel ended up drawn on top of the
		// wheel-name buttons: widening the window from 380 to 480 slid every
		// right-anchored block rightwards along with the frame, so the new panel
		// landed in the lane the old ones still occupied instead of beside them.
		// Anything added to this column from here on goes through these
		// constants, so a width change moves the whole strip as one piece.
		//
		// The wheel is anchored to StartX and reaches StartX + 255 (centre
		// StartX + 125, radius 100, 30px boxes), so the strip clears it.
		const float ColX = (StartX + WindowW) - 140;
		const float ColW = 130;
		const float WButton = ColW - 16;		// mSizeButtonW for the wheel buttons

		// Y budget. Nothing may start above StartY + 30: gDrawWindowCustom draws
		// the title caption at StartY + 10 and the close button is 29px tall from
		// StartY, so the title bar owns StartY + 0..29. Rewards used to start at
		// StartY + 15, which put its header on the caption line next to the close
		// button rather than inside the window.
		const float RewBlockY = StartY + 32;	// Rewards     -  65px (32..97)
		const float ListBlockY = StartY + 102;	// wheel list  - 120px (102..222)
		const float ReqBlockY = StartY + 226;	// Requirement -  74px (226..300)

		g_pBCustomMenuInfo->DrawInfoBox(ColX, ListBlockY, ColW, 120, 0x00000096, 0, 0);

		g_pBCustomMenuInfo->DrawInfoBox(ColX, ReqBlockY, ColW, 74, 0x00000096, 0, 0);


		//Scroll Bar

		int DataListVQ = gVongQuay.DanhSachVongQuay.size();
		

		//if (ListVongQuay == NULL)
		//{
		//	ListVongQuay = new CNewUIScrollBar();
		//	ListVongQuay->Create((StartX + WindowW) - 15, InfoMocNapY + 10, HInfo);
		//	//ListVongQuay->SetMaxPos(DataListVQ > 3 ? (DataListVQ / MaxListVQInPage) : 0);
		//	ListVongQuay->SetPos((StartX + WindowW) - 15, InfoMocNapY + 10);
		//
		//}
		//else
		//{
		//	if (UpdateMaxPosSVQ)
		//	{
		//		ListVongQuay->SetMaxPos(DataListVQ > 3 ? (DataListVQ / MaxListVQInPage) : 0);
		//		UpdateMaxPosSVQ = false;
		//	}
		//	if (gInterface.Data[eWindowVongQuay].OnClick)
		//	{
		//		ListVongQuay->SetPos((StartX + WindowW) - 15, InfoMocNapY + 10);
		//		ListVongQuay->SetCurPos(0);
		//	}
		//	ListVongQuay->MouseWheelWindow = SEASON3B::CheckMouseIn(InfoMocNapX, InfoMocNapY, WindowW, WindowH);
		//	ListVongQuay->Render();
		//	ListVongQuay->UpdateMouseEvent();
		//	ListVongQuay->Update();
		//}
		int sotrang = DataListVQ / MaxListVQInPage;

		if (DataListVQ % MaxListVQInPage > 0)
		{
			sotrang += 1;
		}
		if (SEASON3B::CheckMouseIn(InfoMocNapX, InfoMocNapY, WindowW, WindowH))
		{
			if (MouseWheel != 0)
			{
				if ((MouseWheel < 0))
					if (PageQuay < sotrang - 1)
					{
						PageQuay++;
					}
				if ((MouseWheel > 0))
					if (PageQuay > 0)
					{
						PageQuay--;
					};
				MouseWheel = 0;
			}
		}

		float KhoangCachYMocNap = 23;
		//int MixItemListPage = ListVongQuay->GetCurPos();
		int MaxList = 0;

		// Walks its own cursor rather than mutating InfoMocNapY - that variable is
		// the origin of the window-wide mouse-wheel hit-test just above, and the
		// loop used to leave it pointing at wherever the last button landed.
		float WheelListY = ListBlockY + 5;

		for (int n = (PageQuay * MaxListVQInPage); n < DataListVQ; n++)
		{
			//==Xem
			if (g_pBCustomMenuInfo->DrawButton(ColX + 8, WheelListY, 110, 11, gVongQuay.DanhSachVongQuay[n].Name, WButton) && (GetTickCount() - gInterface.Data[eWindowVongQuay].EventTick) > 300 && gVongQuay.StartRollSau < 1) //"Xem"
			{

				SelectTypeVQ = gVongQuay.DanhSachVongQuay[n].IndexVongQuay;
				Chay = -1;
				gInterface.Data[eWindowVongQuay].OnShow = true;
				gInterface.Data[eWindowVongQuay].EventTick = GetTickCount();
				XULY_CGPACKET pMsg;
				pMsg.header.set(0xD3, 0x8B, sizeof(pMsg));
				pMsg.ThaoTac = SelectTypeVQ; //
				DataSend((LPBYTE)&pMsg, pMsg.header.size);
				gVongQuay.IndexItemSau = -1;
			}
			
			WheelListY = WheelListY + (KhoangCachYMocNap);
			MaxList++;
		
			if (MaxList >= MaxListVQInPage) break;
		}

		if ((GetTickCount() - gInterface.Data[eTickCount].EventTick) > 6000)
		{

			if (g_pBCustomMenuInfo->DrawButton(StartX + 115, StartY + 200, 100, 11, "Spin", 60) && (GetTickCount() - gInterface.Data[eTickCount].EventTick) > 6000) //"Nhận"
			{
				// Always exactly one spin. SoLan must be 1: that is the only value
				// the server answers with its 5-second deferred draw, which is the
				// window the wheel animation lives in. Any other value took the
				// immediate-grant path, so the prize landed in the inventory (and
				// the "you won" notice went out) while the arrow was still turning
				// - which is what gave the result away early.
				XULY_CGPACKET_SOLAN pMsg;
				pMsg.header.set(0xD3, 0x8A, sizeof(pMsg));
				pMsg.ThaoTac = SelectTypeVQ;
				pMsg.SoLan = 1;
				DataSend((LPBYTE)&pMsg, pMsg.header.size);

				BeginWheelSpin();

				gInterface.Data[eTickCount].EventTick = GetTickCount();
			}
		}


		// The spin-count input box and its repeat loop are gone - the wheel is
		// single-spin only now. They were also the actual cause of the reward
		// showing before the arrow stopped: the box was created once with "1" and
		// never reset, so once a player had typed 10 it stayed 10, and every
		// subsequent spin silently took the server's immediate-grant path.

		// Bottom of the shared right strip. The left-aligned rows keep passing
		// WindowW as their text width on purpose: it is only a wrap limit, and a
		// value narrower than the longest formatted amount would start folding
		// "+WCoinP :1,000,000,000" onto a second line.
		float PosYCoinNhan = ReqBlockY + 7;
		TextDraw(g_hFont, ColX + 5, PosYCoinNhan - 5, 0xFF26DEFF, 0x3a4b3978, ColW - 10, 0, 3, "Requirement"); //);
		TextDraw(g_hFont, ColX + 5, PosYCoinNhan + (10 * 1), 0xFF1482FF, 0x0, WindowW, 0, 1, "+WCoin : %s", gInterface.NumberFormat(gVongQuay.WCYC));			//Text3 = "+ WCoin : %s
		TextDraw(g_hFont, ColX + 5, PosYCoinNhan + (10 * 2), 0xFF1482FF, 0x0, WindowW, 0, 1, "+WCoinP :%s", gInterface.NumberFormat(gVongQuay.WPYC));		  //Text4 = "+ WCoinP : %
		TextDraw(g_hFont, ColX + 5, PosYCoinNhan + (10 * 3), 0xFF1482FF, 0x0, WindowW, 0, 1, "+GobinP :%s", gInterface.NumberFormat(gVongQuay.GPYC));		  //Text5 = "+ GobinP : %
		if (gVongQuay.CountItem > 0)
		{
			TextDraw(g_hFont, ColX + 5, PosYCoinNhan + (10 * 4), 0xFF1482FF, 0x0, WindowW, 0, 1, "+Need : %s", gInterface.NumberFormat(gVongQuay.CountItem));		  //Text6 = " + Ruud : % s"
			TextDraw(g_hFont, ColX + 5, PosYCoinNhan + (10 * 5) + 5, 0xFFD7FF26, 0x0, WindowW, 0, 1, "%s", BGetItemName(gVongQuay.IndexYC, 0));//
		}
		// (Removed "Enter Number of Spins" - the wheel is single-spin, so there is
		// nothing left for the player to enter.)


		int DataListItem = gVongQuay.ListItemVongQuay.size();
		float Radius = 100.0f; // Set the radius of the circular layout
		// Derived from the ACTUAL prize count, not a hardcoded 12. The server
		// caps a wheel at 12 prizes and silently drops the rest, but a wheel
		// configured with fewer than 12 used to leave the pointer aiming into
		// empty space between the last segment and the first.
		int SegmentCount = ((DataListItem > 0) ? DataListItem : 1);
		float AngleStep = 2 * 3.14159265358979323846 / (float)SegmentCount;
		float WBox = 30;
		int BBShowInfoItem = -1;
		DWORD boxColor = 0x00000096;

		// Wheel hub - the centre the segments are laid out around, and the point
		// the pointer pivots about.
		float HubX = StartX + 125 + (WBox / 2);
		float HubY = StartY + 140 + (WBox / 2);

		UpdateWheelPointer(SegmentCount, gVongQuay.StartRollSau, gVongQuay.IndexItemSau);

		// Tracks the pointer while it turns, then pins to the revealed slot. Reads
		// the REVEALED value, never the packet, so it cannot leak the result early.
		Chay = ((RevealedRewardSlot() >= 0) ? RevealedRewardSlot() : PointerSegment(SegmentCount));
		for (int n = 0; n < DataListItem; n++)
		{
			const ITEM_ATTRIBUTE* is = &ItemAttribute[gVongQuay.ListItemVongQuay[n].Index];
		
			int size = max(is->Width, is->Height);

			float itemScale = 1.0;
			float addY = 0;
			switch (size)
			{
			case 4:
				addY = -5;
				break;
			case 3:
				addY = -3;
				break;
			case 2:
				addY = -1;
				break;
			}

			// Calculate polar coordinates for circular layout
			float angle = n * AngleStep;
			float PosXBoxItem = StartX + 125 + Radius * cos(angle);
			float PosYBoxItem = StartY + 140 + Radius * sin(angle);

			if (n == Chay)  // Replace yourNewVariable with the variable you want to compare with
			{
				boxColor = 0xD4966396;
			}
			else
			{
				boxColor = 0x00000096;
			}
			// (Removed: a rand()-per-frame flicker that only ran during a multi-spin
			// batch. Multi-spin is gone, and it fought the pointer for the player's
			// attention anyway.)


			g_pBCustomMenuInfo->DrawInfoBox(PosXBoxItem, PosYBoxItem, WBox, WBox, boxColor, 0, 0);
			g_pNewUISystem->RenderItem3DFree(PosXBoxItem + gVongQuay.ListItemVongQuay[n].PosX, PosYBoxItem + gVongQuay.ListItemVongQuay[n].PosY, WBox, WBox, gVongQuay.ListItemVongQuay[n].Index, gVongQuay.ListItemVongQuay[n].Item->Level, gVongQuay.ListItemVongQuay[n].Item->Option1, gVongQuay.ListItemVongQuay[n].Item->ExtOption, 0, 1.0 / gVongQuay.ListItemVongQuay[n].SizeBMD);

			if (SEASON3B::CheckMouseIn(PosXBoxItem, PosYBoxItem, WBox, WBox))
			{
				BBShowInfoItem = n;
			}
		}

		// The rotating pointer, drawn AFTER the segment loop so it composites
		// over the item icons rather than under them.
		//
		// Height is scaled by the x/y screen-rate ratio so the quad comes out
		// SQUARE IN PIXELS. RenderBitmapRotate applies ConvertX to Width and
		// ConvertY to Height before it rotates, so without this the arrow would
		// be stretched and would visibly shear as it spun.
		{
			const float ArrowW = 92.0f;
			const float ArrowH = ArrowW * (g_fScreenRate_x / g_fScreenRate_y);

			// DisableTexture() first, then EnableAlphaTest(). This looks redundant
			// and is not.
			//
			// RenderBitmapRotate calls BindTexture but never enables texturing
			// itself - it relies on the caller's state. EnableAlphaTest does enable
			// it, but only behind a shadow-cache guard (if(!TextureEnable)), and the
			// item renders and coloured boxes drawn just above here issue raw
			// glDisable(GL_TEXTURE_2D) without updating that shadow. So the cache
			// says "texture on" while GL has it off, EnableAlphaTest no-ops, and the
			// arrow draws UNTEXTURED - a solid white quad, since the art's
			// transparent region is white with alpha 0 and glColor is white.
			//
			// DisableTexture(false) drives shadow and GL back into agreement, so the
			// EnableAlphaTest below performs a real glEnable.
			DisableTexture(false);
			::EnableAlphaTest();
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

			RenderBitmapRotate(BITMAP_SPINWHEEL_ARROW, HubX, HubY, ArrowW, ArrowH,
				g_PointerAngleDeg, 0.0f, 0.0f, kArrowUV, kArrowUV);
		}

		// ---- Rewards panel ------------------------------------------------
		// Prizes are never placed in the inventory by the wheel - they are held
		// server-side and claimed here, one at a time, newest first.
		//
		// Only drawn once the pointer has actually stopped, so the wheel cannot
		// give the result away mid-spin (RevealedRewardSlot is published by the
		// single OnWheelStopped event).
		ITEM* pClaimTooltip = NULL;
		{
			// Top block of the shared right strip - see the ColX/*BlockY constants
			// at the head of this function. Everything here is laid out inside 65
			// vertical pixels so the wheel list below stays visible.
			const float RewX = ColX;
			const float RewY = RewBlockY;
			const float RewW = ColW;

			// Full-box warning rides on the header instead of taking a line of its
			// own below the panel - the spin is refused server-side when the box is
			// full, so the player has to be told before they pay, but there is no
			// spare vertical room in the strip for a separate warning row.
			const bool boxFull = (gVongQuay.ClaimFree <= 0);

			TextDraw(g_hFont, RewX, RewY, (boxFull ? 0xFF1482FF : 0xFFD7FF26), 0x0, RewW, 0, 3,
				"%s", (boxFull ? "Rewards - FULL" : "Rewards"));

			// No WheelPointerSettled() term here any more. Gating the panel on the
			// pointer meant a prize the player was already holding stayed invisible
			// after reopening the window (ResetWheelPointer clears the settled flag),
			// so it could not be claimed without spinning again. Suspense is now
			// handled upstream, by parking a mid-spin list until OnWheelStopped.
			const bool haveClaim = (gVongQuay.ListClaim.size() > 0);

			g_pBCustomMenuInfo->DrawInfoBox(RewX, RewY + 13, RewW, 52, 0x00000096, 0, 0);

			if (haveClaim)
			{
				// Newest first - the prize the player just won is the one they want
				// to see, and it is the last row the server sent.
				const CVongQuay::INFO_SPINCLAIM_LOCAL& held = gVongQuay.ListClaim[gVongQuay.ListClaim.size() - 1];

				// Icon left, count and Claim button stacked to its right. Side by
				// side rather than one under the other: stacked, the block needed
				// ~132px and pushed the wheel list off the bottom of the window.
				const float IconX = RewX + 6;
				const float IconY = RewY + 19;

				g_pBCustomMenuInfo->DrawInfoBox(IconX, IconY, 40, 40, 0xD4966396, 0, 0);
				g_pNewUISystem->RenderItem3DFree(IconX + 4, IconY + 2, 32, 32, held.Index,
					held.Item->Level, held.Item->Option1, held.Item->ExtOption, 0, 1.4f, false);

				if (SEASON3B::CheckMouseIn(IconX, IconY, 40, 40))
				{
					pClaimTooltip = held.Item;
				}

				const float BtnX = IconX + 50;
				const float BtnW = RewW - (BtnX - RewX) - 6;

				// "N more" - the box holds up to 8, but only one is shown at a time.
				// Sits in the upper right, above the button.
				if (gVongQuay.ListClaim.size() > 1)
				{
					TextDraw(g_hFont, BtnX, RewY + 22, 0xFF1482FF, 0x0, BtnW, 0, 3,
						"+%d more", (int)gVongQuay.ListClaim.size() - 1);
				}

				// Bottom-right of the panel, not floating in the middle of it.
				// Panel body runs RewY + 13 .. RewY + 65, so this leaves a 3px margin
				// under the button.
				const float BtnY = RewY + 44;
				const float BtnH = 18;

				const bool over = (SEASON3B::CheckMouseIn(BtnX, BtnY, BtnW, BtnH) != 0);

				// Same art and hover ramp as CNewUIBCustomMenuInfo::DrawButton, so
				// Claim reads as a button like Spin and the wheel names do instead of
				// a flat coloured rectangle - but with our OWN click handling, because
				// DrawButton shares one global 500ms debounce (BButtonClickTime) with
				// every other button on screen and Claim and Spin would then eat each
				// other's clicks.
				//
				// DisableTexture(false) first for the same reason the pointer above
				// needs it: RenderItem3DFree ran a few lines up and issues raw
				// glDisable(GL_TEXTURE_2D) without updating the TextureEnable shadow,
				// so a bare EnableAlphaTest() no-ops and this would draw as an
				// untextured white quad.
				DisableTexture(false);
				::EnableAlphaTest();
				glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
				RenderBitmap(SEASON3B::CNewUIBCustomMenuInfo::IMAGE_BCUSTOM_WINDOW_31326,
					BtnX, BtnY, BtnW, BtnH, 0.0f, (over ? 0.227f : 0.0f), 0.830f, 0.227f, 1, 1, 0.0f);
				TextDraw(g_hFontBold, BtnX, BtnY + (BtnH / 2) - 4.0f, 0xE6FCF7FF, 0x0, BtnW, 0, 3, "Claim");

				if (over && SEASON3B::IsRelease(VK_LBUTTON))
				{
					gVongQuay.SendClaim(held.slot);
					PlayBuffer(25, 0, 0);
				}
			}
			else
			{
				TextDraw(g_hFont, RewX, RewY + 33, 0xFFB5B5B5, 0x0, RewW, 0, 3, "No rewards waiting");
			}
		}

		if (BBShowInfoItem != -1)
		{
			RenderItemInfo(MouseX + 75, MouseY, this->ListItemVongQuay[BBShowInfoItem].Item, 0, 0, false, false);
		}
		else if (pClaimTooltip != NULL)
		{
			RenderItemInfo(MouseX + 75, MouseY, pClaimTooltip, 0, 0, false, false);
		}
		float CenterX = StartX + (WindowW / 3)+20;
		float CenterY = (StartY + WindowH) -5;
		const BYTE state[3] = { 0, 1, 2 };
		::EnableAlphaTest();
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		RenderBitmap(32344, CenterX - (60 / 2), CenterY - 22, 60.f, 22.f, 0, 0, 80.f / 128.f, 30.f / 34.f, 1, 1, 0.0);
		TextDraw(g_hFont, CenterX - (60 / 2), CenterY - 22 + 5, 0xffffffff, 0x0, 60, 0, 3, "%d / %d", PageQuay + 1, sotrang);

		DisableAlphaBlend();
	}
}

// ---- Rewards box -----------------------------------------------------------

void CVongQuay::ClearClaimList()
{
	// DeleteItem every ITEM before dropping the vector. CreateItem hands out
	// refcounted objects; the wheel's own prize list leaks them by calling
	// clear() alone, and this list is rebuilt on every claim so it would leak far
	// faster. Any list parked for a spin in progress goes with it.
	FreeClaimVector(gVongQuay.ListClaim);
	DropPendingClaim();
}

void CVongQuay::RecvClaimList(BYTE* Recv)
{
	if (!Recv) return;

	PMSG_SPINCLAIM_LIST_RECV* mRecv = (PMSG_SPINCLAIM_LIST_RECV*)Recv;

	// Built into a local list first, then either applied now or parked until the
	// pointer stops. Parsing straight into gVongQuay.ListClaim would reveal a
	// mid-spin prize the instant the packet arrived.
	std::vector<INFO_SPINCLAIM_LOCAL> rows_out;

	// Validate the declared row count against the real payload before walking it
	// - count comes off the wire and a short packet would otherwise be read past
	// its end.
	const int declared = mRecv->count;
	const int payload = (mRecv->header.size[0] * 256) + mRecv->header.size[1];
	const int usable = (payload - (int)sizeof(PMSG_SPINCLAIM_LIST_RECV)) / (int)sizeof(SPINCLAIM_ROW);
	const int rows = ((declared < usable) ? declared : usable);

	for (int i = 0; i < rows; i++)
	{
		SPINCLAIM_ROW lpRow = *(SPINCLAIM_ROW*)(((BYTE*)Recv) + sizeof(PMSG_SPINCLAIM_LIST_RECV) + (sizeof(SPINCLAIM_ROW) * i));

		INFO_SPINCLAIM_LOCAL info = { 0 };

		info.slot = lpRow.slot;
		info.Item = g_pNewItemMng->CreateItem(lpRow.Item);

		if (!info.Item)
		{
			continue;
		}

		info.Index = info.Item->Type;

		rows_out.push_back(info);
	}

	if (g_SpinInFlight)
	{
		// Mid-spin: hold it back so the panel keeps showing the pre-spin contents.
		// OnWheelStopped swaps this in the instant the pointer comes to rest.
		FreeClaimVector(g_PendingClaim);
		g_PendingClaim.swap(rows_out);
		g_PendingClaimFree = mRecv->free;
		g_HasPendingClaim = true;
		return;
	}

	FreeClaimVector(gVongQuay.ListClaim);
	gVongQuay.ListClaim.swap(rows_out);
	gVongQuay.ClaimFree = mRecv->free;
}

void CVongQuay::SendClaim(int slot)
{
	PMSG_SPINCLAIM_CLAIM_SEND pMsg;
	// header.set (C1), not setE - HackPacketCheck's entry for head 0xD3 pins
	// Encrypt to 0, and a C3/C4 packet on this head disconnects the player.
	pMsg.header.set(0xD3, 0x8E, sizeof(pMsg));
	pMsg.slot = (BYTE)slot;
	DataSend((LPBYTE)&pMsg, pMsg.header.size);
}

void CVongQuay::GetListVQ(BYTE* Recv)
{
	if (!Recv) return;

	gVongQuay.DanhSachVongQuay.clear();
	PMSG_VONGQUAY_SEND* mRecv = (PMSG_VONGQUAY_SEND*)Recv;

	for (int i = 0; i < mRecv->count; i++)
	{
		ListVongQuaySend lpInfo = *(ListVongQuaySend*)(((BYTE*)Recv) + sizeof(PMSG_VONGQUAY_SEND) + (sizeof(ListVongQuaySend) * i));
		gVongQuay.DanhSachVongQuay.push_back(lpInfo);
	}

}


void CVongQuay::RecvListItemVQ(BYTE* Recv)
{
	if (!Recv) return;

	// Same leak as the window-close path: this runs on every wheel switch.
	for (unsigned int n = 0; n < gVongQuay.ListItemVongQuay.size(); n++)
	{
		if (gVongQuay.ListItemVongQuay[n].Item)
		{
			g_pNewItemMng->DeleteItem(gVongQuay.ListItemVongQuay[n].Item);
			gVongQuay.ListItemVongQuay[n].Item = NULL;
		}
	}

	gVongQuay.ListItemVongQuay.clear();

	PMSG_YCVONGQUAY_SEND* mRecv = (PMSG_YCVONGQUAY_SEND*)Recv;
	gVongQuay.IndexYC = mRecv->IndexYC;
	gVongQuay.CountItem = mRecv->CountItem;
	gVongQuay.WCYC = mRecv->WCYC;
	gVongQuay.WPYC = mRecv->WPYC;
	gVongQuay.GPYC = mRecv->GPYC;

	for (int i = 0; i < mRecv->count; i++)
	{
		LISTITEMVONGQUAY_SENDINFO lpInfo = *(LISTITEMVONGQUAY_SENDINFO*)(((BYTE*)Recv) + sizeof(PMSG_YCVONGQUAY_SEND) + (sizeof(LISTITEMVONGQUAY_SENDINFO) * i));
		//==SetINfoItem
		INFO_VONGQUAY_LOCAL_ITEM infoItemLocal = { 0 };
		infoItemLocal.SizeBMD = lpInfo.SizeBMD;
		infoItemLocal.PosX = lpInfo.PosX;
		infoItemLocal.PosY = lpInfo.PosY;
		infoItemLocal.Index = lpInfo.Index;
		infoItemLocal.Item = g_pNewItemMng->CreateItem(lpInfo.Item);
		infoItemLocal.Item->Durability = lpInfo.Dur;
		if (lpInfo.PeriodTime)
		{
			infoItemLocal.Item->bPeriodItem = 1;
			infoItemLocal.Item->lExpireTime = lpInfo.PeriodTime;
		}
		gVongQuay.ListItemVongQuay.push_back(infoItemLocal);

	}
	gInterface.Data[eWindowVongQuay].OnShow = 1;

}


void CVongQuay::GetInfoVQ(BYTE* Recv)
{
	if (!Recv) return;
	XULY_CGPACKET_VONGQUAY* mRecv = (XULY_CGPACKET_VONGQUAY*)Recv;

	// StartRoll 1 = spin accepted, and it now carries the winning slot the server
	// has already drawn so the wheel can decelerate onto it. StartRoll 0 = the
	// prize has been handed out. Either way the value only feeds the animation -
	// the prize is not revealed until OnWheelStopped fires.
	gVongQuay.StartRollSau = mRecv->StartRoll;
	gVongQuay.IndexItemSau = mRecv->IndexWin;
}