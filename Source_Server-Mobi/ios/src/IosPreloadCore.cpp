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
} // namespace mu_preload
