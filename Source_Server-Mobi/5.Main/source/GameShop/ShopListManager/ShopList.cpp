//************************************************************************
//
// Decompiled by @myheart, @synth3r
// <https://forum.ragezone.com/members/2000236254.html>
//
//
// FILE: ShopList.cpp
//
//

#include "stdafx.h"
#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM
#include "ShopList.h"

#include <fstream>

// GetScriptPath() (ListManager.cpp) builds these paths with Windows-style
// backslashes. That's fine for GetFileAttributesA/fopen, which normalize
// backslashes to forward slashes internally on Android (Platform/PlatformDefs.h),
// but std::ifstream::open() here goes straight to the OS with no such
// translation, so a backslash-built path silently fails to open on Android's
// POSIX filesystem (backslash isn't a separator there) - "package file open
// fail" even though GetFileAttributesA just confirmed the file exists.
// Forward slashes work identically on Windows, so this is safe unconditionally.
static std::string NormalizeShopFilePath(const char* szFilePath)
{
	std::string path = szFilePath ? szFilePath : "";
	for (std::string::iterator it = path.begin(); it != path.end(); ++it)
	{
		if (*it == '\\')
		{
			*it = '/';
		}
	}
	return path;
}

CShopList::CShopList() // OK
{
	this->m_CategoryListPtr = new CShopCategoryList;
	this->m_PackageListPtr = new CShopPackageList;
	this->m_ProductListPtr = new CShopProductList;
}

CShopList::~CShopList() // OK
{
	SAFE_DELETE(m_CategoryListPtr);
	SAFE_DELETE(m_PackageListPtr);
	SAFE_DELETE(m_ProductListPtr);
}

WZResult CShopList::LoadCategroy(const char* szFilePath) // OK
{
	WZResult result;

	const std::string normalizedPath = NormalizeShopFilePath(szFilePath);
	szFilePath = normalizedPath.c_str();

	FILE_ENCODE enc = this->IsFileEncodingUtf8(szFilePath);

	std::ifstream ifs;

	ifs.open(szFilePath,std::ifstream::in);

	DWORD LastError = GetLastError();

	for(int n = 0; !ifs.is_open()&&n<10;++n)
	{
		Sleep(0x64);
		ifs.open(szFilePath,std::ifstream::in);
		LastError = GetLastError();
	}

	char buff[1024] = {0};

	if(ifs.is_open())
	{
		this->GetCategoryListPtr()->Clear();

		while(true)
		{
			memset(buff,0,sizeof(buff));

			if(!ifs.getline(buff,sizeof(buff)))
				break;

			CShopCategory cat;

			const std::string data = this->GetDecodeingString(buff,enc);

			if(cat.SetCategory(data))
			{
				this->GetCategoryListPtr()->Append(cat);
			}
		}

		ifs.close();
	}
	else
	{
		result.SetResult(PT_LOADLIBRARY,LastError,"package file open fail");
	}

	return result;
}

WZResult CShopList::LoadPackage (const char* szFilePath) // OK
{
	WZResult result;

	const std::string normalizedPath = NormalizeShopFilePath(szFilePath);
	szFilePath = normalizedPath.c_str();

	FILE_ENCODE enc = this->IsFileEncodingUtf8(szFilePath);

	std::ifstream ifs;

	ifs.open(szFilePath,std::ifstream::in);

	DWORD LastError = GetLastError();

	for(int n = 0; !ifs.is_open()&&n<10;++n)
	{
		Sleep(0x64);
		ifs.open(szFilePath,std::ifstream::in);
		LastError = GetLastError();
	}

	char buff[1024] = {0};

	if(ifs.is_open())
	{
		this->GetPackageListPtr()->Clear();

		while(true)
		{
			if(!ifs.getline(buff,sizeof(buff)))
				break;

			CShopPackage pack;

			if(pack.SetPackage(this->GetDecodeingString(buff,enc)))
			{
				this->GetPackageListPtr()->Append(pack);
				this->GetCategoryListPtr()->InsertPackage(pack.ProductDisplaySeq,pack.PackageProductSeq);
			}
		}

		ifs.close();
	}
	else
	{
		result.SetResult(4,LastError,"package file open fail");
	}

	return result;
}

WZResult CShopList::LoadProduct (const char* szFilePath) // OK
{
	static WZResult result;

	result.BuildSuccessResult();

	const std::string normalizedPath = NormalizeShopFilePath(szFilePath);
	szFilePath = normalizedPath.c_str();

	FILE_ENCODE enc = this->IsFileEncodingUtf8(szFilePath);

	std::ifstream ifs;

	ifs.open(szFilePath,std::ifstream::in);

	DWORD LastError = GetLastError();

	for(int n = 0; !ifs.is_open()&&n<10;++n)
	{
		Sleep(0x64);
		ifs.open(szFilePath,std::ifstream::in);
		LastError = GetLastError();
	}

	char buff[1024] = {0};

	if(ifs.is_open())
	{
		this->GetProductListPtr()->Clear();

		while(true)
		{
			memset(buff,0,sizeof(buff));

			if(!ifs.getline(buff,sizeof(buff)))
				break;

			CShopProduct product;

			std::string data = this->GetDecodeingString(buff,enc);

			if(product.SetProduct(data))
			{
				this->GetProductListPtr()->Append(product);
			}
		}

		ifs.close();
	}
	else
	{
		result.SetResult(4,LastError,"package file open fail");
	}

	return result;
}

