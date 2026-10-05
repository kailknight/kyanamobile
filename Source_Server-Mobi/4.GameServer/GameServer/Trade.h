// Trade.h: interface for the CTrade class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "ItemManager.h"
#include "Protocol.h"

#define MAX_TRADE_MONEY 1000000000

//**********************************************//
//************ Client -> GameServer ************//
//**********************************************//

struct PMSG_TRADE_REQUEST_RECV
{
	PBMSG_HEAD header; // C1:36
	BYTE index[2];
};

struct PMSG_TRADE_RESPONSE_RECV
{
	PBMSG_HEAD header; // C1:37
	BYTE response;
	char name[10];
	WORD level;
	DWORD GuildNumber;
};

struct PMSG_TRADE_MONEY_RECV
{
	PBMSG_HEAD header; // C1:3B
	DWORD money;
};

struct PMSG_TRADE_OK_BUTTON_RECV
{
	PBMSG_HEAD header; // C1:3C
	BYTE flag;
};

//**********************************************//
//************ GameServer -> Client ************//
//**********************************************//

struct PMSG_TRADE_REQUEST_SEND
{
	PBMSG_HEAD header; // C3:36
	char name[10];
};

struct PMSG_TRADE_RESPONSE_SEND
{
	PBMSG_HEAD header; // C1:37
	BYTE response;
	char name[10];
	WORD level;
	DWORD GuildNumber;
};

struct PMSG_TRADE_ITEM_DEL_SEND
{
	PBMSG_HEAD header; // C1:38
	BYTE slot;
};

struct PMSG_TRADE_ITEM_ADD_SEND
{
	PBMSG_HEAD header; // C1:39
	BYTE slot;
	BYTE ItemInfo[MAX_ITEM_INFO];
};

struct PMSG_TRADE_MONEY_SEND
{
	PBMSG_HEAD header; // C1:3B
	DWORD money;
};

struct PMSG_TRADE_OK_BUTTON_SEND
{
	PBMSG_HEAD header; // C1:3C
	BYTE flag;
};

struct PMSG_TRADE_RESULT_SEND
{
	PBMSG_HEAD header; // C1:[3A:3D]
	BYTE result;
};

// Coins in a trade: WCoinC, WCoinP and Goblin Points next to the zen.
#define TRADE_COIN_TYPES		3
#define MAX_TRADE_COIN			1000000000

// Packed: the client sends exactly 9 bytes (TradeCoinPanel.h). Unpacked, the
// DWORD is padded to offset 8, the struct is 12, and the size check in
// Protocol.cpp silently dropped every offer.
#pragma pack(push,1)

struct PMSG_TRADE_COIN_RECV
{
	PSBMSG_HEAD header; // C1:D3:E8
	BYTE type;			// 0 WCoinC, 1 WCoinP, 2 Goblin Points
	DWORD amount;		// 0 takes that coin back out
};

struct PMSG_TRADE_COIN_SEND
{
	PSBMSG_HEAD header; // C1:D3:E7
	DWORD mine[TRADE_COIN_TYPES];
	DWORD theirs[TRADE_COIN_TYPES];
};

#pragma pack(pop)

//**********************************************//
//**********************************************//
//**********************************************//

class CTrade
{
public:
	CTrade();
	virtual ~CTrade();
	void ClearTrade(LPOBJ lpObj);
	void ResetTrade(int aIndex);
	bool ExchangeTradeItem(LPOBJ lpObj,LPOBJ lpTarget);
	void ExchangeTradeItemLog(LPOBJ lpObj,LPOBJ lpTarget);
	void CGTradeRequestRecv(PMSG_TRADE_REQUEST_RECV* lpMsg,int aIndex);
	void CGTradeResponseRecv(PMSG_TRADE_RESPONSE_RECV* lpMsg,int aIndex);
	void CGTradeMoneyRecv(PMSG_TRADE_MONEY_RECV* lpMsg,int aIndex);
	void CGTradeOkButtonRecv(PMSG_TRADE_OK_BUTTON_RECV* lpMsg,int aIndex);
	void CGTradeCancelButtonRecv(int aIndex);
	void GCTradeRequestSend(int aIndex,char* name);
	void GCTradeResponseSend(int aIndex,BYTE response,char* name,int level,int GuildNumber);
	void GCTradeItemDelSend(int aIndex,BYTE slot);
	void GCTradeItemAddSend(int aIndex,BYTE slot,BYTE* ItemInfo);
	void GCTradeMoneySend(int aIndex,DWORD money);
	void GCTradeOkButtonSend(int aIndex,BYTE flag);
	void GCTradeResultSend(int aIndex,BYTE result);
	// Coins.
	void CGTradeCoinRecv(PMSG_TRADE_COIN_RECV* lpMsg,int aIndex);
	void GCTradeCoinSend(int aIndex);
	bool CheckTradeCoins(LPOBJ lpObj,LPOBJ lpTarget);
	bool TransferTradeCoins(LPOBJ lpObj,LPOBJ lpTarget);
private:
	bool MoveCoins(LPOBJ lpObj,const int* coins,int sign);
};

extern CTrade gTrade;
