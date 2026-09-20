// RedeemCode.cpp: implementation of the CRedeemCode class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "RedeemCode.h"

#if(REDEEMCODE)

#include "ItemManager.h"
#include "Util.h"
#include "GameMain.h"
#include "CashShop.h"
#include <algorithm>

CRedeemCode gRedeemCode;

CRedeemCode::CRedeemCode()
{
	memset(this->m_PendingPeek,0,sizeof(this->m_PendingPeek));
}

// Whitelist, not a blacklist - ExecQuery (QueryManager.cpp) builds its SQL
// with plain sprintf-style interpolation, no escaping, so this is the only
// thing standing between a code string and a broken/malicious query. Codes
// are meant to be short and typed by hand anyway, so alphanumeric + dash is
// not a real limitation.
bool CRedeemCode::IsCodeStringValid(const char* code)
{
	if(code == NULL)
	{
		return false;
	}

	size_t len = strlen(code);

	if(len == 0 || len > 32)
	{
		return false;
	}

	for(size_t n = 0; n < len; n++)
	{
		char c = code[n];
		bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';

		if(!ok)
		{
			return false;
		}
	}

	return true;
}

void CRedeemCode::RequestPreview(int aIndex,const char* code)
{
	if(gObj[aIndex].Type != OBJECT_USER || gObjIsConnected(aIndex) == false)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	// Same "not in the middle of something else" guard MocNap uses before it
	// will grant anything (MocNap.cpp) - a stray tap while trading, in an NPC
	// window, or already mid another transaction is simply ignored.
	if(lpObj->Interface.type != INTERFACE_NONE || lpObj->Interface.use != 0 || lpObj->Transaction == 1)
	{
		return;
	}

	if(lpObj->RedeemCodeBusy)
	{
		this->SendResultToClient(aIndex,0xA1,REDEEM_BUSY,NULL,0);
		return;
	}

	if(this->IsCodeStringValid(code) == false)
	{
		this->SendResultToClient(aIndex,0xA1,REDEEM_NOT_FOUND,NULL,0);
		return;
	}

	lpObj->RedeemCodeBusy = true;

	this->m_PendingPeek[aIndex].ForRedeemFlow = false;
	memset(this->m_PendingPeek[aIndex].Code,0,sizeof(this->m_PendingPeek[aIndex].Code));
	strncpy_s(this->m_PendingPeek[aIndex].Code,sizeof(this->m_PendingPeek[aIndex].Code),code,sizeof(this->m_PendingPeek[aIndex].Code)-1);

	this->SendPeekToDataServer(aIndex,lpObj->Account,code);
}

void CRedeemCode::RequestRedeem(int aIndex,const char* code)
{
	if(gObj[aIndex].Type != OBJECT_USER || gObjIsConnected(aIndex) == false)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Interface.type != INTERFACE_NONE || lpObj->Interface.use != 0 || lpObj->Transaction == 1)
	{
		return;
	}

	if(lpObj->RedeemCodeBusy)
	{
		this->SendResultToClient(aIndex,0xA3,REDEEM_BUSY,NULL,0);
		return;
	}

	if(this->IsCodeStringValid(code) == false)
	{
		this->SendResultToClient(aIndex,0xA3,REDEEM_NOT_FOUND,NULL,0);
		return;
	}

	lpObj->RedeemCodeBusy = true;

	// Deliberately re-peeks rather than trusting a bundle the client already
	// has from an earlier "Check Code" - the player may have sat looking at
	// that preview for a while, so this re-validates everything fresh right
	// before the room check and the actual commit. See OnPeekReply.
	this->m_PendingPeek[aIndex].ForRedeemFlow = true;
	memset(this->m_PendingPeek[aIndex].Code,0,sizeof(this->m_PendingPeek[aIndex].Code));
	strncpy_s(this->m_PendingPeek[aIndex].Code,sizeof(this->m_PendingPeek[aIndex].Code),code,sizeof(this->m_PendingPeek[aIndex].Code)-1);

	this->SendPeekToDataServer(aIndex,lpObj->Account,code);
}

