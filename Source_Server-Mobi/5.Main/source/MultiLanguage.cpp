// MultiLanguage.cpp: implementation of the CMultiLanguage class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MultiLanguage.h"


CMultiLanguage* CMultiLanguage::ms_Singleton = NULL;

extern std::string g_strSelectedML;

bool IsVietnameseLanguage()
{
	return _stricmp(g_strSelectedML.c_str(), "Vtm") == 0
		|| _stricmp(g_strSelectedML.c_str(), "Vie") == 0
		|| _stricmp(g_strSelectedML.c_str(), "Vn") == 0;
}

void NormalizeLanguageCode(const char* pszIn, char* pszOut)
{
	const bool vi = pszIn != NULL
		&& (_stricmp(pszIn, "Vtm") == 0 || _stricmp(pszIn, "Vie") == 0 || _stricmp(pszIn, "Vn") == 0);
	strcpy(pszOut, vi ? "Vtm" : "Eng");
}

const char* LocalLanguageFile(char* pszOut, size_t cbOut, const char* pszTemplate)
{
	// Template with every %s replaced by the selected code, then by "Eng" if that
	// file is missing. sprintf with four arguments is safe for any template with
	// fewer %s because the extra arguments are ignored.
	const char* ml = g_strSelectedML.c_str();
	snprintf(pszOut, cbOut, pszTemplate, ml, ml, ml, ml);
	pszOut[cbOut - 1] = 0;

	if (!IsVietnameseLanguage())
	{
		return pszOut;
	}
	if (GetFileAttributesA(pszOut) != INVALID_FILE_ATTRIBUTES)
	{
		return pszOut;
	}

	snprintf(pszOut, cbOut, pszTemplate, "Eng", "Eng", "Eng", "Eng");
	pszOut[cbOut - 1] = 0;
	return pszOut;
}

// Data\Custom\Lang_Vtm.txt: one "English<TAB>Vietnamese" pair per line, UTF-8.
// Keys are the English format string exactly as the source writes it (with its %d / %s).
// A Vietnamese value must keep the same placeholders in the same order, or it is ignored.
// Loaded once, on first use. Only used when the selected language is Vietnamese.
static std::map<std::string, std::string>& VietnameseTemplates()
{
	static std::map<std::string, std::string> s_map;
	static bool s_loaded = false;
	if (!s_loaded)
	{
		s_loaded = true;
		FILE* fp = fopen("Data\\Custom\\Lang_Vtm.txt", "rb");
		if (fp != NULL)
		{
			std::string text;
			char buf[1024];
			size_t got;
			while ((got = fread(buf, 1, sizeof(buf), fp)) > 0)
			{
				text.append(buf, got);
			}
			fclose(fp);

			size_t pos = 0;
			while (pos < text.size())
			{
				size_t end = text.find('\n', pos);
				if (end == std::string::npos)
				{
					end = text.size();
				}
				std::string line = text.substr(pos, end - pos);
				pos = end + 1;
				if (!line.empty() && line.back() == '\r')
				{
					line.pop_back();
				}
				size_t tab = line.find('\t');
				if (tab == std::string::npos || tab == 0)
				{
					continue;
				}
				s_map[line.substr(0, tab)] = line.substr(tab + 1);
			}
		}
	}
	return s_map;
}

static int CountPlaceholders(const char* s, char out[][4], int cap)
{
	// Only the printf conversions that the game uses: %d %s %u (with optional width/precision
	// ignored) and %%, which is literal. Returns the number found, in order.
	int n = 0;
	for (const char* p = s; *p != '\0'; ++p)
	{
		if (*p != '%')
		{
			continue;
		}
		if (p[1] == '%')
		{
			++p;
			continue;
		}
		const char* q = p + 1;
		while (*q != '\0' && (*q == '-' || *q == '+' || *q == ' ' || *q == '#' || *q == '0' || (*q >= '1' && *q <= '9') || *q == '.'))
		{
			++q;
		}
		if (*q == 'd' || *q == 's' || *q == 'u')
		{
			if (n < cap)
			{
				out[n][0] = '%';
				out[n][1] = *q;
				out[n][2] = '\0';
			}
			++n;
		}
	}
	return n;
}

