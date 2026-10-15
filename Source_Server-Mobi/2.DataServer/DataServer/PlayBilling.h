// PlayBilling.h: Google Play and App Store WCoin (C) recharge, DataServer side.
//
// App Store purchases use the same flow with the token "apple-<transaction
// id>": the worker asks Apple's App Store Server API instead of Google
// (RunVerifyApple in PlayBilling.cpp) and the phone, not this side, finishes
// the purchase afterwards.
//
// The phone pays Google, the game hands the purchase token to GameServer,
// GameServer forwards it here (0xD9/0x37). This side asks Google whether the
// purchase is real and unused, credits the pack's WCoin (C) in one SQL
// transaction keyed on the token (WZ_PlayStoreCredit, PlayBilling_Schema.sql),
// answers GameServer (0xD9/0x37), then tells Google the purchase is consumed.
//
// Google is only ever called from a worker thread: everything else in the
// DataServer runs on the one ServerQueueThread, and an HTTPS round trip there
// would stall every other request. The worker posts its answer back into that
// queue as 0xD9/0x38, so all SQL still runs on the queue thread.
//
// Config: .\PlayBilling.ini (see LoadConfig). The pack amounts live there, not
// in the packet, so a modified client cannot ask for more.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "DataServerProtocol.h"

#define PLAY_PRODUCT_ID_SIZE	32
#define PLAY_ORDER_ID_SIZE		64
#define PLAY_TOKEN_SIZE			512

// Result codes, DataServer -> GameServer -> client.
enum ePlayBillingResult
{
	PLAY_RESULT_CREDITED = 0,		// verified, WCoin added
	PLAY_RESULT_ALREADY = 1,		// this token was credited before - nothing added
	PLAY_RESULT_INVALID = 2,		// Google says not purchased / cancelled / unknown token
	PLAY_RESULT_PENDING = 3,		// payment not cleared yet
	PLAY_RESULT_RETRY = 4,			// Google or the database unreachable - try again later
	PLAY_RESULT_DISABLED = 5,		// not configured on this server
	PLAY_RESULT_UNKNOWN_PRODUCT = 6,	// product id not in PlayBilling.ini
};

#pragma pack(push, 1)

// GameServer -> DataServer, 0xD9/0x37 (C2: the token alone can be 200+ bytes).
struct SDHP_PLAY_PURCHASE_RECV
{
	PSWMSG_HEAD header;
	WORD aIndex;
	char AccountID[11];
	char ProductId[PLAY_PRODUCT_ID_SIZE];
	char Token[PLAY_TOKEN_SIZE];
};

// Worker -> queue thread, 0xD9/0x38. Never sent on the wire.
struct SDHP_PLAY_PURCHASE_VERIFIED
{
	PSWMSG_HEAD header;
	DWORD Magic;					// kVerifiedMagic, so a stray 0x38 is ignored
	WORD aIndex;
	char AccountID[11];
	char ProductId[PLAY_PRODUCT_ID_SIZE];
	char OrderId[PLAY_ORDER_ID_SIZE];
	char Token[PLAY_TOKEN_SIZE];
	BYTE Result;					// PLAY_RESULT_*; CREDITED here means "Google says paid and unused"
	BYTE Consumed;					// Google already shows it consumed
};

// DataServer -> GameServer, 0xD9/0x37.
struct SDHP_PLAY_PURCHASE_SEND
{
	PSBMSG_HEAD header;
	WORD aIndex;
	char AccountID[11];
	char ProductId[PLAY_PRODUCT_ID_SIZE];
	BYTE Result;
	DWORD WCoinC;					// added this time (CREDITED only)
};

#pragma pack(pop)

// Must match GameServer's PlayBilling.h (C2 head = 5 bytes, C1 = 4).
static_assert(sizeof(SDHP_PLAY_PURCHASE_RECV) == 5 + 2 + 11 + PLAY_PRODUCT_ID_SIZE + PLAY_TOKEN_SIZE, "SDHP_PLAY_PURCHASE_RECV must match the GameServer layout");
static_assert(sizeof(SDHP_PLAY_PURCHASE_SEND) == 4 + 2 + 11 + PLAY_PRODUCT_ID_SIZE + 1 + 4, "SDHP_PLAY_PURCHASE_SEND must match the GameServer layout");

class CPlayBilling
{
public:
	CPlayBilling();

	void LoadConfig();
	void StartWorker();

	// DataServerProtocolCore, 0xD9/0x37 and 0xD9/0x38.
	void OnPurchaseRequest(SDHP_PLAY_PURCHASE_RECV* lpMsg,int index);
	void OnPurchaseVerified(SDHP_PLAY_PURCHASE_VERIFIED* lpMsg,int index);

	static const DWORD kVerifiedMagic = 0x50424C56; // 'PBLV'

private:
	void SendResult(int index,WORD aIndex,const char* account,const char* productId,BYTE result,DWORD wcoin);

	bool m_Enabled;
	char m_KeyFile[MAX_PATH];
	char m_PackageName[128];

	// App Store ([AppStore] in PlayBilling.ini): purchase tokens "apple-<id>".
	bool m_AppleEnabled;
	bool m_AppleAllowSandbox;
	char m_AppleSandboxAccounts[256];	// sandbox purchases credited for these only
	char m_AppleKeyFile[MAX_PATH];
	char m_AppleKeyId[32];
	char m_AppleIssuer[64];
	char m_AppleBundle[128];
};

extern CPlayBilling gPlayBilling;
