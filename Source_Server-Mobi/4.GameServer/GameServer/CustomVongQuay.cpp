#include "stdafx.h"
#include "CustomVongQuay.h"
#include "MemScript.h"
#include "Util.h"
#include "Notice.h"
#include "DSProtocol.h"
#include "EffectManager.h"
#include "Map.h"
#include "ItemOptionRate.h"
#include "ObjectManager.h"
#include "Guild.h"
#include "Move.h"
#include "Monster.h"
#include "ItemBagManager.h"
#include "Party.h"
#include "CashShop.h"
#include "MapServerManager.h"
#include "ServerInfo.h"
#include "RandomManager.h"
#include "ItemLevel.h"
#include "GameMain.h"	// gDataServerConnection

CCustomVongQuay gCustomVongQuay;

CCustomVongQuay::CCustomVongQuay()
{
	this->Init();
}


CCustomVongQuay::~CCustomVongQuay()
{
}

void CCustomVongQuay::Init()
{
	this->SoVongQuay = 0;
}

void CCustomVongQuay::LoadFileXML(char* FilePath)
{
	pugi::xml_document file;
	pugi::xml_parse_result res = file.load_file(FilePath);
	if (res.status != pugi::status_ok) {
		ErrorMessageBox("File %s load fail. Error: %s", FilePath, res.description());
		return;
	}
	//--
	//--
	pugi::xml_node oCustomVongQuay = file.child("CustomVongQuay");
	this->Enable = oCustomVongQuay.attribute("Enable").as_int();
	this->Firework = oCustomVongQuay.attribute("Firework").as_int();
	this->Notice = oCustomVongQuay.attribute("Notice").as_int();
	//= Mess Load
	this->m_MessageInfoBP.clear();
	pugi::xml_node Message = oCustomVongQuay.child("MessageInfo");
	for (pugi::xml_node msg = Message.child("Message"); msg; msg = msg.next_sibling())
	{
		MESSAGE_INFO_VONGQUAY info;

		info.Index = msg.attribute("Index").as_int();

		strcpy_s(info.Message, msg.attribute("Text").as_string());

		this->m_MessageInfoBP.insert(std::pair<int, MESSAGE_INFO_VONGQUAY>(info.Index, info));
	}
	//====Load Data Moc Nap
	this->m_DataVongQuay.clear();
	pugi::xml_node ConfigVongQuay = oCustomVongQuay.child("ConfigVongQuay");
	int IndexMocNap = 1;
	for (pugi::xml_node VongQuay = ConfigVongQuay.child("VongQuay"); VongQuay; VongQuay = VongQuay.next_sibling())
	{
		DATA_VONGQUAY infoData = { 0 };
		infoData.IndexVongQuay = IndexMocNap++;
		
		infoData.IndexItemYC = SafeGetItem(GET_ITEM(VongQuay.attribute("Type").as_int(), VongQuay.attribute("Index").as_int()));
		infoData.WC = VongQuay.attribute("WC").as_int();
		infoData.WP = VongQuay.attribute("WP").as_int();
		infoData.GP = VongQuay.attribute("GP").as_int();
		infoData.Count = VongQuay.attribute("Count").as_int();
		strncpy_s(infoData.NameVongQuay, VongQuay.attribute("Name").as_string(), sizeof(infoData.NameVongQuay));
		//===ItemNhan
		infoData.ListItemNhan.clear();
		pugi::xml_node ItemNhan = VongQuay.child("ItemNhan");
		
		for (pugi::xml_node Item = ItemNhan.child("Item"); Item; Item = Item.next_sibling())
		{
			if (infoData.ListItemNhan.size() > 11) break;

			DATA_VONGQUAYITEM ListItemInfo;
			ListItemInfo.SizeBMD = Item.attribute("SizeBMD").as_float();
			ListItemInfo.PosX = Item.attribute("PosX").as_float();
			ListItemInfo.PosY = Item.attribute("PosY").as_float();
			ListItemInfo.IndexItem = SafeGetItem(GET_ITEM(Item.attribute("Type").as_int(), Item.attribute("Index").as_int()));
			ListItemInfo.LvItem = Item.attribute("LvItem").as_int();
			ListItemInfo.Dur = Item.attribute("Dur").as_int();
			ListItemInfo.Skill = Item.attribute("Skill").as_int();
			ListItemInfo.Luck = Item.attribute("Luck").as_int();
			ListItemInfo.Opt = Item.attribute("Opt").as_int();
			ListItemInfo.Exc = Item.attribute("Exc").as_int();
			ListItemInfo.Anc = Item.attribute("Anc").as_int();

			ListItemInfo.SK[0] = Item.attribute("SK1").as_int();
			ListItemInfo.SK[1] = Item.attribute("SK2").as_int();
			ListItemInfo.SK[2] = Item.attribute("SK3").as_int();
			ListItemInfo.SK[3] = Item.attribute("SK4").as_int();
			ListItemInfo.SK[4] = Item.attribute("SK5").as_int();

			ListItemInfo.SKBonus = Item.attribute("SKBonus").as_int();
			ListItemInfo.HSD = Item.attribute("HSD").as_int();
			ListItemInfo.Rate = Item.attribute("Rate").as_int();
			//LogAdd(LOG_CAM, "[VongQuay] [%d] index %d size ", ListItemInfo.IndexItem, infoData.ListItemNhan.size());
			infoData.ListItemNhan.push_back(ListItemInfo);
		}
		this->m_DataVongQuay.insert(std::pair<int, DATA_VONGQUAY>(infoData.IndexVongQuay, infoData));
	}
	
	LogAddCat(LOG_CAT_SPINWHEEL, LOG_BLUE, "[SpinWheel] Enabled %d, %d wheels loaded", this->Enable, this->m_DataVongQuay.size());
}

