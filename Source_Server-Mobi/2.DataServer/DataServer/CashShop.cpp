// CashShop.cpp: implementation of the CCashShop class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CashShop.h"
#include "QueryManager.h"
#include "SocketManager.h"
#include "Util.h"

CCashShop gCashShop;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCashShop::CCashShop() // OK
{

}

CCashShop::~CCashShop() // OK
{

}

void CCashShop::GDCashShopPointRecv(SDHP_CASH_SHOP_POINT_RECV* lpMsg, int index) // OK
{
	#if(DATASERVER_UPDATE>=501)

	SDHP_CASH_SHOP_POINT_SEND pMsg;

	pMsg.header.set(0x18,0x00,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	pMsg.result = 0;

	if(gQueryManager.ExecQuery("SELECT * FROM CashShopData WHERE AccountID='%s'",lpMsg->account) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		if(gQueryManager.ExecQuery("INSERT INTO CashShopData (AccountID,WCoinC,WCoinP,GoblinPoint,Ruud) VALUES ('%s',0,0,0,0)",lpMsg->account) == 0)
		{
			gQueryManager.Close();

			pMsg.result = 1;
		}
		else
		{
			gQueryManager.Close();

			pMsg.WCoinC = 0;

			pMsg.WCoinP = 0;

			pMsg.GoblinPoint = 0;

			pMsg.Ruud = 0;
		}
	}
	else
	{
		pMsg.WCoinC = gQueryManager.GetAsInteger("WCoinC");

		pMsg.WCoinP = gQueryManager.GetAsInteger("WCoinP");

		pMsg.GoblinPoint = gQueryManager.GetAsInteger("GoblinPoint");

		pMsg.Ruud = gQueryManager.GetAsInteger("Ruud");

		gQueryManager.Close();
	}

	//=== Check Coin
	bool UpdateCoinFix = false;
	if (CheckCoinBB(pMsg.WCoinC))
	{
		pMsg.WCoinC = 0;
		UpdateCoinFix = true;
	}
	if (CheckCoinBB(pMsg.WCoinP))
	{
		pMsg.WCoinP = 0;
		UpdateCoinFix = true;
	}
	if (CheckCoinBB(pMsg.GoblinPoint))
	{
		pMsg.GoblinPoint = 0;
		UpdateCoinFix = true;
	}
	if (CheckCoinBB(pMsg.Ruud))
	{
		pMsg.Ruud = 0;
		UpdateCoinFix = true;
	}
	if (UpdateCoinFix)
	{
		gQueryManager.ExecQuery("UPDATE CashShopData SET WCoinC=%d,WCoinP=%d,GoblinPoint=%d,Ruud=%d WHERE AccountID='%s'", pMsg.WCoinC, pMsg.WCoinP, pMsg.GoblinPoint, pMsg.Ruud, lpMsg->account);
		gQueryManager.Close();
	}
	//====

	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));

	#else

	SDHP_CASH_SHOP_POINT_SEND pMsg;

	pMsg.header.set(0x18,0x00,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	pMsg.result = 1;

	if(gQueryManager.ExecQuery("EXEC WZ_GetCoin '%s'",lpMsg->account) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		pMsg.WCoinC = 0;
		pMsg.WCoinP = 0;
		pMsg.GoblinPoint = 0;
		gQueryManager.Close();
	}
	else
	{
		pMsg.WCoinC = gQueryManager.GetResult(0);
		pMsg.WCoinP = gQueryManager.GetResult(1);
		pMsg.GoblinPoint = gQueryManager.GetResult(2);
		gQueryManager.Close();
	}
	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

/*
	Escapes a fixed-width, possibly unterminated field from the wire into a SQL
	string literal.

	ExecQuery is printf-style with no bound parameters, so every string that
	reaches it has to be made safe here. Account and character names are already
	constrained by the game, but "already constrained" is how the heartbeat
	format-string bug got in - the field is doubled up on quotes regardless.

	The source arrays are memcpy'd on the sending side and are not guaranteed to
	carry a terminator, hence the explicit length bound rather than strlen.
*/
static void EscapeSqlField(const char* src,int srcSize,char* out,int outSize)
{
	int w = 0;

	for(int r = 0; r < srcSize && w < (outSize-2); r++)
	{
		if(src[r] == 0)
		{
			break;
		}

		if(src[r] == '\'')
		{
			out[w++] = '\'';
		}

		out[w++] = src[r];
	}

	out[w] = 0;
}

/*
	Catalog transfer. Sends the whole shop to the asking GameServer as a BEGIN
	carrying the counts, then one message per product, then one per package with
	its product slots inline and IsLast set on the final row.

	Queries are read in full before anything is sent: ExecQuery/Fetch/Close is a
	single shared cursor on this connection, so a nested query while one is open
	would clobber it. Products are buffered, then packages are streamed only
	after the product cursor is closed.
*/
void CCashShop::GDCashShopCatalogReqRecv(SDHP_CASH_SHOP_CATALOG_REQ_RECV* lpMsg,int index) // OK
{
	#if(DATASERVER_UPDATE>=501)

	// Heap, not static. This runs on an IOCP worker and two GameServers can ask
	// at once; static buffers would be shared across those calls. Boot-time
	// allocation cost is irrelevant next to that.
	SDHP_CASH_SHOP_CATALOG_PRODUCT_SEND* products = new SDHP_CASH_SHOP_CATALOG_PRODUCT_SEND[MAX_CASH_SHOP_ITEM];
	SDHP_CASH_SHOP_CATALOG_PACKAGE_SEND* packages = new SDHP_CASH_SHOP_CATALOG_PACKAGE_SEND[MAX_CASH_SHOP_ITEM];
	int* packageIds = new int[MAX_CASH_SHOP_ITEM];

	int productCount = 0;

	if(gQueryManager.ExecQuery(
		"SELECT BaseIndex,MainIndex,CoinValue,ItemIndex,ItemLevel,"
		"ItemOption1,ItemOption2,ItemOption3,ItemNewOption,ItemSetOption,"
		"ItemHarmonyOption,ItemOptionEx,ItemSocketOption1,ItemSocketOption2,"
		"ItemSocketOption3,ItemSocketOption4,ItemSocketOption5,"
		"ItemQuantity,ItemDuration FROM CustomCashShopProducts "
		"ORDER BY BaseIndex,MainIndex") == 0)
	{
		gQueryManager.Close();
		LogAdd(LOG_RED,"[CashShop] catalog product query failed - the GameServer keeps its .txt catalog");
		delete[] products; delete[] packages; delete[] packageIds;
		return;
	}

	while(gQueryManager.Fetch() != SQL_NO_DATA && productCount < MAX_CASH_SHOP_ITEM)
	{
		SDHP_CASH_SHOP_CATALOG_PRODUCT_SEND* p = &products[productCount++];

		p->header.set(0x18,0x09,sizeof(SDHP_CASH_SHOP_CATALOG_PRODUCT_SEND));

		p->BaseIndex = gQueryManager.GetAsInteger("BaseIndex");
		p->MainIndex = gQueryManager.GetAsInteger("MainIndex");
		p->CoinValue = gQueryManager.GetAsInteger("CoinValue");
		p->ItemIndex = gQueryManager.GetAsInteger("ItemIndex");
		p->ItemLevel = gQueryManager.GetAsInteger("ItemLevel");
		p->ItemOption1 = gQueryManager.GetAsInteger("ItemOption1");
		p->ItemOption2 = gQueryManager.GetAsInteger("ItemOption2");
		p->ItemOption3 = gQueryManager.GetAsInteger("ItemOption3");
		p->ItemNewOption = gQueryManager.GetAsInteger("ItemNewOption");
		p->ItemSetOption = gQueryManager.GetAsInteger("ItemSetOption");
		p->ItemHarmonyOption = gQueryManager.GetAsInteger("ItemHarmonyOption");
		p->ItemOptionEx = gQueryManager.GetAsInteger("ItemOptionEx");
		p->ItemSocketOption1 = gQueryManager.GetAsInteger("ItemSocketOption1");
		p->ItemSocketOption2 = gQueryManager.GetAsInteger("ItemSocketOption2");
		p->ItemSocketOption3 = gQueryManager.GetAsInteger("ItemSocketOption3");
		p->ItemSocketOption4 = gQueryManager.GetAsInteger("ItemSocketOption4");
		p->ItemSocketOption5 = gQueryManager.GetAsInteger("ItemSocketOption5");
		p->ItemQuantity = gQueryManager.GetAsInteger("ItemQuantity");
		p->ItemDuration = gQueryManager.GetAsInteger("ItemDuration");
	}

	gQueryManager.Close();


	int packageCount = 0;

	// Product slots come back as one row per link; PackageId orders them so the
	// slots of a package arrive together.
	if(gQueryManager.ExecQuery(
		"SELECT p.PackageId,p.Category,p.BaseIndex,p.MainIndex,p.ItemIndex,"
		"p.CoinIndex,p.CoinValue,p.BonusGP "
		"FROM CustomCashShopPackages p WHERE p.Enabled = 1 "
		"ORDER BY p.Category,p.SortOrder,p.BaseIndex") == 0)
	{
		gQueryManager.Close();
		LogAdd(LOG_RED,"[CashShop] catalog package query failed - the GameServer keeps its .txt catalog");
		delete[] products; delete[] packages; delete[] packageIds;
		return;
	}


	while(gQueryManager.Fetch() != SQL_NO_DATA && packageCount < MAX_CASH_SHOP_ITEM)
	{
		SDHP_CASH_SHOP_CATALOG_PACKAGE_SEND* k = &packages[packageCount];

		memset(k,0,sizeof(SDHP_CASH_SHOP_CATALOG_PACKAGE_SEND));

		k->header.set(0x18,0x0A,sizeof(SDHP_CASH_SHOP_CATALOG_PACKAGE_SEND));

		packageIds[packageCount] = gQueryManager.GetAsInteger("PackageId");

		k->Category = gQueryManager.GetAsInteger("Category");
		k->BaseIndex = gQueryManager.GetAsInteger("BaseIndex");
		k->MainIndex = gQueryManager.GetAsInteger("MainIndex");
		k->ItemIndex = gQueryManager.GetAsInteger("ItemIndex");
		k->CoinIndex = gQueryManager.GetAsInteger("CoinIndex");
		k->CoinValue = gQueryManager.GetAsInteger("CoinValue");
		k->BonusGP = gQueryManager.GetAsInteger("BonusGP");

		packageCount++;
	}

	gQueryManager.Close();

	// Slots in a second pass, for the same one-cursor reason.
	if(gQueryManager.ExecQuery(
		"SELECT PackageId,Slot,ProductBaseIndex,ProductMainIndex "
		"FROM CustomCashShopPackageProducts ORDER BY PackageId,Slot") != 0)
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA)
		{
			int pid = gQueryManager.GetAsInteger("PackageId");
			int slot = gQueryManager.GetAsInteger("Slot");

			if(slot < 0 || slot >= MAX_CASH_SHOP_PACKAGE_PRODUCT)
			{
				continue;
			}

			for(int n=0;n < packageCount;n++)
			{
				if(packageIds[n] != pid)
				{
					continue;
				}

				packages[n].ProductBaseIndex[slot] = gQueryManager.GetAsInteger("ProductBaseIndex");
				packages[n].ProductMainIndex[slot] = gQueryManager.GetAsInteger("ProductMainIndex");
				break;
			}
		}
	}

	gQueryManager.Close();

	SDHP_CASH_SHOP_CATALOG_BEGIN_SEND begin;

	begin.header.set(0x18,0x08,sizeof(begin));

	begin.ProductCount = productCount;

	begin.PackageCount = packageCount;

	gSocketManager.DataSend(index,(BYTE*)&begin,sizeof(begin));

	for(int n=0;n < productCount;n++)
	{
		gSocketManager.DataSend(index,(BYTE*)&products[n],sizeof(products[n]));
	}

	for(int n=0;n < packageCount;n++)
	{
		packages[n].IsLast = ((n==(packageCount-1))?1:0);

		gSocketManager.DataSend(index,(BYTE*)&packages[n],sizeof(packages[n]));
	}

	LogAdd(LOG_BLUE,"[CashShop] catalog sent: %d products, %d packages",productCount,packageCount);

	delete[] products; delete[] packages; delete[] packageIds;

	#endif
}

