// InGameShopSystem.h: interface for the InGameShopSystem class.
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INGAMESHOPSYSTEM_H__2DF68839_DA28_44BC_B662_213BB22839CB__INCLUDED_)
#define AFX_INGAMESHOPSYSTEM_H__2DF68839_DA28_44BC_B662_213BB22839CB__INCLUDED_

#pragma once

#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM

#include "./GameShop/ShopListManager/ShopListManager.h"				
#include "./GameShop/ShopListManager/BannerListManager.h"


#define INGAMESHOP_ERROR_ZERO_SIZE		(-1)
#define INGAMESHOP_ERROR_INVALID_INDEX	(-2)

typedef std::list<CShopPackage>			type_listPackage;
typedef std::map<int, int>				type_mapZoneSeq;
typedef std::list<unicode::t_string>	type_listName;

class CInGameShopSystem
{
public:
	enum IGS_PACKAGE_GOODS_TYPE
	{
		IGS_GOODS_TYPE_FIXEDAMOUNT		= 135,
		IGS_GOODS_TYPE_FIXEDQUANTITY	= 136,
		IGS_GOODS_TYPE_PREMIUM			= 137,
		IGS_GOODS_TYPE_CONSUMPTION		= 138,
		IGS_GOODS_TYPE_ETERNITY			= 139,
		IGS_GOODS_TYPE_PERIOD			= 140,
		IGS_GOODS_TYPE_PREMIUM_ITEM		= 406,
		IGS_GOODS_TYPE_GOBLIN_POINT		= 515,
	};

	enum IGS_PACKAGE_ATTRIBUTE_TYPE
	{
		IGS_PACKAGE_ATT_TYPE_NONE	= 0,
		IGS_PACKAGE_ATT_TYPE_NAME,
		IGS_PACKAGE_ATT_TYPE_DESCRIPTION,
		IGS_PACKAGE_ATT_TYPE_PRICE,
		IGS_PACKAGE_ATT_TYPE_ITEMCODE,
	};

	enum IGS_PRODUCT_ATTRIBUTE_TYPE
	{
		IGS_PRODUCT_ATT_TYPE_NONE	= 0,
		IGS_PRODUCT_ATT_TYPE_USE_LIMIT_PERIOD,
		IGS_PRODUCT_ATT_TYPE_AVALIABLE_PERIOD,
		IGS_PRODUCT_ATT_TYPE_NUM,
		IGS_PRODUCT_ATT_TYPE_PRICE,
		IGS_PRODUCT_ATT_TYPE_ITEMCODE,
		IGS_PRODUCT_ATT_TYPE_ITEMNAME,
		IGS_PRODUCT_ATT_TYPE_PRICE_SEQUENCE,
	};

	enum IGS_ETC
	{
		IGS_LIMIT_REQUEST_EVENT_PACKAGE	= 20,
	};

protected:
	CInGameShopSystem();
public:
	~CInGameShopSystem();

public:
	static CInGameShopSystem* GetInstance();

 	void Initalize();
// 	bool Update();
// 	bool Render();
 	void Release();

	void SetScriptVersion(int iSalesZone, int iYear, int iYearId);
	void SetBannerVersion(int iSalesZone, int iYear, int iYearId);
	bool ScriptDownload();
	bool BannerDownload();

public:
	bool SelectZone(int iIndex);
	bool SelectCategory(int iIndex);

	/*
		Which category button shows this package, or -1.

		The whole walk lives here rather than in the UI because both halves of
		it - the package list and the category-index mapping - are internals,
		and a banner jump is not a reason to open them up.
	*/
	int GetCategoryButtonIndexOfPackage(int iPackageSeq);

	void BeginPage();
	void NextPage();
	void PrePage();
	int GetTotalPages();
	int GetSelectPage();

	int GetSizeZones();
	int GetSizeCategoriesAsSelectedZone();
	int GetSizePackageAsSelectedCategory();
	int GetSizePackageAsDisplayPackage();

	WORD GetPackageItemCode(int iIndex);

	type_listName GetZoneName();
	type_listName GetCategoryName();

	void SetTotalCash(double dTotalCash);
	void SetTotalPoint(double dTotalPoint);
	void SetTotalMileage(double dTotalMileage);
	void SetCashCreditCard(double dCashCreditCard);		// Global Credit Cash
	void SetCashPrepaid(double dCashPrepaid);			// Global Prepaid Cash
	double GetTotalCash();
	double GetTotalPoint();
	double GetTotalMileage();
	double GetCashCreditCard();							// Global Credit Cash
	double GetCashPrepaid();							// Global Prepaid Cash
	
	CShopPackage* GetDisplayPackage(int iIndex);		

	void SetIsRequestShopOpenning(bool bIsRequestShopOpenning);
	bool GetIsRequestShopOpenning();

	bool GetPackageInfo(int iPackageSeq, int iPackageAttrType, OUT int& iValue, OUT unicode::t_char* pszText);

