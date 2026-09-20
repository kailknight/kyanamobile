// CashShop.h: interface for the CCashShop class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "DataServerProtocol.h"

#define MAX_CASH_SHOP_PAGE 5
#define MAX_CASH_SHOP_PAGE_ITEM 9

// Must match 4.GameServer/CashShop.h. These are separate headers, not a shared
// one, so a change on one side has to be made on both - the catalog messages
// below carry fixed-size arrays sized by MAX_CASH_SHOP_PACKAGE_PRODUCT, and a
// mismatch would silently misalign every package row on the wire.
#define MAX_CASH_SHOP_ITEM 2000
#define MAX_CASH_SHOP_PACKAGE_PRODUCT 10

/*
	Catalog transfer, head 0x18 subs 0x08-0x0A. See 4.GameServer/CashShop.h for
	why this is row-at-a-time rather than one blob.
*/
struct SDHP_CASH_SHOP_CATALOG_REQ_RECV
{
	PSBMSG_HEAD header; // C1:18:08
};

// Price request, GameServer -> DataServer. Head 0x18, sub 0x0B. The index and
// account are echoed back untouched so the GameServer can route the answer to
// the player who opened the shop; the DataServer does not read them.
struct SDHP_CASH_SHOP_PRICE_REQ_RECV
{
	PSBMSG_HEAD header; // C1:18:0B
	WORD index;
	char account[11];
};

struct SDHP_CASH_SHOP_CATALOG_BEGIN_SEND
{
	PSBMSG_HEAD header; // C1:18:08
	int ProductCount;
	int PackageCount;
};

struct SDHP_CASH_SHOP_CATALOG_PRODUCT_SEND
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

struct SDHP_CASH_SHOP_CATALOG_PACKAGE_SEND
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
	Live package prices, DataServer -> GameServer. Head 0x18, sub 0x0C, answering
	the 0x0B request the GameServer sends when a player opens the cash shop.

	Mirrors 4.GameServer/CashShop.h - MAX_CASH_SHOP_PRICE_BATCH and the row layout
	MUST match there, or the batch is misread field by field.

	Rows come straight out of CustomCashShopEffectivePrice, the same view the
	purchase path re-validates against, so the shelf and the charge cannot drift.
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

struct SDHP_CASH_SHOP_PRICE_SEND
{
	PSBMSG_HEAD header; // C1:18:0C
	WORD index;
	char account[11];
	BYTE Count;
	BYTE IsLast;
	BYTE Padding[1];    // pins Row to offset 20 regardless of packing
	CASH_SHOP_PRICE_ROW Row[MAX_CASH_SHOP_PRICE_BATCH];
};

/*
	Package display-name override, DataServer -> GameServer. Head 0x18, sub 0x0D,
	piggybacked on the same 0x0B price request - no separate request sub-code.

	Mirrors 4.GameServer/CashShop.h - MAX_CASH_SHOP_NAME_BATCH and the row layout
	MUST match there, or the batch is misread field by field. Only packages with
	a non-null, non-empty DisplayName are queried, so this can legitimately be a
	zero-row (but still one, IsLast-bearing) batch.
*/
#define MAX_CASH_SHOP_NAME_BATCH 6
#define CASH_SHOP_NAME_LENGTH    32     // truncated from DisplayName NVARCHAR(64)

struct CASH_SHOP_NAME_ROW
{
	int MainIndex;      // == CShopPackage::PackageProductSeq
	char DisplayName[CASH_SHOP_NAME_LENGTH];
};

struct SDHP_CASH_SHOP_NAME_SEND
{
	PSBMSG_HEAD header; // C1:18:0D
	WORD index;
	char account[11];
	BYTE Count;
	BYTE IsLast;
	BYTE Padding[1];    // pins Row to offset 20 regardless of packing
	CASH_SHOP_NAME_ROW Row[MAX_CASH_SHOP_NAME_BATCH];
};

/*
	Active banner, DataServer -> GameServer. Head 0x18, sub 0x0E, piggybacked on
	the same 0x0B price request - no separate request sub-code. Only one banner
	is ever active at a time, so this is a single-row message, not a batch.
	BannerId = -1 means "checked, none active".

	URL length capped at 200 (not the DB column's NVARCHAR(260)) because this
	struct rides the same C1 header as everything else here, whose size field
	is one BYTE - 4(header)+2(index)+11(account)+4(BannerId)+N+4(TargetPackageId)
	must stay under 255, so N tops out at 230; 200 leaves comfortable margin for
	any realistic generated URL (e.g. https://host/banner/banner_<ts>.jpg).
*/
#define CASH_SHOP_BANNER_URL_LENGTH 200

