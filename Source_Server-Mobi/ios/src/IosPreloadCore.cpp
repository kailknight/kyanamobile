// Port of the extraction/install half of android/.../PreloadActivity.java.
// Method names follow the Java so the two can be read side by side.

#include "IosPreloadCore.h"

#include "miniz.h"

#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace mu_preload
{
const char* const kDataFolderName = "Data";
const char* const kReadyMarkerFile = ".mu_data_ready_v1";

namespace
{
const char* const kDataDirHints[] = { "Local", "Item", "Skill", "World1", "Object1" };

const char* const kRequiredFiles[] = {
    "Local/Mix.bmd",
    "Local/Item.bmd",
    "Local/Filter.bmd",
    "Local/FilterName.bmd",
    "Local/MonsterSkill.bmd",
    "Local/MasterSkillTreeData.bmd",
    "Local/Eng/Dialog_Eng.bmd",
    "Local/Eng/Item_Eng.bmd",
    "Local/Eng/itemtooltip_Eng.bmd",
    "Local/Eng/itemleveltooltip_Eng.bmd",
    "Local/Eng/itemtooltiptext_Eng.bmd",
    "Local/Eng/movereq_Eng.bmd",
    "Local/Eng/NpcName(Eng).txt",
    "Local/Eng/Quest_Eng.bmd",
    "Local/Eng/Skill_Eng.bmd",
    "Local/Eng/SocketItem_Eng.bmd",
    "Local/Eng/MasterSkillTooltip_Eng.bmd",
    "Local/Eng/Text_Eng.bmd",
    "Item/bsummoners01.ozj",
    "Item/bsummoners02_render.OZJ",
    "Item/bsummoners02.ozj",
    "Item/sd_soulsummoner.bmd",
    "Item/sd_soulsummonerstickA01.ozj",
    "Item/sd_soulsummonerstickA02_render.ozj",
    "Item/sd_soulsummonerstickA02.ozj",
    "Gate.bmd",
    "Macro.txt",
};

const char* const kRequiredDirs[] = {
    "Custom", "Effect", "Interface", "Item", "Local/Eng",
    "Monster", "Object1", "Player", "Skill", "World1",
};

bool EqualsIgnoreCase(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
        {
            return false;
        }
    }
    return true;
}

std::string ToLowerAscii(std::string s)
{
    for (char& c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::vector<std::string> SplitPath(const std::string& path)
{
    std::vector<std::string> parts;
    std::string current;
    for (char c : path)
    {
        if (c == '/')
        {
            parts.push_back(current);
            current.clear();
        }
        else
        {
            current.push_back(c);
        }
    }
    parts.push_back(current);
    return parts;
}

// normalizeZipPath: backslashes to '/', no leading '/', no "." segments, and
// any ".." makes the whole name unusable (returns "").
std::string NormalizeZipPath(const std::string& entryName)
{
    std::string path = entryName;
    for (char& c : path)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }

    std::string result;
    for (const std::string& part : SplitPath(path))
    {
        if (part.empty() || part == ".")
        {
            continue;
        }
        if (part == "..")
        {
            return {};
        }
        if (!result.empty())
        {
            result.push_back('/');
        }
        result += part;
    }
    return result;
}

bool IsDirectory(const std::string& path)
{
    std::error_code ec;
    return fs::is_directory(path, ec);
}

bool IsNonEmptyFile(const std::string& path)
{
    std::error_code ec;
    return fs::is_regular_file(path, ec) && fs::file_size(path, ec) > 0 && !ec;
}

std::string FindPathIgnoreCase(const std::string& root, const std::string& relativePath)
{
    const std::string normalized = NormalizeZipPath(relativePath);
    if (root.empty() || normalized.empty())
    {
        return {};
    }

    std::string current = root;
    for (const std::string& segment : SplitPath(normalized))
    {
        current = FindChildIgnoreCase(current, segment, false);
        if (current.empty())
        {
            return {};
        }
    }
    return current;
}

// The archive is written by Windows tools. Java opened it with ISO-8859-1 so
// that the one CP949 name in it (Data/Object34/Object01_<B0 A6>.bmd) cannot
// fail to decode; entries flagged as UTF-8 (general purpose bit 11) are UTF-8
// either way. Decoding the same way gives the same on-disk names as Android.
std::string DecodeEntryName(const char* raw, bool utf8Flag)
{
    if (utf8Flag)
    {
        return raw;
    }

    std::string out;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(raw); *p; ++p)
    {
        if (*p < 0x80)
        {
            out.push_back(static_cast<char>(*p));
        }
        else
        {
            out.push_back(static_cast<char>(0xC0 | (*p >> 6)));
            out.push_back(static_cast<char>(0x80 | (*p & 0x3F)));
        }
    }
    return out;
}

// resolvePathCaseInsensitive, with a directory listing cache: the Java lists
// every parent directory again for every entry, which is quadratic over an
// archive this size.
class CaseInsensitiveResolver
{
public:
    std::string Resolve(const std::string& root, const std::string& normalizedPath, bool asDirectory)
    {
        const std::vector<std::string> segments = SplitPath(normalizedPath);
        std::string current = root;
        for (size_t i = 0; i < segments.size(); ++i)
        {
            const bool last = (i + 1 == segments.size());
            std::unordered_map<std::string, std::string>& children = Listing(current);
            const std::string key = ToLowerAscii(segments[i]);

            auto it = children.find(key);
            const std::string name = (it != children.end()) ? it->second : segments[i];
            if (it == children.end())
            {
                children.emplace(key, name);
            }

            const std::string resolved = JoinPath(current, name);
            if (!last || asDirectory)
            {
                if (!EnsureDirectory(resolved))
                {
                    return {};
                }
            }
            current = resolved;
        }
        return current;
    }

private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_listings;

    std::unordered_map<std::string, std::string>& Listing(const std::string& dir)
    {
        auto it = m_listings.find(dir);
        if (it != m_listings.end())
        {
            return it->second;
        }

        std::unordered_map<std::string, std::string> children;
        std::error_code ec;
        for (fs::directory_iterator entry(dir, ec), end; !ec && entry != end; entry.increment(ec))
        {
            const std::string name = entry->path().filename().string();
            children.emplace(ToLowerAscii(name), name);
        }
        return m_listings.emplace(dir, std::move(children)).first->second;
    }
};

struct ExtractSink
{
    FILE* file;
    ExtractProgress* progress;
    const std::atomic<bool>* cancel;
};

size_t WriteExtractedChunk(void* opaque, mz_uint64, const void* buffer, size_t n)
{
    ExtractSink* sink = static_cast<ExtractSink*>(opaque);
    if (sink->cancel->load())
    {
        return 0; // makes miniz abort the entry
    }
    const size_t written = fwrite(buffer, 1, n, sink->file);
    sink->progress->bytes.fetch_add(static_cast<long long>(written));
    return written;
}
} // namespace