/*
	Current shelf prices, answering the GameServer's 0x0B.

	Two kinds of row go back in one stream, tagged by RowKind:

	  0  package  - MainIndex is CustomCashShopPackages.MainIndex, which is what
	                the client calls PackageProductSeq and what the buy request
	                sends. This is what the shop shelf draws.

	  1  product  - MainIndex is a product variant's MainIndex. Duration variants
	                (Angel Charm 1/7/30 days) share a BaseIndex and are what most
	                purchases actually go through, so without these the shelf
	                would show a sale price and the item-select dialog would still
	                quote list.

	Both come from the effective-price views, the same ones the purchase path
	re-validates against, so the shelf and the charge cannot drift apart.

	Rows are gathered before any are sent because there is one ODBC cursor: the
	product query cannot open while the package query is still being read.

	A failed query sends nothing rather than an empty terminal batch - the client
	then keeps drawing its own .txt prices, which is the right thing to show when
	the truth is unknown.
*/
static int CatalogHexVal(char c)
{
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

/*
	An NVARCHAR column, fetched without going through this process's ANSI
	codepage.

	CQueryManager binds every column as SQL_C_CHAR, so the driver converts
	NVARCHAR to the DataServer's ACP - and on a Western ACP every Vietnamese
	character comes back as '?'. The data in SQL is fine; the loss is at the
	fetch, and it is silent.

	So the catalog queries ask for CONVERT(VARBINARY(MAX), col) rendered as hex,
	which is pure ASCII and survives any codepage, and this turns it back into
	UTF-8. The client already knows how to decode UTF-8 - that is exactly what
	its script files are, and CShopList::GetDecodeingString has always done it.

	Anything malformed yields an empty string rather than a guess: a wrong name
	is worse than a missing one, because it looks deliberate.
*/
static void CatalogHexToUtf8(const char* hex,char* out,int outSize)
{
	if(out == 0 || outSize <= 0)
	{
		return;
	}

	out[0] = 0;

	if(hex == 0)
	{
		return;
	}

	int len = (int)strlen(hex);

	// One UTF-16 code unit is two bytes, so four hex characters.
	if(len < 4 || (len % 4) != 0)
	{
		return;
	}

	int count = len / 4;

	WCHAR* wide = new WCHAR[count+1];

	for(int i = 0; i < count; i++)
	{
		int b0hi = CatalogHexVal(hex[(i*4)+0]);
		int b0lo = CatalogHexVal(hex[(i*4)+1]);
		int b1hi = CatalogHexVal(hex[(i*4)+2]);
		int b1lo = CatalogHexVal(hex[(i*4)+3]);

		if(b0hi < 0 || b0lo < 0 || b1hi < 0 || b1lo < 0)
		{
			delete[] wide;
			out[0] = 0;
			return;
		}

		// Little-endian: the low byte of the code unit comes first.
		int lo = (b0hi << 4) | b0lo;
		int hi = (b1hi << 4) | b1lo;

		wide[i] = (WCHAR)((hi << 8) | lo);
	}

	wide[count] = 0;

	if(WideCharToMultiByte(CP_UTF8,0,wide,-1,out,outSize,0,0) == 0)
	{
		out[0] = 0;
	}

	out[outSize-1] = 0;

	delete[] wide;
}

/*
	Every text field that goes into a catalog row, made safe for the row.

	Two characters cannot survive a trip through the @-delimited line and both
	are things an operator will type without a second thought:

	- '@' is the field delimiter. One of them in a description shifts the gift
	  flag, the item code and the price unit name each one field to the left, and
	  the client parses the result without complaining about any of it.

	- CR and LF mean nothing to the client. Its only line break is '#'
	  (DivideStringByPixel(..., '#')), and CR/LF reach GDI as notdef glyphs on the
	  same line - which is what made a two-line description render as one run of
	  text with a gap in the middle of it.

	CashShopAdmin now stores '#' and turns '@' into a space itself, so for
	anything typed there this is a no-op. It runs anyway because the tables are
	also edited by hand in SQL, and because it repairs rows saved before that
	change without anyone having to re-save every package.

	Applied to every text field rather than just the description: the others are
	shorter and less likely, but a stray '@' misaligns the row identically
	wherever it comes from.
*/
static void CatalogScrubText(char* text)
{
	if(text == 0)
	{
		return;
	}

	int write = 0;

	for(int read = 0; text[read] != 0; read++)
	{
		unsigned char ch = (unsigned char)text[read];

		if(ch == '@')
		{
			text[write++] = ' ';
			continue;
		}

		if(ch == '\r' || ch == '\n')
		{
			// Collapse a CRLF pair, and any run of blank lines, into one break -
			// strtok in DivideStringByPixel does the same thing with consecutive
			// delimiters, so anything else would only spend bytes.
			if(write > 0 && text[write-1] == '#')
			{
				continue;
			}

			text[write++] = '#';
			continue;
		}

		// Other C0 controls: a tab or a stray 0x1A has no glyph and no meaning
		// here. Multi-byte UTF-8 continuation bytes are all >= 0x80 and are left
		// alone.
		if(ch < 0x20)
		{
			continue;
		}

		text[write++] = text[read];
	}

	text[write] = 0;
}

/*
	One catalog row, fragmented across as many packets as it needs.

	Kept apart from the handler because every row goes through it and the
	fragment arithmetic is the one part worth reading twice: a line is cut into
	CASH_SHOP_CATALOG_FRAG-byte pieces, and only the piece that finishes the line
	carries CATALOG_FLAG_LINE_END. A line that lands exactly on the boundary
	still gets its flag on the last full piece rather than an empty extra one.
*/
void CCashShop::SendCatalogLine(int index,SDHP_CASH_SHOP_CATALOG_LINE_SEND* pMsg,BYTE rowKind,const char* line)
{
	int len = (int)strlen(line);
	int sent = 0;

	do
	{
		int chunk = len - sent;

		if(chunk > CASH_SHOP_CATALOG_FRAG)
		{
			chunk = CASH_SHOP_CATALOG_FRAG;
		}

		memset(pMsg->Fragment,0,sizeof(pMsg->Fragment));

		if(chunk > 0)
		{
			memcpy(pMsg->Fragment,line+sent,chunk);
		}

		sent += chunk;

		pMsg->RowKind = rowKind;
		pMsg->Flags = ((sent >= len)?CATALOG_FLAG_LINE_END:0);

		gSocketManager.DataSend(index,(BYTE*)pMsg,sizeof(*pMsg));
	}
	while(sent < len);
}

/*
	The display catalog. Head 0x18, sub 0x0F in, sub 0x10 out.

	ORDER MATTERS AND IS NOT COSMETIC. The client links a package to its tab
	inside CShopList::LoadPackage, which calls InsertPackage(category, package) -
	and CShopCategoryList::InsertPackage returns 0 without a word when the
	category is not there yet. Categories therefore go first, then packages, then
	products. The client buffers and replays in this order anyway, but sending
	them out of order would be inviting the one failure nobody can see.

	Every query failure is non-fatal by design: the client keeps whatever catalog
	it already had, which is its own script files. An empty shop is far worse
	than a stale one.
*/
void CCashShop::GDCashShopCatalogLineReqRecv(SDHP_CASH_SHOP_CATALOG_LINE_REQ_RECV* lpMsg,int index) // OK
{
	#if(DATASERVER_UPDATE>=501)

	SDHP_CASH_SHOP_CATALOG_LINE_SEND pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0x18,0x10,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	int version = 0;

	if(gQueryManager.ExecQuery("SELECT TOP 1 Version FROM CustomCashShopCatalogVersion") != 0)
	{
		if(gQueryManager.Fetch() != SQL_NO_DATA)
		{
			version = gQueryManager.GetAsInteger("Version");
		}
	}

	gQueryManager.Close();

	if(version == 0)
	{
		// No version row means 03_catalog_schema.sql has not been run. Say so
		// once and leave the client on its script files.
		LogAdd(LOG_RED,"[CashShop] no catalog version - run 03_catalog_schema.sql; client keeps its .txt catalog");

		pMsg.Version = 0;
		pMsg.RowKind = CASH_SHOP_CATALOG_UPTODATE;
		pMsg.Flags = CATALOG_FLAG_LINE_END;
		gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));
		return;
	}

	pMsg.Version = version;

	if(lpMsg->CachedVersion == version)
	{
		pMsg.RowKind = CASH_SHOP_CATALOG_UPTODATE;
		pMsg.Flags = CATALOG_FLAG_LINE_END;
		gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));
		return;
	}

	/*
		2048, not 1024. Every text field here is bounded, but the bounds sum to
		about 1550 bytes before the numeric fields are counted - so a row built
		from legal-but-long values already overran the old buffer, and the row is
		built with a printf that does not check. Both halves of that are fixed:
		the buffer has real headroom and the builds below are bounded.
	*/
	char line[2048];
	char buffer[512];

	int categories = 0;
	int packages = 0;
	int products = 0;

	// ---- categories, first, for the reason in the comment above -------------
	if(gQueryManager.ExecQuery(
		"SELECT ProductDisplaySeq,"
		"CONVERT(VARCHAR(MAX),CONVERT(VARBINARY(MAX),ISNULL(CategoryName,N'')),2) AS CategoryNameHex,"
		"EventFlag,OpenFlag,"
		"ParentProductDisplaySeq,DisplayOrder,Root "
		"FROM CustomCashShopCategories WHERE Enabled = 1 "
		"ORDER BY Root DESC,DisplayOrder,ProductDisplaySeq") != 0)
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA)
		{
			char hex[4096];

			memset(hex,0,sizeof(hex));
			memset(buffer,0,sizeof(buffer));
			gQueryManager.GetAsString("CategoryNameHex",hex,sizeof(hex));
			CatalogHexToUtf8(hex,buffer,sizeof(buffer));
			CatalogScrubText(buffer);

			_snprintf(line,sizeof(line)-1,"%d@%s@%d@%d@%d@%d@%d",
				gQueryManager.GetAsInteger("ProductDisplaySeq"),
				buffer,
				gQueryManager.GetAsInteger("EventFlag"),
				gQueryManager.GetAsInteger("OpenFlag"),
				gQueryManager.GetAsInteger("ParentProductDisplaySeq"),
				gQueryManager.GetAsInteger("DisplayOrder"),
				gQueryManager.GetAsInteger("Root"));

			line[sizeof(line)-1] = 0;

			this->SendCatalogLine(index,&pMsg,CASH_SHOP_CATALOG_CATEGORY,line);
			categories++;
		}
	}
	else
	{
		LogAdd(LOG_RED,"[CashShop] catalog category query failed - client keeps its .txt catalog");
	}

	gQueryManager.Close();

	// ---- product rows -------------------------------------------------------
	if(gQueryManager.ExecQuery(
		"SELECT ProductSeq,"
		"CONVERT(VARCHAR(MAX),CONVERT(VARBINARY(MAX),ISNULL(ProductName,N'')),2) AS ProductNameHex,"
		"CONVERT(VARCHAR(MAX),CONVERT(VARBINARY(MAX),ISNULL(PropertyName,N'')),2) AS PropertyNameHex,"
		"CONVERT(VARCHAR(MAX),CONVERT(VARBINARY(MAX),ISNULL(PropertyValue,N'')),2) AS PropertyValueHex,"
		"CONVERT(VARCHAR(MAX),CONVERT(VARBINARY(MAX),ISNULL(UnitName,N'')),2) AS UnitNameHex,"
		"Price,"
		"PriceSeq,PropertyType,MustFlag,vOrder,DeleteFlag,StorageGroup,ShareFlag,"
		"InGamePackageID,PropertySeq,ProductType,UnitType "
		"FROM CustomCashShopProductRows "
		"ORDER BY ProductSeq,PriceSeq,PropertySeq") != 0)
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA)
		{
			char name[256];
			char propName[128];
			char propValue[128];
			char unitName[128];
			char itemId[64];

			memset(name,0,sizeof(name));
			memset(propName,0,sizeof(propName));
			memset(propValue,0,sizeof(propValue));
			memset(unitName,0,sizeof(unitName));
			memset(itemId,0,sizeof(itemId));

			char hex[4096];

			memset(hex,0,sizeof(hex));
			gQueryManager.GetAsString("ProductNameHex",hex,sizeof(hex));
			CatalogHexToUtf8(hex,name,sizeof(name));

			memset(hex,0,sizeof(hex));
			gQueryManager.GetAsString("PropertyNameHex",hex,sizeof(hex));
			CatalogHexToUtf8(hex,propName,sizeof(propName));

			memset(hex,0,sizeof(hex));
			gQueryManager.GetAsString("PropertyValueHex",hex,sizeof(hex));
			CatalogHexToUtf8(hex,propValue,sizeof(propValue));

			memset(hex,0,sizeof(hex));
			gQueryManager.GetAsString("UnitNameHex",hex,sizeof(hex));
			CatalogHexToUtf8(hex,unitName,sizeof(unitName));

			// Item codes and seq lists are digits and pipes - ASCII either way, so
			// they come straight across without the hex round trip.
			gQueryManager.GetAsString("InGamePackageID",itemId,sizeof(itemId));

			CatalogScrubText(name);
			CatalogScrubText(propName);
			CatalogScrubText(propValue);
			CatalogScrubText(unitName);
			CatalogScrubText(itemId);

			_snprintf(line,sizeof(line)-1,"%d@%s@%s@%s@%s@%d@%d@%d@%d@%d@%d@%d@%d@%s@%d@%d@%d",
				gQueryManager.GetAsInteger("ProductSeq"),
				name,propName,propValue,unitName,
				gQueryManager.GetAsInteger("Price"),
				gQueryManager.GetAsInteger("PriceSeq"),
				gQueryManager.GetAsInteger("PropertyType"),
				gQueryManager.GetAsInteger("MustFlag"),
				gQueryManager.GetAsInteger("vOrder"),
				gQueryManager.GetAsInteger("DeleteFlag"),
				gQueryManager.GetAsInteger("StorageGroup"),
				gQueryManager.GetAsInteger("ShareFlag"),
				itemId,
				gQueryManager.GetAsInteger("PropertySeq"),
				gQueryManager.GetAsInteger("ProductType"),
				gQueryManager.GetAsInteger("UnitType"));

			line[sizeof(line)-1] = 0;

			this->SendCatalogLine(index,&pMsg,CASH_SHOP_CATALOG_PRODUCT,line);
			products++;
		}
	}
	else
	{
		LogAdd(LOG_RED,"[CashShop] catalog product query failed - variants will be missing");
	}

	gQueryManager.Close();

	// ---- packages, last, so every category they name already exists ---------
	if(gQueryManager.ExecQuery(
		"SELECT Category,ViewOrder,MainIndex,"
		"CONVERT(VARCHAR(MAX),CONVERT(VARBINARY(MAX),ISNULL(DisplayName,N'')),2) AS DisplayNameHex,"
		"PackageProductType,CoinValue,"
		"CONVERT(VARCHAR(MAX),CONVERT(VARBINARY(MAX),ISNULL(Description,N'')),2) AS DescriptionHex,"
		"CONVERT(VARCHAR(MAX),CONVERT(VARBINARY(MAX),ISNULL(Caution,N'')),2) AS CautionHex,"
		"SalesFlag,GiftFlag,StartDateRaw,EndDateRaw,CapsuleFlag,"
		"CapsuleCount,"
		"CONVERT(VARCHAR(MAX),CONVERT(VARBINARY(MAX),ISNULL(ProductCashName,N'')),2) AS ProductCashNameHex,"
		"CONVERT(VARCHAR(MAX),CONVERT(VARBINARY(MAX),ISNULL(PriceUnitName,N'')),2) AS PriceUnitNameHex,"
		"DeleteFlag,ScriptEventFlag,"
		"ProductAmount,ProductSeqList,InGamePackageID,ProductCashSeq,PriceCount,"
		"PriceSeqList,DeductMileageFlag,CoinIndex,CashTypeFlag "
		"FROM CustomCashShopPackages WHERE Enabled = 1 "
		"ORDER BY Category,ViewOrder,MainIndex") != 0)
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA)
		{
			char name[128];
			char description[256];
			char caution[128];
			char startRaw[64];
			char endRaw[64];
			char cashName[128];
			char unitName[128];
			char prodList[256];
			char itemId[64];
			char priceList[256];

			memset(name,0,sizeof(name));
			memset(description,0,sizeof(description));
			memset(caution,0,sizeof(caution));
			memset(startRaw,0,sizeof(startRaw));
			memset(endRaw,0,sizeof(endRaw));
			memset(cashName,0,sizeof(cashName));
			memset(unitName,0,sizeof(unitName));
			memset(prodList,0,sizeof(prodList));
			memset(itemId,0,sizeof(itemId));
			memset(priceList,0,sizeof(priceList));

			char hex[4096];

			memset(hex,0,sizeof(hex));
			gQueryManager.GetAsString("DisplayNameHex",hex,sizeof(hex));
			CatalogHexToUtf8(hex,name,sizeof(name));

			memset(hex,0,sizeof(hex));
			gQueryManager.GetAsString("DescriptionHex",hex,sizeof(hex));
			CatalogHexToUtf8(hex,description,sizeof(description));

			memset(hex,0,sizeof(hex));
			gQueryManager.GetAsString("CautionHex",hex,sizeof(hex));
			CatalogHexToUtf8(hex,caution,sizeof(caution));

			memset(hex,0,sizeof(hex));
			gQueryManager.GetAsString("ProductCashNameHex",hex,sizeof(hex));
			CatalogHexToUtf8(hex,cashName,sizeof(cashName));

			memset(hex,0,sizeof(hex));
			gQueryManager.GetAsString("PriceUnitNameHex",hex,sizeof(hex));
			CatalogHexToUtf8(hex,unitName,sizeof(unitName));

			// Dates, seq lists and item codes are ASCII by construction.
			gQueryManager.GetAsString("StartDateRaw",startRaw,sizeof(startRaw));
			gQueryManager.GetAsString("EndDateRaw",endRaw,sizeof(endRaw));
			gQueryManager.GetAsString("ProductSeqList",prodList,sizeof(prodList));
			gQueryManager.GetAsString("InGamePackageID",itemId,sizeof(itemId));
			gQueryManager.GetAsString("PriceSeqList",priceList,sizeof(priceList));

			// StartDateRaw and EndDateRaw are "0" in every row today and the
			// client's ConvertStringToDateTime accepts that; an empty string
			// would shift nothing but is not what it has ever been handed.
			if(startRaw[0] == 0) { strcpy(startRaw,"0"); }
			if(endRaw[0] == 0) { strcpy(endRaw,"0"); }

			// The description is the field an operator writes prose into, so it
			// is the one that actually carried a newline into the client. The
			// rest are scrubbed for the '@' alignment hazard.
			CatalogScrubText(name);
			CatalogScrubText(description);
			CatalogScrubText(caution);
			CatalogScrubText(cashName);
			CatalogScrubText(unitName);
			CatalogScrubText(prodList);
			CatalogScrubText(itemId);
			CatalogScrubText(priceList);

			_snprintf(line,sizeof(line)-1,"%d@%d@%d@%s@%d@%d@%s@%s@%d@%d@%s@%s@%d@%d@%s@%s@%d@%d@%d@%s@%s@%d@%d@%s@%d@%d@%d",
				gQueryManager.GetAsInteger("Category"),
				gQueryManager.GetAsInteger("ViewOrder"),
				gQueryManager.GetAsInteger("MainIndex"),
				name,
				gQueryManager.GetAsInteger("PackageProductType"),
				gQueryManager.GetAsInteger("CoinValue"),
				description,caution,
				gQueryManager.GetAsInteger("SalesFlag"),
				gQueryManager.GetAsInteger("GiftFlag"),
				startRaw,endRaw,
				gQueryManager.GetAsInteger("CapsuleFlag"),
				gQueryManager.GetAsInteger("CapsuleCount"),
				cashName,unitName,
				gQueryManager.GetAsInteger("DeleteFlag"),
				gQueryManager.GetAsInteger("ScriptEventFlag"),
				gQueryManager.GetAsInteger("ProductAmount"),
				prodList,itemId,
				gQueryManager.GetAsInteger("ProductCashSeq"),
				gQueryManager.GetAsInteger("PriceCount"),
				priceList,
				gQueryManager.GetAsInteger("DeductMileageFlag"),
				gQueryManager.GetAsInteger("CoinIndex"),
				gQueryManager.GetAsInteger("CashTypeFlag"));

			line[sizeof(line)-1] = 0;

			this->SendCatalogLine(index,&pMsg,CASH_SHOP_CATALOG_PACKAGE,line);
			packages++;
		}
	}
	else
	{
		LogAdd(LOG_RED,"[CashShop] catalog package query failed - client keeps its .txt catalog");
	}

	gQueryManager.Close();

	// The client only commits what it has buffered when this arrives, so a
	// half-sent catalog leaves the old one in place rather than a broken one.
	memset(pMsg.Fragment,0,sizeof(pMsg.Fragment));
	pMsg.RowKind = CASH_SHOP_CATALOG_END;
	pMsg.Flags = CATALOG_FLAG_LINE_END;
	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));

	LogAdd(LOG_BLUE,"[CashShop] catalog v%d sent: %d categories, %d packages, %d product rows",
		version,categories,packages,products);

	#endif
}

