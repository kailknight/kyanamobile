// FriendMail.cpp: modern friend window data and item mail, GameServer side.
// See FriendMail.h for the send and claim sequences.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FriendMail.h"
#include "DSProtocol.h"
#include "GameMain.h"
#include "ItemManager.h"
#include "Protocol.h"
#include "ServerInfo.h"
#include "User.h"
#include "Util.h"
#include "ItemStack.h"
#if(JEWELBANKVER2)
#include "BCustomItemBank.h"
#endif

CFriendMail gFriendMail;

// C1 packets store their size in one byte. A struct change that pushes one past
// 255 would be truncated on the wire without these.
static_assert(sizeof(SDHP_FRIENDMAIL_NAME_REQ) <= 255, "C1 packet too large");
static_assert(sizeof(SDHP_FRIENDMAIL_CHECK_REQ) <= 255, "C1 packet too large");
static_assert(sizeof(SDHP_FRIENDMAIL_CHECK_RECV) <= 255, "C1 packet too large");
static_assert(sizeof(SDHP_FRIENDMAIL_STORE_RECV) <= 255, "C1 packet too large");
static_assert(sizeof(SDHP_FRIENDMAIL_MEMO_REQ) <= 255, "C1 packet too large");
static_assert(sizeof(SDHP_FRIENDMAIL_ITEMS_RECV) <= 255, "C1 packet too large");
static_assert(sizeof(SDHP_FRIENDMAIL_UNCLAIM_REQ) <= 255, "C1 packet too large");
static_assert(sizeof(PMSG_FRIENDMAIL_ITEMS_SEND) <= 255, "C1 packet too large");
static_assert(sizeof(PMSG_FRIENDMAIL_CONFIG_SEND) <= 255, "C1 packet too large");
// A send or claim waiting on the DataServer longer than this is given up on,
// so a lost reply cannot lock a player out of mail for the rest of the session.
static const DWORD kPendingTimeoutMs = 10000;

CFriendMail::CFriendMail()
{
	memset(this->m_Pending,0,sizeof(this->m_Pending));
}

bool CFriendMail::BeginPending(int aIndex,BYTE kind)
{
	PENDING& pending = this->m_Pending[aIndex];

	// Busy only for this same account's own operation, and only until the
	// timeout. A slot left behind by a player who disconnected mid-operation
	// belongs to a different account by the time anyone else uses it.
	if(pending.Kind != PENDING_NONE
		&& strncmp(pending.Account,gObj[aIndex].Account,10) == 0
		&& (GetTickCount() - pending.Tick) < kPendingTimeoutMs)
	{
		return false;
	}

	memset(&pending,0,sizeof(pending));
	pending.Kind = kind;
	pending.Tick = GetTickCount();
	memcpy(pending.Account,gObj[aIndex].Account,sizeof(pending.Account) - 1);
	return true;
}

void CFriendMail::EndPending(int aIndex)
{
	if(OBJECT_RANGE(aIndex) != 0)
	{
		memset(&this->m_Pending[aIndex],0,sizeof(PENDING));
	}
}

void CFriendMail::SendResult(int aIndex,BYTE result)
{
	PMSG_FRIENDMAIL_RESULT_SEND pMsg;
	pMsg.header.set(0xD3,0xE1,sizeof(pMsg));
	pMsg.Result = result;
	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CFriendMail::SendClaimResult(int aIndex,DWORD memoIndex,BYTE result,BYTE claimed)
{
	PMSG_FRIENDMAIL_CLAIM_SEND pMsg;
	pMsg.header.set(0xD3,0xE3,sizeof(pMsg));
	pMsg.MemoIndex = (WORD)memoIndex;
	pMsg.Result = result;
	pMsg.Claimed = claimed;
	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CFriendMail::SendNameRequest(int aIndex,BYTE subcode)
{
	SDHP_FRIENDMAIL_NAME_REQ pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,subcode,sizeof(pMsg));
	pMsg.aIndex = (WORD)aIndex;
	memcpy(pMsg.Account,gObj[aIndex].Account,sizeof(pMsg.Account));
	memcpy(pMsg.Name,gObj[aIndex].Name,sizeof(pMsg.Name));
	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

// -----------------------------------------------------------------------------
// login
// -----------------------------------------------------------------------------

void CFriendMail::OnFriendListRequested(int aIndex)
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	PMSG_FRIENDMAIL_CONFIG_SEND pMsg;
	pMsg.header.set(0xD3,0xE4,sizeof(pMsg));
	pMsg.Modern = (gServerInfo.m_FriendWindowModern != 0) ? 1 : 0;
	pMsg.FeeType = (BYTE)gServerInfo.m_MailItemFeeType;
	pMsg.Fee = (DWORD)gServerInfo.m_MailItemFee;
	pMsg.FeeItem = (WORD)gServerInfo.m_MailItemFeeItem;
	pMsg.FeeItemLevel = (BYTE)gServerInfo.m_MailItemFeeItemLevel;
	pMsg.MaxPerLetter = (BYTE)gServerInfo.m_MailItemMaxPerLetter;
	pMsg.ExpireDays = (BYTE)gServerInfo.m_MailItemExpireDays;
	pMsg.SendZen = (DWORD)gServerInfo.m_MailSendZen;
	pMsg.CoinTypes = (BYTE)gServerInfo.m_MailCoinTypes;
	pMsg.CoinMax = (DWORD)gServerInfo.m_MailCoinMaxPerLetter;
	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);

	this->SendNameRequest(aIndex,0x46);
}

