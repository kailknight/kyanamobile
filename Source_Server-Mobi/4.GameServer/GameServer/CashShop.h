// CashShop.h: interface for the CCashShop class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Protocol.h"

#define MAX_CASH_SHOP_ITEM 2000
#define MAX_CASH_SHOP_PACKAGE_PRODUCT 10
#define MAX_CASH_SHOP_PAGE 5
#define MAX_CASH_SHOP_PAGE_ITEM 9

//**********************************************//
//************ Client -> GameServer ************//
//**********************************************//

struct PMSG_CASH_SHOP_OPEN_RECV
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:02
	BYTE OpenType;
	#pragma pack()
};

struct PMSG_CASH_SHOP_ITEM_BUY_RECV
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:03
	DWORD PackageMainIndex;
	DWORD Category;
	DWORD ProductMainIndex;
	WORD ItemIndex;
	UINT CoinIndex;
	BYTE MileageFlag;
	#pragma pack()
};

struct PMSG_CASH_SHOP_ITEM_GIF_RECV
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:04
	DWORD PackageMainIndex;
	DWORD Category;
	DWORD ProductMainIndex;
	DWORD SaleZone;
	WORD ItemIndex;
	UINT CoinIndex;
	BYTE MileageFlag;
	char GiftName[11];
	char GiftText[200];
	#pragma pack()
};

struct PMSG_CASH_SHOP_ITEM_NUM_RECV
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:05
	UINT InventoryPage;
	BYTE InventoryType;
	#pragma pack()
};

struct PMSG_CASH_SHOP_ITEM_USE_RECV
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:0B
	DWORD BaseItemCode;
	DWORD MainItemCode;
	WORD ItemIndex;
	BYTE ProductType;
	#pragma pack()
};

//**********************************************//
//************ GameServer -> Client ************//
//**********************************************//

struct PMSG_CASH_SHOP_POINT_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:01
	BYTE result;
	double WCoinC[2];
	double WCoinP[2];
	double GoblinPoint;
	#pragma pack()
};

struct PMSG_CASH_SHOP_OPEN_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:02
	BYTE result;
	#pragma pack()
};

struct PMSG_CASH_SHOP_ITEM_BUY_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:03
	BYTE result;
	#pragma pack()
};

struct PMSG_CASH_SHOP_ITEM_GIF_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:04
	BYTE result;
	#pragma pack()
};

struct PMSG_CASH_SHOP_VERSION_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:[0C:15]
	WORD version[3];
	#pragma pack()
};

struct PMSG_CASH_SHOP_ITEM_NUM_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:06
	WORD ItemCount;
	WORD PageCount;
	WORD CurPage;
	WORD MaxPage;
	#pragma pack()
};

struct PMSG_CASH_SHOP_ITEM_USE_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:0B
	BYTE result;
	#pragma pack()
};

struct PMSG_CASH_SHOP_ITEM_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:0D
	DWORD BaseItemCode;
	DWORD MainItemCode;
	DWORD PackageMainIndex;
	DWORD ProductBaseIndex;
	DWORD ProductMainIndex;
	double CoinValue;
	BYTE ProductType;
	#pragma pack()
};

struct PMSG_CASH_SHOP_GIFT_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:0E
	DWORD BaseItemCode;
	DWORD MainItemCode;
	DWORD PackageMainIndex;
	DWORD ProductBaseIndex;
	DWORD ProductMainIndex;
	double CoinValue;
	BYTE ProductType;
	char GiftName[11];
	char GiftText[200];
	#pragma pack()
};

struct PMSG_CASH_SHOP_PERIODIC_ITEM_COUNT_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:11
	BYTE count;
	#pragma pack()
};

struct PMSG_CASH_SHOP_PERIODIC_ITEM_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:12
	WORD index;
	WORD slot;
	DWORD time;
	#pragma pack()
};

struct PMSG_COIN_SEND
{
	#pragma pack(1)
	PSBMSG_HEAD header; // C1:D2:01
	int Coin1;
	int Coin2;
	int Coin3;
	int Ruud;
	#pragma pack()
};

//**********************************************//
//********** DataServer -> GameServer **********//
//**********************************************//

