// RedeemCodeWindow.h: interface for the CB_RedeemCodeWindow class.
//
// Desktop UI for the redemption code system. Follows CB_DoiMK's shape (a
// plain GL-drawn window via g_pBCustomMenuInfo->gDrawWindowCustom/DrawButton,
// no bitmap art needed) rather than the heavier CNewUIObj/bitmap-skinned
// window framework, since this is a small text-box-and-a-list window with no
// existing art to reuse.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include "Protocol.h"
#if(REDEEMCODE)

class CB_RedeemCodeWindow
{
public:
	CB_RedeemCodeWindow();
	~CB_RedeemCodeWindow();

	void OpenWindow();
	void DrawWindow();

	// Called from Protocol.cpp's client-facing 0xD3 dispatch for both 0xA1
	// (preview reply) and 0xA3 (redeem reply) - same payload shape, the
	// caller passes which one arrived.
	void RecvPreviewResult(PMSG_REDEEM_CODE_RECV* lpMsg);
	void RecvRedeemResult(PMSG_REDEEM_CODE_RECV* lpMsg);

private:
	enum State
	{
		STATE_ENTRY,    // typing a code, nothing sent yet
		STATE_WAITING,  // preview or redeem round trip in flight
		STATE_PREVIEW,  // bundle shown, not yet consumed - Redeem or Back
		STATE_RESULT,   // redeemed (or a final error) - Close or try another
	};

	void Reset();
	void SendPreviewRequest();
	void SendRedeemRequest();
	void RenderItemRow(int row, float x, float y, float w, float h,const REDEEM_ITEM_DETAIL& detail);
	// Drawn after every row, not inside RenderItemRow - a tooltip drawn mid-loop
	// gets painted over by the next row's own background bar.
	void RenderHoveredTooltip();
	// Currency rows are drawn above the item rows; returns how many were drawn
	// so the caller can offset the item list by that much.
	int RenderCurrencyRows(float x, float y, float w, float rowH, float stride);
	int CurrencyRewardCount() const;
	const char* ResultMessage(BYTE result) const;

	State m_State;
	char m_CodeText[33];
	BYTE m_Result;
	BYTE m_ItemCount;
	REDEEM_ITEM_DETAIL m_Items[REDEEM_CODE_BUNDLE_MAX];
	// Per-code currency rewards, shown as their own rows above the items.
	DWORD m_RewardWCoinC;
	DWORD m_RewardWCoinP;
	DWORD m_RewardGoblinPoint;
	DWORD m_RewardRuud;
	int m_HoveredRow;
	float m_HoveredTipX;      // preferred anchor, just right of the window
	float m_HoveredTipLeftX;  // window's left edge, for the flip-to-left case
	float m_HoveredTipY;
	DWORD m_LastSendTick;
};

extern CB_RedeemCodeWindow* gCB_RedeemCodeWindow;

#endif
