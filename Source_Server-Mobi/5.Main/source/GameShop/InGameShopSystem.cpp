// InGameShopSystem.cpp: implementation of the InGameShopSystem class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM
#include "InGameShopSystem.h"
#include "WSclientinline.h"
#include "ZzzInventory.h"
#include "MsgBoxIGSCommon.h"

// The banner fetch worker needs urlmon for the download, Path for the cache
// directory, and process.h for _beginthreadex - all Windows-only, like the
// fetch itself.
#ifndef __ANDROID__
#include "ShopListManager\interface\PathMethod\Path.h"
#include <UrlMon.h>
#include <process.h>
#pragma comment(lib,"Urlmon.lib")
#else
// Android does the fetch in Java instead - see MU_MobileStartBannerDownload.
#include "Platform/MobilePlatform.h"
#endif // __ANDROID__
 
#ifdef CONSOLE_DEBUG
	#include "./Utilities/Log/muConsoleDebug.h"
#endif // CONSOLE_DEBUG

CInGameShopSystem::CInGameShopSystem()
{
	m_pCategoryList = NULL;
	m_pPackageList = NULL;
	m_pProductList = NULL;
	m_pBannerList = NULL;

	memset(&m_ScriptVerInfo, -1, sizeof(CListVersionInfo));
	memset(&m_BannerVerInfo, -1, sizeof(CListVersionInfo));
	memset(&m_CurrentScriptVerInfo, -1, sizeof(CListVersionInfo));
	memset(&m_CurrentBannerVerInfo, -1, sizeof(CListVersionInfo));

	m_bIsShopOpenLock = true; //louis
	m_bIsBanner	= false;
	m_bIsRequestEventPackage = false;
	m_plistSelectPackage = NULL;
	m_bFirstScriptDownloaded = false;
	m_bFirstBannerDownloaded = false;

	m_iLastAppliedBannerId = -2;
	m_lBannerFetchState = 0;
	m_iBannerFetchId = -1;
	m_szBannerFetchUrl[0] = '\0';
	m_szBannerFetchPath[0] = '\0';
	m_iCatalogVersion = 0;
	m_bServerCatalogApplied = false;
	m_iCatalogStreamVersion = 0;
	m_strCatalogLine.clear();
	m_vCatalogRows.clear();
}

CInGameShopSystem::~CInGameShopSystem()
{
	Release();
}

CInGameShopSystem* CInGameShopSystem::GetInstance()
{
	static CInGameShopSystem s_InGameShopSystem;
	return &s_InGameShopSystem;
}

void CInGameShopSystem::Initalize()
{
	m_mapZoneSeqIndex.clear();
	m_listDisplayPackage.clear();
	m_listNormalPackage.clear();
	m_listEventPackage.clear();
	m_listZoneName.clear();
	m_listCategoryName.clear();
	m_plistSelectPackage = &m_listNormalPackage;
	m_dTotalCash	= 0;
	m_dTotalPoint	= 0;
	m_dTotalMileage	= 0;
	m_dCashCreditCard	= 0; 
	m_dCashPrepaid		= 0;	
	m_iEventPackageCnt	= 0;
	m_iSelectedPage = 1;
	m_iTotalEventPackage = 0;
	m_iCntSelectEventZone = 0;
	m_bSelectEventCategory = false;
	m_bIsRequestShopOpenning = false;
	m_bAbleRequestEventPackage = true;
	InitZoneInfo();
}

void CInGameShopSystem::Release()
{
	m_mapZoneSeqIndex.clear();
	m_listDisplayPackage.clear();
	m_listNormalPackage.clear();
	m_listEventPackage.clear();
	m_listZoneName.clear();
	m_listCategoryName.clear();
	m_pCategoryList = NULL;
	m_pPackageList = NULL;
	m_pProductList = NULL;
}

void CInGameShopSystem::SetScriptVersion(int iSalesZone, int iYear, int iYearId)
{
	m_ScriptVerInfo.Zone = iSalesZone;
	m_ScriptVerInfo.year = iYear;
	m_ScriptVerInfo.yearId = iYearId;
}

void CInGameShopSystem::SetBannerVersion(int iSalesZone, int iYear, int iYearId)
{
	m_BannerVerInfo.Zone = iSalesZone;
	m_BannerVerInfo.year = iYear;
	m_BannerVerInfo.yearId = iYearId;
}

#ifndef __ANDROID__
/*
	Fetch one banner jpg. Runs on its own thread and touches nothing but the file.

	urlmon wants COM on whatever thread calls it, and a thread this function
	created is not the one the client initialised.
*/
unsigned __stdcall CInGameShopSystem::BannerFetchThread(void* pParam)
{
	CInGameShopSystem* pThis = (CInGameShopSystem*)pParam;

	::CoInitialize(NULL);

	HRESULT hr = ::URLDownloadToFileA(0, pThis->m_szBannerFetchUrl, pThis->m_szBannerFetchPath, 0, 0);

	::CoUninitialize();

	::InterlockedExchange(&pThis->m_lBannerFetchState, (hr == S_OK) ? 2 : 3);

	return 0;
}

