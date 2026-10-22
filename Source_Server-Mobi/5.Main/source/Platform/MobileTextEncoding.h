#pragma once

// Locale-independent text conversion shared by iOS and Android. wchar_t is
// UTF-32 on both platforms. Legacy western data uses CP1252; Vietnamese uses
// CP1258 only when the complete input is not valid UTF-8. Never repair or
// re-encode already-decoded strings heuristically.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <climits>

namespace mu_text {
inline bool NextUtf8(const char* text, size_t size, size_t& pos, uint32_t& cp)
{
    const auto* s = reinterpret_cast<const unsigned char*>(text);
    const unsigned char lead = s[pos];
    size_t count;
    uint32_t minimum;
    if (lead < 0x80) { cp = lead; ++pos; return true; }
    if (lead >= 0xC2 && lead <= 0xDF) { count = 2; cp = lead & 0x1F; minimum = 0x80; }
    else if (lead >= 0xE0 && lead <= 0xEF) { count = 3; cp = lead & 0x0F; minimum = 0x800; }
    else if (lead >= 0xF0 && lead <= 0xF4) { count = 4; cp = lead & 7; minimum = 0x10000; }
    else return false;
    if (size - pos < count) return false;
    for (size_t i = 1; i < count; ++i) {
        if ((s[pos + i] & 0xC0) != 0x80) return false;
        cp = (cp << 6) | (s[pos + i] & 0x3F);
    }
    if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
    pos += count;
    return true;
}

inline bool IsUtf8(const char* text, size_t size)
{
    if (!text) return size == 0;
    size_t pos = 0;
    uint32_t cp;
    while (pos < size) if (!NextUtf8(text, size, pos, cp)) return false;
    return true;
}

inline uint32_t LegacyCodePoint(unsigned char c, unsigned codePage)
{
    static const uint16_t western[32] = {
        0x20AC,0x81,0x201A,0x192,0x201E,0x2026,0x2020,0x2021,
        0x2C6,0x2030,0x160,0x2039,0x152,0x8D,0x17D,0x8F,
        0x90,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
        0x2DC,0x2122,0x161,0x203A,0x153,0x9D,0x17E,0x178
    };
    if (codePage == 1258) {
        switch (c) {
        case 0x8A: case 0x8E: case 0x9A: case 0x9E: return c;
        case 0xC3: return 0x102; case 0xCC: return 0x300;
        case 0xD0: return 0x110; case 0xD2: return 0x309;
        case 0xD5: return 0x1A0; case 0xDD: return 0x1AF;
        case 0xDE: return 0x303; case 0xE3: return 0x103;
        case 0xEC: return 0x301; case 0xF0: return 0x111;
        case 0xF2: return 0x323; case 0xF5: return 0x1A1;
        case 0xFD: return 0x1B0; case 0xFE: return 0x20AB;
        }
    }
    return c >= 0x80 && c < 0xA0 ? western[c - 0x80] : c;
}

// Win32 lengths: -1 includes NUL, positive lengths are exact (may contain NUL).
// Query and write return identical counts; insufficient capacity writes nothing.
// MB_ERR_INVALID_CHARS (8) rejects invalid UTF-8; otherwise replace each bad byte.
inline int MultiByteToWide(unsigned codePage, unsigned flags, const char* src,
                           int srcLen, wchar_t* dst, int capacity)
{
    if (!src || srcLen == 0 || srcLen < -1 || capacity < 0 || (!dst && capacity)) return 0;
    if (codePage != 65001 && codePage != 0 && codePage != 1252 && codePage != 1258) return 0;
    const size_t size = srcLen == -1 ? std::strlen(src) + 1 : static_cast<size_t>(srcLen);
    std::wstring out;
    for (size_t pos = 0; pos < size;) {
        uint32_t cp;
        if (codePage == 65001) {
            if (!NextUtf8(src, size, pos, cp)) {
                if (flags & 8) return 0;
                cp = 0xFFFD;
                ++pos;
            }
        } else cp = LegacyCodePoint(static_cast<unsigned char>(src[pos++]), codePage);
        out.push_back(static_cast<wchar_t>(cp));
    }
    if (out.size() > INT_MAX) return 0;
    if (!capacity) return static_cast<int>(out.size());
    if (static_cast<size_t>(capacity) < out.size()) return 0;
    out.copy(dst, out.size());
    return static_cast<int>(out.size());
}

inline std::wstring DecodeText(const char* text, int length, unsigned fallback = 1252)
{
    if (!text || length <= 0) return {};
    const unsigned cp = IsUtf8(text, static_cast<size_t>(length)) ? 65001 : fallback;
    const int count = MultiByteToWide(cp, 0, text, length, nullptr, 0);
    std::wstring result(static_cast<size_t>(count), L'\0');
    if (count) MultiByteToWide(cp, 0, text, length, &result[0], count);
    return result;
}

inline std::vector<uint16_t> ToUtf16(const wchar_t* text, int length)
{
    std::vector<uint16_t> out;
    for (int i = 0; text && i < length && text[i]; ++i) {
        uint32_t cp = static_cast<uint32_t>(text[i]);
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
        if (cp > 0xFFFF) {
            cp -= 0x10000;
            out.push_back(static_cast<uint16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<uint16_t>(0xDC00 + (cp & 0x3FF)));
        } else out.push_back(static_cast<uint16_t>(cp));
    }
    out.push_back(0);
    return out;
}
} // namespace mu_text