/*
	Start a fresh catalog from the server.

	Clears all three lists. Nothing calls this until the whole catalog has
	arrived and been buffered, because a stream that dies half way through would
	otherwise leave the player with part of a shop and no way to tell.
*/
void CShopList::BeginServerCatalog()
{
	if(this->GetCategoryListPtr() != NULL)
	{
		this->GetCategoryListPtr()->Clear();
	}

	if(this->GetPackageListPtr() != NULL)
	{
		this->GetPackageListPtr()->Clear();
	}

	if(this->GetProductListPtr() != NULL)
	{
		this->GetProductListPtr()->Clear();
	}
}

/*
	One row, already reassembled, already the exact text the .txt file would have
	held.

	Packages are the interesting case: LoadPackage does not only append, it also
	calls InsertPackage to attach the package to its category, and
	CShopCategoryList::InsertPackage returns 0 in silence when the category is
	not there yet. That is why the caller replays categories before packages -
	and why this mirrors LoadPackage exactly rather than only appending.
*/
bool CShopList::AddServerCatalogLine(int iRowKind, const std::string& strLine)
{
	if(strLine.empty())
	{
		return false;
	}

	/*
		The same UTF-8 decode LoadCategory/LoadPackage/LoadProduct apply to a
		line off disk.

		The script files are UTF-8 and the loader converts them to the client's
		ANSI codepage before parsing; the DataServer now sends UTF-8 for exactly
		that reason. Skipping this step was what turned every Vietnamese name
		into question marks - the bytes arrived intact and were then rendered as
		if they were already ANSI.

		ASCII rows pass through unchanged, so this costs nothing on the numbers
		and delimiters that make up most of a line.
	*/
	const std::string strDecoded = this->GetDecodeingString(strLine.c_str(), FE_UTF8);

	if(strDecoded.empty())
	{
		return false;
	}

	switch(iRowKind)
	{
	case SERVER_CATALOG_CATEGORY:
		{
			if(this->GetCategoryListPtr() == NULL)
				return false;

			CShopCategory cat;

			if(cat.SetCategory(strDecoded) == 0)
				return false;

			this->GetCategoryListPtr()->Append(cat);
			return true;
		}

	case SERVER_CATALOG_PACKAGE:
		{
			if(this->GetPackageListPtr() == NULL || this->GetCategoryListPtr() == NULL)
				return false;

			CShopPackage pack;

			if(pack.SetPackage(strDecoded) == 0)
				return false;

			this->GetPackageListPtr()->Append(pack);
			this->GetCategoryListPtr()->InsertPackage(pack.ProductDisplaySeq,pack.PackageProductSeq);
			return true;
		}

	case SERVER_CATALOG_PRODUCT:
		{
			if(this->GetProductListPtr() == NULL)
				return false;

			CShopProduct product;

			if(product.SetProduct(strDecoded) == 0)
				return false;

			this->GetProductListPtr()->Append(product);
			return true;
		}
	}

	return false;
}

void CShopList::EndServerCatalog()
{
	// Nothing to finalise: Append and InsertPackage have already done the work
	// as each row arrived. Kept as a named step so the call site reads as the
	// three-part transaction it is, and so there is somewhere obvious to put an
	// integrity check if one is ever wanted.
}

void CShopList::SetCategoryListPtr(CShopCategoryList* CategoryListPtr) // OK
{
	m_CategoryListPtr = CategoryListPtr;
}

void CShopList::SetPackageListPtr (CShopPackageList* PackagePtr) // OK
{
	m_PackageListPtr = PackagePtr;
}

void CShopList::SetProductListPtr (CShopProductList* ProductListPtr) // OK
{
	m_ProductListPtr = ProductListPtr;
}

FILE_ENCODE CShopList::IsFileEncodingUtf8(const char* szFilePath) // OK
{
	std::ifstream ifs;

	ifs.open(szFilePath,std::ifstream::in);

	if(!ifs.is_open())
	{
		return FE_ANSI;
	}

	char buff[16] = {0};

	ifs.getline(buff,sizeof(buff));

	ifs.close();

	if(strlen(buff)<3)
	{
		return FE_ANSI;
	}

	if(buff[0]==0xEF&&buff[1]==0xBB&&buff[2]==0xBF)
	{
		return FE_UTF8;
	}

	if(buff[0]==0xFF&&buff[1]==0xFE)
	{
		return FE_UNICODE;
	}

	return FE_ANSI;
}

std::string CShopList::GetDecodeingString(const char* str,FILE_ENCODE encode) // OK
{
	std::string result;

	if(encode==FE_UTF8)
	{
		int cchWideChar = MultiByteToWideChar(CP_UTF8,0,str,-1,0,0);
		LPWSTR lpWideCharStr = new WCHAR[cchWideChar+1];
		MultiByteToWideChar(CP_UTF8,0,str,-1,lpWideCharStr,cchWideChar);

		cchWideChar = WideCharToMultiByte(0,0,lpWideCharStr,-1,0,0,0,0);
		char* buff = new char[cchWideChar+1];
		WideCharToMultiByte(0,0,lpWideCharStr,-1,buff,cchWideChar,0,0);

		result = (buff);

		delete[] lpWideCharStr;
		delete[] buff;
	}
	else
	{
		if(encode==FE_UNICODE)
		{
			result = "\0";
		}
		else
		{
			result = (str);
		}
	}

	return result;
}
#endif