bool CInGameShopSystem::StartCustomBannerDownload(int iBannerId, const char* pszUrl, char* szLocalPathOut, int iOutSize)
{
	char szLocalPath[MAX_PATH] = {0};

	// Data\InGameShopBanner\Custom\<BannerId>.jpg - a separate subfolder from
	// the legacy Zone.Year.YearId cache so the two systems never collide.
	::GetCurrentDirectoryA(MAX_PATH, szLocalPath);
	sprintf(szLocalPath, "%s\\data\\InGameShopBanner\\Custom\\%d.jpg", szLocalPath, iBannerId);

	Path::CreateDirectorys(szLocalPath, 1);

	// Already cached. Banner ids are unique per upload, so this can never be a
	// stale copy of a different image - it is the same file or it does not exist.
	if( ::GetFileAttributesA(szLocalPath) != INVALID_FILE_ATTRIBUTES )
	{
		strncpy(szLocalPathOut, szLocalPath, iOutSize-1);
		szLocalPathOut[iOutSize-1] = '\0';
		return true;
	}

	// One fetch at a time. A second shop open while the first is still running
	// would otherwise overwrite the url the worker is reading.
	if( m_lBannerFetchState == 1 )
		return false;

	m_iBannerFetchId = iBannerId;
	strncpy(m_szBannerFetchUrl, pszUrl, sizeof(m_szBannerFetchUrl)-1);
	m_szBannerFetchUrl[sizeof(m_szBannerFetchUrl)-1] = '\0';
	strncpy(m_szBannerFetchPath, szLocalPath, sizeof(m_szBannerFetchPath)-1);
	m_szBannerFetchPath[sizeof(m_szBannerFetchPath)-1] = '\0';

	::InterlockedExchange(&m_lBannerFetchState, 1);

	unsigned int uiThreadId = 0;
	HANDLE hThread = (HANDLE)_beginthreadex(0, 0, CInGameShopSystem::BannerFetchThread, this, 0, &uiThreadId);

	if( hThread == 0 )
	{
		::InterlockedExchange(&m_lBannerFetchState, 0);
		return false;
	}

	::CloseHandle(hThread);
	return false;
}

bool CInGameShopSystem::PollCustomBannerDownload(char* szLocalPathOut, int iOutSize, int& iBannerIdOut)
{
	LONG lState = m_lBannerFetchState;

	if( lState != 2 && lState != 3 )
		return false;

	::InterlockedExchange(&m_lBannerFetchState, 0);

	if( lState == 3 )
		return false;

	strncpy(szLocalPathOut, m_szBannerFetchPath, iOutSize-1);
	szLocalPathOut[iOutSize-1] = '\0';
	iBannerIdOut = m_iBannerFetchId;

	return true;
}
#else // __ANDROID__

/*
	Android has neither urlmon nor the bundled curl (that is a Windows .lib),
	so the fetch itself lives in Java and this side only remembers where the
	file is meant to land. The state machine is deliberately the same shape as
	the Windows one above, so NewUIInGameShop::Update polls identically on both.
*/
bool CInGameShopSystem::StartCustomBannerDownload(int iBannerId, const char* pszUrl, char* szLocalPathOut, int iOutSize)
{
	char szDir[MAX_PATH] = {0};
	char szLocalPath[MAX_PATH] = {0};

	/*
		Absolute, not relative - and this is the whole reason the Android path
		is not two lines shorter.

		The process chdir's into its private writable directory at startup, so
		a relative path resolves correctly on this side. But the download runs
		in Java, and the JVM resolves new File("data/...") against user.dir,
		which on Android is "/" and is not writable. The two would disagree
		about where the jpg lives, the fetch would fail, and the cache check
		here would keep saying "not downloaded yet" forever.

		getcwd() is exactly what GetCurrentDirectoryA does on the PC side of
		this same function, so both platforms hand out an absolute path and
		LoadBitmap gets the same kind of string it already gets on PC.
	*/
	if( getcwd(szDir, sizeof(szDir)) == NULL )
		return false;

	/*
		Capital "Data", unlike the PC string just above.

		Windows does not care, so on PC this cache lands in the real Data\
		folder whatever case the literal used. Here it decides whether the jpg
		joins the deployed asset tree or creates a second, stray lowercase
		directory beside it - and worse, AndroidFopen retries reads
		case-insensitively while the GetFileAttributesA check below does a
		plain stat, so the two can resolve to different directories and the
		shop would keep re-fetching a banner it already has.
	*/
	sprintf(szLocalPath, "%s/Data", szDir);
	CreateDirectoryA(szLocalPath, NULL);
	sprintf(szLocalPath, "%s/Data/InGameShopBanner", szDir);
	CreateDirectoryA(szLocalPath, NULL);
	sprintf(szLocalPath, "%s/Data/InGameShopBanner/Custom", szDir);
	CreateDirectoryA(szLocalPath, NULL);

	sprintf(szLocalPath, "%s/Data/InGameShopBanner/Custom/%d.jpg", szDir, iBannerId);

	// Already cached. Banner ids are unique per upload, so this can never be a
	// stale copy of a different image - it is the same file or it does not exist.
	if( ::GetFileAttributesA(szLocalPath) != INVALID_FILE_ATTRIBUTES )
	{
		strncpy(szLocalPathOut, szLocalPath, iOutSize-1);
		szLocalPathOut[iOutSize-1] = '\0';
		return true;
	}

	// One fetch at a time, same as PC - a second shop open while the first is
	// still running would otherwise lose track of which id is arriving.
	if( m_lBannerFetchState == 1 )
		return false;

	m_iBannerFetchId = iBannerId;
	strncpy(m_szBannerFetchPath, szLocalPath, sizeof(m_szBannerFetchPath)-1);
	m_szBannerFetchPath[sizeof(m_szBannerFetchPath)-1] = '\0';

	if( !MU_MobileStartBannerDownload(pszUrl, szLocalPath) )
		return false;

	m_lBannerFetchState = 1;
	return false;
}

bool CInGameShopSystem::PollCustomBannerDownload(char* szLocalPathOut, int iOutSize, int& iBannerIdOut)
{
	if( m_lBannerFetchState != 1 )
		return false;

	// 0 still running, 1 done, -1 failed. The Java side clears itself once it
	// has reported, so this must only be asked while a fetch is outstanding.
	const int iResult = MU_MobilePollBannerDownload();

	if( iResult == 0 )
		return false;

	m_lBannerFetchState = 0;

	if( iResult < 0 )
		return false;

	strncpy(szLocalPathOut, m_szBannerFetchPath, iOutSize-1);
	szLocalPathOut[iOutSize-1] = '\0';
	iBannerIdOut = m_iBannerFetchId;

	return true;
}