struct SDHP_CASH_SHOP_POINT_RECV
{
	PSBMSG_HEAD header; // C1:18:00
	WORD index;
	char account[11];
	BYTE result;
	DWORD WCoinC;
	DWORD WCoinP;
	DWORD GoblinPoint;
	DWORD Ruud;
};


struct SDHP_CASH_SHOP_ITEM_BUY_RECV
{
	PSBMSG_HEAD header; // C1:18:01
	WORD index;
	char account[11];
	BYTE result;
	DWORD PackageMainIndex;
	DWORD Category;
	DWORD ProductMainIndex;
	WORD ItemIndex;
	UINT CoinIndex;
	BYTE MileageFlag;
	DWORD WCoinC;
	DWORD WCoinP;
	DWORD GoblinPoint;
	DWORD ItemCount;
	// --- pricing resolved by the DataServer ---
	// EffectivePrice is what to actually charge. -1 means it could not be
	// resolved (tables absent, no matching row, CustomCashShop off), and the
	// caller falls back to the in-memory CoinValue. ListPrice rides along so the
	// audit row can show what the discount was worth.
	int ListPrice;
	int EffectivePrice;
	int DiscountId;
	short DiscountPercent;
};

struct SDHP_CASH_SHOP_ITEM_GIF_RECV
{
	PSWMSG_HEAD header; // C2:18:02
	WORD index;
	char account[11];
	BYTE result;
	DWORD PackageMainIndex;
	DWORD Category;
	DWORD ProductMainIndex;
	DWORD SaleZone;
	WORD ItemIndex;
	UINT CoinIndex;
	BYTE MileageFlag;
	char GiftName[11];
	char GiftText[200];
	char GiftAccount[11];
	DWORD WCoinC;
	DWORD WCoinP;
	DWORD GoblinPoint;
	DWORD ItemCount;
};

struct SDHP_CASH_SHOP_ITEM_NUM_RECV
{
	PSWMSG_HEAD header; // C2:18:03
	WORD index;
	char account[11];
	BYTE result;
	UINT InventoryPage;
	BYTE InventoryType;
	DWORD ItemCount;
	DWORD PageCount;
	struct
	{
		DWORD BaseItemCode;
		DWORD MainItemCode;
		DWORD PackageMainIndex;
		DWORD ProductBaseIndex;
		DWORD ProductMainIndex;
		double CoinValue;
		BYTE ProductType;
		char GiftName[11];
		char GiftText[200];
	}ProductInfo[MAX_CASH_SHOP_PAGE_ITEM];
};

struct SDHP_CASH_SHOP_ITEM_USE_RECV
{
	PSWMSG_HEAD header; // C2:18:04
	WORD index;
	char account[11];
	BYTE result;
	DWORD BaseItemCode;
	DWORD MainItemCode;
	WORD ItemIndex;
	BYTE ProductType;
	struct
	{
		DWORD BaseItemCode;
		DWORD MainItemCode;
		DWORD PackageMainIndex;
		DWORD ProductBaseIndex;
		DWORD ProductMainIndex;
		double CoinValue;
		BYTE ProductType;
		char GiftName[11];
		char GiftText[200];
	}ProductInfo;
};

struct SDHP_CASH_SHOP_PERIODIC_ITEM_RECV
{
	PSWMSG_HEAD header; // C2:18:05
	WORD index;
	char account[11];
	BYTE count;
};

struct SDHP_CASH_SHOP_PERIODIC_ITEM1
{
	BYTE slot;
	DWORD serial;
	DWORD time;
};

struct SDHP_CASH_SHOP_RECIEVE_POINT_RECV
{
	PSBMSG_HEAD header; // C1:18:06
	WORD index;
	char account[11];
	DWORD CallbackFunc;
	DWORD CallbackArg1;
	DWORD CallbackArg2;
	DWORD WCoinC;
	DWORD WCoinP;
	DWORD GoblinPoint;
};

//**********************************************//
//********** GameServer -> DataServer **********//
//**********************************************//

struct SDHP_CASH_SHOP_POINT_SEND
{
	PSBMSG_HEAD header; // C1:18:00
	WORD index;
	char account[11];
};