void CRedeemCode::OnPeekReply(SDHP_REDEEM_CODE_PEEK_RECV* lpMsg)
{
	int aIndex = lpMsg->aIndex;

	if(aIndex < 0 || aIndex >= MAX_OBJECT || gObjIsAccountValid(aIndex,lpMsg->AccountID) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];
	PendingPeek& pending = this->m_PendingPeek[aIndex];

	if(!pending.ForRedeemFlow)
	{
		// User-visible "Check Code" - relay straight back. Nothing was
		// mutated either way, so there is nothing more to do here.
		lpObj->RedeemCodeBusy = false;
		this->SendResultToClient(aIndex,0xA1,lpMsg->Result,lpMsg->Items,lpMsg->ItemCount,lpMsg->RewardWCoinC,lpMsg->RewardWCoinP,lpMsg->RewardGoblinPoint,lpMsg->RewardRuud);
		return;
	}

	// Redeem flow's own silent re-peek. Anything other than a fresh
	// REDEEM_OK_TO_REDEEM here means the code stopped being valid sometime
	// between the player's earlier preview and this Redeem tap (someone else
	// used the last slot, an admin deactivated it, it expired) - relay that
	// as the final answer and stop, exactly as if this were the commit reply.
	if(lpMsg->Result != REDEEM_OK_TO_REDEEM)
	{
		// Worth seeing: a burst of these from one account is someone guessing
		// codes, which is otherwise invisible outside the database.
		LogAddCat(LOG_CAT_REDEEM,LOG_RED,"[RedeemCode] %s - code rejected (result %d)",
			lpObj->Name,lpMsg->Result);

		lpObj->RedeemCodeBusy = false;
		this->SendResultToClient(aIndex,0xA3,lpMsg->Result,lpMsg->Items,lpMsg->ItemCount,lpMsg->RewardWCoinC,lpMsg->RewardWCoinP,lpMsg->RewardGoblinPoint,lpMsg->RewardRuud);
		return;
	}

	REDEEM_ROOM_CHECK_ITEM roomCheck[REDEEM_CODE_BUNDLE_MAX];
	int roomCheckCount = (lpMsg->ItemCount < REDEEM_CODE_BUNDLE_MAX) ? lpMsg->ItemCount : REDEEM_CODE_BUNDLE_MAX;

	for(int n = 0; n < roomCheckCount; n++)
	{
		roomCheck[n].ItemIndex = lpMsg->Items[n].ItemIndex;
		roomCheck[n].Quantity = lpMsg->Items[n].Quantity;
	}

	// The one thing this whole two-round-trip design exists for: the room
	// check has to run, and fail closed, BEFORE the code is ever consumed -
	// see the plan's Atomicity section. Failing here costs the player nothing.
	if(gItemManager.CheckItemInventorySpaceForBundle(lpObj,roomCheck,roomCheckCount) == false)
	{
		LogAddCat(LOG_CAT_REDEEM,LOG_RED,"[RedeemCode] %s - refused, inventory full (%d item(s) needed)",
			lpObj->Name,roomCheckCount);

		lpObj->RedeemCodeBusy = false;
		this->SendResultToClient(aIndex,0xA3,REDEEM_INVENTORY_FULL,NULL,0);
		return;
	}

	// Room confirmed locally - only now does the code actually get consumed.
	this->SendCommitToDataServer(aIndex,lpMsg->AccountID,lpObj->Name,pending.Code);
}