#endif // __ANDROID__

/*
	Drop whatever half-arrived catalog was in flight.

	Called when a stream is superseded or abandoned. The applied catalog and its
	version are deliberately left alone: a failed push must leave the shop
	exactly as it was, not empty.
*/
void CInGameShopSystem::ResetCatalogStream()
{
	m_strCatalogLine.clear();
	m_vCatalogRows.clear();
	m_iCatalogStreamVersion = 0;
}

/*
	One fragment of one catalog row.

	Fragments accumulate into a line; whole lines accumulate into a buffer; the
	buffer is only committed when the end marker arrives. See the header for why
	nothing is applied as it lands.

	The replay order is categories, then products, then packages - packages last
	because CShopList::AddServerCatalogLine attaches each one to its category as
	it parses, and a package whose category does not exist yet is dropped without
	a word.
*/
void CInGameShopSystem::OnCatalogFragment(int iRowKind, int iFlags, int iVersion, const char* pszFragment)
{
	if( iRowKind == IGS_CATALOG_ROW_UPTODATE )
	{
		// Either the version matched or the server has no catalog at all. Both
		// mean "keep what you have", which for a client that has never received
		// one is its own script files.
		ResetCatalogStream();
		return;
	}

	if( iRowKind == IGS_CATALOG_ROW_END )
	{
		if( m_vCatalogRows.empty() )
		{
			ResetCatalogStream();
			return;
		}

		CShopList* pShopList = m_ShopManager.GetListPtr();

		if( pShopList == NULL )
		{
			ResetCatalogStream();
			return;
		}

		pShopList->BeginServerCatalog();

		int iApplied = 0;

		for( int iKind = 0 ; iKind < 3 ; iKind++ )
		{
			// 0 categories, 2 products, 1 packages - in that order, not numeric.
			int iThisKind = (iKind == 1) ? IGS_CATALOG_ROW_PRODUCT : ((iKind == 2) ? IGS_CATALOG_ROW_PACKAGE : IGS_CATALOG_ROW_CATEGORY);

			for( size_t i = 0 ; i < m_vCatalogRows.size() ; i++ )
			{
				if( m_vCatalogRows[i].first != iThisKind )
					continue;

				if( pShopList->AddServerCatalogLine(iThisKind, m_vCatalogRows[i].second) )
					iApplied++;
			}
		}

		pShopList->EndServerCatalog();

		m_pCategoryList = pShopList->GetCategoryListPtr();
		m_pPackageList = pShopList->GetPackageListPtr();
		m_pProductList = pShopList->GetProductListPtr();

		m_iCatalogVersion = m_iCatalogStreamVersion;
		m_bServerCatalogApplied = (iApplied > 0);

		ResetCatalogStream();

		// The zone and category buttons were built from the old catalog, and the
		// shelf is showing packages that may no longer exist.
		if( m_bServerCatalogApplied && g_pInGameShop != NULL )
		{
			g_pInGameShop->InitZoneBtn();
			g_pInGameShop->InitCategoryBtn();
		}

		return;
	}

	if( iRowKind != IGS_CATALOG_ROW_CATEGORY
		&& iRowKind != IGS_CATALOG_ROW_PACKAGE
		&& iRowKind != IGS_CATALOG_ROW_PRODUCT )
	{
		return;
	}

	m_iCatalogStreamVersion = iVersion;

	if( pszFragment != NULL )
	{
		m_strCatalogLine.append(pszFragment);
	}

	if( (iFlags & IGS_CATALOG_FLAG_LINE_END) == 0 )
	{
		return;		// more of this row is still coming
	}

	if( m_strCatalogLine.empty() == false )
	{
		m_vCatalogRows.push_back(std::make_pair(iRowKind, m_strCatalogLine));
	}

	m_strCatalogLine.clear();
}

bool CInGameShopSystem::ScriptDownload()
{
	m_bFirstScriptDownloaded = true;

	::GetCurrentDirectoryA(255, m_szScriptLocalPath);

	char szScriptRemotePathforDMZ[MAX_TEXT_LENGTH];
	sprintf(m_szScriptLocalPath, "%s%s", m_szScriptLocalPath, "\\data\\InGameShopScript");
	strcpy(m_szScriptIPAddress,			"image.webzen.com");
	strcpy(m_szScriptRemotePath,		"/Global/Payment/ProductTransfer");
	strcpy(szScriptRemotePathforDMZ,	"/Global/Payment/DevScriptGB/ProductTransfer");

#ifdef FOR_WORK
	HANDLE hFile; 
	hFile = CreateFile("dmz.ini",     // file to create
						GENERIC_READ,			// open for reading 
						0,						// do not share 
						NULL,                   // default security 
						OPEN_EXISTING,          // existing file only 
						FILE_ATTRIBUTE_NORMAL,  // normal file 
						NULL);                  // no template 
	
	if (hFile != INVALID_HANDLE_VALUE)
	{
		strcpy(m_szScriptRemotePath,	szScriptRemotePathforDMZ);
	}
	CloseHandle(hFile);
#endif // FOR_WORK
	m_ShopManager.SetListManagerInfo(HTTP, m_szScriptIPAddress, 
									"", 
									"", 
									m_szScriptRemotePath, 
									m_szScriptLocalPath, 
									m_ScriptVerInfo,
									10000);
								
	WZResult res = m_ShopManager.LoadScriptList(false);

	if(!res.IsSuccess())
	{
		m_pCategoryList = NULL;
		m_pPackageList = NULL;
		m_pProductList = NULL;

		ShopOpenLock();

		unicode::t_char szText[MAX_TEXT_LENGTH] = {'\0', };
		sprintf(szText, GlobalText[3029], m_ScriptVerInfo.Zone, m_ScriptVerInfo.year, m_ScriptVerInfo.yearId, res.GetErrorMessage());
		CMsgBoxIGSCommon* pMsgBox = NULL;
		CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSCommonLayout), &pMsgBox);
 		pMsgBox->Initialize(GlobalText[3028], szText);
				return false;
	}

	m_CurrentScriptVerInfo = m_ScriptVerInfo;

	//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt <IngameShop Script Download Success!!!>");
	//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt - Ver %d.%d.%d", m_ScriptVerInfo.Zone, m_ScriptVerInfo.year, m_ScriptVerInfo.yearId);

	ShopOpenUnLock();

	CShopList* pShopList = m_ShopManager.GetListPtr();
	
	m_pCategoryList = pShopList->GetCategoryListPtr();
	m_pPackageList = pShopList->GetPackageListPtr();
	m_pProductList = pShopList->GetProductListPtr();

	return true;
}