// -----------------------------------------------------------------------------
// friend details
// -----------------------------------------------------------------------------

void CFriendMail::CGDetailRequest(int aIndex)
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	this->SendNameRequest(aIndex,0x40);
}

void CFriendMail::DGDetailRecv(SDHP_FRIENDMAIL_DETAIL_RECV* lpMsg)
{
	int aIndex = lpMsg->aIndex;

	if(OBJECT_RANGE(aIndex) == 0 || gObjIsAccountValid(aIndex,lpMsg->Account) == 0)
	{
		return;
	}

	PMSG_FRIENDMAIL_DETAIL_SEND pMsg;
	memset(&pMsg,0,sizeof(pMsg));

	int count = (lpMsg->Count > FRIENDMAIL_MAX_DETAILS) ? FRIENDMAIL_MAX_DETAILS : lpMsg->Count;

	for(int n = 0; n < count; n++)
	{
		PMSG_FRIENDMAIL_DETAIL& out = pMsg.List[n];
		const FRIENDMAIL_DETAIL& in = lpMsg->List[n];

		memcpy(out.Name,in.Name,sizeof(out.Name));
		out.Class = in.Class;
		out.Level = in.Level;
		out.MasterLevel = in.MasterLevel;
		out.Resets = in.Resets;
		memcpy(out.Guild,in.Guild,sizeof(out.Guild));
		out.Pending = in.Pending;
	}

	pMsg.Count = (BYTE)count;

	int size = (int)(sizeof(pMsg) - sizeof(pMsg.List) + (count * sizeof(PMSG_FRIENDMAIL_DETAIL)));
	pMsg.header.set(0xD3,0xE0,size);
	DataSend(aIndex,(BYTE*)&pMsg,size);
}

// -----------------------------------------------------------------------------
// sending
// -----------------------------------------------------------------------------

// The attached items as they stand right now. afterRoundTrip: compare against
// the index and serial captured when the request arrived - the item must be
// the very same one, not just something now sitting in that slot.
bool CFriendMail::CheckItems(LPOBJ lpObj,const PENDING& pending,bool afterRoundTrip)
{
	for(int n = 0; n < pending.ItemCount; n++)
	{
		int slot = pending.Slot[n];

		if(INVENTORY_FULL_RANGE(slot) == 0 || INVENTORY_WEAR_RANGE(slot) != 0)
		{
			return false;
		}

		for(int m = 0; m < n; m++)
		{
			if(pending.Slot[m] == slot)
			{
				return false;
			}
		}

		CItem* lpItem = &lpObj->Inventory[slot];

		if(lpItem->IsItem() == 0)
		{
			return false;
		}

		if(afterRoundTrip && (lpItem->m_Index != pending.ItemIndex[n] || lpItem->m_Serial != pending.ItemSerial[n]))
		{
			return false;
		}

		// The trade window's rule: ItemMove.txt trade flag, periodic, lucky,
		// socketed pentagram, blocked items, account item lock.
		if(gItemManager.CheckItemMoveToTrade(lpObj,lpItem,0) == 0)
		{
			return false;
		}
	}

	return true;
}

