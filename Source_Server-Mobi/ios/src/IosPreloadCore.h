#pragma once
// File-system half of the iOS first-launch data install: everything in
// Android's PreloadActivity that is not networking or UI. Plain C++ so it
// reads line-for-line against the Java it was ported from.

#include <atomic>
#include <string>
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
}