std::string JoinPath(const std::string& a, const std::string& b)
{
    if (a.empty())
    {
        return b;
    }
    return (a.back() == '/') ? (a + b) : (a + "/" + b);
}

bool EnsureDirectory(const std::string& dir)
{
    std::error_code ec;
    if (fs::is_directory(dir, ec))
    {
        return true;
    }
    fs::create_directories(dir, ec);
    return fs::is_directory(dir, ec);
}

void DeleteRecursively(const std::string& path)
{
    std::error_code ec;
    fs::remove_all(path, ec);
}

std::string FindChildIgnoreCase(const std::string& parent, const std::string& name, bool directoriesOnly)
{
    if (parent.empty() || name.empty())
    {
        return {};
    }

    // An exact-case match wins: iOS is case-sensitive, so "Data" and "data"
    // can both exist and must not be confused.
    std::error_code ec;
    const fs::path exact = fs::path(parent) / name;
    if (directoriesOnly ? fs::is_directory(exact, ec) : fs::exists(exact, ec))
    {
        return exact.string();
    }

    for (fs::directory_iterator entry(parent, ec), end; !ec && entry != end; entry.increment(ec))
    {
        if (directoriesOnly && !entry->is_directory(ec))
        {
            continue;
        }
        if (EqualsIgnoreCase(entry->path().filename().string(), name))
        {
            return entry->path().string();
        }
    }
    return {};
}

std::vector<std::string> FindMissingDataEntries(const std::string& dataDir)
{
    std::vector<std::string> missing;
    for (const char* dir : kRequiredDirs)
    {
        const std::string found = FindPathIgnoreCase(dataDir, dir);
        if (found.empty() || !IsDirectory(found))
        {
            missing.push_back(std::string(dir) + "/");
        }
    }
    for (const char* file : kRequiredFiles)
    {
        const std::string found = FindPathIgnoreCase(dataDir, file);
        if (found.empty() || !IsNonEmptyFile(found))
        {
            missing.push_back(file);
        }
    }
    return missing;
}

