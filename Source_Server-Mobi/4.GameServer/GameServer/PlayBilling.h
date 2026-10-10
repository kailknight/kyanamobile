// PlayBilling.h: Google Play WCoin (C) recharge, GameServer side.
//
// Relay only: the phone sends its Play purchase token (0xD3/0xF0), this adds
// the logged-in account and forwards it to the DataServer (0xD9/0x37), which
// checks it with Google and credits the WCoin (2.DataServer PlayBilling.cpp).
// The answer comes back on 0xD9/0x37; this refreshes the balance and tells
// the phone (0xD3/0xF1). Nothing here trusts the client for the amount.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Protocol.h"

#define PLAY_PRODUCT_ID_SIZE	32
#define PLAY_TOKEN_SIZE			512

#pragma pack(push, 1)

// Client -> GameServer, 0xD3/0xF0 (C2).
struct PMSG_PLAY_PURCHASE_RECV
{
	PSWMSG_HEAD header;
	char ProductId[PLAY_PRODUCT_ID_SIZE];
	char Token[PLAY_TOKEN_SIZE];
};

// GameServer -> client, 0xD3/0xF1.
struct PMSG_PLAY_PURCHASE_SEND
{
	PSBMSG_HEAD header;
	BYTE Result;					// PLAY_RESULT_* (DataServer PlayBilling.h)
	DWORD WCoinC;
	char ProductId[PLAY_PRODUCT_ID_SIZE];
};

// GameServer -> DataServer, 0xD9/0x37 (C2). Same layout as the DataServer's
// SDHP_PLAY_PURCHASE_RECV.
struct SDHP_PLAY_PURCHASE_SEND
{
	PSWMSG_HEAD header;
	WORD aIndex;
	char AccountID[11];
	char ProductId[PLAY_PRODUCT_ID_SIZE];
	char Token[PLAY_TOKEN_SIZE];
};

// DataServer -> GameServer, 0xD9/0x37. Same layout as the DataServer's
// SDHP_PLAY_PURCHASE_SEND.
struct SDHP_PLAY_PURCHASE_RECV
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char AccountID[11];
	char ProductId[PLAY_PRODUCT_ID_SIZE];
	BYTE Result;
	DWORD WCoinC;
};

#pragma pack(pop)

static_assert(sizeof(SDHP_PLAY_PURCHASE_SEND) == 5 + 2 + 11 + PLAY_PRODUCT_ID_SIZE + PLAY_TOKEN_SIZE, "SDHP_PLAY_PURCHASE_SEND must match the DataServer layout");
static_assert(sizeof(SDHP_PLAY_PURCHASE_RECV) == 4 + 2 + 11 + PLAY_PRODUCT_ID_SIZE + 1 + 4, "SDHP_PLAY_PURCHASE_RECV must match the DataServer layout");

class CPlayBilling
{
public:
	void CGPurchaseRecv(PMSG_PLAY_PURCHASE_RECV* lpMsg,int aIndex,int size);
	void DGPurchaseRecv(SDHP_PLAY_PURCHASE_RECV* lpMsg);

private:
	void SendClientResult(int aIndex,BYTE result,DWORD wcoin,const char* productId);
};

extern CPlayBilling gPlayBilling;