// Postage (every letter) + the per-item fee (only for attached items) + the
// coins being sent, totalled per currency so one check covers a fee and the
// coins being paid in the same currency.
void CFriendMail::GetCosts(const PENDING& pending,COSTS* costs)
{
	memset(costs,0,sizeof(COSTS));

	costs->Zen = (DWORD)gServerInfo.m_MailSendZen;

	DWORD fee = (DWORD)gServerInfo.m_MailItemFee * pending.ItemCount;

	switch(gServerInfo.m_MailItemFeeType)
	{
		case 0: costs->Zen += fee; break;
		case 1: costs->FeeItems = fee; break;
		case 2: costs->Coin[0] += (int)fee; break;
		case 3: costs->Coin[1] += (int)fee; break;
		case 4: costs->Coin[2] += (int)fee; break;
	}

	if(pending.CoinType >= FRIENDMAIL_COIN_WCOINC && pending.CoinType <= FRIENDMAIL_COIN_GOBLIN)
	{
		costs->Coin[pending.CoinType - 1] += (int)pending.CoinAmount;
	}
}

// FRIENDMAIL_CLIENT_OK, or which of the three is short.
BYTE CFriendMail::CanPay(LPOBJ lpObj,const PENDING& pending,const COSTS& costs)
{
	if(lpObj->Money < costs.Zen)
	{
		return FRIENDMAIL_CLIENT_NO_ZEN;
	}

	if(lpObj->Coin1 < costs.Coin[0] || lpObj->Coin2 < costs.Coin[1] || lpObj->Coin3 < costs.Coin[2])
	{
		return FRIENDMAIL_CLIENT_NO_COINS;
	}

	if(costs.FeeItems > 0)
	{
		int owned = this->CountFeeItemsInBag(lpObj,pending) + this->CountFeeItemsInBank(lpObj->Index);

		if(owned < (int)costs.FeeItems)
		{
			return FRIENDMAIL_CLIENT_NO_FEE;
		}
	}

	return FRIENDMAIL_CLIENT_OK;
}

// Fee items in the inventory and its expansions (GetInventoryMaxValue covers
// both). Fee items that are themselves attached do not count: they are
// leaving with the letter.
int CFriendMail::CountFeeItemsInBag(LPOBJ lpObj,const PENDING& pending)
{
	int owned = gItemManager.GetInventoryItemCount(lpObj,gServerInfo.m_MailItemFeeItem,gServerInfo.m_MailItemFeeItemLevel);

	for(int n = 0; n < pending.ItemCount; n++)
	{
		CItem* lpItem = &lpObj->Inventory[pending.Slot[n]];

		if(lpItem->IsItem() != 0 && lpItem->m_Index == gServerInfo.m_MailItemFeeItem && lpItem->m_Level == gServerInfo.m_MailItemFeeItemLevel)
		{
			owned -= (gItemStack.GetItemMaxStack(lpItem->m_Index) == 0) ? 1 : (int)lpItem->m_Durability;
		}
	}

	return (owned > 0) ? owned : 0;
}

int CFriendMail::CountFeeItemsInBank(int aIndex)
{
#if(JEWELBANKVER2)
	int banked = gBCustomItemBank.CheckCountItemBank(aIndex,gServerInfo.m_MailItemFeeItem,gServerInfo.m_MailItemFeeItemLevel);
	return (banked > 0) ? banked : 0;
#else
	return 0;
#endif
}

// The coins go first and are checked: GDSetCoinSend silently does nothing when
// a balance would go negative, so the only proof it happened is the balance
// itself. Nothing else has been taken yet if this fails.
bool CFriendMail::TakeCoins(LPOBJ lpObj,const COSTS& costs)
{
	if(costs.Coin[0] == 0 && costs.Coin[1] == 0 && costs.Coin[2] == 0)
	{
		return true;
	}

	int before[3] = { lpObj->Coin1, lpObj->Coin2, lpObj->Coin3 };

	GDSetCoinSend(lpObj->Index,-costs.Coin[0],-costs.Coin[1],-costs.Coin[2],"FriendMail");

	return lpObj->Coin1 == before[0] - costs.Coin[0]
		&& lpObj->Coin2 == before[1] - costs.Coin[1]
		&& lpObj->Coin3 == before[2] - costs.Coin[2];
}

// The bank's share of the fee was already withdrawn (DGCheckRecv); this takes
// the zen and the bag's share.
void CFriendMail::TakeZenAndFeeItems(LPOBJ lpObj,const COSTS& costs,int feeFromBag)
{
	if(costs.Zen > 0)
	{
		lpObj->Money -= costs.Zen;
		GCMoneySend(lpObj->Index,lpObj->Money);
	}

	if(feeFromBag > 0)
	{
		gItemManager.DeleteInventoryItemCount(lpObj,gServerInfo.m_MailItemFeeItem,gServerInfo.m_MailItemFeeItemLevel,feeFromBag);
	}
}