/*
	Catalog transfer, replacing the CashShopProduct.txt / CashShopPackage.txt
	loaders when CustomCashShop = 1. Head 0x18, subs 0x08 - 0x0A.

	Row-at-a-time rather than one big blob. The catalog can be up to
	MAX_CASH_SHOP_ITEM (2000) products, which is far past what fits in a packet,
	and chunk-and-reassemble would need its own buffer, length checks and a
	partial-transfer failure mode. This runs once at boot over a local socket, so
	the extra round trips cost nothing and there is nothing to get wrong.

	The BEGIN message carries the counts so the GameServer can clear its arrays
	exactly once and know what to expect. Packages carry their own product slots
	inline - the arrays are fixed at 10 either way, so a separate link message
	would be pure overhead.

	If BEGIN never arrives the arrays are left alone, which means a DataServer
	that is down or a schema that was never applied leaves the shop on whatever
	the .txt files last gave it rather than empty.
*/
struct SDHP_CASH_SHOP_CATALOG_REQ_SEND
{
	PSBMSG_HEAD header; // C1:18:08
};

struct SDHP_CASH_SHOP_CATALOG_BEGIN_RECV
{
	PSBMSG_HEAD header; // C1:18:08
	int ProductCount;
	int PackageCount;
};

struct SDHP_CASH_SHOP_CATALOG_PRODUCT_RECV
{
	PSBMSG_HEAD header; // C1:18:09
	int BaseIndex;
	int MainIndex;
	int CoinValue;
	int ItemIndex;
	int ItemLevel;
	int ItemOption1;
	int ItemOption2;
	int ItemOption3;
	int ItemNewOption;
	int ItemSetOption;
	int ItemHarmonyOption;
	int ItemOptionEx;
	int ItemSocketOption1;
	int ItemSocketOption2;
	int ItemSocketOption3;
	int ItemSocketOption4;
	int ItemSocketOption5;
	int ItemQuantity;
	int ItemDuration;
};

struct SDHP_CASH_SHOP_CATALOG_PACKAGE_RECV
{
	PSBMSG_HEAD header; // C1:18:0A
	int Category;
	int BaseIndex;
	int MainIndex;
	int ItemIndex;
	int CoinIndex;
	int CoinValue;
	int BonusGP;
	int ProductBaseIndex[MAX_CASH_SHOP_PACKAGE_PRODUCT];
	int ProductMainIndex[MAX_CASH_SHOP_PACKAGE_PRODUCT];
	BYTE IsLast;
};
/*
	Live price push, so the shelf can show what the purchase will actually cost.

	The catalog transfer above is a boot-time snapshot. Discounts are time-bounded
	(CustomCashShopDiscounts has StartDate/EndDate), so a price that was right at
	boot is wrong the moment a sale window opens or closes. Rather than cache and
	expire, the GameServer asks for prices when a player opens the shop and
	forwards the answer to that player. A shop open is a keypress, not a tick, so
	the query rate is trivially low and the numbers are always current.

	Display only. The client never sends a price - CGCashShopItemBuyRecv carries
	a package seq and nothing else - so a tampered client can misdraw its own shop
	and still be charged whatever DGCashShopItemBuyRecv works out from the view.

	Batched, unlike the catalog: this runs per shop-open rather than once per boot,
	so one packet per package would be a burst of sends every time somebody opens
	the shop. Twelve rows is what fits under the byte-sized C1 length once the
	index/account echo is accounted for; going wider would need a C2 header, and
	the client's 0xD2 dispatcher reads its sub-code at the C1 offset.
*/
#define MAX_CASH_SHOP_PRICE_BATCH 12

struct CASH_SHOP_PRICE_ROW
{
	int MainIndex;         // package MainIndex, or product MainIndex when RowKind is 1
	int ListPrice;
	int EffectivePrice;
	short DiscountPercent; // 0 when nothing is on sale
	short RowKind;         // 0 = package (PackageProductSeq), 1 = product variant
};

struct SDHP_CASH_SHOP_PRICE_REQ_SEND
{
	PSBMSG_HEAD header; // C1:18:0B
	WORD index;
	char account[11];
};

