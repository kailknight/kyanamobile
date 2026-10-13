#pragma once
// File-system half of the iOS first-launch data install: everything in
// Android's PreloadActivity that is not networking or UI. Plain C++ so it
// reads line-for-line against the Java it was ported from.

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace mu_preload
{
extern const char* const kDataFolderName;      // "Data"
extern const char* const kReadyMarkerFile;     // ".mu_data_ready_v1"

std::string JoinPath(const std::string& a, const std::string& b);
bool EnsureDirectory(const std::string& dir);
void DeleteRecursively(const std::string& path);

// Returns the full path of the child whose name matches ignoring ASCII case,
// or "" when there is none.
std::string FindChildIgnoreCase(const std::string& parent, const std::string& name, bool directoriesOnly);

bool IsDataFolderUsable(const std::string& dataDir);
std::vector<std::string> FindMissingDataEntries(const std::string& dataDir);
std::string JoinMissingEntries(const std::vector<std::string>& missing);

std::string ReadStoredSignature(const std::string& root);
void WriteReadyMarker(const std::string& root, const std::string& dataDir, const std::string& signature);

struct ExtractProgress
{
    std::atomic<long long> bytes { 0 };
    std::atomic<int> files { 0 };
    long long totalBytes = 0;
    int fileCount = 0;
};

bool InspectZip(const std::string& zipPath, ExtractProgress& progress, std::string* error);
bool ExtractZip(const std::string& zipPath, const std::string& extractRoot, ExtractProgress& progress,
                const std::atomic<bool>& cancel, std::string* error);
bool InstallExtractedData(const std::string& extractRoot, const std::string& root, std::string* error);

// ---------------------------------------------------------------------------
// Incremental update (Android: PreloadActivity's "incremental update").
//
// data.zip ends with a central directory: every file's name, CRC-32, size and
// offset. When data.zip changes, only that directory (a few MB) is fetched to
// see which files changed, then only those files' bytes. kDataIndexFile holds
// the CRC and size of every file this device installed; without it the files
// are checked by CRC once instead.
// ---------------------------------------------------------------------------
extern const char* const kDataIndexFile;       // ".mu_data_index_v1"

struct RemoteZipEntry
{
    std::string name;      // decoded the way a full install decodes it
    std::string relPath;   // inside the Data folder; "" when outside it
    std::string key;       // relPath, ASCII lower case
    bool isDirectory = false;
    uint32_t crc = 0;
    uint64_t compressedSize = 0;
    uint64_t size = 0;
    int method = 0;
    int flags = 0;
    uint64_t localOffset = 0;
    uint64_t spanEnd = 0;  // where the next entry, or the directory, starts
};

struct ZipEndInfo
{
    uint64_t entryCount = 0;
    uint64_t centralSize = 0;
    uint64_t centralOffset = 0;
    bool needsZip64 = false;     // fetch 56 bytes at zip64EndOffset, then ParseZip64End
    uint64_t zip64EndOffset = 0;
};

// `tail` is the archive's last min(total, 64 KB + 42) bytes.
bool ParseZipEnd(const std::vector<uint8_t>& tail, ZipEndInfo* out, std::string* error);
bool ParseZip64End(const std::vector<uint8_t>& record, ZipEndInfo* out, std::string* error);
bool ParseCentralDirectory(const std::vector<uint8_t>& central, uint64_t centralOffset, uint64_t entryCount,
                           std::vector<RemoteZipEntry>* entries, std::string* error);

struct IndexEntry
{
    uint32_t crc = 0;
    uint64_t size = 0;
};

// Every regular file under dataDir, keyed like RemoteZipEntry::key.
std::unordered_map<std::string, std::string> ListLocalFiles(const std::string& dataDir);
bool CrcOfFile(const std::string& path, uint32_t* crc);
std::unordered_map<std::string, IndexEntry> ReadDataIndex(const std::string& root);
void WriteDataIndex(const std::string& root, const std::vector<RemoteZipEntry>& entries);
// After a full install, while data.zip is still on disk. Best effort.
void WriteDataIndexFromZip(const std::string& zipPath, const std::string& root);

// Unpacks `entries` from a fetched range that starts at archive offset
// partStart. Each file is written beside its target and renamed over it only
// once its CRC matches, so a bad fetch never leaves a half-written game file.
bool ApplyEntriesFromPart(const std::string& partPath, uint64_t partStart, const std::vector<RemoteZipEntry>& entries,
                          const std::string& dataDir, const std::atomic<bool>& cancel, std::string* error);
}