// Copies the template for the current language into pszOut. In English, or when the table has no
// entry, the English template is copied unchanged, so this never changes the English game.
void TranslateTemplate(const char* pszEnglish, char* pszOut, size_t cbOut)
{
	pszOut[0] = '\0';
	strncpy(pszOut, pszEnglish, cbOut - 1);
	pszOut[cbOut - 1] = '\0';

	if (!IsVietnameseLanguage())
	{
		return;
	}
	std::map<std::string, std::string>& table = VietnameseTemplates();
	std::map<std::string, std::string>::const_iterator it = table.find(pszEnglish);
	if (it == table.end())
	{
		return;
	}

	// Refuse a translation whose printf conversions differ from the English line: a mismatch would
	// read the wrong argument or crash. The English line is then shown.
	char eng[16][4], vi[16][4];
	const int nEng = CountPlaceholders(pszEnglish, eng, 16);
	const int nVi = CountPlaceholders(it->second.c_str(), vi, 16);
	if (nEng != nVi)
	{
		return;
	}
	for (int i = 0; i < nEng && i < 16; ++i)
	{
		if (strcmp(eng[i], vi[i]) != 0)
		{
			return;
		}
	}

	strncpy(pszOut, it->second.c_str(), cbOut - 1);
	pszOut[cbOut - 1] = '\0';
}

// ---------------------------------------------------------------------------------------------
// Text drawn by the UI. Every line the game draws goes through CUIRenderText::RenderText, which asks
// TranslateDrawnText first: in Vietnamese mode a plain English line that Data\Custom\Lang_Vtm.txt
// knows is drawn in Vietnamese, so a window does not need a call to TranslateTemplate per string.
// A line matches either exactly, or a template with %d / %s / %u ("Friends %d/%d" matches
// "Friends 3/10"). A Vietnamese template may use %1 %2 ... to take a captured value by position.
// Only pure ASCII text is looked at (Vietnamese text and player names with accents never are).
// ---------------------------------------------------------------------------------------------
#include <unordered_map>
#include <vector>
#include <mutex>

namespace
{
	struct ViPattern
	{
		std::vector<std::string> enLits;	// the pieces between the placeholders (placeholders + 1 of them)
		std::vector<char> kinds;			// 'd' for %d / %u, 's' for %s
		std::string vi;						// the Vietnamese template as written
	};

	bool SplitViTemplate(const std::string& t, std::vector<std::string>& lits, std::vector<char>& kinds)
	{
		lits.assign(1, std::string());
		kinds.clear();
		for (size_t i = 0; i < t.size(); ++i)
		{
			if (t[i] != '%')
			{
				lits.back() += t[i];
				continue;
			}
			if (i + 1 >= t.size())
			{
				lits.back() += '%';
				continue;
			}
			if (t[i + 1] == '%')
			{
				lits.back() += '%';
				++i;
				continue;
			}
			size_t j = i + 1;
			while (j < t.size() && strchr("-+ #0123456789.", t[j]) != NULL)
			{
				++j;
			}
			if (j < t.size() && (t[j] == 'd' || t[j] == 'u' || t[j] == 's'))
			{
				kinds.push_back(t[j] == 's' ? 's' : 'd');
				lits.push_back(std::string());
				i = j;
				continue;
			}
			return false;	// some other conversion: not a template this matcher handles
		}
		return true;
	}

	bool MatchViPattern(const ViPattern& p, const std::string& text, std::vector<std::string>& caps)
	{
		caps.clear();
		const std::string& first = p.enLits[0];
		if (text.compare(0, first.size(), first) != 0)
		{
			return false;
		}
		size_t pos = first.size();
		for (size_t k = 0; k < p.kinds.size(); ++k)
		{
			const std::string& next = p.enLits[k + 1];
			const bool last = (k + 1 == p.kinds.size());
			size_t end = pos;
			if (p.kinds[k] == 'd')
			{
				if (end < text.size() && (text[end] == '-' || text[end] == '+'))
				{
					++end;
				}
				const size_t digits = end;
				while (end < text.size() && isdigit((unsigned char)text[end]))
				{
					++end;
				}
				if (end == digits)
				{
					return false;
				}
			}
			else if (last)
			{
				// the last placeholder runs up to the closing literal, which must end the text
				if (text.size() < pos + next.size())
				{
					return false;
				}
				end = text.size() - next.size();
			}
			else
			{
				if (next.empty())
				{
					// Two placeholders in a row, as in "item%s%s": the first one is the plural
					// "s" or nothing, the second takes the rest.
					if (end < text.size() && text[end] == 's')
					{
						++end;
					}
					caps.push_back(text.substr(pos, end - pos));
					pos = end;
					continue;
				}
				const size_t f = text.find(next, pos);
				if (f == std::string::npos)
				{
					return false;
				}
				end = f;
			}
			caps.push_back(text.substr(pos, end - pos));
			pos = end;
			if (text.compare(pos, next.size(), next) != 0)
			{
				return false;
			}
			pos += next.size();
		}
		return pos == text.size();
	}