bool CInGameShopSystem::BannerDownload()
{
	m_bFirstBannerDownloaded = true;

	::GetCurrentDirectoryA(255, m_szBannerLocalPath);

	char szBannerRemotePathforDMZ[MAX_TEXT_LENGTH];
	sprintf(m_szBannerLocalPath, "%s%s", m_szBannerLocalPath, "\\data\\InGameShopBanner");

	strcpy(m_szBannerIPAddress,			"image.webzen.com");
	strcpy(m_szBannerRemotePath,		"/Global/Payment/BannerTransfer");
	strcpy(szBannerRemotePathforDMZ,	"/Global/Payment/DevScriptGB/BannerTransfer");

#ifdef FOR_WORK
	HANDLE hFile; 
	hFile = CreateFile("dmz.ini",     // file to create
						GENERIC_READ,			// open for reading 
						0,						// do not share 
						NULL,                   // default security 
						OPEN_EXISTING,          // existing file only 
						FILE_ATTRIBUTE_NORMAL,  // normal file 
						NULL);                  // no template 
	
	if (hFile != INVALID_HANDLE_VALUE)
	{
		strcpy(m_szBannerRemotePath,	szBannerRemotePathforDMZ);
	}
	CloseHandle(hFile);
#endif // FOR_WORK
	
#ifdef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
	m_BannerManager.SetListManagerInfo(HTTP, m_szBannerIPAddress, 
										"", 
										"", 
										m_szBannerRemotePath, 
										m_szBannerLocalPath, 
										m_BannerVerInfo,
										4000);
#else // KJH_MOD_SHOP_SCRIPT_DOWNLOAD
	m_BannerManager.SetListManagerInfo(HTTP, m_szIPAddress, 
										"", 
										"", 
										m_szBannerRemotePath, 
										m_szBannerLocalPath, 
										m_BannerVerInfo);
#endif // KJH_MOD_SHOP_SCRIPT_DOWNLOAD									

	// DownLoad & Load
	WZResult res = m_BannerManager.LoadScriptList(false);
	
	// DownLoad & Load
	if(!res.IsSuccess())
	{
		m_pBannerList = NULL;
		m_bIsBanner = false;

		// MessageBox
		unicode::t_char szText[MAX_TEXT_LENGTH] = {'\0', };
		sprintf(szText, GlobalText[3030],m_BannerVerInfo.Zone, m_BannerVerInfo.year, m_BannerVerInfo.yearId, res.GetErrorMessage());
		CMsgBoxIGSCommon* pMsgBox = NULL;
		CreateMessageBox(MSGBOX_LAYOUT_CLASS(CMsgBoxIGSCommonLayout), &pMsgBox);
		pMsgBox->Initialize(GlobalText[3028], szText);
		
		return false;
	}
	
	m_CurrentBannerVerInfo = m_BannerVerInfo;
	m_pBannerList = m_BannerManager.GetListPtr();

	m_pBannerList->SetFirst();
	if( m_pBannerList->GetNext(m_BannerInfo) == false )
		return false;

	m_bIsBanner = true;
	return true;
}

#ifdef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
bool CInGameShopSystem::IsScriptDownload()
{
	/*
		A catalog from the database outranks the script files.

		Without this, a server that announces a new script version - which the
		0xD2:0x12 handler does - would send the client back to IBSPackage.txt and
		silently overwrite the lists the database just filled. The script files
		are the fallback now, for a client that has never been sent a catalog or
		whose server has the catalog tables missing; they are no longer the
		source of truth, and they must not be able to reclaim the job.

		Not reachable today, because ScriptDownload runs once before any catalog
		can have arrived - but it is one packet away from being reachable, and
		the failure would look like the admin tool's edits randomly reverting.
	*/
	if( m_bServerCatalogApplied == true )
	{
		return false;
	}

	//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt CallStack - CInGameShopSystem::IsScriptDownload()");
	//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt - Script Ver %d.%d.%d", m_ScriptVerInfo.Zone, m_ScriptVerInfo.year, m_ScriptVerInfo.yearId);
	//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt - Current Ver %d.%d.%d", m_CurrentScriptVerInfo.Zone, m_CurrentScriptVerInfo.year, m_CurrentScriptVerInfo.yearId);
	if( ((m_ScriptVerInfo.year == m_CurrentScriptVerInfo.year) && (m_ScriptVerInfo.yearId == m_CurrentScriptVerInfo.yearId) && (m_ScriptVerInfo.Zone == m_CurrentScriptVerInfo.Zone)) && (m_bFirstScriptDownloaded == true))
	{
		//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt Return - false");
		return false;
	}
	//g_ConsoleDebug->Write(MCD_NORMAL,"InGameShopStatue.Txt Return - true");
	return true;
}