	bool GetProductInfoFromPriceSeq(int iProductSeq, int iPriceSeq, int iAttrType,OUT int& iValue, OUT unicode::t_char* pszUnitName);
	bool GetProductInfoFromProductSeq(int iProductSeq, int iAttrType, OUT int& iValue, OUT unicode::t_char* pszUnitName);

	void SetNormalPackage();
	void InitEventPackage(int iTotalEventPackage);
	void InsertEventPackage(int* pPackageSeq);


	bool IsShopOpen();
	bool IsRequestEventPackge();
	void SetRequestEventPackge();

	bool IsBanner();
	unicode::t_char* GetBannerFileName();
	unicode::t_char* GetBannerURL();

	CListVersionInfo GetScriptVer();
	CListVersionInfo GetBannerVer();
#ifdef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
	CListVersionInfo GetCurrentScriptVer();
	CListVersionInfo GetCurrentBannerVer();
	bool IsScriptDownload();
	bool IsBannerDownload();

	void ShopOpenLock();
	void ShopOpenUnLock();
#endif // KJH_MOD_SHOP_SCRIPT_DOWNLOAD

protected:
	void InitZoneInfo();
	void InitPackagePerPage(int iPageIndex);

	int GetZoneSeqIndexByIndex(int iIndex);
	int GetCategorySeqIndexByIndex(int iIndex);

	void SetCategoryName();

#ifndef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
	void ShopOpenLock();
	void ShopOpenUnLock();
#endif // KJH_MOD_SHOP_SCRIPT_DOWNLOAD

	bool GetProductInfo(CShopProduct* pProduct, int iAttrType, OUT int& iValue, OUT unicode::t_char* pszUnitName);

public:
	/*
		Server-side shelf prices, arriving on (0xD2)(0x20) after the shop opens.

		The catalog the client draws comes from IBSPackage.txt, which knows only a
		list price and nothing about a sale. These rows carry what the GameServer
		will actually charge, keyed by PackageProductSeq - the same number the buy
		request sends back - so the shop can show the real price instead of the
		one the script file happens to hold.

		Display only. The buy request has never carried a price and still does not;
		DGCashShopItemBuyRecv works out the charge from the DataServer's own view,
		so a client that misdraws its shop is charged correctly regardless.

		Empty until the first batch lands, and emptied when the shop opens: an
		absent entry means "no server price known", and the caller falls back to
		the script price rather than showing a zero.
	*/
	struct IGS_SERVER_PRICE
	{
		int		iListPrice;
		int		iEffectivePrice;
		int		iDiscountPercent;
	};

	enum IGS_SERVER_PRICE_KIND
	{
		IGS_PRICE_KIND_PACKAGE	= 0,	// keyed by CShopPackage::PackageProductSeq
		IGS_PRICE_KIND_PRODUCT	= 1,	// keyed by CShopProduct::PriceSeq (the variant)
	};

	void	ClearServerPrices();
	void	SetServerPrice(int iKind, int iSeq, int iListPrice, int iEffectivePrice, int iDiscountPercent);
	bool	GetServerPrice(int iKind, int iSeq, OUT IGS_SERVER_PRICE& Price);

	/*
		Admin-edited package display-name overrides, same side-map pattern as
		IGS_SERVER_PRICE above and for the same reason: CShopPackage is copied by
		value out of the master list on every category change (SetNormalPackage/
		InsertEventPackage/UpdateDisplayPackage), so mutating PackageProductName
		in place would miss every copy already taken. Packages only - there is no
		CustomCashShopProducts.DisplayName column to override.

		Empty until the first batch lands, and emptied when the shop opens - same
		lifecycle as the price map.
	*/
	void	ClearServerPackageNames();
	void	SetServerPackageName(int iSeq, const char* pszName);
	bool	GetServerPackageName(int iSeq, OUT char* pszOutName, int cbOutName);

	/*
		Which admin-uploaded banner (CustomCashShopBanners.BannerId) is already
		on disk and applied via InitBanner(). NOT cleared on shop-open like the
		price/name maps above - the banner rarely changes, so re-downloading it
		every time the shop opens would be a network hit for nothing. Default
		-2 so the very first server answer (including -1, "none active")
		always differs and is applied at least once.
	*/
	int		GetLastAppliedBannerId() { return m_iLastAppliedBannerId; }
	void	SetLastAppliedBannerId(int iBannerId) { m_iLastAppliedBannerId = iBannerId; }

	/*
		Custom banner fetch, off the main thread.

		URLDownloadToFileA is synchronous, and it was being called straight from
		the 0xD2:0x22 packet handler - so the first shop open froze the client
		for a DNS lookup, a TLS handshake and a jpg, and for the full timeout if
		the host was unreachable. Subsequent opens were fine only because the
		banner id had not changed, which is why it looked like a first-open bug.

		StartCustomBannerDownload returns true when the banner is already on
		disk and can be applied immediately (banner ids are unique per upload,
		so a cached file is never stale). Otherwise it spawns the fetch and the
		shop's Update polls PollCustomBannerDownload - the bitmap load has to
		happen on the render thread, so the worker only writes the file.
	*/
	bool	StartCustomBannerDownload(int iBannerId, const char* pszUrl, char* szLocalPathOut, int iOutSize);