	bool ViExact(const std::string& text, std::string& out)
	{
		std::map<std::string, std::string>& table = VietnameseTemplates();
		const std::map<std::string, std::string>::const_iterator it = table.find(text);
		if (it == table.end() || it->second.find('%') != std::string::npos)
		{
			return false;
		}
		out = it->second;
		return true;
	}

	std::string BuildViText(const ViPattern& p, const std::vector<std::string>& caps)
	{
		std::string out;
		size_t seq = 0;
		for (size_t i = 0; i < p.vi.size(); ++i)
		{
			const char c = p.vi[i];
			if (c != '%' || i + 1 >= p.vi.size())
			{
				out += c;
				continue;
			}
			const char n = p.vi[i + 1];
			if (n == '%')
			{
				out += '%';
				++i;
				continue;
			}
			size_t index = std::string::npos;
			if (n >= '1' && n <= '9')
			{
				index = (size_t)(n - '1');
				++i;
			}
			else
			{
				size_t j = i + 1;
				while (j < p.vi.size() && strchr("-+ #0123456789.", p.vi[j]) != NULL)
				{
					++j;
				}
				if (j < p.vi.size() && (p.vi[j] == 'd' || p.vi[j] == 'u' || p.vi[j] == 's'))
				{
					index = seq++;
					i = j;
				}
				else
				{
					out += c;
					continue;
				}
			}
			if (index < caps.size())
			{
				std::string piece = caps[index];
				std::string vi;
				if (index < p.kinds.size() && p.kinds[index] == 's' && ViExact(piece, vi))
				{
					piece = vi;		// a captured name that is itself a known line, e.g. " + coins"
				}
				out += piece;
			}
		}
		return out;
	}

	std::vector<ViPattern>& ViPatterns()
	{
		static std::vector<ViPattern> s_patterns;
		static bool s_built = false;
		if (!s_built)
		{
			s_built = true;
			std::map<std::string, std::string>& table = VietnameseTemplates();
			for (std::map<std::string, std::string>::const_iterator it = table.begin(); it != table.end(); ++it)
			{
				ViPattern p;
				if (!SplitViTemplate(it->first, p.enLits, p.kinds) || p.kinds.empty())
				{
					continue;
				}
				size_t litLen = 0;
				for (size_t i = 0; i < p.enLits.size(); ++i)
				{
					litLen += p.enLits[i].size();
				}
				if (litLen < 3)
				{
					continue;
				}
				p.vi = it->second;
				s_patterns.push_back(p);
			}
		}
		return s_patterns;
	}
}

bool TranslateDrawnText(const char* pszText, char* pszOut, size_t cbOut)
{
	static int s_vi = -1;
	if (s_vi < 0)
	{
		if (g_strSelectedML.empty())
		{
			return false;
		}
		s_vi = IsVietnameseLanguage() ? 1 : 0;
	}
	if (s_vi == 0 || pszText == NULL || pszText[0] == '\0' || cbOut < 2)
	{
		return false;
	}

	size_t len = 0;
	for (const unsigned char* p = (const unsigned char*)pszText; *p != '\0'; ++p, ++len)
	{
		if (*p >= 0x80 || len > 600)
		{
			return false;
		}
	}

	static std::mutex s_lock;
	static std::unordered_map<std::string, std::string> s_cache;	// "" = no translation
	std::lock_guard<std::mutex> guard(s_lock);

	const std::string text(pszText);
	std::unordered_map<std::string, std::string>::const_iterator hit = s_cache.find(text);
	if (hit == s_cache.end())
	{
		std::string result;
		if (!ViExact(text, result))
		{
			result.clear();
			std::vector<ViPattern>& patterns = ViPatterns();
			std::vector<std::string> caps;
			for (size_t i = 0; i < patterns.size(); ++i)
			{
				if (MatchViPattern(patterns[i], text, caps))
				{
					result = BuildViText(patterns[i], caps);
					break;
				}
			}
		}
		if (s_cache.size() > 6000)
		{
			s_cache.clear();
		}
		hit = s_cache.insert(std::make_pair(text, result)).first;
	}

	if (hit->second.empty())
	{
		return false;
	}
	strncpy(pszOut, hit->second.c_str(), cbOut - 1);
	pszOut[cbOut - 1] = '\0';
	return true;
}