void CCashShop::GDCashShopPriceReqRecv(SDHP_CASH_SHOP_PRICE_REQ_RECV* lpMsg,int index) // OK
{
	#if(DATASERVER_UPDATE>=501)

	// Heap for the same reason as the catalog: this runs on an IOCP worker and two
	// GameServers can ask at once.
	int capacity = MAX_CASH_SHOP_ITEM * 2;

	CASH_SHOP_PRICE_ROW* rows = new CASH_SHOP_PRICE_ROW[capacity];

	int count = 0;
	int packages = 0;
	int discounted = 0;

	if(gQueryManager.ExecQuery(
		"SELECT p.MainIndex,e.ListPrice,e.EffectivePrice,e.DiscountPercent "
		"FROM CustomCashShopPackages p "
		"JOIN CustomCashShopEffectivePrice e ON e.PackageId = p.PackageId "
		"WHERE p.Enabled = 1 "
		"ORDER BY p.MainIndex") == 0)
	{
		gQueryManager.Close();
		LogAdd(LOG_RED,"[CashShop] package price query failed - the client keeps its .txt prices");
		delete[] rows;
		return;
	}

	while(gQueryManager.Fetch() != SQL_NO_DATA && count < capacity)
	{
		CASH_SHOP_PRICE_ROW* row = &rows[count++];

		row->MainIndex = gQueryManager.GetAsInteger("MainIndex");
		row->ListPrice = gQueryManager.GetAsInteger("ListPrice");
		row->EffectivePrice = gQueryManager.GetAsInteger("EffectivePrice");
		row->DiscountPercent = (short)gQueryManager.GetAsInteger("DiscountPercent");
		row->RowKind = 0;

		if(row->DiscountPercent > 0)
		{
			discounted++;
		}

		packages++;
	}

	gQueryManager.Close();

	/*
		One product can be reachable from more than one package, and those packages
		can be running different sales. Collapsing to the best price the player
		could get matches the "highest active percent wins" rule the view already
		applies within a package - MIN on the price rather than MAX on the percent,
		because MinPrice floors can make a bigger percent the worse deal.
	*/
	if(gQueryManager.ExecQuery(
		"SELECT ProductMainIndex,MAX(ListPrice) AS ListPrice,"
		"MIN(EffectivePrice) AS EffectivePrice,MAX(DiscountPercent) AS DiscountPercent "
		"FROM CustomCashShopEffectiveProductPrice "
		"GROUP BY ProductMainIndex "
		"ORDER BY ProductMainIndex") != 0)
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA && count < capacity)
		{
			CASH_SHOP_PRICE_ROW* row = &rows[count++];

			row->MainIndex = gQueryManager.GetAsInteger("ProductMainIndex");
			row->ListPrice = gQueryManager.GetAsInteger("ListPrice");
			row->EffectivePrice = gQueryManager.GetAsInteger("EffectivePrice");
			row->DiscountPercent = (short)gQueryManager.GetAsInteger("DiscountPercent");
			row->RowKind = 1;
		}
	}
	else
	{
		// Not fatal: the shelf still gets its package prices, and the item-select
		// dialog falls back to the script price it has always shown.
		LogAdd(LOG_RED,"[CashShop] product price query failed - variant prices stay at list");
	}

	gQueryManager.Close();

	SDHP_CASH_SHOP_PRICE_SEND pMsg;

	memset(&pMsg,0,sizeof(pMsg));

	pMsg.header.set(0x18,0x0C,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	int sent = 0;

	// Always at least one packet, so IsLast arrives even for an empty catalog.
	do
	{
		int batch = count - sent;

		if(batch > MAX_CASH_SHOP_PRICE_BATCH)
		{
			batch = MAX_CASH_SHOP_PRICE_BATCH;
		}

		if(batch > 0)
		{
			memcpy(pMsg.Row,&rows[sent],batch*sizeof(CASH_SHOP_PRICE_ROW));
		}

		sent += batch;

		pMsg.Count = (BYTE)batch;
		pMsg.IsLast = ((sent >= count)?1:0);

		gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));
	}
	while(sent < count);

	LogAdd(LOG_BLUE,"[CashShop] prices sent: %d packages (%d on sale), %d product variants",packages,discounted,count-packages);

	delete[] rows;

	/*
		Package display-name overrides, piggybacked on this same round trip -
		no separate request sub-code. Only packages with a non-null, non-empty
		DisplayName are queried, so this can legitimately be a zero-row batch;
		one IsLast-bearing packet is still always sent, same reason as prices.
	*/
	CASH_SHOP_NAME_ROW* nameRows = new CASH_SHOP_NAME_ROW[MAX_CASH_SHOP_ITEM];
	int nameCount = 0;

	if(gQueryManager.ExecQuery(
		"SELECT MainIndex,DisplayName FROM CustomCashShopPackages "
		"WHERE Enabled = 1 AND DisplayName IS NOT NULL AND DisplayName <> '' "
		"ORDER BY MainIndex") == 0)
	{
		gQueryManager.Close();
		LogAdd(LOG_RED,"[CashShop] package name query failed - the client keeps its .txt names");
	}
	else
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA && nameCount < MAX_CASH_SHOP_ITEM)
		{
			CASH_SHOP_NAME_ROW* row = &nameRows[nameCount++];

			row->MainIndex = gQueryManager.GetAsInteger("MainIndex");
			memset(row->DisplayName,0,sizeof(row->DisplayName));
			gQueryManager.GetAsString("DisplayName",row->DisplayName,sizeof(row->DisplayName));
		}

		gQueryManager.Close();
	}

	SDHP_CASH_SHOP_NAME_SEND nMsg;

	memset(&nMsg,0,sizeof(nMsg));

	nMsg.header.set(0x18,0x0D,sizeof(nMsg));

	nMsg.index = lpMsg->index;

	memcpy(nMsg.account,lpMsg->account,sizeof(nMsg.account));

	int nameSent = 0;

	do
	{
		int batch = nameCount - nameSent;

		if(batch > MAX_CASH_SHOP_NAME_BATCH)
		{
			batch = MAX_CASH_SHOP_NAME_BATCH;
		}

		if(batch > 0)
		{
			memcpy(nMsg.Row,&nameRows[nameSent],batch*sizeof(CASH_SHOP_NAME_ROW));
		}

		nameSent += batch;

		nMsg.Count = (BYTE)batch;
		nMsg.IsLast = ((nameSent >= nameCount)?1:0);

		gSocketManager.DataSend(index,(BYTE*)&nMsg,sizeof(nMsg));
	}
	while(nameSent < nameCount);

	LogAdd(LOG_BLUE,"[CashShop] names sent: %d packages with an override",nameCount);

	delete[] nameRows;

	/*
		Active banner, piggybacked on this same round trip - no separate request
		sub-code, same as names. Only one banner is ever active at a time, so
		this is a single-row message, not a batch. BannerId = -1 means "checked,
		none active" - sent explicitly so the client clears any stale banner
		rather than leaving an old one showing forever.
	*/
	SDHP_CASH_SHOP_BANNER_SEND bMsg;

	memset(&bMsg,0,sizeof(bMsg));

	bMsg.header.set(0x18,0x0E,sizeof(bMsg));

	bMsg.index = lpMsg->index;

	memcpy(bMsg.account,lpMsg->account,sizeof(bMsg.account));

	bMsg.BannerId = -1;

	if(gQueryManager.ExecQuery(
		/*
			TargetPackageId is a PackageId - the table's own identity - and the
			client has never heard of it. What it knows is MainIndex, which is the
			same number as the script's PackageProductSeq. Translated here rather
			than on the client, which has no way to do the lookup.

			LEFT JOIN: a banner pointing at a deleted package still shows, it just
			stops being clickable, which beats not showing at all.
		*/
		"SELECT TOP 1 b.BannerId,b.ImagePath,ISNULL(p.MainIndex,0) AS TargetPackageId "
		"FROM CustomCashShopBanners b "
		"LEFT JOIN CustomCashShopPackages p ON p.PackageId = b.TargetPackageId AND p.Enabled = 1 "
		"WHERE b.Enabled = 1 "
		"AND (b.StartDate IS NULL OR b.StartDate <= GETDATE()) "
		"AND (b.EndDate IS NULL OR b.EndDate >= GETDATE()) "
		"ORDER BY b.SortOrder DESC,b.BannerId DESC") == 0)
	{
		gQueryManager.Close();
		LogAdd(LOG_RED,"[CashShop] banner query failed - no banner sent this open");
	}
	else
	{
		if(gQueryManager.Fetch() != SQL_NO_DATA)
		{
			bMsg.BannerId = gQueryManager.GetAsInteger("BannerId");
			gQueryManager.GetAsString("ImagePath",bMsg.ImagePath,sizeof(bMsg.ImagePath));
			bMsg.TargetPackageId = gQueryManager.GetAsInteger("TargetPackageId");
		}

		gQueryManager.Close();
	}

	gSocketManager.DataSend(index,(BYTE*)&bMsg,sizeof(bMsg));

	LogAdd(LOG_BLUE,"[CashShop] banner sent: id=%d",bMsg.BannerId);

	#endif
}