char* CCustomVongQuay::GetMessage(int index) // OK
{
	std::map<int, MESSAGE_INFO_VONGQUAY>::iterator it = this->m_MessageInfoBP.find(index);

	if (it == this->m_MessageInfoBP.end())
	{
		// Was a stack-local buffer returned by address - a dangling pointer the
		// caller then formatted through. Use a member buffer, the same way
		// CMessage::GetMessage does with m_DefaultMessage.
		wsprintf(this->m_DefaultMessage, "Could not find message %d!", index);
		return this->m_DefaultMessage;
	}
	else
	{
		return it->second.Message;
	}
}


void CCustomVongQuay::UserSendClientInfo(int aIndex) //Send Danh Sach Moc Nap Ve Client
{

	if (gObj[aIndex].Type != OBJECT_USER)
	{
		return;
	}

	if (gObjIsConnected(aIndex) == false)
	{
		return;
	}
	if (gObj[aIndex].IsBot >= 1 || gObj[aIndex].m_OfflineMode != 0 || gObj[aIndex].IsFakeOnline != 0)
	{
		return;
	}
	BYTE send[4096];
	PMSG_VONGQUAY_SEND pMsg = { 0 };
	// ---
	pMsg.header.set(0xD3, 0x8A, 0);

	int size = sizeof(pMsg);

	pMsg.count = 0;
	

	for (std::map<int, DATA_VONGQUAY>::iterator it = this->m_DataVongQuay.begin(); it != this->m_DataVongQuay.end(); it++)
	{
		if (it == this->m_DataVongQuay.end())
		{
			break;
		}
		ListVongQuaySend info;
		info.IndexVongQuay = it->second.IndexVongQuay;;
		memset(info.Name, 0, sizeof(info.Name));
		memcpy(info.Name, it->second.NameVongQuay, sizeof(info.Name));
		if ((size + sizeof(info) >= 4096))
		{
			break;
		}
		pMsg.count++;
		memcpy(&send[size], &info, sizeof(info));
		size += sizeof(info);
	}
	pMsg.header.size[0] = SET_NUMBERHB(size);
	pMsg.header.size[1] = SET_NUMBERLB(size);
	// ---
	memcpy(send, &pMsg, sizeof(pMsg));

	DataSend(aIndex, send, size);
	//LogAdd(LOG_RED, "SendINfo List Vong Quay %s", gObj[aIndex].Name);
	//SendListNhanThuong(gObj[aIndex].Index, 1);
}


void VongQuay_ItemByteConvert(BYTE* lpMsg, DATA_VONGQUAYITEM* Data) // OK
{

	lpMsg[0] = Data->IndexItem & 0xFF;

	lpMsg[1] = 0;
	lpMsg[1] |= Data->LvItem * 8;
	lpMsg[1] |= Data->Skill * 128;
	lpMsg[1] |= Data->Luck * 4;
	lpMsg[1] |= Data->Opt & 3;

	lpMsg[2] = Data->Dur;

	lpMsg[3] = 0;
	lpMsg[3] |= (Data->IndexItem & 0x100) >> 1;
	lpMsg[3] |= ((Data->Opt > 3) ? 0x40 : 0);
	lpMsg[3] |= Data->Exc;

	lpMsg[4] = Data->Anc;

	lpMsg[5] = 0;
	lpMsg[5] |= (Data->IndexItem & 0x1E00) >> 5;
	lpMsg[5] |= ((Data->Exc & 0x80) >> 4);
	lpMsg[5] |= ((Data->HSD & 1) << 2);

	lpMsg[6] = Data->SKBonus;

	lpMsg[7] = Data->SK[0];
	lpMsg[8] = Data->SK[1];
	lpMsg[9] = Data->SK[2];
	lpMsg[10] = Data->SK[3];
	lpMsg[11] = Data->SK[4];
}