	/*
		The display catalog, pushed from the database instead of read from
		IBSCategory.txt / IBSPackage.txt / IBSProduct.txt.

		Rows arrive as script lines, fragmented, and are BUFFERED rather than
		applied as they land. Two reasons, both about failure:

		- Applying rows directly means clearing the lists first, so a stream that
		  dies half way through would leave the player with part of a shop.
		  Buffering means a broken push changes nothing at all.
		- The category-to-package link is made inside the package parser, and
		  CShopCategoryList::InsertPackage returns 0 in silence when the category
		  is not there yet. Replaying in a known order removes any dependence on
		  the order rows happened to arrive in.

		GetCatalogVersion is what the request carries, so an unchanged catalog
		costs one packet. It stays 0 until a catalog has been applied, and no
		real version can be 0.
	*/
	void	OnCatalogFragment(int iRowKind, int iFlags, int iVersion, const char* pszFragment);
	int		GetCatalogVersion() { return m_iCatalogVersion; }
	bool	HasServerCatalog() { return m_bServerCatalogApplied; }
	void	ResetCatalogStream();
	bool	PollCustomBannerDownload(char* szLocalPathOut, int iOutSize, int& iBannerIdOut);


protected:
	CShopListManager		m_ShopManager;
	CBannerListManager		m_BannerManager;

	CListVersionInfo		m_ScriptVerInfo;
	CListVersionInfo		m_BannerVerInfo;

#ifdef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
	CListVersionInfo		m_CurrentScriptVerInfo;
	CListVersionInfo		m_CurrentBannerVerInfo;
#endif // KJH_MOD_SHOP_SCRIPT_DOWNLOAD
	
	char					m_szScriptIPAddress[20];
	char					m_szBannerIPAddress[20];
	char					m_szScriptRemotePath[MAX_TEXT_LENGTH];
	char					m_szScriptLocalPath[MAX_TEXT_LENGTH];
	char					m_szBannerRemotePath[MAX_TEXT_LENGTH];
	char					m_szBannerLocalPath[MAX_TEXT_LENGTH];

	CShopCategory			m_SelectedZone;
 	CShopCategory			m_SelectedCategory;
	CBannerInfo				m_BannerInfo;

 	int						m_iSelectedPage;

	type_mapZoneSeq			m_mapZoneSeqIndex;
	type_listPackage		m_listDisplayPackage;
	type_listPackage		m_listNormalPackage;
	type_listPackage		m_listEventPackage;
	type_listPackage*		m_plistSelectPackage;
	type_listName			m_listZoneName;
	type_listName			m_listCategoryName;
	
	CShopCategoryList		*m_pCategoryList;
	CShopPackageList		*m_pPackageList;
	CShopProductList		*m_pProductList;

	CBannerInfoList			*m_pBannerList;
	
	double					m_dTotalCash;
	double					m_dTotalPoint;
	double					m_dTotalMileage;
	double					m_dCashCreditCard;
	double					m_dCashPrepaid;

	bool					m_bIsRequestEventPackage;

	bool					m_bIsRequestShopOpenning;

	bool					m_bSelectEventCategory;
	bool					m_bAbleRequestEventPackage;
	int						m_iCntSelectEventZone;

	int						m_iEventPackageCnt;
	int						m_iTotalEventPackage;
	int						m_iCurrentEventPackage;

	bool					m_bIsShopOpenLock;	
	bool					m_bIsBanner;
	bool					m_bFirstScriptDownloaded;
	bool					m_bFirstBannerDownloaded;

	std::map<int, IGS_SERVER_PRICE>	m_mapServerPackagePrice;
	std::map<int, IGS_SERVER_PRICE>	m_mapServerProductPrice;

	std::map<int, std::string>			m_mapServerPackageName;

	int						m_iLastAppliedBannerId;

	// 0 idle, 1 downloading, 2 finished ok, 3 finished failed. Written by the
	// worker with InterlockedExchange, read by the main thread.
	volatile LONG			m_lBannerFetchState;
	int						m_iBannerFetchId;
	char					m_szBannerFetchUrl[512];
	char					m_szBannerFetchPath[MAX_PATH];

	// Applied catalog version, 0 until one has been. Sent with every request.
	int						m_iCatalogVersion;
	bool					m_bServerCatalogApplied;

	// In-flight stream. m_strCatalogLine reassembles one row across fragments;
	// m_vCatalogRows holds whole rows until the end marker says it is safe.
	int						m_iCatalogStreamVersion;
	std::string				m_strCatalogLine;
	std::vector< std::pair<int,std::string> >	m_vCatalogRows;

	static unsigned __stdcall BannerFetchThread(void* pParam);
};

#define g_InGameShopSystem CInGameShopSystem::GetInstance()

#endif // KJH_ADD_INGAMESHOP_UI_SYSTEM
#endif // !defined(AFX_INGAMESHOPSYSTEM_H__2DF68839_DA28_44BC_B662_213BB22839CB__INCLUDED_)
