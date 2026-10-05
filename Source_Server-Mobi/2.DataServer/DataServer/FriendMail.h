// FriendMail.h - modern friend window data and item mail, DataServer side.
//
// Every SQL statement for the feature lives here; the GameServer never touches
// the database. Messages are in FriendMailProtocol.h, the tables and
// procedures in FriendMail_Schema.sql.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "DataServerProtocol.h"
#include "FriendMailProtocol.h"

class CFriendMail
{
public:
	// GameServer -> DataServer, 0xD9 / 0x40-0x46 (DataServerProtocol.cpp).
	void OnDetailRequest(SDHP_FRIENDMAIL_NAME_REQ* lpMsg,int index);
	void OnCheckRequest(SDHP_FRIENDMAIL_CHECK_REQ* lpMsg,int index);
	void OnStoreRequest(SDHP_FRIENDMAIL_STORE_REQ* lpMsg,int index);
	void OnItemsRequest(SDHP_FRIENDMAIL_MEMO_REQ* lpMsg,int index);
	void OnClaimRequest(SDHP_FRIENDMAIL_MEMO_REQ* lpMsg,int index);
	void OnUnclaimRequest(SDHP_FRIENDMAIL_UNCLAIM_REQ* lpMsg);
	void OnCountsRequest(SDHP_FRIENDMAIL_NAME_REQ* lpMsg,int index);

	// Called by the legacy letter delete (CSProtocol.cpp): a letter that still
	// holds unclaimed items may not be deleted, or the items would be lost.
	int CountWaitingItems(const char* name,DWORD memoIndex);

	// Accepting a request (WZ_FriendAdd) only adds the acceptor's own row; the
	// requester's row stays Del=1 from WZ_WaitFriendAdd, and the pair is stuck
	// half-done (shown offline, "already registered" on re-adding). This clears it.
	void CompleteFriendship(const char* acceptor,const char* requester);

private:
	DWORD GetGuid(const char* name);
	void ReturnExpiredItems();
	void SendCounts(int index,WORD aIndex,const char* account,DWORD guid);
	void NotifyRecipient(const char* toName,DWORD memoIndex,DWORD guid);
	void LogLostItems(const char* reason,const char* name,const SDHP_FRIENDMAIL_STORE_REQ* lpMsg);
};

extern CFriendMail gFriendMail;