std::string JoinMissingEntries(const std::vector<std::string>& missing)
{
    const size_t limit = std::min<size_t>(8, missing.size());
    std::string joined;
    for (size_t i = 0; i < limit; ++i)
    {
        if (i > 0)
        {
            joined += ", ";
        }
        joined += missing[i];
    }
    if (missing.size() > limit)
    {
        joined += " ... +" + std::to_string(missing.size() - limit) + " more";
    }
    return joined;
}

bool IsDataFolderUsable(const std::string& dataDir)
{
    if (dataDir.empty() || !IsDirectory(dataDir))
    {
        return false;
    }

    const std::vector<std::string> missing = FindMissingDataEntries(dataDir);
    if (!missing.empty())
    {
        fprintf(stderr, "[MuPreload] data incomplete at %s, missing: %s\n",
            dataDir.c_str(), JoinMissingEntries(missing).c_str());
        return false;
    }

    int hintMatches = 0;
    for (const char* hint : kDataDirHints)
    {
        if (!FindChildIgnoreCase(dataDir, hint, true).empty())
        {
            ++hintMatches;
        }
    }
    if (hintMatches >= 2)
    {
        return true;
    }

    // quickScanDataFolder(dir, 3, 64): at least 10 files and 256 KB.
    int fileCount = 0;
    long long totalBytes = 0;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(dataDir, ec), end; !ec && it != end && fileCount < 64; it.increment(ec))
    {
        if (it.depth() > 3)
        {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file(ec))
        {
            const auto size = it->file_size(ec);
            if (!ec && size > 0)
            {
                ++fileCount;
                totalBytes += static_cast<long long>(size);
            }
        }
    }
    return fileCount >= 10 && totalBytes >= 256LL * 1024LL;
}

std::string ReadStoredSignature(const std::string& root)
{
    std::ifstream marker(JoinPath(root, kReadyMarkerFile));
    std::string line;
    while (std::getline(marker, line))
    {
        static const std::string kPrefix = "signature=";
        if (line.compare(0, kPrefix.size(), kPrefix) == 0)
        {
            std::string value = line.substr(kPrefix.size());
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
            {
                value.pop_back();
            }
            return value;
        }
    }
    return {};
}

void WriteReadyMarker(const std::string& root, const std::string& dataDir, const std::string& signature)
{
    std::ofstream marker(JoinPath(root, kReadyMarkerFile), std::ios::trunc);
    marker << "ready_at=" << static_cast<long long>(std::time(nullptr)) * 1000LL << "\n"
           << "path=" << dataDir << "\n"
           << "signature=" << signature << "\n";
}

bool InspectZip(const std::string& zipPath, ExtractProgress& progress, std::string* error)
{
    mz_zip_archive zip {};
    if (!mz_zip_reader_init_file_v2(&zip, zipPath.c_str(), 0, 0, 0))
    {
        if (error) *error = std::string("cannot open data.zip: ") + mz_zip_get_error_string(mz_zip_get_last_error(&zip));
        return false;
    }

    long long totalBytes = 0;
    int fileCount = 0;
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i)
    {
        mz_zip_archive_file_stat stat {};
        if (!mz_zip_reader_file_stat(&zip, i, &stat) || stat.m_is_directory)
        {
            continue;
        }
        ++fileCount;
        totalBytes += static_cast<long long>(stat.m_uncomp_size);
    }
    mz_zip_reader_end(&zip);

    progress.totalBytes = totalBytes;
    progress.fileCount = fileCount;
    return true;
}

