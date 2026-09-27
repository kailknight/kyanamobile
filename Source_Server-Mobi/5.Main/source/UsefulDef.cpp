//*****************************************************************************
// File: UsefulDef.cpp
//
// Desc: 유용한 정의 모음.
//
// producer: Ahn Sang-Kyu
//*****************************************************************************

#include "stdafx.h"
#include <windows.h>
#include "UsefulDef.h"
#include "UIControls.h"

extern float g_fScreenRate_x;

bool ReduceStringByPixel(LPTSTR lpszDst, int nDstSize, LPCTSTR lpszSrc, int nPixel)
{
	SIZE size;
	g_pMultiLanguage->_GetTextExtentPoint32(g_pRenderText->GetFontDC(), lpszSrc, lstrlen(lpszSrc), &size);
	int nSrcWidth = int(size.cx / g_fScreenRate_x);

	if (nSrcWidth <= nPixel)
	{
		::strncpy(lpszDst, lpszSrc, nDstSize - 1);
		lpszDst[nDstSize - 1] = '\0';
		return false;
	}

	::CutText3(lpszSrc, lpszDst, nPixel - 6, 1, nDstSize);
	::strcat(lpszDst, "...");
	return true;
}

int DivideStringByPixel(LPTSTR alpszDst, int nDstRow, int nDstColumn, LPCTSTR lpszSrc,int nPixelPerLine, bool bSpaceInsert, const char szNewlineChar)
{
	if (NULL == alpszDst || 0 >= nDstRow || 0 >= nDstColumn || NULL == lpszSrc || 16 > nPixelPerLine)
		return 0;

	// Bounded: the longest thing handed to this is a package description, and
	// CShopPackage::Description is 2048 bytes - twice this buffer. An unchecked
	// strcpy here was one long description away from a stack overwrite, in a
	// function 24 call sites share.
	char szWorkSrc[1024];
	::strncpy(szWorkSrc, lpszSrc, sizeof(szWorkSrc)-1);
	szWorkSrc[sizeof(szWorkSrc)-1] = '\0';

	char szWorkToken[1024];
	int nLine = 0;

	// strtok takes a zero-terminated SET of separators. This used to pass the
	// address of the lone char, so whatever followed it on the stack counted
	// too: on the iOS build that is szWorkToken, holding the previous word, so
	// every letter of it became a break and was deleted - tooltip lines came
	// out as "Du" / "ability:" and "r" / "qui" / "ed:". Android survived only
	// because its stack layout happens to leave a zero there.
	const char szDelimiters[2] = { szNewlineChar, '\0' };

	char* pszToken = ::strtok(szWorkSrc, szDelimiters);

	// Each chunk may only use the rows the earlier chunks left. Passing the full
	// nDstRow to every chunk let a multi-chunk text write rows past the end of
	// the caller's buffer - heap corruption when that buffer is a member array,
	// as in CNewUIMyQuestInfoWindow::m_aszMsg.
	while (pszToken != NULL && nLine < nDstRow)
	{
		if (bSpaceInsert)
		{
			::snprintf(szWorkToken, sizeof(szWorkToken), " %s", pszToken);
			nLine += ::CutText3(szWorkToken, alpszDst + nLine * nDstColumn, nPixelPerLine, nDstRow - nLine, nDstColumn);
		}
		else
		{
			nLine += ::CutText3(pszToken, alpszDst + nLine * nDstColumn, nPixelPerLine, nDstRow - nLine, nDstColumn);
		}
		pszToken = ::strtok(NULL, szDelimiters);
	}

	return nLine;
}

int DivideString(LPTSTR alpszDst, int nDstRow, int nDstColumn, LPCTSTR lpszSrc)
{
	if (NULL == lpszSrc)
		return 0;

	int nSrcLen = ::strlen(lpszSrc);
	if (0 == nSrcLen)
		return 0;

	int nSrcPos = 0;
	int nDstStart = 0;
	int nDstLen = 1;
	int nLineCount = 0;
	
	while (TRUE)
	{
		if (0x80 & lpszSrc[nSrcPos])
		{
			++nSrcPos;
			++nDstLen;
		}

		if ('/' == lpszSrc[nSrcPos])
		{
			::strncpy(alpszDst + nLineCount * nDstColumn, lpszSrc + nDstStart, nDstLen - 1);
			++nLineCount;
			nDstStart = nSrcPos + 1;
			nDstLen = 0;
		}
		else if (nDstLen >= nDstColumn)
		{
			nSrcPos -= 2;
			nDstLen -= 2;
			::strncpy(alpszDst + nLineCount * nDstColumn, lpszSrc + nDstStart, nDstLen);
			++nLineCount;
			nDstStart = nSrcPos + 1;
			nDstLen = 0;
		}
		else if (nSrcPos == nSrcLen - 1)
		{
			::strncpy(alpszDst + nLineCount * nDstColumn, lpszSrc + nDstStart, nDstLen);
			break;
		}
		else if (nDstLen == nDstColumn - 1)
		{
			::strncpy(alpszDst + nLineCount * nDstColumn, lpszSrc + nDstStart, nDstLen);
			++nLineCount;
			nDstStart = nSrcPos + 1;
			nDstLen = 0;
		}

		if (nDstRow == nLineCount)
			break;
		
		++nSrcPos;
		++nDstLen;
	}
	
	return nLineCount + 1;
}

BOOL CheckErrString(LPTSTR lpszTarget)
{
	int i = 0;
	int nLen = ::strlen(lpszTarget);
	while (i < nLen)
	{
		if (0x80 & lpszTarget[i])
		{
			if (i == nLen - 1)
			{
				lpszTarget[i] = NULL;
				return FALSE;
			}
			else
				++i;
		}
		++i;
	}

	return TRUE;
}
