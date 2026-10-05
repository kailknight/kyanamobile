// FriendMail.cpp: modern friend window data and item mail, DataServer side.
//
// Every name and subject reaches SQL as a bound parameter, never through the
// query text: ExecQuery formats with plain sprintf and no escaping
// (QueryManager.cpp), and these strings come from players.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FriendMail.h"
#include "CharacterManager.h"
#include "CSProtocol.h"
#include "QueryManager.h"
#include "ServerManager.h"
#include "SocketManager.h"
#include "Util.h"

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
namespace
{
	// Copies a fixed-size, possibly unterminated wire string into a buffer
	// that is always terminated.
	void CopyName(char* out,int outSize,const char* in,int inSize)
	{
		memset(out,0,outSize);
		int n = (inSize < outSize - 1) ? inSize : (outSize - 1);
		memcpy(out,in,n);
		out[outSize - 1] = 0;
	}

	bool IsEmptyName(const char* name)
	{
		return name == NULL || name[0] == 0;
	}
}

DWORD CFriendMail::GetGuid(const char* name)
{
	char bound[11];
	CopyName(bound,sizeof(bound),name,10);

	DWORD guid = 0;

	gQueryManager.BindParameterAsString(1,bound,sizeof(bound));

	if(gQueryManager.ExecQuery("SELECT GUID FROM T_FriendMain WHERE Name=?") != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		int value = gQueryManager.GetAsInteger("GUID");
		guid = (value > 0) ? (DWORD)value : 0;
	}

	gQueryManager.Close();

	return guid;
}

void CFriendMail::CompleteFriendship(const char* acceptor,const char* requester)
{
	DWORD acceptorGuid = this->GetGuid(acceptor);
	DWORD requesterGuid = this->GetGuid(requester);

	if(acceptorGuid == 0 || requesterGuid == 0)
	{
		return;
	}

	gQueryManager.ExecQuery("UPDATE T_FriendList SET Del=0 WHERE GUID=%d AND FriendGuid=%d",requesterGuid,acceptorGuid);
	gQueryManager.Close();
}

// -----------------------------------------------------------------------------
// 0x40 - level, master level, resets, class and guild of every friend
// -----------------------------------------------------------------------------

void CFriendMail::OnDetailRequest(SDHP_FRIENDMAIL_NAME_REQ* lpMsg,int index)
{
	SDHP_FRIENDMAIL_DETAIL_RECV pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.aIndex = lpMsg->aIndex;
	memcpy(pMsg.Account,lpMsg->Account,sizeof(pMsg.Account));

	DWORD guid = this->GetGuid(lpMsg->Name);

	if(guid != 0 && gQueryManager.ExecQuery(
		"SELECT f.FriendName, f.Del, c.Class, c.cLevel, c.ResetCount, ISNULL(m.MasterLevel,0) AS MasterLevel, ISNULL(g.G_Name,'') AS G_Name "
		"FROM T_FriendList f "
		"LEFT JOIN Character c ON c.Name = f.FriendName "
		"LEFT JOIN MasterSkillTree m ON m.Name = f.FriendName "
		"LEFT JOIN GuildMember g ON g.Name = f.FriendName "
		"WHERE f.GUID=%d",guid) != 0)
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA && pMsg.Count < FRIENDMAIL_MAX_DETAILS)
		{
			FRIENDMAIL_DETAIL& d = pMsg.List[pMsg.Count];

			gQueryManager.GetAsString("FriendName",d.Name,sizeof(d.Name));

			// A friend whose character was deleted has no Character row - the
			// LEFT JOIN leaves its columns NULL, which read as negative here.
			int cls = gQueryManager.GetAsInteger("Class");
			int level = gQueryManager.GetAsInteger("cLevel");
			int resets = gQueryManager.GetAsInteger("ResetCount");
			int master = gQueryManager.GetAsInteger("MasterLevel");

			d.Class = (BYTE)((cls < 0) ? 0xFF : cls);
			d.Level = (WORD)((level < 0) ? 0 : level);
			d.Resets = (WORD)((resets < 0) ? 0 : resets);
			d.MasterLevel = (WORD)((master < 0) ? 0 : master);

			gQueryManager.GetAsString("G_Name",d.Guild,sizeof(d.Guild));

			// Del != 0 is a one-sided entry: the request was never accepted. The
			// legacy list shows those as offline, and item mail refuses them.
			d.Pending = (gQueryManager.GetAsInteger("Del") != 0) ? 1 : 0;

			pMsg.Count++;
		}
	}

	gQueryManager.Close();

	int size = (int)(sizeof(pMsg) - sizeof(pMsg.List) + (pMsg.Count * sizeof(FRIENDMAIL_DETAIL)));
	pMsg.header.set(0xD9,0x40,size);

	gSocketManager.DataSend(index,(BYTE*)&pMsg,size);
}

