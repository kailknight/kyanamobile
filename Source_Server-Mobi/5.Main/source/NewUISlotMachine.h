// NewUISlotMachine.h: interface for the CNewUISlotMachine class.
//
// The 5x3 slot machine window. Opened by the server when the player talks to the
// machine's NPC.
//
// THIS WINDOW DECIDES NOTHING. Every symbol, every winning line and every payout
// arrives finished from the server; all this does is spin the reels and land them
// on what it was told. The reveal is gated on the animation finishing rather than
// on the packet arriving, or the win would flash up before the reels stopped.
//
// Drawn entirely from art the client already ships - the standard message-box
// frame, the existing three-state button, filled boxes for the cells, and item
// icons for the symbols. Nothing new has to reach the player's install, on PC or
// on Android. Every element goes through one of the small Draw* helpers below so
// bespoke artwork can replace any single piece later without touching the layout.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "NewUIBase.h"
#include "NewUIButton.h"
#include "NewUIMessageBox.h"
#include "NewUIMyInventory.h"
#include "Protocol.h"

namespace SEASON3B
{
	// Geometry, matching the GameServer's SlotMachine.h. Fixed on both sides - the
	// server sends its own Reels/Rows in the open packet purely so a mismatched
	// client can refuse rather than misdraw.
	#define SLOTUI_REELS			5
	#define SLOTUI_ROWS				3
	#define SLOTUI_CELLS			(SLOTUI_REELS * SLOTUI_ROWS)

	#define SLOTUI_MAX_SYMBOL		16
	#define SLOTUI_MAX_LINE			16
	#define SLOTUI_MAX_STAKE		16
	#define SLOTUI_CLAIM_SIZE		8

	// Symbol kinds, matching the server's SLOT_SYMBOL_*.
	#define SLOTUI_KIND_NORMAL		0
	#define SLOTUI_KIND_SCATTER		1
	#define SLOTUI_KIND_MULTIPLIER	2

	// The store type the server uses for this machine's off-trade-style Type field
	// is not needed here; the window is opened by a packet, not by a Type.

	class CNewUISlotMachine : public CNewUIObj
	{
	public:
		enum IMAGE_LIST
		{
			// This window's OWN textures - Data\Custom\SlotMachine\*.ozt - so unlike
			// most windows it registers ids and must DeleteBitmap them.
			IMAGE_SLOT_BG = BITMAP_SLOTMACHINE_BEGIN,	// 1024x1024, whole window
			IMAGE_SLOT_CELL,							// 128x128 empty cell
			IMAGE_SLOT_CELL_WIN,						// 128x128 green, a cell that paid
			IMAGE_SLOT_CELL_SCATTER,					// 128x128 star frame
			IMAGE_SLOT_BTN_NORMAL,						// 256x256, art in a 241x78 rect
			IMAGE_SLOT_BTN_HOVER,
			IMAGE_SLOT_BTN_PRESSED,
			IMAGE_SLOT_SMALL,							// 128x256, three round buttons stacked
			IMAGE_SLOT_PLATE,							// 128x64, coin left / coin stack right
			IMAGE_SLOT_BANNER,							// 512x128
			IMAGE_SLOT_COUNT_END
		};

		// The block reserved in _TextureIndex.h is BEGIN+12; these ten must fit it,
		// and must stay under the effect range's ceiling.
		static_assert(IMAGE_SLOT_COUNT_END <= BITMAP_SLOTMACHINE_END, "slot machine textures overflow their reserved id block");
		static_assert(BITMAP_SLOTMACHINE_END < BITMAP_EFFECT_TEXTURE_END, "slot machine ids collide with the no-name texture range");

	private:
		enum
		{
			WINDOW_WIDTH  = 400,
			WINDOW_HEIGHT = 400,

			CELL_SIZE  = 64,
			GRID_X     = 40,			// relative to m_Pos; the frame opening is 40..360
			GRID_Y     = 62,
		};

		// What the reels are doing right now.
		enum eSpinState
		{
			SPIN_IDLE = 0,			// showing the last result, accepting input
			SPIN_ROLLING,			// request sent, server has not answered
			SPIN_LANDING,			// answer in hand, reels easing onto it
			SPIN_REVEAL,			// landed; the win is now allowed to show
		};

	public:
		// ---- Wire, mirrored from the GameServer's SlotMachine.h -------------
		// #pragma pack(1) and the same static_asserts, because nothing else keeps
		// the two declarations in step.
#pragma pack(push,1)

		struct PMSG_SLOT_OPEN_SYMBOL
		{
			BYTE Kind;
			WORD ItemIndex;
			BYTE Level;
			BYTE Value;
		};