void CCustomVongQuay::SendListNhanThuong(int aIndex, int VongQuaySo) //Send List SendListNhanThuong
{
	if (!this->Enable)
	{
		gNotice.GCNoticeSend(aIndex, 1, 0, 0, 0, 0, 0, this->GetMessage(0)); //
		return;
	}

	if (gObj[aIndex].Type != OBJECT_USER)
	{
		return;
	}

	if (gObjIsConnected(aIndex) == false)
	{
		return;
	}
	if (gObj[aIndex].IsBot >= 1 || gObj[aIndex].m_OfflineMode != 0 || gObj[aIndex].IsFakeOnline != 0)
	{
		return;
	}
	BYTE send[4096];
	PMSG_YCVONGQUAY_SEND pMsg = { 0 };
	// ---
	pMsg.header.set(0xD3, 0x8B, 0);

	int size = sizeof(pMsg);

	pMsg.count = 0;

	std::map<int, DATA_VONGQUAY>::iterator it = this->m_DataVongQuay.find(VongQuaySo);

	if (it == this->m_DataVongQuay.end())
	{
		gNotice.GCNoticeSend(aIndex, eMessageBox, 0, 0, 0, 0, 0, this->GetMessage(1)); //Khong co thong tin cua moc nap
		return;
	}
	pMsg.IndexYC = it->second.IndexItemYC;
	pMsg.CountItem = it->second.Count;
	pMsg.WCYC = it->second.WC;
	pMsg.WPYC = it->second.WP;
	pMsg.GPYC = it->second.GP;

	for (std::vector<DATA_VONGQUAYITEM>::iterator itItem = it->second.ListItemNhan.begin(); itItem != it->second.ListItemNhan.end(); itItem++)
	{
		if (itItem == it->second.ListItemNhan.end())
		{
			break;
		}

		
		LISTITEMVONGQUAY_SENDINFO info;
		info.SizeBMD = itItem->SizeBMD;
		info.PosX = itItem->PosX;
		info.PosY = itItem->PosY;
		info.Index = itItem->IndexItem;
		info.Dur = itItem->Dur;
		VongQuay_ItemByteConvert(info.Item, &*itItem);
		time_t t = time(NULL);
		localtime(&t);
		DWORD iTime = (DWORD)t + itItem->HSD * 60;
		if ((itItem->HSD) > 0)
		{
			info.PeriodTime = iTime;
		}
		else
		{
			info.PeriodTime = itItem->HSD;
		}
		if ((size + sizeof(info) >= 4096))
		{
			break;
		}
		pMsg.count++;
		memcpy(&send[size], &info, sizeof(info));
		size += sizeof(info);
	}
	pMsg.header.size[0] = SET_NUMBERHB(size);
	pMsg.header.size[1] = SET_NUMBERLB(size);
	// ---
	memcpy(send, &pMsg, sizeof(pMsg));

	DataSend(aIndex, send, size);
	//LogAdd(LOG_RED, "Send List Item Vong Quay %s", gObj[aIndex].Name);
}

// ===========================================================================
// Claim box
//
// Holds prizes the player had no room for. Stores a TEMPLATE rather than a live
// item: GDCreateItemSend is not called until the prize is actually claimed, so
// the DataServer never issues a serial for an item nobody holds and a crash
// cannot orphan one.
// ===========================================================================

int CCustomVongQuay::GetClaimFreeCount(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return 0;
	}

	LPOBJ lpObj = &gObj[aIndex];

	// SpinClaim is only allocated for user objects - it is NULL for monsters.
	if(lpObj->Type != OBJECT_USER || lpObj->SpinClaim == 0)
	{
		return 0;
	}

	int free = 0;

	for(int n=0;n < SPIN_CLAIM_SIZE;n++)
	{
		if(lpObj->SpinClaim[n].IsItem() == 0)
		{
			free++;
		}
	}

	return free;
}