// -----------------------------------------------------------------------------
// 0x41 - may this letter be sent? Read-only.
// -----------------------------------------------------------------------------

void CFriendMail::OnCheckRequest(SDHP_FRIENDMAIL_CHECK_REQ* lpMsg,int index)
{
	SDHP_FRIENDMAIL_CHECK_RECV pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x41,sizeof(pMsg));
	pMsg.aIndex = lpMsg->aIndex;
	memcpy(pMsg.Account,lpMsg->Account,sizeof(pMsg.Account));
	memcpy(pMsg.ToName,lpMsg->ToName,sizeof(pMsg.ToName));
	pMsg.Result = FRIENDMAIL_NOT_FRIEND;

	char toName[11];
	CopyName(toName,sizeof(toName),lpMsg->ToName,10);

	DWORD guid = this->GetGuid(lpMsg->Name);

	if(guid != 0 && IsEmptyName(toName) == false)
	{
		// Friends only: the recipient must be on the sender's list, and not
		// marked deleted on it.
		gQueryManager.BindParameterAsString(1,toName,sizeof(toName));

		int onList = 0;

		if(gQueryManager.ExecQuery("SELECT COUNT(*) AS N FROM T_FriendList WHERE GUID=%d AND FriendName=? AND Del=0",guid) != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
		{
			onList = gQueryManager.GetAsInteger("N");
		}

		gQueryManager.Close();

		int exists = 0;

		if(onList > 0)
		{
			gQueryManager.BindParameterAsString(1,toName,sizeof(toName));

			if(gQueryManager.ExecQuery("SELECT COUNT(*) AS N FROM Character WHERE Name=?") != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
			{
				exists = gQueryManager.GetAsInteger("N");
			}

			gQueryManager.Close();
		}

		if(onList > 0 && exists > 0)
		{
			// The recipient needs a T_FriendMain row (a mailbox) for the letter
			// to land in - normally made the first time they log in.
			gQueryManager.BindParameterAsString(1,toName,sizeof(toName));
			gQueryManager.ExecQuery("EXEC WZ_UserGuidCreate ?");
			gQueryManager.Fetch();
			gQueryManager.Close();

			pMsg.Result = FRIENDMAIL_OK;
		}
	}

	gSocketManager.DataSend(index,(BYTE*)&pMsg,pMsg.header.size);
}

// -----------------------------------------------------------------------------
// 0x42 - store a letter and its items. Never refuses: the items are already
// gone from the sender's inventory.
// -----------------------------------------------------------------------------

void CFriendMail::OnStoreRequest(SDHP_FRIENDMAIL_STORE_REQ* lpMsg,int index)
{
	SDHP_FRIENDMAIL_STORE_RECV pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x42,sizeof(pMsg));
	pMsg.aIndex = lpMsg->aIndex;
	memcpy(pMsg.Account,lpMsg->Account,sizeof(pMsg.Account));
	memcpy(pMsg.ToName,lpMsg->ToName,sizeof(pMsg.ToName));
	pMsg.Result = FRIENDMAIL_ERROR;

	char name[11];
	char toName[11];
	char subject[33];
	CopyName(name,sizeof(name),lpMsg->Name,10);
	CopyName(toName,sizeof(toName),lpMsg->ToName,10);
	CopyName(subject,sizeof(subject),lpMsg->Subject,32);

	int itemCount = (lpMsg->ItemCount > FRIENDMAIL_MAX_ITEMS) ? FRIENDMAIL_MAX_ITEMS : lpMsg->ItemCount;
	pMsg.ItemCount = (BYTE)itemCount;

	BYTE memo[FRIENDMAIL_MEMO_MAX];
	memset(memo,0,sizeof(memo));
	int memoSize = (lpMsg->MemoSize > FRIENDMAIL_MEMO_MAX) ? FRIENDMAIL_MEMO_MAX : lpMsg->MemoSize;
	if(memoSize > 0)
	{
		memcpy(memo,lpMsg->Memo,memoSize);
	}

	// Into the recipient's mailbox. WZ_WriteMail returns the new letter's
	// MemoIndex and the mailbox owner's GUID, or an error code (<= 10) - a
	// full mailbox, for one.
	DWORD memoIndex = 0;
	DWORD guid = 0;
	bool returned = false;

	gQueryManager.BindParameterAsString(1,name,sizeof(name));
	gQueryManager.BindParameterAsString(2,toName,sizeof(toName));
	gQueryManager.BindParameterAsString(3,subject,sizeof(subject));
	gQueryManager.ExecQuery("EXEC WZ_WriteMail ?,?,?,%d,%d",lpMsg->Dir,lpMsg->Action);
	gQueryManager.Fetch();
	memoIndex = gQueryManager.GetResult(0);
	guid = gQueryManager.GetResult(1);
	gQueryManager.Close();

	if(memoIndex <= 10 || guid == 0)
	{
		// Refused. The items are already out of the sender's inventory, so
		// they go to the sender's own mailbox rather than nowhere.
		LogAdd(LOG_RED,"[FriendMail] %s -> %s refused (code %d), returning %d item(s) to the sender",name,toName,(int)memoIndex,itemCount);

		char back[33];
		memset(back,0,sizeof(back));
		_snprintf_s(back,sizeof(back),_TRUNCATE,"Undelivered: %s",subject);

		gQueryManager.BindParameterAsString(1,name,sizeof(name));
		gQueryManager.BindParameterAsString(2,name,sizeof(name));
		gQueryManager.BindParameterAsString(3,back,sizeof(back));
		gQueryManager.ExecQuery("EXEC WZ_WriteMail ?,?,?,%d,%d",lpMsg->Dir,lpMsg->Action);
		gQueryManager.Fetch();
		memoIndex = gQueryManager.GetResult(0);
		guid = gQueryManager.GetResult(1);
		gQueryManager.Close();

		returned = true;

		if(memoIndex <= 10 || guid == 0)
		{
			// Both mailboxes refused. Nothing left to store into - the item
			// bytes go to a log an admin can restore from (Titan Editor).
			this->LogLostItems("both mailboxes refused",name,lpMsg);
			gSocketManager.DataSend(index,(BYTE*)&pMsg,pMsg.header.size);
			return;
		}
	}

	gQueryManager.BindParameterAsBinary(1,memo,sizeof(memo));
	gQueryManager.BindParameterAsBinary(2,lpMsg->Photo,sizeof(lpMsg->Photo));
	gQueryManager.ExecQuery("UPDATE T_FriendMail SET Memo=?,Photo=? WHERE MemoIndex=%d AND GUID=%d",memoIndex,guid);
	gQueryManager.Close();

	// Returned items expire nowhere: they are already back with their owner.
	int expireDays = returned ? 0 : lpMsg->ExpireDays;

	for(int n = 0; n < itemCount; n++)
	{
		BYTE item[FRIENDMAIL_ITEM_BYTES];
		memcpy(item,lpMsg->Items[n],sizeof(item));

		gQueryManager.BindParameterAsString(1,name,sizeof(name));
		gQueryManager.BindParameterAsBinary(2,item,sizeof(item));

		int stored = 0;

		if(gQueryManager.ExecQuery("EXEC WZ_MailItem_Add %d,%d,?,?,%d",guid,memoIndex,expireDays) != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
		{
			stored = gQueryManager.GetAsInteger("Result");
		}

		gQueryManager.Close();

		if(stored != 1)
		{
			SDHP_FRIENDMAIL_STORE_REQ one = *lpMsg;
			one.ItemCount = 1;
			memcpy(one.Items[0],item,sizeof(item));
			this->LogLostItems("WZ_MailItem_Add failed",name,&one);
		}
	}

	// The coins, as one more attachment row. Already taken from the sender.
	if(lpMsg->CoinType != FRIENDMAIL_COIN_NONE && lpMsg->CoinAmount > 0)
	{
		BYTE empty[FRIENDMAIL_ITEM_BYTES];
		memset(empty,0xFF,sizeof(empty));

		gQueryManager.BindParameterAsString(1,name,sizeof(name));
		gQueryManager.BindParameterAsBinary(2,empty,sizeof(empty));

		int stored = 0;

		if(gQueryManager.ExecQuery("EXEC WZ_MailItem_Add %d,%d,?,?,%d,%d,%d",guid,memoIndex,expireDays,(int)lpMsg->CoinType,(int)lpMsg->CoinAmount) != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
		{
			stored = gQueryManager.GetAsInteger("Result");
		}

		gQueryManager.Close();

		if(stored != 1)
		{
			SDHP_FRIENDMAIL_STORE_REQ one = *lpMsg;
			one.ItemCount = 0;
			this->LogLostItems("WZ_MailItem_Add failed (coins)",name,&one);
		}
	}

	pMsg.Result = returned ? FRIENDMAIL_RETURNED : FRIENDMAIL_OK;

	LogAdd(LOG_BLACK,"[FriendMail] %s -> %s letter %d, %d item(s), coin type %d x %u%s",name,toName,(int)memoIndex,itemCount,(int)lpMsg->CoinType,lpMsg->CoinAmount,returned ? " (returned to sender)" : "");

	gSocketManager.DataSend(index,(BYTE*)&pMsg,pMsg.header.size);

	this->NotifyRecipient(returned ? name : toName,memoIndex,guid);
}

// -----------------------------------------------------------------------------
// 0x43 - a letter's waiting items
// -----------------------------------------------------------------------------

void CFriendMail::OnItemsRequest(SDHP_FRIENDMAIL_MEMO_REQ* lpMsg,int index)
{
	SDHP_FRIENDMAIL_ITEMS_RECV pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x43,sizeof(pMsg));
	pMsg.aIndex = lpMsg->aIndex;
	memcpy(pMsg.Account,lpMsg->Account,sizeof(pMsg.Account));
	memcpy(pMsg.Name,lpMsg->Name,sizeof(pMsg.Name));
	pMsg.MemoIndex = lpMsg->MemoIndex;

	DWORD guid = this->GetGuid(lpMsg->Name);

	if(guid != 0 && gQueryManager.ExecQuery("SELECT ItemID,ItemData,CoinType,CoinAmount FROM T_FriendMailItem WHERE GUID=%d AND MemoIndex=%d AND Status=0 ORDER BY ItemID",guid,lpMsg->MemoIndex) != 0)
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA && pMsg.Count < FRIENDMAIL_MAX_ATTACH)
		{
			FRIENDMAIL_ITEM& item = pMsg.Items[pMsg.Count];
			item.ItemID = gQueryManager.GetAsInteger64("ItemID");
			gQueryManager.GetAsBinary("ItemData",item.ItemData,sizeof(item.ItemData));
			item.CoinType = (BYTE)gQueryManager.GetAsInteger("CoinType");
			item.CoinAmount = (DWORD)gQueryManager.GetAsInteger("CoinAmount");
			pMsg.Count++;
		}
	}

	gQueryManager.Close();

	gSocketManager.DataSend(index,(BYTE*)&pMsg,pMsg.header.size);
}