		struct PMSG_SLOT_OPEN_LINE
		{
			BYTE Row[SLOTUI_REELS];
		};

		struct PMSG_SLOT_OPEN_STAKE
		{
			WORD ItemIndex;
			BYTE Level;
			WORD Credits;
			BYTE Kind;			// 0 item, 1 WCoinC, 2 WCoinP, 3 GoblinPoint
			DWORD Amount;		// items or coins per unit of bet
			char Name[32];
		};

		// C2:D3:8F, followed by SymbolCount symbols, LineCount lines, StakeCount
		// stakes.
		struct PMSG_SLOT_OPEN_RECV
		{
			PSWMSG_HEAD header;
			BYTE SymbolCount;
			BYTE LineCount;
			BYTE StakeCount;
			BYTE MultiCap;
			BYTE Reels;
			BYTE Rows;
			BYTE MaxBet;
			WORD FreeSpinsLeft;
		};

		// C1:D3:90
		struct PMSG_SLOT_SPIN_SEND
		{
			PSBMSG_HEAD header;
			BYTE stake;
			BYTE bet;
		};

		struct PMSG_SLOT_RESULT_LINE
		{
			BYTE line;
			BYTE count;
			BYTE multi;
			DWORD pay;
		};

		// C2:D3:91, followed by WinLineCount lines.
		struct PMSG_SLOT_RESULT_RECV
		{
			PSWMSG_HEAD header;
			BYTE result;
			BYTE Cell[SLOTUI_CELLS];
			BYTE ScatterCount;
			BYTE WinLineCount;
			BYTE FreeSpinAwarded;
			WORD FreeSpinsLeft;
			DWORD TotalPay;
			WORD StakeItemIndex;
			BYTE StakeLevel;
			BYTE WasFreeSpin;
			BYTE BetCount;
		};

		// C1:D3:92. slot 0xFF claims every row.
		struct PMSG_SLOT_CLAIM_SEND
		{
			PSBMSG_HEAD header;
			BYTE slot;
		};

		struct PMSG_SLOT_CLAIM_ROW
		{
			BYTE slot;
			WORD ItemIndex;
			BYTE Level;
			DWORD Count;
		};

		// C2:D3:93, followed by count rows.
		struct PMSG_SLOT_CLAIM_LIST_RECV
		{
			PSWMSG_HEAD header;
			BYTE count;
			BYTE free;
			BYTE result;
		};

#pragma pack(pop)

	public:
		CNewUISlotMachine();
		virtual ~CNewUISlotMachine();

		bool Create(CNewUIManager* pNewUIMng, int x, int y);
		void Release();

		void SetPos(int x, int y);

		// Where the window currently is, in 640x480 UI space.
		//
		// Needed because this window is DRAGGABLE, unlike every other entry in
		// android_main's kAndroidChatFriendlyWindows - those all sit at fixed
		// coordinates, so their clearance from the chat block could be decided once
		// by hand. This one moves, so the same question has to be asked per frame
		// against wherever the player has put it.
		void GetRect(int* x, int* y, int* w, int* h);

		// CNewUIObj
		bool UpdateMouseEvent();
		bool UpdateKeyEvent();
		bool Update();
		bool Render();
		float GetLayerDepth();		// 8.2f - above the minimap, below Help

		void OpeningProcess();
		void ClosingProcess();

		// ---- Packets in ----------------------------------------------------
		void RecvOpen(BYTE* lpMsg);
		void RecvResult(BYTE* lpMsg);
		void RecvClaimList(BYTE* lpMsg);

	private:
		void LoadImages();
		void UnloadImages();

		void SendSpin();
		void SendClaim(int slot);

		// ---- Draw helpers --------------------------------------------------
		// One per visual element, so bespoke art can replace any single piece by
		// changing only that function.
		// Draws a SUB-RECTANGLE of a texture, scaled to a destination rect. The
		// stock RenderImage cannot do this: its 5-argument form takes the on-screen
		// size as the number of texture pixels to read, so it crops instead of
		// scaling. Source coordinates are in texture pixels, top-left origin.
		void DrawArt(int image, float x, float y, float w, float h, float srcX, float srcY, float srcW, float srcH);

		// The wide plank button, in whichever of its three states applies. The label
		// is drawn here rather than baked into the art, because SPIN, CLAIM ALL, MAX
		// and the three stake names all share one plank.
		void DrawWideButton(float x, float y, float w, float h, int state, const char* label, DWORD color);
		// One of the three stacked round buttons in slot_btn_small.
		void DrawRoundButton(float x, float y, float size, int state);

		// 0 = normal, 1 = hover, 2 = pressed - from the live mouse state.
		int  ButtonState(float x, float y, float w, float h);