void CFriendMail::CGSendRequest(PMSG_FRIENDMAIL_SEND_RECV* lpMsg,int aIndex)
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Interface.use != 0 || lpObj->PShopOpen != 0)
	{
		this->SendResult(aIndex,FRIENDMAIL_CLIENT_BUSY);
		return;
	}

	int maxItems = gServerInfo.m_MailItemMaxPerLetter;
	bool hasCoins = (lpMsg->CoinType != FRIENDMAIL_COIN_NONE);

	if(lpMsg->ItemCount > maxItems || lpMsg->ItemCount > FRIENDMAIL_MAX_ITEMS || (lpMsg->ItemCount == 0 && hasCoins == false))
	{
		this->SendResult(aIndex,FRIENDMAIL_CLIENT_TOO_MANY);
		return;
	}

	if(hasCoins)
	{
		// An enabled coin, a positive amount, under the per-letter limit and
		// far enough from INT_MAX that no total below can overflow.
		bool allowed = lpMsg->CoinType >= FRIENDMAIL_COIN_WCOINC && lpMsg->CoinType <= FRIENDMAIL_COIN_GOBLIN
			&& (gServerInfo.m_MailCoinTypes & (1 << (lpMsg->CoinType - 1))) != 0;

		if(allowed == false || lpMsg->CoinAmount == 0 || lpMsg->CoinAmount > 1000000000
			|| (gServerInfo.m_MailCoinMaxPerLetter > 0 && lpMsg->CoinAmount > (DWORD)gServerInfo.m_MailCoinMaxPerLetter))
		{
			this->SendResult(aIndex,FRIENDMAIL_CLIENT_BAD_COIN);
			return;
		}
	}

	if(this->BeginPending(aIndex,PENDING_SEND) == false)
	{
		this->SendResult(aIndex,FRIENDMAIL_CLIENT_BUSY);
		return;
	}

	PENDING& pending = this->m_Pending[aIndex];

	memcpy(pending.ToName,lpMsg->ToName,sizeof(lpMsg->ToName));
	pending.ToName[10] = 0;
	memcpy(pending.Subject,lpMsg->Subject,sizeof(pending.Subject));
	pending.Subject[sizeof(pending.Subject) - 1] = 0;
	pending.MemoSize = (lpMsg->MemoSize > FRIENDMAIL_MEMO_MAX) ? FRIENDMAIL_MEMO_MAX : lpMsg->MemoSize;
	memcpy(pending.Memo,lpMsg->Memo,pending.MemoSize);
	pending.ItemCount = lpMsg->ItemCount;
	pending.CoinType = hasCoins ? lpMsg->CoinType : FRIENDMAIL_COIN_NONE;
	pending.CoinAmount = hasCoins ? lpMsg->CoinAmount : 0;

	for(int n = 0; n < pending.ItemCount; n++)
	{
		pending.Slot[n] = lpMsg->Slot[n];
	}

	if(pending.ToName[0] == 0 || strcmp(pending.ToName,lpObj->Name) == 0 || this->CheckItems(lpObj,pending,false) == false)
	{
		this->EndPending(aIndex);
		this->SendResult(aIndex,FRIENDMAIL_CLIENT_BAD_ITEM);
		return;
	}

	for(int n = 0; n < pending.ItemCount; n++)
	{
		pending.ItemIndex[n] = lpObj->Inventory[pending.Slot[n]].m_Index;
		pending.ItemSerial[n] = lpObj->Inventory[pending.Slot[n]].m_Serial;
	}

	COSTS costs;
	this->GetCosts(pending,&costs);

	BYTE payResult = this->CanPay(lpObj,pending,costs);

	if(payResult != FRIENDMAIL_CLIENT_OK)
	{
		this->EndPending(aIndex);
		this->SendResult(aIndex,payResult);
		return;
	}

	// Nothing is taken yet: first the DataServer confirms the recipient.
	SDHP_FRIENDMAIL_CHECK_REQ pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x41,sizeof(pMsg));
	pMsg.aIndex = (WORD)aIndex;
	memcpy(pMsg.Account,lpObj->Account,sizeof(pMsg.Account));
	memcpy(pMsg.Name,lpObj->Name,sizeof(pMsg.Name));
	memcpy(pMsg.ToName,pending.ToName,sizeof(pMsg.ToName));
	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CFriendMail::DGCheckRecv(SDHP_FRIENDMAIL_CHECK_RECV* lpMsg)
{
	int aIndex = lpMsg->aIndex;

	if(OBJECT_RANGE(aIndex) == 0 || gObjIsAccountValid(aIndex,lpMsg->Account) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];
	PENDING& pending = this->m_Pending[aIndex];

	if(pending.Kind != PENDING_SEND || strncmp(pending.ToName,lpMsg->ToName,10) != 0)
	{
		return;
	}

	if(lpMsg->Result != FRIENDMAIL_OK)
	{
		this->EndPending(aIndex);
		this->SendResult(aIndex,(lpMsg->Result == FRIENDMAIL_NOT_FRIEND) ? FRIENDMAIL_CLIENT_NOT_FRIEND : FRIENDMAIL_CLIENT_ERROR);
		return;
	}

	// Everything again, now that time has passed: the same items, still
	// tradeable, the fee still affordable, no window opened in the meantime.
	COSTS costs;
	this->GetCosts(pending,&costs);

	if(lpObj->Interface.use != 0 || lpObj->PShopOpen != 0)
	{
		this->EndPending(aIndex);
		this->SendResult(aIndex,FRIENDMAIL_CLIENT_BUSY);
		return;
	}

	if(this->CheckItems(lpObj,pending,true) == false)
	{
		this->EndPending(aIndex);
		this->SendResult(aIndex,FRIENDMAIL_CLIENT_BAD_ITEM);
		return;
	}

	BYTE payResult = this->CanPay(lpObj,pending,costs);

	if(payResult != FRIENDMAIL_CLIENT_OK)
	{
		this->EndPending(aIndex);
		this->SendResult(aIndex,payResult);
		return;
	}

	// Fee items: the bag pays what it can, the Jewel Bank the rest.
	int feeFromBag = 0;
	int feeFromBank = 0;

	if(costs.FeeItems > 0)
	{
		int inBag = this->CountFeeItemsInBag(lpObj,pending);
		feeFromBag = ((int)costs.FeeItems < inBag) ? (int)costs.FeeItems : inBag;
		feeFromBank = (int)costs.FeeItems - feeFromBag;
	}

	// The two steps that can still refuse go first, while nothing else has
	// been taken: the bank withdrawal (CongTruBank has guards of its own),
	// then the coins. A coin refusal puts the bank share back - that cannot
	// fail, the balance it checks against was just larger.
#if(JEWELBANKVER2)
	if(feeFromBank > 0 && gBCustomItemBank.CongTruBank(aIndex,gServerInfo.m_MailItemFeeItem,gServerInfo.m_MailItemFeeItemLevel,-feeFromBank,0) == 0)
	{
		this->EndPending(aIndex);
		this->SendResult(aIndex,FRIENDMAIL_CLIENT_NO_FEE);
		return;
	}
#else
	if(feeFromBank > 0)
	{
		this->EndPending(aIndex);
		this->SendResult(aIndex,FRIENDMAIL_CLIENT_NO_FEE);
		return;
	}
#endif

	if(this->TakeCoins(lpObj,costs) == false)
	{
		LogAdd(LOG_RED,"[FriendMail] %s -> %s coin deduction refused (%d,%d,%d)",lpObj->Name,pending.ToName,costs.Coin[0],costs.Coin[1],costs.Coin[2]);

#if(JEWELBANKVER2)
		if(feeFromBank > 0)
		{
			gBCustomItemBank.CongTruBank(aIndex,gServerInfo.m_MailItemFeeItem,gServerInfo.m_MailItemFeeItemLevel,+feeFromBank,0);
		}
#endif

		this->EndPending(aIndex);
		this->SendResult(aIndex,FRIENDMAIL_CLIENT_NO_COINS);
		return;
	}

	if(feeFromBank > 0)
	{
		LogAdd(LOG_BLACK,"[FriendMail] %s fee: %d from the bag, %d from the Jewel Bank",lpObj->Name,feeFromBag,feeFromBank);
	}

	SDHP_FRIENDMAIL_STORE_REQ pStore;
	memset(&pStore,0,sizeof(pStore));
	pStore.aIndex = (WORD)aIndex;
	memcpy(pStore.Account,lpObj->Account,sizeof(pStore.Account));
	memcpy(pStore.Name,lpObj->Name,sizeof(pStore.Name));
	memcpy(pStore.ToName,pending.ToName,sizeof(pStore.ToName));
	memcpy(pStore.Subject,pending.Subject,sizeof(pStore.Subject));
	memcpy(pStore.Photo,lpObj->CharSet,sizeof(pStore.Photo));
	pStore.ItemCount = pending.ItemCount;
	pStore.ExpireDays = (BYTE)gServerInfo.m_MailItemExpireDays;
	pStore.CoinType = pending.CoinType;
	pStore.CoinAmount = pending.CoinAmount;
	pStore.MemoSize = pending.MemoSize;
	memcpy(pStore.Memo,pending.Memo,pending.MemoSize);

	// The items leave the inventory first, then the fee (so a fee item can
	// never be one of the attachments), then the character is saved - and
	// only after that do the items go to the DataServer. Save-then-store on
	// one connection: a crash in between can lose an item but never copy one.
	for(int n = 0; n < pending.ItemCount; n++)
	{
		int slot = pending.Slot[n];

		gItemManager.DBItemByteConvert(pStore.Items[n],&lpObj->Inventory[slot]);

		LogAdd(LOG_BLACK,"[FriendMail] %s -> %s item %d (serial %u) from slot %d",
			lpObj->Name,pending.ToName,lpObj->Inventory[slot].m_Index,lpObj->Inventory[slot].m_Serial,slot);

		gItemManager.InventoryDelItem(aIndex,slot);
		gItemManager.GCItemDeleteSend(aIndex,slot,1);
	}

	this->TakeZenAndFeeItems(lpObj,costs,feeFromBag);

	if(pending.CoinType != FRIENDMAIL_COIN_NONE)
	{
		LogAdd(LOG_BLACK,"[FriendMail] %s -> %s coin type %d x %u",lpObj->Name,pending.ToName,(int)pending.CoinType,pending.CoinAmount);
	}

	GDCharacterInfoSaveSend(aIndex);

	int size = (int)(sizeof(pStore) - sizeof(pStore.Memo) + pStore.MemoSize);
	pStore.header.set(0xD9,0x42,size);
	gDataServerConnection.DataSend((BYTE*)&pStore,size);

	this->EndPending(aIndex);
}