void CCashShop::GDCashShopLogRecv(SDHP_CASH_SHOP_LOG_RECV* lpMsg,int index) // OK
{
	#if(DATASERVER_UPDATE>=501)

	char account[32];
	char character[32];
	char ip[48];

	EscapeSqlField(lpMsg->account,sizeof(lpMsg->account),account,sizeof(account));
	EscapeSqlField(lpMsg->character,sizeof(lpMsg->character),character,sizeof(character));
	EscapeSqlField(lpMsg->IpAddress,sizeof(lpMsg->IpAddress),ip,sizeof(ip));

	// PackageId is left NULL: the GameServer works in BaseIndex/MainIndex and has
	// no idea what identity the row was given. The admin tool joins on the pair.
	if(gQueryManager.ExecQuery(
		"INSERT INTO CustomCashShopLogs "
		"(AccountId,CharacterName,BaseIndex,MainIndex,ItemIndex,CoinIndex,"
		" ListPrice,PaidPrice,DiscountPercent,Result,IpAddress,ServerCode) "
		"VALUES ('%s','%s',%d,%d,%d,%d,%d,%d,%d,%d,'%s',%d)",
		account,character,
		lpMsg->BaseIndex,lpMsg->MainIndex,lpMsg->ItemIndex,lpMsg->CoinIndex,
		lpMsg->ListPrice,lpMsg->PaidPrice,(int)lpMsg->DiscountPercent,
		(int)lpMsg->Result,ip,(int)lpMsg->ServerCode) == 0)
	{
		// Never fail the purchase over the audit trail - the player has already
		// been charged and told. Losing a log row is bad; losing the sale is worse.
		LogAdd(LOG_RED,"[CashShop] purchase log insert failed for account (%s)",account);
	}

	gQueryManager.Close();

	#endif
}