		void DrawFrame();
		void DrawReels();
		void DrawOneCell(float x, float y, int symbol, bool winning);
		void DrawWinLines();
		void DrawStakeRow();
		void DrawBetRow();
		void DrawRewardBox();
		void DrawStatusText();

		// ---- Animation ----------------------------------------------------
		// Millisecond-driven, not frame-counted: a frame counter would make the
		// reels spin at different speeds on different machines.
		void UpdateReels();
		bool IsCellSpinning(int reel);

		// Puts the window in the middle of the visible UI area. Uses the real UI width
		// rather than 640: on a widescreen client the UI space is wider than that, so a
		// hardcoded (640 - 400) / 2 sits left of centre.
		void CenterWindow();
		void ClampToScreen();

		int SymbolItemIndex(int symbol);
		int SymbolLevel(int symbol);
		bool IsCellOnWinningLine(int reel, int row);

	private:
		CNewUIManager*	m_pNewUIMng;
		POINT			m_Pos;


		// ---- Machine config, from the open packet -------------------------
		bool			m_Ready;			// an open packet has been received
		int				m_SymbolCount;
		int				m_LineCount;
		int				m_StakeCount;
		int				m_MultiCap;

		PMSG_SLOT_OPEN_SYMBOL	m_Symbol[SLOTUI_MAX_SYMBOL];
		PMSG_SLOT_OPEN_LINE		m_Line[SLOTUI_MAX_LINE];
		PMSG_SLOT_OPEN_STAKE	m_Stake[SLOTUI_MAX_STAKE];

		int				m_SelectedStake;

		// How many of the chosen item to stake per spin, and the machine's ceiling.
		// The paytable is written for a one-item bet, so this multiplies every
		// payout - which is what makes it a bet SIZE rather than just a quantity.
		int				m_Bet;
		int				m_MaxBet;

		// ---- Last result --------------------------------------------------
		// Held separately from what the reels are DISPLAYING, which is the whole
		// point: m_Cell is the answer, m_ShownCell is what is on screen, and they
		// only agree once the animation has finished.
		BYTE			m_Cell[SLOTUI_CELLS];
		BYTE			m_ShownCell[SLOTUI_CELLS];

		PMSG_SLOT_RESULT_LINE	m_WinLine[SLOTUI_MAX_LINE];
		int				m_WinLineCount;
		int				m_ScatterCount;
		DWORD			m_TotalPay;
		int				m_FreeSpinsLeft;
		int				m_FreeSpinAwarded;
		int				m_StakeItemIndex;
		int				m_StakeLevel;
		bool			m_WasFreeSpin;
		BYTE			m_LastResultCode;

		// ---- Reward box ---------------------------------------------------
		PMSG_SLOT_CLAIM_ROW	m_Claim[SLOTUI_CLAIM_SIZE];
		int				m_ClaimCount;
		int				m_ClaimFree;

		// ---- Animation state ----------------------------------------------
		int				m_SpinState;
		DWORD			m_SpinStartMs;
		DWORD			m_LastFrameMs;
		DWORD			m_LandStartMs;
		// Per-reel scroll offset in pixels, and the moment each reel is due to
		// stop. Reels stop left to right, which is what makes the last reel feel
		// decisive.
		float			m_ReelOffset[SLOTUI_REELS];
		DWORD			m_ReelStopMs[SLOTUI_REELS];

		// Whether each reel has been committed to its real result. Reels are committed
		// ONE AT A TIME as they stop, left to right. Committing all five when the last
		// one lands (the first version) left reels 1-4 showing stale symbols while they
		// sat stopped, then changed all of them at once.
		bool			m_ReelLanded[SLOTUI_REELS];

		// Window dragging. Started from the title bar, so the reels and buttons stay
		// clickable.
		bool			m_Dragging;
		int				m_DragDX;
		int				m_DragDY;

		// Cosmetic scroll while waiting for the server. The reels have to be moving
		// before the answer arrives or pressing Spin looks like nothing happened.
		BYTE			m_RollCell[SLOTUI_CELLS];
		DWORD			m_RollAdvanceMs;
	};

	inline
	void CNewUISlotMachine::SetPos(int x, int y)
	{
		m_Pos.x = x; m_Pos.y = y;
	}

	inline
	void CNewUISlotMachine::GetRect(int* x, int* y, int* w, int* h)
	{
		if(x != NULL) *x = m_Pos.x;
		if(y != NULL) *y = m_Pos.y;
		if(w != NULL) *w = WINDOW_WIDTH;
		if(h != NULL) *h = WINDOW_HEIGHT;
	}

	inline
	float CNewUISlotMachine::GetLayerDepth()
	{
		return 8.2f;
	}
};