int CCustomVongQuay::PushClaimItem(int aIndex,CItem& item) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return -1;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Type != OBJECT_USER || lpObj->SpinClaim == 0)
	{
		return -1;
	}

	for(int n=0;n < SPIN_CLAIM_SIZE;n++)
	{
		if(lpObj->SpinClaim[n].IsItem() != 0)
		{
			continue;
		}

		lpObj->SpinClaim[n] = item;

		// Save straight away rather than waiting for the 10-minute autosave.
		// Nothing else flushes per-player item state, and a crash in between
		// would lose a prize the player has already paid for.
		GDCharacterInfoSaveSend(aIndex);

		return n;
	}

	return -1;
}

void CCustomVongQuay::ClaimItem(int aIndex,int slot) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Type != OBJECT_USER || lpObj->SpinClaim == 0)
	{
		return;
	}

	// Refuse while a trade / chaos box / shop is open, the same guard the spin
	// itself uses - otherwise an item can be created into a grid another
	// transaction is mid-way through rearranging.
	if(lpObj->Interface.type != INTERFACE_NONE || lpObj->Interface.use != 0 || lpObj->Transaction == 1)
	{
		return;
	}

	const int first = ((slot == 0xFF) ? 0 : slot);
	const int last = ((slot == 0xFF) ? (SPIN_CLAIM_SIZE-1) : slot);

	if(SPIN_CLAIM_RANGE(first) == 0 || SPIN_CLAIM_RANGE(last) == 0)
	{
		return;
	}

	int moved = 0;

	for(int n=first;n <= last;n++)
	{
		CItem* lpItem = &lpObj->SpinClaim[n];

		if(lpItem->IsItem() == 0)
		{
			continue;
		}

		// Per-item space check, not a blanket 4x4 - a 1x1 jewel should still be
		// claimable when only small gaps are left. Checked one at a time because
		// each successful claim changes what fits next.
		if(gItemManager.CheckItemInventorySpace(lpObj,lpItem->m_Index) == false)
		{
			gNotice.GCNoticeSend(aIndex,eMessageBox,0,0,0,0,0,this->GetMessage(11));
			break;
		}

		BYTE ItemSocketOption[MAX_SOCKET_OPTION];

		for(int s=0;s < MAX_SOCKET_OPTION;s++)
		{
			ItemSocketOption[s] = lpItem->m_SocketOption[s];
		}

		// Only now does the item become real and get a serial.
		GDCreateItemSend(aIndex,0xEB,(BYTE)lpObj->X,(BYTE)lpObj->Y,lpItem->m_Index,lpItem->m_Level,
			(BYTE)lpItem->m_Durability,lpItem->m_Option1,lpItem->m_Option2,lpItem->m_Option3,-1,
			lpItem->m_NewOption,lpItem->m_SetOption,lpItem->m_JewelOfHarmonyOption,lpItem->m_ItemOptionEx,
			ItemSocketOption,lpItem->m_SocketOptionBonus,
			((lpItem->m_IsPeriodicItem != 0)?lpItem->m_PeriodicItemTime:0));

		LogAddCat(LOG_CAT_SPINWHEEL,LOG_GREEN,"[SpinWheel] %s claimed %s +%d from the claim box (slot %d)",
			lpObj->Name,gItemLevel.GetItemName(lpItem->m_Index,lpItem->m_Level),lpItem->m_Level,n);

		lpItem->Clear();

		moved++;
	}

	if(moved > 0)
	{
		GDCharacterInfoSaveSend(aIndex);
	}

	// Always resend, even on a refusal - the client's grid should reflect what
	// the server actually holds rather than what it optimistically expected.
	this->SendClaimList(aIndex);
}

void CCustomVongQuay::SendClaimList(int aIndex) // OK
{
	if(OBJECT_RANGE(aIndex) == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Type != OBJECT_USER || lpObj->SpinClaim == 0)
	{
		return;
	}

	if(gObjIsConnected(aIndex) == false)
	{
		return;
	}

	BYTE send[1024];

	PMSG_SPINCLAIM_LIST_SEND pMsg = { 0 };

	pMsg.header.set(0xD3,0x8D,0);

	int size = sizeof(pMsg);

	pMsg.count = 0;

	for(int n=0;n < SPIN_CLAIM_SIZE;n++)
	{
		if(lpObj->SpinClaim[n].IsItem() == 0)
		{
			continue;
		}

		SPINCLAIM_ROW row = { 0 };

		row.slot = (BYTE)n;

		gItemManager.ItemByteConvert(row.Item,lpObj->SpinClaim[n]);

		memcpy(&send[size],&row,sizeof(row));

		size += sizeof(row);

		pMsg.count++;
	}

	pMsg.free = (BYTE)this->GetClaimFreeCount(aIndex);

	pMsg.header.size[0] = SET_NUMBERHB(size);
	pMsg.header.size[1] = SET_NUMBERLB(size);

	memcpy(send,&pMsg,sizeof(pMsg));

	// Sent even when count is 0, so the client can clear a stale grid.
	DataSend(aIndex,send,size);
}

