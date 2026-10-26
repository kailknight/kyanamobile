#!/usr/bin/env python3
"""Host tests of actual conversion and GDI adapters; no SDL/device UI needed.
Run: python3 tests/test_mobile_text_encoding.py
Uses the existing unittest + compiled C++ harness pattern of test_mobile_language.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "5.Main/source"


def function(text, signature):
    start = text.index(signature)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


class MobileTextEncodingTest(unittest.TestCase):
    def test_conversion_and_gdi_adapters(self):
        platform = (SOURCE / "Platform/PlatformDefs.h").read_text()
        language = (SOURCE / "MultiLanguage.cpp").read_text()
        gdi = (SOURCE / "Platform/AndroidGDI.cpp").read_text()
        actual = "\n".join(function(text, signature) for text, signature in [
            (platform, "inline int MultiByteToWideChar("),
            (language, "BOOL CMultiLanguage::IsCharUTF8("),
            (language, "int CMultiLanguage::ConvertCharToWideStr("),
            (gdi, "static std::vector<Uint16> ToUint16("),
            (platform, "inline BOOL GetTextExtentPoint32W("),
            (platform, "inline BOOL TextOutW("),
            (language, "BOOL CMultiLanguage::_GetTextExtentPoint32(HDC hdc, LPCSTR"),
            (language, "BOOL CMultiLanguage::_TextOut(HDC hdc, int nXStart, int nYStart, LPCSTR"),
            (platform, "inline BOOL GetTextExtentPoint32(HDC hdc, LPCSTR"),
            (platform, "inline BOOL    TextOut(HDC hdc, int x, int y, LPCWSTR"),
            (platform, "inline BOOL    TextOut(HDC hdc, int x, int y, LPCSTR"),
        ])
        harness = r'''
#include "MobileTextEncoding.h"
#include <cassert>
#include <clocale>
using UINT = unsigned; using DWORD = unsigned; using BOOL = int;
using LPCSTR = const char*; using LPWSTR = wchar_t*; using LPCWSTR = const wchar_t*;
using Uint16 = uint16_t; using HDC = void*;
struct SIZE { int cx, cy; }; using LPSIZE = SIZE*;
#define TRUE 1
#define FALSE 0
#define CP_UTF8 65001
class CMultiLanguage {
public:
    int iCodePage = 1258;
    BOOL IsCharUTF8(const char*);
    int ConvertCharToWideStr(std::wstring&, LPCSTR);
    BOOL _GetTextExtentPoint32(HDC, LPCSTR, int, LPSIZE);
    BOOL _TextOut(HDC, int, int, LPCSTR, int);
};
static std::wstring measured, drawn;
bool AndroidGetTextExtentPoint32(HDC, LPCWSTR t, int n, int* w, int* h) {
    measured.assign(t, n); *w = n; *h = 10; return true;
}
bool AndroidTextOut(HDC, int, int, LPCWSTR t, int n) {
    drawn.assign(t, n); return true;
}
ACTUAL
int main() {
    assert(setlocale(LC_ALL, "C")); // No UTF-8 process locale required.
    CMultiLanguage ml;
    for (const auto& pair : std::vector<std::pair<std::string, std::wstring>>{
        {u8"Đấu sĩ", L"Đấu sĩ"}, {u8"Chiến Binh 1", L"Chiến Binh 1"},
        {u8"Thiên Sứ", L"Thiên Sứ"}, {u8"(Đã bị xóa)", L"(Đã bị xóa)"},
        {"English 123", L"English 123"}, {u8"한국어", L"한국어"},
        {u8"😀", L"\U0001F600"}}) {
        std::wstring wide;
        assert(ml.IsCharUTF8(pair.first.c_str()));
        assert(ml.ConvertCharToWideStr(wide, pair.first.c_str()) == pair.second.size());
        assert(wide == pair.second);
        SIZE size;
        ml._GetTextExtentPoint32(nullptr, pair.first.c_str(), pair.first.size(), &size);
        ml._TextOut(nullptr, 0, 0, pair.first.c_str(), pair.first.size());
        assert(drawn == pair.second && measured == drawn);
        GetTextExtentPoint32(nullptr, pair.first.c_str(), pair.first.size(), &size);
        TextOut(nullptr, 0, 0, pair.first.c_str(), pair.first.size());
        assert(drawn == pair.second && measured == drawn);
        assert(ToUint16(wide.c_str(), wide.size()).back() == 0);
    }
    assert(ToUint16(L"\U0001F600", 1) == std::vector<uint16_t>({0xD83D,0xDE00,0}));
    wchar_t invalidScalar[] = {0xD800, 0x110000, -1, 0};
    assert(ToUint16(invalidScalar, 3) == std::vector<uint16_t>({0xFFFD,0xFFFD,0xFFFD,0}));
    std::wstring wide;
    const char legacy[] = "\xD0\xE2\xF2u si\xDE";
    assert(!ml.IsCharUTF8(legacy));
    ml.ConvertCharToWideStr(wide, legacy);
    assert(wide == L"Đâ\u0323u si\u0303"); // CP1258 decomposed tones.
    ml.iCodePage = 1252;
    ml.ConvertCharToWideStr(wide, "Caf\xE9 \x80");
    assert(wide == L"Café €");
    // Korean UTF-8 works. No CP949 decoder existed in the mobile shim.
    ml.iCodePage = 949;
    assert(ml.ConvertCharToWideStr(wide, "\xC7\xD1\xB1\xDB") == 0 && wide.empty());
    for (const std::string bad : {std::string("\xC0\xAF"), std::string("\xED\xA0\x80"),
        std::string("\xF4\x90\x80\x80"), std::string("\xE1\xBA"),
        std::string("\xF0"), std::string("\x80"), std::string("\xE2(\xA1")}) {
        assert(!ml.IsCharUTF8(bad.c_str()));
        assert(MultiByteToWideChar(65001, 8, bad.data(), bad.size(), nullptr, 0) == 0);
        int count = MultiByteToWideChar(65001, 0, bad.data(), bad.size(), nullptr, 0);
        std::vector<wchar_t> out(count);
        assert(MultiByteToWideChar(65001, 0, bad.data(), bad.size(), out.data(), count) == count);
        assert(out[0] == 0xFFFD);
    }
MORE
}
'''.replace("ACTUAL", actual)
        with tempfile.TemporaryDirectory(prefix="mu-text-test-") as temp:
            path = Path(temp)
            (path / "main.cpp").write_text(harness.replace("MORE", EXTRA))
            binary = path / "text-test"
            subprocess.run([
                os.environ.get("CXX", "clang++"), "-std=c++17", "-DMU_IOS=1",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
                "-I", str(SOURCE / "Platform"), str(path / "main.cpp"),
                "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)


EXTRA = r'''
    wchar_t out[8] = {42,42,42,42,42,42,42,42};
    assert(MultiByteToWideChar(65001, 0, u8"Đấu", -1, nullptr, 0) == 4);
    assert(MultiByteToWideChar(65001, 0, u8"Đấu", -1, out, 3) == 0 && out[0] == 42);
    assert(MultiByteToWideChar(65001, 0, u8"Đấu", -1, out, 4) == 4 && out[3] == 0);
    const char bounded[] = {'A', 0, 'B'};
    assert(MultiByteToWideChar(65001, 0, bounded, 3, out, 3) == 3);
    assert(out[0] == 'A' && out[1] == 0 && out[2] == 'B');
    const char truncated[] = {char(0xE1)}; // No NUL; ASan checks bounds.
    assert(!mu_text::IsUtf8(truncated, 1));
    assert(MultiByteToWideChar(65001, 8, truncated, 1, out, 8) == 0);
    assert(MultiByteToWideChar(65001, 0, nullptr, -1, out, 8) == 0);
    assert(MultiByteToWideChar(65001, 0, "", -1, out, 8) == 1 && out[0] == 0);
    assert(MultiByteToWideChar(65001, 0, "a", 0, out, 8) == 0);
    SIZE size;
    std::string longText(3000, 'x'); longText += u8"Đ";
    TextOut(nullptr, 0, 0, longText.c_str(), longText.size());
    GetTextExtentPoint32(nullptr, longText.c_str(), longText.size(), &size);
    assert(drawn == measured && drawn.size() == 3001);
    TextOut(nullptr, 0, 0, "ABCtail", 3);
    GetTextExtentPoint32(nullptr, "ABCtail", 3, &size);
    assert(drawn == L"ABC" && drawn == measured);
'''

if __name__ == "__main__":
    unittest.main()