bool CInGameShopSystem::IsBannerDownload()
{
	if( ((m_BannerVerInfo.year == m_CurrentBannerVerInfo.year) && (m_BannerVerInfo.yearId == m_CurrentBannerVerInfo.yearId)	&& (m_BannerVerInfo.Zone == m_CurrentBannerVerInfo.Zone)) && (m_bFirstBannerDownloaded == true))
	{
		return false;
	}

	return true;
}
#endif // KJH_MOD_SHOP_SCRIPT_DOWNLOAD

bool CInGameShopSystem::SelectZone(int iIndex)
{
	int iZoneSeqIndex = GetZoneSeqIndexByIndex(iIndex);
	if( (INGAMESHOP_ERROR_ZERO_SIZE == iZoneSeqIndex) || (INGAMESHOP_ERROR_INVALID_INDEX == iZoneSeqIndex))
	{
		return false;
	}

	if( m_pCategoryList->GetValueByKey(iZoneSeqIndex, m_SelectedZone) )
	{		
		SetCategoryName();

		return true;
	}
	return false;
}

bool CInGameShopSystem::SelectCategory(int iIndex)
{
	m_listDisplayPackage.clear();

	int iCategorySeqIndex = GetCategorySeqIndexByIndex(iIndex);
	if( (INGAMESHOP_ERROR_ZERO_SIZE == iCategorySeqIndex) || (INGAMESHOP_ERROR_INVALID_INDEX == iCategorySeqIndex))
	{
		return false;
	}

	if( m_pCategoryList->GetValueByKey(iCategorySeqIndex, m_SelectedCategory) )
	{
		if( m_SelectedZone.EventFlag == 199 && m_SelectedCategory.EventFlag == 199)
		{
			m_bSelectEventCategory = true;
			m_plistSelectPackage = &m_listEventPackage;
			SendRequestIGS_EventItemList(m_SelectedCategory.ProductDisplaySeq);
			m_bIsRequestEventPackage = true;
		}
		else
		{
			m_bSelectEventCategory = false;
			m_plistSelectPackage = &m_listNormalPackage;
			SetNormalPackage();
		}
	
		return true;
	}

	return false;
}

void CInGameShopSystem::BeginPage()
{
	m_iSelectedPage = 1;
	InitPackagePerPage(m_iSelectedPage);
}

void CInGameShopSystem::NextPage()
{
	if( GetTotalPages() > m_iSelectedPage )
	{
		m_iSelectedPage++;
		InitPackagePerPage(m_iSelectedPage);
	}
}

void CInGameShopSystem::PrePage()
{
	if( m_iSelectedPage > 1 )
	{
		m_iSelectedPage--;
		InitPackagePerPage(m_iSelectedPage);
	}
}

int CInGameShopSystem::GetTotalPages()
{
	return (m_plistSelectPackage->size()/INGAMESHOP_DISPLAY_ITEMLIST_SIZE)+1;
}

int CInGameShopSystem::GetSelectPage()
{
	return m_iSelectedPage;
}

void CInGameShopSystem::SetNormalPackage()
{
	CShopPackage Package;
	int iPackageSeqIndex;

	m_listNormalPackage.clear();
	
	m_SelectedCategory.SetPackagSeqFirst();

	while(m_SelectedCategory.GetPackagSeqNext(iPackageSeqIndex))
	{
		if( !m_pPackageList->GetValueByKey(iPackageSeqIndex, Package))
			break;
		
		m_listNormalPackage.push_back(Package);
	}
	BeginPage();
}

void CInGameShopSystem::InitEventPackage(int iTotalEventPackage)
{
	m_listEventPackage.clear();
	m_listDisplayPackage.clear();
	m_iTotalEventPackage = iTotalEventPackage;
	m_iEventPackageCnt = 0;
	m_iCurrentEventPackage = 0;

	if( m_iTotalEventPackage < 1 )
	{
		m_bIsRequestEventPackage = false;
	}
}

void CInGameShopSystem::InsertEventPackage(int* pPackageSeq)
{
	m_SelectedCategory.SetPackagSeqFirst();
	
	CShopPackage Package;
	
	for( int i=0 ; i<INGAMESHOP_DISPLAY_ITEMLIST_SIZE ; i++)
	{
		if( m_pPackageList->GetValueByKey(pPackageSeq[i], Package))
		{
			m_listEventPackage.push_back(Package);			
		}
		
		m_iCurrentEventPackage++;

		if( m_iTotalEventPackage == m_iCurrentEventPackage )
		{
			BeginPage();
			m_bIsRequestEventPackage = false;
			return;
		}
	}
}

int CInGameShopSystem::GetSizeZones()
{
	return m_mapZoneSeqIndex.size();
}

int CInGameShopSystem::GetSizeCategoriesAsSelectedZone()
{
	return m_SelectedZone.CategoryList.size();
}

int CInGameShopSystem::GetSizePackageAsSelectedCategory()
{
	return m_plistSelectPackage->size();
}

int CInGameShopSystem::GetSizePackageAsDisplayPackage()
{
	return m_listDisplayPackage.size();
}

type_listName CInGameShopSystem::GetZoneName()
{
	return m_listZoneName;
}

type_listName CInGameShopSystem::GetCategoryName()
{
	return m_listCategoryName;
}

WORD CInGameShopSystem::GetPackageItemCode(int iIndex)
{
	type_listPackage::iterator iterPackage = m_listDisplayPackage.begin();

	for(int i = 0; i < (int)m_listDisplayPackage.size(); i++)
	{
		if( iterPackage == m_listDisplayPackage.end() )
			return -1;

		if( i == iIndex )
			break;

		iterPackage++;
	}

	return atoi((*iterPackage).InGamePackageID);
}

void CInGameShopSystem::SetTotalCash(double dTotalCash)
{
	m_dTotalCash = dTotalCash;
}

void CInGameShopSystem::SetTotalPoint(double dTotalPoint)
{
	m_dTotalPoint = dTotalPoint;
}