// -----------------------------------------------------------------------------
// 0x44 - claim a letter's waiting items. The reply lists only what THIS call
// took; anything already claimed elsewhere is simply absent.
// -----------------------------------------------------------------------------

void CFriendMail::OnClaimRequest(SDHP_FRIENDMAIL_MEMO_REQ* lpMsg,int index)
{
	SDHP_FRIENDMAIL_ITEMS_RECV pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.header.set(0xD9,0x44,sizeof(pMsg));
	pMsg.aIndex = lpMsg->aIndex;
	memcpy(pMsg.Account,lpMsg->Account,sizeof(pMsg.Account));
	memcpy(pMsg.Name,lpMsg->Name,sizeof(pMsg.Name));
	pMsg.MemoIndex = lpMsg->MemoIndex;

	char name[11];
	CopyName(name,sizeof(name),lpMsg->Name,10);

	DWORD guid = this->GetGuid(name);

	__int64 ids[FRIENDMAIL_MAX_ATTACH];
	int idCount = 0;

	// Read the candidates first: the connection runs one statement at a time,
	// so the claims cannot be issued while this result set is still open.
	if(guid != 0 && gQueryManager.ExecQuery("SELECT ItemID FROM T_FriendMailItem WHERE GUID=%d AND MemoIndex=%d AND Status=0 ORDER BY ItemID",guid,lpMsg->MemoIndex) != 0)
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA && idCount < FRIENDMAIL_MAX_ATTACH)
		{
			ids[idCount++] = gQueryManager.GetAsInteger64("ItemID");
		}
	}

	gQueryManager.Close();

	for(int n = 0; n < idCount; n++)
	{
		gQueryManager.BindParameterAsString(1,name,sizeof(name));

		if(gQueryManager.ExecQuery("EXEC WZ_MailItem_Claim %d,%I64d,?",guid,ids[n]) != 0
			&& gQueryManager.Fetch() != SQL_NO_DATA
			&& gQueryManager.GetAsInteger("Result") == 1)
		{
			FRIENDMAIL_ITEM& item = pMsg.Items[pMsg.Count];
			item.ItemID = ids[n];
			gQueryManager.GetAsBinary("ItemData",item.ItemData,sizeof(item.ItemData));
			item.CoinType = (BYTE)gQueryManager.GetAsInteger("CoinType");
			item.CoinAmount = (DWORD)gQueryManager.GetAsInteger("CoinAmount");
			pMsg.Count++;
		}

		gQueryManager.Close();
	}

	if(pMsg.Count > 0)
	{
		LogAdd(LOG_BLACK,"[FriendMail] %s claimed %d item(s) from letter %d",name,pMsg.Count,(int)lpMsg->MemoIndex);
	}

	gSocketManager.DataSend(index,(BYTE*)&pMsg,pMsg.header.size);
}

