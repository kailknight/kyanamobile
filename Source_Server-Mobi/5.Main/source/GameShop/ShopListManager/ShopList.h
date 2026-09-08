
/**************************************************************************************************

스크립트 목록 최 상위 객체

카테고리 목록, 패키지 목록, 상품(속성) 목록을 가지고 있다.

**************************************************************************************************/

#pragma once

#include "ShopPackage.h"
#include "ShopProduct.h"

#include "ShopCategoryList.h"
#include "ShopPackageList.h"
#include "ShopProductList.h"

class CShopList  
{
public:
	CShopList();
	virtual ~CShopList();

	WZResult LoadCategroy(const char* szFilePath);
	WZResult LoadPackage (const char* szFilePath);
	WZResult LoadProduct (const char* szFilePath);	

	/*
		The same three lists, filled from the server instead of from disk.

		The catalog the player sees used to exist only as IBSCategory.txt,
		IBSPackage.txt and IBSProduct.txt, so nothing edited in the admin tool
		could reach the shelf. These take the identical @-delimited rows off the
		wire and hand them to the identical parsers - SetCategory, SetPackage,
		SetProduct - so everything downstream is unchanged.

		BeginServerCatalog clears all three lists, so the caller must be sure the
		whole catalog arrived before starting: a half-applied catalog is an empty
		shop. CInGameShopSystem buffers the rows and only calls these once the
		end-of-catalog marker lands.
	*/
	void BeginServerCatalog();
	bool AddServerCatalogLine(int iRowKind, const std::string& strLine);
	void EndServerCatalog();

	enum SERVER_CATALOG_ROW
	{
		SERVER_CATALOG_CATEGORY = 0,
		SERVER_CATALOG_PACKAGE  = 1,
		SERVER_CATALOG_PRODUCT  = 2,
	};


	CShopCategoryList* GetCategoryListPtr() {return m_CategoryListPtr;};	// 카테고리 목록 가져온다.
	CShopPackageList*  GetPackageListPtr()  {return m_PackageListPtr;};		// 패키지 목록 가져온다.
	CShopProductList*  GetProductListPtr()  {return m_ProductListPtr;};		// 상품(속성) 목록 가져온다.

	void SetCategoryListPtr(CShopCategoryList* CategoryListPtr);
	void SetPackageListPtr (CShopPackageList* PackagePtr);
	void SetProductListPtr (CShopProductList* ProductListPtr);

private:	
	CShopCategoryList* m_CategoryListPtr;
	CShopPackageList*  m_PackageListPtr;
	CShopProductList*  m_ProductListPtr;

	FILE_ENCODE IsFileEncodingUtf8(const char* szFilePath);
	std::string GetDecodeingString(const char* str, FILE_ENCODE encode);
};