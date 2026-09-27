// iOS side of the platform hooks declared in 5.Main/source/Platform/IosPlatform.h.
// Android does the same jobs in MuMainNativeActivity.java.

#import <CoreText/CoreText.h>
#import <UIKit/UIKit.h>

#include "Platform/IosPlatform.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

const char* MU_IosDataRoot()
{
    // A plain buffer, not std::string: this is first called from a
    // constructor(101) function, before ordinary static objects are built.
    static char s_root[1024] = {};
    if (s_root[0] == '\0')
    {
        const char* home = getenv("HOME");
        snprintf(s_root, sizeof(s_root), "%s/Documents", (home && home[0]) ? home : ".");
    }
    return s_root;
}

void MU_IosChdirToDataRoot()
{
    const char* root = MU_IosDataRoot();
    mkdir(root, 0755);
    if (chdir(root) != 0)
    {
        fprintf(stderr, "I/MuMain: chdir(%s) failed errno=%d\n", root, errno);
        return;
    }
    fprintf(stderr, "I/MuMain: working dir set to %s\n", root);
}

void MU_IosSetIdleTimerDisabled(bool disabled)
{
    void (^apply)(void) = ^{
        [UIApplication sharedApplication].idleTimerDisabled = disabled ? YES : NO;
    };
    if ([NSThread isMainThread])
    {
        apply();
    }
    else
    {
        dispatch_async(dispatch_get_main_queue(), apply);
    }
}

static UIWindow* MainWindow()
{
    for (UIScene* scene in [UIApplication sharedApplication].connectedScenes)
    {
        if ([scene isKindOfClass:[UIWindowScene class]])
        {
            for (UIWindow* window in ((UIWindowScene*)scene).windows)
            {
                if (window.isKeyWindow)
                {
                    return window;
                }
            }
        }
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return [UIApplication sharedApplication].windows.firstObject;
#pragma clang diagnostic pop
}

void MU_IosGetSafeAreaFractions(float* left, float* top, float* right, float* bottom)
{
    *left = *top = *right = *bottom = 0.0f;
    UIWindow* window = MainWindow();
    const CGSize size = window.bounds.size;
    if (window == nil || size.width <= 0 || size.height <= 0)
    {
        return;
    }
    const UIEdgeInsets insets = window.safeAreaInsets;
    *left = static_cast<float>(insets.left / size.width);
    *right = static_cast<float>(insets.right / size.width);
    *top = static_cast<float>(insets.top / size.height);
    *bottom = static_cast<float>(insets.bottom / size.height);
}

int MU_IosGetBatteryPercent()
{
    UIDevice* device = [UIDevice currentDevice];
    if (!device.batteryMonitoringEnabled)
    {
        device.batteryMonitoringEnabled = YES;
    }
    const float level = device.batteryLevel;
    return (level < 0.0f) ? -1 : static_cast<int>(level * 100.0f + 0.5f);
}

const char* MU_IosSystemFontPath(bool bold)
{
    // Resolved once per weight; the text layer asks for every font size.
    static std::string s_paths[2];
    static bool s_resolved[2] = { false, false };
    const int slot = bold ? 1 : 0;
    if (s_resolved[slot])
    {
        return s_paths[slot].empty() ? nullptr : s_paths[slot].c_str();
    }
    s_resolved[slot] = true;

    // Arial matches the PC client's look and covers Vietnamese; Helvetica is
    // the fallback that every iOS version has.
    NSArray<NSString*>* names = bold ? @[ @"Arial-BoldMT", @"Helvetica-Bold", @"ArialMT", @"Helvetica" ]
                                     : @[ @"ArialMT", @"Helvetica" ];
    for (NSString* name in names)
    {
        CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithNameAndSize((__bridge CFStringRef)name, 12.0);
        if (descriptor == nullptr)
        {
            continue;
        }
        CFURLRef url = (CFURLRef)CTFontDescriptorCopyAttribute(descriptor, kCTFontURLAttribute);
        CFRelease(descriptor);
        if (url == nullptr)
        {
            continue;
        }
        NSString* path = ((__bridge NSURL*)url).path;
        CFRelease(url);
        if (path.length > 0 && access(path.fileSystemRepresentation, R_OK) == 0)
        {
            s_paths[slot] = path.fileSystemRepresentation;
            fprintf(stderr, "I/MuMain: system font %s -> %s\n", name.UTF8String, s_paths[slot].c_str());
            return s_paths[slot].c_str();
        }
    }
    fprintf(stderr, "E/MuMain: no readable system font found\n");
    return nullptr;
}

namespace
{
bool ReadWholeFile(const fs::path& path, std::vector<char>& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// Same rule as copyAssetFile on Android: compare content, write only when it
// differs, so a new app build carrying changed art under the same file name
// replaces what an older install already put in Documents.
bool CopyIfDifferent(const fs::path& source, const fs::path& target)
{
    std::vector<char> sourceBytes;
    if (!ReadWholeFile(source, sourceBytes))
    {
        return false;
    }

    std::vector<char> targetBytes;
    if (ReadWholeFile(target, targetBytes) && targetBytes == sourceBytes)
    {
        return false;
    }

    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    out.write(sourceBytes.data(), static_cast<std::streamsize>(sourceBytes.size()));
    return static_cast<bool>(out);
}

std::string FindBundledAssetRoot()
{
    NSString* resources = [[NSBundle mainBundle] resourcePath];
    for (NSString* candidate in @[ @"MuAssets", @"Resources/MuAssets" ])
    {
        NSString* path = [resources stringByAppendingPathComponent:candidate];
        BOOL isDirectory = NO;
        if ([[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory] && isDirectory)
        {
            return path.UTF8String;
        }
    }
    return {};
}

// Android's external storage ignores case, so the APK's data/ assets and the
// downloaded Data/ folder are one folder there. iOS is case-sensitive: keep a
// single "Data" folder, folding any lowercase "data" into it.
void MergeLowercaseDataFolder(const fs::path& root)
{
    const fs::path lower = root / "data";
    const fs::path canonical = root / "Data";
    std::error_code ec;
    if (!fs::is_directory(lower, ec))
    {
        return;
    }
    if (!fs::exists(canonical, ec))
    {
        fs::rename(lower, canonical, ec);
        if (!ec)
        {
            return;
        }
        ec.clear();
    }
    fs::copy(lower, canonical, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    fs::remove_all(lower, ec);
}
}

void MU_IosCopyBundledAssets()
{
    const std::string assetRoot = FindBundledAssetRoot();
    if (assetRoot.empty())
    {
        fprintf(stderr, "W/MuMain: no MuAssets folder in the app bundle\n");
        return;
    }

    const fs::path dataRoot(MU_IosDataRoot());
    MergeLowercaseDataFolder(dataRoot);

    int copied = 0;
    int total = 0;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(assetRoot, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec))
        {
            continue;
        }
        ++total;
        fs::path relative = fs::relative(it->path(), assetRoot, ec);
        // The bundle's data/ is Android's lowercase assets folder; see above.
        auto first = relative.begin();
        if (first != relative.end() && *first == "data")
        {
            relative = fs::path("Data") / fs::relative(relative, "data");
        }
        if (CopyIfDifferent(it->path(), dataRoot / relative))
        {
            ++copied;
        }
    }
    fprintf(stderr, "I/MuMain: bundled assets: %d files, %d updated\n", total, copied);
}