// -----------------------------------------------------------------------------
// 0x45 - undo a claim the GameServer could not complete
// -----------------------------------------------------------------------------

void CFriendMail::OnUnclaimRequest(SDHP_FRIENDMAIL_UNCLAIM_REQ* lpMsg)
{
	char name[11];
	CopyName(name,sizeof(name),lpMsg->Name,10);

	DWORD guid = this->GetGuid(name);

	if(guid == 0)
	{
		return;
	}

	int count = (lpMsg->Count > FRIENDMAIL_MAX_ATTACH) ? FRIENDMAIL_MAX_ATTACH : lpMsg->Count;

	for(int n = 0; n < count; n++)
	{
		gQueryManager.BindParameterAsString(1,name,sizeof(name));
		gQueryManager.ExecQuery("EXEC WZ_MailItem_Unclaim %d,%I64d,?",guid,lpMsg->ItemID[n]);
		gQueryManager.Fetch();
		gQueryManager.Close();
	}

	LogAdd(LOG_RED,"[FriendMail] %s - %d claimed item(s) put back in the mailbox (claim not completed)",name,count);
}

// -----------------------------------------------------------------------------
// 0x46 - waiting-item count per letter. Also runs the expiry sweep: it is
// requested at every login, which is often enough without a timer.
// -----------------------------------------------------------------------------