struct SDHP_CASH_SHOP_PRICE_RECV
{
	PSBMSG_HEAD header; // C1:18:0C
	WORD index;
	char account[11];   // re-checked on arrival, exactly like the point query
	BYTE Count;
	BYTE IsLast;
	BYTE Padding[1];    // see PMSG_CASH_SHOP_PRICE_SEND - Row starts at 20 either way
	CASH_SHOP_PRICE_ROW Row[MAX_CASH_SHOP_PRICE_BATCH];
};

// GameServer -> client. Same rows, forwarded as they arrive.
struct PMSG_CASH_SHOP_PRICE_SEND
{
	PSBMSG_HEAD header; // C1:D2:20
	BYTE Count;
	BYTE IsLast;
	// Explicit, because the client declares its copy inside a #pragma pack(push,1)
	// region while both servers build at default alignment. Without these two
	// bytes Row would start at 6 on one side and 8 on the other, and every field
	// would read two bytes off with nothing to show for it but wrong prices.
	BYTE Padding[2];
	CASH_SHOP_PRICE_ROW Row[MAX_CASH_SHOP_PRICE_BATCH];
};

/*
	Package display-name override, admin-edited (CustomCashShopPackages.DisplayName),
	piggybacked on the same per-shop-open round trip as the price batch above -
	no new request sub-code. Head 0x18, sub 0x0D (0x09-0x0C already taken).

	Parallel to CASH_SHOP_PRICE_ROW rather than a field added to it: bolting a
	32-byte name onto that row would force its batch from 12 rows down to ~5 and
	risk the already-live price path for no reason. Only packages with a
	non-null, non-empty DisplayName are sent - packages without an override cost
	zero wire bytes and keep showing whatever IBSPackage.txt already gives them.
*/
#define MAX_CASH_SHOP_NAME_BATCH 6
#define CASH_SHOP_NAME_LENGTH    32     // truncated from DisplayName NVARCHAR(64)

struct CASH_SHOP_NAME_ROW
{
	int MainIndex;      // == CShopPackage::PackageProductSeq
	char DisplayName[CASH_SHOP_NAME_LENGTH];
};

struct SDHP_CASH_SHOP_NAME_RECV
{
	PSBMSG_HEAD header; // C1:18:0D
	WORD index;
	char account[11];   // re-checked on arrival, exactly like the price query
	BYTE Count;
	BYTE IsLast;
	BYTE Padding[1];    // Row starts at 20, same discipline as SDHP_CASH_SHOP_PRICE_RECV
	CASH_SHOP_NAME_ROW Row[MAX_CASH_SHOP_NAME_BATCH];
};

// GameServer -> client. Same rows, forwarded as they arrive.
struct PMSG_CASH_SHOP_NAME_SEND
{
	PSBMSG_HEAD header; // C1:D2:21
	BYTE Count;
	BYTE IsLast;
	// Explicit, same reason as PMSG_CASH_SHOP_PRICE_SEND above - the client
	// declares its copy inside a #pragma pack(push,1) region while both
	// servers build at default alignment.
	BYTE Padding[2];
	CASH_SHOP_NAME_ROW Row[MAX_CASH_SHOP_NAME_BATCH];
};

/*
	Active banner, piggybacked on the same round trip as price/name above. Head
	0x18 sub 0x0E (DataServer -> GameServer), forwarded as 0xD2 sub 0x22
	(GameServer -> client). Single row, not batched - only one banner is ever
	active. BannerId = -1 means "checked, none active", so the client clears
	any stale banner rather than leaving an old one showing forever.

	URL length capped at 200: SDHP_CASH_SHOP_BANNER_RECV rides the same C1
	header as everything else here (one-BYTE size field) -
	4(header)+2(index)+11(account)+4(BannerId)+N+4(TargetPackageId) must stay
	under 255, so N tops out at 230; 200 leaves comfortable margin.
*/
#define CASH_SHOP_BANNER_URL_LENGTH 200

struct SDHP_CASH_SHOP_BANNER_RECV
{
	PSBMSG_HEAD header; // C1:18:0E
	WORD index;
	char account[11];
	int BannerId;
	char ImagePath[CASH_SHOP_BANNER_URL_LENGTH];
	int TargetPackageId;
};