void CFriendMail::DGStoreRecv(SDHP_FRIENDMAIL_STORE_RECV* lpMsg)
{
	int aIndex = lpMsg->aIndex;

	if(OBJECT_RANGE(aIndex) == 0 || gObjIsAccountValid(aIndex,lpMsg->Account) == 0)
	{
		return;
	}

	BYTE result = FRIENDMAIL_CLIENT_ERROR;

	if(lpMsg->Result == FRIENDMAIL_OK)
	{
		result = FRIENDMAIL_CLIENT_OK;
	}
	else if(lpMsg->Result == FRIENDMAIL_RETURNED)
	{
		result = FRIENDMAIL_CLIENT_RETURNED;
	}

	this->SendResult(aIndex,result);
}

// -----------------------------------------------------------------------------
// viewing and claiming
// -----------------------------------------------------------------------------

void CFriendMail::CGItemsRequest(PMSG_FRIENDMAIL_MEMO_RECV* lpMsg,int aIndex)
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	SDHP_FRIENDMAIL_MEMO_REQ pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x43,sizeof(pMsg));
	pMsg.aIndex = (WORD)aIndex;
	memcpy(pMsg.Account,gObj[aIndex].Account,sizeof(pMsg.Account));
	memcpy(pMsg.Name,gObj[aIndex].Name,sizeof(pMsg.Name));
	pMsg.MemoIndex = lpMsg->MemoIndex;
	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CFriendMail::CGClaimRequest(PMSG_FRIENDMAIL_MEMO_RECV* lpMsg,int aIndex)
{
	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Interface.use != 0 || lpObj->PShopOpen != 0 || this->BeginPending(aIndex,PENDING_CLAIM) == false)
	{
		this->SendClaimResult(aIndex,lpMsg->MemoIndex,FRIENDMAIL_CLIENT_BUSY,0);
		return;
	}

	this->m_Pending[aIndex].MemoIndex = lpMsg->MemoIndex;

	// First just look: the room check runs on what is waiting before anything
	// is claimed (DGItemsRecv).
	SDHP_FRIENDMAIL_MEMO_REQ pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x43,sizeof(pMsg));
	pMsg.aIndex = (WORD)aIndex;
	memcpy(pMsg.Account,lpObj->Account,sizeof(pMsg.Account));
	memcpy(pMsg.Name,lpObj->Name,sizeof(pMsg.Name));
	pMsg.MemoIndex = lpMsg->MemoIndex;
	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CFriendMail::DGItemsRecv(SDHP_FRIENDMAIL_ITEMS_RECV* lpMsg)
{
	int aIndex = lpMsg->aIndex;

	if(OBJECT_RANGE(aIndex) == 0 || gObjIsAccountValid(aIndex,lpMsg->Account) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];
	PENDING& pending = this->m_Pending[aIndex];
	int count = (lpMsg->Count > FRIENDMAIL_MAX_ATTACH) ? FRIENDMAIL_MAX_ATTACH : lpMsg->Count;

	if(pending.Kind != PENDING_CLAIM || pending.MemoIndex != lpMsg->MemoIndex)
	{
		// Just viewing: forward the items in the client's item format, and
		// the coins as a total per currency.
		PMSG_FRIENDMAIL_ITEMS_SEND pMsg;
		memset(&pMsg,0,sizeof(pMsg));
		pMsg.header.set(0xD3,0xE2,sizeof(pMsg));
		pMsg.MemoIndex = (WORD)lpMsg->MemoIndex;

		for(int n = 0; n < count; n++)
		{
			FRIENDMAIL_ITEM& row = lpMsg->Items[n];

			if(row.CoinType != FRIENDMAIL_COIN_NONE)
			{
				if(row.CoinType <= FRIENDMAIL_COIN_GOBLIN)
				{
					pMsg.Coins[row.CoinType - 1] += row.CoinAmount;
				}

				continue;
			}

			CItem item;

			if(pMsg.Count < FRIENDMAIL_MAX_ITEMS && gItemManager.ConvertItemByte(&item,row.ItemData) != 0)
			{
				gItemManager.ItemByteConvert(pMsg.Items[pMsg.Count],item);
				pMsg.Count++;
			}
		}

		DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
		return;
	}

	if(count == 0)
	{
		this->EndPending(aIndex);
		this->SendClaimResult(aIndex,lpMsg->MemoIndex,FRIENDMAIL_CLIENT_NOTHING,0);
		return;
	}

	// Room is needed for the items only; coins go to the account.
	REDEEM_ROOM_CHECK_ITEM room[FRIENDMAIL_MAX_ATTACH];
	int roomCount = 0;

	for(int n = 0; n < count; n++)
	{
		if(lpMsg->Items[n].CoinType != FRIENDMAIL_COIN_NONE)
		{
			continue;
		}

		CItem item;
		gItemManager.ConvertItemByte(&item,lpMsg->Items[n].ItemData);
		room[roomCount].ItemIndex = item.m_Index;
		room[roomCount].Quantity = 1;
		roomCount++;
	}

	if(lpObj->Interface.use != 0 || lpObj->PShopOpen != 0)
	{
		this->EndPending(aIndex);
		this->SendClaimResult(aIndex,lpMsg->MemoIndex,FRIENDMAIL_CLIENT_BUSY,0);
		return;
	}

	if(roomCount > 0 && gItemManager.CheckItemInventorySpaceForBundle(lpObj,room,roomCount) == false)
	{
		this->EndPending(aIndex);
		this->SendClaimResult(aIndex,lpMsg->MemoIndex,FRIENDMAIL_CLIENT_FULL,0);
		return;
	}

	SDHP_FRIENDMAIL_MEMO_REQ pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x44,sizeof(pMsg));
	pMsg.aIndex = (WORD)aIndex;
	memcpy(pMsg.Account,lpObj->Account,sizeof(pMsg.Account));
	memcpy(pMsg.Name,lpObj->Name,sizeof(pMsg.Name));
	pMsg.MemoIndex = lpMsg->MemoIndex;
	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CFriendMail::DGClaimRecv(SDHP_FRIENDMAIL_ITEMS_RECV* lpMsg)
{
	int aIndex = lpMsg->aIndex;
	int count = (lpMsg->Count > FRIENDMAIL_MAX_ATTACH) ? FRIENDMAIL_MAX_ATTACH : lpMsg->Count;

	SDHP_FRIENDMAIL_UNCLAIM_REQ undo;
	memset(&undo,0,sizeof(undo));
	undo.header.set(0xD9,0x45,sizeof(undo));

	// The DataServer echoes the character name, so a claim can be undone even
	// when the player is no longer here to ask.
	memcpy(undo.Name,lpMsg->Name,sizeof(undo.Name));

	if(OBJECT_RANGE(aIndex) == 0 || gObjIsAccountValid(aIndex,lpMsg->Account) == 0)
	{
		// The player left while the claim was in flight: the items are marked
		// claimed but are in no inventory. Put every one back in the mailbox.
		for(int n = 0; n < count; n++)
		{
			undo.ItemID[undo.Count++] = lpMsg->Items[n].ItemID;
		}

		if(undo.Count > 0)
		{
			gDataServerConnection.DataSend((BYTE*)&undo,undo.header.size);
		}

		LogAdd(LOG_RED,"[FriendMail] %.10s left during a claim - %d item(s) put back in the mailbox",lpMsg->Name,count);

		return;
	}

	LPOBJ lpObj = &gObj[aIndex];
	int placed = 0;

	for(int n = 0; n < count; n++)
	{
		FRIENDMAIL_ITEM& row = lpMsg->Items[n];

		if(row.CoinType != FRIENDMAIL_COIN_NONE)
		{
			// Coins: credited to the account. GDSetCoinSend refuses silently
			// (balance would pass INT_MAX), so check the balance moved, and
			// put the row back if it did not.
			int add[3] = { 0, 0, 0 };

			if(row.CoinType <= FRIENDMAIL_COIN_GOBLIN && row.CoinAmount > 0 && row.CoinAmount <= 1000000000)
			{
				add[row.CoinType - 1] = (int)row.CoinAmount;
			}

			int before[3] = { lpObj->Coin1, lpObj->Coin2, lpObj->Coin3 };

			if(add[0] != 0 || add[1] != 0 || add[2] != 0)
			{
				GDSetCoinSend(aIndex,add[0],add[1],add[2],"FriendMailClaim");
			}

			if((add[0] | add[1] | add[2]) == 0
				|| lpObj->Coin1 != before[0] + add[0] || lpObj->Coin2 != before[1] + add[1] || lpObj->Coin3 != before[2] + add[2])
			{
				undo.ItemID[undo.Count++] = row.ItemID;
				continue;
			}

			LogAdd(LOG_BLACK,"[FriendMail] %s claimed coin type %d x %u from letter %d",lpObj->Name,(int)row.CoinType,row.CoinAmount,(int)lpMsg->MemoIndex);

			placed++;
			continue;
		}

		CItem item;

		if(gItemManager.ConvertItemByte(&item,row.ItemData) == 0)
		{
			undo.ItemID[undo.Count++] = row.ItemID;
			continue;
		}

		BYTE slot = gItemManager.InventoryInsertItem(aIndex,item);

		if(slot == 0xFF)
		{
			// Room ran out between the check and now. Not lost: back it goes.
			undo.ItemID[undo.Count++] = lpMsg->Items[n].ItemID;
			continue;
		}

		gItemManager.GCItemModifySend(aIndex,slot);

		LogAdd(LOG_BLACK,"[FriendMail] %s claimed item %d (serial %u) into slot %d from letter %d",
			lpObj->Name,item.m_Index,item.m_Serial,slot,(int)lpMsg->MemoIndex);

		placed++;
	}

	if(placed > 0)
	{
		GDCharacterInfoSaveSend(aIndex);
	}

	if(undo.Count > 0)
	{
		gDataServerConnection.DataSend((BYTE*)&undo,undo.header.size);
	}

	this->EndPending(aIndex);

	BYTE result = (placed > 0) ? FRIENDMAIL_CLIENT_OK : ((count == 0) ? FRIENDMAIL_CLIENT_NOTHING : FRIENDMAIL_CLIENT_FULL);
	this->SendClaimResult(aIndex,lpMsg->MemoIndex,result,(BYTE)placed);

	// Fresh counts, so the letter loses its attachment mark.
	this->SendNameRequest(aIndex,0x46);
}

void CFriendMail::DGCountsRecv(SDHP_FRIENDMAIL_COUNTS_RECV* lpMsg)
{
	int aIndex = lpMsg->aIndex;

	if(OBJECT_RANGE(aIndex) == 0 || gObjIsAccountValid(aIndex,lpMsg->Account) == 0)
	{
		return;
	}

	PMSG_FRIENDMAIL_COUNTS_SEND pMsg;
	memset(&pMsg,0,sizeof(pMsg));

	int count = (lpMsg->Count > FRIENDMAIL_MAX_COUNTS) ? FRIENDMAIL_MAX_COUNTS : lpMsg->Count;

	for(int n = 0; n < count; n++)
	{
		pMsg.List[n].MemoIndex = (WORD)lpMsg->List[n].MemoIndex;
		pMsg.List[n].Count = lpMsg->List[n].Count;
	}

	pMsg.Count = (BYTE)count;

	int size = (int)(sizeof(pMsg) - sizeof(pMsg.List) + (count * sizeof(PMSG_FRIENDMAIL_COUNT)));
	pMsg.header.set(0xD3,0xE5,size);
	DataSend(aIndex,(BYTE*)&pMsg,size);
}