void CInGameShopSystem::SetTotalMileage(double dTotalMileage)
{
	m_dTotalMileage = dTotalMileage;
}

void CInGameShopSystem::SetCashCreditCard(double dCashCreditCard)
{
	m_dCashCreditCard = dCashCreditCard;
}

void CInGameShopSystem::SetCashPrepaid(double dCashPrepaid)
{
	m_dCashPrepaid = dCashPrepaid;
}

double CInGameShopSystem::GetTotalCash()
{
	return m_dTotalCash;
}

double CInGameShopSystem::GetTotalPoint()
{
	return m_dTotalPoint;
}

double CInGameShopSystem::GetTotalMileage()
{
	return m_dTotalMileage;
}

double CInGameShopSystem::GetCashCreditCard()
{
	return m_dCashCreditCard;
}

double CInGameShopSystem::GetCashPrepaid()
{
	return m_dCashPrepaid;
}

CShopPackage* CInGameShopSystem::GetDisplayPackage(int iIndex)
{
	type_listPackage::iterator iterPackage = m_listDisplayPackage.begin();

	for(int i = 0; i < (int)m_listDisplayPackage.size(); i++)
	{
		if( iterPackage == m_listDisplayPackage.end() )
			return NULL;
		
		if( i == iIndex )
			break;
		
		iterPackage++;
	}
	
	return &(*iterPackage);
}

void CInGameShopSystem::SetIsRequestShopOpenning(bool IsRequestShopOpenning)
{
	m_bIsRequestShopOpenning = IsRequestShopOpenning;
}

bool CInGameShopSystem::GetIsRequestShopOpenning()
{
	return m_bIsRequestShopOpenning;
}

bool CInGameShopSystem::GetPackageInfo(int iPackageSeq, int iPackageAttrType, OUT int& iValue, OUT unicode::t_char* pszText)
{
	CShopPackage Package;
	
	if( m_pPackageList->GetValueByKey(iPackageSeq, Package) == true )
	{
		switch(iPackageAttrType)
		{
		case IGS_PACKAGE_ATT_TYPE_NAME:
			{
				iValue = 0;
				strcpy(pszText, Package.PackageProductName);
				return true;
			}break;
		case IGS_PACKAGE_ATT_TYPE_DESCRIPTION:
			{
				iValue = 0;
				strcpy(pszText, Package.Description);
				return true;
			}break;
		case IGS_PACKAGE_ATT_TYPE_PRICE:
			{
				unicode::t_char szText[MAX_TEXT_LENGTH] = {'\0', };
				// Server price where there is one, so the confirm box quotes the same
				// number the purchase will actually take. IBSPackage.txt knows nothing
				// about sales, so it can only ever offer the list price.
				IGS_SERVER_PRICE ServerPrice;

				if( gProtect.m_MainInfo.CustomCashShop != 0
					&& GetServerPrice(IGS_PRICE_KIND_PACKAGE, iPackageSeq, ServerPrice) == true )
				{
					iValue = ServerPrice.iEffectivePrice;
				}
				else
				{
					iValue = Package.Price;
				}

				ConvertGold(iValue, szText);
				sprintf(pszText, "%s %s", szText, Package.PricUnitName);
				return true;
			}break;
		case IGS_PACKAGE_ATT_TYPE_ITEMCODE:
			{
				iValue = atoi(Package.InGamePackageID);
				pszText[0] = '\0';
				return true;
			}break;
		default:
			{
				iValue = 0;
				pszText[0] = '\0';
			}break;
		}
	}

	return false;
}

bool CInGameShopSystem::GetProductInfoFromPriceSeq(int iProductSeq, int iPriceSeq, int iAttrType, OUT int& iValue, OUT unicode::t_char* pszUnitName)
{
	CShopProduct Product;
	
	m_pProductList->SetPriceSeqFirst(iProductSeq, iPriceSeq);
	
	while(m_pProductList->GetPriceSeqNext(Product))
	{
		if( GetProductInfo(&Product, iAttrType, iValue, pszUnitName) == true )
		{
			return true;
		}
	}
	
	iValue = -1;
	pszUnitName[0] = '\0';	

	return false;
}

bool CInGameShopSystem::GetProductInfoFromProductSeq(int iProductSeq, int iAttrType, OUT int& iValue, OUT unicode::t_char* pszUnitName)
{
	CShopProduct Product;
	
	m_pProductList->SetProductSeqFirst(iProductSeq);
	
	while(m_pProductList->GetProductSeqNext(Product))
	{
		if( GetProductInfo(&Product, iAttrType, iValue, pszUnitName) == true )
		{
			return true;
		}
	}
	
	iValue = -1;
	pszUnitName[0] = '\0';	

	return false;
}

