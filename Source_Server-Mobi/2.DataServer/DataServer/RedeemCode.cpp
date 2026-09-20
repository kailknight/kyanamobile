// RedeemCode.cpp: implementation of the CRedeemCode class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if(REDEEMCODE)

#include "RedeemCode.h"
#include "Util.h"
#include "QueryManager.h"
#include "SocketManager.h"
#include "DataServerProtocol.h"

CRedeemCode gRedeemCode;

// Defense in depth: GameServer already whitelists the code string to
// alphanumeric+dash before it is ever sent (RedeemCode.cpp, GameServer side),
// but ExecQuery (QueryManager.cpp) builds its SQL with plain sprintf-style
// interpolation and no escaping, so this is checked again at the actual SQL
// boundary rather than trusted blindly from the wire.
static bool IsSafeIdentifierString(const char* text,size_t maxLen)
{
	if(text == NULL)
	{
		return false;
	}

	size_t len = strlen(text);

	if(len == 0 || len > maxLen)
	{
		return false;
	}

	for(size_t n = 0; n < len; n++)
	{
		char c = text[n];
		bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';

		if(!ok)
		{
			return false;
		}
	}

	return true;
}

// WZ_PeekRedeemCode and WZ_CommitRedeemCode (RedeemCode_Schema.sql) both
// return the exact same shape: a single "Result" column on every path, plus
// - only when Result is REDEEM_OK_TO_REDEEM or REDEEM_SUCCESS - one row per
// bundle item alongside it. SET NOCOUNT ON in both procs is what keeps this
// a single result set for ExecQuery (QueryManager.cpp:102) to parse; without
// it, the intervening INSERT/UPDATE in WZ_CommitRedeemCode would desync the
// column binding for the caller.
BYTE CRedeemCode::RunRedeemQuery(const char* query,REDEEM_BUNDLE_ITEM* outItems,BYTE* outItemCount,REDEEM_CURRENCY_REWARD* outCurrency)
{
	*outItemCount = 0;
	memset(outCurrency,0,sizeof(*outCurrency));

	BYTE result = REDEEM_SERVER_ERROR;

	if(gQueryManager.ExecQuery((char*)query) != 0)
	{
		bool first = true;

		while(gQueryManager.Fetch() != SQL_NO_DATA)
		{
			if(first)
			{
				result = (BYTE)gQueryManager.GetAsInteger("Result");
				first = false;

				// Per-code, so it is repeated identically on every row of the
				// join - read it once, off the first row only. Guarded by the
				// result check because the early-exit error paths SELECT only
				// a Result column, and GetAsInteger returns -1 for a column
				// that isn't in the result set (QueryManager.cpp).
				if(result == REDEEM_OK_TO_REDEEM || result == REDEEM_SUCCESS)
				{
					int wcoinC = gQueryManager.GetAsInteger("RewardWCoinC");
					int wcoinP = gQueryManager.GetAsInteger("RewardWCoinP");
					int goblin = gQueryManager.GetAsInteger("RewardGoblinPoint");
					int ruud   = gQueryManager.GetAsInteger("RewardRuud");

					outCurrency->WCoinC      = (wcoinC > 0) ? (DWORD)wcoinC : 0;
					outCurrency->WCoinP      = (wcoinP > 0) ? (DWORD)wcoinP : 0;
					outCurrency->GoblinPoint = (goblin > 0) ? (DWORD)goblin : 0;
					outCurrency->Ruud        = (ruud   > 0) ? (DWORD)ruud   : 0;
				}
			}

			if((result == REDEEM_OK_TO_REDEEM || result == REDEEM_SUCCESS) && *outItemCount < REDEEM_CODE_BUNDLE_MAX)
			{
				// Rows with no bundle (the early-exit error results) simply
				// don't have these columns - GetAsInteger returns -1 for an
				// unknown column name (QueryManager.cpp), which never
				// matters here because this branch only runs on the two
				// results that DO carry a bundle.
				int itemIndex = gQueryManager.GetAsInteger("ItemIndex");

				if(itemIndex >= 0)
				{
					REDEEM_BUNDLE_ITEM& item = outItems[*outItemCount];
					item.ItemIndex = (WORD)itemIndex;
					item.ItemLevel = (BYTE)gQueryManager.GetAsInteger("ItemLevel");
					item.ItemSkill = (BYTE)gQueryManager.GetAsInteger("ItemSkill");
					item.ItemLuck = (BYTE)gQueryManager.GetAsInteger("ItemLuck");
					item.ItemOption = (BYTE)gQueryManager.GetAsInteger("ItemOption");
					item.ItemExcellent = (BYTE)gQueryManager.GetAsInteger("ItemExcellent");
					item.Quantity = (BYTE)gQueryManager.GetAsInteger("Quantity");
					item.ItemDuration = (DWORD)gQueryManager.GetAsInteger("ItemDuration");
					item.SocketOption[0] = (BYTE)gQueryManager.GetAsInteger("ItemSocket1");
					item.SocketOption[1] = (BYTE)gQueryManager.GetAsInteger("ItemSocket2");
					item.SocketOption[2] = (BYTE)gQueryManager.GetAsInteger("ItemSocket3");
					item.SocketOption[3] = (BYTE)gQueryManager.GetAsInteger("ItemSocket4");
					item.SocketOption[4] = (BYTE)gQueryManager.GetAsInteger("ItemSocket5");
					item.SocketOptionBonus = (BYTE)gQueryManager.GetAsInteger("SocketBonus");
					item.Item380 = (BYTE)gQueryManager.GetAsInteger("Item380");
					item.HarmonyOption = (BYTE)gQueryManager.GetAsInteger("HarmonyOption");
					item.HarmonyOptionLevel = (BYTE)gQueryManager.GetAsInteger("HarmonyOptionLevel");
					(*outItemCount)++;
				}
			}
		}

		if(first)
		{
			// ExecQuery succeeded but returned no rows at all - shouldn't
			// happen (every path in both procs SELECTs at least a Result
			// row), but fail closed rather than report a false success.
			result = REDEEM_SERVER_ERROR;
		}
	}

	gQueryManager.Close();

	return result;
}