// GameServer -> client.
struct PMSG_CASH_SHOP_BANNER_SEND
{
	PSBMSG_HEAD header; // C1:D2:22
	int BannerId;
	char ImagePath[CASH_SHOP_BANNER_URL_LENGTH];
	int TargetPackageId;
};

/*
	The display catalog - what the player sees, as opposed to the 0x08/0x09/0x0A
	catalog, which is what the server charges and grants.

	Three hops:
	    client -> GS   0xD2:0x23  "send it, unless it is still version N"
	    GS -> DS       0x18:0x0F  the same question, with the player's index
	    DS -> GS       0x18:0x10  one fragment of one row, streamed
	    GS -> client   0xD2:0x24  the same fragment, relayed

	The GameServer holds none of this. It is a pass-through, exactly like the
	price and name relays, so there is no server-side cache to invalidate when
	the admin tool edits something - the version check does that job instead.

	THE PAYLOAD IS A SCRIPT LINE, not a struct: the same @-delimited text
	IBSCategory.txt / IBSPackage.txt / IBSProduct.txt hold, parsed on the client
	by the same SetCategory / SetPackage / SetProduct it has always used. That is
	also why this is fragmented - a C1 size field is one byte, and the longest
	package row already runs to 228 of the 231 a single packet could carry.

	CASH_SHOP_CATALOG_FRAG and the flag values MUST match
	2.DataServer/CashShop.h, or fragments reassemble into nonsense.
*/
#define CASH_SHOP_CATALOG_FRAG 200

#define CASH_SHOP_CATALOG_CATEGORY  0
#define CASH_SHOP_CATALOG_PACKAGE   1
#define CASH_SHOP_CATALOG_PRODUCT   2
#define CASH_SHOP_CATALOG_UPTODATE  0xFE
#define CASH_SHOP_CATALOG_END       0xFF

#define CATALOG_FLAG_LINE_END 0x01

// GameServer -> DataServer.
struct SDHP_CASH_SHOP_CATALOG_LINE_REQ_SEND
{
	PSBMSG_HEAD header; // C1:18:0F
	WORD index;
	char account[11];
	BYTE Padding[1];
	int CachedVersion;
};

// DataServer -> GameServer.
struct SDHP_CASH_SHOP_CATALOG_LINE_RECV
{
	PSBMSG_HEAD header; // C1:18:10
	WORD index;
	char account[11];
	BYTE RowKind;
	BYTE Flags;
	BYTE Padding[1];
	int Version;
	char Fragment[CASH_SHOP_CATALOG_FRAG];
};

// client -> GameServer.
//
// No padding: the client writes this with CStreamPacketEngine, which appends
// tightly after the 4-byte header, and an int at offset 4 is already aligned
// here so no compiler pad is inserted either.
struct PMSG_CASH_SHOP_CATALOG_REQ_RECV
{
	PSBMSG_HEAD header;  // C1:D2:23
	int CachedVersion;   // 4..7
};

// GameServer -> client.
//
// Padding[2], not [1]. The client declares its copy inside a
// #pragma pack(push,1) region while this builds at default alignment: with one
// pad byte Version lands at offset 7 there and 8 here, and every field after it
// reads off by one - wrong catalog, no crash, no error. Two pins it to 8 on
// both sides. Same trap the price row hit in Phase 4.
struct PMSG_CASH_SHOP_CATALOG_SEND
{
	PSBMSG_HEAD header; // C1:D2:24
	BYTE RowKind;       // 4
	BYTE Flags;         // 5
	BYTE Padding[2];    // 6..7
	int Version;        // 8..11
	char Fragment[CASH_SHOP_CATALOG_FRAG];
};

/*
	Purchase audit row, GameServer -> DataServer. Head 0x18, sub 0x07 (0x00-0x06
	were already taken). Fire and forget: there is no reply, because nothing the
	GameServer does next depends on the row landing.

	It has to be a message rather than a query. The purchase decision is made in
	the GameServer (CashShop.cpp, DGCashShopItemBuyRecv) - that is the only place
	that knows the outcome - but the GameServer holds no ODBC handle. The
	DataServer owns the only one.

	Prices are both carried even though Phase 2 sets them equal: once the discount
	engine lands, ListPrice and PaidPrice diverge, and a purchase whose client
	price disagreed with the server's is logged as Result 2 rather than silently
	refused.
*/
struct SDHP_CASH_SHOP_LOG_SEND
{
	PSBMSG_HEAD header; // C1:18:07
	char account[11];
	char character[11];
	int BaseIndex;
	int MainIndex;
	int ItemIndex;
	int CoinIndex;
	int ListPrice;
	int PaidPrice;
	short DiscountPercent;
	BYTE Result;          // 0 ok | 1 funds | 2 price mismatch | 3 currency | 4 unknown package
	char IpAddress[16];
	short ServerCode;
};