bool CInGameShopSystem::GetProductInfo(CShopProduct* pProduct, int iAttrType, OUT int& iValue, OUT unicode::t_char* pszUnitName)
{
	switch(iAttrType)
	{
	case IGS_PRODUCT_ATT_TYPE_USE_LIMIT_PERIOD:
		{
			if( (pProduct->PropertySeq == 2) || (pProduct->PropertySeq == 28) || (pProduct->PropertySeq == 12)
				|| (pProduct->PropertySeq == 58) || (pProduct->PropertySeq == 10) )
			{
				iValue = atoi(pProduct->Value);
				switch( pProduct->UnitType)
				{
				case 386:
					{		
						if( iValue >= 86400 )
						{
							iValue /= 86400;
							strcpy(pszUnitName, GlobalText[2298]);
						}
						else if( iValue >= 3600 )
						{
							iValue /= 3600;
							strcpy(pszUnitName, GlobalText[2299]);
						}
						else if( iValue >= 60)
						{
							iValue /= 60;
							strcpy(pszUnitName, GlobalText[2300]);
						}
						else 
						{
							strcpy(pszUnitName, GlobalText[2301]);
						}
					}break;		
				case 174:
					{
						if( iValue >= 1440 )
						{
							iValue /= 1440;
							strcpy(pszUnitName, GlobalText[2298]);
						}
						else if( iValue >= 60 )
						{
							iValue /= 60;
							strcpy(pszUnitName, GlobalText[2299]);
						}
						else
						{
							strcpy(pszUnitName, GlobalText[2300]);
						}
					}break;
				case 172:
					{
						if( iValue >= 24 )
						{
							iValue /= 24;
							strcpy(pszUnitName, GlobalText[2298]);
						}
						else
						{
							strcpy(pszUnitName, GlobalText[2299]);
						}
					}break;
				default:
					{
						strcpy(pszUnitName, pProduct->UnitName);
					}break;
				}		
				return true;
			}
		}break;
	case IGS_PRODUCT_ATT_TYPE_AVALIABLE_PERIOD:
		{
			if( (pProduct->PropertySeq == 46) || (pProduct->PropertySeq == 49) || (pProduct->PropertySeq == 48)	|| (pProduct->PropertySeq == 51) || (pProduct->PropertySeq == 52) || (pProduct->PropertySeq == 53) || (pProduct->PropertySeq == 50) || (pProduct->PropertySeq == 60) )
			{
				iValue = atoi(pProduct->Value);
				strcpy(pszUnitName, pProduct->UnitName);
				return true;
			}
		}break;
	case IGS_PRODUCT_ATT_TYPE_NUM:
		{
			if( (pProduct->PropertySeq == 30) || (pProduct->PropertySeq == 11) || (pProduct->PropertySeq == 7) || (pProduct->PropertySeq == 8) || (pProduct->PropertySeq == 9) || (pProduct->PropertySeq == 31) )
			{
				iValue = atoi(pProduct->Value);
				strcpy(pszUnitName, pProduct->UnitName);
				return true;
			}
		}break;
	case IGS_PRODUCT_ATT_TYPE_PRICE:
		{
			// Server price where there is one, so the item-select and confirm boxes
			// quote the same number the purchase will actually take. IBSProduct.txt
			// knows nothing about sales, so it can only ever offer the list price.
			//
			// Keyed by PriceSeq, not ProductSeq: the server sends one row per
			// CustomCashShopProducts.MainIndex, which is the variant. Duration
			// variants share a ProductSeq, so looking that up gave every variant
			// the first one's price while the purchase charged the right one.
			IGS_SERVER_PRICE ServerPrice;

			if( gProtect.m_MainInfo.CustomCashShop != 0
				&& GetServerPrice(IGS_PRICE_KIND_PRODUCT, pProduct->PriceSeq, ServerPrice) == true )
			{
				iValue = ServerPrice.iEffectivePrice;
			}
			else
			{
				iValue = pProduct->Price;
			}

			ConvertGold(iValue, pszUnitName);
			return true;
		}break;
	case IGS_PRODUCT_ATT_TYPE_ITEMCODE:
		{
			iValue = atoi(pProduct->InGamePackageID);
			pszUnitName[0] = '\0';
			return true;
		}break;
	case IGS_PRODUCT_ATT_TYPE_ITEMNAME:
		{
			iValue = -1;
			strcpy(pszUnitName, pProduct->ProductName);
			return true;
		}break;
	case IGS_PRODUCT_ATT_TYPE_PRICE_SEQUENCE:
		{
			iValue = pProduct->PriceSeq;
			pszUnitName[0] = '\0';
			return true;
		}break;
	default:
		{
			iValue = -1;
			pszUnitName[0] = '\0';	
		}break;
	}

	return false;
}

bool CInGameShopSystem::IsRequestEventPackge()
{
	if( m_bIsRequestEventPackage == true )
		return false;

	return true;
}

void CInGameShopSystem::SetRequestEventPackge()
{
	m_bIsRequestEventPackage = false;
}

bool CInGameShopSystem::IsShopOpen()
{
	return m_bIsShopOpenLock ? false : true;
}

bool CInGameShopSystem::IsBanner()
{
	return m_bIsBanner;
}

unicode::t_char* CInGameShopSystem::GetBannerFileName()
{
	if( m_bIsBanner == false )
		return NULL;

	return m_BannerInfo.BannerImagePath;
}

unicode::t_char* CInGameShopSystem::GetBannerURL()
{
	if( m_bIsBanner == false )
		return NULL;
	
	return m_BannerInfo.BannerLinkURL;
}

void CInGameShopSystem::InitZoneInfo()
{
	m_mapZoneSeqIndex.clear();
	m_listZoneName.clear();

	m_pCategoryList->SetFirst();
	CShopCategory Zone;
	
	int i=0;
	while(m_pCategoryList->GetNext(Zone))
	{
		if(1 == Zone.Root)
		{
			m_mapZoneSeqIndex.insert(type_mapZoneSeq::value_type(i++, Zone.ProductDisplaySeq));
			m_listZoneName.push_back(Zone.CategroyName);
		}
	}
}

void CInGameShopSystem::InitPackagePerPage(int iPageIndex)
{
	m_listDisplayPackage.clear();

	type_listPackage::iterator	iterlistPackage;

	iterlistPackage = m_plistSelectPackage->begin();

	int iBeginDisplayItemIndex = INGAMESHOP_DISPLAY_ITEMLIST_SIZE*(iPageIndex-1);
	for(int i=0 ; i<iBeginDisplayItemIndex ; i++)
	{
		iterlistPackage++;
	}
	
	for(int j=0 ; j<INGAMESHOP_DISPLAY_ITEMLIST_SIZE ; j++)
	{
		if( iterlistPackage == m_plistSelectPackage->end())
			break;

		m_listDisplayPackage.push_back(*iterlistPackage);
		iterlistPackage++;
	}
}

