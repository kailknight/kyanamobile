// RedeemCode.h: interface for the CRedeemCode class.
//
// Redemption code system, GameServer-side. See the design plan for the full
// picture (schema, atomicity, packets) - this class is the client-facing and
// DataServer-facing entry points for it.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "DefaultClassInfo.h"
#include "User.h"
#include "Protocol.h"
#include "DSProtocol.h"

#if(REDEEMCODE)

class CRedeemCode
{
public:
	CRedeemCode();

	// Client -> GameServer entry points (Protocol.cpp's 0xD3/0xA0 and
	// 0xD3/0xA2 dispatch call these directly).
	void RequestPreview(int aIndex,const char* code);
	void RequestRedeem(int aIndex,const char* code);

	// DataServer -> GameServer reply entry points (DSProtocol.cpp's
	// DataServerProtocolCore, 0xD9/0x30 and 0xD9/0x31, call these).
	void OnPeekReply(SDHP_REDEEM_CODE_PEEK_RECV* lpMsg);
	void OnCommitReply(SDHP_REDEEM_CODE_COMMIT_RECV* lpMsg);

private:
	bool IsCodeStringValid(const char* code);
	void SendResultToClient(int aIndex,BYTE subcode,BYTE result,const REDEEM_BUNDLE_ITEM* items,int itemCount,DWORD wcoinC = 0,DWORD wcoinP = 0,DWORD goblinPoint = 0,DWORD ruud = 0);
	void GrantBundle(int aIndex,const REDEEM_BUNDLE_ITEM* items,int itemCount);

	// GameServer -> DataServer, this feature's own GD*Send equivalents.
	void SendPeekToDataServer(int aIndex,const char* accountID,const char* code);
	void SendCommitToDataServer(int aIndex,const char* accountID,const char* characterName,const char* code);

	// Per-player state for the one preview round trip that is always in
	// flight before either a "Check Code" reply or an actual commit - see
	// RequestRedeem's own comment for why the redeem flow re-peeks rather
	// than trusting a cached bundle from an earlier, possibly stale, preview.
	// Keyed by aIndex, sized to match gObj's own bound (User.h's MAX_OBJECT).
	struct PendingPeek
	{
		bool ForRedeemFlow; // false = user-visible preview; true = pre-commit check
		char Code[33];
	};
	PendingPeek m_PendingPeek[MAX_OBJECT];
};

extern CRedeemCode gRedeemCode;

#endif