void CFriendMail::OnCountsRequest(SDHP_FRIENDMAIL_NAME_REQ* lpMsg,int index)
{
	this->ReturnExpiredItems();

	DWORD guid = this->GetGuid(lpMsg->Name);

	this->SendCounts(index,lpMsg->aIndex,lpMsg->Account,guid);
}

void CFriendMail::SendCounts(int index,WORD aIndex,const char* account,DWORD guid)
{
	SDHP_FRIENDMAIL_COUNTS_RECV pMsg;
	memset(&pMsg,0,sizeof(pMsg));
	pMsg.aIndex = aIndex;
	memcpy(pMsg.Account,account,sizeof(pMsg.Account));

	if(guid != 0 && gQueryManager.ExecQuery("SELECT MemoIndex, COUNT(*) AS N FROM T_FriendMailItem WHERE GUID=%d AND Status=0 GROUP BY MemoIndex",guid) != 0)
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA && pMsg.Count < FRIENDMAIL_MAX_COUNTS)
		{
			pMsg.List[pMsg.Count].MemoIndex = (DWORD)gQueryManager.GetAsInteger("MemoIndex");
			pMsg.List[pMsg.Count].Count = (BYTE)gQueryManager.GetAsInteger("N");
			pMsg.Count++;
		}
	}

	gQueryManager.Close();

	int size = (int)(sizeof(pMsg) - sizeof(pMsg.List) + (pMsg.Count * sizeof(FRIENDMAIL_COUNT)));
	pMsg.header.set(0xD9,0x46,size);

	gSocketManager.DataSend(index,(BYTE*)&pMsg,size);
}