struct SDHP_CASH_SHOP_ITEM_BUY_SEND
{
	PSBMSG_HEAD header; // C1:18:01
	WORD index;
	char account[11];
	DWORD PackageMainIndex;
	DWORD Category;
	DWORD ProductMainIndex;
	WORD ItemIndex;
	UINT CoinIndex;
	BYTE MileageFlag;
};

struct SDHP_CASH_SHOP_ITEM_GIF_SEND
{
	PSWMSG_HEAD header; // C2:18:02
	WORD index;
	char account[11];
	DWORD PackageMainIndex;
	DWORD Category;
	DWORD ProductMainIndex;
	DWORD SaleZone;
	WORD ItemIndex;
	UINT CoinIndex;
	BYTE MileageFlag;
	char GiftName[11];
	char GiftText[200];
};

struct SDHP_CASH_SHOP_ITEM_NUM_SEND
{
	PSBMSG_HEAD header; // C1:18:03
	WORD index;
	char account[11];
	UINT InventoryPage;
	BYTE InventoryType;
};

struct SDHP_CASH_SHOP_ITEM_USE_SEND
{
	PSBMSG_HEAD header; // C1:18:04
	WORD index;
	char account[11];
	DWORD BaseItemCode;
	DWORD MainItemCode;
	WORD ItemIndex;
	BYTE ProductType;
};

struct SDHP_CASH_SHOP_PERIODIC_ITEM_SEND
{
	PSWMSG_HEAD header; // C2:18:05
	WORD index;
	char account[11];
	BYTE count;
};

struct SDHP_CASH_SHOP_PERIODIC_ITEM2
{
	BYTE slot;
	DWORD serial;
};

struct SDHP_CASH_SHOP_RECIEVE_POINT_SEND
{
	PSBMSG_HEAD header; // C1:18:06
	WORD index;
	char account[11];
	DWORD CallbackFunc;
	DWORD CallbackArg1;
	DWORD CallbackArg2;
};

struct SDHP_CASH_SHOP_ADD_POINT_SAVE_SEND
{
	PSBMSG_HEAD header; // C1:18:30
	WORD index;
	char account[11];
	char GiftAccount[11];
	DWORD AddWCoinC;
	DWORD AddWCoinP;
	DWORD AddGoblinPoint;
	DWORD AddRuud;
};

struct SDHP_CASH_SHOP_SUB_POINT_SAVE_SEND
{
	PSBMSG_HEAD header; // C1:18:31
	WORD index;
	char account[11];
	char GiftAccount[11];
	DWORD SubWCoinC;
	DWORD SubWCoinP;
	DWORD SubGoblinPoint;
	DWORD SubRuud;
};

struct SDHP_CASH_SHOP_INSERT_ITEM_SAVE_SEND
{
	PSWMSG_HEAD header; // C2:18:32
	WORD index;
	char account[11];
	char GiftAccount[11];
	BYTE InventoryType;
	DWORD PackageMainIndex;
	DWORD ProductBaseIndex;
	DWORD ProductMainIndex;
	double CoinValue;
	BYTE ProductType;
	char GiftName[11];
	char GiftText[200];
};

struct SDHP_CASH_SHOP_DELETE_ITEM_SAVE_SEND
{
	PSBMSG_HEAD header; // C1:18:33
	WORD index;
	char account[11];
	char GiftAccount[11];
	DWORD BaseItemCode;
	DWORD MainItemCode;
};

struct SDHP_CASH_SHOP_PERIODIC_ITEM_SAVE_SEND
{
	PSWMSG_HEAD header; // C2:18:34
	WORD index;
	char account[11];
	BYTE count;
};