bool ExtractZip(const std::string& zipPath, const std::string& extractRoot, ExtractProgress& progress,
                const std::atomic<bool>& cancel, std::string* error)
{
    mz_zip_archive zip {};
    if (!mz_zip_reader_init_file_v2(&zip, zipPath.c_str(), 0, 0, 0))
    {
        if (error) *error = std::string("cannot open data.zip: ") + mz_zip_get_error_string(mz_zip_get_last_error(&zip));
        return false;
    }

    CaseInsensitiveResolver resolver;
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    bool ok = true;

    for (mz_uint i = 0; i < count && ok; ++i)
    {
        if (cancel.load())
        {
            if (error) *error = "Preload cancelled";
            ok = false;
            break;
        }

        mz_zip_archive_file_stat stat {};
        if (!mz_zip_reader_file_stat(&zip, i, &stat))
        {
            continue;
        }

        const bool utf8Name = (stat.m_bit_flag & (1u << 11)) != 0;
        const std::string name = NormalizeZipPath(DecodeEntryName(stat.m_filename, utf8Name));
        if (name.empty())
        {
            continue;
        }

        // NormalizeZipPath already refuses "..", so every resolved path stays
        // under extractRoot (Java's assertUnderRoot).
        const bool isDirectory = stat.m_is_directory || name.back() == '/';
        const std::string outPath = resolver.Resolve(extractRoot, name, isDirectory);
        if (outPath.empty())
        {
            if (error) *error = "cannot create folder for " + name;
            ok = false;
            break;
        }
        if (isDirectory)
        {
            continue;
        }

        FILE* out = fopen(outPath.c_str(), "wb");
        if (out == nullptr)
        {
            if (error) *error = "cannot write " + outPath;
            ok = false;
            break;
        }

        ExtractSink sink { out, &progress, &cancel };
        const bool extracted = mz_zip_reader_extract_to_callback(&zip, i, WriteExtractedChunk, &sink, 0) != 0;
        const bool closed = fclose(out) == 0;
        if (!extracted || !closed)
        {
            if (error)
            {
                *error = cancel.load() ? std::string("Preload cancelled")
                                       : ("failed to extract " + name + ": "
                                          + mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
            }
            ok = false;
            break;
        }
        progress.files.fetch_add(1);
    }

    mz_zip_reader_end(&zip);
    return ok;
}

bool InstallExtractedData(const std::string& extractRoot, const std::string& root, std::string* error)
{
    std::string extractedData = FindChildIgnoreCase(extractRoot, kDataFolderName, true);
    if (extractedData.empty())
    {
        std::vector<std::string> directories;
        std::error_code ec;
        for (fs::directory_iterator entry(extractRoot, ec), end; !ec && entry != end; entry.increment(ec))
        {
            if (entry->is_directory(ec))
            {
                directories.push_back(entry->path().string());
            }
        }
        if (directories.size() == 1)
        {
            extractedData = directories[0];
        }
    }
    if (extractedData.empty())
    {
        if (error) *error = "data folder not found in downloaded data.zip";
        return false;
    }

    const std::string existing = FindChildIgnoreCase(root, kDataFolderName, true);
    const std::string target = existing.empty() ? JoinPath(root, kDataFolderName) : existing;

    DeleteRecursively(target);
    std::error_code ec;
    fs::rename(extractedData, target, ec);
    if (ec)
    {
        // Merge-copy fallback, as on Android when the old folder resists deletion.
        ec.clear();
        fs::copy(extractedData, target,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        DeleteRecursively(extractedData);
        if (ec)
        {
            if (error) *error = "cannot install Data folder: " + ec.message();
            return false;
        }
    }

    const std::vector<std::string> missing = FindMissingDataEntries(target);
    if (!missing.empty())
    {
        if (error) *error = "downloaded data incomplete, missing: " + JoinMissingEntries(missing);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Incremental update
// ---------------------------------------------------------------------------
const char* const kDataIndexFile = ".mu_data_index_v1";

namespace
{
constexpr uint32_t kZipLocalHeaderSig = 0x04034b50u;
constexpr uint32_t kZipCentralSig = 0x02014b50u;
constexpr uint32_t kZipEndSig = 0x06054b50u;
constexpr uint32_t kZip64EndSig = 0x06064b50u;
constexpr uint32_t kZip64LocatorSig = 0x07064b50u;
constexpr size_t kIoBufferBytes = 256 * 1024;

uint32_t Le16(const uint8_t* b) { return b[0] | (b[1] << 8); }
uint32_t Le32(const uint8_t* b) { return Le16(b) | (Le16(b + 2) << 16); }
uint64_t Le64(const uint8_t* b) { return Le32(b) | (static_cast<uint64_t>(Le32(b + 4)) << 32); }

// assignDataRelativePaths: a full install keeps only the archive's Data folder
// (InstallExtractedData), or its single top-level folder when there is none.
// Entries get their path inside that folder; anything outside it gets "".
bool AssignDataRelativePaths(std::vector<RemoteZipEntry>& entries, std::string* error)
{
    std::string top;
    std::string onlyTop;
    bool severalTops = false;

    for (const RemoteZipEntry& entry : entries)
    {
        const std::string normalized = NormalizeZipPath(entry.name);
        if (normalized.empty())
        {
            continue;
        }
        const size_t slash = normalized.find('/');
        std::string first;
        if (slash != std::string::npos)
        {
            first = normalized.substr(0, slash);
        }
        else if (entry.isDirectory)
        {
            first = normalized;
        }
        else
        {
            continue;
        }
        if (EqualsIgnoreCase(first, kDataFolderName))
        {
            top = kDataFolderName;
        }
        if (onlyTop.empty())
        {
            onlyTop = first;
        }
        else if (!EqualsIgnoreCase(onlyTop, first))
        {
            severalTops = true;
        }
    }

    if (top.empty())
    {
        if (onlyTop.empty() || severalTops)
        {
            if (error) *error = "data folder not found in data.zip";
            return false;
        }
        top = onlyTop;
    }

    for (RemoteZipEntry& entry : entries)
    {
        const std::string normalized = NormalizeZipPath(entry.name);
        const size_t slash = normalized.find('/');
        entry.relPath.clear();
        entry.key.clear();
        if (slash == std::string::npos || slash == 0 || !EqualsIgnoreCase(normalized.substr(0, slash), top))
        {
            continue;
        }
        entry.relPath = normalized.substr(slash + 1);
        entry.key = ToLowerAscii(entry.relPath);
    }
    return true;
}

bool IsIndexed(const RemoteZipEntry& entry)
{
    return !entry.key.empty() && !entry.isDirectory;
}
} // namespace

bool ParseZipEnd(const std::vector<uint8_t>& tail, ZipEndInfo* out, std::string* error)
{
    if (tail.size() < 22)
    {
        if (error) *error = "zip end record not found";
        return false;
    }
    long end = -1;
    for (long i = static_cast<long>(tail.size()) - 22; i >= 0; --i)
    {
        if (Le32(&tail[i]) == kZipEndSig)
        {
            end = i;
            break;
        }
    }
    if (end < 0)
    {
        if (error) *error = "zip end record not found";
        return false;
    }

    out->entryCount = Le16(&tail[end + 10]);
    out->centralSize = Le32(&tail[end + 12]);
    out->centralOffset = Le32(&tail[end + 16]);
    out->needsZip64 = out->entryCount == 0xFFFF || out->centralSize == 0xFFFFFFFFu
        || out->centralOffset == 0xFFFFFFFFu;
    if (out->needsZip64)
    {
        if (end < 20 || Le32(&tail[end - 20]) != kZip64LocatorSig)
        {
            if (error) *error = "zip64 locator not found";
            return false;
        }
        out->zip64EndOffset = Le64(&tail[end - 20 + 8]);
    }
    return true;
}

bool ParseZip64End(const std::vector<uint8_t>& record, ZipEndInfo* out, std::string* error)
{
    if (record.size() < 56 || Le32(record.data()) != kZip64EndSig)
    {
        if (error) *error = "zip64 end record not found";
        return false;
    }
    out->entryCount = Le64(&record[32]);
    out->centralSize = Le64(&record[40]);
    out->centralOffset = Le64(&record[48]);
    out->needsZip64 = false;
    return true;
}

bool ParseCentralDirectory(const std::vector<uint8_t>& central, uint64_t centralOffset, uint64_t entryCount,
                           std::vector<RemoteZipEntry>* entries, std::string* error)
{
    entries->clear();
    size_t p = 0;
    while (p + 46 <= central.size() && Le32(&central[p]) == kZipCentralSig)
    {
        RemoteZipEntry entry;
        entry.flags = static_cast<int>(Le16(&central[p + 8]));
        entry.method = static_cast<int>(Le16(&central[p + 10]));
        entry.crc = Le32(&central[p + 16]);
        entry.compressedSize = Le32(&central[p + 20]);
        entry.size = Le32(&central[p + 24]);
        const size_t nameLength = Le16(&central[p + 28]);
        const size_t extraLength = Le16(&central[p + 30]);
        const size_t commentLength = Le16(&central[p + 32]);
        entry.localOffset = Le32(&central[p + 42]);

        const size_t nameStart = p + 46;
        const size_t extraEnd = nameStart + nameLength + extraLength;
        if (extraEnd + commentLength > central.size())
        {
            if (error) *error = "zip central directory truncated";
            return false;
        }

        // Same decoding ExtractZip uses, so a file lands at the path a full
        // install would give it.
        const std::string raw(reinterpret_cast<const char*>(&central[nameStart]), nameLength);
        entry.name = DecodeEntryName(raw.c_str(), (entry.flags & 0x800) != 0);

        // Zip64 extra field: 64-bit values for whichever fields are maxed out.
        size_t e = nameStart + nameLength;
        while (e + 4 <= extraEnd)
        {
            const uint32_t id = Le16(&central[e]);
            const size_t size = Le16(&central[e + 2]);
            size_t d = e + 4;
            const size_t dataEnd = std::min(extraEnd, d + size);
            if (id == 0x0001)
            {
                if (entry.size == 0xFFFFFFFFu && d + 8 <= dataEnd)
                {
                    entry.size = Le64(&central[d]);
                    d += 8;
                }
                if (entry.compressedSize == 0xFFFFFFFFu && d + 8 <= dataEnd)
                {
                    entry.compressedSize = Le64(&central[d]);
                    d += 8;
                }
                if (entry.localOffset == 0xFFFFFFFFu && d + 8 <= dataEnd)
                {
                    entry.localOffset = Le64(&central[d]);
                }
            }
            e += 4 + size;
        }

        entry.isDirectory = !entry.name.empty() && (entry.name.back() == '/' || entry.name.back() == '\\');
        entries->push_back(std::move(entry));
        p = extraEnd + commentLength;
    }

    if (entries->empty())
    {
        if (error) *error = "zip central directory empty";
        return false;
    }
    if (entries->size() != entryCount && entryCount != 0xFFFF)
    {
        fprintf(stderr, "[MuPreload] central directory lists %zu entries, end record says %llu\n",
            entries->size(), static_cast<unsigned long long>(entryCount));
    }

    if (!AssignDataRelativePaths(*entries, error))
    {
        return false;
    }

    // Each entry's bytes run up to the next entry's local header (or the
    // directory, for the last one): fetching that span gets the local header,
    // the data and any data descriptor in one go.
    std::vector<RemoteZipEntry*> byOffset;
    byOffset.reserve(entries->size());
    for (RemoteZipEntry& entry : *entries)
    {
        byOffset.push_back(&entry);
    }
    std::sort(byOffset.begin(), byOffset.end(),
              [](const RemoteZipEntry* a, const RemoteZipEntry* b) { return a->localOffset < b->localOffset; });
    for (size_t i = 0; i < byOffset.size(); ++i)
    {
        uint64_t next = centralOffset;
        for (size_t j = i + 1; j < byOffset.size(); ++j)
        {
            if (byOffset[j]->localOffset > byOffset[i]->localOffset)
            {
                next = byOffset[j]->localOffset;
                break;
            }
        }
        if (next <= byOffset[i]->localOffset || next > centralOffset)
        {
            if (error) *error = "zip entry offsets out of order";
            return false;
        }
        byOffset[i]->spanEnd = next;
    }
    return true;
}

std::unordered_map<std::string, std::string> ListLocalFiles(const std::string& dataDir)
{
    std::unordered_map<std::string, std::string> files;
    std::error_code ec;
    const fs::path base(dataDir);
    for (fs::recursive_directory_iterator it(dataDir, ec), end; !ec && it != end; it.increment(ec))
    {
        if (it->is_regular_file(ec))
        {
            const std::string rel = it->path().lexically_relative(base).generic_string();
            files.emplace(ToLowerAscii(rel), it->path().string());
        }
    }
    return files;
}

bool CrcOfFile(const std::string& path, uint32_t* crc)
{
    FILE* in = fopen(path.c_str(), "rb");
    if (in == nullptr)
    {
        return false;
    }
    std::vector<uint8_t> buffer(kIoBufferBytes);
    mz_ulong value = MZ_CRC32_INIT;
    size_t n = 0;
    while ((n = fread(buffer.data(), 1, buffer.size(), in)) > 0)
    {
        value = mz_crc32(value, buffer.data(), n);
    }
    const bool ok = !ferror(in);
    fclose(in);
    *crc = static_cast<uint32_t>(value);
    return ok;
}

std::unordered_map<std::string, IndexEntry> ReadDataIndex(const std::string& root)
{
    std::unordered_map<std::string, IndexEntry> index;
    std::ifstream file(JoinPath(root, kDataIndexFile));
    std::string line;
    while (std::getline(file, line))
    {
        const size_t tab1 = line.find('\t');
        const size_t tab2 = (tab1 == std::string::npos) ? std::string::npos : line.find('\t', tab1 + 1);
        if (tab2 == std::string::npos)
        {
            continue; // a damaged line: that file is simply re-checked
        }
        char* endCrc = nullptr;
        char* endSize = nullptr;
        const std::string crcText = line.substr(0, tab1);
        const std::string sizeText = line.substr(tab1 + 1, tab2 - tab1 - 1);
        const unsigned long crc = strtoul(crcText.c_str(), &endCrc, 16);
        const unsigned long long size = strtoull(sizeText.c_str(), &endSize, 10);
        if (crcText.empty() || sizeText.empty() || *endCrc != '\0' || *endSize != '\0')
        {
            continue;
        }
        index[line.substr(tab2 + 1)] = IndexEntry { static_cast<uint32_t>(crc), size };
    }
    return index;
}

void WriteDataIndex(const std::string& root, const std::vector<RemoteZipEntry>& entries)
{
    const std::string path = JoinPath(root, kDataIndexFile);
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        char crcText[16];
        for (const RemoteZipEntry& entry : entries)
        {
            if (!IsIndexed(entry))
            {
                continue;
            }
            snprintf(crcText, sizeof(crcText), "%x", entry.crc);
            out << crcText << '\t' << entry.size << '\t' << entry.key << '\n';
        }
        if (!out.good())
        {
            fprintf(stderr, "[MuPreload] could not write the data index\n");
            std::error_code ec;
            fs::remove(temp, ec);
            return;
        }
    }
    if (rename(temp.c_str(), path.c_str()) != 0)
    {
        fprintf(stderr, "[MuPreload] could not install the data index\n");
        std::error_code ec;
        fs::remove(temp, ec);
    }
}

void WriteDataIndexFromZip(const std::string& zipPath, const std::string& root)
{
    mz_zip_archive zip {};
    std::string error;
    bool ok = mz_zip_reader_init_file_v2(&zip, zipPath.c_str(), 0, 0, 0) != 0;
    std::vector<RemoteZipEntry> entries;
    if (ok)
    {
        const mz_uint count = mz_zip_reader_get_num_files(&zip);
        for (mz_uint i = 0; i < count; ++i)
        {
            mz_zip_archive_file_stat stat {};
            if (!mz_zip_reader_file_stat(&zip, i, &stat))
            {
                ok = false;
                break;
            }
            RemoteZipEntry entry;
            entry.name = DecodeEntryName(stat.m_filename, (stat.m_bit_flag & (1u << 11)) != 0);
            entry.isDirectory = stat.m_is_directory != 0;
            entry.crc = stat.m_crc32;
            entry.size = stat.m_uncomp_size;
            entries.push_back(std::move(entry));
        }
        mz_zip_reader_end(&zip);
    }
    if (ok && AssignDataRelativePaths(entries, &error))
    {
        WriteDataIndex(root, entries);
        return;
    }
    fprintf(stderr, "[MuPreload] could not index data.zip %s\n", error.c_str());
    std::error_code ec;
    fs::remove(JoinPath(root, kDataIndexFile), ec);
}

bool ApplyEntriesFromPart(const std::string& partPath, uint64_t partStart, const std::vector<RemoteZipEntry>& entries,
                          const std::string& dataDir, const std::atomic<bool>& cancel, std::string* error)
{
    FILE* in = fopen(partPath.c_str(), "rb");
    if (in == nullptr)
    {
        if (error) *error = "cannot read downloaded chunk";
        return false;
    }
    fseeko(in, 0, SEEK_END);
    const uint64_t partLength = static_cast<uint64_t>(ftello(in));

    CaseInsensitiveResolver resolver;
    std::vector<uint8_t> inBuffer(kIoBufferBytes);
    std::vector<uint8_t> outBuffer(kIoBufferBytes);
    bool ok = true;

    for (const RemoteZipEntry& entry : entries)
    {
        auto fail = [&](const std::string& message) {
            if (error) *error = message;
            ok = false;
        };
        if (cancel.load())
        {
            fail("Download cancelled");
            break;
        }
        if ((entry.flags & 0x1) != 0)
        {
            fail("encrypted entry: " + entry.relPath);
            break;
        }
        if (entry.method != 0 && entry.method != 8)
        {
            fail("unsupported compression " + std::to_string(entry.method) + ": " + entry.relPath);
            break;
        }

        uint8_t header[30];
        const uint64_t base = entry.localOffset - partStart;
        if (fseeko(in, static_cast<off_t>(base), SEEK_SET) != 0 || fread(header, 1, sizeof(header), in) != sizeof(header)
            || Le32(header) != kZipLocalHeaderSig)
        {
            fail("bad local header: " + entry.relPath);
            break;
        }
        const uint64_t dataStart = base + 30 + Le16(header + 26) + Le16(header + 28);
        if (dataStart + entry.compressedSize > partLength)
        {
            fail("entry data cut short: " + entry.relPath);
            break;
        }

        const std::string target = resolver.Resolve(dataDir, entry.relPath, false);
        if (target.empty())
        {
            fail("cannot create folder for " + entry.relPath);
            break;
        }
        const std::string temp = target + ".mu_patch";
        FILE* out = fopen(temp.c_str(), "wb");
        if (out == nullptr)
        {
            fail("cannot write " + entry.relPath);
            break;
        }

        fseeko(in, static_cast<off_t>(dataStart), SEEK_SET);
        uint64_t remaining = entry.compressedSize;
        uint64_t written = 0;
        mz_ulong crc = MZ_CRC32_INIT;
        std::string entryError;

        auto emit = [&](const uint8_t* data, size_t n) {
            if (n > 0 && fwrite(data, 1, n, out) != n)
            {
                entryError = "cannot write " + entry.relPath + " (storage full?)";
                return false;
            }
            crc = mz_crc32(crc, data, n);
            written += n;
            return true;
        };

        if (entry.method == 0)
        {
            while (remaining > 0 && entryError.empty())
            {
                const size_t n = fread(inBuffer.data(), 1, static_cast<size_t>(std::min<uint64_t>(remaining, inBuffer.size())), in);
                if (n == 0)
                {
                    entryError = "entry data cut short: " + entry.relPath;
                    break;
                }
                remaining -= n;
                emit(inBuffer.data(), n);
            }
        }
        else
        {
            mz_stream stream {};
            if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK)
            {
                entryError = "inflate init failed: " + entry.relPath;
            }
            bool finished = false;
            while (!finished && entryError.empty())
            {
                if (stream.avail_in == 0 && remaining > 0)
                {
                    const size_t n = fread(inBuffer.data(), 1, static_cast<size_t>(std::min<uint64_t>(remaining, inBuffer.size())), in);
                    if (n == 0)
                    {
                        entryError = "entry data cut short: " + entry.relPath;
                        break;
                    }
                    remaining -= n;
                    stream.next_in = inBuffer.data();
                    stream.avail_in = static_cast<unsigned int>(n);
                }
                stream.next_out = outBuffer.data();
                stream.avail_out = static_cast<unsigned int>(outBuffer.size());
                const int status = mz_inflate(&stream, MZ_NO_FLUSH);
                const size_t produced = outBuffer.size() - stream.avail_out;
                if (!emit(outBuffer.data(), produced))
                {
                    break;
                }
                if (status == MZ_STREAM_END)
                {
                    finished = true;
                }
                else if (status == MZ_BUF_ERROR)
                {
                    if (produced == 0 && stream.avail_in == 0 && remaining == 0)
                    {
                        entryError = "entry data cut short: " + entry.relPath;
                    }
                }
                else if (status != MZ_OK)
                {
                    entryError = "invalid zip data: " + entry.relPath;
                }
            }
            mz_inflateEnd(&stream);
        }

        const bool closed = fclose(out) == 0;
        if (entryError.empty() && !closed)
        {
            entryError = "cannot write " + entry.relPath + " (storage full?)";
        }
        if (entryError.empty() && (static_cast<uint32_t>(crc) != entry.crc || written != entry.size))
        {
            entryError = "CRC mismatch: " + entry.relPath;
        }
        // rename() replaces the old file in one step.
        if (entryError.empty() && rename(temp.c_str(), target.c_str()) != 0)
        {
            entryError = "cannot install " + entry.relPath;
        }
        if (!entryError.empty())
        {
            std::error_code ec;
            fs::remove(temp, ec);
            fail(entryError);
            break;
        }
    }

    fclose(in);
    return ok;
}
} // namespace mu_preload