int CFriendMail::CountWaitingItems(const char* name,DWORD memoIndex)
{
	DWORD guid = this->GetGuid(name);

	if(guid == 0)
	{
		return 0;
	}

	int count = 0;

	if(gQueryManager.ExecQuery("SELECT COUNT(*) AS N FROM T_FriendMailItem WHERE GUID=%d AND MemoIndex=%d AND Status=0",guid,memoIndex) != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		count = gQueryManager.GetAsInteger("N");
	}

	gQueryManager.Close();

	return (count > 0) ? count : 0;
}

// -----------------------------------------------------------------------------
// Unclaimed items past their expiry go back to the sender, in a new letter.
// Admin mail has no expiry, and returned items get none either.
// -----------------------------------------------------------------------------

void CFriendMail::ReturnExpiredItems()
{
	struct EXPIRED
	{
		__int64 ItemID;
		DWORD Guid;
		char Sender[11];
		char Recipient[11];
	};

	EXPIRED rows[200];
	int rowCount = 0;

	if(gQueryManager.ExecQuery(
		"SELECT TOP 200 i.ItemID, i.GUID, i.SenderName, m.Name AS RecvName "
		"FROM T_FriendMailItem i JOIN T_FriendMain m ON m.GUID = i.GUID "
		"WHERE i.Status=0 AND i.ExpireAt IS NOT NULL AND i.ExpireAt < GETDATE() "
		"ORDER BY i.GUID, i.SenderName") != 0)
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA && rowCount < 200)
		{
			EXPIRED& row = rows[rowCount];
			memset(&row,0,sizeof(row));
			row.ItemID = gQueryManager.GetAsInteger64("ItemID");
			row.Guid = (DWORD)gQueryManager.GetAsInteger("GUID");
			gQueryManager.GetAsString("SenderName",row.Sender,sizeof(row.Sender));
			gQueryManager.GetAsString("RecvName",row.Recipient,sizeof(row.Recipient));
			rowCount++;
		}
	}

	gQueryManager.Close();

	// One returned letter per (mailbox, sender) group.
	for(int start = 0; start < rowCount; )
	{
		int end = start + 1;

		while(end < rowCount && rows[end].Guid == rows[start].Guid && strcmp(rows[end].Sender,rows[start].Sender) == 0)
		{
			end++;
		}

		char recipient[11];
		char sender[11];
		char subject[33];
		CopyName(recipient,sizeof(recipient),rows[start].Recipient,10);
		CopyName(sender,sizeof(sender),rows[start].Sender,10);
		CopyName(subject,sizeof(subject),"Returned items",32);

		gQueryManager.BindParameterAsString(1,recipient,sizeof(recipient));
		gQueryManager.BindParameterAsString(2,sender,sizeof(sender));
		gQueryManager.BindParameterAsString(3,subject,sizeof(subject));
		gQueryManager.ExecQuery("EXEC WZ_WriteMail ?,?,?,0,0");
		gQueryManager.Fetch();
		DWORD memoIndex = gQueryManager.GetResult(0);
		DWORD guid = gQueryManager.GetResult(1);
		gQueryManager.Close();

		if(memoIndex > 10 && guid != 0)
		{
			BYTE memo[FRIENDMAIL_MEMO_MAX];
			memset(memo,0,sizeof(memo));
			_snprintf_s((char*)memo,sizeof(memo),_TRUNCATE,"%s did not claim these items in time, so they came back to you.",recipient);

			BYTE photo[18];
			memset(photo,0,sizeof(photo));

			gQueryManager.BindParameterAsBinary(1,memo,sizeof(memo));
			gQueryManager.BindParameterAsBinary(2,photo,sizeof(photo));
			gQueryManager.ExecQuery("UPDATE T_FriendMail SET Memo=?,Photo=? WHERE MemoIndex=%d AND GUID=%d",memoIndex,guid);
			gQueryManager.Close();

			for(int n = start; n < end; n++)
			{
				gQueryManager.BindParameterAsString(1,recipient,sizeof(recipient));
				gQueryManager.ExecQuery("UPDATE T_FriendMailItem SET GUID=%d, MemoIndex=%d, SenderName=?, ExpireAt=NULL, CreatedAt=GETDATE() WHERE ItemID=%I64d AND Status=0",guid,memoIndex,rows[n].ItemID);
				gQueryManager.Close();
			}

			LogAdd(LOG_BLACK,"[FriendMail] %d unclaimed item(s) returned from %s to %s",end - start,recipient,sender);

			this->NotifyRecipient(sender,memoIndex,guid);
		}
		// else: the sender's mailbox is full. The items stay where they are
		// and the next sweep tries again.

		start = end;
	}
}