struct SDHP_CASH_SHOP_BANNER_SEND
{
	PSBMSG_HEAD header; // C1:18:0E
	WORD index;
	char account[11];
	int BannerId;       // -1 = no active banner
	char ImagePath[CASH_SHOP_BANNER_URL_LENGTH];
	int TargetPackageId; // stored, not yet acted on client-side
};

/*
	The display catalog, DataServer -> GameServer. Head 0x18, sub 0x10.

	This is the shop the PLAYER sees - the tabs, the cards, the duration
	variants - as opposed to the 0x08 catalog, which is what the server charges
	and grants. It lived only in three client-side script files until now, which
	is why editing a package in the admin tool changed nothing in game.

	THE WIRE FORMAT IS THE SCRIPT LINE. Each row is rendered as the same
	@-delimited string IBSCategory.txt, IBSPackage.txt and IBSProduct.txt hold,
	and the client feeds it to the same CShopCategory::SetCategory /
	CShopPackage::SetPackage / CShopProduct::SetProduct parsers it has always
	used. Nothing downstream of those parsers changes, and there is no row struct
	whose packing could disagree across the three binaries.

	FRAGMENTED, because a C1 header carries one BYTE of size. The fixed part of
	this message is 24 bytes, so one packet could hold at most a 231-byte line -
	and the longest package row in the live catalog is already 228. A description
	one word longer would have truncated silently. Lines are split into
	CASH_SHOP_CATALOG_FRAG-byte pieces and reassembled on the client;
	CATALOG_FLAG_LINE_END marks the last piece of a line.
*/
#define CASH_SHOP_CATALOG_FRAG 200

// RowKind. Below 0x80 is a real row, and the value picks the parser.
#define CASH_SHOP_CATALOG_CATEGORY  0
#define CASH_SHOP_CATALOG_PACKAGE   1
#define CASH_SHOP_CATALOG_PRODUCT   2
#define CASH_SHOP_CATALOG_UPTODATE  0xFE
#define CASH_SHOP_CATALOG_END       0xFF

#define CATALOG_FLAG_LINE_END 0x01

struct SDHP_CASH_SHOP_CATALOG_LINE_SEND
{
	PSBMSG_HEAD header;     // C1:18:10
	WORD index;             //  4..5
	char account[11];       //  6..16
	BYTE RowKind;           // 17
	BYTE Flags;             // 18
	BYTE Padding[1];        // 19  - pins Version to 20 regardless of packing
	int Version;            // 20..23
	char Fragment[CASH_SHOP_CATALOG_FRAG];
};

//**********************************************//
//********** GameServer -> DataServer **********//
//**********************************************//

/*
	"Send me the display catalog, unless it is still version N." Head 0x18, sub
	0x0F.

	Carries the version the client already holds, so an unchanged catalog costs
	one packet instead of a hundred. A client that has never asked sends 0, which
	no real version can be - CustomCashShopCatalogVersion starts at 1.
*/
struct SDHP_CASH_SHOP_CATALOG_LINE_REQ_RECV
{
	PSBMSG_HEAD header;     // C1:18:0F
	WORD index;
	char account[11];
	BYTE Padding[1];
	int CachedVersion;
};

struct SDHP_CASH_SHOP_POINT_RECV
{
	PSBMSG_HEAD header; // C1:18:00
	WORD index;
	char account[11];
};

/*
	Purchase audit row from the GameServer. Head 0x18, sub 0x07.

	One-way: the GameServer decides the purchase but holds no ODBC handle, so
	it hands the finished outcome here to be written. Nothing is sent back -
	the player has already been told the result.
*/
struct SDHP_CASH_SHOP_LOG_RECV
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
	BYTE Result;
	char IpAddress[16];
	short ServerCode;
};

struct SDHP_CASH_SHOP_ITEM_BUY_RECV
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

struct SDHP_CASH_SHOP_ITEM_GIF_RECV
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

struct SDHP_CASH_SHOP_ITEM_NUM_RECV
{
	PSBMSG_HEAD header; // C1:18:03
	WORD index;
	char account[11];
	UINT InventoryPage;
	BYTE InventoryType;
};

struct SDHP_CASH_SHOP_ITEM_USE_RECV
{
	PSBMSG_HEAD header; // C1:18:04
	WORD index;
	char account[11];
	DWORD BaseItemCode;
	DWORD MainItemCode;
	WORD ItemIndex;
	BYTE ProductType;
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
};

