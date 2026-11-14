// CB_ItemReroll.h: Excellent Item Reroller (1.0.50) client side - the NPC window with the item slot, option locks,
// jewel / price display, Reroll (spinning reel), Confirm.
// Server side: 4.GameServer\GameServer\ItemReroll.cpp (C1:D3:68 window, C1:D3:69 state, C1:D3:69 client buttons).
// Code drawn (no textures), same pattern as CB_DungeonRace. Texts: Data\Custom\Text_xxx.ini, section [Text_ItemReroll].
#pragma once

#include "_struct.h"

class CB_ItemReroll
{
public:
	CB_ItemReroll();

	// Every frame, from Interface::Work().
	void DrawWindow();

	// Packet handlers (Protocol.cpp, case 0xD3).
	void RecvOpen(const BYTE* lpMsg, int size);
	void RecvState(const BYTE* lpMsg, int size);

	// Right click / long press on a bag item while the window is open (Interface::ActionSendItemGS).
	bool SendItem(ITEM* item, int slot);

	bool IsOpen() const { return m_Open; }

private:
	void SendAction(BYTE action, BYTE slot, BYTE locks, BYTE pay);
	void BuildLabels();
	void Reset();
	void Close(bool tellServer);
	int  Percent(int bit, BYTE locks) const;
	void SetMessage(int textIndex, bool good);

	bool  m_Click;
	bool  m_WasDown;

	bool  m_Open;
	DWORD m_OpenTick;

	// configuration (C1:D3:68)
	BYTE  m_PayMask;
	WORD  m_JewelItem, m_JewelCount, m_JewelPerLock, m_SpinMs, m_JewelHave;
	DWORD m_PriceWC, m_PriceGP, m_LockWC, m_LockGP;
	WORD  m_PriceJW, m_LockJW;

	// the chosen item (C1:D3:69)
	ITEM  m_Item;
	bool  m_HasItem;
	BYTE  m_Stage, m_Slot, m_Valid, m_Old, m_New, m_RollLocks;
	WORD  m_ItemIndex;
	BYTE  m_Flags;     // bit 0: current options cannot come up again
	BYTE  m_Weight[6]; // config chance weight per option
	BYTE  m_Mult[6];   // price multiplier per option
	BYTE  m_Locks;     // locks the player has toggled for the next Reroll
	BYTE  m_Pay;       // 1 WCoin, 2 Goblin Points, 3 jewels
	char  m_Label[6][96];

	// window position (drag the title bar)
	float m_PosX, m_PosY, m_DragDX, m_DragDY;
	bool  m_Dragging;

	// spin
	bool  m_Spinning;
	DWORD m_SpinStart;
	DWORD m_RequestTick;   // a request is in flight: buttons are locked until the answer arrives

	// message line
	char  m_Msg[160];
	bool  m_MsgGood;
	DWORD m_MsgTick;
};

extern CB_ItemReroll* gCB_ItemReroll;