int CInGameShopSystem::GetZoneSeqIndexByIndex(int iIndex)
{
	if( GetSizeZones() <= 0 )
		return INGAMESHOP_ERROR_ZERO_SIZE;

	type_mapZoneSeq::iterator iterZoneSeqIndex = m_mapZoneSeqIndex.find(iIndex);

	if( iterZoneSeqIndex == m_mapZoneSeqIndex.end() )
		return INGAMESHOP_ERROR_INVALID_INDEX;

	return (int)iterZoneSeqIndex->second;
}

int CInGameShopSystem::GetCategoryButtonIndexOfPackage(int iPackageSeq)
{
	if( m_pPackageList == NULL || iPackageSeq <= 0 )
		return -1;

	CShopPackage Package;

	if( m_pPackageList->GetValueByKey(iPackageSeq, Package) == 0 )
		return -1;

	// Field 1 of the script row is the category the card is drawn on, but the
	// buttons are addressed by position, so it has to be walked back.
	int iCategorySeq = Package.ProductDisplaySeq;
	int iSize = GetSizeCategoriesAsSelectedZone();

	for(int i = 0 ; i < iSize ; i++)
	{
		if( GetCategorySeqIndexByIndex(i) == iCategorySeq )
			return i;
	}

	return -1;
}

int CInGameShopSystem::GetCategorySeqIndexByIndex(int iIndex)
{
	int iCategorySeqIndex = 0;
	bool bRes = false;

	if( GetSizeCategoriesAsSelectedZone() <= 0 )
		return INGAMESHOP_ERROR_ZERO_SIZE;

	m_SelectedZone.SetCategoryFirst();
	for(int i=0 ; i<=iIndex ; i++ )
	{
		bRes = m_SelectedZone.GetCategoryNext(iCategorySeqIndex);
	}

	if( bRes == false )
		return INGAMESHOP_ERROR_INVALID_INDEX;

	return iCategorySeqIndex;
}

void CInGameShopSystem::SetCategoryName()
{
	m_listCategoryName.clear();

	int iCategorySeqIndex;
	CShopCategory Category;
	m_SelectedZone.SetCategoryFirst();

	while( m_SelectedZone.GetCategoryNext(iCategorySeqIndex) )
	{
		m_pCategoryList->GetValueByKey(iCategorySeqIndex, Category);
		m_listCategoryName.push_back(Category.CategroyName);
	}
}

void CInGameShopSystem::ShopOpenLock()
{
	m_bIsShopOpenLock = true;
}

void CInGameShopSystem::ShopOpenUnLock()
{
	m_bIsShopOpenLock = false;
}

CListVersionInfo CInGameShopSystem::GetScriptVer()
{
	return m_ScriptVerInfo;
}

CListVersionInfo CInGameShopSystem::GetBannerVer()
{
	return m_BannerVerInfo;
}

#ifdef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
CListVersionInfo CInGameShopSystem::GetCurrentScriptVer()
{
	return m_CurrentScriptVerInfo;
}

CListVersionInfo CInGameShopSystem::GetCurrentBannerVer()
{
	return m_CurrentBannerVerInfo;
}
#endif // KJH_MOD_SHOP_SCRIPT_DOWNLOAD

//////////////////////////////////////////////////////////////////////
// Server-side shelf prices - see the comment on IGS_SERVER_PRICE.
//////////////////////////////////////////////////////////////////////

void CInGameShopSystem::ClearServerPrices()
{
	m_mapServerPackagePrice.clear();
	m_mapServerProductPrice.clear();
}

void CInGameShopSystem::SetServerPrice(int iKind, int iSeq, int iListPrice, int iEffectivePrice, int iDiscountPercent)
{
	// A zero or negative effective price is not a free item, it is a row the
	// server could not resolve. Dropping it leaves the script price showing.
	if( iEffectivePrice <= 0 )
	{
		return;
	}

	IGS_SERVER_PRICE Price;

	Price.iListPrice		= ((iListPrice > 0) ? iListPrice : iEffectivePrice);
	Price.iEffectivePrice	= iEffectivePrice;
	Price.iDiscountPercent	= iDiscountPercent;

	// A percent with no actual saving is not a sale worth drawing a badge for.
	if( Price.iEffectivePrice >= Price.iListPrice )
	{
		Price.iDiscountPercent = 0;
	}

	if( iKind == IGS_PRICE_KIND_PRODUCT )
	{
		m_mapServerProductPrice[iSeq] = Price;
	}
	else
	{
		m_mapServerPackagePrice[iSeq] = Price;
	}
}

bool CInGameShopSystem::GetServerPrice(int iKind, int iSeq, OUT IGS_SERVER_PRICE& Price)
{
	std::map<int, IGS_SERVER_PRICE>& mapPrice =
		((iKind == IGS_PRICE_KIND_PRODUCT) ? m_mapServerProductPrice : m_mapServerPackagePrice);

	std::map<int, IGS_SERVER_PRICE>::iterator it = mapPrice.find(iSeq);

	if( it == mapPrice.end() )
	{
		return false;
	}

	Price = it->second;

	return true;
}

void CInGameShopSystem::ClearServerPackageNames()
{
	m_mapServerPackageName.clear();
}

void CInGameShopSystem::SetServerPackageName(int iSeq, const char* pszName)
{
	if( pszName == NULL || pszName[0] == '\0' )
	{
		return;
	}

	m_mapServerPackageName[iSeq] = pszName;
}

bool CInGameShopSystem::GetServerPackageName(int iSeq, OUT char* pszOutName, int cbOutName)
{
	std::map<int, std::string>::iterator it = m_mapServerPackageName.find(iSeq);

	if( it == m_mapServerPackageName.end() )
	{
		return false;
	}

	strncpy(pszOutName, it->second.c_str(), cbOutName - 1);
	pszOutName[cbOutName - 1] = '\0';

	return true;
}

#endif // KJH_ADD_INGAMESHOP_UI_SYSTEM