struct SDHP_CASH_SHOP_RECIEVE_POINT_RECV
{
	PSBMSG_HEAD header; // C1:18:06
	WORD index;
	char account[11];
	DWORD CallbackFunc;
	DWORD CallbackArg1;
	DWORD CallbackArg2;
};

struct SDHP_CASH_SHOP_ADD_POINT_SAVE_RECV
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

struct SDHP_CASH_SHOP_SUB_POINT_SAVE_RECV
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

struct SDHP_CASH_SHOP_INSERT_ITEM_SAVE_RECV
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

struct SDHP_CASH_SHOP_DELETE_ITEM_SAVE_RECV
{
	PSBMSG_HEAD header; // C1:18:33
	WORD index;
	char account[11];
	char GiftAccount[11];
	DWORD BaseItemCode;
	DWORD MainItemCode;
};

struct SDHP_CASH_SHOP_PERIODIC_ITEM_SAVE_RECV
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
//********** DataServer -> GameServer **********//
//**********************************************//

struct SDHP_CASH_SHOP_POINT_SEND
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

struct SDHP_CASH_SHOP_ITEM_BUY_SEND
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
	// --- pricing resolved here, read by the GameServer ---
	// Must stay byte-identical to SDHP_CASH_SHOP_ITEM_BUY_RECV in
	// 4.GameServer/CashShop.h. EffectivePrice = -1 means unresolved.
	int ListPrice;
	int EffectivePrice;
	int DiscountId;
	short DiscountPercent;
};

struct SDHP_CASH_SHOP_ITEM_GIF_SEND
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

struct SDHP_CASH_SHOP_ITEM_NUM_SEND
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

struct SDHP_CASH_SHOP_ITEM_USE_SEND
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
	DWORD time;
};

struct SDHP_CASH_SHOP_RECIEVE_POINT_SEND
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
//**********************************************//
//**********************************************//

class CCashShop
{
public:
	CCashShop();
	virtual ~CCashShop();
	void GDCashShopPointRecv(SDHP_CASH_SHOP_POINT_RECV* lpMsg,int index);
	void GDCashShopItemBuyRecv(SDHP_CASH_SHOP_ITEM_BUY_RECV* lpMsg,int index);
	void GDCashShopLogRecv(SDHP_CASH_SHOP_LOG_RECV* lpMsg,int index);
	void GDCashShopCatalogReqRecv(SDHP_CASH_SHOP_CATALOG_REQ_RECV* lpMsg,int index);
	void GDCashShopPriceReqRecv(SDHP_CASH_SHOP_PRICE_REQ_RECV* lpMsg,int index);
	void GDCashShopCatalogLineReqRecv(SDHP_CASH_SHOP_CATALOG_LINE_REQ_RECV* lpMsg,int index);
	void SendCatalogLine(int index,SDHP_CASH_SHOP_CATALOG_LINE_SEND* pMsg,BYTE rowKind,const char* line);
	void GDCashShopItemGifRecv(SDHP_CASH_SHOP_ITEM_GIF_RECV* lpMsg,int index);
	void GDCashShopItemNumRecv(SDHP_CASH_SHOP_ITEM_NUM_RECV* lpMsg,int index);
	void GDCashShopItemUseRecv(SDHP_CASH_SHOP_ITEM_USE_RECV* lpMsg,int index);
	void GDCashShopPeriodicItemRecv(SDHP_CASH_SHOP_PERIODIC_ITEM_RECV* lpMsg,int index);
	void GDCashShopRecievePointRecv(SDHP_CASH_SHOP_RECIEVE_POINT_RECV* lpMsg,int index);
	void GDCashShopAddPointSaveRecv(SDHP_CASH_SHOP_ADD_POINT_SAVE_RECV* lpMsg);
	void GDCashShopSubPointSaveRecv(SDHP_CASH_SHOP_SUB_POINT_SAVE_RECV* lpMsg);
	void GDCashShopInsertItemSaveRecv(SDHP_CASH_SHOP_INSERT_ITEM_SAVE_RECV* lpMsg);
	void GDCashShopDeleteItemSaveRecv(SDHP_CASH_SHOP_DELETE_ITEM_SAVE_RECV* lpMsg);
	void GDCashShopPeriodicItemSaveRecv(SDHP_CASH_SHOP_PERIODIC_ITEM_SAVE_RECV* lpMsg);
};

extern CCashShop gCashShop;