CMultiLanguage::CMultiLanguage(std::string strSelectedML)
{
	ms_Singleton = this;

	if (_stricmp(strSelectedML.c_str(), "ENG") == 0)
	{
		byLanguage = 0;
		iCodePage = 1252;
		iNumByteForOneCharUTF8 = 2;
	}
	else if(_stricmp(strSelectedML.c_str(), "POR") == 0)
	{	
		byLanguage = 1;
		iCodePage = 1252;
		iNumByteForOneCharUTF8 = 2;
	}
	else if(_stricmp(strSelectedML.c_str(), "SPN") == 0)
	{
		byLanguage = 2;
		iCodePage = 1252;
		iNumByteForOneCharUTF8 = 2;
	}
	else if(_stricmp(strSelectedML.c_str(), "VTM") == 0)
	{
		// Vietnamese. All Vietnamese data is authored as UTF-8 and ConvertCharToWideStr
		// takes the UTF-8 path whenever the bytes are valid UTF-8, so the code page is
		// only the fallback for non-UTF-8 input. A Vietnamese letter is up to 3 UTF-8
		// bytes, which sizes the chat and shop-title buffers. The server never reads
		// this byte (CGCharacterListRecv takes no packet body).
		byLanguage = 7;	// LANGUAGE_VIETNAMESE (Winmain.h)
		iCodePage = 1258;
		iNumByteForOneCharUTF8 = 3;
	}
	else
	{
		byLanguage = 0;
		iCodePage = 1252;
		iNumByteForOneCharUTF8 = 2;
	}
}

BYTE CMultiLanguage::GetLanguage()
{
	return byLanguage;
}

int CMultiLanguage::GetCodePage()
{
	return iCodePage;
}

int CMultiLanguage::GetNumByteForOneCharUTF8()
{
	return iNumByteForOneCharUTF8;
}

BOOL CMultiLanguage::IsCharUTF8(const char* pszText)
{
#if defined(__ANDROID__) || defined(MU_IOS)
    return mu_text::IsUtf8(pszText, pszText ? strlen(pszText) : 0) ? TRUE : FALSE;
#else
    if (pszText == NULL || strlen(pszText) <= 0)
        return TRUE;
    
    const char* pbyCurr = pszText;
	BOOL        bUTF8   = TRUE;
    
    while ( (*pbyCurr != 0x00) && bUTF8)
    {
        // U+0000 ~ U+007F (0 ~ 127)
        if ((*pbyCurr & 0x80) == 0x00)
        {
            pbyCurr++;
        }
        // U+0080 ~ U+07FF (128 ~ 2,047)
        else if (((*pbyCurr & 0xE0) == 0xC0) && ((*(pbyCurr+1) & 0xC0) == 0x80))
        {
            pbyCurr += 2;
        }
        // U+0800 ~ U+FFFF (2,048 ~ 65,535)
        else if (((*pbyCurr & 0xF0) == 0xE0) && ((*(pbyCurr+1) & 0xC0) == 0x80) && ((*(pbyCurr+2) & 0xC0) == 0x80))
        {
            pbyCurr += 3;
        }
        // U+10000 ~ U+10FFFF (65,536 ~ 1,114,111)
        else if (((*pbyCurr & 0xF8) == 0xF0) && 
            ((*(pbyCurr+1) & 0xC0) == 0x80) && ((*(pbyCurr+2) & 0xC0) == 0x80) && ((*(pbyCurr+3) & 0xC0) == 0x80))
        {
            pbyCurr += 4;
        }
        // not UTF-8
        else
        {
            bUTF8 = FALSE;
        }
    }
    return bUTF8;
#endif
}

int CMultiLanguage::ConvertCharToWideStr(std::wstring& wstrDest, LPCSTR lpString)
{
    wstrDest = L"";

    if (lpString == NULL || strlen(lpString) <= 0)
        return 0;

    int nLenOfWideCharStr;
	int iConversionType;

	iConversionType = (IsCharUTF8(lpString)) ? CP_UTF8 : iCodePage;

    // calculate the number of characters needed to hold the wide-character version of the string.
    nLenOfWideCharStr = MultiByteToWideChar(iConversionType, 0, lpString, -1, NULL, 0);
    if (nLenOfWideCharStr <= 0)
        return 0;
    std::vector<wchar_t> wide(nLenOfWideCharStr, L'\0');
    const int converted = MultiByteToWideChar(iConversionType, 0, lpString, -1,
        wide.data(), nLenOfWideCharStr);
    if (converted <= 0)
        return 0;
    wstrDest.assign(wide.data(), converted - 1);
    return converted - 1;
}