void CCustomVongQuay::CGClaimRecv(int aIndex,BYTE* lpRecv) // OK
{
	if(OBJECT_RANGE(aIndex) == 0 || lpRecv == 0)
	{
		return;
	}

	LPOBJ lpObj = &gObj[aIndex];

	// The 0xD3 dispatch has no login guard of its own, so self-guard exactly the
	// way the wheel's own handlers do.
	if(lpObj->Type != OBJECT_USER || gObjIsConnected(aIndex) == false)
	{
		return;
	}

	if(lpObj->IsBot >= 1 || lpObj->m_OfflineMode != 0 || lpObj->IsFakeOnline != 0)
	{
		return;
	}

	PMSG_SPINCLAIM_CLAIM_RECV* lpMsg = (PMSG_SPINCLAIM_CLAIM_RECV*)lpRecv;

	// Player-supplied slot: 0xFF (claim all) or a real slot, nothing else.
	if(lpMsg->slot != 0xFF && SPIN_CLAIM_RANGE(lpMsg->slot) == 0)
	{
		return;
	}

	this->ClaimItem(aIndex,lpMsg->slot);
}

// ---- Claim box persistence -------------------------------------------------

void CCustomVongQuay::GDSpinClaimSend(int aIndex) // OK
{
	if(gObjIsAccountValid(aIndex,gObj[aIndex].Account) == 0)
	{
		return;
	}

	// Guard against a second load overwriting a box that is already live - the
	// same shape EventInventory uses.
	if(gObj[aIndex].LoadSpinClaim != 0)
	{
		return;
	}

	SDHP_SPINCLAIM_RECV pMsg;

	pMsg.header.set(0xD9,0x32,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,gObj[aIndex].Account,sizeof(pMsg.account));

	memcpy(pMsg.name,gObj[aIndex].Name,sizeof(pMsg.name));

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CCustomVongQuay::DGSpinClaimRecv(SDHP_SPINCLAIM_SEND* lpMsg) // OK
{
	// Re-validate the account against the echoed name before applying anything.
	// Object slots are recycled on login, so without this a reply that arrived
	// late could be applied to whoever now occupies the slot - which for an item
	// store means moving real items between accounts.
	if(gObjIsAccountValid(lpMsg->index,lpMsg->account) == 0)
	{
		LogAdd(LOG_RED,"[DGSpinClaimRecv] Invalid Account [%d](%s)",lpMsg->index,lpMsg->account);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	if(lpObj->Type != OBJECT_USER || lpObj->SpinClaim == 0)
	{
		return;
	}

	if(lpObj->LoadSpinClaim != 0)
	{
		return;
	}

	lpObj->LoadSpinClaim = 1;

	for(int n=0;n < SPIN_CLAIM_SIZE;n++)
	{
		CItem item;

		lpObj->SpinClaim[n].Clear();

		// ConvertItemByte returns 0 for a 0xFF-filled (empty) record.
		if(gItemManager.ConvertItemByte(&item,lpMsg->SpinClaim[n]) != 0)
		{
			lpObj->SpinClaim[n] = item;
		}
	}

	// Only push to the client if it is already in game - on the login path the
	// window is not open yet, and opening it requests the list anyway.
	if(gObjIsConnected(lpMsg->index) != false)
	{
		this->SendClaimList(lpMsg->index);
	}
}

void CCustomVongQuay::GDSpinClaimSaveSend(int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->Type != OBJECT_USER || lpObj->SpinClaim == 0)
	{
		return;
	}

	// Never save a box that was never loaded - that would write 8 blank slots
	// over whatever the database actually holds.
	if(lpObj->LoadSpinClaim == 0)
	{
		return;
	}

	SDHP_SPINCLAIM_SAVE_RECV pMsg;

	pMsg.header.set(0xD9,0x33,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	memcpy(pMsg.name,lpObj->Name,sizeof(pMsg.name));

	for(int n=0;n < SPIN_CLAIM_SIZE;n++)
	{
		gItemManager.DBItemByteConvert(pMsg.SpinClaim[n],&lpObj->SpinClaim[n]);
	}

	gDataServerConnection.DataSend((BYTE*)&pMsg,sizeof(pMsg));
}

// The weighted prize draw, factored out so the spin request and the (defensive)
// grant-time fallback cannot disagree about how a winner is chosen.
int CCustomVongQuay::DrawWheelPrize(DATA_VONGQUAY& wheel)
{
	if (wheel.ListItemNhan.empty())
	{
		return 0;
	}

	CRandomManager RandomMng;
	WORD iIndex = 0;

	for (int n = 0; n < wheel.ListItemNhan.size(); n++)
	{
		RandomMng.AddElement(n, wheel.ListItemNhan[n].Rate);
	}

	RandomMng.GetRandomElement(&iIndex);

	if (iIndex >= wheel.ListItemNhan.size())
	{
		iIndex = 0;
	}

	return (int)iIndex;
}

void CCustomVongQuay::MakeItem(int aIndex,int type)
{
	LPOBJ lpObj = &gObj[aIndex];
	if (type != 1)
	{
		goto Next;
	}
	if (lpObj->SauVongQuay > 0 && GetTickCount() - lpObj->SauVongQuay > 5000)
		Next:
	{
		lpObj->SauVongQuay = 0;

		// Per-player wheel id, not the old shared singleton - see
		// OBJECTSTRUCT::SpinWheelIndex.
		const int WheelIndex = lpObj->SpinWheelIndex;

		lpObj->SpinWheelIndex = 0;

		std::map<int, DATA_VONGQUAY>::iterator it = this->m_DataVongQuay.find(WheelIndex);

		// Mandatory: this dereferenced end() unconditionally before. Reachable
		// without any bad packet - SauVongQuay is never cleared on
		// connect/disconnect, so a player who dropped inside the 5s window left
		// it set in that object slot and the next character to occupy the slot
		// got a free unpaid draw on the next tick. With no wheel id recorded for
		// them, find() misses and this crashed the whole GameServer.
		if (it == this->m_DataVongQuay.end())
		{
			return;
		}

		// The prize was already drawn in ActionVongQuay and sent to the client so
		// its wheel could animate onto it - do NOT draw again here, or the item
		// handed out would not be the one the pointer landed on.
		WORD iIndex = 0;

		if (lpObj->SpinWheelWin >= 0 && lpObj->SpinWheelWin < (int)it->second.ListItemNhan.size())
		{
			iIndex = (WORD)lpObj->SpinWheelWin;
		}
		else
		{
			// No valid pre-draw (shouldn't happen - defensive only). Draw now so a
			// paid spin still pays out rather than silently awarding slot 0.
			iIndex = (WORD)this->DrawWheelPrize(it->second);
		}

		lpObj->SpinWheelWin = -1;

		BYTE ItemSocketOption[MAX_SOCKET_OPTION] = { it->second.ListItemNhan[iIndex].SK[0], it->second.ListItemNhan[iIndex].SK[1], it->second.ListItemNhan[iIndex].SK[2], it->second.ListItemNhan[iIndex].SK[3], it->second.ListItemNhan[iIndex].SK[4] };
		time_t t = time(NULL);
		localtime(&t);
		DWORD iTime = (DWORD)t + it->second.ListItemNhan[iIndex].HSD * 60;

		DATA_VONGQUAYITEM& prize = it->second.ListItemNhan[iIndex];

		// EVERY prize goes to the Rewards box - never straight into the inventory.
		// The player claims it by hand from the wheel window.
		//
		// This is deliberate design, not a fallback: the prize is only revealed
		// when the wheel stops, and dropping it silently into the grid at that
		// moment gave the player nothing to react to. It also removes the whole
		// class of failure this feature started from - there is no longer any
		// path where a won prize can be destroyed by a full inventory, because
		// nothing is inserted into the inventory at grant time at all.
		//
		// The box holding 8 slots is not a loss risk either: ActionVongQuay
		// refuses the spin (before charging) when the box is full, so a prize can
		// never arrive with nowhere to go.
		CItem hold;

		hold.m_Level = prize.LvItem;
		hold.m_Durability = (float)prize.Dur;

		hold.Convert(prize.IndexItem,prize.Skill,prize.Luck,prize.Opt,prize.Exc,prize.Anc,0,0,ItemSocketOption,prize.SKBonus);

		hold.m_IsPeriodicItem = ((prize.HSD > 0)?1:0);
		hold.m_LoadPeriodicItem = ((prize.HSD > 0)?1:0);
		hold.m_PeriodicItemTime = ((prize.HSD > 0)?iTime:0);

		if(this->PushClaimItem(lpObj->Index,hold) >= 0)
		{
			this->SendClaimList(lpObj->Index);
		}
		else
		{
			// Should be unreachable - the spin was refused up front if the box was
			// full. Logged rather than swallowed in case that guard ever regresses.
			LogAddCat(LOG_CAT_SPINWHEEL,LOG_RED,"[SpinWheel] %s LOST %s +%d - Rewards box was full at grant time",
				lpObj->Name,gItemLevel.GetItemName(prize.IndexItem,prize.LvItem),prize.LvItem);

			gNotice.GCNoticeSend(lpObj->Index,eMessageBox,0,0,0,0,0,this->GetMessage(9));
		}

		// The weighted draw's outcome exists nowhere else - this is the only
		// record of what a spin actually awarded, which is the whole point of
		// the Logs > SpinWheel Logs view.
		LogAddCat(LOG_CAT_SPINWHEEL, LOG_GREEN, "[SpinWheel] %s won %s +%d (wheel %d, slot %d)",
			lpObj->Name,
			gItemLevel.GetItemName(it->second.ListItemNhan[iIndex].IndexItem, it->second.ListItemNhan[iIndex].LvItem),
			it->second.ListItemNhan[iIndex].LvItem,
			WheelIndex, iIndex);

		//==Send Effect
		if (this->Firework == 1)
		{
			GCServerCommandSend(lpObj->Index, 0, lpObj->X, lpObj->Y);
		}
		else if (this->Firework == 2)
		{
			GCServerCommandSend(lpObj->Index, 2, lpObj->X, lpObj->Y);
		}
		else if (this->Firework == 3)
		{
			GCServerCommandSend(lpObj->Index, 58, SET_NUMBERHB(lpObj->Index), SET_NUMBERLB(lpObj->Index));
		}
		char tmp[255];
		char tmp2[255];
		wsprintf(tmp, this->GetMessage(5), lpObj->Name, gItemLevel.GetItemName(it->second.ListItemNhan[iIndex].IndexItem, it->second.ListItemNhan[iIndex].LvItem));
		if (this->Notice == 1) { //Thong Bao trong Sub
			gNotice.GCNoticeSend(lpObj->Index, 0, 0, 0, 0, 0, 0, tmp);
		}
		else if (this->Notice == 2)
		{ //Thong Bao Toan Sub

			GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, tmp);
		}
		else if (this->Notice == 3)
		{ //Thong Bao Toan Sub
			wsprintf(tmp2, "%s %s", gServerInfo.m_ServerName, tmp);
			GDGlobalNoticeSend(gMapServerManager.GetMapServerGroup(), 0, 0, 0, 0, 0, 0, tmp2);
		}

		XULY_CGPACKET_VONGQUAY pMsg;
		pMsg.header.set(0xD3, 0x8C, sizeof(pMsg));
		pMsg.StartRoll = 0;	//Clear Item Cache O Client
		pMsg.IndexWin = iIndex;
		DataSend(lpObj->Index, (BYTE*)&pMsg, pMsg.header.size);
	}
}

void CCustomVongQuay::ActionVongQuay(int aIndex, int MocNap , int solan)
{
	if (!this->Enable)
	{
		gNotice.GCNoticeSend(aIndex, 1, 0, 0, 0, 0, 0, this->GetMessage(0)); //
		return;
	}

	if (gObj[aIndex].Type != OBJECT_USER)
	{
		return;
	}

	

	if (gObjIsConnected(aIndex) == false)
	{
		return;
	}
	if (gObj[aIndex].IsBot >= 1 || gObj[aIndex].m_OfflineMode != 0 || gObj[aIndex].IsFakeOnline != 0)
	{
		return;
	}
	LPOBJ lpObj = &gObj[aIndex];

	//if ((GetTickCount() - lpObj->ClickClientSend) < 2000)
	//{
	//	gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, this->GetMessage(8));
	//	return;
	//}

	//===KIem tra trang thai co duoc add item khong
	if (lpObj->Interface.type != INTERFACE_NONE || lpObj->Interface.use != 0 || lpObj->Transaction == 1 )
	{
		return;
	}

	if (gItemManager.ChaosBoxHasItem(lpObj) || gItemManager.TradeHasItem(lpObj))
	{
		return;
	}
	//===================================================

	
	//===Lay Thong Tin Moc Nap
	std::map<int, DATA_VONGQUAY>::iterator it = this->m_DataVongQuay.find(MocNap);
	// Deliberately NOT recorded yet - the old code stored the wheel id here,
	// before every validity, currency and space check below, so even a rejected
	// spin overwrote it. It is committed further down, once the spin is going
	// ahead.
	if (it == this->m_DataVongQuay.end())
	{
		gNotice.GCNoticeSend(aIndex, eMessageBox, 0, 0, 0, 0, 0, this->GetMessage(1)); //Khong co thong tin cua moc nap
		return;
	}
	if (it->second.WC > lpObj->Coin1)
	{
		gNotice.GCNoticeSend(aIndex, eMessageBox, 0, 0, 0, 0, 0, this->GetMessage(3), NumberFormat(it->second.WC)); //Yeu cau gia tri nap
		return;
	}

	if (it->second.WP > lpObj->Coin2)
	{
		gNotice.GCNoticeSend(aIndex, eMessageBox, 0, 0, 0, 0, 0, this->GetMessage(3), NumberFormat(it->second.WP)); //Yeu cau gia tri nap
		return;
	}

	if (it->second.GP > lpObj->Coin3)
	{
		gNotice.GCNoticeSend(aIndex, eMessageBox, 0, 0, 0, 0, 0, this->GetMessage(3), NumberFormat(it->second.GP)); //Yeu cau gia tri nap
		return;
	}

	// Entrance-fee items: only COUNT them here. The actual deletion moved below
	// the inventory-space check - it used to run first, so a wheel with an item
	// cost destroyed the fee and could then still refuse the spin on the space
	// check, with no refund and no log line.
	if (it->second.Count > 0)
	{
		int count = gItemManager.GetInventoryItemCount(lpObj, it->second.IndexItemYC, 0);
		if (count < it->second.Count)
		{
			gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, this->GetMessage(7), it->second.Count, gItemManager.GetItemName(it->second.IndexItemYC));
			return ;
		}
	}


	// Prizes now land in the Rewards box, not the inventory, so the inventory's
	// free space is irrelevant here - what matters is whether the box has room.
	// Refused BEFORE the charge below, so a player can never pay for a spin whose
	// prize would have nowhere to go.
	if (this->GetClaimFreeCount(aIndex) <= 0)
	{
		gNotice.GCNoticeSend(lpObj->Index, eMessageBox, 0, 0, 0, 0, 0, this->GetMessage(10));
		return;
	}

	// Past every check - the spin is definitely happening, so now take the fee
	// and commit the wheel id the deferred draw will read.
	if (it->second.Count > 0)
	{
		gItemManager.DeleteInventoryItemCount(lpObj, it->second.IndexItemYC, 0, it->second.Count);
	}

	lpObj->SpinWheelIndex = MocNap;

	// Draw the prize NOW, not when it is handed out 5 seconds later, and tell the
	// client below. That is what lets the wheel decelerate onto the real winning
	// segment across the whole window instead of only finding out the result
	// after the item, firework and global notice have already been delivered.
	lpObj->SpinWheelWin = this->DrawWheelPrize(it->second);


	
	
	//===============Cong Coin
	if (it->second.WC > 0 || it->second.WP > 0 || it->second.GP > 0)
	{
		GDSetCoinSend(lpObj->Index, -it->second.WC, -it->second.WP, -it->second.GP, "Tru Coin Vong Quay");
	}
	
	

	XULY_CGPACKET_VONGQUAY pMsg;
	pMsg.header.set(0xD3, 0x8C, sizeof(pMsg));
	pMsg.StartRoll = 1;	//Clear Item Cache O Client
	// Was -1 ("unknown yet"). Now carries the already-drawn winning slot so the
	// client can spin down onto it. The grant is still entirely server-side and
	// still happens on the server's own schedule - this is only the target the
	// animation aims at.
	pMsg.IndexWin = lpObj->SpinWheelWin;
	DataSend(lpObj->Index, (BYTE*)&pMsg, pMsg.header.size);
	if (solan == 1)
	{
		lpObj->SauVongQuay = GetTickCount();
	}
	else
	{
		this->MakeItem(lpObj->Index,2);
	}
	//lpObj->ClickClientSend = GetTickCount();
}