void CRedeemCode::OnCommitReply(SDHP_REDEEM_CODE_COMMIT_RECV* lpMsg)
{
	int aIndex = lpMsg->aIndex;

	if(aIndex < 0 || aIndex >= MAX_OBJECT || gObjIsAccountValid(aIndex,lpMsg->AccountID) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	lpObj->RedeemCodeBusy = false;

	if(lpMsg->Result == REDEEM_SUCCESS)
	{
		// The redemption is already recorded in the DB at this point; this is the
		// only operator-visible record that it happened. Codes are one-per-account
		// (UQ_CustomCodeRedemptions_Code_Account), so this view stays sparse.
		LogAddCat(LOG_CAT_REDEEM,LOG_GREEN,"[RedeemCode] %s redeemed a code - %d item(s), WC %d, WP %d, GP %d, Ruud %d",
			lpObj->Name,lpMsg->ItemCount,
			lpMsg->RewardWCoinC,lpMsg->RewardWCoinP,lpMsg->RewardGoblinPoint,lpMsg->RewardRuud);

		this->GrantBundle(aIndex,lpMsg->Items,lpMsg->ItemCount);

		// Currency rewards. One call covers all four - GDCashShopAddPointSaveSend
		// (CashShop.cpp:1701) is the engine's own primitive for them and handles
		// both the DataServer-side save and the live player's Coin1/Coin2/Coin3/
		// Ruud, including its own overflow guard (CheckCoinBBT). Skipped entirely
		// when a code carries no currency so an items-only code doesn't emit a
		// pointless zero save.
		if(lpMsg->RewardWCoinC != 0 || lpMsg->RewardWCoinP != 0 || lpMsg->RewardGoblinPoint != 0 || lpMsg->RewardRuud != 0)
		{
			gCashShop.GDCashShopAddPointSaveSend(aIndex,0,
				lpMsg->RewardWCoinC,
				lpMsg->RewardWCoinP,
				lpMsg->RewardGoblinPoint,
				lpMsg->RewardRuud,
				"RedeemCode");
		}
	}

	this->SendResultToClient(aIndex,0xA3,lpMsg->Result,lpMsg->Items,lpMsg->ItemCount,lpMsg->RewardWCoinC,lpMsg->RewardWCoinP,lpMsg->RewardGoblinPoint,lpMsg->RewardRuud);
}

// Only reached after WZ_CommitRedeemCode has already recorded the redemption
// and incremented UsedCount - if anything below silently failed to actually
// place an item (e.g. GDCreateItemSend's own async round trip losing the
// race against something else changing the grid), that is the same class of
// gap MocNap already lives with, not a new one this feature introduces. See
// the plan's note on GDCreateItemSend/DGCreateItemRecv's own lack of
// end-to-end atomicity.
void CRedeemCode::GrantBundle(int aIndex,const REDEEM_BUNDLE_ITEM* items,int itemCount)
{
	LPOBJ lpObj = &gObj[aIndex];
	int count = (itemCount < REDEEM_CODE_BUNDLE_MAX) ? itemCount : REDEEM_CODE_BUNDLE_MAX;

	for(int n = 0; n < count; n++)
	{
		// Packed exactly as ZzzInventory.cpp:9475-9476 unpacks it on the
		// client - high nibble = option type (0-7), low nibble = required
		// level (0-15). A type of 0 reads as "no option" client-side, same
		// as this byte being 0 outright.
		BYTE harmonyByte = (BYTE)(((items[n].HarmonyOption & 0x0F) << 4) | (items[n].HarmonyOptionLevel & 0x0F));

		// C380ItemOption::Is380Item checks bit 0x80 of ItemOptionEx
		// (380ItemOption.cpp:114-122) - no other bits of this byte are used
		// by anything this feature grants, so it's safe to just set the one
		// bit outright rather than OR it into some other base value.
		BYTE itemOptionEx = items[n].Item380 ? 0x80 : 0;

		for(int copy = 0; copy < items[n].Quantity; copy++)
		{
			GDCreateItemSend(
				aIndex,
				0xEB,                    // map==0xEB -> inventory
				(BYTE)lpObj->X,
				(BYTE)lpObj->Y,
				items[n].ItemIndex,
				items[n].ItemLevel,
				0,                       // Dur - 0 lets this auto-compute the item's default
				items[n].ItemSkill,
				items[n].ItemLuck,
				items[n].ItemOption,
				-1,                      // LootIndex - not from a monster loot table
				items[n].ItemExcellent,
				0,                       // SetOption/ancient set - CItem::Convert (Item.cpp:229) zeroes this
				                         // unconditionally regardless of what's passed, so it has no effect
				                         // through this path; not exposed to the admin for that reason.
				harmonyByte,
				itemOptionEx,
				const_cast<BYTE*>(items[n].SocketOption),
				items[n].SocketOptionBonus,
				items[n].ItemDuration);
		}
	}
}

void CRedeemCode::SendResultToClient(int aIndex,BYTE subcode,BYTE result,const REDEEM_BUNDLE_ITEM* items,int itemCount,DWORD wcoinC,DWORD wcoinP,DWORD goblinPoint,DWORD ruud)
{
	PMSG_REDEEM_CODE_RECV pMsg;

	pMsg.header.set(0xD3,subcode,sizeof(pMsg));
	pMsg.Result = result;
	pMsg.ItemCount = 0;
	memset(pMsg.Items,0,sizeof(pMsg.Items));
	pMsg.RewardWCoinC = wcoinC;
	pMsg.RewardWCoinP = wcoinP;
	pMsg.RewardGoblinPoint = goblinPoint;
	pMsg.RewardRuud = ruud;

	if(items != NULL)
	{
		int count = (itemCount < REDEEM_CODE_BUNDLE_MAX) ? itemCount : REDEEM_CODE_BUNDLE_MAX;

		for(int n = 0; n < count; n++)
		{
			pMsg.Items[n].ItemIndex = items[n].ItemIndex;
			pMsg.Items[n].ItemLevel = items[n].ItemLevel;
			pMsg.Items[n].ItemSkill = items[n].ItemSkill;
			pMsg.Items[n].ItemLuck = items[n].ItemLuck;
			pMsg.Items[n].ItemOption = items[n].ItemOption;
			pMsg.Items[n].ItemExcellent = items[n].ItemExcellent;
			pMsg.Items[n].Quantity = items[n].Quantity;
			memcpy(pMsg.Items[n].SocketOption, items[n].SocketOption, sizeof(pMsg.Items[n].SocketOption));
			pMsg.Items[n].SocketOptionBonus = items[n].SocketOptionBonus;
			pMsg.Items[n].Item380 = items[n].Item380;
			pMsg.Items[n].HarmonyOption = items[n].HarmonyOption;
			pMsg.Items[n].HarmonyOptionLevel = items[n].HarmonyOptionLevel;
		}

		pMsg.ItemCount = (BYTE)count;
	}

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CRedeemCode::SendPeekToDataServer(int aIndex,const char* accountID,const char* code)
{
	if(gObjIsAccountValid(aIndex,gObj[aIndex].Account) == 0)
	{
		return;
	}

	GSSENDDS_REDEEM_CODE_PEEK pMsg;

	pMsg.header.set(0xD9,0x30,sizeof(pMsg));
	pMsg.aIndex = aIndex;

	memset(pMsg.AccountID,0,sizeof(pMsg.AccountID));
	strncpy_s(pMsg.AccountID,sizeof(pMsg.AccountID),accountID,sizeof(pMsg.AccountID)-1);

	memset(pMsg.Code,0,sizeof(pMsg.Code));
	strncpy_s(pMsg.Code,sizeof(pMsg.Code),code,sizeof(pMsg.Code)-1);

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CRedeemCode::SendCommitToDataServer(int aIndex,const char* accountID,const char* characterName,const char* code)
{
	if(gObjIsAccountValid(aIndex,gObj[aIndex].Account) == 0)
	{
		return;
	}

	GSSENDDS_REDEEM_CODE_COMMIT pMsg;

	pMsg.header.set(0xD9,0x31,sizeof(pMsg));
	pMsg.aIndex = aIndex;

	memset(pMsg.AccountID,0,sizeof(pMsg.AccountID));
	strncpy_s(pMsg.AccountID,sizeof(pMsg.AccountID),accountID,sizeof(pMsg.AccountID)-1);

	memset(pMsg.CharacterName,0,sizeof(pMsg.CharacterName));
	strncpy_s(pMsg.CharacterName,sizeof(pMsg.CharacterName),characterName,sizeof(pMsg.CharacterName)-1);

	memset(pMsg.Code,0,sizeof(pMsg.Code));
	strncpy_s(pMsg.Code,sizeof(pMsg.Code),code,sizeof(pMsg.Code)-1);

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

#endif
