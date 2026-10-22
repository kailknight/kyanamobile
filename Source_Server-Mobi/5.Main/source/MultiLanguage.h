// MultiLanguage.h: interface for the CMultiLanguage class.
//////////////////////////////////////////////////////////////////////

#pragma once

// ---------------------------------------------------------------------------
// Language selection helpers (g_strSelectedML is defined in Winmain.cpp).
//
// The selected language is a 3 letter code: "Eng" or "Vtm". "Vn" and "Vie" were
// used by older checks and are treated as Vietnamese so a stale setting still
// works. Anything unknown is English.
// ---------------------------------------------------------------------------
bool IsVietnameseLanguage();

// Writes the canonical code ("Eng" / "Vtm") into pszOut (needs >= 4 bytes).
void NormalizeLanguageCode(const char* pszIn, char* pszOut);

// Fills pszOut from a path template in which every "%s" is the language code,
// e.g. "Data\\Local\\%s\\Item_%s.bmd". If the selected language is not English
// and that file does not exist, the English file is used instead. Returns pszOut.
//
// Every loader of a Data\Local\<ML> file must go through this: a missing file
// makes the client show a message box and exit, so a language pack that only
// translates some of its files has to fall back to English file by file.
// The template may contain at most four "%s".
const char* LocalLanguageFile(char* pszOut, size_t cbOut, const char* pszTemplate);

class CMultiLanguage
{

private:
	static CMultiLanguage *ms_Singleton;

	BYTE byLanguage;
	int iCodePage;
	int iNumByteForOneCharUTF8;

	CMultiLanguage()	{ ms_Singleton = this; };				

public:
	CMultiLanguage(std::string strSelectedML);
	~CMultiLanguage()	{ ms_Singleton = 0; };

	BYTE GetLanguage();				// Getters
	int GetCodePage();
	int GetNumByteForOneCharUTF8();

	BOOL IsCharUTF8(const char* pszText);
	int	 ConvertCharToWideStr(std::wstring& wstrDest, LPCSTR lpString);
	int  ConvertWideCharToStr(std::string& strDest, LPCWSTR lpwString, int iConversionType = CP_UTF8);
	void ConvertANSIToUTF8OrViceVersa(std::string& strDest, LPCSTR lpString);
	int	 GetClosestBlankFromCenter(const std::wstring wstrTarget);
	
	WPARAM ConvertFulltoHalfWidthChar(DWORD wParam);
	BOOL _GetTextExtentPoint32(HDC hdc, LPCWSTR lpString, int cbString, LPSIZE lpSize);
	BOOL _GetTextExtentPoint32(HDC hdc, LPCSTR lpString, int cbString, LPSIZE lpSize);

	BOOL _TextOut(HDC hdc, int nXStart, int nYStart, LPCWSTR lpString, int cbString);
	BOOL _TextOut(HDC hdc, int nXStart, int nYStart, LPCSTR lpString, int cbString);
	
	static CMultiLanguage* GetSingletonPtr() { return ms_Singleton; };
};