int CMultiLanguage::ConvertWideCharToStr(std::string& strDest, LPCWSTR lpwString, int iConversionType)
{
    strDest = "";

    if (lpwString == NULL || wcslen(lpwString) <= 0)
        return 0;

    int nLenOfWideCharStr;
    
    // calculate the number of characters needed to hold the wide-character version of the string.
    nLenOfWideCharStr = WideCharToMultiByte(iConversionType, 0, lpwString, -1, NULL, 0, 0, 0);
    // memory allocation
    char* pszStr = new char[nLenOfWideCharStr];
    
    // convert the multi-byte string to a wide-character string.
    WideCharToMultiByte(iConversionType, 0, lpwString, -1, pszStr, nLenOfWideCharStr, 0, 0);

    //assign
    strDest += pszStr;
    
    // release the allocated memory.
    delete[] pszStr;

    return nLenOfWideCharStr-1;
}

void CMultiLanguage::ConvertANSIToUTF8OrViceVersa(std::string& strDest, LPCSTR lpString)
{
	std::wstring wstrUTF16 = L"";
	int iDestType = (IsCharUTF8(lpString)) ? CP_ACP : CP_UTF8;

	ConvertCharToWideStr(wstrUTF16, lpString);
	ConvertWideCharToStr(strDest, wstrUTF16.c_str(), iDestType);
}

int	CMultiLanguage::GetClosestBlankFromCenter(const std::wstring wstrTarget)
{
	int iLength = wstrTarget.length();
	
	std::wstring wstrText1 = wstrTarget.substr(iLength/2, std::wstring::npos);
	std::wstring wstrText2 = wstrTarget.substr(0, iLength/2);
	
	int iPosLastBlankFromFirstHalf = wstrText2.find_last_of(L" ");
	int iPosFirstBlankFromSecondHalf = wstrText1.find_first_of(L" ", 1);
	int iClosestBlankFromCenter = 0;
	
	if (iPosLastBlankFromFirstHalf == std::wstring::npos && iPosFirstBlankFromSecondHalf == std::wstring::npos)
	{
		iClosestBlankFromCenter = iLength/2;
	}
	else if (iPosLastBlankFromFirstHalf == std::wstring::npos)
	{
		iClosestBlankFromCenter = iPosFirstBlankFromSecondHalf+iLength/2;
	}
	else if (iPosFirstBlankFromSecondHalf == std::wstring::npos)
	{
		iClosestBlankFromCenter = iPosLastBlankFromFirstHalf;
	}
	else if (iPosFirstBlankFromSecondHalf <= iLength/2-iPosLastBlankFromFirstHalf)
	{
		iClosestBlankFromCenter = iPosFirstBlankFromSecondHalf+iLength/2;
	}
	else
	{
		iClosestBlankFromCenter = iPosLastBlankFromFirstHalf;
	}
	
	return iClosestBlankFromCenter;
}

BOOL CMultiLanguage::_GetTextExtentPoint32(HDC hdc, LPCWSTR lpString, int cbString, LPSIZE lpSize)
{
	return GetTextExtentPoint32W(hdc, lpString, cbString, lpSize); 
}

BOOL CMultiLanguage::_GetTextExtentPoint32(HDC hdc, LPCSTR lpString, int cbString, LPSIZE lpSize)
{
	std::wstring wstrText = L"";
	ConvertCharToWideStr(wstrText, lpString);
	
	return GetTextExtentPoint32W(hdc, wstrText.c_str(), wstrText.length(), lpSize);
}

BOOL CMultiLanguage::_TextOut(HDC hdc, int nXStart, int nYStart, LPCWSTR lpString, int cbString)
{
	return TextOutW(hdc, nXStart, nYStart, lpString, cbString); 
}

BOOL CMultiLanguage::_TextOut(HDC hdc, int nXStart, int nYStart, LPCSTR lpString, int cbString)
{
	std::wstring wstrText = L"";
	ConvertCharToWideStr(wstrText, lpString);
	
	return TextOutW(hdc, nXStart, nYStart, wstrText.c_str(), wstrText.length()); 
}
	
WPARAM CMultiLanguage::ConvertFulltoHalfWidthChar(DWORD wParam)
{
	wchar_t Char = (wchar_t)(wParam);

	if (Char >= 0xFF01 && Char <= 0xFF5A) 
		wParam -= 0xFEE0;
	else if (Char == 0x3000)
		wParam = 0x0020;

	return wParam;
}