struct SDHP_CASH_SHOP_PERIODIC_ITEM_SAVE
{
	DWORD serial;
	DWORD time;
};

//**********************************************//
//**********************************************//
//**********************************************//

struct CASH_SHOP_INVENTORY
{
	DWORD BaseItemCode;
	DWORD MainItemCode;
	DWORD PackageMainIndex;
	DWORD ProductBaseIndex;
	DWORD ProductMainIndex;
	double CoinValue;
	BYTE ProductType;
	char GiftName[11];
	char GiftText[200];
};

struct CASH_SHOP_PACKAGE_INFO
{
	int Category;
	int BaseIndex;
	int MainIndex;
	int ItemIndex;
	int CoinIndex;
	int CoinValue;
	int BonusGP;
	int ProductBaseIndex[MAX_CASH_SHOP_PACKAGE_PRODUCT];
	int ProductMainIndex[MAX_CASH_SHOP_PACKAGE_PRODUCT];
};

struct CASH_SHOP_PRODUCT_INFO
{
	int BaseIndex;
	int MainIndex;
	int CoinValue;
	int ItemIndex;
	int ItemLevel;
	int ItemOption1;
	int ItemOption2;
	int ItemOption3;
	int ItemNewOption;
	int ItemSetOption;
	int ItemHarmonyOption;
	int ItemOptionEx;
	int ItemSocketOption1;
	int ItemSocketOption2;
	int ItemSocketOption3;
	int ItemSocketOption4;
	int ItemSocketOption5;
	int ItemQuantity;
	int ItemDuration;
};

class CCashShop
{
public:
	CCashShop();
	virtual ~CCashShop();
	void LoadPackage(char* path);
	void LoadProduct(char* path);
	void SetPackageInfo(CASH_SHOP_PACKAGE_INFO info);
	void SetProductInfo(CASH_SHOP_PRODUCT_INFO info);
	CASH_SHOP_PACKAGE_INFO* GetPackageInfo(int index);
	CASH_SHOP_PRODUCT_INFO* GetProductInfo(int index);
	CASH_SHOP_PRODUCT_INFO* GetProductInfoByBaseIndex(int index);
	void MainProc();
	int GetPackageProductBaseIndexCount(CASH_SHOP_PACKAGE_INFO* lpPackageInfo);
	int GetPackageProductMainIndexCount(CASH_SHOP_PACKAGE_INFO* lpPackageInfo);
	void CGCashShopPointRecv(int aIndex);
	void CGCashShopOpenRecv(PMSG_CASH_SHOP_OPEN_RECV* lpMsg,int aIndex);
	void CGCashShopItemBuyRecv(PMSG_CASH_SHOP_ITEM_BUY_RECV* lpMsg,int aIndex);
	void CGCashShopItemGifRecv(PMSG_CASH_SHOP_ITEM_GIF_RECV* lpMsg,int aIndex);
	void CGCashShopItemNumRecv(PMSG_CASH_SHOP_ITEM_NUM_RECV* lpMsg,int aIndex);
	void CGCashShopItemUseRecv(PMSG_CASH_SHOP_ITEM_USE_RECV* lpMsg,int aIndex);
	void GCCashShopInitSend(LPOBJ lpObj);
	void GCCashShopScriptVersionSend(LPOBJ lpObj);
	void GCCashShopBannerVersionSend(LPOBJ lpObj);
	void GCCashShopItemSend(LPOBJ lpObj,int PageCount,CASH_SHOP_INVENTORY* CashInventory);
	void GCCashShopGiftSend(LPOBJ lpObj,int PageCount,CASH_SHOP_INVENTORY* CashInventory);
	void GCCashShopPeriodicItemSend(int aIndex,int index,int slot,int time);
	void DGCashShopPointRecv(SDHP_CASH_SHOP_POINT_RECV* lpMsg);
	void DGCashShopItemBuyRecv(SDHP_CASH_SHOP_ITEM_BUY_RECV* lpMsg);
	void DGCashShopItemGifRecv(SDHP_CASH_SHOP_ITEM_GIF_RECV* lpMsg);
	void DGCashShopItemNumRecv(SDHP_CASH_SHOP_ITEM_NUM_RECV* lpMsg);
	void DGCashShopItemUseRecv(SDHP_CASH_SHOP_ITEM_USE_RECV* lpMsg);
	void DGCashShopPeriodicItemRecv(SDHP_CASH_SHOP_PERIODIC_ITEM_RECV* lpMsg);
	void DGCashShopRecievePointRecv(SDHP_CASH_SHOP_RECIEVE_POINT_RECV* lpMsg);
	void GDCashShopPeriodicItemSend(int aIndex);
	void GDCashShopRecievePointSend(int aIndex,DWORD CallbackFunc,DWORD CallbackArg1,DWORD CallbackArg2);
	void GDCashShopAddPointSaveSend(int aIndex, char* GiftAccount, DWORD AddWCoinC, DWORD AddWCoinP, DWORD AddGoblinPoint, DWORD AddRuud = 0, char* NameLog = "NO");
	void GDCashShopSubPointSaveSend(int aIndex,char* GiftAccount,DWORD SubWCoinC,DWORD SubWCoinP,DWORD SubGoblinPoint, DWORD SubRuud = 0, char* NameLog = "NO");
	// Writes one CustomCashShop_Logs row via the DataServer. Safe to call on the
	// failure paths too - a refused purchase is the interesting kind.
	void GDCashShopLogSend(int aIndex,int BaseIndex,int MainIndex,int ItemIndex,int CoinIndex,int ListPrice,int PaidPrice,int DiscountPercent,BYTE Result);