void CRedeemCode::OnPeekRequest(GSSENDDS_REDEEM_CODE_PEEK* lpMsg,int aIndex)
{
	SDHP_REDEEM_CODE_PEEK_RECV pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x30,sizeof(pMsg));
	pMsg.aIndex = lpMsg->aIndex;
	memcpy(pMsg.AccountID,lpMsg->AccountID,sizeof(pMsg.AccountID));

	if(!IsSafeIdentifierString(lpMsg->Code,32) || !IsSafeIdentifierString(lpMsg->AccountID,10))
	{
		pMsg.Result = REDEEM_NOT_FOUND;
	}
	else
	{
		char query[256];
		sprintf_s(query,sizeof(query),"EXEC WZ_PeekRedeemCode '%s','%s'",lpMsg->Code,lpMsg->AccountID);

		BYTE itemCount = 0;
		REDEEM_CURRENCY_REWARD currency;
		pMsg.Result = this->RunRedeemQuery(query,pMsg.Items,&itemCount,&currency);
		pMsg.ItemCount = itemCount;
		pMsg.RewardWCoinC = currency.WCoinC;
		pMsg.RewardWCoinP = currency.WCoinP;
		pMsg.RewardGoblinPoint = currency.GoblinPoint;
		pMsg.RewardRuud = currency.Ruud;
	}

	gSocketManager.DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CRedeemCode::OnCommitRequest(GSSENDDS_REDEEM_CODE_COMMIT* lpMsg,int aIndex)
{
	SDHP_REDEEM_CODE_COMMIT_RECV pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x31,sizeof(pMsg));
	pMsg.aIndex = lpMsg->aIndex;
	memcpy(pMsg.AccountID,lpMsg->AccountID,sizeof(pMsg.AccountID));

	if(!IsSafeIdentifierString(lpMsg->Code,32)
		|| !IsSafeIdentifierString(lpMsg->AccountID,10)
		|| !IsSafeIdentifierString(lpMsg->CharacterName,10))
	{
		pMsg.Result = REDEEM_NOT_FOUND;
	}
	else
	{
		char query[256];
		sprintf_s(query,sizeof(query),"EXEC WZ_CommitRedeemCode '%s','%s','%s'",lpMsg->Code,lpMsg->AccountID,lpMsg->CharacterName);

		BYTE itemCount = 0;
		REDEEM_CURRENCY_REWARD currency;
		pMsg.Result = this->RunRedeemQuery(query,pMsg.Items,&itemCount,&currency);
		pMsg.ItemCount = itemCount;
		pMsg.RewardWCoinC = currency.WCoinC;
		pMsg.RewardWCoinP = currency.WCoinP;
		pMsg.RewardGoblinPoint = currency.GoblinPoint;
		pMsg.RewardRuud = currency.Ruud;
	}

	gSocketManager.DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

#endif