void CCashShop::GDCashShopItemBuyRecv(SDHP_CASH_SHOP_ITEM_BUY_RECV* lpMsg,int index) // OK
{
	#if(DATASERVER_UPDATE>=501)

	SDHP_CASH_SHOP_ITEM_BUY_SEND pMsg;

	pMsg.header.set(0x18,0x01,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	pMsg.result = 0;

	pMsg.PackageMainIndex = lpMsg->PackageMainIndex;

	pMsg.Category = lpMsg->Category;

	pMsg.ProductMainIndex = lpMsg->ProductMainIndex;

	pMsg.ItemIndex = lpMsg->ItemIndex;

	pMsg.CoinIndex = lpMsg->CoinIndex;

	pMsg.MileageFlag = lpMsg->MileageFlag;

	/*
		Resolve the discounted price before anything else, while no other cursor
		is open - ExecQuery/Fetch/Close share one on this connection.

		Which view depends on the charge path, and getting this wrong is the whole
		point of having two (see CashShop.cpp:975 vs :1033 on the GameServer):

		    ProductMainIndex == 0  ->  package price   (bundle)
		    ProductMainIndex != 0  ->  product price   (duration variant)

		-1 on any failure. The GameServer then charges its own CoinValue exactly
		as it did before, so a missing table or an absent row degrades to the old
		behaviour rather than to a free item.
	*/
	pMsg.ListPrice = -1;
	pMsg.EffectivePrice = -1;
	pMsg.DiscountId = 0;
	pMsg.DiscountPercent = 0;

	if(lpMsg->ProductMainIndex == 0)
	{
		if(gQueryManager.ExecQuery(
			"SELECT e.ListPrice,e.EffectivePrice,e.DiscountPercent,ISNULL(e.DiscountId,0) AS DiscId "
			"FROM CustomCashShopEffectivePrice e "
			"JOIN CustomCashShopPackages p ON p.PackageId = e.PackageId "
			"WHERE p.MainIndex = %d AND p.Enabled = 1",lpMsg->PackageMainIndex) != 0
			&& gQueryManager.Fetch() != SQL_NO_DATA)
		{
			pMsg.ListPrice = gQueryManager.GetAsInteger("ListPrice");
			pMsg.EffectivePrice = gQueryManager.GetAsInteger("EffectivePrice");
			pMsg.DiscountPercent = (short)gQueryManager.GetAsInteger("DiscountPercent");
			pMsg.DiscountId = gQueryManager.GetAsInteger("DiscId");
		}
	}
	else
	{
		if(gQueryManager.ExecQuery(
			"SELECT v.ListPrice,v.EffectivePrice,v.DiscountPercent,ISNULL(v.DiscountId,0) AS DiscId "
			"FROM CustomCashShopEffectiveProductPrice v "
			"JOIN CustomCashShopPackages p ON p.PackageId = v.PackageId "
			"WHERE p.MainIndex = %d AND v.ProductMainIndex = %d AND p.Enabled = 1",
			lpMsg->PackageMainIndex,lpMsg->ProductMainIndex) != 0
			&& gQueryManager.Fetch() != SQL_NO_DATA)
		{
			pMsg.ListPrice = gQueryManager.GetAsInteger("ListPrice");
			pMsg.EffectivePrice = gQueryManager.GetAsInteger("EffectivePrice");
			pMsg.DiscountPercent = (short)gQueryManager.GetAsInteger("DiscountPercent");
			pMsg.DiscountId = gQueryManager.GetAsInteger("DiscId");
		}
	}

	gQueryManager.Close();

	if(gQueryManager.ExecQuery("SELECT * FROM CashShopData WHERE AccountID='%s'",lpMsg->account) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		pMsg.result = 1;
	}
	else
	{
		pMsg.WCoinC = gQueryManager.GetAsInteger("WCoinC");

		pMsg.WCoinP = gQueryManager.GetAsInteger("WCoinP");

		pMsg.GoblinPoint = gQueryManager.GetAsInteger("GoblinPoint");

		gQueryManager.Close();

		if(gQueryManager.ExecQuery("SELECT count(*) FROM CashShopInventory WHERE AccountID='%s' AND InventoryType=%d",lpMsg->account,83) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
		{
			gQueryManager.Close();

			pMsg.result = 1;
		}
		else
		{
			pMsg.ItemCount = gQueryManager.GetResult(0);

			gQueryManager.Close();
		}
	}

	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

void CCashShop::GDCashShopItemGifRecv(SDHP_CASH_SHOP_ITEM_GIF_RECV* lpMsg,int index) // OK
{
	#if(DATASERVER_UPDATE>=501)

	SDHP_CASH_SHOP_ITEM_GIF_SEND pMsg;

	pMsg.header.set(0x18,0x02,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	pMsg.result = 0;

	pMsg.PackageMainIndex = lpMsg->PackageMainIndex;

	pMsg.Category = lpMsg->Category;

	pMsg.ProductMainIndex = lpMsg->ProductMainIndex;

	pMsg.SaleZone = lpMsg->SaleZone;

	pMsg.ItemIndex = lpMsg->ItemIndex;

	pMsg.CoinIndex = lpMsg->CoinIndex;

	pMsg.MileageFlag = lpMsg->MileageFlag;

	memcpy(pMsg.GiftName,lpMsg->GiftName,sizeof(pMsg.GiftName));

	memcpy(pMsg.GiftText,lpMsg->GiftText,sizeof(pMsg.GiftText));

	gQueryManager.BindParameterAsString(1,lpMsg->GiftName,sizeof(lpMsg->GiftName));

	if(gQueryManager.ExecQuery("SELECT AccountID FROM Character WHERE Name=?") == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		pMsg.result = 1;
	}
	else
	{
		gQueryManager.GetAsString("AccountID",pMsg.GiftAccount,sizeof(pMsg.GiftAccount));

		gQueryManager.Close();

		if(gQueryManager.ExecQuery("SELECT * FROM CashShopData WHERE AccountID='%s'",lpMsg->account) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
		{
			gQueryManager.Close();

			pMsg.result = 1;
		}
		else
		{
			pMsg.WCoinC = gQueryManager.GetAsInteger("WCoinC");

			pMsg.WCoinP = gQueryManager.GetAsInteger("WCoinP");

			pMsg.GoblinPoint = gQueryManager.GetAsInteger("GoblinPoint");

			gQueryManager.Close();

			if(gQueryManager.ExecQuery("SELECT count(*) FROM CashShopInventory WHERE AccountID='%s' AND InventoryType=%d",pMsg.GiftAccount,71) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
			{
				gQueryManager.Close();

				pMsg.result = 1;
			}
			else
			{
				pMsg.ItemCount = gQueryManager.GetResult(0);

				gQueryManager.Close();
			}
		}
	}

	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

void CCashShop::GDCashShopItemNumRecv(SDHP_CASH_SHOP_ITEM_NUM_RECV* lpMsg,int index) // OK
{
	#if(DATASERVER_UPDATE>=501)

	SDHP_CASH_SHOP_ITEM_NUM_SEND pMsg;

	pMsg.header.set(0x18,0x03,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	pMsg.result = 0;

	pMsg.InventoryPage = lpMsg->InventoryPage;

	pMsg.InventoryType = lpMsg->InventoryType;

	pMsg.ItemCount = 0;

	pMsg.PageCount = 0;

	if(gQueryManager.ExecQuery("SELECT * FROM CashShopInventory WHERE AccountID='%s' AND InventoryType=%d",lpMsg->account,lpMsg->InventoryType) == 0)
	{
		gQueryManager.Close();

		pMsg.result = 1;
	}
	else
	{
		while(gQueryManager.Fetch() != SQL_NO_DATA)
		{
			if((((pMsg.ItemCount++)/MAX_CASH_SHOP_PAGE_ITEM)+1) == lpMsg->InventoryPage)
			{
				pMsg.ProductInfo[pMsg.PageCount].BaseItemCode = gQueryManager.GetAsInteger("BaseItemCode");

				pMsg.ProductInfo[pMsg.PageCount].MainItemCode = gQueryManager.GetAsInteger("MainItemCode");

				pMsg.ProductInfo[pMsg.PageCount].PackageMainIndex = gQueryManager.GetAsInteger("PackageMainIndex");

				pMsg.ProductInfo[pMsg.PageCount].ProductBaseIndex = gQueryManager.GetAsInteger("ProductBaseIndex");

				pMsg.ProductInfo[pMsg.PageCount].ProductMainIndex = gQueryManager.GetAsInteger("ProductMainIndex");

				pMsg.ProductInfo[pMsg.PageCount].CoinValue = gQueryManager.GetAsFloat("CoinValue");

				pMsg.ProductInfo[pMsg.PageCount].ProductType = gQueryManager.GetAsInteger("ProductType");

				gQueryManager.GetAsString("GiftName",pMsg.ProductInfo[pMsg.PageCount].GiftName,sizeof(pMsg.ProductInfo[pMsg.PageCount].GiftName));

				gQueryManager.GetAsString("GiftText",pMsg.ProductInfo[pMsg.PageCount].GiftText,sizeof(pMsg.ProductInfo[pMsg.PageCount].GiftText));

				pMsg.PageCount++;
			}
		}

		gQueryManager.Close();
	}

	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

void CCashShop::GDCashShopItemUseRecv(SDHP_CASH_SHOP_ITEM_USE_RECV* lpMsg,int index) // OK
{
	#if(DATASERVER_UPDATE>=501)

	SDHP_CASH_SHOP_ITEM_USE_SEND pMsg;

	pMsg.header.set(0x18,0x04,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	pMsg.result = 0;

	pMsg.BaseItemCode = lpMsg->BaseItemCode;

	pMsg.MainItemCode = lpMsg->MainItemCode;

	pMsg.ItemIndex = lpMsg->ItemIndex;

	pMsg.ProductType = lpMsg->ProductType;

	if(gQueryManager.ExecQuery("SELECT * FROM CashShopInventory WHERE BaseItemCode=%d",lpMsg->BaseItemCode) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		pMsg.result = 1;
	}
	else
	{
		pMsg.ProductInfo.BaseItemCode = gQueryManager.GetAsInteger("BaseItemCode");

		pMsg.ProductInfo.MainItemCode = gQueryManager.GetAsInteger("MainItemCode");

		pMsg.ProductInfo.PackageMainIndex = gQueryManager.GetAsInteger("PackageMainIndex");

		pMsg.ProductInfo.ProductBaseIndex = gQueryManager.GetAsInteger("ProductBaseIndex");

		pMsg.ProductInfo.ProductMainIndex = gQueryManager.GetAsInteger("ProductMainIndex");

		pMsg.ProductInfo.CoinValue = gQueryManager.GetAsFloat("CoinValue");

		pMsg.ProductInfo.ProductType = gQueryManager.GetAsInteger("ProductType");

		gQueryManager.GetAsString("GiftName",pMsg.ProductInfo.GiftName,sizeof(pMsg.ProductInfo.GiftName));

		gQueryManager.GetAsString("GiftText",pMsg.ProductInfo.GiftText,sizeof(pMsg.ProductInfo.GiftText));

		gQueryManager.Close();
	}

	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

void CCashShop::GDCashShopPeriodicItemRecv(SDHP_CASH_SHOP_PERIODIC_ITEM_RECV* lpMsg,int index) // OK
{
	#if(DATASERVER_UPDATE>=501)

	BYTE send[4096];

	SDHP_CASH_SHOP_PERIODIC_ITEM_SEND pMsg;

	pMsg.header.set(0x18,0x05,0);

	int size = sizeof(pMsg);

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	pMsg.count = 0;

	SDHP_CASH_SHOP_PERIODIC_ITEM2 info;

	for(int n=0;n < lpMsg->count;n++)
	{
		SDHP_CASH_SHOP_PERIODIC_ITEM1* lpInfo = (SDHP_CASH_SHOP_PERIODIC_ITEM1*)(((BYTE*)lpMsg)+sizeof(SDHP_CASH_SHOP_PERIODIC_ITEM_RECV)+(sizeof(SDHP_CASH_SHOP_PERIODIC_ITEM1)*n));

		if(gQueryManager.ExecQuery("SELECT Time FROM CashShopPeriodicItem WHERE ItemSerial=%d",lpInfo->serial) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
		{
			gQueryManager.Close();
			gQueryManager.ExecQuery("INSERT INTO CashShopPeriodicItem (ItemSerial,Time) VALUES (%d,%d)",lpInfo->serial,0);
			gQueryManager.Close();

			info.slot = lpInfo->slot;
			info.serial = lpInfo->serial;
			info.time = 0;
		}
		else
		{
			info.slot = lpInfo->slot;
			info.serial = lpInfo->serial;
			info.time = gQueryManager.GetAsInteger("Time");

			gQueryManager.Close();
		}

		memcpy(&send[size],&info,sizeof(info));
		size += sizeof(info);

		pMsg.count++;
	}

	pMsg.header.size[0] = SET_NUMBERHB(size);
	pMsg.header.size[1] = SET_NUMBERLB(size);

	memcpy(send,&pMsg,sizeof(pMsg));

	gSocketManager.DataSend(index,send,size);

	#endif
}

void CCashShop::GDCashShopRecievePointRecv(SDHP_CASH_SHOP_RECIEVE_POINT_RECV* lpMsg,int index) // OK
{
	#if(DATASERVER_UPDATE>=501)

	SDHP_CASH_SHOP_RECIEVE_POINT_SEND pMsg;

	pMsg.header.set(0x18,0x06,sizeof(pMsg));

	pMsg.index = lpMsg->index;

	memcpy(pMsg.account,lpMsg->account,sizeof(pMsg.account));

	pMsg.CallbackFunc = lpMsg->CallbackFunc;

	pMsg.CallbackArg1 = lpMsg->CallbackArg1;

	pMsg.CallbackArg2 = lpMsg->CallbackArg2;

	if(gQueryManager.ExecQuery("SELECT * FROM CashShopData WHERE AccountID='%s'",lpMsg->account) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		pMsg.WCoinC = 0;

		pMsg.WCoinP = 0;

		pMsg.GoblinPoint = 0;
	}
	else
	{
		pMsg.WCoinC = gQueryManager.GetAsInteger("WCoinC");

		pMsg.WCoinP = gQueryManager.GetAsInteger("WCoinP");

		pMsg.GoblinPoint = gQueryManager.GetAsInteger("GoblinPoint");
		
		gQueryManager.Close();
	}

	gSocketManager.DataSend(index,(BYTE*)&pMsg,sizeof(pMsg));

	#endif
}

void CCashShop::GDCashShopAddPointSaveRecv(SDHP_CASH_SHOP_ADD_POINT_SAVE_RECV* lpMsg) // OK
{
	#if(DATASERVER_UPDATE>=501)

	char TargetAccount[11];

	memcpy(TargetAccount,((lpMsg->GiftAccount[0]==0) ? lpMsg->account : lpMsg->GiftAccount),sizeof(TargetAccount));

	if(gQueryManager.ExecQuery("SELECT WCoinC,WCoinP,GoblinPoint,Ruud FROM CashShopData WHERE AccountID='%s'",TargetAccount) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		gQueryManager.ExecQuery("INSERT INTO CashShopData (AccountID,WCoinC,WCoinP,GoblinPoint,Ruud) VALUES ('%s',%d,%d,%d,%d)",TargetAccount,lpMsg->AddWCoinC,lpMsg->AddWCoinP,lpMsg->AddGoblinPoint,lpMsg->AddRuud);

		gQueryManager.Close();
	}
	else
	{
		DWORD WCoinC = gQueryManager.GetAsInteger("WCoinC");

		DWORD WCoinP = gQueryManager.GetAsInteger("WCoinP");

		DWORD GoblinPoint = gQueryManager.GetAsInteger("GoblinPoint");

		DWORD Ruud = gQueryManager.GetAsInteger("Ruud");

		gQueryManager.Close();

		gQueryManager.ExecQuery("UPDATE CashShopData SET WCoinC=%d,WCoinP=%d,GoblinPoint=%d,Ruud=%d WHERE AccountID='%s'",(((WCoinC+lpMsg->AddWCoinC)>0x7FFFFFFF)?0x7FFFFFFF:(WCoinC+lpMsg->AddWCoinC)),(((WCoinP+lpMsg->AddWCoinP)>0x7FFFFFFF)?0x7FFFFFFF:(WCoinP+lpMsg->AddWCoinP)),(((GoblinPoint+lpMsg->AddGoblinPoint)>0x7FFFFFFF)?0x7FFFFFFF:(GoblinPoint+lpMsg->AddGoblinPoint)), (((Ruud + lpMsg->AddRuud) > 0x7FFFFFFF) ? 0x7FFFFFFF : ( Ruud + lpMsg->AddRuud )), TargetAccount);

		gQueryManager.Close();
	}

	#endif
}

void CCashShop::GDCashShopSubPointSaveRecv(SDHP_CASH_SHOP_SUB_POINT_SAVE_RECV* lpMsg) // OK
{
	#if(DATASERVER_UPDATE>=501)

	char TargetAccount[11];

	memcpy(TargetAccount,((lpMsg->GiftAccount[0]==0)?lpMsg->account:lpMsg->GiftAccount),sizeof(TargetAccount));

	if(gQueryManager.ExecQuery("SELECT WCoinC, WCoinP, GoblinPoint, Ruud FROM CashShopData WHERE AccountID='%s'",TargetAccount) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
	{
		gQueryManager.Close();

		gQueryManager.ExecQuery("INSERT INTO CashShopData (AccountID,WCoinC,WCoinP,GoblinPoint, Ruud) VALUES ('%s',%d,%d,%d,%d)",TargetAccount,0,0,0,0);

		gQueryManager.Close();
	}
	else
	{
		DWORD WCoinC = gQueryManager.GetAsInteger("WCoinC");

		DWORD WCoinP = gQueryManager.GetAsInteger("WCoinP");

		DWORD GoblinPoint = gQueryManager.GetAsInteger("GoblinPoint");

		DWORD RuudPoint = gQueryManager.GetAsInteger("Ruud");

		gQueryManager.Close();

		gQueryManager.ExecQuery("UPDATE CashShopData SET WCoinC=%d, WCoinP=%d, GoblinPoint=%d, Ruud=%d WHERE AccountID='%s'", ((lpMsg->SubWCoinC>WCoinC)?0:(WCoinC-lpMsg->SubWCoinC)), ((lpMsg->SubWCoinP>WCoinP)?0:(WCoinP-lpMsg->SubWCoinP)), ((lpMsg->SubGoblinPoint>GoblinPoint)?0:(GoblinPoint-lpMsg->SubGoblinPoint)), ((lpMsg->SubRuud>RuudPoint)?0:(RuudPoint-lpMsg->SubRuud)), TargetAccount);

		gQueryManager.Close();

		if (lpMsg->SubWCoinC > 0 )
		{
			gQueryManager.ExecQuery("INSERT INTO LOG_CREDITOS (login,valor,tipo) VALUES ('%s',%d,%d)",TargetAccount,((lpMsg->SubWCoinC>WCoinC)?0:(-lpMsg->SubWCoinC)),4);

			gQueryManager.Close();
		}
	}

	#endif
}

void CCashShop::GDCashShopInsertItemSaveRecv(SDHP_CASH_SHOP_INSERT_ITEM_SAVE_RECV* lpMsg) // OK
{
	#if(DATASERVER_UPDATE>=501)

	char TargetAccount[11];

	memcpy(TargetAccount,((lpMsg->GiftAccount[0]==0)?lpMsg->account:lpMsg->GiftAccount),sizeof(TargetAccount));

	gQueryManager.BindParameterAsString(1,lpMsg->GiftName,sizeof(lpMsg->GiftName));

	gQueryManager.BindParameterAsString(2,lpMsg->GiftText,sizeof(lpMsg->GiftText));

	gQueryManager.ExecQuery("INSERT INTO CashShopInventory (MainItemCode,AccountID,InventoryType,PackageMainIndex,ProductBaseIndex,ProductMainIndex,CoinValue,ProductType,GiftName,GiftText) VALUES (%d,'%s',%d,%d,%d,%d,%f,%d,?,?)",0,TargetAccount,lpMsg->InventoryType,lpMsg->PackageMainIndex,lpMsg->ProductBaseIndex,lpMsg->ProductMainIndex,lpMsg->CoinValue,lpMsg->ProductType);

	gQueryManager.Close();

	#endif
}

void CCashShop::GDCashShopDeleteItemSaveRecv(SDHP_CASH_SHOP_DELETE_ITEM_SAVE_RECV* lpMsg) // OK
{
	#if(DATASERVER_UPDATE>=501)

	char TargetAccount[11];

	memcpy(TargetAccount,((lpMsg->GiftAccount[0]==0)?lpMsg->account:lpMsg->GiftAccount),sizeof(TargetAccount));

	gQueryManager.ExecQuery("DELETE FROM CashShopInventory WHERE BaseItemCode=%d",lpMsg->BaseItemCode);
	gQueryManager.Close();

	#endif
}

void CCashShop::GDCashShopPeriodicItemSaveRecv(SDHP_CASH_SHOP_PERIODIC_ITEM_SAVE_RECV* lpMsg) // OK
{
	#if(DATASERVER_UPDATE>=501)

	for(int n=0;n < lpMsg->count;n++)
	{
		SDHP_CASH_SHOP_PERIODIC_ITEM_SAVE* lpInfo = (SDHP_CASH_SHOP_PERIODIC_ITEM_SAVE*)(((BYTE*)lpMsg)+sizeof(SDHP_CASH_SHOP_PERIODIC_ITEM_SAVE_RECV)+(sizeof(SDHP_CASH_SHOP_PERIODIC_ITEM_SAVE)*n));

		if(gQueryManager.ExecQuery("SELECT ItemSerial FROM CashShopPeriodicItem WHERE ItemSerial=%d",lpInfo->serial) == 0 || gQueryManager.Fetch() == SQL_NO_DATA)
		{
			gQueryManager.Close();
			gQueryManager.ExecQuery("INSERT INTO CashShopPeriodicItem (ItemSerial,Time) VALUES (%d,%d)",lpInfo->serial,lpInfo->time);
			gQueryManager.Close();
		}
		else
		{
			gQueryManager.Close();
			gQueryManager.ExecQuery("UPDATE CashShopPeriodicItem SET Time=%d WHERE ItemSerial=%d",lpInfo->time,lpInfo->serial);
			gQueryManager.Close();
		}
	}

	#endif
}
