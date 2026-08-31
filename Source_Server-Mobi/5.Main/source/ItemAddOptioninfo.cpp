// ItemAddOptioninfo.cpp: implementation of the ItemAddOptioninfo class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "stdafx.h"
#include "ZzzOpenglUtil.h"
#include "ZzzTexture.h"
#include "UIManager.h"

#include "ItemAddOptioninfo.h"
#include "wsclientinline.h"

#define ITEMADDOPTION_DATA_FILE "Data\\Local\\ItemAddOption.bmd"

ItemAddOptioninfo* ItemAddOptioninfo::MakeInfo()
{
	ItemAddOptioninfo* option = new ItemAddOptioninfo();
	return option;
}

ItemAddOptioninfo::ItemAddOptioninfo()
{
	bool Result = true;

	Result = OpenItemAddOptionInfoFile( ITEMADDOPTION_DATA_FILE );

	if( !Result )
	{
		char szMessage[256];
		::sprintf(szMessage, "%s file not found.\r\n", ITEMADDOPTION_DATA_FILE);
		g_ErrorReport.Write(szMessage);
		::MessageBox(g_hWnd, szMessage, NULL, MB_OK);
		::PostMessage(g_hWnd, WM_DESTROY, 0, 0);
	}
}

ItemAddOptioninfo::~ItemAddOptioninfo()
{

}

const bool ItemAddOptioninfo::OpenItemAddOptionInfoFile( const std::string& filename )
{
	FILE *fp = ::fopen( filename.c_str(), "rb" );
	if ( fp != NULL )
	{
		int nSize = sizeof(ITEM_ADD_OPTION) * MAX_ITEM;

		::fread(m_ItemAddOption, nSize, 1, fp);
		::BuxConvert((BYTE*)m_ItemAddOption, nSize);
		::fclose(fp);

		return true;
	}
	return false;
}

void ItemAddOptioninfo::GetItemAddOtioninfoText( std::vector<std::string>& outtextlist, int type )
{
	int optiontype = 0;
	int optionvalue = 0;

	// An item type with no 380 entry at all reads m_byOption1/2 == 0, which
	// matches none of the cases below - TempText was then left uninitialized
	// and pushed anyway, so the tooltip printed raw stack garbage as its two
	// "380 option" lines. Bounds-check the lookup for the same reason: type is
	// an item index straight from an ITEM and nothing above guarantees it is
	// inside this table.
	if( type < 0 || type >= MAX_ITEM )
	{
		return;
	}

	for( int i = 0; i < 2; ++i )
	{
		std::string text;
		char TempText[100] = { 0, };

		if( i == 0 )
		{
			optiontype  = m_ItemAddOption[type].m_byOption1;
			optionvalue = m_ItemAddOption[type].m_byValue1;
		}
		else 
		{
			optiontype  = m_ItemAddOption[type].m_byOption2;
			optionvalue = m_ItemAddOption[type].m_byValue2;
		}

 		switch( optiontype )
		{
		case 1: wsprintf( TempText, GlobalText[2184], optionvalue );
			break;
		case 2: wsprintf( TempText, GlobalText[2185], optionvalue );
			break;
		case 3: wsprintf( TempText, GlobalText[2186], optionvalue );
			break;
		case 4: wsprintf( TempText, GlobalText[2187], optionvalue );
			break;
		case 5: wsprintf( TempText, GlobalText[2188], optionvalue );
			break;
		case 6: wsprintf( TempText, GlobalText[2189], optionvalue );
			break;
		case 7: wsprintf( TempText, "%s", GlobalText[2190] );
			break;
		case 8: wsprintf( TempText, GlobalText[2191], optionvalue );
			break;
		default:
			// No 380 option in this slot (0) or an option id this build doesn't
			// know - emit nothing rather than an empty/garbage tooltip line.
			// Callers already handle a short (or empty) list: MixMgr.cpp:605
			// explicitly tests optionTextlist.empty().
			continue;
		}

		text = TempText;
		outtextlist.push_back( text );
	}
}