	// Catalog transfer. The request goes out once the DataServer link is up; the
	// three Recv handlers rebuild the same arrays the .txt loaders fill.
	void GDCashShopCatalogReqSend();
	void DGCashShopCatalogBeginRecv(SDHP_CASH_SHOP_CATALOG_BEGIN_RECV* lpMsg);
	void DGCashShopCatalogProductRecv(SDHP_CASH_SHOP_CATALOG_PRODUCT_RECV* lpMsg);
	void DGCashShopCatalogPackageRecv(SDHP_CASH_SHOP_CATALOG_PACKAGE_RECV* lpMsg);

	// Live prices for the package shelf. Requested when a player opens the shop
	// and forwarded straight to them - see the comment on the structs above.
	void GDCashShopPriceReqSend(int aIndex);
	void DGCashShopPriceRecv(SDHP_CASH_SHOP_PRICE_RECV* lpMsg);

	// Package display-name overrides, piggybacked on the price round trip above.
	void DGCashShopNameRecv(SDHP_CASH_SHOP_NAME_RECV* lpMsg);

	// Active banner, piggybacked on the same price round trip.
	void DGCashShopBannerRecv(SDHP_CASH_SHOP_BANNER_RECV* lpMsg);
	void CGCashShopCatalogReqRecv(PMSG_CASH_SHOP_CATALOG_REQ_RECV* lpMsg,int aIndex);
	void GDCashShopCatalogLineReqSend(int aIndex,int cachedVersion);
	void DGCashShopCatalogLineRecv(SDHP_CASH_SHOP_CATALOG_LINE_RECV* lpMsg);

	// The display catalog version this server last reloaded its sellable
	// catalog for. See DGCashShopCatalogLineRecv.
	int m_iLastCatalogVersion;

	void GDCashShopInsertItemSaveSend(int aIndex,char* GiftAccount,BYTE InventoryType,DWORD PackageMainIndex,DWORD ProductBaseIndex,DWORD ProductMainIndex,double CoinValue,BYTE ProductType,char* GiftName,char* GiftText);
	void GDCashShopDeleteItemSaveSend(int aIndex,char* GiftAccount,DWORD BaseItemCode,DWORD MainItemCode);
	void GDCashShopPeriodicItemSaveSend(int aIndex);
	void GCSendCoin(LPOBJ lpObj, int coin1, int coin2, int coin3);
private:
	CASH_SHOP_PACKAGE_INFO m_CashShopPackageInfo[MAX_CASH_SHOP_ITEM];
	CASH_SHOP_PRODUCT_INFO m_CashShopProductInfo[MAX_CASH_SHOP_ITEM];
	int m_CashShopPackageCount;
	int m_CashShopProductCount;
};

extern CCashShop gCashShop;
