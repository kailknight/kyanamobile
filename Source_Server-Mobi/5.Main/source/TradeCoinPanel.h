// TradeCoinPanel.h - coins in a trade: WCoin (C), WCoin (P) and Goblin Points
// as well as zen. The trade window's zen button opens a chooser (Zen and the
// three coins); each choice opens the usual amount box. Each money bar shows
// the currency that side offers and its amount - one currency per side.
// PC and mobile share it.
//
// Packets (server side: GameServer Trade.h - keep the layouts in step):
//   C1:D3:E8 client -> server: offer <amount> of coin <type> (0 takes it back)
//   C1:D3:E7 server -> client: both offers, sent to both traders on every change
// Zen keeps its own packets. The server un-confirms both sides on any change,
// clears the other currency (one per side), re-checks both balances when the
// trade completes, and moves the coins all-or-nothing.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "WSclient.h"
#include "NewUIMessageBox.h"
#include "NewUICustomMessageBox.h"

#define TRADE_COIN_TYPES	3

#pragma pack(push,1)

struct PMSG_TRADE_COIN_SET			// C1:D3:E8
{
	PSBMSG_HEAD header;
	BYTE type;						// 0 WCoinC, 1 WCoinP, 2 Goblin Points
	DWORD amount;
};

struct PMSG_TRADE_COIN_RECV			// C1:D3:E7
{
	PSBMSG_HEAD header;
	DWORD mine[TRADE_COIN_TYPES];
	DWORD theirs[TRADE_COIN_TYPES];
};

#pragma pack(pop)

class CTradeCoinPanel
{
public:
	CTradeCoinPanel();

	// Draws the currency chooser while it is open (CBInterface draw loop).
	void Draw();
	void OpenChooser();
	void RecvCoins(PMSG_TRADE_COIN_RECV* lpMsg);
	void SendOffer(int type,DWORD amount);

	// The coin on offer by one side, or -1 for none (zen).
	int OfferedCoin(bool mine,DWORD* amount) const;

	int m_PendingType;		// coin the open amount box is for

private:
	void Reset();

	DWORD m_Mine[TRADE_COIN_TYPES];
	DWORD m_Theirs[TRADE_COIN_TYPES];
	bool m_ChooserOpen;
	DWORD m_ChooserTick;
	bool m_WasOpen;
	bool m_WasDown;
};

extern CTradeCoinPanel* gTradeCoinPanel;

// Hooks for NewUITrade.cpp (kept to one line each there - that file is in a
// legacy code page and is edited byte-safe).
void TradeCoinOpenChooser();
void TradeCoinRenderOfferBar(bool mine,int x,int y,int zen);

namespace SEASON3B
{
	// The zen box's twin for a coin: same look, the coin's name in the text.
	class CTradeCoinMsgBoxLayout : public TMsgBoxLayout<CNewUITextInputMsgBox>
	{
	public:
		bool SetLayout();
		static CALLBACK_RESULT ReturnDown(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam);
		static CALLBACK_RESULT OkBtnDown(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam);
		static CALLBACK_RESULT CancelBtnDown(class CNewUIMessageBoxBase* pOwner, const leaf::xstreambuf& xParam);
		static CALLBACK_RESULT ProcessOk(class CNewUIMessageBoxBase* pOwner);
	};
}