// -----------------------------------------------------------------------------
// A letter with items just landed in someone's mailbox: if they are online,
// show it in their list now (the legacy 0x71 entry) and send fresh counts.
// -----------------------------------------------------------------------------

void CFriendMail::NotifyRecipient(const char* toName,DWORD memoIndex,DWORD guid)
{
	char name[11];
	CopyName(name,sizeof(name),toName,10);

	CHARACTER_INFO CharacterInfo;

	if(gCharacterManager.GetCharacterInfo(&CharacterInfo,name) == 0)
	{
		return;
	}

	CServerManager* lpServerManager = FindServerByCode(CharacterInfo.GameServerCode);

	if(lpServerManager == 0)
	{
		return;
	}

	FHP_FRIEND_MEMO_LIST pList;
	memset(&pList,0,sizeof(pList));
	pList.h.set(0x71,sizeof(pList));
	pList.Number = CharacterInfo.UserIndex;
	pList.MemoIndex = (WORD)memoIndex;
	memcpy(pList.RecvName,name,sizeof(pList.RecvName));

	bool found = false;

	if(gQueryManager.ExecQuery("SELECT FriendName,wDate,Subject,bRead FROM T_FriendMail WHERE MemoIndex=%d AND GUID=%d",memoIndex,guid) != 0 && gQueryManager.Fetch() != SQL_NO_DATA)
	{
		gQueryManager.GetAsString("FriendName",pList.SendName,sizeof(pList.SendName));
		gQueryManager.GetAsString("wDate",pList.Date,sizeof(pList.Date));
		gQueryManager.GetAsString("Subject",pList.Subject,sizeof(pList.Subject));
		pList.read = gQueryManager.GetAsInteger("bRead");
		found = true;
	}

	gQueryManager.Close();

	if(found)
	{
		CSDataSend(lpServerManager->m_index,(BYTE*)&pList,sizeof(pList));
	}

	this->SendCounts(lpServerManager->m_index,CharacterInfo.UserIndex,CharacterInfo.Account,guid);
}

void CFriendMail::LogLostItems(const char* reason,const char* name,const SDHP_FRIENDMAIL_STORE_REQ* lpMsg)
{
	FILE* file = fopen("FriendMail_Lost.log","a");

	if(file == NULL)
	{
		return;
	}

	SYSTEMTIME now;
	GetLocalTime(&now);

	int count = (lpMsg->ItemCount > FRIENDMAIL_MAX_ITEMS) ? FRIENDMAIL_MAX_ITEMS : lpMsg->ItemCount;

	for(int n = 0; n < count; n++)
	{
		fprintf(file,"%04d-%02d-%02d %02d:%02d:%02d [%s] from %.10s to %.10s item ",
			now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,reason,name,lpMsg->ToName);

		for(int b = 0; b < FRIENDMAIL_ITEM_BYTES; b++)
		{
			fprintf(file,"%02X",lpMsg->Items[n][b]);
		}

		fprintf(file,"\n");
	}

	if(lpMsg->CoinType != FRIENDMAIL_COIN_NONE && lpMsg->CoinAmount > 0)
	{
		fprintf(file,"%04d-%02d-%02d %02d:%02d:%02d [%s] from %.10s to %.10s coins type %d amount %u\n",
			now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,reason,name,lpMsg->ToName,(int)lpMsg->CoinType,lpMsg->CoinAmount);
	}

	fclose(file);

	LogAdd(LOG_RED,"[FriendMail] %s - %d item(s) from %s written to FriendMail_Lost.log",reason,count,name);
}
