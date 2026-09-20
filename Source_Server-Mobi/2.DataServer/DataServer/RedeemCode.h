// RedeemCode.h: interface for the CRedeemCode class.
//
// Redemption code system, DataServer-side. Runs the two stored procedures
// (RedeemCode_Schema.sql) and replies to GameServer - all the actual SQL
// lives here, GameServer never touches the database directly.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "DataServerProtocol.h"

#if(REDEEMCODE)

class CRedeemCode
{
public:
	// GameServer -> DataServer request handlers (DataServerProtocol.cpp's
	// DataServerProtocolCore, 0xD9/0x30 and 0xD9/0x31, call these).
	void OnPeekRequest(GSSENDDS_REDEEM_CODE_PEEK* lpMsg,int aIndex);
	void OnCommitRequest(GSSENDDS_REDEEM_CODE_COMMIT* lpMsg,int aIndex);

private:
	// Runs WZ_PeekRedeemCode/WZ_CommitRedeemCode and reads back whichever
	// result set they returned (just a Result code, or Result + the bundle)
	// into the two out-params. Shared because the two procs return the exact
	// same shape (see RedeemCode_Schema.sql) - only the query string differs.
	BYTE RunRedeemQuery(const char* query,REDEEM_BUNDLE_ITEM* outItems,BYTE* outItemCount,REDEEM_CURRENCY_REWARD* outCurrency);
};

extern CRedeemCode gRedeemCode;

#endif
