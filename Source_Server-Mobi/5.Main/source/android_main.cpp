// =============================================================================
// android_main.cpp
// sokol_app entry point for Android 鑺掗埀顑解偓?replaces Winmain.cpp on Android platform.
//
// Mapping t璋╃鑺?Windows 鑺掗垾鐘偓?Android:
//   WinMain()              鑺掗垾鐘偓?sokol_main()/sapp callbacks
//   CreateWindow / WGL     鑺掗垾鐘偓?sokol_app EGL/GLES3 context
//   WndProc / PeekMessage  鑺掗垾鐘偓?sapp_event callbacks
//   wzAudio + DirectSound  ->  MuAudio (SoundPool + MediaPlayer, Java side)
//   SetTimer()             鑺掗垾鐘偓?SDL_AddTimer / std::thread
//   wglSwapBuffers()       鑺掗垾鐘偓?sokol_app frame present
//   HWND/HDC/HGLRC         鑺掗垾鐘偓?nullptr stubs (PlatformDefs.h)
// =============================================================================

#ifdef __ANDROID__

#include "stdafx.h"
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#define TAKUMI_ANDROID_MAIN_UNDEF_MINMAX 1

#include <unistd.h>

// 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
// Set working directory BEFORE any C++ global constructors run.
// Priority 101 = runs before default C++ constructors (priority 65535).
// Game data lives in the app's external files dir on sdcard so it can be
// pushed via adb push without root access.
// 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
#if defined(__ANDROID__)
__attribute__((constructor(101)))
static void android_set_data_dir_early()
{
    // External files dir 鑺掗埀顑解偓?accessible via adb push, no root needed
    int r1 = chdir("/sdcard/Android/data/com.muonline.client/files");
#if !defined(MU_ANDROID_DISABLE_LOG)
    __android_log_print(ANDROID_LOG_INFO, "MuMain",
        "early chdir(/sdcard/.../files) = %d (errno=%d)", r1, errno);
#endif
    if (r1 == 0) return;
    // Fallback: internal storage
    int r2 = chdir("/data/data/com.muonline.client/files");
#if !defined(MU_ANDROID_DISABLE_LOG)
    __android_log_print(ANDROID_LOG_INFO, "MuMain",
        "fallback chdir(/data/.../files) = %d (errno=%d)", r2, errno);
#endif
}
#endif

#include <SDL.h>
#include <sokol_app.h>
#include <android/input.h>
#include <android/log.h>
#include <android/keycodes.h>
#include <jni.h>
#include <sys/system_properties.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <memory>
#include <mutex>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <vector>

// Game systems
#include "GameConfig/GameConfig.h"
#include "MainLoad.h"
#include "ZzzOpenglUtil.h"
#include "ZzzTexture.h"
#include "ZzzOpenData.h"
#include "ZzzScene.h"
#include "ScenePerfTelemetry.h"
#include "WSclient.h"

#include "ZzzBMD.h"
#include "ZzzInfomation.h"
#include "ZzzObject.h"
#include "ZzzAI.h"
#include "ZzzCharacter.h"
#include "ZzzEffect.h"
#include "CharacterManager.h"
#include "SkillManager.h"
#include "ZzzInterface.h"
#include "ZzzInventory.h"
#include "ZzzLodTerrain.h"
#include "NewUIMainFrameWindow.h"
#include "NewUIMyInventory.h"
#include "NewUIInventoryCtrl.h"
#include "NewUISystem.h"
#include "NewUIFriendWindow.h"
#include "Translation/i18n.h"
#include "Time/Timer.h"
#include "UIMng.h"
#include "UIManager.h"
#include "UIMapName.h"

float GetAdaptiveEffectSpawnScale();
bool ShouldThrottleAdaptiveEffectSpawn(int kind, int type, vec3_t Position, int SubType, float Scale, OBJECT* Owner);
int AndroidBindVirtualPotionSlotFromInventory(int itemType, int itemLevel);
#include "w_MapHeaders.h"
#include "w_PetProcess.h"
#include "Input.h"
#include "NewUIMuHelper.h"
#include "CB_MUHelper.h"
#include "CB_NewJewelBank.h"
#include "CB_JewelBank.h"
#include "DuelMgr.h"
#include "GameShop/InGameShopSystem.h"
#include "Platform/AndroidGDI.h"
#include "Platform/RenderBackend.h"
#include "Platform/gl_compat.h"
#include "android/AndroidNetwork.h"
#include "android/SimpleModulusCrypt.h"
#include "wsclientinline.h"
#include "Util.h"

// stb_image 鑺掗埀顑解偓?implementation is in android_turbojpeg_stubs.cpp; only declare here.
#include "stb_image.h"

#define LOG_TAG "MuMain"
#if defined(MU_ANDROID_DISABLE_LOG)
#define LOGI(...) ((void)0)
#define LOGE(...) ((void)0)
#define LOGW(...) ((void)0)
#else
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#endif

#if defined(MU_ANDROID_PERF_LOG)
static void PerfLogInfo(const char* fmt, ...)
{
    if (fmt == nullptr || fmt[0] == '\0')
    {
        return;
    }

    char buffer[1536];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, buffer);

    // Logcat is unavailable on some devices/ROMs (observed on RedMagic Game Space) -
    // mirror to a plain file so perf data can be pulled via `adb pull` regardless.
    if (FILE* f = fopen("mu_perf_log.txt", "a"))
    {
        fprintf(f, "%s\n", buffer);
        fclose(f);
    }
}
#define PERF_LOGI(...) PerfLogInfo(__VA_ARGS__)
#else
#define PERF_LOGI(...) ((void)0)
#endif

extern float g_fScreenRate_x;
extern float g_fScreenRate_y;
extern int DisplayWinCDepthBox;
extern int DisplayWin;
extern int DisplayHeight;
extern int DisplayWinMid;
extern int DisplayWinExt;
extern int DisplayHeightExt;
extern int DisplayWinReal;
extern BOOL g_bGameServerConnected;
extern BYTE g_byPacketSerialSend;
extern bool First;
extern int FirstTime;

static void UpdateAndroidScreenMetrics(int screenW, int screenH)
{
    WindowWidth = static_cast<unsigned int>(screenW);
    WindowHeight = static_cast<unsigned int>(screenH);
    g_fScreenRate_x = static_cast<float>(WindowWidth) / 640.0f;
    g_fScreenRate_y = static_cast<float>(WindowHeight) / 480.0f;

    DisplayWin = 640;
    DisplayHeight = 480;
    DisplayWinMid = 320;
    DisplayWinExt = 0;
    DisplayWinReal = 640;
    DisplayWinCDepthBox = 0;
    DisplayHeightExt = 0;
}

static std::string ReadAndroidSystemProperty(const char* key)
{
    if (!key || !key[0])
    {
        return {};
    }

    char value[PROP_VALUE_MAX] = {};
    const int length = __system_property_get(key, value);
    if (length <= 0)
    {
        return {};
    }

    return std::string(value, static_cast<std::size_t>(length));
}

static void SetWorkingDirectoryToMobileDataRoot()
{
    static constexpr const char* kMobileDataRoots[] = {
        "/sdcard/Android/data/com.muonline.client/files",
        "/storage/emulated/0/Android/data/com.muonline.client/files",
        "/data/user/0/com.muonline.client/files",
        "/data/data/com.muonline.client/files"
    };

    for (const char* workingDir : kMobileDataRoots)
    {
        if ((workingDir != nullptr) && (workingDir[0] != '\0') && (chdir(workingDir) == 0))
        {
            LOGI("Working dir set to: %s", workingDir);
            return;
        }
    }

    LOGW("Failed to resolve a writable working directory for this mobile platform");
}

// ---- TEMPORARY: shows whether CBGetMain.bin was accepted, since logcat is
// dead on these devices. Remove once confirmed. ----
char g_protectLoadStatus[96] = "GETMAIN not run";

static void InitializeTakumiProtectState()
{
    static bool initialized = false;
    if (initialized)
    {
        return;
    }

    initialized = true;

    auto applyFallbackMainInfo = []()
    {
        std::memset(&gProtect.m_MainInfo, 0, sizeof(gProtect.m_MainInfo));
        gProtect.m_MainInfo.GSPortMin = 55901;
        gProtect.m_MainInfo.GSPortMax = 55999;
        std::strcpy(gProtect.m_MainInfo.CustomerName, "takumi12");
        std::strcpy(gProtect.m_MainInfo.IpAddress, "139.99.24.220");
        gProtect.m_MainInfo.IpAddressPort = 63000;
        std::strcpy(gProtect.m_MainInfo.ClientVersion, "1.04.05");
        std::strcpy(gProtect.m_MainInfo.ClientSerial, "TbYehR2hFUPBKgZj");
        gProtect.LoadEncDec();
        LOGW(
            "Protect fallback active: gsPorts=%u-%u server=%s:%u serial=%s",
            static_cast<unsigned int>(gProtect.m_MainInfo.GSPortMin),
            static_cast<unsigned int>(gProtect.m_MainInfo.GSPortMax),
            gProtect.m_MainInfo.IpAddress,
            static_cast<unsigned int>(gProtect.m_MainInfo.IpAddressPort),
            gProtect.m_MainInfo.ClientSerial);
    };

    // Read exactly what GetMainInfo generates, the same way MainLoad::Load does
    // on PC. Paths use forward slashes and go through fopen: on Android that is
    // redirected to AndroidFopen, which fixes separators and retries with a
    // case-insensitive lookup. std::ifstream gets none of that, which is why
    // reading this file used to be skipped here entirely.
    auto readProtectBlob = [](const char* path, void* dest, size_t size) -> bool
    {
        FILE* fp = fopen(path, "rb");

        if (fp == nullptr)
        {
            LOGE("Open failed for %s", path);
            return false;
        }

        std::fseek(fp, 0, SEEK_END);
        const long fileSize = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);

        if (fileSize != static_cast<long>(size))
        {
            LOGE("Size mismatch for %s: %ld, expected %zu", path, fileSize, size);
            std::fclose(fp);
            return false;
        }

        const size_t got = std::fread(dest, 1, size, fp);
        std::fclose(fp);

        if (got != size)
        {
            LOGE("Read failed for %s: %zu of %zu", path, got, size);
            return false;
        }

        // Same two-step obfuscation GetMainInfo applies on the way out.
        for (size_t n = 0; n < size; ++n)
        {
            reinterpret_cast<BYTE*>(dest)[n] -= static_cast<BYTE>(0x95 ^ HIBYTE(n));
            reinterpret_cast<BYTE*>(dest)[n] ^= static_cast<BYTE>(0xCA ^ LOBYTE(n));
        }

        return true;
    };

    static MAIN_FILE_INFO mainInfo {};   // ~1MB; far too big for the stack

    if (!readProtectBlob("Data/Local/CBGetMain.bin", &mainInfo, sizeof(mainInfo)))
    {
        snprintf(g_protectLoadStatus, sizeof(g_protectLoadStatus) - 1,
                 "GETMAIN fallback (want %zu)", sizeof(MAIN_FILE_INFO));
        applyFallbackMainInfo();
        return;
    }

    // The size check above only proves the file is as long as this build thinks
    // the struct is. If the layout disagreed anywhere in the middle, everything
    // past that point is garbage - including the server address - so check the
    // fields that would strand the player before trusting any of it.
    const bool addressLooksSane =
        (mainInfo.IpAddress[0] > 0x20) &&
        (strnlen(mainInfo.IpAddress, sizeof(mainInfo.IpAddress)) < sizeof(mainInfo.IpAddress)) &&
        (mainInfo.IpAddressPort > 0) &&
        (mainInfo.GSPortMin > 0) &&
        (mainInfo.GSPortMin <= mainInfo.GSPortMax);

    if (!addressLooksSane)
    {
        LOGE("CBGetMain.bin decoded implausibly (ip='%s' port=%u gs=%u-%u); using fallback",
             mainInfo.IpAddress,
             static_cast<unsigned int>(mainInfo.IpAddressPort),
             static_cast<unsigned int>(mainInfo.GSPortMin),
             static_cast<unsigned int>(mainInfo.GSPortMax));
        snprintf(g_protectLoadStatus, sizeof(g_protectLoadStatus) - 1, "GETMAIN bad layout");
        applyFallbackMainInfo();
        return;
    }

    std::memcpy(&gProtect.m_MainInfo, &mainInfo, sizeof(MAIN_FILE_INFO));
    gProtect.LoadEncDec();

    // Reading the file only fills gProtect. On PC, MainLoad::Load then hands
    // that data to the managers that actually read it - custom messages, jewels,
    // wings, pets, monsters, NPC names, VIP packages and so on. Winmain.cpp is
    // not part of the Android build, so none of that ran here and every lookup
    // came back empty ("Could not find message 0!"). SetTargetFps is left out on
    // purpose: mobile does its own frame pacing.
    gMainLoad.ApplyProtectData();

    snprintf(g_protectLoadStatus, sizeof(g_protectLoadStatus) - 1,
             "GETMAIN ok %s:%u rc=%u",
             gProtect.m_MainInfo.IpAddress,
             static_cast<unsigned int>(gProtect.m_MainInfo.IpAddressPort),
             static_cast<unsigned int>(gProtect.m_MainInfo.ReconnectTime));

    // Not fatal: the client still runs without it, only the custom text blocks
    // are empty, so a missing file should not knock out the server address too.
    static TEXT_FILE_INFO textInfo {};   // like mainInfo: far too big for the stack

    if (readProtectBlob("Data/Local/CBTextInfo.bin", &textInfo, sizeof(textInfo)))
    {
        std::memcpy(&gProtect.m_TextInfo, &textInfo, sizeof(TEXT_FILE_INFO));
    }
    else
    {
        LOGW("CBTextInfo.bin not loaded; custom text will be empty");
    }

    LOGI(
        "Protect extras: reconnect=%u fpsLimit=%u showName=%u zoom=%u-%u",
        static_cast<unsigned int>(gProtect.m_MainInfo.ReconnectTime),
        static_cast<unsigned int>(gProtect.m_MainInfo.FpsLimit),
        static_cast<unsigned int>(gProtect.m_MainInfo.PlayerShowName),
        static_cast<unsigned int>(gProtect.m_MainInfo.ZoomMin),
        static_cast<unsigned int>(gProtect.m_MainInfo.ZoomMax));

    LOGI(
        "Protect loaded: gsPorts=%u-%u server=%s:%u clientVersion=%s serial=%s",
        static_cast<unsigned int>(gProtect.m_MainInfo.GSPortMin),
        static_cast<unsigned int>(gProtect.m_MainInfo.GSPortMax),
        gProtect.m_MainInfo.IpAddress,
        static_cast<unsigned int>(gProtect.m_MainInfo.IpAddressPort),
        gProtect.m_MainInfo.ClientVersion,
        gProtect.m_MainInfo.ClientSerial);
}

extern CSimpleModulus g_SimpleModulusCS;
extern CSimpleModulus g_SimpleModulusSC;

static void InitializeTakumiPacketKeys()
{
    const BOOL encLoaded = g_SimpleModulusCS.LoadEncryptionKey((char*)"Data/Enc1.dat");
    const BOOL decLoaded = g_SimpleModulusSC.LoadDecryptionKey((char*)"Data/Dec2.dat");

    LOGI(
        "SimpleModulus init: enc=%d dec=%d",
        encLoaded ? 1 : 0,
        decLoaded ? 1 : 0);
}

static bool ContainsNoCase(const std::string& haystack, const char* needle)
{
    if (!needle || !needle[0] || haystack.empty())
    {
        return false;
    }

    std::string lowerHaystack(haystack);
    std::transform(
        lowerHaystack.begin(),
        lowerHaystack.end(),
        lowerHaystack.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::string lowerNeedle(needle);
    std::transform(
        lowerNeedle.begin(),
        lowerNeedle.end(),
        lowerNeedle.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return lowerHaystack.find(lowerNeedle) != std::string::npos;
}

static bool IsLikelyAndroidEmulator()
{
    const std::string kernelQemu = ReadAndroidSystemProperty("ro.kernel.qemu");
    if (kernelQemu == "1")
    {
        return true;
    }

    // LDPlayer service marker observed on local test images.
    const std::string ldInitService = ReadAndroidSystemProperty("init.svc.ldinit");
    if (!ldInitService.empty())
    {
        return true;
    }

    const std::string hardware = ReadAndroidSystemProperty("ro.hardware");
    const std::string product = ReadAndroidSystemProperty("ro.product.device");
    const std::string model = ReadAndroidSystemProperty("ro.product.model");
    const std::string manufacturer = ReadAndroidSystemProperty("ro.product.manufacturer");
    const std::string abi = ReadAndroidSystemProperty("ro.product.cpu.abi");

    return ContainsNoCase(hardware, "ranchu")
        || ContainsNoCase(hardware, "goldfish")
        || ContainsNoCase(hardware, "vbox")
        || ContainsNoCase(product, "emulator")
        || ContainsNoCase(product, "simulator")
        || ContainsNoCase(product, "vbox")
        || ContainsNoCase(model, "emulator")
        || ContainsNoCase(model, "ldplayer")
        || ContainsNoCase(model, "android sdk built for")
        || ContainsNoCase(manufacturer, "genymotion")
        || ContainsNoCase(manufacturer, "netease")
        || ContainsNoCase(abi, "x86");
}

// Forward-declare camera variables at file scope so they're accessible
// from the anonymous namespace below.
extern float CameraDistance;
extern float CameraDistanceTarget;
extern float g_androidZoomOverride;   // defined in CameraUtility.cpp
extern float CameraAngle[3];

// TEMP: per-render-pass mesh draw counts, defined in ZzzBMD.cpp. Says which
// pass supplies the bulk of the skinned draws before any collapse is designed.
extern int g_ProfMeshPass[5];
int g_ProfMeshPassLast[5] = { 0, 0, 0, 0, 0 };

// =============================================================================
// Globals (defined here on Android 鑺掗埀顑解偓?in Winmain.cpp on Windows)
// =============================================================================

// Stub handles 鑺掗埀顑解偓?referenced by existing code but unused on Android
HWND      g_hWnd      = nullptr;
HINSTANCE g_hInst     = nullptr;
HDC       g_hDC       = nullptr;
HGLRC     g_hRC       = nullptr;
HFONT     g_hFont     = nullptr;
HFONT     g_hFontBold = nullptr;
HFONT     g_hFontBig  = nullptr;
HFONT     g_hFixFont  = nullptr;

CTimer*   g_pTimer    = new CTimer();
bool      Destroy     = false;
bool      ActiveIME   = false;
bool      g_bWndActive = true;
static bool g_AndroidGameInitialized = false;
static bool g_AndroidQuitRequested = false;
static int  g_DrawableWidth = 1280;
static int  g_DrawableHeight = 720;

BYTE*             RendomMemoryDump        = nullptr;
ITEM_ATTRIBUTE*   ItemAttRibuteMemoryDump = nullptr;
CHARACTER*        CharacterMemoryDump     = nullptr;

int         RandomTable[100];
CErrorReport g_ErrorReport;

BOOL g_bMinimizedEnabled    = FALSE;
int  g_iScreenSaverOldValue = 0;
BOOL g_bUseWindowMode       = FALSE;   // Always fullscreen on Android
BOOL g_bUseFullscreenMode   = TRUE;

char m_Username[11]  = {};
char m_Password[21]  = {};
char m_Version[11]   = "2.04d";
char m_ExeVersion[11]= "1.00";
int     m_SoundOnOff    = 1;
int     m_MusicOnOff    = 1;
int     m_Resolution    = 0;
int     m_nColorDepth   = 0;
int     m_RememberMe    = 0;

char g_aszMLSelection[MAX_LANGUAGE_NAME_LENGTH] = {};
int     g_iRenderTextType = 0;

char Mp3FileName[256]   = {};
CMultiLanguage* pMultiLanguage  = nullptr;
// g_dwTopWindow defined in UIControls.cpp
CUIManager* g_pUIManager        = nullptr;
CUIMapName* g_pUIMapName        = nullptr;

CUIMercenaryInputBox* g_pMercenaryInputBox  = nullptr;
CUITextInputBox*      g_pSingleTextInputBox  = nullptr;
CUITextInputBox*      g_pSinglePasswdInputBox = nullptr;
int  g_iChatInputType = 1;

// 鑺掗垾婵冨亾鑺掗垾婵冨亾 Custom standalone character-name input (bypasses Android edit-control stub) 鑺掗垾婵冨亾鑺掗垾婵冨亾
bool     g_charNameInputActive = false;
wchar_t  g_charNameBuf[11]     = {};
int      g_charNameLen         = 0;
// g_bIMEBlock defined in UIControls.cpp

int Time_Effect = 0;
bool  ashies      = false;
int   weather     = 0;
double CPU_AVG    = 0.0;
int    g_MaxMessagePerCycle = -1;

int g_iInactiveTime  = 0;
int g_iNoMouseTime   = 0;
int g_iInactiveWarning = 0;
int g_iMousePopPosition_x = 0;
int g_iMousePopPosition_y = 0;

BOOL g_bInactiveTimeChecked = FALSE;

// Symbols defined in Winmain.cpp on Windows 鑺掗埀顑解偓?stub here on Android
bool g_bEnterPressed = false;
static SDL_FingerID g_primaryTouchFinger = -1;
static bool g_seenFingerInput = false;
static bool g_pendingImeEnterTextInput = false;

// Double-tap tracking 鑺掗埀顑解偓?replaces Windows WM_LBUTTONDBLCLK on Android
// Record last primary finger-up so next finger-down can detect double-tap.
static uint32_t s_doubleTapLastUpMs = 0;
static float    s_doubleTapLastUpNX = -1.0f;   // normalized 0..1
static float    s_doubleTapLastUpNY = -1.0f;
constexpr uint32_t kDoubleTapMaxMs   = 320;     // max ms between taps
constexpr float    kDoubleTapMaxDist = 0.07f;   // max normalized distance
static std::unique_ptr<IRenderBackend> g_RenderBackend;
extern int ActionTarget;
extern int TargetX;
extern int TargetY;
extern int Attacking;
void CGAutoMove(int Type);

// Rect the client's tooltip last drew into, and the switch that stops it
// painting its own background - both live in ZzzInventory.cpp. These must stay
// at global scope: inside the anonymous namespace below they would declare new
// internal symbols instead of referring to those.
extern float g_fLastTipX;
extern float g_fLastTipY;
extern float g_fLastTipW;
extern float g_fLastTipH;
extern bool  g_bTipSuppressBG;

// The global teleport latch from WSclient. Declared here for the same reason as
// the above - inside the namespace it becomes a separate internal symbol.
extern bool Teleport;

// ZzzOpenglUtil's shadows of real GL state: the bound texture, and which blend
// mode the Enable*/Disable* helpers think is active. TextureEnable is already
// declared in ZzzOpenglUtil.h. Same reason again for putting these here -
// declared inside the namespace they link to nothing.
extern int CachTexture;
extern int AlphaBlendType;

namespace
{
constexpr bool kUseLegacyMainHud = true;
constexpr bool kEnableVirtualCombatOverlay = true;
constexpr int kVirtualAttackButton = 0;
constexpr int kVirtualSkillButtonBase = 1;
constexpr int kVirtualVisibleSkillButtonCount = 6;
constexpr int kVirtualSkillSlotCount = kVirtualVisibleSkillButtonCount;

// Of the six buttons in the arc, the first five hold skills (hotkeys 1-5) and
// the sixth opens the skill picker so those five can be rebound. The big circle
// in the middle of the arc is the attack button.
//
// Skill buttons select rather than fire: tap a skill to arm it, then tap attack.
constexpr int kVirtualOverlaySkillSlotCount = 5;
constexpr int kVirtualSkillSelectorVisualSlot = 5;
constexpr int kVirtualUtilityButtonCount = 4;
constexpr int kVirtualUtilityButtonChat = 3;
constexpr int kVirtualRightPanelUtilityActionCount = 12;
constexpr int kVirtualRightPanelGridColumns = 3;
constexpr int kVirtualRightPanelGridRows = 4;
constexpr int kVirtualRightPanelModeGridSlot = -1;
constexpr uint32_t kVirtualMiniMapButtonCooldownMs = 220;
constexpr uint32_t kVirtualAttackRepeatMs = 140;

// Holding attack with the combo on steps through the three skills instead of
// swinging repeatedly, and each step needs long enough to actually go off. The
// 140ms weapon cadence would blow through all three before the first landed.
constexpr uint32_t kVirtualComboRepeatMs = 420;
constexpr uint32_t kVirtualUtilityButtonCooldownMs = 200;
constexpr uint32_t kVirtualSkillAssignLongPressMs = 480;
constexpr uint32_t kVirtualAssignModeTimeoutMs = 9000;
constexpr uint32_t kVirtualAssignTapDebounceMs = 160;
constexpr uint32_t kAndroidLongPressRightClickMs = 1000;
constexpr float kAndroidLongPressRightClickMoveCancelUi = 8.0f;

// Bag item interaction: a quick tap shows the item (the ambient PC-style
// hover/tooltip state machine in CNewUIInventoryCtrl already does this for
// free once nothing intercepts the tap - see the removed ambient branch of
// TryAutoBindAndroidInventoryHotKeyItemAt), a quick second tap on the same
// slot picks it up (same ambient machinery), and holding uses it - drinks/
// equips it for ordinary items, or binds it to a Q/W/E/R slot for a
// consumable, the same thing tapping it used to do before this. Resolved
// while still held, the same way the world-target long-press above is, not on
// release - see UpdateAndroidBagHold.
constexpr uint32_t kAndroidBagHoldMs = 500;
constexpr float kAndroidBagHoldMoveCancelUi = 8.0f;

// Same three gestures, this time for the character's currently-worn equipment
// slots (CNewUIMyInventory::EquipmentWindowProcess, NewUIMyInventory.cpp).
// Unlike the bag, PC's own click-to-pick-up there has no hover-first gate at
// all - m_iPointedSlot != -1 && IsRelease(VK_LBUTTON) fires on the very first
// click, no CNewUIInventoryCtrl::EVENT_HOVER equivalent to lean on - so a
// plain single tap has to be actively held back here (see StartAndroidEquipHold)
// rather than getting tap=show for free the way the bag's ambient machinery
// gives it. The equipment tooltip itself (RenderItemToolTip, driven straight
// off m_iPointedSlot every frame with no cached "last item") needs nothing
// held back at all - MouseX/MouseY alone is enough to make it show correctly.
constexpr uint32_t kAndroidEquipHoldMs = 500;
constexpr float kAndroidEquipHoldMoveCancelUi = 8.0f;
constexpr uint32_t kAndroidEquipDoubleTapMaxMs = 320;

constexpr uint32_t kAndroidTradeAutoMoveIntervalMs = 520;
constexpr uint32_t kAndroidTradeAutoMoveTimeoutMs = 15000;
constexpr const char* kVirtualSkillSlotsPath = "Data/Local/android_touch_skill_slots.cfg";
// Was 426 to keep the pad off the legacy bottom frame's button strip. The frame
// is no longer drawn on mobile, so the pad owns the full height now.
constexpr float kVirtualPadInputMaxY = 480.0f;
constexpr float kInventoryWindowWidth = 190.0f;
constexpr float kInventoryWindowHeight = 429.0f;
constexpr float kVirtualAutoAcquireMaxDistance = 10.0f;
// The stick has one home and never moves off it. It used to recentre on
// whichever finger grabbed it, anywhere in the lower-left quadrant, and only
// appear while touched - so there was nothing on screen to aim at and no way to
// tell which way you were pushing relative to it.
constexpr float kVirtualJoystickDefaultCenterX = 94.0f;
constexpr float kVirtualJoystickDefaultCenterY = 356.0f;

// Sized and hit-tested in device pixels, not UI units. UI space is a 640x480
// stretch of whatever the panel is (see TouchToVirtualUi), so on the 2480x1116
// test phone one UI unit is 3.875px across but only 2.325px down: a ring
// specified in UI units draws as a wide ellipse, and a 45 degree push measured
// in UI units is 59 degrees under the thumb. Both the ring the player aims at
// and the boundaries between the eight directions have to agree with what is on
// the glass, so the physical size is the constant and the UI extents are derived
// from it per axis.
constexpr float kVirtualJoystickRingDiameterPx = 300.0f;
constexpr float kVirtualJoystickKnobDiameterPx = 138.0f;
constexpr float kVirtualJoystickDeadZonePx = 40.0f;

// Grab margin outside the ring, so the stick answers without having to be hit
// exactly.
//
// 120px was not enough in practice. A tap that missed it fell straight through to
// the world, and a tap in the bottom left corner of the screen picks ground that
// is always behind and to the left of the character - so a tap aimed at the right
// side of the stick walked left, whichever side of the stick it was aimed at. That
// is what "I tap right and it still goes left" was: not the stick choosing a
// direction, the stick never being touched at all.
constexpr float kVirtualJoystickGrabMarginPx = 200.0f;
constexpr float kVirtualJoystickMaxScreenFraction = 0.30f;

// A press shorter than this is a tap and gets exactly one tile, like a single
// click on PC. Past it the press is a hold and the path is kept topped up.
//
// 160ms was too eager: an unhurried tap runs 200-300ms, so ordinary taps were
// being promoted to holds and walking two tiles or more. This has to sit above
// how long a person holds a button they mean as a tap, not at the low end of it.
constexpr uint32_t kVirtualJoystickHoldMs = 300;

// How close to the middle of the tile it is entering the character has to be before
// a direction change is acted on, in world units (a tile is TERRAIN_SCALE = 100).
// MovePath swaps waypoints at a distance of 20, so anything at or under that is
// effectively "the step is done"; 30 commits the turn a frame or two earlier without
// cutting the step short enough to see.
constexpr float kVirtualJoystickTurnCommitPx = 30.0f;

// Floor between two path commands. A direction change is meant to be immediate,
// and three frames is not noticeable, but without a floor a thumb sitting on the
// edge of the dead zone or a refused SendMove would re-path every frame.
constexpr uint32_t kVirtualJoystickMinIssueMs = 50;

// Tiles per path while holding. Long enough that the character is re-pathed once
// every couple of tiles rather than every one, short enough that letting go
// cannot leave the server believing the hero walked much further than it did.
constexpr int kVirtualJoystickHoldSteps = 2;

// Tile deltas for the eight headings, indexed clockwise from world -Y - the
// convention CreateAngle uses (ZzzAI.cpp).
constexpr int kVirtualJoystickOctantDX[8] = {  0, +1, +1, +1,  0, -1, -1, -1 };
constexpr int kVirtualJoystickOctantDY[8] = { -1, -1,  0, +1, +1, +1,  0, -1 };
constexpr SDL_FingerID kPcMouseJoystickFingerId = static_cast<SDL_FingerID>(-2);
constexpr bool kShowVirtualAttackButton = kEnableVirtualCombatOverlay;
constexpr bool kShowVirtualSkillButtons = kEnableVirtualCombatOverlay;

// Its draw call sits inside the disabled custom-HUD block, so the button is not
// on screen; without this the hit test stayed live and a tap on empty ground
// near the bottom of the screen would open the keyboard. Turn this back on in
// the same change that gives the button somewhere visible to live.
constexpr bool kShowVirtualChatQuickButton = false;

// Prints the armed skill and the last ground-cast result over the HUD. Kept
// because logcat does not come through on these devices, so this is the only
// practical way to trace the cast path on hardware.
constexpr bool kShowAndroidSkillDebug = false;

struct VirtualButtonLayout
{
    float cx;
    float cy;
    float radius;
};

constexpr float kVirtualAttackButtonCx = 596.0f;
constexpr float kVirtualAttackButtonCy = 350.0f;
constexpr float kVirtualAttackButtonRadius = 29.0f;
constexpr float kVirtualSkillButtonRadius = 19.0f;
struct VirtualUiOffset
{
    float x;
    float y;
};
constexpr std::array<VirtualUiOffset, kVirtualVisibleSkillButtonCount> kVirtualSkillCenters = {
    // Lift the whole right-side skill cluster above the main action bar.
    VirtualUiOffset{ 565.0f, 284.0f },
    VirtualUiOffset{ 610.0f, 284.0f },
    VirtualUiOffset{ 547.0f, 323.0f },
    VirtualUiOffset{ 547.0f, 368.0f },
    VirtualUiOffset{ 565.0f, 414.0f },
    VirtualUiOffset{ 610.0f, 414.0f },
};
constexpr float kVirtualSkillFrameW = 22.0f;
constexpr float kVirtualSkillFrameH = 28.0f;
constexpr float kVirtualSkillBaseFrameW = 32.0f;
constexpr float kVirtualSkillBaseFrameH = 38.0f;
constexpr float kVirtualSkillSourceIconW = 20.0f;
constexpr float kVirtualSkillSourceIconH = 28.0f;
constexpr float kVirtualRightPanelFrameX = 534.0f;
constexpr float kVirtualRightPanelFrameY = 274.0f;
constexpr float kVirtualRightPanelFrameW = 104.0f;
constexpr float kVirtualRightPanelFrameH = 152.0f;
constexpr float kVirtualRightPanelButtonW = 24.0f;
constexpr float kVirtualRightPanelButtonH = 20.0f;
constexpr float kVirtualRightPanelToggleButtonX = 492.0f;
constexpr float kVirtualRightPanelToggleButtonY = 438.0f;
constexpr float kVirtualRightPanelToggleButtonW = 30.0f;
constexpr float kVirtualRightPanelToggleButtonH = 30.0f;

enum VirtualRightPanelUtilityAction
{
    kVirtualRightPanelUtilityActionPk = 0,
    kVirtualRightPanelUtilityActionChat,
    kVirtualRightPanelUtilityActionJewelBank,
    kVirtualRightPanelUtilityActionXShop,
    kVirtualRightPanelUtilityActionHelper,
    kVirtualRightPanelUtilityActionBag,
    kVirtualRightPanelUtilityActionCharacter,
    kVirtualRightPanelUtilityActionMap,
    kVirtualRightPanelUtilityActionSetting,
    kVirtualRightPanelUtilityActionCommand,
    kVirtualRightPanelUtilityActionFriend,
    kVirtualRightPanelUtilityActionGuild,
};

constexpr std::array<const TCHAR*, kVirtualRightPanelUtilityActionCount> kVirtualRightPanelUtilityLabels = {
    _T("PK"),
    _T("CHAT"),
    _T("JWL"),
    _T("SHOP"),
    _T("HELP"),
    _T("BAG"),
    _T("CHAR"),
    _T("MAP"),
    _T("SET"),
    _T("CMD"),
    _T("FRD"),
    _T("GuilD"),
};
constexpr const TCHAR* kVirtualRightPanelModeButtonLabel = _T("CHG");

// The always-visible labelled row along the top right. These are the five the
// reference layout puts there; the remaining seven actions above stay reachable
// through the CHG button, which still opens the full grid. The enum is the
// switch key for both TriggerVirtualRightPanelUtilityAction and the lit-state
// predicate, so it is deliberately left intact rather than trimmed to five.
// Not a utility action: this one opens the grid holding the other seven, and
// used to be the CHG button parked in the bottom right corner.
constexpr int kTopBarActionMenu = -2;
constexpr int kTopBarActionNone = -1;
// Starts and stops the helper directly, without opening its window - the same
// thing the desktop HOME key does. Negative like the menu sentinel because it
// is not one of the utility actions the grid dispatches.
constexpr int kTopBarActionHelperPlay = -3;
// The map-name/coordinates chip, not one of the row's button slots - opens the
// same move/map window the grid's MAP action does.
constexpr int kTopBarActionLocation = -4;
// The row's own show/hide circle, to the row's left.
constexpr int kTopBarActionRowToggle = -5;

// The right-anchored row, and then two more stacked in the gap to its left:
// Helper and, under it, the play toggle. Every loop over the buttons - draw,
// label, hit test, lit state - runs to kTopBarButtonCount, so the pair is
// picked up everywhere by GetTopBarButtonRect placing slots 5 and 6 off-row.
constexpr int kTopBarRowButtonCount = 5;
constexpr int kTopBarButtonCount = 7;
constexpr int kTopBarSlotHelper = 5;
constexpr int kTopBarSlotHelperPlay = 6;

constexpr std::array<int, kTopBarButtonCount> kTopBarActions = {
    kTopBarActionMenu,
    kVirtualRightPanelUtilityActionGuild,
    kVirtualRightPanelUtilityActionXShop,
    kVirtualRightPanelUtilityActionSetting,
    kVirtualRightPanelUtilityActionBag,
    kVirtualRightPanelUtilityActionHelper,
    kTopBarActionHelperPlay,
};

constexpr std::array<const TCHAR*, kTopBarButtonCount> kTopBarLabels = {
    _T("Menu"),
    _T("Guild"),
    _T("Shop"),
    _T("Settings"),
    _T("Bags"),
    _T("Helper"),
    _T("Play"),
};

// Optional per-button art. Missing files are fine: DrawIconButton skips an
// unloaded texture, and the box and label underneath are drawn regardless, so
// the row stays usable until real icons exist.
constexpr std::array<const char*, kTopBarButtonCount> kTopBarIconAssets = {
    "ui/topbar_menu.png",
    "ui/topbar_guild.png",
    "ui/topbar_shop.png",
    "ui/topbar_settings.png",
    "ui/topbar_bags.png",
    "ui/topbar_helper.png",
    // Not drawn yet - the "Play" label carries it until the art exists.
    "ui/topbar_helper_play.png",
};

// 34 rather than the old 52: the row now has to fit entirely to the right of
// the fixed Helper/Play column (kTopBarSideX below) without overlapping it -
// see the rowLeft >= kTopBarSideX + kTopBarButtonW + gap check this and
// kTopBarLocationChipW together satisfy, sized by hand against a 640-wide bar.
constexpr float kTopBarButtonW = 34.0f;
constexpr float kTopBarButtonH = 34.0f;
constexpr float kTopBarButtonGap = 4.0f;
constexpr float kTopBarMarginRight = 8.0f;
constexpr float kTopBarY = 8.0f;

// X of the off-row Helper/Play column: flush against the right edge of the
// HP/MP/SD/AG panel. That panel's constants are declared much further down, so
// the value is written out here and static_assert'd against them at their
// definition rather than being allowed to drift.
constexpr float kTopBarSideX = 250.0f;

// The location chip sits under the row, matching the reference. The gold/Zen
// chip that used to sit beside it was removed. 108 rather than the old 128 -
// see kTopBarButtonW's comment; this and the row width are what keep the
// right-anchored row/toggle/chip chain clear of the fixed Helper/Play column.
constexpr float kTopBarChipH = 18.0f;
constexpr float kTopBarChipGap = 4.0f;
constexpr float kTopBarLocationChipW = 108.0f;

// Columns mirrored (612/579/546 instead of 546/579/612) so the grid reads
// right-to-left, matching the row/toggle/location chip chain above it, which
// is anchored the same way. Slot-to-action mapping (kVirtualRightPanelUtilityAction*)
// is untouched - only where each slot's box lands on screen changed.
constexpr std::array<VirtualUiOffset, kVirtualRightPanelUtilityActionCount> kVirtualRightPanelButtonTopLefts = {
    VirtualUiOffset{ 612.0f, 298.0f },
    VirtualUiOffset{ 579.0f, 298.0f },
    VirtualUiOffset{ 546.0f, 298.0f },
    VirtualUiOffset{ 612.0f, 337.0f },
    VirtualUiOffset{ 579.0f, 337.0f },
    VirtualUiOffset{ 546.0f, 337.0f },
    VirtualUiOffset{ 612.0f, 376.0f },
    VirtualUiOffset{ 579.0f, 376.0f },
    VirtualUiOffset{ 546.0f, 376.0f },
    VirtualUiOffset{ 612.0f, 400.0f },
    VirtualUiOffset{ 579.0f, 400.0f },
    VirtualUiOffset{ 546.0f, 400.0f },
};

const std::array<VirtualButtonLayout, 1 + kVirtualVisibleSkillButtonCount> kVirtualButtons = []()
{
    std::array<VirtualButtonLayout, 1 + kVirtualVisibleSkillButtonCount> buttons{};
    buttons[kVirtualAttackButton] = {
        kVirtualAttackButtonCx,
        kVirtualAttackButtonCy,
        kVirtualAttackButtonRadius
    };

    for (int i = 0; i < kVirtualVisibleSkillButtonCount; ++i)
    {
        buttons[kVirtualSkillButtonBase + i] = {
            kVirtualSkillCenters[i].x,
            kVirtualSkillCenters[i].y,
            kVirtualSkillButtonRadius
        };
    }

    return buttons;
}();

// 鑺掗垾婵冨亾鑺掗垾婵冨亾 Consumable potion slots (restored stable layout) 鑺掗垾婵冨亾鑺掗垾婵冨亾
constexpr int kVirtualConsumableSlotCount = 3;
constexpr std::array<VirtualButtonLayout, kVirtualConsumableSlotCount> kVirtualConsumableSlots = {
    VirtualButtonLayout{ 472.0f, 334.0f, 15.0f }, // consumable slot 0
    VirtualButtonLayout{ 472.0f, 388.0f, 15.0f }, // consumable slot 1
    VirtualButtonLayout{ 472.0f, 442.0f, 15.0f }, // consumable slot 2
};

struct VirtualConsumableSlot {
    int itemType  = -1;   // -1 = empty
    int itemLevel = 0;
};
std::array<VirtualConsumableSlot, kVirtualConsumableSlotCount> g_virtualConsumableSlots{};

// The four consumable buttons along the bottom left. These mirror the Q/W/E/R
// hotkeys rather than being a separate binding: GetVirtualHotKeyBySlot already
// maps slot 0..3 onto them, they persist to the options file, and the inventory
// auto-binds into them, so a potion put on Q on the desktop client is on the
// first button here.
//
// They sit inside the joystick's dynamic area, which is deliberate - the layout
// wants them there, and HandleVirtualFingerDown tests these slots before it
// falls through to the joystick, so a tap on a button never starts a walk.
constexpr int kVirtualMirrorHotKeySlotCount = 4;

// No longer borrowed from the skill boxes: these are the primary consumable
// controls now and need to take a fingertip rather than a mouse pointer.
constexpr float kVirtualMirrorHotKeyFrameW = 38.0f;
constexpr float kVirtualMirrorHotKeyFrameH = 38.0f;
constexpr float kVirtualMirrorHotKeyTouchPadding = 8.0f;
constexpr std::array<int, kVirtualMirrorHotKeySlotCount> kVirtualMirrorHotKeys = {
    SEASON3B::HOTKEY_Q,
    SEASON3B::HOTKEY_W,
    SEASON3B::HOTKEY_E,
    SEASON3B::HOTKEY_R,
};
constexpr std::array<VirtualButtonLayout, kVirtualMirrorHotKeySlotCount> kVirtualMirrorHotKeySlots = {
    VirtualButtonLayout{  38.0f, 447.0f, 19.0f },
    VirtualButtonLayout{  86.0f, 447.0f, 19.0f },
    VirtualButtonLayout{ 134.0f, 447.0f, 19.0f },
    VirtualButtonLayout{ 182.0f, 447.0f, 19.0f },
};
constexpr std::array<const TCHAR*, kVirtualMirrorHotKeySlotCount> kVirtualMirrorHotKeyLabels = {
    _T("Q"),
    _T("W"),
    _T("E"),
    _T("R"),
};

// Pushed down from 12 so the minimap/map stack clears the labelled top bar and
// the coin/location chips that now occupy the corner above it.
constexpr float kTopRightButtonY =
    kTopBarY + kTopBarButtonH + kTopBarChipGap + kTopBarChipH + kTopBarChipGap + 4.0f;
constexpr float kTopRightButtonSize = 44.0f;
constexpr float kTopRightButtonGap = 6.0f;
constexpr float kTopRightButtonMarginRight = 8.0f;
constexpr float kTopRightPanelGap = 8.0f;
constexpr float kCompactMiniMapPanelWidth = 86.0f;
constexpr float kCompactMiniMapPanelHeight = 86.0f;
constexpr float kCompactMiniMapPanelGapToIcons = 10.0f;
constexpr float kVirtualChatQuickButtonCx = 305.0f;
constexpr float kVirtualChatQuickButtonCy = 413.0f;
constexpr float kVirtualChatQuickButtonRadius = 18.0f;
// Select-target: a button that opens a scrollable list of nearby enemies, and
// locks onto whichever one is tapped. Tapping the button again releases the
// lock rather than reopening the list.
//
// Range is 10 tiles to match kVirtualAutoAcquireMaxDistance rather than the
// item menu's 5 - something worth deliberately targeting is usually further off
// than dropped loot.
constexpr float kTargetPickerRangeTiles = 10.0f;
constexpr int kTargetPickerMaxEntries = 16;
constexpr int kTargetPickerVisibleRows = 6;
constexpr float kTargetPickerX = 300.0f;
constexpr float kTargetPickerY = 90.0f;
constexpr float kTargetPickerW = 180.0f;
constexpr float kTargetPickerHeaderH = 26.0f;
constexpr float kTargetPickerRowH = 28.0f;
constexpr float kTargetPickerFooterH = 26.0f;

// The scan walks every client character with a non-trivial predicate, so it is
// throttled rather than run per frame. Finger down and finger up force a
// refresh regardless, so a selection always matches what was on screen.
constexpr uint32_t kTargetPickerRefreshMs = 200;

constexpr float kTargetSelectButtonCx = 624.0f;
constexpr float kTargetSelectButtonCy = 452.0f;
constexpr float kTargetSelectButtonRadius = 18.0f;

// Tabbed chat panel along the bottom centre. The channels already exist - the
// chat log keeps a separate message vector per type and ChangeMessage switches
// which one it renders - so these tabs are a selector over that, not a new
// message store.
constexpr float kChatTabsX = 215.0f;
constexpr float kChatTabsY = 352.0f;
constexpr float kChatTabW = 30.0f;
constexpr float kChatTabH = 16.0f;
constexpr float kChatTabGap = 1.0f;

// Bottom edge of the log itself; it renders upward from here.
constexpr float kChatLogX = 215.0f;
constexpr float kChatLogBottomY = 470.0f;

constexpr int kChatTabCount = 7;


// Last entry is the overflow tab, which cycles the channels that have no tab of
// their own rather than opening a menu.
constexpr std::array<const char*, kChatTabCount> kChatTabLabels = {
    "All", "Chat", "Party", "Guild", "Alliance", "System", "+"
};

// Auto-combo toggle. Only drawn and only hit-tested for the Knight line, so it
// costs nothing on classes that have no combo.
constexpr float kComboToggleX = 596.0f;
constexpr float kComboToggleY = 240.0f;
constexpr float kComboToggleW = 40.0f;
constexpr float kComboToggleH = 22.0f;

constexpr int kAndroidTradePickerMaxEntries = MAX_CHARACTERS_CLIENT;
constexpr int kAndroidTradePickerVisibleRows = 6;
constexpr float kAndroidTradePickerX = 280.0f;
constexpr float kAndroidTradePickerY = 90.0f;
constexpr float kAndroidTradePickerW = 168.0f;
constexpr float kAndroidTradePickerHeaderH = 26.0f;
constexpr float kAndroidTradePickerRowH = 28.0f;
constexpr float kAndroidTradePickerFooterH = 26.0f;

// 鑺掗垾婵冨亾鑺掗垾婵冨亾 UI icon texture cache (loaded once from assets/ui/*.png) 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
struct UITexture { GLuint id = 0; int w = 0; int h = 0; };
static UITexture g_uiTex_map;
static UITexture g_uiTex_minimap;
static UITexture g_uiTex_attack;
static UITexture g_uiTex_skillbox;
static UITexture g_uiTex_skillline;
static UITexture g_uiTex_joystick1;
static UITexture g_uiTex_joystick2;
static UITexture g_uiTex_balo;
static UITexture g_uiTex_character;
static UITexture g_uiTex_setting;

// One per top bar button, in kTopBarActions order. Any that fail to load stay
// at id 0, which DrawIconButton treats as "draw nothing" - the button keeps its
// box and text label, so a missing file costs the icon and nothing else.
static std::array<UITexture, kTopBarButtonCount> g_uiTex_topBar;

// Per-class portraits for the top-left panel, indexed by the CLASS_TYPE base
// class. Any that fail to load stay at id 0 and fall back to character.png, so
// the panel keeps working with a partial set.
constexpr int kClassPortraitCount = 7;
constexpr std::array<const char*, kClassPortraitCount> kClassPortraitAssets = {
    "ui/portrait_dw.png",   // CLASS_WIZARD     - Dark Wizard
    "ui/portrait_dk.png",   // CLASS_KNIGHT     - Dark Knight
    "ui/portrait_fe.png",   // CLASS_ELF        - Fairy Elf
    "ui/portrait_mg.png",   // CLASS_DARK       - Magic Gladiator
    "ui/portrait_dl.png",   // CLASS_DARK_LORD  - Dark Lord
    "ui/portrait_su.png",   // CLASS_SUMMONER   - Summoner
    "ui/portrait_rf.png",   // CLASS_RAGEFIGHTER
};
static std::array<UITexture, kClassPortraitCount> g_uiTex_classPortrait;
static bool g_uiTexturesLoaded = false;

constexpr float kSkillLineU = 157.0f / 677.0f;
constexpr float kSkillLineV = 3.0f / 369.0f;
constexpr float kSkillLineUW = 363.0f / 677.0f;
constexpr float kSkillLineVH = 363.0f / 369.0f;
constexpr float kJoystickKnobU = 203.0f / 677.0f;
constexpr float kJoystickKnobV = 41.0f / 369.0f;
constexpr float kJoystickKnobUW = 286.0f / 677.0f;
constexpr float kJoystickKnobVH = 290.0f / 369.0f;
constexpr float kJoystickRingU = 181.0f / 677.0f;
constexpr float kJoystickRingV = 15.0f / 369.0f;
constexpr float kJoystickRingUW = 356.0f / 677.0f;
constexpr float kJoystickRingVH = 353.0f / 369.0f;

struct AndroidUiRect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

AndroidUiRect GetVirtualMirrorHotKeyRect(int slot)
{
    if (slot < 0 || slot >= kVirtualMirrorHotKeySlotCount)
    {
        return {};
    }

    const VirtualButtonLayout& layout = kVirtualMirrorHotKeySlots[slot];
    return {
        layout.cx - (kVirtualMirrorHotKeyFrameW * 0.5f),
        layout.cy - (kVirtualMirrorHotKeyFrameH * 0.5f),
        kVirtualMirrorHotKeyFrameW,
        kVirtualMirrorHotKeyFrameH
    };
}

float GetTopBarBottomY()
{
    return kTopBarY + kTopBarButtonH;
}

// Right-anchored to the screen edge - the coin chip that used to sit here was
// removed, so this is now the rightmost element in the row. Vertically
// centered in the row's height rather than sitting on a second line below it -
// Menu, the toggle and the chip all read as one row now.
AndroidUiRect GetTopBarLocationChipRect()
{
    return {
        640.0f - kTopBarMarginRight - kTopBarLocationChipW,
        kTopBarY + (kTopBarButtonH - kTopBarChipH) / 2.0f,
        kTopBarLocationChipW,
        kTopBarChipH
    };
}

// Placeholder circle between the icon row and the location chip, toggling
// g_topBarRowIconsVisible. Anchored to the chip (not the row) since the row's
// own left edge is in turn anchored to this button below - the chip is the
// one fixed point the whole right-anchored chain hangs off.
AndroidUiRect GetTopBarRowToggleButtonRect()
{
    constexpr float kToggleSize = 24.0f;
    const AndroidUiRect locRect = GetTopBarLocationChipRect();
    return {
        locRect.x - kTopBarChipGap - kToggleSize,
        kTopBarY + (kTopBarButtonH - kToggleSize) / 2.0f,
        kToggleSize,
        kToggleSize
    };
}

// Menu (slot 0) sits beside the toggle rather than as the leftmost member of
// the hideable row - it stays put (and tappable) no matter which way the
// toggle is set, so it reads as paired with the toggle instead of belonging
// to the group the toggle hides.
AndroidUiRect GetTopBarMenuButtonRect()
{
    const AndroidUiRect toggleRect = GetTopBarRowToggleButtonRect();
    return {
        toggleRect.x - kTopBarButtonGap - kTopBarButtonW,
        kTopBarY,
        kTopBarButtonW,
        kTopBarButtonH
    };
}

// Right-anchored to the Menu button (in turn anchored to the toggle, in turn
// the chip), so the whole group reads as one right-to-left chain: chip,
// toggle, Menu, then the four hideable icons. Slot 1 (Guild) is the leftmost
// of the four.
AndroidUiRect GetTopBarButtonRect(int slot)
{
    if (slot < 0 || slot >= kTopBarButtonCount)
    {
        return {};
    }

    // Helper and its play toggle sit off the row, stacked immediately right of
    // the HP/MP/SD/AG panel rather than against the row's left edge, so they
    // read as belonging to the status block. Anchored to the panel so they
    // follow it if its width ever changes.
    if (slot >= kTopBarRowButtonCount)
    {
        const int stackIndex = slot - kTopBarRowButtonCount;
        return {
            kTopBarSideX,
            kTopBarY + static_cast<float>(stackIndex) * (kTopBarButtonH + kTopBarButtonGap),
            kTopBarButtonW,
            kTopBarButtonH
        };
    }

    if (slot == 0)
    {
        return GetTopBarMenuButtonRect();
    }

    // The four hideable icons (Guild/Shop/Settings/Bags), slots 1-4.
    const int rowCount = kTopBarRowButtonCount - 1;
    const float rowW = (static_cast<float>(rowCount) * kTopBarButtonW)
                     + (static_cast<float>(rowCount - 1) * kTopBarButtonGap);
    const AndroidUiRect menuRect = GetTopBarMenuButtonRect();
    const float rowLeft = menuRect.x - kTopBarButtonGap - rowW;

    return {
        rowLeft + static_cast<float>(slot - 1) * (kTopBarButtonW + kTopBarButtonGap),
        kTopBarY,
        kTopBarButtonW,
        kTopBarButtonH
    };
}

// The live rotating minimap, docked directly under the location chip. Display
// only - see DrawAndroidMiniMap (NewUIHeroPositionInfo.cpp), which reaches
// this rect through AndroidGetMiniMapPanelRect below rather than the older,
// never-rendered-into GetCompactMiniMapRect (that one is anchored to the
// permanently-disabled "Map" stack button and has no renderer left to use it).
AndroidUiRect GetTopBarMiniMapPanelRect()
{
    const AndroidUiRect locRect = GetTopBarLocationChipRect();
    return {
        locRect.x,
        locRect.y + locRect.h + kTopBarChipGap,
        locRect.w,
        locRect.w
    };
}

AndroidUiRect GetTopRightStackButtonRect(int stackIndex)
{
    return {
        640.0f - kTopRightButtonMarginRight - kTopRightButtonSize,
        kTopRightButtonY + static_cast<float>(stackIndex) * (kTopRightButtonSize + kTopRightButtonGap),
        kTopRightButtonSize,
        kTopRightButtonSize
    };
}

bool IsMiniMapPanelVisible()
{
    return g_pNewUISystem != nullptr
        && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MINI_MAP);
}

AndroidUiRect GetMiniMapButtonRect()
{
    return GetTopRightStackButtonRect(0);
}

AndroidUiRect GetMapButtonRect()
{
    return GetTopRightStackButtonRect(1);
}

AndroidUiRect GetCompactMiniMapRect()
{
    const AndroidUiRect mapButton = GetMapButtonRect();
    const float x = std::clamp(
        mapButton.x - kCompactMiniMapPanelWidth - kCompactMiniMapPanelGapToIcons,
        6.0f,
        640.0f - kCompactMiniMapPanelWidth - 6.0f);
    const float y = std::clamp(
        mapButton.y + 2.0f,
        6.0f,
        480.0f - kCompactMiniMapPanelHeight - 6.0f);

    return { x, y, kCompactMiniMapPanelWidth, kCompactMiniMapPanelHeight };
}

AndroidUiRect GetVirtualUtilityButtonRect(int button)
{
    if (button < 0 || button >= kVirtualUtilityButtonCount)
    {
        return {};
    }

    if (button == kVirtualUtilityButtonChat)
    {
        return {
            kVirtualChatQuickButtonCx - kVirtualChatQuickButtonRadius,
            kVirtualChatQuickButtonCy - kVirtualChatQuickButtonRadius,
            kVirtualChatQuickButtonRadius * 2.0f,
            kVirtualChatQuickButtonRadius * 2.0f
        };
    }

    return GetTopRightStackButtonRect(2 + button);
}

AndroidUiRect GetVirtualRightPanelRect()
{
    return {
        kVirtualRightPanelFrameX,
        kVirtualRightPanelFrameY,
        kVirtualRightPanelFrameW,
        kVirtualRightPanelFrameH
    };
}

AndroidUiRect GetVirtualRightPanelGridRect(int gridSlot)
{
    if (gridSlot < 0 || gridSlot >= kVirtualRightPanelUtilityActionCount)
    {
        return {};
    }

    const VirtualUiOffset& buttonPos = kVirtualRightPanelButtonTopLefts[gridSlot];
    return {
        buttonPos.x,
        buttonPos.y,
        kVirtualRightPanelButtonW,
        kVirtualRightPanelButtonH
    };
}

AndroidUiRect GetVirtualRightPanelCombatModeButtonRect()
{
    return {
        kVirtualRightPanelToggleButtonX,
        kVirtualRightPanelToggleButtonY,
        kVirtualRightPanelToggleButtonW,
        kVirtualRightPanelToggleButtonH
    };
}

AndroidUiRect GetVirtualRightPanelUtilityModeButtonRect()
{
    return GetVirtualRightPanelCombatModeButtonRect();
}

bool HitTestAndroidUiRect(float uiX, float uiY, const AndroidUiRect& rect)
{
    return uiX >= rect.x
        && uiX <= (rect.x + rect.w)
        && uiY >= rect.y
        && uiY <= (rect.y + rect.h);
}

float GetAndroidCompactMiniMapTopYInternal()
{
    return GetCompactMiniMapRect().y;
}

float GetAndroidCompactMiniMapLeftXInternal()
{
    return GetCompactMiniMapRect().x;
}

bool GetAndroidMoveMapWindowPositionInternal(int panelWidth, int panelHeight, int* outX, int* outY)
{
    if (outX == nullptr || outY == nullptr)
    {
        return false;
    }

    const int marginX = 12;
    const int topReserved = 72;

    // Was 66 to clear the legacy bottom frame. With the frame gone the window
    // only has to stay off the very edge of the screen.
    const int bottomReserved = 10;
    const int preferredY = 78;
    const int centeredX = (640 - panelWidth) / 2;
    const int maxX = std::max(marginX, 640 - panelWidth - marginX);
    const int minY = topReserved;
    const int maxY = std::max(minY, 480 - panelHeight - bottomReserved);

    *outX = std::clamp(centeredX, marginX, maxX);
    *outY = std::clamp(preferredY, minY, maxY);
    return true;
}

// 鑺掗垾婵冨亾鑺掗垾婵冨亾 Bottom HUD 鑺掗埀顑解偓?HP/MP/AG/EXP bars (1/3 screen width, numbers on bar) 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
// Bars occupy the left 1/3 of the screen (鑺掗垾鍏?13 virtual units), stacked at bottom.
// Numbers are drawn centered directly on each bar. No blending 鑺掗埀顑解偓?solid opaque.
// NOTE: these kHud* values belong to the disabled custom-HUD block further down
// and are not what the game draws. The live HP/MP/SD bars are the kPortrait*
// block below. Do not add new code against these.
constexpr float kHudStripY      = 432.0f;   // top of the HUD strip (virtual y)
constexpr float kHudBarH        =  11.0f;   // height of one bar (tall enough for numbers)
constexpr float kHudBarGap      =   2.0f;   // vertical gap between bars
constexpr float kHudBarLeft     =   4.0f;   // bar left edge (virtual x)
constexpr float kHudBarRight    = 218.0f;   // bar right edge  (鑺掗垾鍏?1/3 of 640)
// Number center X = center of the bar
constexpr float kHudNumCenterX  = (kHudBarLeft + kHudBarRight) * 0.5f;  // 鑺掗垾鍏?111

// Top-left status panel: portrait on the left, HP/MP/SD stacked to its right.
// This replaces the orbs and gauges that used to live on the legacy bottom
// frame, so it is the only place the player can read their health now.
constexpr float kPortraitPanelX     = 6.0f;
constexpr float kPortraitPanelY     = 6.0f;
constexpr float kPortraitAvatarSize = 46.0f;
constexpr float kPortraitBarLeft    = kPortraitPanelX + kPortraitAvatarSize + 6.0f;
constexpr float kPortraitBarRight   = 246.0f;
// Keeps the off-row Helper/Play column flush against this panel; kTopBarSideX
// has to be spelled out up there because it is used before this point.
static_assert(kTopBarSideX == kPortraitBarRight + kTopBarButtonGap,
              "Helper/Play column must sit flush against the status panel");
constexpr float kPortraitBarW       = kPortraitBarRight - kPortraitBarLeft;
constexpr float kPortraitBarH       = 12.0f;
constexpr float kPortraitBarGap     = 3.0f;
constexpr float kPortraitBarTop     = kPortraitPanelY + 4.0f;

// Panel bounds, derived rather than written out at the draw call so the backing
// and everything sized against it cannot drift apart. The backing sits one
// inset outside the content on all four sides.
constexpr float kPortraitPanelInset  = 2.0f;
constexpr float kPortraitBarsBottom  = kPortraitBarTop + (4.0f * kPortraitBarH) + (3.0f * kPortraitBarGap);
constexpr float kPortraitPanelLeft   = kPortraitPanelX - kPortraitPanelInset;
constexpr float kPortraitPanelTop    = kPortraitPanelY - kPortraitPanelInset;
constexpr float kPortraitPanelW      = (kPortraitBarRight - kPortraitPanelX) + (kPortraitPanelInset * 2.0f);
constexpr float kPortraitPanelH      = (kPortraitBarsBottom - kPortraitPanelY) + (kPortraitPanelInset * 2.0f);
constexpr float kPortraitPanelBottom = kPortraitPanelTop + kPortraitPanelH;

// Level / experience strip, in the gap between the status panel and the pet
// gauge. The bar is aligned to the four stat bars above it rather than to the
// panel edge, so the whole left column reads as one stack; the level text sits
// in the portrait's column beside it, where nothing else draws. Everything
// below - the pet icon and its durability bar - is anchored to this row's
// bottom so adding it pushes them down instead of drawing over them.
constexpr float kStatRowGap    = 3.0f;
constexpr float kStatRowH      = 13.0f;
constexpr float kStatRowY      = kPortraitPanelBottom + kStatRowGap;
constexpr float kStatRowBottom = kStatRowY + kStatRowH;
constexpr float kStatRowShift  = kStatRowH + kStatRowGap;
constexpr float kStatLevelX    = kPortraitPanelLeft + 2.0f;
constexpr float kStatLevelW    = (kPortraitBarLeft - 2.0f) - kStatLevelX;
constexpr float kExpBarX       = kPortraitBarLeft;
constexpr float kExpBarW       = kPortraitBarW;

// The portrait owns the panel's whole left column: flush to the backing's top,
// left and bottom edges, and out to the gutter before the bars. Drawing it as a
// square instead left the bottom third of that column showing bare backing,
// because the bar stack is taller than the avatar was wide.
constexpr float kPortraitAvatarX = kPortraitPanelLeft;
constexpr float kPortraitAvatarY = kPortraitPanelTop;
constexpr float kPortraitAvatarW = (kPortraitBarLeft - kPortraitPanelInset) - kPortraitPanelLeft;
constexpr float kPortraitAvatarH = kPortraitPanelH;

// Fenrir / helper durability bar, below the status panel.
constexpr float kPetBarX = 6.0f;
// Was a flat 98; shifted by the level/experience row now sitting above it.
constexpr float kPetBarY = 98.0f + kStatRowShift;
constexpr float kPetBarW = 50.0f;
constexpr float kPetBarH = 10.0f;

// Pet sits under the status panel, matching the reference layout. Anchored to
// the panel's bottom edge, not to the old avatar square - the portrait reaches
// all the way down now, so the previous offset would have put the pet on top of
// the portrait's chest.
constexpr float kPortraitPetSize    = 26.0f;
constexpr float kPortraitPetX       = kPortraitPanelX + 2.0f;
constexpr float kPortraitPetY       = kStatRowBottom + 4.0f;

// Share of the portrait's vertical cover-crop taken off the bottom of the art.
// 1.0 anchors the sampled window to the top of the image, 0.5 centres it.
constexpr float kPortraitCropFromBottom = 1.0f;

// Disabled: the original MU mainframe skill box is back. Keep only joystick
// custom on mobile and do not draw an extra Android-only skill box on top.
constexpr bool kShowVirtualCurrentSkillBox = false;
constexpr float kVirtualCurrentSkillBoxX = kHudBarRight + 4.0f;
constexpr float kVirtualCurrentSkillBoxY = 431.0f;
constexpr float kVirtualCurrentSkillBoxW = 32.0f;
constexpr float kVirtualCurrentSkillBoxH = 38.0f;
constexpr float kVirtualHudChatBoxX = 226.0f;

// Y was 400 so the box cleared the top of the legacy frame. That frame is gone,
// so it can drop into the reclaimed strip and stop covering the play area; 418
// keeps its 52px height just inside the 480 bottom edge.
constexpr float kVirtualHudChatBoxY = 418.0f;
constexpr float kVirtualHudChatBoxW = 170.0f;
constexpr float kVirtualHudChatBoxH = 52.0f;

// 鑺掗垾婵冨亾鑺掗垾婵冨亾 Zoom +/- buttons: top-center beside the level badge 鑺掗垾婵冨亾鑺掗垾婵冨亾
constexpr float kZoomMin  = 800.0f;
constexpr float kZoomMax  = 1600.0f;
constexpr float kZoomDefault = 1200.0f;

// Two-finger pinch. The gap has to open or close by this much, in real screen
// pixels, before the camera moves at all - otherwise resting a second thumb on
// the glass would jog the view. Measured in physical pixels rather than UI
// units because UI space is stretched unevenly on non-4:3 panels, and a pinch
// should feel the same whichever way the fingers happen to be oriented.
constexpr float kPinchActivateSlopPx = 24.0f;
constexpr float kMainFrameItemHotKeyX = 0.0f;
constexpr float kMainFrameItemHotKeyY = 430.0f;
constexpr float kMainFrameItemHotKeyW = 38.0f;
constexpr float kMainFrameItemHotKeyH = 38.0f;

struct ActiveVirtualTouch
{
    SDL_FingerID fingerId = static_cast<SDL_FingerID>(-1);
    int button = -1;
    uint32_t downMs = 0;
    uint32_t lastRepeatMs = 0;
};

struct ActiveVirtualJoystick
{
    SDL_FingerID fingerId = static_cast<SDL_FingerID>(-1);

    // Thumb position as an offset from the fixed home, in UI units, clamped to
    // the ring. Only the knob draw uses it; direction comes from octant.
    float thumbOffsetX = 0.0f;
    float thumbOffsetY = 0.0f;

    // Which of the eight headings is being pushed, or -1 inside the dead zone.
    int octant = -1;

    uint32_t pressedMs = 0;
    uint32_t lastIssueMs = 0;

    // What was last asked of the character: the heading, and whether that command
    // was a hold (topped up) or the single tile of a tap.
    int  issuedOctant = -1;
    bool issuedAsHold = false;

    // Set while the character is walking a path this stick commanded, so letting
    // go only ever truncates the stick's own path and not, say, a tap-to-move
    // destination the player picked on the ground.
    bool ownsHeroPath = false;
};

struct ActiveVirtualPickerTouch
{
    SDL_FingerID fingerId = static_cast<SDL_FingerID>(-1);
    int skillIndex = -1;
};

struct PendingAndroidLongPressRightClick
{
    bool active = false;
    bool fired = false;
    bool releaseRightOnNextUpdate = false;
    SDL_FingerID fingerId = static_cast<SDL_FingerID>(-1);
    uint32_t downMs = 0;
    float startNX = 0.0f;
    float startNY = 0.0f;
    int startMouseX = 0;
    int startMouseY = 0;
};

// A hold on a filled bag slot. Started passively alongside the ambient touch-
// to-mouse simulation (it does not claim the touch), so a plain tap or a quick
// double-tap on the same slot still reaches CNewUIInventoryCtrl's own tooltip/
// pickup handling exactly as it does today; this only ever does something if
// the press outlasts kAndroidBagHoldMs. uiX/uiY are virtual-UI (640x480)
// coordinates - the same space FindAndroidInventoryHotKeyItemAt and MouseX/
// MouseY already use.
struct PendingAndroidBagHold
{
    bool active = false;
    bool fired = false;
    bool releaseRightOnNextUpdate = false;
    SDL_FingerID fingerId = static_cast<SDL_FingerID>(-1);
    uint32_t downMs = 0;
    float startNX = 0.0f;
    float startNY = 0.0f;
    float uiX = 0.0f;
    float uiY = 0.0f;
};

// A hold on a filled equipment slot - same shape as PendingAndroidBagHold, plus
// which slot (and which of the two equipment arrays) it started on, since
// there is no bag-style ITEM* to re-derive that from later.
struct PendingAndroidEquipHold
{
    bool active = false;
    bool fired = false;
    bool releaseRightOnNextUpdate = false;
    SDL_FingerID fingerId = static_cast<SDL_FingerID>(-1);
    uint32_t downMs = 0;
    float startNX = 0.0f;
    float startNY = 0.0f;
    float uiX = 0.0f;
    float uiY = 0.0f;
    int slot = -1;
    bool isMuun = false;
};

// A single-frame press-then-release pulse on VK_LBUTTON, aimed at a specific
// equipment slot, to drive CNewUIMyInventory::EquipmentWindowProcess's own
// pickup branch (m_iPointedSlot != -1 && IsRelease(VK_LBUTTON)) exactly the
// way a real PC click there would - rather than reimplementing pickup here and
// risking missing one of its edge cases (the WD_10HEAVEN pet/wing veto, the
// repair-mode redirect). See FireAndroidEquipSelectPulse/UpdateAndroidEquipSelectPulse.
struct PendingAndroidEquipSelectPulse
{
    bool releaseOnNextUpdate = false;
};

enum class AndroidPlayerCommandMode
{
    None,
    Trade,
    Party,
    Guild,
    Duel,
    Friend
};

struct AndroidTradePickerEntry
{
    int characterIndex = -1;
    SHORT key = 0;
    char id[MAX_ID_SIZE + 1] = {};
    int x = 0;
    int y = 0;
    int distance2 = 0;
};

struct AndroidTradePickerState
{
    bool visible = false;
    AndroidPlayerCommandMode mode = AndroidPlayerCommandMode::None;
    int entryCount = 0;
    std::array<AndroidTradePickerEntry, kAndroidTradePickerMaxEntries> entries{};
    int pendingIndex = -1;
    SHORT pendingKey = 0;
    char pendingId[MAX_ID_SIZE + 1] = {};
    int targetStartX = 0;
    int targetStartY = 0;
    int requestedTargetX = 0;
    int requestedTargetY = 0;
    uint32_t startedMs = 0;
    uint32_t lastMoveMs = 0;
    bool autoMoving = false;
    int scrollOffset = 0;
    bool dragging = false;
    bool dragMoved = false;
    SDL_FingerID dragFingerId = static_cast<SDL_FingerID>(-1);
    float dragStartY = 0.0f;
    float dragLastY = 0.0f;
    SHORT pressedKey = 0;
};

struct AndroidTargetPickerEntry
{
    int characterIndex = -1;
    SHORT key = 0;
    char id[MAX_ID_SIZE + 1] = {};
    int distance2 = 0;
};

struct AndroidTargetPickerState
{
    bool visible = false;
    int entryCount = 0;
    std::array<AndroidTargetPickerEntry, kTargetPickerMaxEntries> entries{};
    uint32_t lastRefreshMs = 0;
    int scrollOffset = 0;
    bool dragging = false;
    bool dragMoved = false;
    SDL_FingerID dragFingerId = static_cast<SDL_FingerID>(-1);
    float dragStartY = 0.0f;
    float dragLastY = 0.0f;
    SHORT pressedKey = 0;
};

AndroidTargetPickerState g_androidTargetPicker{};

std::array<ActiveVirtualTouch, 4> g_activeVirtualTouches{};
std::array<int, kVirtualSkillSlotCount> g_virtualSkillSlots = []()
{
    std::array<int, kVirtualSkillSlotCount> slots{};
    slots.fill(-1);
    return slots;
}();
std::array<int, kVirtualSkillSlotCount> g_virtualSkillTypes{};
ActiveVirtualJoystick g_virtualJoystick{};
ActiveVirtualPickerTouch g_virtualPickerTouch{};
// True while the stick has the character walking one of its own paths. The
// click-to-move loop in ZzzInterface.cpp reads it through
// IsAndroidVirtualJoystickHoldingMovement to stay out of the way.
bool g_virtualJoystickHoldingMovement = false;
bool g_joystickPcMouseCaptured = false;
bool g_virtualSkillSlotsLoaded = false;
bool g_virtualSkillSlotsDirty = false;
bool g_virtualAssignModeActive = false;
int g_virtualAssignSkillIndex = -1;
uint32_t g_virtualAssignModeUntilMs = 0;
bool g_virtualLastSkillPickerOpen = false;
int g_virtualAssignPickerSkillIndex = -1;
bool g_virtualAssignConsumedForPickerSkill = false;
bool g_virtualAssignConsumedForPickerSession = false;
uint32_t g_virtualLastAssignTapMs = 0;
uint32_t g_virtualLastMiniMapTapMs = 0;
uint32_t g_virtualLastUtilityTapMs = 0;

// Which Q/W/E/R slot is waiting to be filled, or -1. Tapping the '+' on an
// empty slot opens the inventory and parks the slot here; the next consumable
// tapped in there binds to it instead of the auto-chosen slot. Reusing the real
// inventory beats building a parallel potion list - it already has the icons,
// the stacks and the hit testing.
int g_androidPendingHotKeyBindSlot = -1;

// The legacy Q/W/E/R hotkey (CNewUIItemHotKey, NewUIMainFrameWindow.cpp) has no
// real "empty" state: clearing m_iHotKeyItemType to -1 only stops it matching
// that one exact item, but GetHotKeyItemIndex falls back to a whole category
// (basic potions, mana potions, ...) whenever the exact type does not match,
// and happily finds and keeps using whatever else of that category is in the
// bag. So a hold-to-unbind that only cleared the type never looked empty as
// long as a same-category potion still existed. This flag is the actual
// "empty" Android's mirror bar honours - set on unbind, cleared the moment a
// slot is bound to something again (either flow: the deliberate '+' pick, or
// AndroidBindVirtualPotionSlotFromInventory). See GetVirtualMirrorHotKeyItem.
bool g_androidHotKeySlotCleared[kVirtualMirrorHotKeySlotCount] = {};

// A press on a Q/W/E/R slot, resolved when the finger lifts: a quick tap drinks,
// a hold rebinds. Deciding on release means no per-frame timer is needed, and
// the alternative - acting on touch-down and then also firing a hold - would
// drink a potion before opening the bag.
constexpr uint32_t kHotKeyRebindHoldMs = 500;
constexpr float kHotKeyPressMoveCancelUi = 14.0f;

struct AndroidHotKeyPressState
{
    SDL_FingerID fingerId = static_cast<SDL_FingerID>(-1);
    int slot = -1;
    uint32_t downMs = 0;
    float downX = 0.0f;
    float downY = 0.0f;
};
AndroidHotKeyPressState g_androidHotKeyPress{};

// Two-finger pinch zoom. fingerA is simply the first finger currently down -
// it may well belong to the joystick or a button - and the gesture only starts
// once a second one lands. startGap/startZoom are the reference the whole
// gesture scales from, so the zoom tracks the fingers absolutely instead of
// accumulating drift frame to frame.
struct AndroidPinchZoomState
{
    SDL_FingerID fingerA = static_cast<SDL_FingerID>(-1);
    SDL_FingerID fingerB = static_cast<SDL_FingerID>(-1);
    float ax = 0.0f, ay = 0.0f;
    float bx = 0.0f, by = 0.0f;
    bool  active = false;
    float startGap = 0.0f;
    float startZoom = 0.0f;
};
AndroidPinchZoomState g_androidPinch{};
// Which arc slot is armed, or -1 for a plain weapon attack. Tapping a skill
// button sets this; the attack button reads it.
int g_virtualSelectedSkillSlot = -1;

// Auto-combo. Each press of the attack button advances one step through arc
// slots 1, 2 and 3, so the Knight combo can be played with a single button.
// Step 0 is meant to hold the weapon skill.
//
// The chain restarts from the opener after a pause, so walking away mid-combo
// and coming back does not start you halfway through the sequence.
constexpr int kVirtualComboSlotCount = 3;
constexpr uint32_t kVirtualComboResetMs = 2500;

bool g_virtualComboEnabled = false;
int g_virtualComboStep = 0;
uint32_t g_virtualComboLastMs = 0;

// What the last combo press actually did. Shown next to the toggle because
// logcat does not come through on these devices, so an on-screen readout is the
// only way to tell a blocked step from an unbound one.
enum AndroidComboResult
{
    kAndroidComboResultNone = 0,
    kAndroidComboResultCast,      // skill went out, step advanced
    kAndroidComboResultWeapon,    // slot empty, swung the weapon instead
    kAndroidComboResultBlocked,   // skill refused, step held for a retry
};

int g_androidComboLastResult = kAndroidComboResultNone;

// Aiming a ground-targeted skill. On the desktop client these are cast by
// pointing at the terrain and right-clicking - the skill reads the global
// TargetX/TargetY that the mouse pick filled in. There is no pointer on a
// phone, so the attack button doubles as an aim stick: press and drag to move
// a reticle over the ground, release to cast there.
struct AndroidGroundAim
{
    bool aiming = false;
    SDL_FingerID fingerId = static_cast<SDL_FingerID>(-1);
    float uiX = 0.0f;
    float uiY = 0.0f;
    int skillIndex = -1;

    // Set on release. The cast itself has to wait for the scene phase, where
    // the camera matrices the terrain pick needs are set up.
    bool pendingCast = false;

    // Frames still to wait before picking. MoveHero never builds the pick ray
    // itself - it reads the MouseTarget the render phase produced from
    // MouseX/MouseY on an earlier frame. So the aim point is parked in
    // MouseX/MouseY and the pick waits for a render to turn it into a ray.
    int settleFrames = 0;
};

AndroidGroundAim g_androidGroundAim{};

// Why the last ground cast did or did not happen. On screen rather than in the
// log, because logcat does not come through on these devices.
enum AndroidGroundCastResult
{
    kGroundCastNone = 0,
    kGroundCastOk,          // tile picked, skill sent
    kGroundCastRefused,     // skill sent but the client refused it
    kGroundCastNoTile,      // aim point hit no terrain
    kGroundCastOffMap,      // picked tile outside the map
    kGroundCastBadSkill,    // armed skill index no longer valid
};

int g_androidGroundCastResult = kGroundCastNone;
int g_androidGroundCastTileX = -1;
int g_androidGroundCastTileY = -1;

// Wall attributes of the last destination tile, with ACTION and HEIGHT masked
// off - the exact value the teleport branch requires to be zero.
int g_androidGroundCastWall = -1;

// Which of the client's own gates refused the cast, when one did.
enum AndroidGroundReason
{
    kGroundReasonOk = 0,
    kGroundReasonSafeZone,    // CanExecuteSkill refuses everything in a safe zone
    kGroundReasonDemand,      // DemendConditionCheckSkill
    kGroundReasonUseCond,     // CheckSkillUseCondition
    kGroundReasonMana,        // CheckMana
    kGroundReasonNoSkillIdx,  // skill type not present in the skill list
};

int g_androidGroundCastReason = kGroundReasonOk;

// The screen point the last pick was taken from, so the readout can show
// whether the drag is being captured separately from whether it resolved.
int g_androidGroundAimX = -1;
int g_androidGroundAimY = -1;

// Hero state at the moment of the cast, captured because all three decide
// whether the teleport branch is reachable at all.
int g_androidGroundCastClass = -1;
int g_androidGroundCastAction = -1;
int g_androidGroundCastMoving = -1;
int g_androidGroundCastTpState = -1;
int g_androidGroundCastAlpha = -1;
int g_androidGroundCastLatch = -1;
int g_androidGroundCastLatchAfter = -1;
int g_androidGroundCastDelay = -1;
int g_androidGroundCastReqChar = -1;
int g_androidGroundCastHaveChar = -1;
int g_androidGroundCastReqEnergy = -1;
int g_androidGroundCastHaveEnergy = -1;

// When the global teleport latch was first seen set. Used to recognise one that
// is never going to be cleared, because the reply that would clear it is not
// coming.
uint32_t g_androidTeleportLatchSinceMs = 0;
constexpr uint32_t kAndroidTeleportLatchStuckMs = 4000;
int g_androidGroundCastFromX = -1;
int g_androidGroundCastFromY = -1;

bool g_virtualRightPanelUtilityMode = false;

// Show/hide for the top bar's Guild/Shop/Settings/Bags slots (row indices 1-4
// - Menu itself, slot 0, stays put since it already doubles as the entry
// point to the other utility panel). Toggled by a small circle beside Menu,
// drawn procedurally for now (DrawTopBarRowToggleButton) until real icon art
// exists for it.
bool g_topBarRowIconsVisible = true;

// Staggered reveal for the four hideable slots: showing sweeps right-to-left
// (the slot closest to the toggle appears first), hiding sweeps left-to-right
// (the leftmost slot disappears first) - set on every toggle tap, read by
// GetTopBarRowSlotAlpha. Starts inactive so the row is simply visible on
// launch with no intro animation.
bool g_topBarRowAnimActive = false;
DWORD g_topBarRowAnimStartTick = 0;
constexpr float kTopBarRowAnimStaggerMs = 55.0f;
constexpr float kTopBarRowAnimFadeMs = 130.0f;
bool g_virtualHudChatPinned = false;
PendingAndroidLongPressRightClick g_androidLongPressRightClick{};
PendingAndroidBagHold g_androidBagHold{};
PendingAndroidEquipHold g_androidEquipHold{};
PendingAndroidEquipSelectPulse g_androidEquipSelectPulse{};

// The last completed short tap on a filled equipment slot that was NOT a hold,
// so the next finger-down on the same slot within kAndroidEquipDoubleTapMaxMs
// can be recognised as a double-tap (select) - equipment has no equivalent of
// the bag's own MouseLButtonDBClick-driven ambient double-tap, because this
// whole gesture is claimed rather than left to fall through (see
// StartAndroidEquipHold).
uint32_t g_androidLastEquipTapUpMs = 0;
int g_androidLastEquipTapSlot = -1;
bool g_androidLastEquipTapIsMuun = false;

AndroidTradePickerState g_androidTradePicker{};

// An explicitly chosen combat target that survives between attacks.
//
// This cannot live in SelectedCharacter. SelectObjects() in ZzzInterface runs
// every frame ahead of the overlay and unconditionally clears that global,
// then re-derives it from a mouse ray - and on touch the "mouse" is wherever
// the player last tapped. So the choice is kept here and pushed back into
// SelectedCharacter at the moment each attack fires.
//
// Keyed on CHARACTER::Key rather than the array index, because client indices
// are recycled as monsters die and respawn; cachedIndex is only a fast path and
// is always revalidated against the key.
struct AndroidTargetLock
{
    bool  active = false;
    SHORT key = 0;
    int   cachedIndex = -1;
    char  id[MAX_ID_SIZE + 1] = {};
};

AndroidTargetLock g_androidTargetLock{};

void CancelAndroidTradeAutoMove(const char* reason);

void CancelAndroidMiniMapAutoMove(const char* reason)
{
    if (g_pNewUIMiniMap == nullptr || !g_pNewUIMiniMap->Movement)
    {
        return;
    }

#if !defined(MU_ANDROID_DISABLE_LOG)
    LOGI("AndroidMiniMap: cancel auto move reason=%s", reason ? reason : "unknown");
#endif

    g_pNewUIMiniMap->Movement = false;
    if (Hero != nullptr)
    {
        Hero->Movement = false;
        SetPlayerStop(Hero);
    }
    CGAutoMove(0);
}

void CancelAndroidAutoMoveForManualInput(const char* reason)
{
    if (g_androidTradePicker.autoMoving)
    {
        CancelAndroidTradeAutoMove(reason);
    }
    CancelAndroidMiniMapAutoMove(reason);
}

int GetVirtualHotKeyBySlot(int slot)
{
    switch (slot)
    {
    case 0: return SEASON3B::HOTKEY_Q;
    case 1: return SEASON3B::HOTKEY_W;
    case 2: return SEASON3B::HOTKEY_E;
    case 3: return SEASON3B::HOTKEY_R;
    default: return SEASON3B::HOTKEY_Q;
    }
}

float GetVirtualButtonHitRadius(int button)
{
    if (button < 0 || button >= static_cast<int>(kVirtualButtons.size()))
    {
        return 0.0f;
    }

    const VirtualButtonLayout& layout = kVirtualButtons[button];
    if (button == kVirtualAttackButton)
    {
        return layout.radius + 8.0f;
    }

    return layout.radius + 10.0f;
}

bool IsWithinVirtualAutoAcquireRange(int characterIndex)
{
    if (characterIndex < 0
        || characterIndex >= MAX_CHARACTERS_CLIENT
        || Hero == nullptr
        || CharactersClient == nullptr)
    {
        return false;
    }

    const CHARACTER* c = &CharactersClient[characterIndex];
    const float dx = static_cast<float>(c->PositionX - Hero->PositionX);
    const float dy = static_cast<float>(c->PositionY - Hero->PositionY);
    const float dist2 = (dx * dx) + (dy * dy);
    const float maxDist = kVirtualAutoAcquireMaxDistance;
    return dist2 <= (maxDist * maxDist);
}

bool IsVirtualPadAvailable()
{
    return SceneFlag == MAIN_SCENE
        && Hero != nullptr
        && CharacterAttribute != nullptr
        && g_pMainFrame != nullptr
        && !AndroidHasFocusedTextInput();
}

void SyncVirtualHudChatBox()
{
    if (g_pNewUISystem == nullptr || g_pChatInputBox == nullptr)
    {
        return;
    }

    if (SceneFlag != MAIN_SCENE)
    {
        return;
    }

    g_pChatInputBox->SetWndPos(
        static_cast<int>(std::lround(kVirtualHudChatBoxX)),
        static_cast<int>(std::lround(kVirtualHudChatBoxY)));

    if (!g_virtualHudChatPinned)
    {
        return;
    }

    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX))
    {
        return;
    }

    // Keep compact chat panel visible in HUD without stealing gameplay focus.
    g_pNewUISystem->Show(SEASON3B::INTERFACE_CHATINPUTBOX);
    SetFocus(g_hWnd ? g_hWnd : reinterpret_cast<HWND>(0x1));
}

bool HitTestVirtualHudChatBox(float uiX, float uiY)
{
    if (SceneFlag != MAIN_SCENE
        || g_pNewUISystem == nullptr
        || !g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX))
    {
        return false;
    }

    return uiX >= kVirtualHudChatBoxX
        && uiX <= (kVirtualHudChatBoxX + kVirtualHudChatBoxW)
        && uiY >= kVirtualHudChatBoxY
        && uiY <= (kVirtualHudChatBoxY + kVirtualHudChatBoxH);
}

bool FocusVirtualChatInputAt(float uiX, float uiY)
{
    if (SceneFlag != MAIN_SCENE
        || g_pNewUISystem == nullptr
        || g_pChatInputBox == nullptr
        || !g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX))
    {
        return false;
    }

    auto hitInput = [uiX, uiY](CUITextInputBox* input)
    {
        return input != nullptr
            && input->GetState() == UISTATE_NORMAL
            && uiX >= input->GetPosition_x()
            && uiX <= (input->GetPosition_x() + input->GetWidth())
            && uiY >= input->GetPosition_y()
            && uiY <= (input->GetPosition_y() + input->GetHeight());
    };

    if (hitInput(g_pChatInputBox->m_pWhsprIDInputBox))
    {
        g_pChatInputBox->m_pWhsprIDInputBox->GiveFocus(TRUE);
        return true;
    }

    if (hitInput(g_pChatInputBox->m_pChatInputBox))
    {
        g_pChatInputBox->m_pChatInputBox->GiveFocus(FALSE);
        return true;
    }

    if (HitTestVirtualHudChatBox(uiX, uiY) && g_pChatInputBox->m_pChatInputBox != nullptr)
    {
        g_pChatInputBox->m_pChatInputBox->GiveFocus(FALSE);
        return true;
    }

    return false;
}

bool FocusVirtualLoginInputAt(float uiX, float uiY)
{
    CUIMng& uiMng = CUIMng::Instance();
    if (!uiMng.m_LoginWin.IsShow())
    {
        return false;
    }

    return uiMng.m_LoginWin.FocusInputAt(uiX, uiY);
}

bool AndroidPointInTextInputBox(CUITextInputBox* input, float uiX, float uiY, float padding = 6.0f)
{
    if (input == nullptr || input->GetState() == UISTATE_HIDE)
    {
        return false;
    }

    const float x = static_cast<float>(input->GetPosition_x()) - padding;
    const float y = static_cast<float>(input->GetPosition_y()) - padding;
    const float w = static_cast<float>(input->GetWidth()) + padding * 2.0f;
    const float h = static_cast<float>(input->GetHeight()) + padding * 2.0f;

    return uiX >= x
        && uiX <= (x + w)
        && uiY >= y
        && uiY <= (y + h);
}

bool AndroidFocusedTextInputContainsPoint(float uiX, float uiY)
{
    if (!AndroidHasFocusedTextInput())
    {
        return false;
    }

    const HWND focused = GetFocus();
    CUITextInputBox* input =
        reinterpret_cast<CUITextInputBox*>(GetWindowLongW(focused, GWL_USERDATA));
    return AndroidPointInTextInputBox(input, uiX, uiY);
}

void AndroidClearFocusedTextInput()
{
    if (!AndroidHasFocusedTextInput())
    {
        return;
    }

    g_pendingImeEnterTextInput = false;
    SetFocus(g_hWnd ? g_hWnd : reinterpret_cast<HWND>(0x1));
}

void AndroidHideKeyboardForOutsideTap(float uiX, float uiY)
{
    if (!AndroidHasFocusedTextInput())
    {
        return;
    }

    if (AndroidFocusedTextInputContainsPoint(uiX, uiY))
    {
        return;
    }

    AndroidClearFocusedTextInput();
}

void ToggleVirtualChatInputBox()
{
    if (g_pNewUISystem == nullptr || g_pChatInputBox == nullptr)
    {
        return;
    }

    const bool isVisible = g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX);

    if (!isVisible)
    {
        g_pNewUISystem->Show(SEASON3B::INTERFACE_CHATINPUTBOX);
        if (g_pChatInputBox->m_pChatInputBox != nullptr)
        {
            g_pChatInputBox->m_pChatInputBox->GiveFocus(TRUE);
        }
        return;
    }

    if (g_pChatInputBox->HaveFocus())
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_CHATINPUTBOX);
        return;
    }

    if (g_pChatInputBox->m_pChatInputBox != nullptr)
    {
        g_pChatInputBox->m_pChatInputBox->GiveFocus(FALSE);
    }
}

bool IsValidSkillIndex(int skillIndex)
{
    if (CharacterAttribute == nullptr)
    {
        return false;
    }

    if (skillIndex < 0 || skillIndex >= MAX_MAGIC)
    {
        return false;
    }

    const int skillType = CharacterAttribute->Skill[skillIndex];
    return skillType > 0 && skillType < MAX_SKILLS;
}

int FindSkillIndexByType(int skillType)
{
    if (CharacterAttribute == nullptr || skillType <= 0 || skillType >= MAX_SKILLS)
    {
        return -1;
    }

    for (int i = 0; i < MAX_MAGIC; ++i)
    {
        if (CharacterAttribute->Skill[i] == skillType)
        {
            return i;
        }
    }

    return -1;
}

bool IsAssignableVirtualSkillIndex(int skillIndex)
{
    // The custom mobile skill rings stay disabled; if they are ever re-enabled,
    // keep index 0 out so they only store explicit skill-list entries.
    return skillIndex > 0
        && skillIndex <= static_cast<int>(std::numeric_limits<BYTE>::max())
        && IsValidSkillIndex(skillIndex);
}

bool IsVirtualOverlayHotKeySkillIndex(int skillIndex)
{
    if (skillIndex >= AT_PET_COMMAND_DEFAULT && skillIndex < AT_PET_COMMAND_END)
    {
        return true;
    }

    return skillIndex >= 0
        && skillIndex <= static_cast<int>(std::numeric_limits<BYTE>::max())
        && IsValidSkillIndex(skillIndex);
}

int GetSkillTypeFromIndex(int skillIndex)
{
    if (!IsValidSkillIndex(skillIndex))
    {
        return 0;
    }

    const int skillType = CharacterAttribute->Skill[skillIndex];
    return (skillType > 0 && skillType < MAX_SKILLS) ? skillType : 0;
}

int GetHeroCharacterIndex();
void SyncVirtualSlotsToMainFrame();
void SaveVirtualSkillSlots();
void ClearVirtualPickerTouch();

void SanitizeVirtualSkillSlots()
{
    for (int& slot : g_virtualSkillSlots)
    {
        if (!IsAssignableVirtualSkillIndex(slot))
        {
            slot = -1;
        }
    }

    for (int i = 0; i < kVirtualSkillSlotCount; ++i)
    {
        if (!IsAssignableVirtualSkillIndex(g_virtualSkillSlots[i]))
        {
            continue;
        }

        const int skillType = GetSkillTypeFromIndex(g_virtualSkillSlots[i]);
        for (int j = 0; j < i; ++j)
        {
            if (!IsAssignableVirtualSkillIndex(g_virtualSkillSlots[j]))
            {
                continue;
            }

            if (g_virtualSkillSlots[j] == g_virtualSkillSlots[i]
                || (skillType > 0 && GetSkillTypeFromIndex(g_virtualSkillSlots[j]) == skillType))
            {
                g_virtualSkillSlots[i] = -1;
                break;
            }
        }
    }
}

void RefreshVirtualSkillTypesFromSlots()
{
    for (int i = 0; i < kVirtualSkillSlotCount; ++i)
    {
        g_virtualSkillTypes[i] = GetSkillTypeFromIndex(g_virtualSkillSlots[i]);
    }
}

std::string BuildVirtualSkillArrayString(const std::array<int, kVirtualSkillSlotCount>& values)
{
    std::string text;
    text.reserve(2 + kVirtualSkillSlotCount * 8);
    text.push_back('[');
    for (int i = 0; i < kVirtualSkillSlotCount; ++i)
    {
        if (i > 0)
        {
            text.push_back(',');
        }
        text += std::to_string(values[i]);
    }
    text.push_back(']');
    return text;
}

bool IsVirtualSkillTypeAssigned(int skillType)
{
    if (skillType <= 0)
    {
        return false;
    }

    for (int slot = 0; slot < kVirtualSkillSlotCount; ++slot)
    {
        if (!IsAssignableVirtualSkillIndex(g_virtualSkillSlots[slot]))
        {
            continue;
        }

        if (GetSkillTypeFromIndex(g_virtualSkillSlots[slot]) == skillType)
        {
            return true;
        }
    }

    return false;
}

bool HasAnyAssignedVirtualSkillSlot()
{
    for (int slot = 0; slot < kVirtualSkillSlotCount; ++slot)
    {
        if (IsAssignableVirtualSkillIndex(g_virtualSkillSlots[slot]))
        {
            return true;
        }
    }

    return false;
}

int FindFirstAssignableSkillIndex()
{
    if (IsAssignableVirtualSkillIndex(Hero != nullptr ? Hero->CurrentSkill : -1))
    {
        return static_cast<int>(Hero->CurrentSkill);
    }

    for (int skillIndex = 1; skillIndex < MAX_MAGIC; ++skillIndex)
    {
        if (IsAssignableVirtualSkillIndex(skillIndex))
        {
            return skillIndex;
        }
    }

    return -1;
}

bool AutoFillVirtualSkillSlotsFromCharacter()
{
    if (CharacterAttribute == nullptr)
    {
        return false;
    }

    bool changed = false;
    int nextSlot = 0;

    for (int skillIndex = 1; skillIndex < MAX_MAGIC && nextSlot < kVirtualSkillSlotCount; ++skillIndex)
    {
        if (!IsAssignableVirtualSkillIndex(skillIndex))
        {
            continue;
        }

        const int skillType = GetSkillTypeFromIndex(skillIndex);
        if (skillType <= 0 || IsVirtualSkillTypeAssigned(skillType))
        {
            continue;
        }

        while (nextSlot < kVirtualSkillSlotCount && IsAssignableVirtualSkillIndex(g_virtualSkillSlots[nextSlot]))
        {
            ++nextSlot;
        }

        if (nextSlot >= kVirtualSkillSlotCount)
        {
            break;
        }

        g_virtualSkillSlots[nextSlot] = skillIndex;
        g_virtualSkillTypes[nextSlot] = GetSkillTypeFromIndex(skillIndex);
        changed = true;
        ++nextSlot;
    }

    return changed;
}

bool ResolveVirtualSkillSlotsFromTypes()
{
    bool changed = false;
    for (int i = 0; i < kVirtualSkillSlotCount; ++i)
    {
        if (IsAssignableVirtualSkillIndex(g_virtualSkillSlots[i]))
        {
            const int resolvedType = GetSkillTypeFromIndex(g_virtualSkillSlots[i]);
            if (resolvedType > 0 && g_virtualSkillTypes[i] != resolvedType)
            {
                g_virtualSkillTypes[i] = resolvedType;
                changed = true;
            }
            continue;
        }

        if (g_virtualSkillTypes[i] <= 0)
        {
            continue;
        }

        const int resolvedIndex = FindSkillIndexByType(g_virtualSkillTypes[i]);
        if (!IsAssignableVirtualSkillIndex(resolvedIndex))
        {
            continue;
        }

        g_virtualSkillSlots[i] = resolvedIndex;
        changed = true;
    }

    if (changed)
    {
        SyncVirtualSlotsToMainFrame();
        if (g_virtualSkillSlotsLoaded)
        {
            g_virtualSkillSlotsDirty = true;
            SaveVirtualSkillSlots();
        }
    }

    return changed;
}

void SyncVirtualSlotsToMainFrame()
{
    // Keep virtual slots independent from the legacy hotkey bar (1..0 / WER/Q).
    // We intentionally do not write these bindings back to the default UI hotkey system.
}

void LoadVirtualSkillSlots()
{
    if (g_virtualSkillSlotsLoaded)
    {
        return;
    }

    for (int slot = 0; slot < kVirtualSkillSlotCount; ++slot)
    {
        g_virtualSkillSlots[slot] = -1;
        g_virtualSkillTypes[slot] = 0;
    }

    int loadedVersion = 0;
    int savedCountLoaded = 0;
    std::ifstream in(kVirtualSkillSlotsPath, std::ios::in);
    if (in.good())
    {
        int version = 0;
        if (in >> version)
        {
            if (version == 1 || version == 2)
            {
                int slot0 = -1;
                int slot1 = -1;
                int slot2 = -1;
                if (in >> slot0 >> slot1 >> slot2)
                {
                    loadedVersion = version;
                    savedCountLoaded = 3;
                    if (version == 2)
                    {
                        g_virtualSkillTypes[0] = slot0;
                        g_virtualSkillTypes[1] = slot1;
                        g_virtualSkillTypes[2] = slot2;
                    }
                    else
                    {
                        // Backward compatibility: v1 stored skill indices directly.
                        g_virtualSkillSlots[0] = slot0;
                        g_virtualSkillSlots[1] = slot1;
                        g_virtualSkillSlots[2] = slot2;
                    }
                }
            }
            else if (version == 3)
            {
                int savedCount = 0;
                if ((in >> savedCount) && savedCount > 0)
                {
                    bool readOk = true;
                    for (int i = 0; i < savedCount; ++i)
                    {
                        int savedType = 0;
                        if (!(in >> savedType))
                        {
                            readOk = false;
                            break;
                        }

                        if (i < kVirtualSkillSlotCount)
                        {
                            g_virtualSkillTypes[i] = savedType;
                        }
                    }

                    if (readOk)
                    {
                        loadedVersion = version;
                        savedCountLoaded = savedCount;
                    }
                }
            }
            else if (version >= 4)
            {
                int savedCount = 0;
                if ((in >> savedCount) && savedCount > 0)
                {
                    bool readOk = true;
                    for (int i = 0; i < savedCount; ++i)
                    {
                        int savedIndex = -1;
                        if (!(in >> savedIndex))
                        {
                            readOk = false;
                            break;
                        }

                        if (i < kVirtualSkillSlotCount)
                        {
                            g_virtualSkillSlots[i] = savedIndex;
                        }
                    }

                    if (readOk)
                    {
                        loadedVersion = version;
                        savedCountLoaded = savedCount;
                    }

                    // Version 6 appends the auto-combo flag. Older files simply
                    // stop here and leave it at its default of off.
                    if (readOk && version >= 6)
                    {
                        int comboEnabled = 0;
                        if (in >> comboEnabled)
                        {
                            g_virtualComboEnabled = (comboEnabled != 0);
                        }
                    }
                }
            }
        }
    }

    SanitizeVirtualSkillSlots();
    bool resolvedChanged = false;
    if (loadedVersion < 4)
    {
        if (loadedVersion == 1)
        {
            RefreshVirtualSkillTypesFromSlots();
        }

        resolvedChanged = ResolveVirtualSkillSlotsFromTypes();
    }
    else
    {
        RefreshVirtualSkillTypesFromSlots();
    }
    bool autoFilled = false;
    if (!HasAnyAssignedVirtualSkillSlot() || (savedCountLoaded > 0 && savedCountLoaded < kVirtualSkillSlotCount))
    {
        autoFilled = AutoFillVirtualSkillSlotsFromCharacter();
    }
    SyncVirtualSlotsToMainFrame();
    g_virtualSkillSlotsLoaded = true;
    g_virtualSkillSlotsDirty = resolvedChanged || autoFilled || loadedVersion < 5;
    if (g_virtualSkillSlotsDirty)
    {
        SaveVirtualSkillSlots();
    }

    const std::string slotText = BuildVirtualSkillArrayString(g_virtualSkillSlots);
    const std::string typeText = BuildVirtualSkillArrayString(g_virtualSkillTypes);
    LOGI(
        "VirtualPad: slots loaded version=%d idx=%s type=%s path=%s",
        loadedVersion,
        slotText.c_str(),
        typeText.c_str(),
        kVirtualSkillSlotsPath);
}

void SaveVirtualSkillSlots()
{
    if (!g_virtualSkillSlotsLoaded || !g_virtualSkillSlotsDirty)
    {
        return;
    }

    SanitizeVirtualSkillSlots();

    std::error_code ec;
    const std::filesystem::path savePath(kVirtualSkillSlotsPath);
    if (savePath.has_parent_path())
    {
        std::filesystem::create_directories(savePath.parent_path(), ec);
    }

    std::ofstream out(savePath, std::ios::out | std::ios::trunc);
    if (!out.good())
    {
        LOGW("VirtualPad: failed to save slots at '%s'", kVirtualSkillSlotsPath);
        return;
    }

    RefreshVirtualSkillTypesFromSlots();
    out << "6 " << kVirtualSkillSlotCount;
    for (int slot = 0; slot < kVirtualSkillSlotCount; ++slot)
    {
        out << ' ' << g_virtualSkillSlots[slot];
    }
    out << ' ' << (g_virtualComboEnabled ? 1 : 0);
    out << '\n';
    g_virtualSkillSlotsDirty = false;
    const std::string slotText = BuildVirtualSkillArrayString(g_virtualSkillSlots);
    LOGI("VirtualPad: slots saved idx=%s", slotText.c_str());
}

void SetVirtualSkillSlot(int slot, int skillIndex)
{
    if (slot < 0 || slot >= kVirtualSkillSlotCount)
    {
        return;
    }

    if (!IsAssignableVirtualSkillIndex(skillIndex))
    {
        const int skillType = (IsValidSkillIndex(skillIndex) && CharacterAttribute != nullptr)
            ? CharacterAttribute->Skill[skillIndex]
            : -1;
        LOGI(
            "VirtualPad: slot%d assign rejected skillIndex=%d skillType=%d",
            slot,
            skillIndex,
            skillType);
        return;
    }

    const int assignedSkillType = GetSkillTypeFromIndex(skillIndex);
    for (int i = 0; i < kVirtualSkillSlotCount; ++i)
    {
        if (i == slot)
        {
            continue;
        }

        if (g_virtualSkillSlots[i] == skillIndex
            || (assignedSkillType > 0 && GetSkillTypeFromIndex(g_virtualSkillSlots[i]) == assignedSkillType))
        {
            g_virtualSkillSlots[i] = -1;
            g_virtualSkillTypes[i] = 0;
        }
    }

    g_virtualSkillSlots[slot] = skillIndex;
    g_virtualSkillTypes[slot] = assignedSkillType;
    SyncVirtualSlotsToMainFrame();
    g_virtualSkillSlotsDirty = true;
    SaveVirtualSkillSlots();

    const std::string slotText = BuildVirtualSkillArrayString(g_virtualSkillSlots);
    LOGI(
        "VirtualPad: slot%d set to skillIndex=%d skillType=%d slots=%s",
        slot,
        skillIndex,
        CharacterAttribute->Skill[skillIndex],
        slotText.c_str());
}

void DeactivateVirtualAssignMode(const char* reason)
{
    if (!g_virtualAssignModeActive)
    {
        return;
    }

    LOGI(
        "VirtualPad: assign mode OFF skillIndex=%d reason=%s",
        g_virtualAssignSkillIndex,
        (reason != nullptr) ? reason : "n/a");
    g_virtualAssignModeActive = false;
    g_virtualAssignSkillIndex = -1;
    g_virtualAssignModeUntilMs = 0;
}

void ActivateVirtualAssignMode(int skillIndex, const char* reason)
{
    if (!IsVirtualOverlayHotKeySkillIndex(skillIndex))
    {
        return;
    }

    const uint32_t nowMs = MU_MobileGetTicks();
    if (g_virtualAssignModeActive && g_virtualAssignSkillIndex == skillIndex)
    {
        g_virtualAssignModeUntilMs = nowMs + kVirtualAssignModeTimeoutMs;
        return;
    }

    g_virtualAssignModeActive = true;
    g_virtualAssignSkillIndex = skillIndex;
    g_virtualAssignModeUntilMs = nowMs + kVirtualAssignModeTimeoutMs;
    const int skillType = (skillIndex >= AT_PET_COMMAND_DEFAULT && skillIndex < AT_PET_COMMAND_END)
        ? skillIndex
        : ((CharacterAttribute != nullptr && IsValidSkillIndex(skillIndex))
            ? CharacterAttribute->Skill[skillIndex]
            : -1);
    LOGI(
        "VirtualPad: assign mode ON skillIndex=%d skillType=%d reason=%s",
        skillIndex,
        skillType,
        (reason != nullptr) ? reason : "n/a");
}

bool IsVirtualAssignModeActive()
{
    if (!g_virtualAssignModeActive)
    {
        return false;
    }

    if (!IsVirtualOverlayHotKeySkillIndex(g_virtualAssignSkillIndex))
    {
        DeactivateVirtualAssignMode("invalid-skill");
        return false;
    }

    const uint32_t nowMs = MU_MobileGetTicks();
    if (nowMs > g_virtualAssignModeUntilMs)
    {
        DeactivateVirtualAssignMode("timeout");
        return false;
    }

    return true;
}

int GetPendingVirtualAssignSkillIndex(uint32_t nowMs)
{
    (void)nowMs;

    if (g_pSkillList != nullptr)
    {
        const int directPickerSkill = g_pSkillList->GetAndroidTouchAssignSkillIndex();
        if (IsVirtualOverlayHotKeySkillIndex(directPickerSkill))
        {
            return directPickerSkill;
        }
    }

    if (IsVirtualAssignModeActive())
    {
        return g_virtualAssignSkillIndex;
    }

    // Keep the last picker-selected skill pending until the player assigns it
    // to one of the mobile skill slots or explicitly reopens the picker.
    if (IsVirtualOverlayHotKeySkillIndex(g_virtualAssignPickerSkillIndex))
    {
        return g_virtualAssignPickerSkillIndex;
    }

    return -1;
}

void UpdateVirtualAssignMode()
{
    if (g_pSkillList == nullptr || Hero == nullptr || CharacterAttribute == nullptr)
    {
        DeactivateVirtualAssignMode("missing-context");
        g_virtualLastSkillPickerOpen = false;
        g_virtualAssignPickerSkillIndex = -1;
        g_virtualAssignConsumedForPickerSkill = false;
        g_virtualAssignConsumedForPickerSession = false;
        ClearVirtualPickerTouch();
        return;
    }

    const bool pickerOpen = g_pSkillList->IsSkillPickerOpen();
    const int pickedSkill = g_pSkillList->GetAndroidTouchAssignSkillIndex();

    if (pickerOpen)
    {
        if (!g_virtualLastSkillPickerOpen)
        {
            g_virtualAssignPickerSkillIndex = -1;
            g_virtualAssignConsumedForPickerSkill = false;
            g_virtualAssignConsumedForPickerSession = false;
        }

        if (pickedSkill != g_virtualAssignPickerSkillIndex)
        {
            g_virtualAssignPickerSkillIndex = pickedSkill;
            g_virtualAssignConsumedForPickerSkill = false;
            g_virtualAssignConsumedForPickerSession = false;
        }

        if (IsVirtualOverlayHotKeySkillIndex(pickedSkill))
        {
            ActivateVirtualAssignMode(pickedSkill, "picker-open");
        }
        else
        {
            DeactivateVirtualAssignMode("picker-await-selection");
        }
    }
    else
    {
        g_virtualAssignConsumedForPickerSession = false;
        if (g_virtualLastSkillPickerOpen
            && IsVirtualOverlayHotKeySkillIndex(pickedSkill))
        {
            // Allow one quick assignment after closing picker.
            g_virtualAssignPickerSkillIndex = pickedSkill;
            g_virtualAssignConsumedForPickerSkill = false;
            ActivateVirtualAssignMode(pickedSkill, "picker-closed");
        }
    }

    g_virtualLastSkillPickerOpen = pickerOpen;
}

void TouchToVirtualUi(const SDL_TouchFingerEvent& touch, float& outX, float& outY)
{
    const float nx = std::clamp(touch.x, 0.0f, 1.0f);
    const float ny = std::clamp(touch.y, 0.0f, 1.0f);
    outX = nx * 640.0f;
    outY = ny * 480.0f;
}

bool IsTouchOverInventoryWindow(float uiX, float uiY)
{
    if (g_pNewUISystem == nullptr
        || g_pMyInventory == nullptr
        || !g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY))
    {
        return false;
    }

    const POINT& pos = g_pMyInventory->GetPos();
    const float left = static_cast<float>(pos.x);
    const float top = static_cast<float>(pos.y);
    const float right = left + kInventoryWindowWidth;
    const float bottom = top + kInventoryWindowHeight;
    return uiX >= left && uiX <= right && uiY >= top && uiY <= bottom;
}

// outCtrl receives whichever inventory control (main bag or the expanded one)
// actually owns the item found, or nullptr if none - callers that need to poke
// the control itself (SetEventState, IsLocked, ...) would otherwise have to
// redo this exact lookup a second time to get it.
ITEM* FindAndroidBagItemAndCtrlAt(float uiX, float uiY, SEASON3B::CNewUIInventoryCtrl** outCtrl)
{
    if (outCtrl != nullptr)
    {
        *outCtrl = nullptr;
    }

    if (g_pNewUISystem == nullptr
        || g_pMyInventory == nullptr
        || !g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY))
    {
        return nullptr;
    }

    const int mouseX = std::clamp(static_cast<int>(uiX), 0, 640);
    const int mouseY = std::clamp(static_cast<int>(uiY), 0, 480);

    if (SEASON3B::CNewUIInventoryCtrl::GetPickedItem() != nullptr)
    {
        return nullptr;
    }

    if (SEASON3B::CNewUIInventoryCtrl* inventoryCtrl = g_pMyInventory->GetInventoryCtrl())
    {
        if (ITEM* item = inventoryCtrl->FindItemAtPt(mouseX, mouseY))
        {
            if (outCtrl != nullptr)
            {
                *outCtrl = inventoryCtrl;
            }
            return item;
        }
    }

    if (g_pMyInventoryExt != nullptr
        && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_ExpandInventory))
    {
        if (ITEM* item = g_pMyInventoryExt->FindItemAtPt(mouseX, mouseY))
        {
            if (outCtrl != nullptr)
            {
                // CNewUIInventoryExtension holds one CNewUIInventoryCtrl per
                // page, and GetInventoryCtrl() (no index) picks the page by
                // checking the live MouseX/MouseY globals against each one's
                // rect - it does not take a point parameter, so this touch's
                // position has to be there for it to see, not whatever the
                // mouse was doing last frame. Restored right after; nothing
                // else here is meant to move the simulated mouse.
                const int savedMouseX = MouseX;
                const int savedMouseY = MouseY;
                MouseX = mouseX;
                MouseY = mouseY;
                *outCtrl = g_pMyInventoryExt->GetInventoryCtrl();
                MouseX = savedMouseX;
                MouseY = savedMouseY;
            }
            return item;
        }
    }

    return nullptr;
}

ITEM* FindAndroidInventoryHotKeyItemAt(float uiX, float uiY)
{
    return FindAndroidBagItemAndCtrlAt(uiX, uiY, nullptr);
}

// A hold on a filled bag slot - see PendingAndroidBagHold and the
// implementations further down (alongside the world-target long-press this is
// modelled on). Forward-declared here, still inside this same anonymous
// namespace, because HandleVirtualFingerDown/Motion/Up below call them well
// before that point in the file.
void StartAndroidBagHold(const SDL_TouchFingerEvent& touch, float uiX, float uiY);
void UpdateAndroidBagHoldMotion(const SDL_TouchFingerEvent& touch);
bool FinishAndroidBagHoldFingerUp(SDL_FingerID fingerId);
void UpdateAndroidBagHold();

// Same idea, for the character's currently-worn equipment slots - see
// PendingAndroidEquipHold/PendingAndroidEquipSelectPulse and the
// implementations further down.
void StartAndroidEquipHold(const SDL_TouchFingerEvent& touch, float uiX, float uiY, int slot, bool isMuun);
// Both return whether they were tracking this finger at all (not whether a
// hold fired) - true means claim the event, matching the hotkey-slot press
// pattern this is modelled on: like that one, the equipment hold IS claimed at
// finger-down (StartAndroidEquipHold, unlike the bag's), so its motion and
// release must stay claimed too rather than falling through to the joystick.
bool UpdateAndroidEquipHoldMotion(const SDL_TouchFingerEvent& touch);
bool FinishAndroidEquipHoldFingerUp(SDL_FingerID fingerId);
void UpdateAndroidEquipHold();
void UpdateAndroidEquipSelectPulse();
void FireAndroidEquipSelectPulse(float uiX, float uiY);

// Wraps CNewUIMyInventory::FindEquippedItemAtPt, converting the same virtual-UI
// (640x480) coordinates FindAndroidBagItemAndCtrlAt uses into the int point that
// call needs, and gating on the inventory window being visible - the same
// visibility flag CNewUIMyInventory::UpdateMouseEvent (and so
// EquipmentWindowProcess) itself is gated on, so nothing here can fire while
// the window driving it is not even being updated.
ITEM* FindAndroidEquippedItemAt(float uiX, float uiY, int* outSlot, bool* outIsMuun)
{
    if (outSlot != nullptr)
    {
        *outSlot = -1;
    }
    if (outIsMuun != nullptr)
    {
        *outIsMuun = false;
    }

    if (g_pNewUISystem == nullptr
        || g_pMyInventory == nullptr
        || !g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY))
    {
        return nullptr;
    }

    const int x = std::clamp(static_cast<int>(uiX), 0, 640);
    const int y = std::clamp(static_cast<int>(uiY), 0, 480);
    return g_pMyInventory->FindEquippedItemAtPt(x, y, outSlot, outIsMuun);
}

bool TryAutoBindAndroidInventoryHotKeyItemAt(float uiX, float uiY)
{
    // Only the deliberate flow lands here now: tapping the '+' on an empty
    // Q/W/E/R slot parks that slot here and opens the bag, and the next
    // consumable tapped in there binds to it. Passively tapping a potion while
    // just browsing used to auto-bind it too - that moved to a hold
    // (UpdateAndroidBagHold) so a plain tap can show the item like everything
    // else in the bag, the same as the new single-tap/double-tap/hold model
    // asks for.
    if (g_androidPendingHotKeyBindSlot < 0
        || g_androidPendingHotKeyBindSlot >= kVirtualMirrorHotKeySlotCount
        || g_pMainFrame == nullptr)
    {
        return false;
    }

    ITEM* item = FindAndroidInventoryHotKeyItemAt(uiX, uiY);
    if (item == nullptr || !SEASON3B::CNewUIMyInventory::CanRegisterItemHotKey(item->Type))
    {
        return false;
    }

    const int itemLevel = (item->Level >> 3) & 15;

    g_pMainFrame->SetItemHotKey(
        kVirtualMirrorHotKeys[g_androidPendingHotKeyBindSlot], item->Type, itemLevel);
    g_androidHotKeySlotCleared[g_androidPendingHotKeyBindSlot] = false;
    LOGI("Android bind consumable type=%d level=%d -> requested slot=%d",
         item->Type, itemLevel, g_androidPendingHotKeyBindSlot);
    g_androidPendingHotKeyBindSlot = -1;

    // The bag was opened purely to answer the '+', so dismiss it once that is
    // done.
    if (g_pNewUISystem != nullptr)
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_INVENTORY);
    }

    MouseLButton = false;
    MouseLButtonPush = false;
    MouseLButtonPop = false;
    MouseLButtonDBClick = false;
    return true;
}

bool IsVirtualJoystickCaptured(SDL_FingerID fingerId)
{
    return g_virtualJoystick.fingerId != static_cast<SDL_FingerID>(-1)
        && g_virtualJoystick.fingerId == fingerId;
}

bool IsVirtualPickerTouchCaptured(SDL_FingerID fingerId)
{
    return g_virtualPickerTouch.fingerId != static_cast<SDL_FingerID>(-1)
        && g_virtualPickerTouch.fingerId == fingerId;
}

void ClearVirtualPickerTouch()
{
    g_virtualPickerTouch.fingerId = static_cast<SDL_FingerID>(-1);
    g_virtualPickerTouch.skillIndex = -1;
}

bool HandleVirtualPickerFingerDown(const SDL_TouchFingerEvent& touch)
{
    if (!kShowVirtualSkillButtons)
    {
        return false;
    }

    if (g_pSkillList == nullptr || !g_pSkillList->IsSkillPickerOpen())
    {
        return false;
    }

    float uiX = 0.0f;
    float uiY = 0.0f;
    TouchToVirtualUi(touch, uiX, uiY);

    // Page arrows first: they sit beside the grid, so a tap there is not a
    // skill and must not fall through to the world underneath.
    const int pageButton = g_pSkillList->HitTestAndroidSkillPickerPageButton(uiX, uiY);
    if (pageButton >= 0)
    {
        g_pSkillList->StepAndroidSkillPickerPage(pageButton == 0 ? -1 : 1);
        PlayBuffer(SOUND_CLICK01);
        return true;
    }

    const int skillIndex = g_pSkillList->HitTestAndroidTouchSkillPicker(uiX, uiY);
    if (skillIndex < 0)
    {
        return false;
    }

    g_virtualPickerTouch.fingerId = touch.fingerId;
    g_virtualPickerTouch.skillIndex = skillIndex;
    return true;
}

bool HandleVirtualPickerFingerMotion(const SDL_TouchFingerEvent& touch)
{
    if (!kShowVirtualSkillButtons)
    {
        return false;
    }

    return IsVirtualPickerTouchCaptured(touch.fingerId);
}

bool HandleVirtualPickerFingerUp(const SDL_TouchFingerEvent& touch)
{
    if (!kShowVirtualSkillButtons)
    {
        return false;
    }

    if (!IsVirtualPickerTouchCaptured(touch.fingerId))
    {
        return false;
    }

    float uiX = 0.0f;
    float uiY = 0.0f;
    TouchToVirtualUi(touch, uiX, uiY);

    const int releasedSkill = (g_pSkillList != nullptr)
        ? g_pSkillList->HitTestAndroidTouchSkillPicker(uiX, uiY)
        : -1;
    const int chosenSkill = (releasedSkill == g_virtualPickerTouch.skillIndex)
        ? releasedSkill
        : g_virtualPickerTouch.skillIndex;

    if (g_pSkillList != nullptr && chosenSkill >= 0)
    {
        int previousSkillType = 0;
        if (Hero != nullptr && CharacterAttribute != nullptr)
        {
            if (Hero->CurrentSkill >= AT_PET_COMMAND_DEFAULT && Hero->CurrentSkill < AT_PET_COMMAND_END)
            {
                previousSkillType = Hero->CurrentSkill;
            }
            else if (Hero->CurrentSkill >= 0 && Hero->CurrentSkill < MAX_MAGIC)
            {
                previousSkillType = CharacterAttribute->Skill[Hero->CurrentSkill];
            }
        }

        if (previousSkillType > 0)
        {
            g_pSkillList->SetHeroPriorSkill(static_cast<BYTE>(previousSkillType));
        }

        if (Hero != nullptr)
        {
            Hero->CurrentSkill = static_cast<BYTE>(chosenSkill);
        }

        g_pSkillList->SetAndroidTouchAssignSkillIndex(chosenSkill);
        g_pSkillList->SetSkillPickerOpen(false);

        if (IsVirtualOverlayHotKeySkillIndex(chosenSkill))
        {
            g_virtualAssignPickerSkillIndex = chosenSkill;
            g_virtualAssignConsumedForPickerSkill = false;
            g_virtualAssignConsumedForPickerSession = false;
            ActivateVirtualAssignMode(chosenSkill, "picker-touch");
        }
        else
        {
            g_virtualAssignPickerSkillIndex = -1;
            DeactivateVirtualAssignMode("picker-touch-nonassignable");
        }

        UpdateVirtualAssignMode();
    }

    ClearVirtualPickerTouch();
    return true;
}

// Everything about the ring in one place: the home in UI units, and the ring,
// knob, dead zone and grab area in device pixels alongside the UI extents they
// come out as on this screen. The pixel figures are what the direction maths and
// the hit test use; the UI extents exist because the draw calls take UI units.
struct VirtualJoystickGeometry
{
    float centerX = kVirtualJoystickDefaultCenterX;
    float centerY = kVirtualJoystickDefaultCenterY;
    float pxPerUiX = 1.0f;
    float pxPerUiY = 1.0f;
    float ringRadiusPx = 0.0f;
    float deadZonePx = 0.0f;
    float grabRadiusPx = 0.0f;
    float ringDiameterUiX = 0.0f;
    float ringDiameterUiY = 0.0f;
    float knobDiameterUiX = 0.0f;
    float knobDiameterUiY = 0.0f;
};

VirtualJoystickGeometry GetVirtualJoystickGeometry()
{
    VirtualJoystickGeometry geometry;

    geometry.pxPerUiX = std::max(static_cast<float>(WindowWidth) / 640.0f, 0.0001f);
    geometry.pxPerUiY = std::max(static_cast<float>(WindowHeight) / 480.0f, 0.0001f);

    // Keep the ring a sane fraction of the panel on screens far from the shape
    // the pixel sizes above were picked on.
    const float shortEdgePx = std::min(static_cast<float>(WindowWidth), static_cast<float>(WindowHeight));
    const float maxDiameterPx = std::max(shortEdgePx * kVirtualJoystickMaxScreenFraction, 1.0f);
    const float scale = std::min(1.0f, maxDiameterPx / kVirtualJoystickRingDiameterPx);

    const float ringDiameterPx = kVirtualJoystickRingDiameterPx * scale;
    const float knobDiameterPx = kVirtualJoystickKnobDiameterPx * scale;

    geometry.ringRadiusPx = ringDiameterPx * 0.5f;
    geometry.deadZonePx = kVirtualJoystickDeadZonePx * scale;
    geometry.grabRadiusPx = geometry.ringRadiusPx + (kVirtualJoystickGrabMarginPx * scale);

    geometry.ringDiameterUiX = ringDiameterPx / geometry.pxPerUiX;
    geometry.ringDiameterUiY = ringDiameterPx / geometry.pxPerUiY;
    geometry.knobDiameterUiX = knobDiameterPx / geometry.pxPerUiX;
    geometry.knobDiameterUiY = knobDiameterPx / geometry.pxPerUiY;

    // Nudge the home in only as far as it takes to keep the whole ring on screen.
    const float ringRadiusUiX = geometry.ringDiameterUiX * 0.5f;
    const float ringRadiusUiY = geometry.ringDiameterUiY * 0.5f;
    const float minX = ringRadiusUiX + 4.0f;
    const float minY = ringRadiusUiY + 4.0f;
    geometry.centerX = std::clamp(kVirtualJoystickDefaultCenterX, minX, std::max(minX, 640.0f - minX));
    geometry.centerY = std::clamp(kVirtualJoystickDefaultCenterY, minY, std::max(minY, kVirtualPadInputMaxY - minY));

    return geometry;
}

float GetVirtualJoystickRenderCenterX()
{
    return std::round(GetVirtualJoystickGeometry().centerX);
}

float GetVirtualJoystickRenderCenterY()
{
    return std::round(GetVirtualJoystickGeometry().centerY);
}

// TEMPORARY diagnostic for the tap behaviour. Writes to mu_joystick_debug.txt in
// the external files dir the process chdir's to at startup, because adb logcat
// comes back empty on these phones. Remove with MU_JOYSTICK_TRACE once the tap
// question is settled.
#define MU_JOYSTICK_TRACE 1

#if defined(MU_JOYSTICK_TRACE)
void JoystickTrace(const char* what)
{
    FILE* f = fopen("mu_joystick_debug.txt", "a");
    if (f == nullptr)
    {
        return;
    }

    if (Hero != nullptr)
    {
        const PATH_t& p = Hero->Path;
        fprintf(f,
            // Run is a float (w_CharacterInfo.h:244). Printing it with %d put it in an
            // FP register with nothing reading it, and shifted every later %d one slot
            // early - which is why the first pull of this log decoded as nonsense.
            "%8u %-10s pos=(%3d,%3d) obj=(%7.1f,%7.1f) tile=(%6.2f,%6.2f) angle=%6.1f mv=%d mt=%d run=%5.1f act=%3d path=%d/%d/%d p0=(%3d,%3d) p1=(%3d,%3d) oct=%d issued=%d hold=%d\n",
            MU_MobileGetTicks(),
            what,
            Hero->PositionX,
            Hero->PositionY,
            Hero->Object.Position[0],
            Hero->Object.Position[1],
            Hero->Object.Position[0] / TERRAIN_SCALE,
            Hero->Object.Position[1] / TERRAIN_SCALE,
            Hero->Object.Angle[2],
            Hero->Movement ? 1 : 0,
            Hero->MovementType,
            Hero->Run,
            Hero->Object.CurrentAction,
            static_cast<int>(p.CurrentPath),
            static_cast<int>(p.CurrentPathFloat),
            static_cast<int>(p.PathNum),
            p.PathNum > 0 ? p.PathX[0] : -1,
            p.PathNum > 0 ? p.PathY[0] : -1,
            p.PathNum > 1 ? p.PathX[1] : -1,
            p.PathNum > 1 ? p.PathY[1] : -1,
            g_virtualJoystick.octant,
            g_virtualJoystick.issuedOctant,
            g_virtualJoystick.issuedAsHold ? 1 : 0);
    }
    else
    {
        fprintf(f, "%8u %-10s (no hero)\n", MU_MobileGetTicks(), what);
    }

    fclose(f);
}
#else
inline void JoystickTrace(const char*) {}
#endif

void ClearVirtualJoystick()
{
    g_virtualJoystick = ActiveVirtualJoystick{};
    g_virtualJoystickHoldingMovement = false;
}

// A thumb's width around the ring, and nothing else. The whole lower-left
// quadrant used to grab the stick, which is why a tap on the ground down there
// walked the character instead of doing what a tap anywhere else does.
bool IsInsideVirtualJoystickDynamicArea(float uiX, float uiY)
{
    if (!IsVirtualPadAvailable())
    {
        return false;
    }

    if (uiY >= kVirtualPadInputMaxY)
    {
        return false;
    }

    if (IsTouchOverInventoryWindow(uiX, uiY))
    {
        return false;
    }

    const VirtualJoystickGeometry geometry = GetVirtualJoystickGeometry();

    // The screen corner behind the stick belongs to the stick. Nobody taps the
    // bottom-left corner to walk somewhere - and a tap there resolves to ground
    // that is always down and to the left of the centred character, so missing the
    // grab circle by a few pixels did not fall through harmlessly, it walked the
    // character the wrong way with conviction.
    if (uiX <= geometry.centerX && uiY >= geometry.centerY)
    {
        return true;
    }

    const float pxX = (uiX - geometry.centerX) * geometry.pxPerUiX;
    const float pxY = (uiY - geometry.centerY) * geometry.pxPerUiY;

    return ((pxX * pxX) + (pxY * pxY)) <= (geometry.grabRadiusPx * geometry.grabRadiusPx);
}

bool HitTestVirtualJoystick(float uiX, float uiY)
{
    return IsInsideVirtualJoystickDynamicArea(uiX, uiY);
}

// Which of the eight headings the thumb is pushing, in world tile space.
//
// CreateAngle measures clockwise from -Y (ZzzAI.cpp), and the click-to-move
// conversion in ZzzInterface.cpp is HeroAngle = -CreateAngle(mouse, hero) + 360
// + 45, which for the main scene's fixed -45 degree camera yaw reduces to
// worldAngle = 225 - screenAngle. Running the push through the same rotation is
// what makes the stick read the screen - push up and the character goes up the
// screen - while the tiles it walks stay in world space.
int SnapVirtualJoystickOctant(float pxX, float pxY, int previousOctant)
{
    const float screenAngle = CreateAngle(0.0f, 0.0f, pxX, pxY);
    const float worldAngle = std::fmod((225.0f - screenAngle) + 720.0f, 360.0f);
    const int snapped = static_cast<int>(std::lround(worldAngle / 45.0f)) & 7;

    if (previousOctant < 0 || snapped == previousOctant)
    {
        return snapped;
    }

    // A thumb resting on a boundary would otherwise flip between two headings
    // every frame, and each flip is a re-path. Hold the heading already being
    // walked until the push is clearly inside the next one.
    const float offCentre = std::fabs(
        std::fmod((worldAngle - (static_cast<float>(previousOctant) * 45.0f)) + 540.0f, 360.0f) - 180.0f);
    return (offCentre > 27.0f) ? snapped : previousOctant;
}

void UpdateVirtualJoystickByUi(float uiX, float uiY)
{
    const VirtualJoystickGeometry geometry = GetVirtualJoystickGeometry();

    // Measured on the glass rather than in UI units: the 640x480 space is
    // stretched differently across and down, so a push that looks like 45 degrees
    // is not 45 degrees in UI units, and the octant it lands in would not be the
    // one the player aimed at.
    float pxX = (uiX - geometry.centerX) * geometry.pxPerUiX;
    float pxY = (uiY - geometry.centerY) * geometry.pxPerUiY;
    const float distPx = std::sqrt((pxX * pxX) + (pxY * pxY));

    if (distPx > geometry.ringRadiusPx && distPx > 0.0001f)
    {
        const float clamp = geometry.ringRadiusPx / distPx;
        pxX *= clamp;
        pxY *= clamp;
    }

    g_virtualJoystick.thumbOffsetX = pxX / geometry.pxPerUiX;
    g_virtualJoystick.thumbOffsetY = pxY / geometry.pxPerUiY;

    if (distPx < geometry.deadZonePx)
    {
        g_virtualJoystick.octant = -1;
        return;
    }

    CancelAndroidAutoMoveForManualInput("joystick");
    g_virtualJoystick.octant = SnapVirtualJoystickOctant(pxX, pxY, g_virtualJoystick.octant);
}

void StartVirtualJoystick(SDL_FingerID fingerId, float uiX, float uiY)
{
    g_virtualJoystick = ActiveVirtualJoystick{};
    g_virtualJoystick.fingerId = fingerId;
    g_virtualJoystick.pressedMs = MU_MobileGetTicks();
    UpdateVirtualJoystickByUi(uiX, uiY);
}

bool HandleVirtualJoystickFingerDown(const SDL_TouchFingerEvent& touch)
{
    float uiX = 0.0f;
    float uiY = 0.0f;
    TouchToVirtualUi(touch, uiX, uiY);

    if (!HitTestVirtualJoystick(uiX, uiY))
    {
        return false;
    }

    if (g_virtualJoystick.fingerId != static_cast<SDL_FingerID>(-1)
        && g_virtualJoystick.fingerId != touch.fingerId)
    {
        return true;
    }

    StartVirtualJoystick(touch.fingerId, uiX, uiY);
    JoystickTrace("DOWN");
    return true;
}

bool HandleVirtualJoystickFingerMotion(const SDL_TouchFingerEvent& touch)
{
    if (!IsVirtualJoystickCaptured(touch.fingerId))
    {
        return false;
    }

    float uiX = 0.0f;
    float uiY = 0.0f;
    TouchToVirtualUi(touch, uiX, uiY);
    UpdateVirtualJoystickByUi(uiX, uiY);
    return true;
}

// Letting go finishes the tile the character is on and stops there, so it never
// halts between tiles.
//
// MovePath ends a path once it reaches node PathNum-1 (ZzzAI.cpp), and while a
// tile is in progress PathX/PathY[CurrentPath+1] is the tile being entered - so
// making that node the last one stops the character exactly there, with the same
// spline finish any other path end gets. Nothing is sent: SendMove transmits the
// path from PathX[0], which is where this walk started rather than where it is
// now, so a resend would replay the whole thing. The server has the longer path
// and will believe the hero went one tile further; the next command corrects it,
// the same way an interrupted click-to-move walk does on the desktop.
void StopVirtualJoystickMovementAtCurrentTile()
{
    if (!g_virtualJoystick.ownsHeroPath)
    {
        return;
    }

    g_virtualJoystick.ownsHeroPath = false;

    if (Hero == nullptr || !Hero->Movement || Hero->MovementType != MOVEMENT_MOVE)
    {
        return;
    }

    PATH_t& path = Hero->Path;
    const int stopAt = static_cast<int>(path.CurrentPath) + 2;
    if (stopAt < static_cast<int>(path.PathNum))
    {
        path.PathNum = static_cast<unsigned char>(stopAt);
    }
}

void ApplyVirtualJoystickMovement();

bool HandleVirtualJoystickFingerUp(const SDL_TouchFingerEvent& touch)
{
    if (!IsVirtualJoystickCaptured(touch.fingerId))
    {
        return false;
    }

    // A tap shorter than a frame would otherwise be swallowed whole: down and up
    // both arrive in the same event pump, so the per-frame driver never sees a
    // finger on the stick and the single tile never gets walked. Issue it here
    // instead. The truncation below is then a no-op - a one tile path is already
    // as short as it can be.
    JoystickTrace("UP");
    if (g_virtualJoystick.issuedOctant < 0 && g_virtualJoystick.octant >= 0)
    {
        ApplyVirtualJoystickMovement();
        JoystickTrace("UP-FLUSH");
    }

    StopVirtualJoystickMovementAtCurrentTile();
    JoystickTrace("UP-TRUNC");
    ClearVirtualJoystick();
    return true;
}

// Is this tile one the character could step onto?
//
// The same test PATH::FindPath applies with its default TW_CHARACTER wall
// (ZzzPath.h): the action, height and camera bits are not obstacles, anything at
// or above TW_CHARACTER is. Asking PathFinding2 instead would answer a different
// question - A* is happy to route around a single blocked tile, so it would
// report a direction with a wall dead ahead as open.
bool IsVirtualJoystickTileOpen(int tileX, int tileY)
{
    if (tileX < 0 || tileX > 255 || tileY < 0 || tileY > 255)
    {
        return false;
    }

    int attribute = TerrainWall[TERRAIN_INDEX_REPEAT(tileX, tileY)];
    if ((attribute & TW_ACTION) == TW_ACTION) attribute -= TW_ACTION;
    if ((attribute & TW_HEIGHT) == TW_HEIGHT) attribute -= TW_HEIGHT;
    if ((attribute & TW_CAMERA_UP) == TW_CAMERA_UP) attribute -= TW_CAMERA_UP;

    return TW_CHARACTER > attribute;
}

// Which of the eight headings the character is walking right now, or -1 if it is
// standing still or on a path that is not a single-tile step.
//
// Read off the path rather than off Object.Angle, which is a float that is part
// way through turning, and rather than off whatever the stick last issued, which
// says nothing about a path some other mover set - or about the previous tap,
// since each press starts with no issued heading of its own.
int GetVirtualJoystickStepOctant(const PATH_t& path, bool movement)
{
    if (!movement || path.PathNum < 2)
    {
        return -1;
    }

    const int current = static_cast<int>(path.CurrentPath);
    if (current < 0 || (current + 1) > (static_cast<int>(path.PathNum) - 1))
    {
        return -1;
    }

    // PathX[CurrentPath] is the tile just left, PathX[CurrentPath+1] the one being
    // entered.
    const int stepX = static_cast<int>(path.PathX[current + 1]) - static_cast<int>(path.PathX[current]);
    const int stepY = static_cast<int>(path.PathY[current + 1]) - static_cast<int>(path.PathY[current]);

    for (int octant = 0; octant < 8; ++octant)
    {
        if (kVirtualJoystickOctantDX[octant] == stepX && kVirtualJoystickOctantDY[octant] == stepY)
        {
            return octant;
        }
    }

    return -1;
}

// Nearest open heading to the one being pushed, so a wall is slid along rather
// than walked into: straight on first, then one step to either side, then two.
int SlideVirtualJoystickOctant(int tileX, int tileY, int octant)
{
    static const int kSearchOrder[5] = { 0, 1, -1, 2, -2 };

    for (int i = 0; i < 5; ++i)
    {
        const int candidate = (octant + kSearchOrder[i] + 8) & 7;
        if (IsVirtualJoystickTileOpen(tileX + kVirtualJoystickOctantDX[candidate],
                                      tileY + kVirtualJoystickOctantDY[candidate]))
        {
            return candidate;
        }
    }

    return -1;
}

// Walk the character by handing it real paths, which is what every other mover in
// the client does: PathFinding2 fills c->Path, MovePath walks it a quarter tile at
// a time, SendMove tells the server.
//
// This used to park the mouse cursor out in the push direction and hold the left
// button down so the desktop click-to-move loop would chase it. That could not do
// what the stick is meant to do. A tap always walked as far as the parked cursor
// projected rather than one tile; a direction change had to wait out
// MouseUpdateTimeMax before the loop would re-path; and the destination was
// whatever tile the cursor happened to land on, not a chosen heading.
void ApplyVirtualJoystickMovement()
{
    if (g_virtualJoystick.fingerId == static_cast<SDL_FingerID>(-1))
    {
        g_virtualJoystickHoldingMovement = false;
        return;
    }

    if (!IsVirtualPadAvailable())
    {
        ClearVirtualJoystick();
        return;
    }

    if (Hero == nullptr || Hero->Object.Live == 0 || Hero->Dead != 0)
    {
        return;
    }

    g_virtualJoystickHoldingMovement = true;

    // Inside the dead zone nothing is being asked for, and whatever tile the
    // character is walking into finishes on its own.
    if (g_virtualJoystick.octant < 0)
    {
        return;
    }

    CHARACTER* c = Hero;
    OBJECT* o = &c->Object;

    // The same predicate the desktop click-to-move branch gates on, rather than a
    // second copy that would drift away from it.
    if (!CanHeroAcceptMoveCommand(o))
    {
        return;
    }

    // SendMove drops the packet outright while a shop or trade window owns the
    // interface. Pathing anyway would walk the character locally with the server
    // never having heard about it.
    if (g_pNewUISystem != nullptr && g_pNewUISystem->IsImpossibleSendMoveInterface())
    {
        return;
    }

    const uint32_t nowMs = MU_MobileGetTicks();
    const bool holding = (nowMs - g_virtualJoystick.pressedMs) >= kVirtualJoystickHoldMs;
    const int octant = g_virtualJoystick.octant;
    const PATH_t& path = c->Path;

    // Which way the live path says the current step is going, and whether the thumb
    // is asking for the exact opposite. Read off the path rather than off this
    // press's own last issue, so it is answerable on the first frame of a press.
    const int stepOctant = GetVirtualJoystickStepOctant(path, c->Movement);
    const bool doublingBack = c->Movement
        && g_virtualJoystick.issuedOctant >= 0
        && stepOctant >= 0
        && (((octant - stepOctant) & 7) == 4);

    // How far the model still has to travel to reach the middle of the tile it is
    // walking into. Tile centres are at tile*TERRAIN_SCALE + half a tile, which the
    // trace confirms: PositionX 176 sits at Position[0] 17650.
    const float destX = (static_cast<float>(c->PositionX) + 0.5f) * TERRAIN_SCALE;
    const float destY = (static_cast<float>(c->PositionY) + 0.5f) * TERRAIN_SCALE;
    const float stepRemaining = std::sqrt(((destX - o->Position[0]) * (destX - o->Position[0]))
        + ((destY - o->Position[1]) * (destY - o->Position[1])));

    // "If you are mid-step you finish that one step" - and until now it did not.
    // PathFinding2 resets CurrentPath and CurrentPathFloat to 0, and MovePath's
    // first waypoint off a fresh path is only ~20% of the way from PathX[0] toward
    // PathX[1], so re-pathing mid-step threw away the progress the model had made
    // and restarted it from a tile it had not reached. Swinging the thumb round
    // re-issued every ~100ms and the character stood still and vibrated: traced at
    // 17 re-issues over 2.7s for 0.7 tiles of travel.
    //
    // So a turn waits for the step boundary. Standing still and doubling straight
    // back are the two exceptions the spec calls out - both turn at once.
    const bool turnCommitted = !c->Movement
        || doublingBack
        || stepRemaining <= kVirtualJoystickTurnCommitPx;

    bool reissue = false;
    if (g_virtualJoystick.issuedOctant < 0)
    {
        // First step of this press.
        reissue = true;
    }
    else if (octant != g_virtualJoystick.issuedOctant)
    {
        // The thumb crossed into another heading. Honour it at the end of the step
        // in progress, not partway through it.
        if (!turnCommitted)
        {
            return;
        }
        reissue = true;
    }
    else if (holding && !g_virtualJoystick.issuedAsHold)
    {
        // The press has turned into a hold. Extend the tap's single tile before it
        // runs out, so the character carries straight on instead of stopping and
        // starting again - SetPlayerStop resets c->Run, and Run has to reach 40
        // before the walk becomes a run (ZzzCharacter.cpp).
        reissue = true;
    }
    else if (holding && static_cast<int>(path.CurrentPath) >= (static_cast<int>(path.PathNum) - 2))
    {
        // Entering the last leg of the current path: top it up.
        reissue = true;
    }
    else if (holding && !c->Movement)
    {
        // The path ended, or the send was refused.
        //
        // Only while holding. Without that, a press that let go before the hold
        // threshold could still hand out a second tile the moment the first one
        // finished - a tile takes long enough at walk speed that a slow tap
        // outlasted it - so a tap walked two tiles, or three.
        reissue = true;
    }

    if (!reissue)
    {
        return;
    }

    if (g_virtualJoystick.issuedOctant >= 0
        && (nowMs - g_virtualJoystick.lastIssueMs) < kVirtualJoystickMinIssueMs)
    {
        return;
    }

    // MovePath assigns c->PositionX/Y = PathX/Y[CurrentPath+1] the moment a tile
    // begins (ZzzAI.cpp), so mid-walk this is the tile being entered and not the
    // one under the character's feet. Pathing from it is what makes a direction
    // change finish the step in progress and turn at its end.
    //
    // Doubling straight back is the exception: path from the tile just left, so the
    // character turns where it stands instead of walking the step out first.
    //
    // Which way it is doubling back from has to be read off the live path rather
    // than off this press's own last issue, or a tap does not count as a reversal
    // at all - a fresh press starts with issuedOctant at -1. Tapping right while a
    // left step was still running then pathed one tile right OF THE LEFT TILE,
    // which is the tile the character had just left: it walked the rest of the way
    // left and came back for no net movement, and only the tap after that went
    // right.
    // Only while a heading has already been issued in this press, which means only
    // while turning under a hold. A tap took this branch too for a while, and it
    // was worse than what it fixed: pathing from behind the character snaps its
    // facing through 180 degrees mid-tile and walks it back the way it came. A tap
    // that reverses now finishes the tile it is entering and walks from there, the
    // same as any other tap.
    int startX = c->PositionX;
    int startY = c->PositionY;
    if (doublingBack && path.PathNum > 0)
    {
        const int current = std::clamp(static_cast<int>(path.CurrentPath), 0, static_cast<int>(path.PathNum) - 1);
        startX = path.PathX[current];
        startY = path.PathY[current];
    }

    // ...but only while PositionX/Y still describes where the character is. It can
    // run away from the model: a server position sync can snap it several tiles at
    // once, and every re-issue while turning leaves it another tile ahead of the
    // object, which never catches up because the next path is rooted on the stale
    // value again. Traced at 4 tiles of drift, and the effect on screen is the
    // character sliding sideways across the map to reach a path start it was never
    // standing on.
    //
    // One tile of lead is the normal, wanted case - that is the tile being entered.
    // More than that is drift, and the tile the model is actually in wins.
    const int objectTileX = static_cast<int>(std::floor(c->Object.Position[0] / TERRAIN_SCALE));
    const int objectTileY = static_cast<int>(std::floor(c->Object.Position[1] / TERRAIN_SCALE));
    const bool drifted = std::abs(objectTileX - startX) > 1 || std::abs(objectTileY - startY) > 1;
    if (drifted && objectTileX >= 0 && objectTileX <= 255 && objectTileY >= 0 && objectTileY <= 255)
    {
        startX = objectTileX;
        startY = objectTileY;
    }

#if defined(MU_JOYSTICK_TRACE)
    if (FILE* f = fopen("mu_joystick_debug.txt", "a"))
    {
        fprintf(f,
            "%8u ISSUE-PRE  oct=%d stepOct=%d back=%d drift=%d hold=%d rem=%5.1f start=(%3d,%3d) pos=(%3d,%3d) objtile=(%3d,%3d) path=%d/%d\n",
            nowMs, octant, stepOctant, doublingBack ? 1 : 0, drifted ? 1 : 0, holding ? 1 : 0,
            stepRemaining, startX, startY, c->PositionX, c->PositionY, objectTileX, objectTileY,
            static_cast<int>(path.CurrentPath), static_cast<int>(path.PathNum));
        fclose(f);
    }
#endif

    const int heading = SlideVirtualJoystickOctant(startX, startY, octant);
    if (heading < 0)
    {
        // Boxed in on every nearby heading.
        return;
    }

    const int steps = holding ? kVirtualJoystickHoldSteps : 1;
    const int stepX = kVirtualJoystickOctantDX[heading];
    const int stepY = kVirtualJoystickOctantDY[heading];

    CancelAndroidAutoMoveForManualInput("joystick");
    g_iFollowCharacter = -1;
    c->MovementType = MOVEMENT_MOVE;

    bool issued = false;
    for (int reach = steps; reach >= 1 && !issued; --reach)
    {
        TargetX = std::clamp(startX + (stepX * reach), 0, 255);
        TargetY = std::clamp(startY + (stepY * reach), 0, 255);

        if (PathFinding2(startX, startY, TargetX, TargetY, &c->Path))
        {
            // Immediately after the query and never on a path that has been
            // touched since: SendMove transmits PathX[0..] on the assumption that
            // index 0 is where the character is standing.
            SendMove(c, o);
            issued = true;
        }
    }

    if (!issued)
    {
        return;
    }

    g_virtualJoystick.issuedOctant = octant;
    g_virtualJoystick.issuedAsHold = holding;
    g_virtualJoystick.lastIssueMs = nowMs;
    g_virtualJoystick.ownsHeroPath = true;

#if defined(MU_JOYSTICK_TRACE)
    if (FILE* f = fopen("mu_joystick_debug.txt", "a"))
    {
        fprintf(f,
            "%8u ISSUE-OK   oct=%d head=%d hold=%d start=(%3d,%3d) target=(%3d,%3d) pos=(%3d,%3d) angle=%6.1f path=%d/%d\n",
            nowMs, octant, heading, holding ? 1 : 0,
            startX, startY, TargetX, TargetY,
            c->PositionX, c->PositionY, o->Angle[2],
            static_cast<int>(path.CurrentPath), static_cast<int>(path.PathNum));
        fclose(f);
    }
#endif
}

#if defined(MU_JOYSTICK_TRACE)
// One line whenever the tile the hero occupies changes, or movement starts or
// stops, so the trace shows the walk itself and not only the commands. Runs from
// UpdateVirtualPadHolds, which is every frame.
void TraceHeroTileChanges()
{
    static int s_lastX = -1;
    static int s_lastY = -1;
    static int s_lastMovement = -1;

    if (Hero == nullptr)
    {
        return;
    }

    const int movement = Hero->Movement ? 1 : 0;
    if (Hero->PositionX == s_lastX && Hero->PositionY == s_lastY && movement == s_lastMovement)
    {
        return;
    }

    const bool tileChanged = (Hero->PositionX != s_lastX) || (Hero->PositionY != s_lastY);
    s_lastX = Hero->PositionX;
    s_lastY = Hero->PositionY;
    s_lastMovement = movement;

    JoystickTrace(tileChanged ? (movement ? "TILE-ENTER" : "TILE-STOP") : (movement ? "MOVE-ON" : "MOVE-OFF"));
}
#endif

bool IsMiniMapToggleAvailable()
{
    // Permanently disabled: the old circular "MINI" button (which opened the
    // static, non-rotating full-screen minimap popup) is removed now that the
    // always-on rotating panel (GetTopBarMiniMapPanelRect) replaces it. Every
    // render/hit-test site below gates on this one function, same pattern as
    // HitTestMapButton's hardcoded false just below.
    return false;
}

// 鑺掗垾婵冨亾鑺掗垾婵冨亾 Map button hit test 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
bool HitTestMapButton(float uiX, float uiY)
{
    (void)uiX;
    (void)uiY;
    return false;
}

void ToggleMapListByVirtualButton()
{
    if (g_pNewUISystem == nullptr)
        return;
    if (g_androidTradePicker.autoMoving)
        return;
    g_pNewUISystem->Toggle(SEASON3B::INTERFACE_MOVEMAP);
    LOGI("VirtualPad: map list toggled");
}

bool CanAndroidUseFriendFeature()
{
    if (CharacterAttribute != nullptr && CharacterAttribute->Level < 6)
    {
        if (g_pChatListBox != nullptr
            && g_pChatListBox->CheckChatRedundancy(GlobalText[1067]) == FALSE)
        {
            g_pChatListBox->AddText("", GlobalText[1067], SEASON3B::TYPE_SYSTEM_MESSAGE);
        }
        return false;
    }

    return true;
}

void ToggleFriendListByVirtualButton()
{
    if (g_pNewUISystem == nullptr || !CanAndroidUseFriendFeature())
    {
        return;
    }

    g_pNewUISystem->Toggle(SEASON3B::INTERFACE_FRIEND);
    LOGI("VirtualPad: friend list toggled");
}

bool HitTestMiniMapToggleButton(float uiX, float uiY)
{
    if (!IsMiniMapToggleAvailable())
    {
        return false;
    }

    const AndroidUiRect rect = GetMiniMapButtonRect();
    return uiX >= rect.x
        && uiX <= (rect.x + rect.w)
        && uiY >= rect.y
        && uiY <= (rect.y + rect.h);
}

bool IsVirtualUtilityButtonsAvailable();

bool HitTestVirtualChatUtilityButton(float uiX, float uiY)
{
    if (!kShowVirtualChatQuickButton)
    {
        return false;
    }

    if (!IsVirtualUtilityButtonsAvailable())
    {
        return false;
    }

    const AndroidUiRect rect = GetVirtualUtilityButtonRect(kVirtualUtilityButtonChat);
    return uiX >= rect.x
        && uiX <= (rect.x + rect.w)
        && uiY >= rect.y
        && uiY <= (rect.y + rect.h);
}

bool IsVirtualUtilityButtonsAvailable()
{
    return SceneFlag == MAIN_SCENE
        && g_pNewUISystem != nullptr
        && !AndroidHasFocusedTextInput();
}

int HitTestVirtualUtilityButton(float uiX, float uiY)
{
    if (!IsVirtualUtilityButtonsAvailable())
    {
        return -1;
    }

    for (int i = 0; i < kVirtualUtilityButtonCount; ++i)
    {
        const AndroidUiRect rect = GetVirtualUtilityButtonRect(i);
        if (uiX >= rect.x && uiX <= (rect.x + rect.w)
            && uiY >= rect.y && uiY <= (rect.y + rect.h))
        {
            return i;
        }
    }

    return -1;
}

bool IsVirtualUtilityButtonActive(int button)
{
    if (g_pNewUISystem == nullptr)
    {
        return false;
    }

    switch (button)
    {
    case 0: return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY);
    case 1: return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHARACTER);
    case 2: return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION);
    case 3: return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX);
    default: return false;
    }
}

void ToggleVirtualUtilityButton(int button)
{
    if (g_pNewUISystem == nullptr)
    {
        return;
    }

    switch (button)
    {
    case 0:
        g_pNewUISystem->Toggle(SEASON3B::INTERFACE_INVENTORY);
        LOGI("VirtualPad: utility toggle -> inventory");
        break;
    case 1:
        g_pNewUISystem->Toggle(SEASON3B::INTERFACE_CHARACTER);
        LOGI("VirtualPad: utility toggle -> character");
        break;
    case 2:
        g_pNewUISystem->Toggle(SEASON3B::INTERFACE_OPTION);
        LOGI("VirtualPad: utility toggle -> option");
        break;
    case 3:
        ToggleVirtualChatInputBox();
        LOGI("VirtualPad: utility toggle -> chat");
        break;
    default:
        break;
    }
}

void ToggleMiniMapByVirtualButton()
{
    if (g_pNewUISystem == nullptr)
    {
        return;
    }
    if (g_androidTradePicker.autoMoving)
    {
        return;
    }

    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MINI_MAP))
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_MINI_MAP);
        LOGI("VirtualPad: minimap toggle -> hide");
    }
    else
    {
        g_pNewUISystem->Toggle(SEASON3B::INTERFACE_MINI_MAP);
        LOGI("VirtualPad: minimap toggle -> show");
    }
}

// ---------------------------------------------------------------------------
// Two-finger pinch zoom, replacing the old on-screen +/- buttons.
// ---------------------------------------------------------------------------

// Physical pixels, not UI units: pinch distance has to be measured in what the
// fingers actually travel. UI space is 640x480 stretched onto a panel that is
// rarely 4:3, so the same gap would read differently horizontally and
// vertically if it were measured there.
void TouchToScreenPx(const SDL_TouchFingerEvent& touch, float& outX, float& outY)
{
    outX = std::clamp(touch.x, 0.0f, 1.0f) * static_cast<float>(WindowWidth);
    outY = std::clamp(touch.y, 0.0f, 1.0f) * static_cast<float>(WindowHeight);
}

float GetCurrentAndroidZoom()
{
    float zoom = (g_androidZoomOverride > 0.0f) ? g_androidZoomOverride : CameraDistanceTarget;
    if (zoom <= 0.0f)
    {
        zoom = kZoomDefault;
    }
    return std::clamp(zoom, kZoomMin, kZoomMax);
}

float GetPinchGap()
{
    const float dx = g_androidPinch.bx - g_androidPinch.ax;
    const float dy = g_androidPinch.by - g_androidPinch.ay;
    return std::sqrt((dx * dx) + (dy * dy));
}

void ClearAndroidPinch()
{
    g_androidPinch = AndroidPinchZoomState{};
}

bool IsAndroidPinchActive()
{
    return g_androidPinch.active;
}

// Called for every finger that goes down. Returns true only once the pinch has
// actually begun, so a normal single touch falls through untouched.
bool HandleAndroidPinchFingerDown(const SDL_TouchFingerEvent& touch)
{
    if (!IsVirtualPadAvailable())
    {
        return false;
    }

    float px = 0.0f, py = 0.0f;
    TouchToScreenPx(touch, px, py);

    if (g_androidPinch.fingerA == static_cast<SDL_FingerID>(-1))
    {
        g_androidPinch.fingerA = touch.fingerId;
        g_androidPinch.ax = px;
        g_androidPinch.ay = py;
        return false;
    }

    if (g_androidPinch.fingerB == static_cast<SDL_FingerID>(-1)
        && touch.fingerId != g_androidPinch.fingerA)
    {
        g_androidPinch.fingerB = touch.fingerId;
        g_androidPinch.bx = px;
        g_androidPinch.by = py;
        g_androidPinch.startGap = GetPinchGap();
        g_androidPinch.startZoom = GetCurrentAndroidZoom();
        g_androidPinch.active = true;

        // The first finger was almost certainly already driving the joystick,
        // and the character should not keep running through a pinch.
        ClearVirtualJoystick();
        return true;
    }

    return false;
}

bool HandleAndroidPinchFingerMotion(const SDL_TouchFingerEvent& touch)
{
    if (touch.fingerId != g_androidPinch.fingerA && touch.fingerId != g_androidPinch.fingerB)
    {
        return false;
    }

    float px = 0.0f, py = 0.0f;
    TouchToScreenPx(touch, px, py);
    if (touch.fingerId == g_androidPinch.fingerA)
    {
        g_androidPinch.ax = px;
        g_androidPinch.ay = py;
    }
    else
    {
        g_androidPinch.bx = px;
        g_androidPinch.by = py;
    }

    if (!g_androidPinch.active)
    {
        // One finger down: not our gesture, let the joystick have the motion.
        return false;
    }

    const float gap = GetPinchGap();
    if (g_androidPinch.startGap <= 1.0f)
    {
        return true;
    }
    if (std::fabs(gap - g_androidPinch.startGap) < kPinchActivateSlopPx)
    {
        return true;
    }

    // Fingers apart = zoom in = camera closer, so the ratio is inverted.
    // Scaling from the gesture's own start rather than the previous frame keeps
    // it absolute: put the fingers back where they began and the zoom returns
    // exactly to where it began.
    const float scale = g_androidPinch.startGap / std::max(gap, 1.0f);
    const float nextZoom = std::clamp(g_androidPinch.startZoom * scale, kZoomMin, kZoomMax);

    g_androidZoomOverride = nextZoom;
    CameraDistanceTarget = nextZoom;
    return true;
}

bool HandleAndroidPinchFingerUp(const SDL_TouchFingerEvent& touch)
{
    if (touch.fingerId != g_androidPinch.fingerA && touch.fingerId != g_androidPinch.fingerB)
    {
        return false;
    }

    const bool wasActive = g_androidPinch.active;

    // Promote the surviving finger to A so lifting one of two does not leave a
    // stale slot behind, and a third pinch can start cleanly.
    if (touch.fingerId == g_androidPinch.fingerA)
    {
        g_androidPinch.fingerA = g_androidPinch.fingerB;
        g_androidPinch.ax = g_androidPinch.bx;
        g_androidPinch.ay = g_androidPinch.by;
    }
    g_androidPinch.fingerB = static_cast<SDL_FingerID>(-1);
    g_androidPinch.active = false;
    g_androidPinch.startGap = 0.0f;
    g_androidPinch.startZoom = 0.0f;

    // Swallow the lift only if it ended a real pinch; otherwise the normal
    // release path still needs to see it.
    return wasActive;
}

bool HitTestVirtualCurrentSkillBox(float uiX, float uiY);
void ToggleVirtualSkillPickerByTouch();

bool HandleVirtualTopControlTap(float uiX, float uiY)
{
    const uint32_t nowMs = MU_MobileGetTicks();

    if (HitTestMiniMapToggleButton(uiX, uiY))
    {
        if ((nowMs - g_virtualLastMiniMapTapMs) >= kVirtualMiniMapButtonCooldownMs)
        {
            g_virtualLastMiniMapTapMs = nowMs;
            ToggleMiniMapByVirtualButton();
        }

        return true;
    }

    if (HitTestMapButton(uiX, uiY))
    {
        if ((nowMs - g_virtualLastMiniMapTapMs) >= kVirtualMiniMapButtonCooldownMs)
        {
            g_virtualLastMiniMapTapMs = nowMs;
            ToggleMapListByVirtualButton();
        }

        return true;
    }

    if (HitTestVirtualChatUtilityButton(uiX, uiY))
    {
        if ((nowMs - g_virtualLastUtilityTapMs) >= kVirtualUtilityButtonCooldownMs)
        {
            g_virtualLastUtilityTapMs = nowMs;
            ToggleVirtualChatInputBox();
        }

        return true;
    }

    if (kUseLegacyMainHud)
    {
        return false;
    }

    if (HitTestVirtualCurrentSkillBox(uiX, uiY))
    {
        ToggleVirtualSkillPickerByTouch();
        return true;
    }

    return false;
}

bool HitTestVirtualCurrentSkillBox(float uiX, float uiY)
{
    if (!kShowVirtualCurrentSkillBox)
    {
        return false;
    }

    if (!IsVirtualPadAvailable())
    {
        return false;
    }

    return uiX >= kVirtualCurrentSkillBoxX
        && uiX <= (kVirtualCurrentSkillBoxX + kVirtualCurrentSkillBoxW)
        && uiY >= kVirtualCurrentSkillBoxY
        && uiY <= (kVirtualCurrentSkillBoxY + kVirtualCurrentSkillBoxH);
}

void ToggleVirtualSkillPickerByTouch()
{
    if (g_pSkillList == nullptr || CharacterAttribute == nullptr)
    {
        return;
    }

    if (CharacterAttribute->SkillNumber <= 0 && CharacterAttribute->SkillMasterNumber <= 0)
    {
        return;
    }

    if (!g_pSkillList->IsSkillPickerOpen())
    {
        // Each picker open starts a fresh "pick one skill -> assign one slot" flow.
        g_pSkillList->SetAndroidTouchAssignSkillIndex(-1);
        g_virtualAssignPickerSkillIndex = -1;
        DeactivateVirtualAssignMode("picker-reset");
    }
    ClearVirtualPickerTouch();

    g_pSkillList->ToggleSkillPicker();
    UpdateVirtualAssignMode();
}

int HitTestVirtualButton(float uiX, float uiY)
{
    if (!IsVirtualPadAvailable())
    {
        return -1;
    }

    int bestButton = -1;
    float bestNormDist = 1000.0f;

    for (int i = 0; i < static_cast<int>(kVirtualButtons.size()); ++i)
    {
        const float dx = uiX - kVirtualButtons[i].cx;
        const float dy = uiY - kVirtualButtons[i].cy;
        const float hitRadius = GetVirtualButtonHitRadius(i);
        const float r2 = hitRadius * hitRadius;
        const float d2 = (dx * dx) + (dy * dy);
        if (d2 <= r2)
        {
            const float norm = d2 / std::max(r2, 1.0f);
            if (norm < bestNormDist)
            {
                bestNormDist = norm;
                bestButton = i;
            }
        }
    }

    // Keep bottom action bar touches for original UI unless the touch landed
    // directly on one of the virtual combat buttons above.
    if (uiY >= kVirtualPadInputMaxY && bestButton < 0)
    {
        return -1;
    }

    return bestButton;
}

// 鑺掗垾婵冨亾鑺掗垾婵冨亾 Consumable slot helpers 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾

// Returns the lineal slot position (y*8 + x + offset) needed by SendRequestUse/FindItem.
// Uses g_pMyInventory which is the authoritative item store (NOT the legacy Inventory[] array).
int FindConsumableInInventory(int itemType, int itemLevel)
{
    if (itemType < 0 || g_pMyInventory == nullptr)
        return -1;
    const short sType = static_cast<short>(itemType);
    // Try exact level match first; fall back to any-level (-1) for robustness.
    int idx = g_pMyInventory->FindItemReverseIndex(sType, itemLevel);
    if (idx < 0)
        idx = g_pMyInventory->FindItemReverseIndex(sType, -1);
    return idx;
}

int HitTestVirtualConsumableSlot(float uiX, float uiY)
{
    if (!IsVirtualPadAvailable())
        return -1;
    // Use a larger touch radius than the visual radius so fat fingers can tap easily.
    constexpr float kTouchRadius = 24.0f;  // visual radius is 14; this gives ~50% more hit area
    for (int i = 0; i < kVirtualConsumableSlotCount; ++i)
    {
        const float dx = uiX - kVirtualConsumableSlots[i].cx;
        const float dy = uiY - kVirtualConsumableSlots[i].cy;
        if ((dx * dx + dy * dy) <= (kTouchRadius * kTouchRadius))
            return i;
    }
    if (uiY >= kVirtualPadInputMaxY)
        return -1;
    return -1;
}

bool TryBindPickedItemToVirtualConsumableSlot(int slot)
{
    if (slot < 0 || slot >= kVirtualConsumableSlotCount)
    {
        return false;
    }

    auto* picked = SEASON3B::CNewUIInventoryCtrl::GetPickedItem();
    if (picked == nullptr)
    {
        return false;
    }

    ITEM* pickedItem = picked->GetItem();
    if (pickedItem == nullptr)
    {
        return false;
    }

    const bool isBindableConsumable = false;
    if (!isBindableConsumable)
    {
        return false;
    }

    // Keep one binding per item type/level across the 3 consumable slots.
    for (int i = 0; i < kVirtualConsumableSlotCount; ++i)
    {
        if (i == slot)
        {
            continue;
        }

        if (g_virtualConsumableSlots[i].itemType == pickedItem->Type
            && g_virtualConsumableSlots[i].itemLevel == pickedItem->Level)
        {
            g_virtualConsumableSlots[i] = VirtualConsumableSlot{};
        }
    }

    g_virtualConsumableSlots[slot].itemType = pickedItem->Type;
    g_virtualConsumableSlots[slot].itemLevel = pickedItem->Level;

    SEASON3B::CNewUIInventoryCtrl::BackupPickedItem();

    MouseLButton = false;
    MouseLButtonPush = false;
    MouseLButtonPop = false;
    MouseLButtonDBClick = false;
    return true;
}

void UseVirtualConsumableSlot(int slot)
{
    if (slot < 0 || slot >= kVirtualConsumableSlotCount)
        return;
    const VirtualConsumableSlot& cs = g_virtualConsumableSlots[slot];
    if (cs.itemType < 0)
        return;
    const int idx = FindConsumableInInventory(cs.itemType, cs.itemLevel);
    if (idx >= 0)
        (void)idx;
    else
        g_virtualConsumableSlots[slot] = VirtualConsumableSlot{};  // item gone 鑺掗埀顑解偓?clear slot
}

int FindActiveVirtualTouchSlot(SDL_FingerID fingerId)
{
    for (int i = 0; i < static_cast<int>(g_activeVirtualTouches.size()); ++i)
    {
        if (g_activeVirtualTouches[i].fingerId == fingerId)
        {
            return i;
        }
    }

    return -1;
}

int AcquireActiveVirtualTouchSlot(SDL_FingerID fingerId)
{
    const int existing = FindActiveVirtualTouchSlot(fingerId);
    if (existing >= 0)
    {
        return existing;
    }

    for (int i = 0; i < static_cast<int>(g_activeVirtualTouches.size()); ++i)
    {
        if (g_activeVirtualTouches[i].fingerId == static_cast<SDL_FingerID>(-1))
        {
            return i;
        }
    }

    return -1;
}

void StopVirtualNormalAttackHold()
{
    Attacking = -1;
    ActionTarget = -1;
    MouseRButton = false;
    MouseRButtonPush = false;
}

void ReleaseVirtualNovaCharge();   // defined next to TriggerVirtualCombat below

void ClearActiveVirtualTouchSlot(int slot)
{
    if (slot < 0 || slot >= static_cast<int>(g_activeVirtualTouches.size()))
    {
        return;
    }

    if (g_activeVirtualTouches[slot].button == kVirtualAttackButton)
    {
        StopVirtualNormalAttackHold();
    }
    else if (g_activeVirtualTouches[slot].button >= kVirtualSkillButtonBase)
    {
        // Finger lifted off a skill button - fire the Nova release if this
        // press started a charge. No-op for every other skill.
        ReleaseVirtualNovaCharge();
    }

    g_activeVirtualTouches[slot] = ActiveVirtualTouch{};
}

bool IsValidAutoCombatTarget(int characterIndex)
{
    if (!IsVirtualPadAvailable()
        || CharactersClient == nullptr
        || characterIndex < 0
        || characterIndex >= MAX_CHARACTERS_CLIENT)
    {
        return false;
    }

    // Reject self-attack: check both by pointer AND by index.
    // Hero pointer can move to a random slot (HeroIndex = rand()),
    // so we must use GetHeroCharacterIndex() as the authoritative check.
    const int heroIdx = GetHeroCharacterIndex();
    CHARACTER* target = &CharactersClient[characterIndex];
    if (target == nullptr || target == Hero || characterIndex == heroIdx)
    {
        return false;
    }

    if (target->Dead > 0
        || !target->Object.Live
        || target->Object.HiddenMesh == -2
        || target->Object.Alpha <= 0.05f)
    {
        return false;
    }

    const int kind = target->Object.Kind;
    if (kind != KIND_MONSTER && kind != KIND_PLAYER)
    {
        return false;
    }

    return true;
}

// MU Helper rewrites SelectedCharacter from its own scan on a 250ms tick, so a
// manual lock and the bot would pull against each other every frame. The bot
// wins: it is an explicit "play for me" mode, and a lock the player cannot see
// taking effect is worse than no lock at all.
bool IsAndroidMuHelperRunning()
{
    if (g_pNewUISystem == nullptr)
    {
        return false;
    }

    SEASON3B::CNewUIMuHelper* helper = g_pNewUISystem->Get_pNewUIMuHelper();
    return helper != nullptr && helper->DataAutoMu.Started;
}

// Start/stop the helper without opening its window, for the top bar play
// button. StartMuHelper takes the *current* state - 0 starts, 1 stops - so
// handing it the running flag flips it, exactly as the desktop HOME key does
// (NewUIHotKey.cpp). It refuses to start with its own message box while the
// helper window is open or the hero is in a safe zone, so there is nothing to
// validate here.
void ToggleAndroidMuHelperRunning()
{
    if (g_pNewUISystem == nullptr)
    {
        return;
    }

    SEASON3B::CNewUIMuHelper* helper = g_pNewUISystem->Get_pNewUIMuHelper();
    if (helper == nullptr)
    {
        return;
    }

    StartMuHelper(helper->DataAutoMu.Started ? 1 : 0);
    PlayBuffer(SOUND_CLICK01);
}

void ClearAndroidTargetLock(const char* reason)
{
    if (!g_androidTargetLock.active)
    {
        return;
    }

    LOGI("TargetLock: cleared (%s) target=%s", reason != nullptr ? reason : "?", g_androidTargetLock.id);

    g_androidTargetLock = AndroidTargetLock{};
}

// Returns the locked character's current index, or -1 once it is gone. Clears
// itself in the latter case so a dead target does not keep being looked up.
int ResolveAndroidLockedTargetIndex()
{
    if (!g_androidTargetLock.active)
    {
        return -1;
    }

    // Checked on resolve rather than only when the helper starts, so it also
    // covers the helper being started by the server or by a hotkey.
    if (IsAndroidMuHelperRunning())
    {
        ClearAndroidTargetLock("mu-helper-running");
        return -1;
    }

    const int cached = g_androidTargetLock.cachedIndex;
    if (cached >= 0
        && cached < MAX_CHARACTERS_CLIENT
        && CharactersClient != nullptr
        && CharactersClient[cached].Key == g_androidTargetLock.key
        && IsValidAutoCombatTarget(cached))
    {
        return cached;
    }

    const int resolved = FindCharacterIndex(g_androidTargetLock.key);
    if (resolved >= 0 && resolved < MAX_CHARACTERS_CLIENT && IsValidAutoCombatTarget(resolved))
    {
        g_androidTargetLock.cachedIndex = resolved;
        return resolved;
    }

    ClearAndroidTargetLock("target-gone");
    return -1;
}

bool IsAndroidTargetLockActive()
{
    return ResolveAndroidLockedTargetIndex() >= 0;
}

void SetAndroidTargetLock(int characterIndex)
{
    if (!IsValidAutoCombatTarget(characterIndex) || IsAndroidMuHelperRunning())
    {
        return;
    }

    const CHARACTER* target = &CharactersClient[characterIndex];

    g_androidTargetLock.active = true;
    g_androidTargetLock.key = target->Key;
    g_androidTargetLock.cachedIndex = characterIndex;

    memcpy(g_androidTargetLock.id, target->ID, MAX_ID_SIZE);
    g_androidTargetLock.id[MAX_ID_SIZE] = '\0';

    LOGI("TargetLock: set target=%s key=%d index=%d",
         g_androidTargetLock.id,
         static_cast<int>(g_androidTargetLock.key),
         characterIndex);
}

//========================= select-target picker =============================

// Defined below, alongside the other targeting predicates.
bool IsVirtualPkTargetingEnabled();

int GetAndroidTargetPickerRowCount()
{
    if (g_androidTargetPicker.entryCount <= 0)
    {
        return 1;
    }

    return std::min(g_androidTargetPicker.entryCount, kTargetPickerVisibleRows);
}

AndroidUiRect GetAndroidTargetPickerRect()
{
    return {
        kTargetPickerX,
        kTargetPickerY,
        kTargetPickerW,
        kTargetPickerHeaderH
            + (static_cast<float>(GetAndroidTargetPickerRowCount()) * kTargetPickerRowH)
            + kTargetPickerFooterH
    };
}

AndroidUiRect GetAndroidTargetPickerRowRect(int row)
{
    return {
        kTargetPickerX + 6.0f,
        kTargetPickerY + kTargetPickerHeaderH + (static_cast<float>(row) * kTargetPickerRowH),
        kTargetPickerW - 12.0f,
        kTargetPickerRowH - 2.0f
    };
}

AndroidUiRect GetAndroidTargetPickerFooterRect()
{
    const AndroidUiRect rect = GetAndroidTargetPickerRect();
    return {
        rect.x + 6.0f,
        rect.y + rect.h - kTargetPickerFooterH + 2.0f,
        rect.w - 12.0f,
        kTargetPickerFooterH - 6.0f
    };
}

AndroidUiRect GetComboToggleRect()
{
    return { kComboToggleX, kComboToggleY, kComboToggleW, kComboToggleH };
}

// Defined further down, next to the tab rendering.
bool HandleAndroidChatTabTap(float uiX, float uiY);
bool HandleAndroidChatLogTap(float uiX, float uiY);

AndroidUiRect GetChatTabRect(int tab)
{
    if (tab < 0 || tab >= kChatTabCount)
    {
        return {};
    }

    return {
        kChatTabsX + static_cast<float>(tab) * (kChatTabW + kChatTabGap),
        kChatTabsY,
        kChatTabW,
        kChatTabH
    };
}

AndroidUiRect GetTargetSelectButtonRect()
{
    return {
        kTargetSelectButtonCx - kTargetSelectButtonRadius,
        kTargetSelectButtonCy - kTargetSelectButtonRadius,
        kTargetSelectButtonRadius * 2.0f,
        kTargetSelectButtonRadius * 2.0f
    };
}

void ClampAndroidTargetPickerScroll()
{
    const int maxOffset = std::max(0, g_androidTargetPicker.entryCount - kTargetPickerVisibleRows);
    g_androidTargetPicker.scrollOffset = std::clamp(g_androidTargetPicker.scrollOffset, 0, maxOffset);
}

bool IsAndroidTargetPickerCandidate(int characterIndex)
{
    if (!IsValidAutoCombatTarget(characterIndex) || Hero == nullptr)
    {
        return false;
    }

    const CHARACTER* c = &CharactersClient[characterIndex];

    // Other players only count when PK targeting is on, so the list does not
    // offer targets an attack would refuse to hit.
    if (c->Object.Kind == KIND_PLAYER && !IsVirtualPkTargetingEnabled())
    {
        return false;
    }

    const int dx = c->PositionX - Hero->PositionX;
    const int dy = c->PositionY - Hero->PositionY;
    const int dist2 = (dx * dx) + (dy * dy);

    return dist2 <= static_cast<int>(kTargetPickerRangeTiles * kTargetPickerRangeTiles);
}

// Nearest first. Insertion into a fixed array rather than push_back plus sort:
// this runs while the panel is open and 16 entries makes the shift trivial,
// with no per-refresh allocation.
void RefreshAndroidTargetPickerEntries(bool force)
{
    const uint32_t nowMs = MU_MobileGetTicks();
    if (!force && (nowMs - g_androidTargetPicker.lastRefreshMs) < kTargetPickerRefreshMs)
    {
        return;
    }
    g_androidTargetPicker.lastRefreshMs = nowMs;

    g_androidTargetPicker.entryCount = 0;

    if (CharactersClient == nullptr || Hero == nullptr)
    {
        return;
    }

    for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
    {
        if (!IsAndroidTargetPickerCandidate(i))
        {
            continue;
        }

        const CHARACTER* c = &CharactersClient[i];
        const int dx = c->PositionX - Hero->PositionX;
        const int dy = c->PositionY - Hero->PositionY;

        AndroidTargetPickerEntry entry;
        entry.characterIndex = i;
        entry.key = c->Key;
        entry.distance2 = (dx * dx) + (dy * dy);
        memcpy(entry.id, c->ID, MAX_ID_SIZE);
        entry.id[MAX_ID_SIZE] = '\0';

        int pos = g_androidTargetPicker.entryCount;
        while (pos > 0 && g_androidTargetPicker.entries[pos - 1].distance2 > entry.distance2)
        {
            if (pos < kTargetPickerMaxEntries)
            {
                g_androidTargetPicker.entries[pos] = g_androidTargetPicker.entries[pos - 1];
            }
            --pos;
        }

        if (pos < kTargetPickerMaxEntries)
        {
            g_androidTargetPicker.entries[pos] = entry;
            if (g_androidTargetPicker.entryCount < kTargetPickerMaxEntries)
            {
                ++g_androidTargetPicker.entryCount;
            }
        }
    }

    ClampAndroidTargetPickerScroll();
}

void HideAndroidTargetPicker()
{
    g_androidTargetPicker.visible = false;
    g_androidTargetPicker.dragging = false;
    g_androidTargetPicker.dragMoved = false;
    g_androidTargetPicker.dragFingerId = static_cast<SDL_FingerID>(-1);
    g_androidTargetPicker.pressedKey = 0;
}

void ShowAndroidTargetPicker()
{
    g_androidTargetPicker.visible = true;
    g_androidTargetPicker.scrollOffset = 0;
    RefreshAndroidTargetPickerEntries(true);
}

bool IsTargetAttackable(int characterIndex)
{
    if (!IsValidAutoCombatTarget(characterIndex))
    {
        return false;
    }

    const int previousSelection = SelectedCharacter;
    SelectedCharacter = characterIndex;
    const bool canAttack = CheckAttack();
    SelectedCharacter = previousSelection;
    return canAttack;
}

bool IsVirtualPkTargetingEnabled()
{
    return g_pBCustomMenuInfo != nullptr && g_pBCustomMenuInfo->AutoCtrlPK;
}

int FindNearestTargetByKind(int objectKind, bool requireAttackable, bool requireVisible, bool limitToAcquireRange)
{
    if (!IsVirtualPadAvailable() || CharactersClient == nullptr)
    {
        return -1;
    }

    int bestIndex = -1;
    float bestDist2 = std::numeric_limits<float>::max();

    for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
    {
        CHARACTER* c = &CharactersClient[i];
        if (!IsValidAutoCombatTarget(i) || c->Object.Kind != objectKind)
        {
            continue;
        }

        if (requireVisible && !c->Object.Visible)
        {
            continue;
        }

        if (limitToAcquireRange && !IsWithinVirtualAutoAcquireRange(i))
        {
            continue;
        }

        if (requireAttackable && !IsTargetAttackable(i))
        {
            continue;
        }

        const float dx = static_cast<float>(c->PositionX - Hero->PositionX);
        const float dy = static_cast<float>(c->PositionY - Hero->PositionY);
        const float dist2 = (dx * dx) + (dy * dy);

        if (bestIndex < 0 || dist2 < bestDist2)
        {
            bestIndex = i;
            bestDist2 = dist2;
        }
    }

    return bestIndex;
}

int FindNearestAttackablePlayerTarget(bool limitToAcquireRange)
{
    return FindNearestTargetByKind(KIND_PLAYER, true, false, limitToAcquireRange);
}

int FindNearestVisiblePlayerTarget(bool requireAttackable)
{
    return FindNearestTargetByKind(KIND_PLAYER, requireAttackable, true, false);
}

int FindNearestPlayerTarget(bool limitToAcquireRange)
{
    return FindNearestTargetByKind(KIND_PLAYER, false, false, limitToAcquireRange);
}

int FindNearestAttackableMonsterTarget(bool limitToAcquireRange);
int FindNearestMonsterTarget(bool limitToAcquireRange);

int FindNearestAttackableTarget()
{
    if (IsVirtualPkTargetingEnabled())
    {
        const int playerTarget = FindNearestAttackablePlayerTarget(false);
        if (playerTarget >= 0)
        {
            return playerTarget;
        }
    }

    const int monsterTarget = FindNearestAttackableMonsterTarget(false);
    if (monsterTarget >= 0)
    {
        return monsterTarget;
    }

    if (!IsVirtualPkTargetingEnabled())
    {
        return FindNearestAttackablePlayerTarget(false);
    }

    return -1;
}

int FindNearestAttackableMonsterTarget(bool limitToAcquireRange)
{
    return FindNearestTargetByKind(KIND_MONSTER, true, false, limitToAcquireRange);
}

int FindNearestVisibleMonsterTarget(bool requireAttackable)
{
    return FindNearestTargetByKind(KIND_MONSTER, requireAttackable, true, false);
}

int FindNearestMonsterTarget(bool limitToAcquireRange)
{
    return FindNearestTargetByKind(KIND_MONSTER, false, false, limitToAcquireRange);
}

int GetHeroCharacterIndex()
{
    if (!IsVirtualPadAvailable() || CharactersClient == nullptr || Hero == nullptr)
    {
        return -1;
    }

    if (Hero < &CharactersClient[0] || Hero >= (&CharactersClient[0] + MAX_CHARACTERS_CLIENT))
    {
        return -1;
    }

    return static_cast<int>(Hero - &CharactersClient[0]);
}

void EnsureCombatTarget()
{
    if (!IsVirtualPadAvailable())
    {
        return;
    }

    // An explicit lock outranks proximity. This has to come before the
    // IsTargetAttackable early-out, or a stale SelectedCharacter that happens
    // to still be valid would win over the player's actual choice.
    const int lockedTarget = ResolveAndroidLockedTargetIndex();
    if (lockedTarget >= 0)
    {
        SelectedCharacter = lockedTarget;
        return;
    }

    if (IsTargetAttackable(SelectedCharacter))
    {
        return;
    }

    const int nearest = FindNearestAttackableTarget();
    if (nearest >= 0)
    {
        SelectedCharacter = nearest;
    }
    else
    {
        SelectedCharacter = -1;
    }
}

void EnsureOffensiveSkillTarget()
{
    if (!IsVirtualPadAvailable() || CharactersClient == nullptr)
    {
        return;
    }

    const int heroIdx = GetHeroCharacterIndex();
    if (heroIdx >= 0 && SelectedCharacter == heroIdx)
    {
        SelectedCharacter = -1;
    }

    // After the hero-self sanitize above, but before the IsTargetAttackable
    // early-out below, for the same reason as in EnsureCombatTarget.
    const int lockedTarget = ResolveAndroidLockedTargetIndex();
    if (lockedTarget >= 0)
    {
        SelectedCharacter = lockedTarget;
        return;
    }

    if (IsTargetAttackable(SelectedCharacter))
    {
        return;
    }

    if (IsVirtualPkTargetingEnabled())
    {
        const int nearestVisibleAttackablePlayer = FindNearestVisiblePlayerTarget(true);
        if (nearestVisibleAttackablePlayer >= 0)
        {
            SelectedCharacter = nearestVisibleAttackablePlayer;
            return;
        }

        const int nearestVisiblePlayer = FindNearestVisiblePlayerTarget(false);
        if (nearestVisiblePlayer >= 0)
        {
            SelectedCharacter = nearestVisiblePlayer;
            return;
        }

        const int nearestAttackablePlayer = FindNearestAttackablePlayerTarget(false);
        if (nearestAttackablePlayer >= 0)
        {
            SelectedCharacter = nearestAttackablePlayer;
            return;
        }

        const int nearestPlayer = FindNearestPlayerTarget(false);
        if (nearestPlayer >= 0)
        {
            SelectedCharacter = nearestPlayer;
            return;
        }
    }

    const int nearestVisibleAttackableMonster = FindNearestVisibleMonsterTarget(true);
    if (nearestVisibleAttackableMonster >= 0)
    {
        SelectedCharacter = nearestVisibleAttackableMonster;
        return;
    }

    const int nearestVisibleMonster = FindNearestVisibleMonsterTarget(false);
    if (nearestVisibleMonster >= 0)
    {
        SelectedCharacter = nearestVisibleMonster;
        return;
    }

    const int nearestAttackableMonster = FindNearestAttackableMonsterTarget(false);
    if (nearestAttackableMonster >= 0)
    {
        SelectedCharacter = nearestAttackableMonster;
        return;
    }

    const int nearestMonster = FindNearestMonsterTarget(false);
    if (nearestMonster >= 0)
    {
        SelectedCharacter = nearestMonster;
        return;
    }

    EnsureCombatTarget();
}

void EnsureNormalAttackTarget()
{
    if (!IsVirtualPadAvailable())
    {
        return;
    }

    // Sanitize: never allow hero index as attack target.
    // SelectedCharacter is a global that external code can set to heroIndex.
    const int heroIdx = GetHeroCharacterIndex();
    if (heroIdx >= 0 && SelectedCharacter == heroIdx)
    {
        SelectedCharacter = -1;
    }

    // Deliberately not range-limited the way the auto-acquire path below is: if
    // the player picked this target on purpose, walking a little too far away
    // should not silently swap them onto whatever is nearest.
    const int lockedTarget = ResolveAndroidLockedTargetIndex();
    if (lockedTarget >= 0)
    {
        SelectedCharacter = lockedTarget;
        return;
    }

    if (IsTargetAttackable(SelectedCharacter)
        && IsWithinVirtualAutoAcquireRange(SelectedCharacter))
    {
        return;
    }

    const uint32_t nowMs = MU_MobileGetTicks();
    static uint32_t s_lastAutoTargetLogMs = 0;

    if (IsVirtualPkTargetingEnabled())
    {
        const int nearestAttackablePlayer = FindNearestAttackablePlayerTarget(true);
        if (nearestAttackablePlayer >= 0)
        {
            if (SelectedCharacter != nearestAttackablePlayer && (nowMs - s_lastAutoTargetLogMs) > 240)
            {
                s_lastAutoTargetLogMs = nowMs;
                LOGI("VirtualPad: normal target -> player-attackable=%d", nearestAttackablePlayer);
            }
            SelectedCharacter = nearestAttackablePlayer;
            return;
        }

        const int nearestPlayer = FindNearestPlayerTarget(true);
        if (nearestPlayer >= 0)
        {
            if (SelectedCharacter != nearestPlayer && (nowMs - s_lastAutoTargetLogMs) > 240)
            {
                s_lastAutoTargetLogMs = nowMs;
                LOGI("VirtualPad: normal target -> fallback-player=%d", nearestPlayer);
            }
            SelectedCharacter = nearestPlayer;
            return;
        }
    }

    const int nearestAttackable = FindNearestAttackableMonsterTarget(true);
    if (nearestAttackable >= 0)
    {
        if (SelectedCharacter != nearestAttackable && (nowMs - s_lastAutoTargetLogMs) > 240)
        {
            s_lastAutoTargetLogMs = nowMs;
            LOGI("VirtualPad: normal target -> monster-attackable=%d", nearestAttackable);
        }
        SelectedCharacter = nearestAttackable;
        return;
    }

    // Fallback: keep melee behavior alive by locking nearest monster even if attack check is transient.
    const int nearestMonster = FindNearestMonsterTarget(true);
    if (nearestMonster >= 0)
    {
        if (SelectedCharacter != nearestMonster && (nowMs - s_lastAutoTargetLogMs) > 240)
        {
            s_lastAutoTargetLogMs = nowMs;
            LOGI("VirtualPad: normal target -> fallback-monster=%d", nearestMonster);
        }
        SelectedCharacter = nearestMonster;
    }
    else
    {
        if ((nowMs - s_lastAutoTargetLogMs) > 800)
        {
            s_lastAutoTargetLogMs = nowMs;
            LOGI("VirtualPad: normal target -> none");
        }
        SelectedCharacter = -1;
    }
}

bool IsSupportOrSelfSkill(ActionSkillType skillType)
{
    return IsCorrectSkillType_Buff(skillType) || IsCorrectSkillType_FrendlySkill(skillType);
}

bool TriggerVirtualNormalAutoAttack()
{
    if (!IsVirtualPadAvailable()
        || Hero == nullptr
        || CharactersClient == nullptr
        || SelectedCharacter < 0
        || SelectedCharacter >= MAX_CHARACTERS_CLIENT
        || !IsValidAutoCombatTarget(SelectedCharacter)
        || !CheckAttack())
    {
        return false;
    }

    // Snapshot SelectedCharacter into a local to guard against the global
    // being modified by another code path (network thread, UI, etc.)
    // between validation above and the actual memory access below.
    const int localTarget = SelectedCharacter;

    // Double-check: reject self-attack by index (Hero can be at any random
    // slot after ReceiveJoinMapServer sets HeroIndex = rand()).
    const int heroIdx = GetHeroCharacterIndex();
    if (localTarget < 0 || localTarget >= MAX_CHARACTERS_CLIENT || localTarget == heroIdx)
    {
        return false;
    }

    // Re-validate target is still alive right before accessing its data.
    // A monster can die between IsValidAutoCombatTarget() and this point.
    const CHARACTER& targetChar = CharactersClient[localTarget];
    if (targetChar.Dead > 0 || !targetChar.Object.Live)
    {
        return false;
    }

    CHARACTER* c = Hero;
    OBJECT* o = &c->Object;

    Attacking = 1;
    c->MovementType = MOVEMENT_ATTACK;
    ActionTarget = localTarget;
    TargetX = static_cast<int>(CharactersClient[ActionTarget].Object.Position[0] / TERRAIN_SCALE);
    TargetY = static_cast<int>(CharactersClient[ActionTarget].Object.Position[1] / TERRAIN_SCALE);

    if (!CheckWall(c->PositionX, c->PositionY, TargetX, TargetY))
    {
        return false;
    }

    if (!PathFinding2(c->PositionX, c->PositionY, TargetX, TargetY, &c->Path))
    {
        if (!CheckArrow())
        {
            return false;
        }
        Action(c, o, true);
        return true;
    }

    const bool rangedAutoAttack =
        (gCharacterManager.GetEquipedBowType() != BOWTYPE_NONE)
        || (c->MonsterIndex == 0);

    if (rangedAutoAttack)
    {
        if (!CheckArrow())
        {
            return false;
        }
        Action(c, o, true);
    }
    else
    {
        SendMove(c, o);
    }

    return true;
}

// Nova is the one skill on the virtual pad that needs press-and-hold rather
// than a tap: the press starts the charge, the server ticks the charge counter
// while the finger stays down, and the release fires damage scaled by it. The
// pad fires every other skill as a single Attack() call, so the release half
// never happened and Nova could not be used at all on Android.
// We remember that a charge is running here and finish it in
// ClearActiveVirtualTouchSlot() when the finger lifts. No per-frame pumping is
// needed: MouseRButtonPress survives between the two calls, and the charge
// itself accumulates server side.
static bool g_novaChargeActive = false;
static int  g_novaChargeSkillIndex = -1;

// Hotbar (bottom bar) Nova state: tap to start charging, tap again to release.
bool g_novaTapCharging = false;

// Two-tap item pickup: the first tap on a dropped item selects it so its
// description is shown, the second tap on the same item performs the normal
// walk-to-and-collect. Cleared when the player taps elsewhere, and also when
// the item stops existing (someone else took it, or it timed out).
int g_tappedItemKey = -1;

void ClearTappedItemIfGone()
{
    if (g_tappedItemKey < 0)
    {
        return;
    }

    if (g_tappedItemKey >= MAX_ITEMS
        || !Items[g_tappedItemKey].Object.Live
        || !Items[g_tappedItemKey].Object.Visible)
    {
        g_tappedItemKey = -1;
    }
}

// Defined further down, next to the rest of the shared virtual-UI drawing
// helpers - forward-declared here so RenderItemMenu can match their look.
void DrawVirtualRectFilled(float uiX, float uiY, float uiW, float uiH, float red, float green, float blue, float alpha);
void DrawVirtualRectOutline(float uiX, float uiY, float uiW, float uiH, float red, float green, float blue, float alpha, float lineWidth);
void DrawVirtualRightPanelButtonBox(const AndroidUiRect& rect, bool active);

// ---------------------------------------------------------------------------
// Dropped item menu. Tapping an item opens this instead of interacting with the
// world, so the character never walks just because you touched an item. Pick Up
// runs MU's normal walk-to-it-and-collect; Cancel dismisses.
// ---------------------------------------------------------------------------
constexpr float kItemMenuIconSize = 34.0f;
constexpr float kItemMenuWidth   = 168.0f + kItemMenuIconSize;
constexpr float kItemMenuRowH    = 15.0f;
constexpr float kItemMenuPad     = 4.0f;

// Header bar is always drawn; the body below it is what the toggle hides.
constexpr float kItemMenuHeaderH = kItemMenuRowH;
constexpr float kItemMenuBodyH   = kItemMenuIconSize;
constexpr float kItemMenuTextX   = kItemMenuPad + kItemMenuIconSize + 6.0f;

bool  g_itemMenuOpen = false;
int   g_itemMenuItemKey = -1;
float g_itemMenuX = 0.0f;
float g_itemMenuY = 0.0f;

// Collapsed to just its title bar. Sticky across items so a player who wants
// the screen clear keeps it clear as they walk over drop after drop.
bool g_itemMenuCollapsed = false;

// Every drop currently in range, nearest first, with one shown at a time. The
// player pages through them rather than the menu guessing which one they meant.
constexpr int kItemMenuMaxEntries = 16;

int g_itemMenuList[kItemMenuMaxEntries] = {};
int g_itemMenuCount = 0;
int g_itemMenuPage  = 0;

// How close a drop has to be, in tiles, before its menu appears.
constexpr int kItemMenuRangeTiles = 5;

float ItemMenuHeight()
{
    const float body = g_itemMenuCollapsed ? 0.0f : (kItemMenuBodyH + kItemMenuPad);

    return (kItemMenuPad * 2.0f) + kItemMenuHeaderH + body;
}

void CloseItemMenu()
{
    g_itemMenuOpen = false;
    g_itemMenuItemKey = -1;

    // Otherwise the container is sized from a stale tooltip the next time a
    // drop comes into range.
    g_fLastTipW = 0.0f;
    g_fLastTipH = 0.0f;
}

// Mirrors the click-to-pick-up path in ZzzInterface so the walk, the range
// check and the inventory-full handling all behave exactly as on PC.
void StartItemPickupFromMenu()
{
    const int itemKey = g_itemMenuItemKey;

    CloseItemMenu();

    if (itemKey < 0 || itemKey >= MAX_ITEMS || Hero == nullptr)
    {
        return;
    }

    if (!Items[itemKey].Object.Live)
    {
        return;
    }

    CHARACTER* c = Hero;
    OBJECT*    o = &Hero->Object;

    SelectedItem = itemKey;
    c->MovementType = MOVEMENT_GET;
    ItemKey = itemKey;
    TargetX = (int)(Items[itemKey].Object.Position[0] / TERRAIN_SCALE);
    TargetY = (int)(Items[itemKey].Object.Position[1] / TERRAIN_SCALE);

    if (PathFinding2((c->PositionX), (c->PositionY), TargetX, TargetY, &c->Path))
    {
        SendMove(c, o);
    }
    else
    {
        Action(c, o, true);
    }
}

// A dropped item is a few pixels of sprite, which is far too small to hit with
// a fingertip. Rather than demand a pixel-exact tap, project every live item to
// screen space and take the closest one within finger reach of where the player
// actually touched.
constexpr float kItemTapReachUi = 55.0f;

int FindItemNearTap(float uiX, float uiY)
{
    int   bestItem = -1;
    float bestDistSq = kItemTapReachUi * kItemTapReachUi;

    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        OBJECT* o = &Items[i].Object;

        if (!o->Live || !o->Visible)
        {
            continue;
        }

        vec3_t position;
        Vector(o->Position[0], o->Position[1], o->Position[2] + 20.0f, position);

        int screenX = 0;
        int screenY = 0;
        Projection(position, &screenX, &screenY);

        const float dx = static_cast<float>(screenX) - uiX;
        const float dy = static_cast<float>(screenY) - uiY;
        const float distSq = (dx * dx) + (dy * dy);

        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            bestItem = i;
        }
    }

    return bestItem;
}

// Returns true when the tap was inside the menu (and therefore consumed).
bool HandleItemMenuTap(float uiX, float uiY)
{
    if (!g_itemMenuOpen)
    {
        return false;
    }

    if (uiX < g_itemMenuX || uiX > (g_itemMenuX + kItemMenuWidth)
        || uiY < g_itemMenuY || uiY > (g_itemMenuY + ItemMenuHeight()))
    {
        // Outside the container the tap belongs to the game, so it falls
        // through to normal movement and attacking.
        return false;
    }

    const float localY = uiY - (g_itemMenuY + kItemMenuPad);

    if (localY < kItemMenuHeaderH)
    {
        // Title bar doubles as the show/hide toggle.
        g_itemMenuCollapsed = !g_itemMenuCollapsed;
        return true;
    }

    if (g_itemMenuCollapsed)
    {
        return true;
    }

    const float bodyY = localY - kItemMenuHeaderH - kItemMenuPad;

    if (bodyY < kItemMenuRowH)
    {
        // Page row: previous on the left third, next on the right third.
        if (g_itemMenuCount > 1)
        {
            const float local = uiX - (g_itemMenuX + kItemMenuTextX);
            const float navW  = kItemMenuWidth - kItemMenuTextX - kItemMenuPad;

            if (local < (navW / 3.0f))
            {
                g_itemMenuPage = (g_itemMenuPage + g_itemMenuCount - 1) % g_itemMenuCount;
            }
            else if (local > (navW * 2.0f / 3.0f))
            {
                g_itemMenuPage = (g_itemMenuPage + 1) % g_itemMenuCount;
            }

            g_itemMenuItemKey = g_itemMenuList[g_itemMenuPage];
        }
    }
    else
    {
        StartItemPickupFromMenu();
    }

    return true;
}

// Picks the nearest drop within kItemMenuRangeTiles of the character and shows
// its menu automatically. Tying this to the character rather than to where the
// finger landed means the player never has to aim at a sprite a few pixels
// across - if something is on the ground next to them, the menu is simply there.
void UpdateItemMenuNearCharacter()
{
    const int previousKey = g_itemMenuOpen ? g_itemMenuItemKey : -1;

    g_itemMenuCount = 0;

    if (Hero == nullptr)
    {
        CloseItemMenu();
        return;
    }

    // Rebuilt every frame rather than cached: drops appear, get taken by other
    // players and expire on their own, and the character is usually moving.
    int dist[kItemMenuMaxEntries] = {};

    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        const OBJECT* o = &Items[i].Object;

        if (!o->Live || !o->Visible)
        {
            continue;
        }

        const int dx = static_cast<int>(o->Position[0] / TERRAIN_SCALE) - Hero->PositionX;
        const int dy = static_cast<int>(o->Position[1] / TERRAIN_SCALE) - Hero->PositionY;
        const int distSq = (dx * dx) + (dy * dy);

        if (distSq > (kItemMenuRangeTiles * kItemMenuRangeTiles))
        {
            continue;
        }

        // Insertion sort by distance, nearest first, so page 1 is always the
        // drop the player is standing on.
        int slot = g_itemMenuCount;

        if (slot >= kItemMenuMaxEntries)
        {
            if (distSq >= dist[kItemMenuMaxEntries - 1])
            {
                continue;
            }

            slot = kItemMenuMaxEntries - 1;
        }
        else
        {
            ++g_itemMenuCount;
        }

        while (slot > 0 && dist[slot - 1] > distSq)
        {
            dist[slot] = dist[slot - 1];
            g_itemMenuList[slot] = g_itemMenuList[slot - 1];
            --slot;
        }

        dist[slot] = distSq;
        g_itemMenuList[slot] = i;
    }

    if (g_itemMenuCount == 0)
    {
        CloseItemMenu();
        return;
    }

    // Keep showing whatever the player had paged to, even as nearer items come
    // and go around it; only fall back to page 1 when that item is really gone.
    g_itemMenuPage = 0;

    if (previousKey >= 0)
    {
        for (int i = 0; i < g_itemMenuCount; ++i)
        {
            if (g_itemMenuList[i] == previousKey)
            {
                g_itemMenuPage = i;
                break;
            }
        }
    }

    // Fixed, always-visible spot rather than following the item around. Kept
    // clear of the skill pad down the right edge, since the tooltip that hangs
    // below the box is centred on it and is wider than the box itself.
    g_itemMenuOpen = true;
    g_itemMenuItemKey = g_itemMenuList[g_itemMenuPage];
    g_itemMenuX = 640.0f - kItemMenuWidth - 90.0f;
    g_itemMenuY = 60.0f;
}

void RenderItemMenu()
{
    UpdateItemMenuNearCharacter();

    if (!g_itemMenuOpen)
    {
        return;
    }

    // Ground drops only ever get Type, Level, Durability, Option1 and ExtOption
    // filled in (see CreateItem in ZzzObject.cpp) - the excellent options, the
    // damage and the requirements are all derived, and inventory items get them
    // by way of ItemConvert. Doing the same on a copy leaves the world item
    // untouched while giving the tooltip everything it needs.
    ITEM item = Items[g_itemMenuItemKey].Item;

    if (item.Type != ITEM_POTION + 15)      // not money, whose Level is an amount
    {
        ItemConvert(&item, static_cast<BYTE>(item.Level), item.Option1, item.ExtOption);
    }

    const float menuH = ItemMenuHeight();

    // One container around the whole thing - header, buttons and item info. The
    // tooltip's height is only known inside RenderTipTextList, which records the
    // rect it drew into, so the box is sized from the previous frame's values.
    // Content only changes when the player pages, so the lag is never visible.
    float boxX = g_itemMenuX;
    float boxY = g_itemMenuY;
    float boxR = g_itemMenuX + kItemMenuWidth;
    float boxB = g_itemMenuY + menuH;

    const bool haveTip = (!g_itemMenuCollapsed && g_fLastTipW > 1.0f && g_fLastTipH > 1.0f);

    if (haveTip)
    {
        boxX = std::min(boxX, g_fLastTipX - kItemMenuPad);
        boxR = std::max(boxR, g_fLastTipX + g_fLastTipW + kItemMenuPad);
        boxB = std::max(boxB, g_fLastTipY + g_fLastTipH + kItemMenuPad);
    }

    const float boxW = boxR - boxX;
    const float boxH = boxB - boxY;

    // Same layered shadow/fill/border treatment as RenderAndroidTargetPicker,
    // via TextDraw/DrawVirtual* instead of g_pRenderText - this is the family
    // that operates directly in the same 640x480 space as the hit-test rects
    // below, so unlike the old g_pRenderText path it needs no display-pixel
    // rescale on the way out.
    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DrawVirtualRectFilled(boxX - 3.0f, boxY - 3.0f, boxW + 6.0f, boxH + 6.0f, 0.0f, 0.0f, 0.0f, 0.38f);
    DrawVirtualRectFilled(boxX, boxY, boxW, boxH, 0.10f, 0.04f, 0.05f, 0.78f);
    DrawVirtualRectFilled(boxX + 2.0f, boxY + 2.0f, boxW - 4.0f, boxH - 4.0f, 0.22f, 0.09f, 0.10f, 0.64f);
    DrawVirtualRectFilled(boxX + 3.0f, boxY + 3.0f, boxW - 6.0f, kItemMenuHeaderH - 2.0f, 0.62f, 0.24f, 0.24f, 0.36f);
    DrawVirtualRectOutline(boxX, boxY, boxW, boxH, 0.86f, 0.34f, 0.34f, 0.94f, 2.0f);
    DrawVirtualRectOutline(boxX + 2.0f, boxY + 2.0f, boxW - 4.0f, boxH - 4.0f, 0.20f, 0.06f, 0.08f, 0.94f, 1.0f);

    HFONT headerFont = g_hFontBold != nullptr ? g_hFontBold : g_hFont;
    HFONT rowFont    = g_hFontMini != nullptr ? g_hFontMini : g_hFont;

    char szLine[128];

    // Header, which is also the show/hide toggle.
    snprintf(szLine, sizeof(szLine) - 1, "%s Items (%d)",
             g_itemMenuCollapsed ? "[+]" : "[-]", g_itemMenuCount);
    szLine[sizeof(szLine) - 1] = '\0';
    TextDraw(headerFont,
             static_cast<int>(g_itemMenuX + kItemMenuPad),
             static_cast<int>(g_itemMenuY + kItemMenuPad),
             0xFFFFFFFF, 0x0,
             static_cast<int>(kItemMenuWidth - kItemMenuPad * 2.0f), 0, 3,
             "%s", szLine);

    if (g_itemMenuCollapsed)
    {
        EndBitmap();
        return;
    }

    const float bodyTop = g_itemMenuY + kItemMenuPad + kItemMenuHeaderH + kItemMenuPad;

    // Item picture on the left, text to its right. RenderItem3DFree draws the
    // item's own model and takes 640x480 UI coordinates directly. It swaps in
    // a perspective projection to draw the model, which is why it has to step
    // outside the 2D bitmap state this is called from - the disabled
    // EndBitmap() at the top of that function is the same thing, done by
    // whoever wrote it.
    EndBitmap();
    g_pNewUISystem->RenderItem3DFree(g_itemMenuX + kItemMenuPad, bodyTop,
                                     kItemMenuIconSize, kItemMenuIconSize,
                                     item.Type, item.Level,
                                     item.Option1, item.ExtOption,
                                     false, 1.2f);
    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Page row. Arrows only mean anything with more than one drop in range,
    // but the count is worth showing either way so the player knows there is
    // no second item hiding behind the first. Split matches HandleItemMenuTap's
    // thirds exactly, so the drawn buttons line up with what's tappable.
    const float navW = kItemMenuWidth - kItemMenuTextX - kItemMenuPad;
    if (g_itemMenuCount > 1)
    {
        const AndroidUiRect prevRect{ g_itemMenuX + kItemMenuTextX, bodyTop, navW / 3.0f, kItemMenuRowH };
        const AndroidUiRect nextRect{ g_itemMenuX + kItemMenuTextX + navW * 2.0f / 3.0f, bodyTop, navW / 3.0f, kItemMenuRowH };
        DrawVirtualRightPanelButtonBox(prevRect, false);
        DrawVirtualRightPanelButtonBox(nextRect, false);
        TextDraw(rowFont, static_cast<int>(prevRect.x), static_cast<int>(prevRect.y + 2.0f), 0xFFFFFFFF, 0x0, static_cast<int>(prevRect.w), 0, 3, "%s", "<");
        TextDraw(rowFont, static_cast<int>(nextRect.x), static_cast<int>(nextRect.y + 2.0f), 0xFFFFFFFF, 0x0, static_cast<int>(nextRect.w), 0, 3, "%s", ">");
        snprintf(szLine, sizeof(szLine) - 1, "%d/%d", g_itemMenuPage + 1, g_itemMenuCount);
    }
    else
    {
        snprintf(szLine, sizeof(szLine) - 1, "1/1");
    }
    szLine[sizeof(szLine) - 1] = '\0';
    TextDraw(rowFont, static_cast<int>(g_itemMenuX + kItemMenuTextX), static_cast<int>(bodyTop + 2.0f),
             0xFFE8D8A0, 0x0, static_cast<int>(navW), 0, 3, "%s", szLine);

    const AndroidUiRect pickRect{ g_itemMenuX + kItemMenuTextX, bodyTop + kItemMenuRowH, navW, kItemMenuRowH };
    DrawVirtualRightPanelButtonBox(pickRect, true);
    TextDraw(g_hFontBold != nullptr ? g_hFontBold : g_hFont,
             static_cast<int>(pickRect.x), static_cast<int>(pickRect.y + 2.0f),
             0xFFFFFFFF, 0x0, static_cast<int>(pickRect.w), 0, 3, "%s", "Pick Up");

    // The client's own tooltip, so the name, level, excellent options, sockets
    // and requirements all read exactly as they do in the inventory. Its
    // background is suppressed because the container above already covers it.
    g_bTipSuppressBG = true;
    RenderItemInfo(static_cast<int>(g_itemMenuX + kItemMenuWidth * 0.5f),
                   static_cast<int>(g_itemMenuY + menuH),
                   &item, false, 0, false, false);
    g_bTipSuppressBG = false;

    EndBitmap();
}

void ReleaseVirtualNovaCharge()
{

    if (!g_novaChargeActive)
    {
        return;
    }

    g_novaChargeActive = false;

    if (Hero == nullptr || Hero->Dead > 0 || g_novaChargeSkillIndex < 0)
    {
        g_novaChargeSkillIndex = -1;
        MouseRButtonPress = 0;
        return;
    }

    // Attack() reads the skill from Hero->CurrentSkill, and TriggerVirtualCombat
    // restores that to the previous slot when it returns, so point it back at
    // Nova for the release.
    const int previousSkillIndex = Hero->CurrentSkill;
    Hero->CurrentSkill = static_cast<BYTE>(g_novaChargeSkillIndex);

    MouseRButtonPop = true;
    Attack(Hero);
    MouseRButtonPop = false;

    Hero->CurrentSkill = static_cast<BYTE>(previousSkillIndex);
    g_novaChargeSkillIndex = -1;
}

void TriggerVirtualCombat(bool useNormalAttack, int skillSlot)
{
    if (!IsVirtualPadAvailable() || Hero->Dead > 0)
    {
        return;
    }

    static uint32_t s_lastVirtualCombatLog = 0;
    const uint32_t nowMs = MU_MobileGetTicks();

    if (useNormalAttack)
    {
        EnsureNormalAttackTarget();

        // Reject hero index 鑺掗埀顑解偓?after ReceiveJoinMapServer, HeroIndex is random
        // so SelectedCharacter could accidentally equal it.
        const int heroIdx = GetHeroCharacterIndex();
        if (SelectedCharacter == heroIdx && heroIdx >= 0)
        {
            SelectedCharacter = -1;
        }

        const int selectedBeforeAttack = SelectedCharacter;
        if (selectedBeforeAttack < 0)
        {
            LOGI(
                "VirtualPad: fire skipped mode=normal reason=no-target");
            return;
        }

        const bool triggered = TriggerVirtualNormalAutoAttack();
        if ((nowMs - s_lastVirtualCombatLog) > 150)
        {
            s_lastVirtualCombatLog = nowMs;
            LOGI(
                "VirtualPad: fire mode=normal target=%d triggered=%d",
                selectedBeforeAttack,
                triggered ? 1 : 0);
        }
        return;
    }

    LoadVirtualSkillSlots();


    if (skillSlot < 0 || skillSlot >= kVirtualSkillSlotCount)
    {
        return;
    }

    if (!IsAssignableVirtualSkillIndex(g_virtualSkillSlots[skillSlot]))
    {
        const int currentSkillType = (IsValidSkillIndex(Hero->CurrentSkill) && CharacterAttribute != nullptr)
            ? CharacterAttribute->Skill[Hero->CurrentSkill]
            : -1;
        LOGI(
            "VirtualPad: slot%d empty; currentSkill=%d skillType=%d",
            skillSlot,
            Hero->CurrentSkill,
            currentSkillType);
        return;
    }

    const int previousSkillIndex = Hero->CurrentSkill;
    Hero->CurrentSkill = static_cast<BYTE>(g_virtualSkillSlots[skillSlot]);
    const int rawSkillType = CharacterAttribute->Skill[Hero->CurrentSkill];
    if (rawSkillType <= 0 || rawSkillType >= MAX_SKILLS)
    {
        LOGW("VirtualPad: invalid skillType=%d skillIndex=%d", rawSkillType, Hero->CurrentSkill);
        Hero->CurrentSkill = static_cast<BYTE>(previousSkillIndex);
        return;
    }

    const ActionSkillType skillType = static_cast<ActionSkillType>(rawSkillType);
    const bool supportSkill = IsSupportOrSelfSkill(skillType);
    if (!supportSkill)
    {
        EnsureOffensiveSkillTarget();
    }
    else
    {
        // Buff/friendly skills are safer when explicitly bound to self target.
        SelectedCharacter = GetHeroCharacterIndex();
    }

    // Nova is offensive but does not need a target: it is an area attack
    // centred on the caster, and the PC path falls back to the hero's own key
    // when nothing is selected. Without this the "offensive skills need a
    // target" guard below returned before Attack() was ever called, so the
    // charge could never start on the virtual pad.
    if (!supportSkill
        && SelectedCharacter < 0
        && CharacterAttribute != nullptr
        && CharacterAttribute->Skill[Hero->CurrentSkill] == AT_SKILL_BLAST_HELL)
    {
        SelectedCharacter = GetHeroCharacterIndex();
    }

    const int selectedBeforeAttack = SelectedCharacter;
    if (supportSkill && selectedBeforeAttack < 0)
    {
        LOGI(
            "VirtualPad: fire skipped mode=skill slot=%d skillIndex=%d skillType=%d reason=self-target-unavailable",
            skillSlot,
            Hero->CurrentSkill,
            static_cast<int>(skillType));
        Hero->CurrentSkill = static_cast<BYTE>(previousSkillIndex);
        return;
    }

    if (!supportSkill && selectedBeforeAttack < 0)
    {
        LOGI(
            "VirtualPad: fire skipped mode=skill slot=%d skillIndex=%d skillType=%d reason=no-target",
            skillSlot,
            Hero->CurrentSkill,
            static_cast<int>(skillType));
        Hero->CurrentSkill = static_cast<BYTE>(previousSkillIndex);
        return;
    }

    const bool isBuffType = IsCorrectSkillType_Buff(skillType) == TRUE;
    const bool isFriendlyType = IsCorrectSkillType_FrendlySkill(skillType) == TRUE;
    if ((nowMs - s_lastVirtualCombatLog) > 150)
    {
        s_lastVirtualCombatLog = nowMs;
        LOGI(
            "VirtualPad: fire mode=skill slot=%d skillIndex=%d skillType=%d target=%d",
            skillSlot,
            Hero->CurrentSkill,
            static_cast<int>(skillType),
            selectedBeforeAttack);
        LOGI(
            "VirtualPad: skill meta index=%d type=%d support=%d buff=%d friendly=%d target=%d",
            Hero->CurrentSkill,
            static_cast<int>(skillType),
            supportSkill ? 1 : 0,
            isBuffType ? 1 : 0,
            isFriendlyType ? 1 : 0,
            selectedBeforeAttack);
    }

    // Any charge left over from a previous press would make Attack() take the
    // release branch instead of starting a new one.
    ReleaseVirtualNovaCharge();

    const bool isNovaSkill = (CharacterAttribute != nullptr
        && Hero->CurrentSkill >= 0
        && CharacterAttribute->Skill[Hero->CurrentSkill] == AT_SKILL_BLAST_HELL);

    MouseRButtonPop = false;
    MouseRButtonPush = true;
    MouseRButton = true;
    Attack(Hero);
    MouseRButtonPush = false;
    MouseRButton = false;

    // Attack() sets MouseRButtonPress when it starts a Nova charge. Leave that
    // standing and finish it on finger-up rather than restoring state here, so
    // the hold actually charges. Every other skill still behaves as a tap.
    if (isNovaSkill && MouseRButtonPress != 0)
    {
        g_novaChargeActive = true;
        g_novaChargeSkillIndex = Hero->CurrentSkill;
    }


    Hero->CurrentSkill = static_cast<BYTE>(previousSkillIndex);
}

bool AndroidTriggerNormalAttackButtonInternal()
{
    if (!kShowVirtualAttackButton)
    {
        return false;
    }

    const int selectedBefore = SelectedCharacter;
    TriggerVirtualCombat(true, -1);
    return SelectedCharacter != -1 || selectedBefore != SelectedCharacter;
}

// Skills that are cast at a point on the ground rather than at a character.
// They read the global TargetX/TargetY, so they need the aim flow rather than
// a selected target.
bool IsGroundTargetedSkillType(int skillType)
{
    return skillType == AT_SKILL_TELEPORT
        || skillType == AT_SKILL_TELEPORT_B;
}

bool IsGroundTargetedSkillIndex(int skillIndex)
{
    if (CharacterAttribute == nullptr || !IsValidSkillIndex(skillIndex))
    {
        return false;
    }

    return IsGroundTargetedSkillType(CharacterAttribute->Skill[skillIndex]);
}

bool AndroidTriggerHotKeySkillTapInternal(int hotKeySkillIndex)
{
    if (!IsVirtualPadAvailable()
        || Hero == nullptr
        || CharacterAttribute == nullptr
        || Hero->Dead > 0)
    {
        return false;
    }

    if (hotKeySkillIndex >= AT_PET_COMMAND_DEFAULT && hotKeySkillIndex < AT_PET_COMMAND_END)
    {
        if (Hero->m_pPet == nullptr)
        {
            return false;
        }

        Hero->CurrentSkill = static_cast<BYTE>(hotKeySkillIndex);
        return true;
    }

    if (!IsValidSkillIndex(hotKeySkillIndex))
    {
        return false;
    }

    const int rawSkillType = CharacterAttribute->Skill[hotKeySkillIndex];
    if (rawSkillType <= 0 || rawSkillType >= MAX_SKILLS)
    {
        return false;
    }

    const int previousSkillIndex = Hero->CurrentSkill;
    const ActionSkillType skillType = static_cast<ActionSkillType>(rawSkillType);
    const bool supportSkill = IsSupportOrSelfSkill(skillType);

    // Nova is a charge skill. The hotbar only ever gets a single tap event, so
    // hold-to-charge is not available here; instead the first tap starts the
    // charge and a second tap releases it early. If the player never taps
    // again the server fires it at full charge by itself after 12 ticks
    // (gObjSkillNovaCheckTime), so a single tap still gives a full Nova.
    const bool isNovaSkill = (rawSkillType == AT_SKILL_BLAST_HELL);

    const bool groundSkill = IsGroundTargetedSkillType(rawSkillType);

    if (supportSkill)
    {
        SelectedCharacter = GetHeroCharacterIndex();
    }
    else if (groundSkill)
    {
        // Must be -1, not the hero. AttackWizard calls CheckTarget before its
        // switch, and CheckTarget overwrites TargetX/TargetY with the selected
        // character's own position whenever one is set. Pointing it at the hero
        // - which is what this used to do, to get past the no-target check
        // below - silently replaced the aimed tile with the tile the player was
        // already standing on, and the teleport then failed its wall test.
        //
        // With no selection, CheckTarget instead runs the terrain pick, which
        // resolves the ray built from the aim point. That is the same path the
        // desktop client takes on a right click.
        SelectedCharacter = -1;
    }
    else
    {
        EnsureOffensiveSkillTarget();

        // Nova is an area attack centred on the caster and does not need a
        // target - the PC path falls back to the hero's own key. Without this
        // the no-target check below rejected it whenever nothing was selected.
        if (isNovaSkill && SelectedCharacter < 0)
        {
            SelectedCharacter = GetHeroCharacterIndex();
        }
    }

    // Ground skills are exempt: they cast at a map tile and having no character
    // selected is the correct state for them, not a failure.
    if (SelectedCharacter < 0 && !groundSkill)
    {
        LOGI(
            "VirtualPad: hotkey skill skipped skillIndex=%d skillType=%d reason=no-target support=%d",
            hotKeySkillIndex,
            rawSkillType,
            supportSkill ? 1 : 0);
        return false;
    }

    const float skillDistance = gSkillManager.GetSkillDistance(skillType, Hero);

    // Tap 1 sends the charge-start id, tap 2 sends the normal id to release.
    // ExecuteSkill maps the charge-start id back to Nova when it looks the
    // skill up, so both taps resolve to the same skill slot.
    int skillToSend = static_cast<int>(skillType);

    if (isNovaSkill)
    {
        if (!g_novaTapCharging)
        {
            skillToSend = AT_SKILL_BLAST_HELL_BEGIN;
            g_novaTapCharging = true;
        }
        else
        {
            g_novaTapCharging = false;
        }
    }
    else
    {
        // Using any other skill abandons a pending Nova charge.
        g_novaTapCharging = false;
    }

    const int executeResult = ExecuteSkill(Hero, skillToSend, skillDistance);
    const bool startedSkillMove = Hero->Movement && Hero->MovementType == MOVEMENT_SKILL;

    LOGI(
        "VirtualPad: hotkey skill skillIndex=%d skillType=%d target=%d result=%d move=%d movementType=%d visible=%d",
        hotKeySkillIndex,
        rawSkillType,
        SelectedCharacter,
        executeResult,
        startedSkillMove ? 1 : 0,
        Hero->MovementType,
        (SelectedCharacter >= 0 && SelectedCharacter < MAX_CHARACTERS_CLIENT && CharactersClient[SelectedCharacter].Object.Visible) ? 1 : 0);

    Hero->CurrentSkill = static_cast<BYTE>(previousSkillIndex);
    return executeResult != 0 || startedSkillMove;

}

// Defined below, next to the other overlay slot helpers.
int GetVirtualOverlayHotKeySkillIndex(int visualSlot);

// The combo is a Knight mechanic, so the setting only takes effect on that
// class line. Leaving it enabled on another character is harmless - it simply
// does nothing.
bool IsAndroidComboClass()
{
    return Hero != nullptr
        && gCharacterManager.GetBaseClass(Hero->Class) == CLASS_KNIGHT;
}

bool IsAndroidComboActive()
{
    return g_virtualComboEnabled && IsAndroidComboClass();
}

// Paired with RenderComboToggle further down; defined here so the input chain,
// which comes first in the file, can reach it.
bool HitTestComboToggle(float uiX, float uiY)
{
    if (!IsVirtualPadAvailable() || !IsAndroidComboClass())
    {
        return false;
    }

    return HitTestAndroidUiRect(uiX, uiY, GetComboToggleRect());
}

void CancelAndroidGroundAim(const char* reason)
{
    if (!g_androidGroundAim.aiming && !g_androidGroundAim.pendingCast)
    {
        return;
    }

    LOGI("VirtualPad: ground aim cancelled (%s)", reason != nullptr ? reason : "?");
    g_androidGroundAim = AndroidGroundAim{};
}

// Runs from the scene phase, straight after MoveHero, because it needs the same
// camera matrices MoveHero's own terrain pick relies on. Doing this from the
// touch handler would run it before the frame set those up.
void UpdateAndroidGroundAimCast()
{
    if (!g_androidGroundAim.pendingCast)
    {
        return;
    }

    const int skillIndex = g_androidGroundAim.skillIndex;
    const float aimUiX = g_androidGroundAim.uiX;
    const float aimUiY = g_androidGroundAim.uiY;

    if (!IsVirtualPadAvailable() || Hero == nullptr || !IsValidSkillIndex(skillIndex))
    {
        g_androidGroundCastResult = kGroundCastBadSkill;
        g_androidGroundAim = AndroidGroundAim{};
        return;
    }

    // MouseX/MouseY are in the same 640x480 space as the overlay on Android
    // (UpdateAndroidScreenMetrics pins DisplayWin/DisplayHeight), so the aim
    // point can be parked there directly.
    //
    // Building the ray here with CreateScreenVector does not work: this runs in
    // the move phase, where the camera matrices it reads are not the ones the
    // frame renders with, so every aim point collapsed onto the same tile. The
    // ray has to come from the render phase, which is why the pick waits.
    MouseX = static_cast<int>(aimUiX);
    MouseY = static_cast<int>(aimUiY);

    g_androidGroundAimX = MouseX;
    g_androidGroundAimY = MouseY;

    if (g_androidGroundAim.settleFrames > 0)
    {
        --g_androidGroundAim.settleFrames;
        return;
    }

    g_androidGroundAim = AndroidGroundAim{};

    // MouseX/MouseY are deliberately left at the aim point. No button is held,
    // so a parked pointer does nothing, and restoring the old value here would
    // undo the very thing the pick depends on.

    // MouseTarget now describes the ray through the aim point. This pick is
    // only to report where the shot is going - CheckTarget inside AttackWizard
    // runs the identical pair of calls and is what actually sets TargetX/Y, so
    // nothing here is load bearing.
    RenderTerrain(true);
    const bool picked = RenderTerrainTile(SelectXF, SelectYF, (int)SelectXF, (int)SelectYF, 1.f, 1, true);

    if (picked)
    {
        const int tileX = static_cast<int>(CollisionPosition[0] / TERRAIN_SCALE);
        const int tileY = static_cast<int>(CollisionPosition[1] / TERRAIN_SCALE);

        g_androidGroundCastTileX = tileX;
        g_androidGroundCastTileY = tileY;

        if (tileX >= 0 && tileX < 256 && tileY >= 0 && tileY < 256)
        {
            TargetX = tileX;
            TargetY = tileY;

            // The teleport branch in AttackWizard drops the cast without a word
            // unless the destination tile has no wall attributes left after
            // ACTION and HEIGHT are masked off - so a safe zone, or any no-move
            // ground, silently does nothing. Record the same value it tests so
            // the readout can say that is what happened.
            int wall = TerrainWall[TERRAIN_INDEX_REPEAT(tileX, tileY)];
            if ((wall & TW_ACTION) == TW_ACTION) wall -= TW_ACTION;
            if ((wall & TW_HEIGHT) == TW_HEIGHT) wall -= TW_HEIGHT;
            g_androidGroundCastWall = wall;

            // Evaluate the same gates CanExecuteSkill and ExecuteSkill apply, so
            // a refusal names the reason instead of just failing. Read-only:
            // these are all predicates, none of them change state.
            const int skillType = (CharacterAttribute != nullptr)
                ? CharacterAttribute->Skill[skillIndex]
                : -1;

            g_androidGroundCastReason = kGroundReasonOk;

            if (Hero->SafeZone)
            {
                g_androidGroundCastReason = kGroundReasonSafeZone;
            }
            else if (skillType > 0 && !gSkillManager.DemendConditionCheckSkill(static_cast<WORD>(skillType)))
            {
                g_androidGroundCastReason = kGroundReasonDemand;
            }
            else if (skillType > 0 && !CheckSkillUseCondition(&Hero->Object, skillType))
            {
                g_androidGroundCastReason = kGroundReasonUseCond;
            }
            else if (skillType > 0 && !CheckMana(Hero, skillType))
            {
                g_androidGroundCastReason = kGroundReasonMana;
            }
            else if (g_pSkillList != nullptr && g_pSkillList->GetSkillIndex(skillType) == -1)
            {
                g_androidGroundCastReason = kGroundReasonNoSkillIdx;
            }

            // Captured before the cast. ExecuteSkill refuses outright unless the
            // hero is in one of a list of standing actions, and its return value
            // is SkillSuccess && !Movement - so a moving hero reports failure
            // even when the skill did go out. Class matters too: the teleport
            // branch lives in AttackWizard, which only runs for the wizard,
            // dark and summoner lines.
            g_androidGroundCastClass = gCharacterManager.GetBaseClass(Hero->Class);
            g_androidGroundCastAction = Hero->Object.CurrentAction;
            g_androidGroundCastMoving = Hero->Movement ? 1 : 0;

            // The last two gates inside the teleport branch itself. A teleport
            // state left over from an earlier attempt, or a faded character,
            // both make it skip the send without a word.
            g_androidGroundCastTpState = Hero->Object.Teleport;
            g_androidGroundCastAlpha = static_cast<int>(Hero->Object.Alpha * 100.0f);

            // Position before the cast, so the next frame can tell whether the
            // hero actually moved. The return value below cannot: ExecuteSkill
            // reports SkillSuccess, which only becomes true when the server
            // replies, so it is always false for a skill sent this instant.
            g_androidGroundCastFromX = Hero->PositionX;
            g_androidGroundCastFromY = Hero->PositionY;

            // The global teleport latch. SendRequestMagicTeleport sets it and
            // refuses every later request while it is set; only a server reply
            // (ReceiveTeleport / ReceiveMagic / ReceiveRevival / map change)
            // clears it. If the server ever ignores a request - an unreachable
            // destination, say - it stays set and teleport is dead for the rest
            // of the session with no message of any kind.
            //
            // Clear it if it has been stuck well past any plausible round trip.
            // A real reply would have arrived long before this.
            {
                const uint32_t nowMs = MU_MobileGetTicks();

                if (!Teleport)
                {
                    g_androidTeleportLatchSinceMs = 0;
                }
                else
                {
                    if (g_androidTeleportLatchSinceMs == 0)
                    {
                        g_androidTeleportLatchSinceMs = nowMs;
                    }
                    else if ((nowMs - g_androidTeleportLatchSinceMs) > kAndroidTeleportLatchStuckMs)
                    {
                        LOGI("VirtualPad: clearing stuck teleport latch after %ums",
                             nowMs - g_androidTeleportLatchSinceMs);
                        Teleport = false;
                        g_androidTeleportLatchSinceMs = 0;
                    }
                }

                g_androidGroundCastLatch = Teleport ? 1 : 0;
            }

            // AttackWizard has its own gates before the teleport case, and they
            // all return silently. CheckMana - which gate-ok above covers -
            // checks mana and AG but NOT energy, so this one has been invisible
            // the whole time.
            if (skillType > 0 && CharacterAttribute != nullptr)
            {
                int reqEnergy = 0;
                gSkillManager.GetSkillInformation_Energy(skillType, &reqEnergy);
                g_androidGroundCastReqEnergy = reqEnergy;
                g_androidGroundCastHaveEnergy =
                    CharacterAttribute->Energy + CharacterAttribute->AddEnergy;
            }

            const bool sent = AndroidTriggerHotKeySkillTapInternal(skillIndex);
            g_androidGroundCastResult = sent ? kGroundCastOk : kGroundCastRefused;

            // CheckSkillDelay is the last silent return before AttackWizard's
            // switch, and it fails for two reasons that nothing reports: the
            // skill still being on cooldown, or a charisma requirement the
            // character cannot meet. Both are read here rather than guessed.
            if (CharacterAttribute != nullptr && skillType > 0)
            {
                g_androidGroundCastDelay = CharacterAttribute->SkillDelay[skillIndex];

                int reqCharisma = 0;
                gSkillManager.GetSkillInformation_Charisma(skillType, &reqCharisma);
                g_androidGroundCastReqChar = reqCharisma;
                g_androidGroundCastHaveChar =
                    CharacterAttribute->Charisma + CharacterAttribute->AddCharisma;
            }

            // The one hard fact left. SendRequestMagicTeleport sets the latch
            // as it writes the packet, so reading it immediately afterwards
            // says whether a packet was actually produced:
            //   1 -> the request went out, so anything wrong is server side
            //   0 -> the macro refused, nothing was ever sent
            g_androidGroundCastLatchAfter = Teleport ? 1 : 0;

            LOGI("VirtualPad: ground cast skillIndex=%d type=%d tile=(%d,%d) wall=%d reason=%d sent=%d",
                 skillIndex, skillType, TargetX, TargetY, wall, g_androidGroundCastReason, sent ? 1 : 0);
        }
        else
        {
            g_androidGroundCastResult = kGroundCastOffMap;
            LOGI("VirtualPad: ground cast rejected, tile out of range (%d,%d)", tileX, tileY);
        }
    }
    else
    {
        g_androidGroundCastResult = kGroundCastNoTile;
        LOGI("VirtualPad: ground cast rejected, no terrain under aim point");
    }
}

// Fires whatever the attack button should fire right now. Three cases, in
// order: an active combo advances one step, an armed skill slot casts that
// skill, and otherwise it is a plain weapon attack.
//
// Shared by the tap path and the hold-to-repeat path so both behave the same.
bool TriggerVirtualAttackButtonPress()
{
    if (!kShowVirtualAttackButton)
    {
        return false;
    }

    if (IsAndroidComboActive())
    {
        const uint32_t nowMs = MU_MobileGetTicks();

        // Lapsed since the last press, so the chain is cold - start from the
        // opener rather than resuming halfway through. Updated on every press,
        // including failed ones, so retrying a blocked step does not itself
        // time the combo out.
        if ((nowMs - g_virtualComboLastMs) > kVirtualComboResetMs)
        {
            g_virtualComboStep = 0;
        }
        g_virtualComboLastMs = nowMs;

        const int step = g_virtualComboStep;
        const int skillIndex = GetVirtualOverlayHotKeySkillIndex(step);

        // Highlight follows the chain so the player can see which step is up.
        g_virtualSelectedSkillSlot = step;

        // An empty slot is the weapon step by design - slot 1 is meant to hold
        // the weapon skill, and "no skill bound" means a plain swing.
        if (!IsValidSkillIndex(skillIndex))
        {
            g_virtualComboStep = (step + 1) % kVirtualComboSlotCount;
            g_androidComboLastResult = kAndroidComboResultWeapon;
            return AndroidTriggerNormalAttackButtonInternal();
        }

        if (Hero != nullptr)
        {
            Hero->CurrentSkill = static_cast<BYTE>(skillIndex);
        }

        if (AndroidTriggerHotKeySkillTapInternal(skillIndex))
        {
            // Advance only once the skill actually went out.
            g_virtualComboStep = (step + 1) % kVirtualComboSlotCount;
            g_androidComboLastResult = kAndroidComboResultCast;
            return true;
        }

        // The skill was refused - cooldown, out of range, or nothing targeted.
        //
        // Hold the step rather than advancing: burning it here is why the chain
        // never finished, because the next press moved on while this skill had
        // not landed. And deliberately do not swing the weapon instead, since a
        // normal attack in the middle of a combo resets the server's chain.
        g_androidComboLastResult = kAndroidComboResultBlocked;
        LOGI("VirtualPad: combo step=%d skillIndex=%d refused, holding step", step, skillIndex);
        return false;
    }

    const int slotToFire = g_virtualSelectedSkillSlot;

    if (slotToFire >= 0 && slotToFire < kVirtualOverlaySkillSlotCount)
    {
        const int hotKeySkillIndex = GetVirtualOverlayHotKeySkillIndex(slotToFire);

        // Keep CurrentSkill in step too, so the legacy UI and anything else
        // reading it agrees with the button that is lit.
        if (Hero != nullptr && IsValidSkillIndex(hotKeySkillIndex))
        {
            Hero->CurrentSkill = static_cast<BYTE>(hotKeySkillIndex);
        }

        if (AndroidTriggerHotKeySkillTapInternal(hotKeySkillIndex))
        {
            return true;
        }

        TriggerVirtualCombat(false, slotToFire);
        return true;
    }

    return AndroidTriggerNormalAttackButtonInternal();
}

int GetVirtualOverlayHotKeySlot(int visualSlot)
{
    // The last button in the arc is the skill selector, not a skill: it owns no
    // hotkey. Returning -1 here is what keeps it out of the fire and assign
    // paths, all of which route through this one function.
    if (visualSlot < 0 || visualSlot >= kVirtualOverlaySkillSlotCount)
    {
        return -1;
    }

    return visualSlot + 1;
}

int GetVirtualOverlayHotKeySkillIndex(int visualSlot)
{
    if (g_pSkillList == nullptr)
    {
        return -1;
    }

    const int hotKeySlot = GetVirtualOverlayHotKeySlot(visualSlot);
    return (hotKeySlot >= 0) ? g_pSkillList->GetHotKey(hotKeySlot) : -1;
}

void ClearVirtualCombatTouches()
{
    for (int i = 0; i < static_cast<int>(g_activeVirtualTouches.size()); ++i)
    {
        ClearActiveVirtualTouchSlot(i);
    }
}

bool IsVirtualRightPanelUtilityActionActive(int button)
{
    if (g_pNewUISystem == nullptr)
    {
        return false;
    }

    switch (button)
    {
    case kVirtualRightPanelUtilityActionPk:
        return g_pBCustomMenuInfo != nullptr && g_pBCustomMenuInfo->AutoCtrlPK;
    case kVirtualRightPanelUtilityActionChat:
        return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX);
    case kVirtualRightPanelUtilityActionXShop:
        return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INGAMESHOP);
    case kVirtualRightPanelUtilityActionHelper:
        return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MuHelper);
    case kVirtualRightPanelUtilityActionBag:
        return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY);
    case kVirtualRightPanelUtilityActionCharacter:
        return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHARACTER);
    case kVirtualRightPanelUtilityActionMap:
        return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MOVEMAP);
    case kVirtualRightPanelUtilityActionSetting:
        return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION);
    case kVirtualRightPanelUtilityActionCommand:
        return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_COMMAND);
    case kVirtualRightPanelUtilityActionFriend:
        return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_FRIEND);
    case kVirtualRightPanelUtilityActionGuild:
        return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_GUILDINFO);
    default:
        return false;
    }
}

// True while any real MU window owns the screen. The whole touch overlay hides
// behind this so its controls never sit on top of the bag, the character sheet
// or an NPC window, and so taps go to that window instead.
//
// Deliberately lists only MU interfaces: the Android trade and target pickers
// are themselves modal panels, and including them here would make each one hide
// itself the moment it opened.
bool IsAndroidGameWindowOpen()
{
    if (g_pNewUISystem == nullptr)
    {
        return false;
    }

    return g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY)
        || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHARACTER)
        || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INGAMESHOP)
        || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MuHelper)
        || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MOVEMAP)
        || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION)
        || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_COMMAND)
        || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_FRIEND)
        || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_GUILDINFO);
}

bool IsVirtualRightPanelUtilityWindowVisible()
{
    return g_pNewUISystem != nullptr
        && (g_androidTradePicker.visible
            || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX)
            || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INGAMESHOP)
            || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MuHelper)
            || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY)
            || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHARACTER)
            || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MOVEMAP)
            || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION)
            || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_COMMAND)
            || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_FRIEND)
            || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_GUILDINFO));
}

bool ShouldYieldVirtualRightPanelUtilityOverlay()
{
    if (!g_virtualRightPanelUtilityMode || g_pNewUISystem == nullptr)
    {
        return false;
    }

    return IsVirtualRightPanelUtilityWindowVisible();
}

void ToggleVirtualRightPanelMode()
{
    g_virtualRightPanelUtilityMode = !g_virtualRightPanelUtilityMode;
    ClearVirtualCombatTouches();
    LOGI(
        "VirtualPad: right panel mode -> %s",
        g_virtualRightPanelUtilityMode ? "utility" : "combat");
    PlayBuffer(SOUND_CLICK01);
}

void TriggerVirtualRightPanelUtilityAction(int button)
{
    if (g_pNewUISystem == nullptr)
    {
        return;
    }

    switch (button)
    {
    case kVirtualRightPanelUtilityActionPk:
        if (g_pBCustomMenuInfo != nullptr)
        {
            g_pBCustomMenuInfo->AutoCtrlPK ^= 1;
            PlayBuffer(SOUND_CLICK01);
        }
        break;

    case kVirtualRightPanelUtilityActionChat:
        ToggleVirtualChatInputBox();
        PlayBuffer(SOUND_CLICK01);
        break;

    case kVirtualRightPanelUtilityActionJewelBank:
#if defined(JEWELBANKVER2) && (JEWELBANKVER2)
        if (gCB_NewJewelBank != nullptr)
        {
            gCB_NewJewelBank->OpenOnOff();
            PlayBuffer(SOUND_CLICK01);
        }
#else
        gCBJewelBank.OpenOnOff();
        PlayBuffer(SOUND_CLICK01);
#endif
        break;

    case kVirtualRightPanelUtilityActionXShop:
        // Mirrors the PC "X" toolbar button (NewUIMainFrameWindow.cpp's
        // PBG_ADD_INGAMESHOP_UI_MAINFRAME handler): the shop's own movement/
        // safe-zone gate, then load the category/package/product script
        // (from the locally-shipped IBS*.txt files - see MuAudio-adjacent
        // asset extraction in MuMainNativeActivity.java) before requesting
        // the server open the shop. Banner download is skipped: it's a
        // separate, PC-only-triggered subsystem not needed for shop content.
#ifdef KJH_ADD_INGAMESHOP_UI_SYSTEM
        if (g_pInGameShop->IsInGameShopOpen() == false)
        {
            break;
        }

#ifdef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
        if (g_InGameShopSystem->IsScriptDownload() == true)
        {
            if (g_InGameShopSystem->ScriptDownload() == false)
            {
                break;
            }
        }
#endif

        if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INGAMESHOP) == false)
        {
            if (g_InGameShopSystem->GetIsRequestShopOpenning() == false)
            {
                SendRequestIGS_CashShopOpen(0);
                g_InGameShopSystem->SetIsRequestShopOpenning(true);
                PlayBuffer(SOUND_CLICK01);
            }
        }
        else
        {
            SendRequestIGS_CashShopOpen(1);
            g_pNewUISystem->Hide(SEASON3B::INTERFACE_INGAMESHOP);
            PlayBuffer(SOUND_CLICK01);
        }
#endif
        break;

    case kVirtualRightPanelUtilityActionHelper:
        g_pNewUISystem->Toggle(SEASON3B::INTERFACE_MuHelper);
        PlayBuffer(SOUND_CLICK01);
        break;

    case kVirtualRightPanelUtilityActionBag:
        g_pNewUISystem->Toggle(SEASON3B::INTERFACE_INVENTORY);
        PlayBuffer(SOUND_CLICK01);
        break;

    case kVirtualRightPanelUtilityActionCharacter:
        g_pNewUISystem->Toggle(SEASON3B::INTERFACE_CHARACTER);
        PlayBuffer(SOUND_CLICK01);
        break;

    case kVirtualRightPanelUtilityActionMap:
        ToggleMapListByVirtualButton();
        PlayBuffer(SOUND_CLICK01);
        break;

    case kVirtualRightPanelUtilityActionSetting:
        g_pNewUISystem->Toggle(SEASON3B::INTERFACE_OPTION);
        PlayBuffer(SOUND_CLICK01);
        break;

    case kVirtualRightPanelUtilityActionCommand:
        if (gMapManager.InChaosCastle() == false)
        {
            g_pNewUISystem->Toggle(SEASON3B::INTERFACE_COMMAND);
            PlayBuffer(SOUND_CLICK01);
        }
        break;

    case kVirtualRightPanelUtilityActionFriend:
        ToggleFriendListByVirtualButton();
        PlayBuffer(SOUND_CLICK01);
        break;

    case kVirtualRightPanelUtilityActionGuild:
        g_pNewUISystem->Toggle(SEASON3B::INTERFACE_GUILDINFO);
        PlayBuffer(SOUND_CLICK01);
        break;

    default:
        break;
    }
}

// Paired with RenderVirtualTopBar. Returns the utility action for the button
// under the finger, or -1. Note this gates on IsVirtualUtilityButtonsAvailable
// rather than the combat/utility mode flag: the top bar is always on screen, it
// does not belong to the CHG toggle.
int HitTestVirtualTopBarButton(float uiX, float uiY)
{
    // Not -1 for "nothing here": the menu entry uses a negative sentinel of its
    // own, so the miss value has to be distinct from any real action.
    if (!IsVirtualUtilityButtonsAvailable() || !IsVirtualPadAvailable())
    {
        return kTopBarActionNone;
    }

    if (HitTestAndroidUiRect(uiX, uiY, GetTopBarRowToggleButtonRect()))
    {
        return kTopBarActionRowToggle;
    }

    for (int slot = 0; slot < kTopBarButtonCount; ++slot)
    {
        // Slots 1-4 (Guild/Shop/Settings/Bags) are the row the toggle above
        // hides; slot 0 (Menu) and the Helper/Play stack (5, 6) stay tappable
        // regardless, same as they stay drawn in RenderVirtualTopBar.
        if (!g_topBarRowIconsVisible && slot >= 1 && slot < kTopBarRowButtonCount)
        {
            continue;
        }

        if (HitTestAndroidUiRect(uiX, uiY, GetTopBarButtonRect(slot)))
        {
            return kTopBarActions[slot];
        }
    }

    if (HitTestAndroidUiRect(uiX, uiY, GetTopBarLocationChipRect()))
    {
        return kTopBarActionLocation;
    }

    return kTopBarActionNone;
}

// The portrait avatar doubles as the character sheet button. The top bar only
// has room for six entries and stats did not make the cut, so tapping your own
// face is the way in - the same gesture the reference layout uses. The rect is
// the one RenderVirtualPortraitHud draws the avatar into, so what is visible is
// exactly what is tappable; the bars to its right and the pet slot below stay
// out of it. Same availability gate as the panel itself, which is what
// guarantees the panel is on screen when this reports a hit.
bool HitTestVirtualPortraitAvatar(float uiX, float uiY)
{
    if (!IsVirtualPadAvailable())
    {
        return false;
    }

    const AndroidUiRect rect{
        kPortraitAvatarX,
        kPortraitAvatarY,
        kPortraitAvatarW,
        kPortraitAvatarH
    };

    return HitTestAndroidUiRect(uiX, uiY, rect);
}

int HitTestVirtualRightPanelUtilityActionButton(float uiX, float uiY)
{
    if (!g_virtualRightPanelUtilityMode || !IsVirtualPadAvailable())
    {
        return -1;
    }

    for (int button = 0; button < kVirtualRightPanelUtilityActionCount; ++button)
    {
        if (HitTestAndroidUiRect(uiX, uiY, GetVirtualRightPanelGridRect(button)))
        {
            return button;
        }
    }

    return -1;
}

bool HitTestVirtualRightPanelModeButton(float uiX, float uiY)
{
    if (!IsVirtualPadAvailable())
    {
        return false;
    }

    const AndroidUiRect rect = g_virtualRightPanelUtilityMode
        ? GetVirtualRightPanelUtilityModeButtonRect()
        : GetVirtualRightPanelCombatModeButtonRect();
    return HitTestAndroidUiRect(uiX, uiY, rect);
}

bool HitTestVirtualRightPanelFrame(float uiX, float uiY)
{
    return g_virtualRightPanelUtilityMode
        && IsVirtualPadAvailable()
        && HitTestAndroidUiRect(uiX, uiY, GetVirtualRightPanelRect());
}

bool HandleVirtualRightPanelTap(float uiX, float uiY)
{
    const uint32_t nowMs = MU_MobileGetTicks();

    if (ShouldYieldVirtualRightPanelUtilityOverlay())
    {
        return false;
    }

    // No CHG hit test here any more: its render was removed, and leaving the
    // test behind would be an invisible tap target in the corner. The MEN entry
    // on the top bar owns this toggle.

    const int utilityButton = HitTestVirtualRightPanelUtilityActionButton(uiX, uiY);
    if (utilityButton >= 0)
    {
        if ((nowMs - g_virtualLastUtilityTapMs) >= kVirtualUtilityButtonCooldownMs)
        {
            g_virtualLastUtilityTapMs = nowMs;
            TriggerVirtualRightPanelUtilityAction(utilityButton);
        }
        return true;
    }

    return HitTestVirtualRightPanelFrame(uiX, uiY);
}

// Defined further down, next to the hotkey rendering.
ITEM* GetVirtualMirrorHotKeyItem(int slot);

int HitTestVirtualMirrorHotKeySlot(float uiX, float uiY)
{
    // No local window guard: hiding the overlay while a MU window is open is
    // handled once, centrally, by the IsAndroidGameWindowOpen checks in
    // RenderVirtualPad and HandleVirtualFingerDown.
    if (!IsVirtualPadAvailable())
    {
        return -1;
    }

    for (int slot = 0; slot < kVirtualMirrorHotKeySlotCount; ++slot)
    {
        AndroidUiRect rect = GetVirtualMirrorHotKeyRect(slot);
        rect.x -= kVirtualMirrorHotKeyTouchPadding;
        rect.y -= kVirtualMirrorHotKeyTouchPadding;
        rect.w += kVirtualMirrorHotKeyTouchPadding * 2.0f;
        rect.h += kVirtualMirrorHotKeyTouchPadding * 2.0f;
        if (HitTestAndroidUiRect(uiX, uiY, rect))
        {
            return slot;
        }
    }

    return -1;
}

bool UseVirtualMirrorHotKeySlot(int slot)
{
    if (slot < 0 || slot >= kVirtualMirrorHotKeySlotCount || g_pMainFrame == nullptr)
    {
        return false;
    }

    return g_pMainFrame->UseItemHotKey(kVirtualMirrorHotKeys[slot]);
}

int HitTestVirtualAttackButton(float uiX, float uiY)
{
    if (!kShowVirtualAttackButton || !IsVirtualPadAvailable() || g_virtualRightPanelUtilityMode)
    {
        return -1;
    }

    const VirtualButtonLayout& button = kVirtualButtons[kVirtualAttackButton];
    const float dx = uiX - button.cx;
    const float dy = uiY - button.cy;
    const float hitRadius = GetVirtualButtonHitRadius(kVirtualAttackButton);
    return ((dx * dx) + (dy * dy)) <= (hitRadius * hitRadius)
        ? kVirtualAttackButton
        : -1;
}

int HitTestVirtualSkillButton(float uiX, float uiY)
{
    if (!kShowVirtualSkillButtons || !IsVirtualPadAvailable() || g_virtualRightPanelUtilityMode)
    {
        return -1;
    }

    for (int visualSlot = 0; visualSlot < kVirtualVisibleSkillButtonCount; ++visualSlot)
    {
        const int buttonIndex = kVirtualSkillButtonBase + visualSlot;
        const VirtualButtonLayout& button = kVirtualButtons[buttonIndex];
        const float left = button.cx - (kVirtualSkillFrameW * 0.5f);
        const float top = button.cy - (kVirtualSkillFrameH * 0.5f);
        if (uiX >= left
            && uiX <= (left + kVirtualSkillFrameW)
            && uiY >= top
            && uiY <= (top + kVirtualSkillFrameH))
        {
            return buttonIndex;
        }
    }

    return -1;
}

int GetAndroidTradePickerRowCount()
{
    if (g_androidTradePicker.entryCount <= 0)
    {
        return 1;
    }

    return std::min(g_androidTradePicker.entryCount, kAndroidTradePickerVisibleRows);
}

int GetAndroidTradePickerMaxScrollOffset()
{
    return std::max(0, g_androidTradePicker.entryCount - kAndroidTradePickerVisibleRows);
}

void ClampAndroidTradePickerScroll()
{
    g_androidTradePicker.scrollOffset = std::clamp(
        g_androidTradePicker.scrollOffset,
        0,
        GetAndroidTradePickerMaxScrollOffset());
}

const char* GetAndroidPlayerCommandPickerTitle()
{
    switch (g_androidTradePicker.mode)
    {
    case AndroidPlayerCommandMode::Friend:
        return "FRIEND LIST";
    case AndroidPlayerCommandMode::Duel:
        return "DUEL LIST";
    case AndroidPlayerCommandMode::Guild:
        return "GUILD LIST";
    case AndroidPlayerCommandMode::Party:
        return "PARTY LIST";
    case AndroidPlayerCommandMode::Trade:
        return "TRADE LIST";
    default:
        return "PLAYER LIST";
    }
}

bool CanAndroidSendPartyCommand()
{
    if (Hero != nullptr
        && PartyNumber > 0
        && std::strcmp(Party[0].Name, Hero->ID) != 0)
    {
        if (g_pChatListBox != nullptr)
        {
            g_pChatListBox->AddText("", GlobalText[257], SEASON3B::TYPE_ERROR_MESSAGE);
        }
        return false;
    }

    return true;
}

bool CanAndroidSendGuildCommand()
{
    if (Hero != nullptr && Hero->GuildStatus != G_NONE)
    {
        if (g_pChatListBox != nullptr)
        {
            g_pChatListBox->AddText("", GlobalText[255], SEASON3B::TYPE_SYSTEM_MESSAGE);
        }
        return false;
    }

    return true;
}

bool CanAndroidSendDuelCommand()
{
    if (CharacterAttribute == nullptr)
    {
        return false;
    }

    if (CharacterAttribute->Level < 30)
    {
        if (g_pChatListBox != nullptr)
        {
            char szError[255] = "";
            sprintf(szError, GlobalText[2704], 30);
            g_pChatListBox->AddText("", szError, SEASON3B::TYPE_ERROR_MESSAGE);
        }
        return false;
    }

    if (gMapManager.WorldActive >= WD_65DOPPLEGANGER1
        && gMapManager.WorldActive <= WD_68DOPPLEGANGER4)
    {
        if (g_pChatListBox != nullptr)
        {
            g_pChatListBox->AddText("", GlobalText[2866], SEASON3B::TYPE_ERROR_MESSAGE);
        }
        return false;
    }

    if (gMapManager.WorldActive == WD_79UNITEDMARKETPLACE)
    {
        if (g_pChatListBox != nullptr)
        {
            g_pChatListBox->AddText("", GlobalText[3063], SEASON3B::TYPE_ERROR_MESSAGE);
        }
        return false;
    }

    return true;
}

bool CanAndroidSendFriendCommand()
{
    if (!CanAndroidUseFriendFeature())
    {
        return false;
    }

    if (g_pWindowMgr != nullptr && g_pWindowMgr->IsServerEnable() == FALSE)
    {
        SendRequestFriendList();
    }

    return true;
}

AndroidUiRect GetAndroidTradePickerRect()
{
    return {
        kAndroidTradePickerX,
        kAndroidTradePickerY,
        kAndroidTradePickerW,
        kAndroidTradePickerHeaderH
            + (static_cast<float>(GetAndroidTradePickerRowCount()) * kAndroidTradePickerRowH)
            + kAndroidTradePickerFooterH
    };
}

AndroidUiRect GetAndroidTradePickerRowRect(int row)
{
    return {
        kAndroidTradePickerX + 6.0f,
        kAndroidTradePickerY + kAndroidTradePickerHeaderH + (static_cast<float>(row) * kAndroidTradePickerRowH),
        kAndroidTradePickerW - 12.0f,
        kAndroidTradePickerRowH - 2.0f
    };
}

AndroidUiRect GetAndroidTradePickerFooterRect()
{
    return {
        kAndroidTradePickerX + 6.0f,
        kAndroidTradePickerY + kAndroidTradePickerHeaderH
            + (static_cast<float>(GetAndroidTradePickerRowCount()) * kAndroidTradePickerRowH),
        kAndroidTradePickerW - 12.0f,
        kAndroidTradePickerFooterH - 3.0f
    };
}

bool IsAndroidTradeTargetAvailable(int characterIndex, bool requireVisible)
{
    if (!IsVirtualPadAvailable()
        || CharactersClient == nullptr
        || Hero == nullptr
        || characterIndex < 0
        || characterIndex >= MAX_CHARACTERS_CLIENT)
    {
        return false;
    }

    const int heroIndex = GetHeroCharacterIndex();
    CHARACTER* c = &CharactersClient[characterIndex];
    OBJECT* o = &c->Object;
    if (c == Hero || characterIndex == heroIndex)
    {
        return false;
    }

    if (c->Dead > 0
        || !o->Live
        || o->Kind != KIND_PLAYER
        || !(o->Type == MODEL_PLAYER || c->Change)
        || o->HiddenMesh == -2
        || o->Alpha <= 0.05f
        || c->ID[0] == '\0')
    {
        return false;
    }

    if (requireVisible && !o->Visible)
    {
        return false;
    }

    if (o->SubType == MODEL_XMAS_EVENT_CHA_DEER
        || o->SubType == MODEL_XMAS_EVENT_CHA_SNOWMAN
        || o->SubType == MODEL_XMAS_EVENT_CHA_SSANTA)
    {
        return false;
    }

    return true;
}

bool IsAndroidPlayerCommandCandidate(int characterIndex, AndroidPlayerCommandMode mode, bool requireVisible)
{
    if (!IsAndroidTradeTargetAvailable(characterIndex, requireVisible))
    {
        return false;
    }

    CHARACTER* c = &CharactersClient[characterIndex];
    switch (mode)
    {
    case AndroidPlayerCommandMode::Guild:
        return c->GuildMarkIndex >= 0 && c->GuildStatus == G_MASTER;
    case AndroidPlayerCommandMode::Duel:
        return !g_DuelMgr.IsDuelEnabled()
            || g_DuelMgr.IsDuelPlayer(c, DUEL_ENEMY) == TRUE;
    default:
        return true;
    }
}

bool IsAndroidTradeCandidate(int characterIndex)
{
    return IsAndroidPlayerCommandCandidate(characterIndex, g_androidTradePicker.mode, true);
}

int GetAndroidTradeDistance2(const CHARACTER* c)
{
    if (c == nullptr || Hero == nullptr)
    {
        return std::numeric_limits<int>::max();
    }

    const int dx = c->PositionX - Hero->PositionX;
    const int dy = c->PositionY - Hero->PositionY;
    return (dx * dx) + (dy * dy);
}

bool IsAndroidTradeTargetClose(const CHARACTER* c)
{
    if (c == nullptr || Hero == nullptr)
    {
        return false;
    }

    return std::abs(c->PositionX - Hero->PositionX) <= MAX_DISTANCE_TILE
        && std::abs(c->PositionY - Hero->PositionY) <= MAX_DISTANCE_TILE;
}

bool GetAndroidTradeCandidateScreenPosition(const CHARACTER* c, int* outScreenX, int* outScreenY)
{
    if (c == nullptr || outScreenX == nullptr || outScreenY == nullptr)
    {
        return false;
    }

    const OBJECT* o = &c->Object;
    vec3_t position;
    Vector(
        o->Position[0],
        o->Position[1],
        o->Position[2] + o->BoundingBoxMax[2] + 60.0f,
        position);
    Projection(position, outScreenX, outScreenY);
    return true;
}

bool IsAndroidTradeCandidateOnGameScreen(const CHARACTER* c)
{
    if (c == nullptr || Hero == nullptr)
    {
        return false;
    }

    const int dx = std::abs(c->PositionX - Hero->PositionX);
    const int dy = std::abs(c->PositionY - Hero->PositionY);
    constexpr int kScreenAreaTileRadius = 18;
    if (dx <= kScreenAreaTileRadius
        && dy <= kScreenAreaTileRadius
        && ((dx * dx) + (dy * dy)) <= (kScreenAreaTileRadius * kScreenAreaTileRadius))
    {
        return true;
    }

    int screenX = 0;
    int screenY = 0;
    if (!GetAndroidTradeCandidateScreenPosition(c, &screenX, &screenY))
    {
        return false;
    }

    const int gameRight = std::clamp(GetScreenWidth(), 1, 640);
    const int gameBottom = static_cast<int>(kVirtualPadInputMaxY);
    constexpr int margin = 96;
    return screenX >= -margin
        && screenX <= (gameRight + margin)
        && screenY >= -margin
        && screenY <= (gameBottom + margin);
}

void RefreshAndroidTradePickerEntries()
{
    g_androidTradePicker.entryCount = 0;
    if (!IsVirtualPadAvailable() || CharactersClient == nullptr || Hero == nullptr)
    {
        return;
    }

    std::vector<AndroidTradePickerEntry> entries;
    entries.reserve(MAX_CHARACTERS_CLIENT);

    for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
    {
        if (!IsAndroidTradeCandidate(i))
        {
            continue;
        }

        CHARACTER* c = &CharactersClient[i];
        if (!IsAndroidTradeCandidateOnGameScreen(c))
        {
            continue;
        }

        AndroidTradePickerEntry entry{};
        entry.characterIndex = i;
        entry.key = c->Key;
        std::strncpy(entry.id, c->ID, MAX_ID_SIZE);
        entry.id[MAX_ID_SIZE] = '\0';
        entry.x = c->PositionX;
        entry.y = c->PositionY;
        entry.distance2 = GetAndroidTradeDistance2(c);
        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const AndroidTradePickerEntry& lhs, const AndroidTradePickerEntry& rhs)
    {
        if (lhs.distance2 != rhs.distance2)
        {
            return lhs.distance2 < rhs.distance2;
        }
        return std::strncmp(lhs.id, rhs.id, MAX_ID_SIZE) < 0;
    });

    const int count = std::min<int>(static_cast<int>(entries.size()), kAndroidTradePickerMaxEntries);
    g_androidTradePicker.entryCount = count;
    for (int i = 0; i < count; ++i)
    {
        g_androidTradePicker.entries[i] = entries[i];
    }
    ClampAndroidTradePickerScroll();
}

int ResolveAndroidPendingTradeTargetIndex()
{
    if (!g_androidTradePicker.autoMoving)
    {
        return -1;
    }

    const int pendingIndex = g_androidTradePicker.pendingIndex;
    if (pendingIndex >= 0
        && pendingIndex < MAX_CHARACTERS_CLIENT
        && CharactersClient != nullptr
        && CharactersClient[pendingIndex].Object.Live
        && CharactersClient[pendingIndex].Key == g_androidTradePicker.pendingKey)
    {
        return pendingIndex;
    }

    const int resolved = FindCharacterIndex(g_androidTradePicker.pendingKey);
    if (resolved >= 0 && resolved < MAX_CHARACTERS_CLIENT)
    {
        return resolved;
    }

    return -1;
}

void CancelAndroidTradeAutoMove(const char* reason)
{
    if (g_androidTradePicker.autoMoving)
    {
#if !defined(MU_ANDROID_DISABLE_LOG)
        LOGI("AndroidTrade: cancel reason=%s target=%s", reason ? reason : "unknown", g_androidTradePicker.pendingId);
#endif
    }

    g_androidTradePicker.autoMoving = false;
    g_androidTradePicker.pendingIndex = -1;
    g_androidTradePicker.pendingKey = 0;
    g_androidTradePicker.pendingId[0] = '\0';
    g_androidTradePicker.targetStartX = 0;
    g_androidTradePicker.targetStartY = 0;
    g_androidTradePicker.requestedTargetX = 0;
    g_androidTradePicker.requestedTargetY = 0;
    g_androidTradePicker.startedMs = 0;
    g_androidTradePicker.lastMoveMs = 0;
    g_androidTradePicker.pressedKey = 0;
}

void HideAndroidTradePicker()
{
    CancelAndroidTradeAutoMove("hide");
    g_androidTradePicker.visible = false;
    g_androidTradePicker.mode = AndroidPlayerCommandMode::None;
    g_androidTradePicker.entryCount = 0;
    g_androidTradePicker.scrollOffset = 0;
    g_androidTradePicker.dragging = false;
    g_androidTradePicker.dragMoved = false;
    g_androidTradePicker.dragFingerId = static_cast<SDL_FingerID>(-1);
    g_androidTradePicker.pressedKey = 0;
}

void CloseAndroidTradePickerView()
{
    g_androidTradePicker.visible = false;
    g_androidTradePicker.entryCount = 0;
    g_androidTradePicker.scrollOffset = 0;
    g_androidTradePicker.dragging = false;
    g_androidTradePicker.dragMoved = false;
    g_androidTradePicker.dragFingerId = static_cast<SDL_FingerID>(-1);
    g_androidTradePicker.pressedKey = 0;
}

bool TrySendAndroidTradeToIndex(int characterIndex)
{
    if (!IsAndroidTradeTargetAvailable(characterIndex, false))
    {
        return false;
    }

    CHARACTER* target = &CharactersClient[characterIndex];
    if (!IsAndroidTradeTargetClose(target))
    {
        return false;
    }

    SelectedCharacter = characterIndex;
    if (g_pCommandWindow == nullptr)
    {
        return false;
    }

    bool sent = false;
    if (g_androidTradePicker.mode == AndroidPlayerCommandMode::Party)
    {
        if (!CanAndroidSendPartyCommand())
        {
            return true;
        }
        sent = g_pCommandWindow->CommandParty(target->Key);
    }
    else if (g_androidTradePicker.mode == AndroidPlayerCommandMode::Guild)
    {
        if (!CanAndroidSendGuildCommand())
        {
            return true;
        }

        if (!IsAndroidPlayerCommandCandidate(characterIndex, AndroidPlayerCommandMode::Guild, false))
        {
            if (g_pChatListBox != nullptr)
            {
                g_pChatListBox->AddText("", GlobalText[507], SEASON3B::TYPE_SYSTEM_MESSAGE);
            }
            return true;
        }

        sent = g_pCommandWindow->CommandGuild(target);
    }
    else if (g_androidTradePicker.mode == AndroidPlayerCommandMode::Duel)
    {
        if (!CanAndroidSendDuelCommand())
        {
            return true;
        }

        const int result = g_pCommandWindow->CommandDual(target);
        sent = (result == 1 || result == 2);
        if (!sent && result != 0)
        {
            return true;
        }
    }
    else if (g_androidTradePicker.mode == AndroidPlayerCommandMode::Friend)
    {
        if (!CanAndroidSendFriendCommand())
        {
            return true;
        }

        sent = g_pCommandWindow->CommandAddFriend(target);
        if (!sent)
        {
            SendRequestAddFriend(target->ID);
            sent = true;
        }
    }
    else
    {
        sent = g_pCommandWindow->CommandTrade(target);
    }

    if (sent && g_pNewUISystem != nullptr)
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_COMMAND);
    }
    return sent;
}

bool RequestAndroidTradeAutoMove(int characterIndex, uint32_t nowMs)
{
    if (!IsAndroidTradeTargetAvailable(characterIndex, false) || Hero == nullptr)
    {
        return false;
    }

    CHARACTER* target = &CharactersClient[characterIndex];
    Hero->MovementType = MOVEMENT_MOVE;
    TargetX = target->PositionX;
    TargetY = target->PositionY;

    if (!PathFinding2(
            Hero->PositionX,
            Hero->PositionY,
            TargetX,
            TargetY,
            &Hero->Path,
            static_cast<float>(MAX_DISTANCE_TILE)))
    {
        return false;
    }

    SendMove(Hero, &Hero->Object);
    g_androidTradePicker.requestedTargetX = TargetX;
    g_androidTradePicker.requestedTargetY = TargetY;
    g_androidTradePicker.lastMoveMs = nowMs;
    return true;
}

void StartAndroidTradeAutoMove(int characterIndex)
{
    if (!IsAndroidTradeTargetAvailable(characterIndex, false))
    {
        CancelAndroidTradeAutoMove("invalid-start");
        return;
    }

    CHARACTER* target = &CharactersClient[characterIndex];
    const uint32_t nowMs = MU_MobileGetTicks();

    g_androidTradePicker.autoMoving = true;
    g_androidTradePicker.pendingIndex = characterIndex;
    g_androidTradePicker.pendingKey = target->Key;
    std::strncpy(g_androidTradePicker.pendingId, target->ID, MAX_ID_SIZE);
    g_androidTradePicker.pendingId[MAX_ID_SIZE] = '\0';
    g_androidTradePicker.targetStartX = target->PositionX;
    g_androidTradePicker.targetStartY = target->PositionY;
    g_androidTradePicker.startedMs = nowMs;
    g_androidTradePicker.lastMoveMs = 0;
    SelectedCharacter = characterIndex;

    if (g_pNewUISystem != nullptr)
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_COMMAND);
    }
    CloseAndroidTradePickerView();

    if (!RequestAndroidTradeAutoMove(characterIndex, nowMs))
    {
        CancelAndroidTradeAutoMove("path-failed");
    }
}

void SelectAndroidTradePickerTarget(int characterIndex)
{
    if (!IsAndroidTradeTargetAvailable(characterIndex, false))
    {
        return;
    }

    CancelAndroidTradeAutoMove("new-select");

    if (TrySendAndroidTradeToIndex(characterIndex))
    {
        HideAndroidTradePicker();
        return;
    }

    StartAndroidTradeAutoMove(characterIndex);
}

void SelectAndroidTradePickerEntryByKey(SHORT key)
{
    if (key == 0)
    {
        return;
    }

    for (int i = 0; i < g_androidTradePicker.entryCount; ++i)
    {
        if (g_androidTradePicker.entries[i].key == key)
        {
            SelectAndroidTradePickerTarget(g_androidTradePicker.entries[i].characterIndex);
            return;
        }
    }

    const int characterIndex = FindCharacterIndex(key);
    SelectAndroidTradePickerTarget(characterIndex);
}

void SelectAndroidTradePickerEntry(int visibleRow)
{
    const int entryIndex = g_androidTradePicker.scrollOffset + visibleRow;
    if (visibleRow < 0 || entryIndex < 0 || entryIndex >= g_androidTradePicker.entryCount)
    {
        return;
    }

    SelectAndroidTradePickerEntryByKey(g_androidTradePicker.entries[entryIndex].key);
}

void UpdateAndroidTradeAutoMove()
{
    if (!g_androidTradePicker.autoMoving)
    {
        return;
    }

    if (!IsVirtualPadAvailable() || CharactersClient == nullptr || Hero == nullptr)
    {
        CancelAndroidTradeAutoMove("unavailable");
        return;
    }

    if (g_pNewUISystem != nullptr)
    {
        if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MINI_MAP))
        {
            g_pNewUISystem->Hide(SEASON3B::INTERFACE_MINI_MAP);
        }
        if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MOVEMAP))
        {
            g_pNewUISystem->Hide(SEASON3B::INTERFACE_MOVEMAP);
        }
    }

    const uint32_t nowMs = MU_MobileGetTicks();
    if ((nowMs - g_androidTradePicker.startedMs) >= kAndroidTradeAutoMoveTimeoutMs)
    {
        CancelAndroidTradeAutoMove("timeout");
        return;
    }

    const int characterIndex = ResolveAndroidPendingTradeTargetIndex();
    if (!IsAndroidTradeTargetAvailable(characterIndex, false))
    {
        CancelAndroidTradeAutoMove("target-gone");
        return;
    }

    CHARACTER* target = &CharactersClient[characterIndex];
    if (!IsAndroidTradeCandidateOnGameScreen(target))
    {
        CancelAndroidTradeAutoMove("target-offscreen");
        return;
    }

    if (Hero->Movement
        && g_androidTradePicker.lastMoveMs != 0
        && (TargetX != g_androidTradePicker.requestedTargetX
            || TargetY != g_androidTradePicker.requestedTargetY))
    {
        CancelAndroidTradeAutoMove("manual-move");
        return;
    }

    if (TrySendAndroidTradeToIndex(characterIndex))
    {
        HideAndroidTradePicker();
        return;
    }

    if ((nowMs - g_androidTradePicker.lastMoveMs) >= kAndroidTradeAutoMoveIntervalMs)
    {
        g_androidTradePicker.targetStartX = target->PositionX;
        g_androidTradePicker.targetStartY = target->PositionY;
        if (!RequestAndroidTradeAutoMove(characterIndex, nowMs))
        {
            CancelAndroidTradeAutoMove("path-refresh-failed");
        }
    }
}

bool ShowAndroidPlayerCommandPickerFromCommand(AndroidPlayerCommandMode mode)
{
    if (!IsVirtualPadAvailable() || gMapManager.InChaosCastle())
    {
        return false;
    }

    if (mode == AndroidPlayerCommandMode::Trade
        && CharacterAttribute != nullptr
        && CharacterAttribute->Level < TRADELIMITLEVEL)
    {
        if (g_pChatListBox != nullptr)
        {
            g_pChatListBox->AddText("", GlobalText[478], SEASON3B::TYPE_SYSTEM_MESSAGE);
        }
        return true;
    }

    if (mode == AndroidPlayerCommandMode::Party && !CanAndroidSendPartyCommand())
    {
        return true;
    }

    if (mode == AndroidPlayerCommandMode::Guild && !CanAndroidSendGuildCommand())
    {
        return true;
    }

    if (mode == AndroidPlayerCommandMode::Duel && !CanAndroidSendDuelCommand())
    {
        return true;
    }

    if (mode == AndroidPlayerCommandMode::Friend && !CanAndroidSendFriendCommand())
    {
        return true;
    }

    CancelAndroidTradeAutoMove("show");
    g_androidTradePicker.visible = true;
    g_androidTradePicker.mode = mode;
    g_androidTradePicker.scrollOffset = 0;
    g_androidTradePicker.dragging = false;
    g_androidTradePicker.dragMoved = false;
    g_androidTradePicker.dragFingerId = static_cast<SDL_FingerID>(-1);
    g_androidTradePicker.pressedKey = 0;
    RefreshAndroidTradePickerEntries();
    return true;
}

bool ShowAndroidTradePickerFromCommand()
{
    return ShowAndroidPlayerCommandPickerFromCommand(AndroidPlayerCommandMode::Trade);
}

bool ShowAndroidPartyPickerFromCommand()
{
    return ShowAndroidPlayerCommandPickerFromCommand(AndroidPlayerCommandMode::Party);
}

bool ShowAndroidGuildPickerFromCommand()
{
    return ShowAndroidPlayerCommandPickerFromCommand(AndroidPlayerCommandMode::Guild);
}

bool ShowAndroidDuelPickerFromCommand()
{
    return ShowAndroidPlayerCommandPickerFromCommand(AndroidPlayerCommandMode::Duel);
}

bool ShowAndroidFriendPickerFromCommand()
{
    return ShowAndroidPlayerCommandPickerFromCommand(AndroidPlayerCommandMode::Friend);
}

bool HandleAndroidTargetPickerFingerDown(const SDL_TouchFingerEvent& touch, float uiX, float uiY)
{
    if (!g_androidTargetPicker.visible)
    {
        return false;
    }

    // Forced, so a row tapped here is matched against what was actually drawn.
    RefreshAndroidTargetPickerEntries(true);

    const AndroidUiRect pickerRect = GetAndroidTargetPickerRect();
    if (!HitTestAndroidUiRect(uiX, uiY, pickerRect))
    {
        HideAndroidTargetPicker();
        return true;
    }

    if (HitTestAndroidUiRect(uiX, uiY, GetAndroidTargetPickerFooterRect()))
    {
        HideAndroidTargetPicker();
        PlayBuffer(SOUND_CLICK01);
        return true;
    }

    g_androidTargetPicker.dragging = false;
    g_androidTargetPicker.dragMoved = false;
    g_androidTargetPicker.dragFingerId = static_cast<SDL_FingerID>(-1);
    g_androidTargetPicker.pressedKey = 0;

    for (int row = 0; row < GetAndroidTargetPickerRowCount(); ++row)
    {
        if (HitTestAndroidUiRect(uiX, uiY, GetAndroidTargetPickerRowRect(row)))
        {
            const int entryIndex = g_androidTargetPicker.scrollOffset + row;
            if (entryIndex < g_androidTargetPicker.entryCount)
            {
                g_androidTargetPicker.dragging = true;
                g_androidTargetPicker.dragFingerId = touch.fingerId;
                g_androidTargetPicker.dragStartY = uiY;
                g_androidTargetPicker.dragLastY = uiY;
                g_androidTargetPicker.pressedKey = g_androidTargetPicker.entries[entryIndex].key;
            }
            return true;
        }
    }

    return true;
}

bool HandleAndroidTargetPickerFingerMotion(const SDL_TouchFingerEvent& touch)
{
    if (!g_androidTargetPicker.visible
        || !g_androidTargetPicker.dragging
        || g_androidTargetPicker.dragFingerId != touch.fingerId)
    {
        return false;
    }

    float uiX = 0.0f;
    float uiY = 0.0f;
    TouchToVirtualUi(touch, uiX, uiY);

    // Past this much travel the gesture is a scroll, and finger-up must not
    // also select the row it started on.
    const float totalDy = uiY - g_androidTargetPicker.dragStartY;
    if ((totalDy * totalDy) > 36.0f)
    {
        g_androidTargetPicker.dragMoved = true;
    }

    const float dy = uiY - g_androidTargetPicker.dragLastY;
    const int steps = static_cast<int>(std::fabs(dy) / kTargetPickerRowH);
    if (steps > 0)
    {
        if (dy < 0.0f)
        {
            g_androidTargetPicker.scrollOffset += steps;
            g_androidTargetPicker.dragLastY -= static_cast<float>(steps) * kTargetPickerRowH;
        }
        else
        {
            g_androidTargetPicker.scrollOffset -= steps;
            g_androidTargetPicker.dragLastY += static_cast<float>(steps) * kTargetPickerRowH;
        }
        ClampAndroidTargetPickerScroll();
    }

    return true;
}

bool HandleAndroidTargetPickerFingerUp(const SDL_TouchFingerEvent& touch)
{
    if (!g_androidTargetPicker.dragging
        || g_androidTargetPicker.dragFingerId != touch.fingerId)
    {
        return false;
    }

    float uiX = 0.0f;
    float uiY = 0.0f;
    TouchToVirtualUi(touch, uiX, uiY);

    const SHORT pressedKey = g_androidTargetPicker.pressedKey;
    const bool shouldSelect = !g_androidTargetPicker.dragMoved && pressedKey != 0;

    g_androidTargetPicker.dragging = false;
    g_androidTargetPicker.dragMoved = false;
    g_androidTargetPicker.dragFingerId = static_cast<SDL_FingerID>(-1);
    g_androidTargetPicker.pressedKey = 0;

    if (!shouldSelect || !g_androidTargetPicker.visible)
    {
        return true;
    }

    RefreshAndroidTargetPickerEntries(true);

    for (int row = 0; row < GetAndroidTargetPickerRowCount(); ++row)
    {
        const int entryIndex = g_androidTargetPicker.scrollOffset + row;
        if (entryIndex >= g_androidTargetPicker.entryCount
            || g_androidTargetPicker.entries[entryIndex].key != pressedKey)
        {
            continue;
        }

        // Only if the finger is still on the row it went down on.
        if (HitTestAndroidUiRect(uiX, uiY, GetAndroidTargetPickerRowRect(row)))
        {
            SetAndroidTargetLock(g_androidTargetPicker.entries[entryIndex].characterIndex);
            HideAndroidTargetPicker();
            PlayBuffer(SOUND_CLICK01);
        }
        return true;
    }

    return true;
}

// Tap with no lock opens the list; tap while locked releases it without
// reopening, so a second tap is "let go" and a third starts a new choice.
bool HandleTargetSelectButtonTap()
{
    if (IsAndroidTargetLockActive())
    {
        ClearAndroidTargetLock("button-toggle");
        HideAndroidTargetPicker();
        PlayBuffer(SOUND_CLICK01);
        return true;
    }

    if (g_androidTargetPicker.visible)
    {
        HideAndroidTargetPicker();
        return true;
    }

    ShowAndroidTargetPicker();
    PlayBuffer(SOUND_CLICK01);
    return true;
}

bool HandleAndroidTradePickerFingerDown(const SDL_TouchFingerEvent& touch, float uiX, float uiY)
{
    if (!g_androidTradePicker.visible)
    {
        return false;
    }

    RefreshAndroidTradePickerEntries();
    const AndroidUiRect pickerRect = GetAndroidTradePickerRect();
    if (!HitTestAndroidUiRect(uiX, uiY, pickerRect))
    {
        if (g_androidTradePicker.autoMoving)
        {
            HideAndroidTradePicker();
            return false;
        }

        HideAndroidTradePicker();
        return true;
    }

    const AndroidUiRect footerRect = GetAndroidTradePickerFooterRect();
    if (HitTestAndroidUiRect(uiX, uiY, footerRect))
    {
        HideAndroidTradePicker();
        PlayBuffer(SOUND_CLICK01);
        return true;
    }

    g_androidTradePicker.dragging = false;
    g_androidTradePicker.dragMoved = false;
    g_androidTradePicker.dragFingerId = static_cast<SDL_FingerID>(-1);
    g_androidTradePicker.pressedKey = 0;

    const int rowCount = GetAndroidTradePickerRowCount();
    for (int row = 0; row < rowCount; ++row)
    {
        if (HitTestAndroidUiRect(uiX, uiY, GetAndroidTradePickerRowRect(row)))
        {
            const int entryIndex = g_androidTradePicker.scrollOffset + row;
            if (entryIndex < g_androidTradePicker.entryCount)
            {
                g_androidTradePicker.dragging = true;
                g_androidTradePicker.dragMoved = false;
                g_androidTradePicker.dragFingerId = touch.fingerId;
                g_androidTradePicker.dragStartY = uiY;
                g_androidTradePicker.dragLastY = uiY;
                g_androidTradePicker.pressedKey = g_androidTradePicker.entries[entryIndex].key;
            }
            return true;
        }
    }

    return true;
}

bool HandleAndroidTradePickerFingerMotion(const SDL_TouchFingerEvent& touch)
{
    if (!g_androidTradePicker.visible
        || !g_androidTradePicker.dragging
        || g_androidTradePicker.dragFingerId != touch.fingerId)
    {
        return false;
    }

    float uiX = 0.0f;
    float uiY = 0.0f;
    TouchToVirtualUi(touch, uiX, uiY);

    const float totalDy = uiY - g_androidTradePicker.dragStartY;
    if ((totalDy * totalDy) > 36.0f)
    {
        g_androidTradePicker.dragMoved = true;
    }

    const float dy = uiY - g_androidTradePicker.dragLastY;
    const int steps = static_cast<int>(std::fabs(dy) / kAndroidTradePickerRowH);
    if (steps > 0)
    {
        if (dy < 0.0f)
        {
            g_androidTradePicker.scrollOffset += steps;
            g_androidTradePicker.dragLastY -= static_cast<float>(steps) * kAndroidTradePickerRowH;
        }
        else
        {
            g_androidTradePicker.scrollOffset -= steps;
            g_androidTradePicker.dragLastY += static_cast<float>(steps) * kAndroidTradePickerRowH;
        }
        ClampAndroidTradePickerScroll();
    }

    return true;
}

bool HandleAndroidTradePickerFingerUp(const SDL_TouchFingerEvent& touch)
{
    if (!g_androidTradePicker.dragging
        || g_androidTradePicker.dragFingerId != touch.fingerId)
    {
        return false;
    }

    float uiX = 0.0f;
    float uiY = 0.0f;
    TouchToVirtualUi(touch, uiX, uiY);

    const SHORT pressedKey = g_androidTradePicker.pressedKey;
    const bool shouldSelect = !g_androidTradePicker.dragMoved && pressedKey != 0;
    g_androidTradePicker.dragging = false;
    g_androidTradePicker.dragMoved = false;
    g_androidTradePicker.dragFingerId = static_cast<SDL_FingerID>(-1);
    g_androidTradePicker.pressedKey = 0;

    if (!shouldSelect || !g_androidTradePicker.visible)
    {
        return true;
    }

    RefreshAndroidTradePickerEntries();
    for (int row = 0; row < GetAndroidTradePickerRowCount(); ++row)
    {
        const int entryIndex = g_androidTradePicker.scrollOffset + row;
        if (entryIndex >= g_androidTradePicker.entryCount
            || g_androidTradePicker.entries[entryIndex].key != pressedKey)
        {
            continue;
        }

        if (HitTestAndroidUiRect(uiX, uiY, GetAndroidTradePickerRowRect(row)))
        {
            SelectAndroidTradePickerEntryByKey(pressedKey);
            PlayBuffer(SOUND_CLICK01);
        }
        return true;
    }

    return true;
}

bool HandleVirtualFingerDown(const SDL_TouchFingerEvent& touch)
{
    float uiX = 0.0f;
    float uiY = 0.0f;
    TouchToVirtualUi(touch, uiX, uiY);

    // Before anything else: this registers every finger so the pinch tracker
    // knows when a second one lands. It only claims the event on the frame the
    // gesture actually starts, so single touches carry on as normal.
    if (HandleAndroidPinchFingerDown(touch))
    {
        return true;
    }

    {
        CCharMakeWin& charMakeWin = CUIMng::Instance().m_CharMakeWin;
        if (charMakeWin.IsShow())
        {
            if (charMakeWin.HandleNameInputTap(uiX, uiY))
            {
                return true;
            }
        }
        else if (g_charNameInputActive)
        {
            g_charNameInputActive = false;
            MU_MobileStopTextInput();
        }
    }

    if (FocusVirtualLoginInputAt(uiX, uiY))
    {
        return true;
    }

    // Chat input must receive tap priority so Android IME pops up reliably.
    if (g_pNewUISystem != nullptr
        && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX)
        && g_pChatInputBox != nullptr)
    {
        const bool focused = FocusVirtualChatInputAt(uiX, uiY);
#if !defined(MU_ANDROID_DISABLE_LOG)
        LOGI(
            "CHATIME tap ui=(%.1f,%.1f) focused=%d hasFocusedText=%d sdlTextInput=%d",
            uiX,
            uiY,
            focused ? 1 : 0,
            AndroidHasFocusedTextInput() ? 1 : 0,
            MU_MobileIsTextInputActive() ? 1 : 0);
#endif
        if (focused)
        {
            return true;
        }
    }

    AndroidHideKeyboardForOutsideTap(uiX, uiY);

    if (TryAutoBindAndroidInventoryHotKeyItemAt(uiX, uiY))
    {
        return true;
    }

    // Does not claim the touch - a plain tap or a quick double-tap on this same
    // slot still needs to reach the ambient touch-to-mouse simulation below and
    // CNewUIInventoryCtrl's own tooltip/pickup handling, untouched. This only
    // ever does something if the press outlasts kAndroidBagHoldMs (UpdateAndroidBagHold,
    // polled from UpdateVirtualPadHolds).
    {
        SEASON3B::CNewUIInventoryCtrl* bagCtrl = nullptr;
        if (ITEM* bagItem = FindAndroidBagItemAndCtrlAt(uiX, uiY, &bagCtrl))
        {
            // CNewUIInventoryCtrl::UpdateProcess only resets EVENT_HOVER to
            // EVENT_NONE when the pointer moves off every filled slot, not when
            // it moves from one filled slot straight to another - so without
            // this, tapping a second, different item while the first item's
            // hover state was still active picked the second one straight up
            // (EVENT_HOVER + this tap's own release already satisfied the
            // pickup condition) instead of showing it. A tap that lands back on
            // the SAME item already being hovered is left alone - that is a
            // real double-tap-to-select and must still go through.
            if (bagCtrl != nullptr && bagCtrl->FindItemPointedSquareIndex() != bagItem)
            {
                bagCtrl->SetEventState(SEASON3B::CNewUIInventoryCtrl::EVENT_NONE);
            }

            StartAndroidBagHold(touch, uiX, uiY);
        }
    }

    // Equipped items: unlike the bag, PC's own pickup here has no hover-first
    // gate (see the constant block above), so a plain single tap has to be
    // actively claimed and held back, not just watched passively - otherwise
    // its own release would satisfy m_iPointedSlot != -1 && IsRelease(VK_LBUTTON)
    // and pick the item up immediately, the very thing "single tap shows it"
    // is supposed to prevent.
    {
        int equipSlot = -1;
        bool equipIsMuun = false;
        if (FindAndroidEquippedItemAt(uiX, uiY, &equipSlot, &equipIsMuun) != nullptr)
        {
            const uint32_t nowMs = MU_MobileGetTicks();
            const bool isDoubleTap = g_androidLastEquipTapUpMs > 0
                && g_androidLastEquipTapSlot == equipSlot
                && g_androidLastEquipTapIsMuun == equipIsMuun
                && (nowMs - g_androidLastEquipTapUpMs) <= kAndroidEquipDoubleTapMaxMs;

            if (isDoubleTap)
            {
                // Consumed so a third tap starts fresh rather than chaining.
                g_androidLastEquipTapUpMs = 0;
                FireAndroidEquipSelectPulse(uiX, uiY);
            }
            else
            {
                // MouseX/MouseY still need to land on the slot even though the
                // touch is claimed, because CNewUIMyInventory::Update() reads
                // them - unclaimed or not - to decide m_iPointedSlot, and that
                // is what the equipment tooltip renders every frame.
                MouseX = static_cast<int>(uiX);
                MouseY = static_cast<int>(uiY);
                StartAndroidEquipHold(touch, uiX, uiY, equipSlot, equipIsMuun);
            }
            return true;
        }
    }

    if (g_androidTradePicker.visible && HandleAndroidTradePickerFingerDown(touch, uiX, uiY))
    {
        return true;
    }

    // Early, beside the trade picker: while the panel is open it owns every
    // touch inside itself, before any world or pad control sees it.
    if (g_androidTargetPicker.visible && HandleAndroidTargetPickerFingerDown(touch, uiX, uiY))
    {
        return true;
    }

    if (g_androidTradePicker.autoMoving)
    {
        if (HitTestMiniMapToggleButton(uiX, uiY) || HitTestMapButton(uiX, uiY))
        {
            return true;
        }

        HideAndroidTradePicker();
    }

    if (!IsVirtualPadAvailable())
    {
        return false;
    }

    // Paired with the same check in RenderVirtualPad. The controls are not on
    // screen while a MU window is open, so returning false here hands the touch
    // to that window instead of letting an invisible button eat it. This sits
    // after the picker handlers above, which are windows themselves.
    if (IsAndroidGameWindowOpen())
    {
        return false;
    }

    // Before the pad controls: the tab strip sits above the chat log, clear of
    // them, and a tap there should switch channel rather than fall through.
    if (HandleAndroidChatTabTap(uiX, uiY))
    {
        return true;
    }

    // Directly after the tabs: same region of the screen, and a tap in the log
    // should start typing rather than fall through to the world behind it.
    if (HandleAndroidChatLogTap(uiX, uiY))
    {
        return true;
    }

    // Before the top control stack: the labelled bar sits above it in the
    // corner, and both are in the same region of the screen.
    const int topBarAction = HitTestVirtualTopBarButton(uiX, uiY);
    if (topBarAction != kTopBarActionNone)
    {
        const uint32_t nowMs = MU_MobileGetTicks();
        if ((nowMs - g_virtualLastUtilityTapMs) >= kVirtualUtilityButtonCooldownMs)
        {
            g_virtualLastUtilityTapMs = nowMs;

            if (topBarAction == kTopBarActionMenu)
            {
                ToggleVirtualRightPanelMode();
            }
            else if (topBarAction == kTopBarActionHelperPlay)
            {
                // Same call the desktop HOME key makes: the argument is the
                // current state, so passing it in flips the helper. Start is
                // refused with its own message box while the helper window is
                // open or the hero is in a safe zone, which is why this does
                // not try to validate anything itself.
                ToggleAndroidMuHelperRunning();
            }
            else if (topBarAction == kTopBarActionLocation)
            {
                // Same window the utility grid's MAP action opens.
                ToggleMapListByVirtualButton();
                PlayBuffer(SOUND_CLICK01);
            }
            else if (topBarAction == kTopBarActionRowToggle)
            {
                g_topBarRowIconsVisible = !g_topBarRowIconsVisible;
                g_topBarRowAnimActive = true;
                g_topBarRowAnimStartTick = GetTickCount();
                PlayBuffer(SOUND_CLICK01);
            }
            else
            {
                TriggerVirtualRightPanelUtilityAction(topBarAction);
            }
        }
        return true;
    }

    // Portrait avatar opens the character sheet. Shares the top bar's cooldown
    // because it is the same class of action - a window toggle that a double
    // report from the touch stack would open and immediately shut again.
    if (HitTestVirtualPortraitAvatar(uiX, uiY))
    {
        const uint32_t nowMs = MU_MobileGetTicks();
        if ((nowMs - g_virtualLastUtilityTapMs) >= kVirtualUtilityButtonCooldownMs)
        {
            g_virtualLastUtilityTapMs = nowMs;
            TriggerVirtualRightPanelUtilityAction(kVirtualRightPanelUtilityActionCharacter);
        }
        return true;
    }

    if (HandleVirtualTopControlTap(uiX, uiY))
    {
        return true;
    }

    if (HandleVirtualRightPanelTap(uiX, uiY))
    {
        return true;
    }

    const int mirrorHotKeySlot = HitTestVirtualMirrorHotKeySlot(uiX, uiY);
    if (mirrorHotKeySlot >= 0)
    {
        // Only recorded here - see HandleVirtualFingerUp, which decides between
        // drinking and rebinding based on how long the finger stayed down.
        g_androidHotKeyPress.fingerId = touch.fingerId;
        g_androidHotKeyPress.slot = mirrorHotKeySlot;
        g_androidHotKeyPress.downMs = MU_MobileGetTicks();
        g_androidHotKeyPress.downX = uiX;
        g_androidHotKeyPress.downY = uiY;
        return true;
    }

    // Ahead of the attack button: GetVirtualButtonHitRadius pads that circle by
    // 8, so testing it first would let it claim taps meant for this one.
    if (HitTestAndroidUiRect(uiX, uiY, GetTargetSelectButtonRect()))
    {
        return HandleTargetSelectButtonTap();
    }

    if (HitTestComboToggle(uiX, uiY))
    {
        g_virtualComboEnabled = !g_virtualComboEnabled;
        g_virtualComboStep = 0;
        g_virtualSkillSlotsDirty = true;
        SaveVirtualSkillSlots();
        PlayBuffer(SOUND_CLICK01);
        LOGI("VirtualPad: auto-combo %s", g_virtualComboEnabled ? "on" : "off");
        return true;
    }

    if (!kShowVirtualAttackButton && !kShowVirtualSkillButtons)
    {
        return HandleVirtualJoystickFingerDown(touch);
    }

    if (HitTestVirtualAttackButton(uiX, uiY) == kVirtualAttackButton)
    {
        const uint32_t nowMs = MU_MobileGetTicks();

        // A ground-targeted skill needs a map position, which a button press
        // alone cannot express. Turn the press into a drag-to-aim instead of
        // firing: no touch slot is claimed, so the hold-repeat loop leaves it
        // alone and the finger drives the reticle until it lifts.
        const int armedSkillIndex = GetVirtualOverlayHotKeySkillIndex(g_virtualSelectedSkillSlot);
        if (IsGroundTargetedSkillIndex(armedSkillIndex))
        {
            g_androidGroundAim.aiming = true;
            g_androidGroundAim.pendingCast = false;
            g_androidGroundAim.fingerId = touch.fingerId;
            g_androidGroundAim.uiX = uiX;
            g_androidGroundAim.uiY = uiY;
            g_androidGroundAim.skillIndex = armedSkillIndex;
            return true;
        }

        const int slot = AcquireActiveVirtualTouchSlot(touch.fingerId);
        if (slot >= 0)
        {
            g_activeVirtualTouches[slot].fingerId = touch.fingerId;
            g_activeVirtualTouches[slot].button = kVirtualAttackButton;
            g_activeVirtualTouches[slot].downMs = nowMs;
            g_activeVirtualTouches[slot].lastRepeatMs = g_activeVirtualTouches[slot].downMs;
        }

        TriggerVirtualAttackButtonPress();
        return true;
    }

    const int skillButton = HitTestVirtualSkillButton(uiX, uiY);
    if (skillButton >= kVirtualSkillButtonBase)
    {
        const int skillSlot = skillButton - kVirtualSkillButtonBase;

        // The last button in the arc is the picker, not a skill.
        if (skillSlot == kVirtualSkillSelectorVisualSlot)
        {
            ToggleVirtualSkillPickerByTouch();
            PlayBuffer(SOUND_CLICK01);
            return true;
        }

        const int hotKeySlot = GetVirtualOverlayHotKeySlot(skillSlot);
        const uint32_t nowMs = MU_MobileGetTicks();
        const int pendingSkill = GetPendingVirtualAssignSkillIndex(nowMs);
        if (hotKeySlot >= 0 && IsVirtualOverlayHotKeySkillIndex(pendingSkill) && g_pSkillList != nullptr)
        {
            g_pSkillList->SetHotKey(hotKeySlot, pendingSkill);
            if (Hero != nullptr)
            {
                Hero->CurrentSkill = static_cast<BYTE>(pendingSkill);
            }
            if (g_pSkillList != nullptr)
            {
                g_pSkillList->SetAndroidTouchAssignSkillIndex(-1);
                g_pSkillList->SetSkillPickerOpen(false);
            }
            DeactivateVirtualAssignMode("assigned-slot");
            g_virtualAssignPickerSkillIndex = -1;
            g_virtualAssignConsumedForPickerSkill = true;
            g_virtualAssignConsumedForPickerSession = true;
        }
        else
        {
            // Arms the slot instead of casting it. The skill only goes off when
            // the attack button in the middle of the arc is pressed, so aiming
            // and firing are two separate deliberate taps.
            //
            // No touch slot is claimed here: nothing is being held down, so the
            // hold-to-repeat loop has no business tracking this finger.
            if (g_virtualSelectedSkillSlot == skillSlot)
            {
                g_virtualSelectedSkillSlot = -1;   // tap again to go back to weapon
            }
            else
            {
                g_virtualSelectedSkillSlot = skillSlot;

                // Mirror it onto CurrentSkill so the rest of the UI shows the
                // same armed skill the overlay is highlighting.
                const int hotKeySkillIndex = GetVirtualOverlayHotKeySkillIndex(skillSlot);
                if (Hero != nullptr && IsValidSkillIndex(hotKeySkillIndex))
                {
                    Hero->CurrentSkill = static_cast<BYTE>(hotKeySkillIndex);
                }
            }

            PlayBuffer(SOUND_CLICK01);
        }
        return true;
    }

    return HandleVirtualJoystickFingerDown(touch);
}

bool HandleVirtualFingerMotion(const SDL_TouchFingerEvent& touch)
{
    // Keeps both tracked finger positions current, and owns the motion outright
    // once the pinch is running so the joystick cannot also act on it.
    if (HandleAndroidPinchFingerMotion(touch))
    {
        return true;
    }

    // Does not claim the touch - see StartAndroidBagHold. Sliding off the slot
    // just abandons the hold, the same as sliding off a hotkey slot below does.
    UpdateAndroidBagHoldMotion(touch);

    // The equipment hold IS claimed at finger-down (StartAndroidEquipHold), so
    // its motion must stay claimed too - sliding off the slot abandons the
    // hold rather than unequipping wherever the finger ends up, or falling
    // through to the joystick.
    if (UpdateAndroidEquipHoldMotion(touch))
    {
        return true;
    }

    // Sliding off a hotkey slot abandons the press, so a stray drag across the
    // consumable row cannot drink anything.
    if (g_androidHotKeyPress.slot >= 0 && g_androidHotKeyPress.fingerId == touch.fingerId)
    {
        float moveX = 0.0f;
        float moveY = 0.0f;
        TouchToVirtualUi(touch, moveX, moveY);
        const float dx = moveX - g_androidHotKeyPress.downX;
        const float dy = moveY - g_androidHotKeyPress.downY;
        if (((dx * dx) + (dy * dy)) > (kHotKeyPressMoveCancelUi * kHotKeyPressMoveCancelUi))
        {
            g_androidHotKeyPress = AndroidHotKeyPressState{};
        }
        return true;
    }

    if (HandleAndroidTradePickerFingerMotion(touch))
    {
        return true;
    }

    if (HandleAndroidTargetPickerFingerMotion(touch))
    {
        return true;
    }

    // Ground aim owns its finger until it lifts, so the reticle follows the
    // drag instead of the joystick picking the movement up.
    if (g_androidGroundAim.aiming && g_androidGroundAim.fingerId == touch.fingerId)
    {
        TouchToVirtualUi(touch, g_androidGroundAim.uiX, g_androidGroundAim.uiY);
        return true;
    }

    if (FindActiveVirtualTouchSlot(touch.fingerId) >= 0)
    {
        return true;
    }

    return HandleVirtualJoystickFingerMotion(touch);
}

bool HandleVirtualFingerUp(const SDL_TouchFingerEvent& touch)
{
    // Unregisters the finger either way; only swallows the event when it ended
    // a live pinch, so ordinary releases still reach the handlers below.
    if (HandleAndroidPinchFingerUp(touch))
    {
        return true;
    }

    // Does not claim the touch - see StartAndroidBagHold. If the hold already
    // fired, MouseLButton was forced false when it did (UpdateAndroidBagHold),
    // so the ambient tap/double-tap machinery below has nothing left to react
    // to; this just clears the pending state either way.
    FinishAndroidBagHoldFingerUp(touch.fingerId);

    // The equipment hold IS claimed (StartAndroidEquipHold) - if it fired, the
    // right-click pulse already did its job (UpdateAndroidEquipHold); if not,
    // this was a completed short tap, recorded here so the next tap on the
    // same slot within kAndroidEquipDoubleTapMaxMs is recognised as the double-
    // tap that selects it (StartAndroidEquipHold reads these back).
    if (FinishAndroidEquipHoldFingerUp(touch.fingerId))
    {
        return true;
    }

    // Resolve a Q/W/E/R press. Quick tap on a filled slot drinks it. Tapping an
    // empty '+' slot arms that slot and opens the bag so the next consumable
    // tapped in there is assigned to it. Holding a filled slot clears it - to
    // put something else there, clear it first and then tap the now-empty '+'.
    if (g_androidHotKeyPress.slot >= 0 && g_androidHotKeyPress.fingerId == touch.fingerId)
    {
        const int slot = g_androidHotKeyPress.slot;
        const uint32_t heldMs = MU_MobileGetTicks() - g_androidHotKeyPress.downMs;
        g_androidHotKeyPress = AndroidHotKeyPressState{};

        const bool isEmpty = (GetVirtualMirrorHotKeyItem(slot) == nullptr);
        if (isEmpty)
        {
            g_androidPendingHotKeyBindSlot = slot;
            if (g_pNewUISystem != nullptr
                && !g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY))
            {
                g_pNewUISystem->Show(SEASON3B::INTERFACE_INVENTORY);
            }
            PlayBuffer(SOUND_CLICK01);
        }
        else if (heldMs >= kHotKeyRebindHoldMs)
        {
            if (g_pMainFrame != nullptr && slot >= 0 && slot < kVirtualMirrorHotKeySlotCount)
            {
                // The type clear alone does not make GetVirtualMirrorHotKeyItem
                // (and so isEmpty above) actually see this as empty next time -
                // GetHotKeyItemIndex falls back to category matching and would
                // just find and keep using some other potion of the same kind
                // still in the bag. g_androidHotKeySlotCleared is what really
                // empties it; see its declaration.
                g_pMainFrame->SetItemHotKey(kVirtualMirrorHotKeys[slot], -1, 0);
                g_androidHotKeySlotCleared[slot] = true;
                PlayBuffer(SOUND_CLICK01);
            }
        }
        else
        {
            UseVirtualMirrorHotKeySlot(slot);
        }
        return true;
    }

    if (HandleAndroidTradePickerFingerUp(touch))
    {
        return true;
    }

    if (HandleAndroidTargetPickerFingerUp(touch))
    {
        return true;
    }

    if (g_androidGroundAim.aiming && g_androidGroundAim.fingerId == touch.fingerId)
    {
        float upX = g_androidGroundAim.uiX;
        float upY = g_androidGroundAim.uiY;
        TouchToVirtualUi(touch, upX, upY);

        g_androidGroundAim.uiX = upX;
        g_androidGroundAim.uiY = upY;
        g_androidGroundAim.aiming = false;

        // Handed to the scene phase rather than cast here - see
        // UpdateAndroidGroundAimCast. Two frames of settle so the render phase
        // has definitely turned the parked MouseX/MouseY into a pick ray.
        g_androidGroundAim.pendingCast = true;
        g_androidGroundAim.settleFrames = 2;
        return true;
    }

    const int slot = FindActiveVirtualTouchSlot(touch.fingerId);
    if (slot >= 0)
    {
        ClearActiveVirtualTouchSlot(slot);
        return true;
    }

    return HandleVirtualJoystickFingerUp(touch);
}

void ReleaseAndroidLongPressRightButton()
{
    MouseRButtonPush = false;
    if (MouseRButton)
    {
        MouseRButtonPop = true;
    }
    MouseRButton = false;
}

void ClearAndroidLongPressRightClick(bool releaseRightButton)
{
    if (releaseRightButton && g_androidLongPressRightClick.fired)
    {
        ReleaseAndroidLongPressRightButton();
    }
    g_androidLongPressRightClick = PendingAndroidLongPressRightClick{};
}

bool IsAndroidLongPressRightClickTarget(int characterIndex)
{
    if (SceneFlag != MAIN_SCENE
        || CharactersClient == nullptr
        || characterIndex < 0
        || characterIndex >= MAX_CHARACTERS_CLIENT)
    {
        return false;
    }

    const int heroIndex = GetHeroCharacterIndex();
    if (characterIndex == heroIndex)
    {
        return false;
    }

    CHARACTER* c = &CharactersClient[characterIndex];
    OBJECT* o = &c->Object;
    if (c == Hero || c->Dead > 0 || !o->Live || !o->Visible || o->Alpha <= 0.05f)
    {
        return false;
    }

    return o->Kind == KIND_PLAYER || o->Kind == KIND_MONSTER || o->Kind == KIND_NPC;
}

void StartAndroidLongPressRightClick(const SDL_TouchFingerEvent& touch)
{
    if (!IsVirtualPadAvailable())
    {
        ClearAndroidLongPressRightClick(false);
        return;
    }

    g_androidLongPressRightClick = PendingAndroidLongPressRightClick{};
    g_androidLongPressRightClick.active = true;
    g_androidLongPressRightClick.fingerId = touch.fingerId;
    g_androidLongPressRightClick.downMs = MU_MobileGetTicks();
    g_androidLongPressRightClick.startNX = touch.x;
    g_androidLongPressRightClick.startNY = touch.y;
    g_androidLongPressRightClick.startMouseX = MouseX;
    g_androidLongPressRightClick.startMouseY = MouseY;
}

void UpdateAndroidLongPressRightClickMotion(const SDL_TouchFingerEvent& touch)
{
    if (!g_androidLongPressRightClick.active
        || g_androidLongPressRightClick.fingerId != touch.fingerId
        || g_androidLongPressRightClick.fired)
    {
        return;
    }

    const float dx = (touch.x - g_androidLongPressRightClick.startNX) * 640.0f;
    const float dy = (touch.y - g_androidLongPressRightClick.startNY) * 480.0f;
    const float maxMove = kAndroidLongPressRightClickMoveCancelUi;
    if ((dx * dx + dy * dy) > (maxMove * maxMove))
    {
        ClearAndroidLongPressRightClick(false);
    }
}

bool FinishAndroidLongPressRightClickFingerUp(SDL_FingerID fingerId)
{
    if (!g_androidLongPressRightClick.active
        || g_androidLongPressRightClick.fingerId != fingerId)
    {
        return false;
    }

    const bool fired = g_androidLongPressRightClick.fired;
    ClearAndroidLongPressRightClick(fired);
    return fired;
}

void UpdateAndroidLongPressRightClick()
{
    if (g_androidLongPressRightClick.releaseRightOnNextUpdate)
    {
        ReleaseAndroidLongPressRightButton();
        g_androidLongPressRightClick.releaseRightOnNextUpdate = false;
    }

    if (!g_androidLongPressRightClick.active)
    {
        return;
    }

    if (!IsVirtualPadAvailable())
    {
        ClearAndroidLongPressRightClick(false);
        return;
    }

    if (g_androidLongPressRightClick.fired)
    {
        return;
    }

    const uint32_t nowMs = MU_MobileGetTicks();
    if ((nowMs - g_androidLongPressRightClick.downMs) < kAndroidLongPressRightClickMs)
    {
        return;
    }

    if (!IsAndroidLongPressRightClickTarget(SelectedCharacter))
    {
        return;
    }

    MouseX = g_androidLongPressRightClick.startMouseX;
    MouseY = g_androidLongPressRightClick.startMouseY;
    g_iNoMouseTime = 0;

    MouseLButtonPush = false;
    MouseLButtonPop = false;
    MouseLButtonDBClick = false;
    MouseLButton = false;

    MouseRButtonPop = false;
    MouseRButtonPush = !MouseRButton;
    MouseRButton = true;

    g_androidLongPressRightClick.fired = true;
    g_androidLongPressRightClick.releaseRightOnNextUpdate = true;

#if !defined(MU_ANDROID_DISABLE_LOG)
    LOGI(
        "INPUT long-press right-click target=%d mouse=%d,%d",
        SelectedCharacter,
        MouseX,
        MouseY);
#endif
}

void ClearAndroidBagHold()
{
    g_androidBagHold = PendingAndroidBagHold{};
}

// Called on finger-down for any touch that starts on a filled bag slot. Does
// not claim the touch - the caller keeps going into the normal touch-to-mouse
// simulation, which is what lets a tap or double-tap on the very same slot
// still reach CNewUIInventoryCtrl's own tooltip/pickup handling untouched.
void StartAndroidBagHold(const SDL_TouchFingerEvent& touch, float uiX, float uiY)
{
    g_androidBagHold = PendingAndroidBagHold{};
    g_androidBagHold.active = true;
    g_androidBagHold.fingerId = touch.fingerId;
    g_androidBagHold.downMs = MU_MobileGetTicks();
    g_androidBagHold.startNX = touch.x;
    g_androidBagHold.startNY = touch.y;
    g_androidBagHold.uiX = uiX;
    g_androidBagHold.uiY = uiY;
}

void UpdateAndroidBagHoldMotion(const SDL_TouchFingerEvent& touch)
{
    if (!g_androidBagHold.active
        || g_androidBagHold.fingerId != touch.fingerId
        || g_androidBagHold.fired)
    {
        return;
    }

    const float dx = (touch.x - g_androidBagHold.startNX) * 640.0f;
    const float dy = (touch.y - g_androidBagHold.startNY) * 480.0f;
    if ((dx * dx + dy * dy) > (kAndroidBagHoldMoveCancelUi * kAndroidBagHoldMoveCancelUi))
    {
        ClearAndroidBagHold();
    }
}

// Clears the pending hold on finger-up, wherever it came from. Returns whether
// it had already fired - by then MouseLButton has already been forced false
// (see UpdateAndroidBagHold), so the ambient tap/double-tap machinery has
// nothing left to react to and the caller does not need to suppress anything
// further; the return value exists for parity with the world-target long-press
// pattern this is modelled on and for callers that want to log it.
bool FinishAndroidBagHoldFingerUp(SDL_FingerID fingerId)
{
    if (!g_androidBagHold.active || g_androidBagHold.fingerId != fingerId)
    {
        return false;
    }

    const bool fired = g_androidBagHold.fired;
    ClearAndroidBagHold();
    return fired;
}

void UpdateAndroidBagHold()
{
    if (g_androidBagHold.releaseRightOnNextUpdate)
    {
        ReleaseAndroidLongPressRightButton();
        g_androidBagHold.releaseRightOnNextUpdate = false;
    }

    if (!g_androidBagHold.active || g_androidBagHold.fired)
    {
        return;
    }

    if ((MU_MobileGetTicks() - g_androidBagHold.downMs) < kAndroidBagHoldMs)
    {
        return;
    }

    // Re-checked fresh rather than trusting whatever was under the finger at
    // touch-down: GetPickedItem() != nullptr here means something is already on
    // the cursor (dragging it elsewhere while resting a second finger, or this
    // same finger having somehow already picked something up), and holding
    // longer should not also try to use/bind whatever is still sitting in the
    // slot underneath it.
    SEASON3B::CNewUIInventoryCtrl* ctrl = nullptr;
    ITEM* item = FindAndroidBagItemAndCtrlAt(g_androidBagHold.uiX, g_androidBagHold.uiY, &ctrl);
    if (item == nullptr)
    {
        ClearAndroidBagHold();
        return;
    }

    g_androidBagHold.fired = true;

    // A hold always wins over whatever the ambient hover/pickup state machine
    // was doing with this same press (it may already be mid-hover from an
    // earlier tap on this slot) - this press does not turn into a pickup once
    // it does. Also cancels the unrelated world-target long-press-to-right-
    // click, in case some previously selected character left it armed while
    // the bag happened to be open.
    //
    // The state reset matters, not just the button clear: if this item's
    // tooltip was already showing, m_EventState was already EVENT_HOVER
    // (NewUIInventoryCtrl.cpp), and clearing MouseLButton from true to false
    // right below IS a release edge as far as CNewKeyInput::ScanAsyncKeyState
    // is concerned - EVENT_HOVER + that release satisfies UpdateMouseEvent's
    // own pickup condition and grabs the item right here, emptying the slot
    // before the right-click pulse below ever reaches it. Confirmed exactly
    // this way: holding an item equipped fine until its tooltip was showing,
    // at which point holding it silently stopped equipping anything - the
    // slot was already empty (picked up) by the time HandleInventoryActions
    // went looking for what was in it.
    if (ctrl != nullptr)
    {
        ctrl->SetEventState(SEASON3B::CNewUIInventoryCtrl::EVENT_NONE);
    }
    MouseLButtonPush = false;
    MouseLButtonPop = false;
    MouseLButtonDBClick = false;
    MouseLButton = false;
    ClearAndroidLongPressRightClick(false);

    const int itemLevel = (item->Level >> 3) & 15;
    if (SEASON3B::CNewUIMyInventory::CanRegisterItemHotKey(item->Type))
    {
        // Consumables bind to a hotkey slot instead of being used directly -
        // this is what tapping one used to do; it moved to a hold so a plain
        // tap can show the item like everything else in the bag.
        const int boundSlot = AndroidBindVirtualPotionSlotFromInventory(item->Type, itemLevel);
        if (boundSlot >= 0)
        {
            g_androidHotKeySlotCleared[boundSlot] = false;
            PlayBuffer(SOUND_CLICK01);
        }
        return;
    }

    // Everything else: the same one-frame right-click pulse
    // StartAndroidLongPressRightClick/UpdateAndroidLongPressRightClick already
    // use for a world target, aimed at this slot instead - HandleInventoryActions
    // (NewUIMyInventory.cpp) re-finds the item at MouseX/MouseY itself and
    // uses/equips it exactly as a PC right-click would.
    MouseX = static_cast<int>(g_androidBagHold.uiX);
    MouseY = static_cast<int>(g_androidBagHold.uiY);
    g_iNoMouseTime = 0;

    MouseRButtonPop = false;
    MouseRButtonPush = !MouseRButton;
    MouseRButton = true;
    g_androidBagHold.releaseRightOnNextUpdate = true;
}

void ClearAndroidEquipHold()
{
    g_androidEquipHold = PendingAndroidEquipHold{};
}

void StartAndroidEquipHold(const SDL_TouchFingerEvent& touch, float uiX, float uiY, int slot, bool isMuun)
{
    g_androidEquipHold = PendingAndroidEquipHold{};
    g_androidEquipHold.active = true;
    g_androidEquipHold.fingerId = touch.fingerId;
    g_androidEquipHold.downMs = MU_MobileGetTicks();
    g_androidEquipHold.startNX = touch.x;
    g_androidEquipHold.startNY = touch.y;
    g_androidEquipHold.uiX = uiX;
    g_androidEquipHold.uiY = uiY;
    g_androidEquipHold.slot = slot;
    g_androidEquipHold.isMuun = isMuun;
}

bool UpdateAndroidEquipHoldMotion(const SDL_TouchFingerEvent& touch)
{
    if (!g_androidEquipHold.active || g_androidEquipHold.fingerId != touch.fingerId)
    {
        return false;
    }

    if (!g_androidEquipHold.fired)
    {
        const float dx = (touch.x - g_androidEquipHold.startNX) * 640.0f;
        const float dy = (touch.y - g_androidEquipHold.startNY) * 480.0f;
        if ((dx * dx + dy * dy) > (kAndroidEquipHoldMoveCancelUi * kAndroidEquipHoldMoveCancelUi))
        {
            ClearAndroidEquipHold();
        }
    }
    return true;
}

// Returns whether it was tracking this finger at all - see the forward
// declaration for why the caller claims on true regardless of whether a hold
// actually fired.
bool FinishAndroidEquipHoldFingerUp(SDL_FingerID fingerId)
{
    if (!g_androidEquipHold.active || g_androidEquipHold.fingerId != fingerId)
    {
        return false;
    }

    if (!g_androidEquipHold.fired)
    {
        // A completed short tap - the candidate half of a possible double-tap,
        // resolved on the NEXT finger-down rather than by waiting here to see
        // if one arrives (see HandleVirtualFingerDown's isDoubleTap check).
        g_androidLastEquipTapUpMs = MU_MobileGetTicks();
        g_androidLastEquipTapSlot = g_androidEquipHold.slot;
        g_androidLastEquipTapIsMuun = g_androidEquipHold.isMuun;
    }

    ClearAndroidEquipHold();
    return true;
}

void UpdateAndroidEquipHold()
{
    if (g_androidEquipHold.releaseRightOnNextUpdate)
    {
        ReleaseAndroidLongPressRightButton();
        g_androidEquipHold.releaseRightOnNextUpdate = false;
    }

    if (!g_androidEquipHold.active || g_androidEquipHold.fired)
    {
        return;
    }

    if ((MU_MobileGetTicks() - g_androidEquipHold.downMs) < kAndroidEquipHoldMs)
    {
        return;
    }

    // Re-checked fresh, the same as the bag's hold does: confirms the slot is
    // still filled, and (FindEquippedItemAtPt's own guard) that nothing is
    // already on the cursor.
    int slot = -1;
    bool isMuun = false;
    if (FindAndroidEquippedItemAt(g_androidEquipHold.uiX, g_androidEquipHold.uiY, &slot, &isMuun) == nullptr
        || slot != g_androidEquipHold.slot
        || isMuun != g_androidEquipHold.isMuun)
    {
        ClearAndroidEquipHold();
        return;
    }

    g_androidEquipHold.fired = true;

    // A hold always wins over a pending double-tap-to-select on this same
    // slot, and cancels the unrelated world-target long-press-to-right-click
    // in case a previously selected character left it armed.
    g_androidLastEquipTapUpMs = 0;
    MouseLButtonPush = false;
    MouseLButtonPop = false;
    MouseLButtonDBClick = false;
    MouseLButton = false;
    ClearAndroidLongPressRightClick(false);

    // One-frame right-click pulse aimed at this slot, driving
    // EquipmentWindowProcess's own IsRelease(VK_RBUTTON) unequip branch
    // (NewUIMyInventory.cpp) exactly as a real PC right-click there would -
    // that branch re-reads CharacterMachine->Equipment[iSourceIndex]/
    // EquipmentMuun itself from m_iPointedSlot, so nothing further needs to be
    // passed in beyond MouseX/MouseY.
    MouseX = static_cast<int>(g_androidEquipHold.uiX);
    MouseY = static_cast<int>(g_androidEquipHold.uiY);
    g_iNoMouseTime = 0;

    MouseRButtonPop = false;
    MouseRButtonPush = !MouseRButton;
    MouseRButton = true;
    g_androidEquipHold.releaseRightOnNextUpdate = true;
}

void UpdateAndroidEquipSelectPulse()
{
    if (!g_androidEquipSelectPulse.releaseOnNextUpdate)
    {
        return;
    }

    g_androidEquipSelectPulse.releaseOnNextUpdate = false;
    MouseLButtonPush = false;
    if (MouseLButton)
    {
        MouseLButtonPop = true;
    }
    MouseLButton = false;
}

// One-frame press-then-release pulse on VK_LBUTTON at (uiX, uiY) - see
// PendingAndroidEquipSelectPulse. The press half happens now; released on the
// next UpdateAndroidEquipSelectPulse tick, which is what turns it into an
// IsRelease(VK_LBUTTON) edge for EquipmentWindowProcess to see (a same-frame
// press+release would never register as either a press or a release, since
// CNewKeyInput::ScanAsyncKeyState only advances one state per call).
void FireAndroidEquipSelectPulse(float uiX, float uiY)
{
    MouseX = static_cast<int>(uiX);
    MouseY = static_cast<int>(uiY);
    g_iNoMouseTime = 0;

    MouseLButtonPop = false;
    MouseLButtonPush = !MouseLButton;
    MouseLButton = true;
    g_androidEquipSelectPulse.releaseOnNextUpdate = true;
}

bool IsVirtualButtonPressed(int button)
{
    for (const ActiveVirtualTouch& active : g_activeVirtualTouches)
    {
        if (active.button == button && active.fingerId != static_cast<SDL_FingerID>(-1))
        {
            return true;
        }
    }
    return false;
}

void UpdateVirtualPadHolds()
{
    UpdateAndroidLongPressRightClick();
    UpdateAndroidBagHold();
    UpdateAndroidEquipHold();
    UpdateAndroidEquipSelectPulse();
    UpdateAndroidTradeAutoMove();

    if (!IsVirtualPadAvailable())
    {
        for (int i = 0; i < static_cast<int>(g_activeVirtualTouches.size()); ++i)
        {
            ClearActiveVirtualTouchSlot(i);
        }
        ClearVirtualJoystick();
        return;
    }

    if (g_virtualRightPanelUtilityMode)
    {
        ClearVirtualCombatTouches();
        ApplyVirtualJoystickMovement();
        return;
    }

    const uint32_t nowMs = MU_MobileGetTicks();
    for (ActiveVirtualTouch& active : g_activeVirtualTouches)
    {
        if (active.fingerId == static_cast<SDL_FingerID>(-1)
            || active.button != kVirtualAttackButton)
        {
            continue;
        }

        const uint32_t repeatMs = IsAndroidComboActive()
            ? kVirtualComboRepeatMs
            : kVirtualAttackRepeatMs;

        if ((nowMs - active.lastRepeatMs) >= repeatMs)
        {
            active.lastRepeatMs = nowMs;

            // Same entry point as a tap, so holding the button keeps casting the
            // armed skill and keeps a combo advancing rather than reverting to
            // bare weapon swings.
            TriggerVirtualAttackButtonPress();
        }
    }

    ApplyVirtualJoystickMovement();
#if defined(MU_JOYSTICK_TRACE)
    TraceHeroTileChanges();
#endif
}

float UiToScreenX(float uiX)
{
    return uiX * (static_cast<float>(WindowWidth) / 640.0f);
}

float UiToScreenY(float uiY)
{
    return uiY * (static_cast<float>(WindowHeight) / 480.0f);
}

void DrawVirtualCircle(float uiX, float uiY, float uiRadius, float red, float green, float blue, float alpha, bool filled)
{
    const float centerX = UiToScreenX(uiX);
    const float centerY = static_cast<float>(WindowHeight) - UiToScreenY(uiY);
    const float radiusX = UiToScreenX(uiRadius);
    const float radiusY = UiToScreenY(uiRadius);
    constexpr int kSegments = 36;
    constexpr float kPi = 3.14159265358979323846f;

    glColor4f(red, green, blue, alpha);
    glBegin(filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
    if (filled)
    {
        glVertex2f(centerX, centerY);
    }

    for (int i = 0; i <= kSegments; ++i)
    {
        const float angle = (static_cast<float>(i) / static_cast<float>(kSegments))
            * 2.0f * kPi;
        const float px = centerX + std::cos(angle) * radiusX;
        const float py = centerY + std::sin(angle) * radiusY;
        glVertex2f(px, py);
    }
    glEnd();
}

void DrawVirtualCombatButtonFrame(float uiX, float uiY, float uiRadius, bool pressed, bool assignGlow)
{
    const float scale = pressed ? 0.94f : 1.0f;
    const float radius = uiRadius * scale;
    const float glowAlpha = assignGlow ? 0.28f : 0.0f;

    if (assignGlow)
    {
        DrawVirtualCircle(uiX, uiY, radius + 4.0f, 0.74f, 0.92f, 1.0f, glowAlpha, true);
    }

    DrawVirtualCircle(uiX, uiY, radius, 0.26f, 0.46f, 0.86f, pressed ? 0.92f : 0.78f, true);
    DrawVirtualCircle(uiX, uiY, radius * 0.76f, 0.10f, 0.18f, 0.34f, pressed ? 0.92f : 0.80f, true);
    glLineWidth(2.2f);
    DrawVirtualCircle(uiX, uiY, radius, 0.88f, 0.96f, 1.0f, pressed ? 1.0f : 0.84f, false);
    glLineWidth(1.0f);
}

void DrawVirtualRectFilled(float uiX, float uiY, float uiW, float uiH, float red, float green, float blue, float alpha)
{
    const float sx = UiToScreenX(uiX);
    const float sw = UiToScreenX(uiX + uiW) - sx;
    const float syT = static_cast<float>(WindowHeight) - UiToScreenY(uiY);
    const float syB = static_cast<float>(WindowHeight) - UiToScreenY(uiY + uiH);

    glColor4f(red, green, blue, alpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(sx, syB);
    glVertex2f(sx + sw, syB);
    glVertex2f(sx + sw, syT);
    glVertex2f(sx, syT);
    glEnd();
}

void DrawVirtualRectOutline(float uiX, float uiY, float uiW, float uiH, float red, float green, float blue, float alpha, float lineWidth)
{
    const float sx = UiToScreenX(uiX);
    const float sw = UiToScreenX(uiX + uiW) - sx;
    const float syT = static_cast<float>(WindowHeight) - UiToScreenY(uiY);
    const float syB = static_cast<float>(WindowHeight) - UiToScreenY(uiY + uiH);

    glLineWidth(lineWidth);
    glColor4f(red, green, blue, alpha);
    glBegin(GL_LINE_LOOP);
    glVertex2f(sx + 0.5f, syB + 0.5f);
    glVertex2f(sx + sw - 0.5f, syB + 0.5f);
    glVertex2f(sx + sw - 0.5f, syT - 0.5f);
    glVertex2f(sx + 0.5f, syT - 0.5f);
    glEnd();
    glLineWidth(1.0f);
}

// 鑺掗垾婵冨亾鑺掗垾婵冨亾 Horizontal status bar (fill from left to right) 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
// uiLeft/uiTop: UI-space top-left.  uiW/uiH: virtual width/height.
// ratio: 0=empty, 1=full.
void DrawVirtualBarH(float uiLeft, float uiTop, float uiW, float uiH, float ratio,
                     float fillR, float fillG, float fillB,
                     float bgR, float bgG, float bgB)
{
    const float sx  = UiToScreenX(uiLeft);
    const float sw  = UiToScreenX(uiLeft + uiW) - sx;
    const float syT = static_cast<float>(WindowHeight) - UiToScreenY(uiTop);
    const float syB = static_cast<float>(WindowHeight) - UiToScreenY(uiTop + uiH);
    // syB < syT in GL (Y-up)

    // Background 鑺掗埀顑解偓?solid dark
    glColor4f(bgR, bgG, bgB, 1.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(sx,      syB);
    glVertex2f(sx + sw, syB);
    glVertex2f(sx + sw, syT);
    glVertex2f(sx,      syT);
    glEnd();

    // Filled portion (left 鑺掗垾鐘偓?right) 鑺掗埀顑解偓?solid bright color
    const float fillW = sw * std::clamp(ratio, 0.0f, 1.0f);
    if (fillW > 0.5f)
    {
        glColor4f(fillR, fillG, fillB, 1.0f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(sx,         syB);
        glVertex2f(sx + fillW, syB);
        glVertex2f(sx + fillW, syT);
        glVertex2f(sx,         syT);
        glEnd();
    }

    // White border
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(sx + 0.5f,       syB + 0.5f);
    glVertex2f(sx + sw - 0.5f,  syB + 0.5f);
    glVertex2f(sx + sw - 0.5f,  syT - 0.5f);
    glVertex2f(sx + 0.5f,       syT - 0.5f);
    glEnd();
}

// 鑺掗垾婵冨亾鑺掗垾婵冨亾 UI texture loader (called once after GL context is ready) 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
// MuMainNativeActivity.extractGameAssets() copies assets/ui/*.png to the external
// files dir before native code runs. fopen("ui/map.png") therefore finds the
// file on the real filesystem (cwd = /sdcard/.../files).
// stbi_load_from_memory is used for decoding (STBI_NO_STDIO is set elsewhere).
static UITexture LoadUITextureAsset(const char* assetPath)
{
    UITexture tex;

    std::ifstream file(assetPath, std::ios::binary | std::ios::ate);
    if (!file)
    {
        LOGE("LoadUITextureAsset: fopen failed for '%s'", assetPath);
        return tex;
    }

    const std::streamsize size = file.tellg();
    if (size <= 0)
    {
        LOGE("LoadUITextureAsset: zero size for '%s'", assetPath);
        return tex;
    }

    std::vector<stbi_uc> buf(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(buf.data()), size))
    {
        LOGE("LoadUITextureAsset: read failed for '%s'", assetPath);
        return tex;
    }

    stbi_set_flip_vertically_on_load(1);   // flip so (0,0) = bottom-left for GL
    int w = 0, h = 0, comp = 0;
    stbi_uc* pixels = stbi_load_from_memory(buf.data(), static_cast<int>(size), &w, &h, &comp, 4);
    if (!pixels)
    {
        LOGE("LoadUITextureAsset: stbi decode failed for '%s'", assetPath);
        return tex;
    }

    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);

    tex.w = w; tex.h = h;
    LOGI("LoadUITextureAsset: OK '%s' (%dx%d texId=%u)", assetPath, w, h, tex.id);
    return tex;
}

static void EnsureUITextures()
{
    if (g_uiTexturesLoaded) return;
    g_uiTexturesLoaded = true;
    g_uiTex_map       = LoadUITextureAsset("ui/map.png");
    g_uiTex_minimap   = LoadUITextureAsset("ui/minimap.png");
    g_uiTex_attack    = LoadUITextureAsset("ui/attack.png");
    g_uiTex_skillbox  = LoadUITextureAsset("ui/skillbox.png");
    g_uiTex_skillline = LoadUITextureAsset("ui/skillline.png");
    g_uiTex_joystick1 = LoadUITextureAsset("ui/joystick1.png");
    g_uiTex_joystick2 = LoadUITextureAsset("ui/joystick2.png");
    g_uiTex_balo      = LoadUITextureAsset("ui/balo.png");
    g_uiTex_character = LoadUITextureAsset("ui/character.png");
    g_uiTex_setting   = LoadUITextureAsset("ui/setting.png");

    for (int slot = 0; slot < kTopBarButtonCount; ++slot)
    {
        g_uiTex_topBar[slot] = LoadUITextureAsset(kTopBarIconAssets[slot]);
    }

    for (int cls = 0; cls < kClassPortraitCount; ++cls)
    {
        g_uiTex_classPortrait[cls] = LoadUITextureAsset(kClassPortraitAssets[cls]);
    }
}

// Portrait for the hero's current base class, or character.png while that class
// has no art yet. GetBaseClass folds the evolutions down - a Soul Master still
// answers CLASS_WIZARD - so one portrait per base class covers every character.
const UITexture& GetClassPortraitTexture()
{
    if (Hero != nullptr)
    {
        const int baseClass = gCharacterManager.GetBaseClass(Hero->Class);
        if (baseClass >= 0 && baseClass < kClassPortraitCount
            && g_uiTex_classPortrait[baseClass].id != 0)
        {
            return g_uiTex_classPortrait[baseClass];
        }
    }
    return g_uiTex_character;
}

const UITexture& GetTopBarIconTexture(int slot)
{
    static const UITexture kEmpty{};
    if (slot < 0 || slot >= kTopBarButtonCount)
    {
        return kEmpty;
    }
    return g_uiTex_topBar[slot];
}

// Draw a PNG icon at the given UI rect 鑺掗埀顑解偓?NO background, NO border.
// Renders the texture as-is with correct alpha transparency.
// If the texture hasn't loaded yet, draws nothing.
static void DrawIconButton(float uiX, float uiY, float uiW, float uiH,
                           const UITexture& tex, float alpha = 1.0f,
                           float bgR = 0.0f, float bgG = 0.0f, float bgB = 0.0f)
{
    if (tex.id == 0) return;  // texture not loaded 鑺掗埀顑解偓?skip entirely

    const float sx  = UiToScreenX(uiX);
    const float sw  = UiToScreenX(uiX + uiW) - sx;
    const float syB = static_cast<float>(WindowHeight) - UiToScreenY(uiY + uiH);
    const float syT = static_cast<float>(WindowHeight) - UiToScreenY(uiY);

    // RenderNumber may have left GL_TEXTURE_2D enabled with an atlas bound 鑺掗埀顑解偓?reset it.
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw PNG texture directly 鑺掗埀顑解偓?no background, no border
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(sx,      syB);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(sx + sw, syB);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(sx + sw, syT);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(sx,      syT);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    // Restore additive blend expected by the rest of the virtual pad
    glBlendFunc(GL_ONE, GL_ONE);

    // Everything above drove GL directly, so ZzzOpenglUtil's shadows of that
    // state are now lying: it still believes texturing is on, the old texture is
    // bound and the blend mode is unchanged. Left stale, the next
    // EnableAlphaTest() skips its glEnable(GL_TEXTURE_2D) and the next
    // BindTexture() skips its bind - which is what drew the top bar labels as
    // solid untextured blocks. Invalidate them so the helpers do real work.
    TextureEnable  = false;
    CachTexture    = 0x7FFFFFFF;
    AlphaBlendType = -1;
}

static void DrawIconButtonUv(float uiX, float uiY, float uiW, float uiH,
                             const UITexture& tex,
                             float u0, float v0, float uW, float vH,
                             float alpha = 1.0f)
{
    if (tex.id == 0) return;

    const float sx  = UiToScreenX(uiX);
    const float sw  = UiToScreenX(uiX + uiW) - sx;
    const float syB = static_cast<float>(WindowHeight) - UiToScreenY(uiY + uiH);
    const float syT = static_cast<float>(WindowHeight) - UiToScreenY(uiY);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(u0,      v0);      glVertex2f(sx,      syB);
    glTexCoord2f(u0 + uW, v0);      glVertex2f(sx + sw, syB);
    glTexCoord2f(u0 + uW, v0 + vH); glVertex2f(sx + sw, syT);
    glTexCoord2f(u0,      v0 + vH); glVertex2f(sx,      syT);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glBlendFunc(GL_ONE, GL_ONE);

    // Same shadow invalidation as DrawIconButton - this drives GL directly too,
    // so ZzzOpenglUtil's trackers no longer match reality.
    TextureEnable  = false;
    CachTexture    = 0x7FFFFFFF;
    AlphaBlendType = -1;
}

static void DrawVirtualTopRightTextButton(const AndroidUiRect& rect, const TCHAR* label, bool active)
{
    const float fillR = active ? 0.12f : 0.02f;
    const float fillG = active ? 0.22f : 0.04f;
    const float fillB = active ? 0.38f : 0.08f;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawVirtualRectFilled(rect.x, rect.y, rect.w, rect.h, 0.01f, 0.02f, 0.05f, 0.92f);
    DrawVirtualRectFilled(rect.x + 2.0f, rect.y + 2.0f, rect.w - 4.0f, rect.h - 4.0f, fillR, fillG, fillB, 0.88f);
    DrawVirtualRectOutline(rect.x, rect.y, rect.w, rect.h, active ? 1.0f : 0.70f, active ? 0.82f : 0.86f, active ? 0.28f : 1.0f, 0.98f, 2.0f);

    TextDraw(
        g_hFontBold != nullptr ? g_hFontBold : g_hFont,
        static_cast<int>(rect.x),
        static_cast<int>(rect.y + rect.h * 0.5f - 5.0f),
        active ? 0xFFD86AFF : 0xFFFFFFFF,
        0x0,
        static_cast<int>(rect.w),
        0,
        3,
        "%s",
        label);
}

// 鑺掗垾婵冨亾鑺掗垾婵冨亾 Map button (top-right companion to minimap) 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
static void ConfigureVirtualSkillIconNoBlendState();

void DrawVirtualMapButton()
{
}

void RenderVirtualTopRightControls()
{
    if (SceneFlag != MAIN_SCENE || g_pNewUISystem == nullptr || AndroidHasFocusedTextInput())
    {
        return;
    }

    EnsureUITextures();
    BeginBitmap();

    if (IsMiniMapToggleAvailable())
    {
        const AndroidUiRect miniRect = GetMiniMapButtonRect();
        if (g_uiTex_minimap.id != 0)
        {
            DrawIconButton(miniRect.x, miniRect.y, miniRect.w, miniRect.h, g_uiTex_minimap, 1.0f);
        }
        else
        {
            DrawVirtualTopRightTextButton(miniRect, _T("MINI"), g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_MINI_MAP));
        }
    }

    EndBitmap();
}

void DrawVirtualChatUtilityButton()
{
    if (!kShowVirtualChatQuickButton)
    {
        return;
    }

    if (SceneFlag != MAIN_SCENE || g_pNewUISystem == nullptr)
    {
        return;
    }

    const AndroidUiRect rect = GetVirtualUtilityButtonRect(kVirtualUtilityButtonChat);
    const bool isActive = IsVirtualUtilityButtonActive(kVirtualUtilityButtonChat);
    const float uiCx = rect.x + (rect.w * 0.5f);
    const float uiCy = rect.y + (rect.h * 0.5f);
    const float uiRadius = rect.w * 0.5f;
    const float cx = UiToScreenX(uiCx);
    const float cy = static_cast<float>(WindowHeight) - UiToScreenY(uiCy);
    const float rx = UiToScreenX(uiRadius);
    const float ry = UiToScreenY(uiRadius);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(isActive ? 0.56f : 0.06f, isActive ? 0.20f : 0.06f, isActive ? 0.10f : 0.06f, isActive ? 0.94f : 0.88f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 40; ++i)
    {
        const float angle = (static_cast<float>(i) / 40.0f) * 6.28318530718f;
        glVertex2f(cx + (std::cos(angle) * rx), cy + (std::sin(angle) * ry));
    }
    glEnd();

    glLineWidth(2.4f);
    glColor4f(isActive ? 1.0f : 0.94f, isActive ? 0.88f : 0.94f, isActive ? 0.58f : 0.94f, 0.98f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 40; ++i)
    {
        const float angle = (static_cast<float>(i) / 40.0f) * 6.28318530718f;
        glVertex2f(cx + (std::cos(angle) * rx), cy + (std::sin(angle) * ry));
    }
    glEnd();
    glLineWidth(1.0f);

    glColor4f(0.08f, 0.08f, 0.08f, 0.92f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 40; ++i)
    {
        const float angle = (static_cast<float>(i) / 40.0f) * 6.28318530718f;
        glVertex2f(cx + (std::cos(angle) * rx * 0.72f), cy + (std::sin(angle) * ry * 0.72f));
    }
    glEnd();

    glColor4f(isActive ? 1.0f : 0.92f, isActive ? 0.72f : 0.92f, isActive ? 0.22f : 0.92f, 0.92f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 40; ++i)
    {
        const float angle = (static_cast<float>(i) / 40.0f) * 6.28318530718f;
        glVertex2f(cx + (std::cos(angle) * rx * 0.72f), cy + (std::sin(angle) * ry * 0.72f));
    }
    glEnd();

    g_pRenderText->SetFont(g_hFixFont != nullptr ? g_hFixFont : g_hFont);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(isActive ? CLRDW_BR_ORANGE : CLRDW_WHITE);
    g_pRenderText->RenderText(
        static_cast<int>(uiCx),
        static_cast<int>(uiCy - 5.0f),
        _T("chat"),
        0,
        0,
        RT3_WRITE_CENTER);
}

void RenderVirtualRightPanelButtonLabel(const AndroidUiRect& rect, const TCHAR* label, bool active)
{
    if (label == nullptr)
    {
        return;
    }

    const size_t labelLength = std::char_traits<TCHAR>::length(label);
    HFONT labelFont = g_hFontMini != nullptr ? g_hFontMini : (g_hFixFont != nullptr ? g_hFixFont : g_hFont);
    if (rect.w >= 24.0f || rect.h >= 20.0f)
    {
        labelFont = g_hFontBold != nullptr ? g_hFontBold : (g_hFixFont != nullptr ? g_hFixFont : g_hFont);
    }
    if ((rect.w <= 24.0f || rect.h <= 28.0f) && labelLength <= 2)
    {
        labelFont = g_hFontBold != nullptr ? g_hFontBold : (g_hFixFont != nullptr ? g_hFixFont : g_hFont);
    }

    const int textX = static_cast<int>(rect.x);
    const int textY = static_cast<int>(rect.y + ((rect.h <= 28.0f) ? 7.0f : 4.0f));
    const DWORD textColor = active ? 0xFFF2B6FF : 0xFFFFFFFF;

    TextDraw(labelFont, textX, textY, textColor, 0x0, static_cast<int>(rect.w), 0, 3, "%s", label);
}

void RenderVirtualRightPanelModeButtonLabel(const AndroidUiRect& rect)
{
    HFONT labelFont = g_hFontMini != nullptr ? g_hFontMini : (g_hFixFont != nullptr ? g_hFixFont : g_hFont);
    const int textX = static_cast<int>(rect.x - 5.0f);
    const int textY = static_cast<int>(rect.y + 10.0f);
    const int textW = static_cast<int>(rect.w + 10.0f);

    TextDraw(labelFont, textX + 1, textY + 1, 0xC0000000, 0x0, textW, 0, 3, "%s", kVirtualRightPanelModeButtonLabel);
    TextDraw(labelFont, textX, textY, 0xFFFFFFFF, 0x0, textW, 0, 3, "%s", kVirtualRightPanelModeButtonLabel);
}

void DrawVirtualRightPanelButtonBox(const AndroidUiRect& rect, bool active)
{
    const float borderR = active ? 0.98f : 0.16f;
    const float borderG = active ? 0.82f : 0.28f;
    const float borderB = active ? 0.34f : 0.56f;
    const float fillR = active ? 0.16f : 0.06f;
    const float fillG = active ? 0.22f : 0.12f;
    const float fillB = active ? 0.34f : 0.22f;

    DrawVirtualRectFilled(rect.x, rect.y, rect.w, rect.h, 0.01f, 0.03f, 0.08f, 0.92f);
    DrawVirtualRectFilled(rect.x + 1.0f, rect.y + 1.0f, rect.w - 2.0f, rect.h - 2.0f, fillR, fillG, fillB, active ? 0.92f : 0.84f);
    DrawVirtualRectFilled(rect.x + 2.0f, rect.y + 2.0f, rect.w - 4.0f, 3.0f, 0.76f, 0.88f, 1.0f, active ? 0.22f : 0.14f);
    DrawVirtualRectOutline(rect.x, rect.y, rect.w, rect.h, borderR, borderG, borderB, active ? 0.98f : 0.86f, 1.8f);
    DrawVirtualRectOutline(rect.x + 1.0f, rect.y + 1.0f, rect.w - 2.0f, rect.h - 2.0f, 0.03f, 0.05f, 0.12f, 0.96f, 1.0f);
}

// Reticle for a ground-targeted skill while the finger is dragging. Drawn at
// the aim point itself, which is where the terrain pick will be taken from.
void RenderAndroidGroundAim()
{
    if (!g_androidGroundAim.aiming || !IsVirtualPadAvailable())
    {
        return;
    }

    const float cx = g_androidGroundAim.uiX;
    const float cy = g_androidGroundAim.uiY;

    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DrawVirtualCircle(cx, cy, 20.0f, 0.30f, 0.80f, 1.0f, 0.30f, true);
    DrawVirtualCircle(cx, cy, 20.0f, 0.60f, 0.92f, 1.0f, 0.95f, false);
    DrawVirtualCircle(cx, cy, 5.0f, 0.85f, 0.97f, 1.0f, 0.95f, true);

    // Cross hairs, so the exact point is readable against busy terrain.
    DrawVirtualRectFilled(cx - 30.0f, cy - 0.5f, 20.0f, 1.5f, 0.60f, 0.92f, 1.0f, 0.85f);
    DrawVirtualRectFilled(cx + 10.0f, cy - 0.5f, 20.0f, 1.5f, 0.60f, 0.92f, 1.0f, 0.85f);
    DrawVirtualRectFilled(cx - 0.5f, cy - 30.0f, 1.5f, 20.0f, 0.60f, 0.92f, 1.0f, 0.85f);
    DrawVirtualRectFilled(cx - 0.5f, cy + 10.0f, 1.5f, 20.0f, 0.60f, 0.92f, 1.0f, 0.85f);

    HFONT font = g_hFontMini != nullptr ? g_hFontMini : g_hFont;
    TextDraw(font,
             static_cast<int>(cx - 40.0f),
             static_cast<int>(cy + 24.0f),
             0xFFFFFFFF, 0x0, 80, 0, 3,
             "%s", "release to cast");

    EndBitmap();
}

// Tab index -> chat channel. The overflow tab has no fixed channel of its own.
SEASON3B::MESSAGE_TYPE GetChatTabMessageType(int tab)
{
    switch (tab)
    {
    case 0:  return SEASON3B::TYPE_ALL_MESSAGE;
    case 1:  return SEASON3B::TYPE_CHAT_MESSAGE;
    case 2:  return SEASON3B::TYPE_PARTY_MESSAGE;
    case 3:  return SEASON3B::TYPE_GUILD_MESSAGE;
    case 4:  return SEASON3B::TYPE_UNION_MESSAGE;   // alliance
    case 5:  return SEASON3B::TYPE_SYSTEM_MESSAGE;
    default: return SEASON3B::TYPE_ALL_MESSAGE;
    }
}

SEASON3B::CNewUIChatLogWindow* GetAndroidChatLog()
{
    return (g_pNewUISystem != nullptr) ? g_pNewUISystem->GetUI_NewChatLogWindow() : nullptr;
}

bool IsChatTabActive(int tab)
{
    SEASON3B::CNewUIChatLogWindow* pLog = GetAndroidChatLog();
    if (pLog == nullptr || tab < 0 || tab >= kChatTabCount)
    {
        return false;
    }

    // The overflow tab lights whenever the active channel is one without a tab.
    if (tab == kChatTabCount - 1)
    {
        const SEASON3B::MESSAGE_TYPE cur = pLog->GetCurrentMsgType();
        for (int i = 0; i < kChatTabCount - 1; ++i)
        {
            if (GetChatTabMessageType(i) == cur)
            {
                return false;
            }
        }
        return true;
    }

    return pLog->GetCurrentMsgType() == GetChatTabMessageType(tab);
}

void RenderAndroidChatTabs()
{
    if (!IsVirtualPadAvailable() || GetAndroidChatLog() == nullptr)
    {
        return;
    }

    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Backing behind the log area so the text stays readable over terrain, the
    // way the tabbed panel in the reference does.
    DrawVirtualRectFilled(kChatLogX - 4.0f, kChatTabsY + kChatTabH,
                          (kChatTabW + kChatTabGap) * kChatTabCount + 8.0f,
                          kChatLogBottomY - (kChatTabsY + kChatTabH) + 2.0f,
                          0.02f, 0.02f, 0.03f, 0.45f);

    HFONT font = g_hFontMini != nullptr ? g_hFontMini : g_hFont;

    for (int tab = 0; tab < kChatTabCount; ++tab)
    {
        const AndroidUiRect rect = GetChatTabRect(tab);
        const bool active = IsChatTabActive(tab);

        DrawVirtualRectFilled(rect.x, rect.y, rect.w, rect.h,
                              active ? 1.00f : 0.10f,
                              active ? 0.82f : 0.09f,
                              active ? 0.10f : 0.08f,
                              active ? 1.00f : 0.75f);
        DrawVirtualRectOutline(rect.x, rect.y, rect.w, rect.h,
                               active ? 1.00f : 0.35f,
                               active ? 0.92f : 0.30f,
                               active ? 0.40f : 0.22f,
                               active ? 1.00f : 0.85f,
                               active ? 1.5f : 1.0f);

        TextDraw(font,
                 static_cast<int>(rect.x),
                 static_cast<int>(rect.y + 4.0f),
                 active ? 0xFFFFFFFF : 0xFFC8C8C8,
                 0x0,
                 static_cast<int>(rect.w),
                 0, 3,
                 "%s", kChatTabLabels[tab]);
    }

    EndBitmap();
}

bool HandleAndroidChatTabTap(float uiX, float uiY)
{
    SEASON3B::CNewUIChatLogWindow* pLog = GetAndroidChatLog();
    if (pLog == nullptr || !IsVirtualPadAvailable())
    {
        return false;
    }

    for (int tab = 0; tab < kChatTabCount; ++tab)
    {
        if (!HitTestAndroidUiRect(uiX, uiY, GetChatTabRect(tab)))
        {
            continue;
        }

        if (tab == kChatTabCount - 1)
        {
            // Overflow: step through the channels with no tab of their own.
            static const SEASON3B::MESSAGE_TYPE kExtra[] = {
                SEASON3B::TYPE_WHISPER_MESSAGE,
                SEASON3B::TYPE_GM_MESSAGE,
                SEASON3B::TYPE_GENS_MESSAGE,
                SEASON3B::TYPE_ERROR_MESSAGE,
            };
            static int s_extraIndex = 0;

            pLog->ChangeMessage(kExtra[s_extraIndex]);
            s_extraIndex = (s_extraIndex + 1) % (sizeof(kExtra) / sizeof(kExtra[0]));
        }
        else
        {
            pLog->ChangeMessage(GetChatTabMessageType(tab));
        }

        PlayBuffer(SOUND_CLICK01);
        return true;
    }

    return false;
}

// Tap anywhere in the chat log to start typing; tap a player's name on a line to
// address them privately first.
//
// The desktop reaches the whisper through a right click on the line
// (NewUIChatLogWindow::UpdateMouseEvent, the SetWhsprID call), which touch can
// never produce - so the same thing is wired to a tap here. It is the name that
// answers, not the whole line: GetAndroidChatMessageIDAt measures the drawn
// "<name> : " and only reports a sender for a tap inside it, so tapping the
// message text still just opens the keyboard.
//
// The keyboard is not raised directly: showing the input box and giving it
// focus is enough, because the frame loop starts text input whenever a field
// has focus. Opening is deliberate rather than a toggle, so a second tap while
// typing does not dismiss the keyboard mid-message.
bool HandleAndroidChatLogTap(float uiX, float uiY)
{
    SEASON3B::CNewUIChatLogWindow* pLog = GetAndroidChatLog();
    if (pLog == nullptr || g_pNewUISystem == nullptr || !IsVirtualPadAvailable())
    {
        return false;
    }

    // Body of the log only. The tab strip directly above it is claimed by
    // HandleAndroidChatTabTap, which runs first.
    const float left = kChatLogX - 4.0f;
    const float right = left + ((kChatTabW + kChatTabGap) * kChatTabCount) + 8.0f;
    const float top = kChatTabsY + kChatTabH;
    if (uiX < left || uiX > right || uiY < top || uiY > kChatLogBottomY)
    {
        return false;
    }

    // Whisper target follows the tap, and can always be cleared without extra
    // UI: tapping the same name again drops it, and so does tapping any part of
    // the log that is not a line with a sender. Without that there is no way
    // back to normal chat once a name has been picked up.
    std::string senderId;
    bool onSender = pLog->GetAndroidChatMessageIDAt(uiX, uiY, senderId) && !senderId.empty();

    // Your own chat lines carry your name as well, and whispering yourself does
    // nothing, so a tap there counts as a tap on empty log - which clears the
    // current target instead of replacing it with something unusable.
    if (onSender && Hero != nullptr && senderId == Hero->ID)
    {
        onSender = false;
    }

    if (g_pChatInputBox != nullptr)
    {
        // Same type as CNewUIChatInputBox::type_string, spelled out because that
        // typedef is not reachable from here.
        std::basic_string<unicode::t_char> currentWhisper;
        g_pChatInputBox->GetWhsprID(currentWhisper);
        const std::string current(currentWhisper.begin(), currentWhisper.end());

        if (!onSender || current == senderId)
        {
            g_pChatInputBox->SetWhsprID("");
        }
        else
        {
            g_pChatInputBox->SetWhsprID(senderId.c_str());
        }
    }

    if (!g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX))
    {
        g_pNewUISystem->Show(SEASON3B::INTERFACE_CHATINPUTBOX);
    }
    if (g_pChatInputBox != nullptr && g_pChatInputBox->m_pChatInputBox != nullptr)
    {
        g_pChatInputBox->m_pChatInputBox->GiveFocus(TRUE);
    }

    PlayBuffer(SOUND_CLICK01);
    return true;
}

// Auto-combo toggle, Knight line only. Paired with HitTestComboToggle.
void RenderComboToggle()
{
    if (!IsVirtualPadAvailable() || !IsAndroidComboClass())
    {
        return;
    }

    const AndroidUiRect rect = GetComboToggleRect();

    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DrawVirtualRightPanelButtonBox(rect, g_virtualComboEnabled);

    HFONT font = g_hFontMini != nullptr ? g_hFontMini : g_hFont;
    TextDraw(font,
             static_cast<int>(rect.x),
             static_cast<int>(rect.y + 6.0f),
             g_virtualComboEnabled ? 0xFF80FFB0 : 0xFFFFFFFF,
             0x0,
             static_cast<int>(rect.w),
             0, 3,
             "%s", g_virtualComboEnabled ? "CMB ON" : "CMB");

    // Which step of the chain comes next. Worth keeping in normal play - with
    // the chain advancing only on a successful cast, the player needs to see
    // where it actually is. Turns red while a step is being retried.
    if (g_virtualComboEnabled)
    {
        TextDraw(font,
                 static_cast<int>(rect.x),
                 static_cast<int>(rect.y + rect.h + 2.0f),
                 g_androidComboLastResult == kAndroidComboResultBlocked ? 0xFF8080FF : 0xFFD0D0FF,
                 0x0,
                 static_cast<int>(rect.w),
                 0, 3,
                 "%d/%d", g_virtualComboStep + 1, kVirtualComboSlotCount);
    }

    EndBitmap();
}

// The select-target button. Lit while a lock is held, and captioned with the
// locked target's name so the player can see what they are committed to without
// opening the list.
void RenderTargetSelectButton()
{
    if (!IsVirtualPadAvailable())
    {
        return;
    }

    const AndroidUiRect rect = GetTargetSelectButtonRect();
    const bool locked = IsAndroidTargetLockActive();

    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DrawVirtualCircle(
        kTargetSelectButtonCx,
        kTargetSelectButtonCy,
        kTargetSelectButtonRadius,
        locked ? 0.62f : 0.06f,
        locked ? 0.16f : 0.12f,
        locked ? 0.16f : 0.20f,
        0.86f,
        true);

    DrawVirtualRectOutline(rect.x, rect.y, rect.w, rect.h,
                           locked ? 0.98f : 0.42f,
                           locked ? 0.54f : 0.60f,
                           locked ? 0.30f : 0.86f,
                           0.94f, 2.0f);

    HFONT font = g_hFontMini != nullptr ? g_hFontMini : g_hFont;
    TextDraw(font,
             static_cast<int>(rect.x),
             static_cast<int>(rect.y + rect.h * 0.5f - 5.0f),
             locked ? 0xFFB0B0FF : 0xFFFFFFFF,
             0x0,
             static_cast<int>(rect.w),
             0, 3, "%s", locked ? "LOCK" : "TGT");

    if (locked)
    {
        TextDraw(font,
                 static_cast<int>(rect.x - 40.0f),
                 static_cast<int>(rect.y - 12.0f),
                 0xFFFFFFFF,
                 0x0,
                 static_cast<int>(rect.w + 40.0f),
                 0, 3, "%s", g_androidTargetLock.id);
    }

    EndBitmap();
}

void RenderAndroidTargetPicker()
{
    if (!g_androidTargetPicker.visible || !IsVirtualPadAvailable())
    {
        return;
    }

    RefreshAndroidTargetPickerEntries(false);
    const AndroidUiRect pickerRect = GetAndroidTargetPickerRect();

    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DrawVirtualRectFilled(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 0.0f, 0.0f, 0.24f);
    DrawVirtualRectFilled(pickerRect.x - 3.0f, pickerRect.y - 3.0f, pickerRect.w + 6.0f, pickerRect.h + 6.0f, 0.0f, 0.0f, 0.0f, 0.38f);
    DrawVirtualRectFilled(pickerRect.x, pickerRect.y, pickerRect.w, pickerRect.h, 0.10f, 0.04f, 0.05f, 0.78f);
    DrawVirtualRectFilled(pickerRect.x + 2.0f, pickerRect.y + 2.0f, pickerRect.w - 4.0f, pickerRect.h - 4.0f, 0.22f, 0.09f, 0.10f, 0.64f);
    DrawVirtualRectFilled(pickerRect.x + 3.0f, pickerRect.y + 3.0f, pickerRect.w - 6.0f, kTargetPickerHeaderH - 6.0f, 0.62f, 0.24f, 0.24f, 0.36f);
    DrawVirtualRectOutline(pickerRect.x, pickerRect.y, pickerRect.w, pickerRect.h, 0.86f, 0.34f, 0.34f, 0.94f, 2.0f);
    DrawVirtualRectOutline(pickerRect.x + 2.0f, pickerRect.y + 2.0f, pickerRect.w - 4.0f, pickerRect.h - 4.0f, 0.20f, 0.06f, 0.08f, 0.94f, 1.0f);

    for (int row = 0; row < GetAndroidTargetPickerRowCount(); ++row)
    {
        const int entryIndex = g_androidTargetPicker.scrollOffset + row;
        const bool isLocked = entryIndex < g_androidTargetPicker.entryCount
            && g_androidTargetLock.active
            && g_androidTargetPicker.entries[entryIndex].key == g_androidTargetLock.key;

        DrawVirtualRightPanelButtonBox(GetAndroidTargetPickerRowRect(row), isLocked);
    }

    HFONT rowFont = g_hFontBold != nullptr ? g_hFontBold : g_hFont;
    HFONT smallFont = g_hFontMini != nullptr ? g_hFontMini : g_hFont;

    if (g_androidTargetPicker.entryCount <= 0)
    {
        TextDraw(smallFont,
                 static_cast<int>(pickerRect.x + 7.0f),
                 static_cast<int>(pickerRect.y + kTargetPickerHeaderH + 8.0f),
                 0xFFC0C0FF, 0x0,
                 static_cast<int>(pickerRect.w - 14.0f), 0, 3,
                 "%s", "No enemies nearby");
    }
    else
    {
        for (int row = 0; row < GetAndroidTargetPickerRowCount(); ++row)
        {
            const int entryIndex = g_androidTargetPicker.scrollOffset + row;
            if (entryIndex >= g_androidTargetPicker.entryCount)
            {
                break;
            }

            const AndroidTargetPickerEntry& entry = g_androidTargetPicker.entries[entryIndex];
            const AndroidUiRect rowRect = GetAndroidTargetPickerRowRect(row);
            const bool isLocked = g_androidTargetLock.active && entry.key == g_androidTargetLock.key;

            TextDraw(rowFont,
                     static_cast<int>(rowRect.x + 7.0f),
                     static_cast<int>(rowRect.y + 8.0f),
                     isLocked ? 0xFF90FFB0 : 0xFFFFFFFF,
                     0x0,
                     static_cast<int>(rowRect.w - 40.0f), 0, 1,
                     "%s", entry.id);

            // Distance is squared in tiles; the sqrt is only for display.
            TextDraw(smallFont,
                     static_cast<int>(rowRect.x + rowRect.w - 34.0f),
                     static_cast<int>(rowRect.y + 9.0f),
                     0xFFC8C8FF, 0x0,
                     30, 0, 3,
                     "%dm", static_cast<int>(std::sqrt(static_cast<float>(entry.distance2))));
        }
    }

    const AndroidUiRect footerRect = GetAndroidTargetPickerFooterRect();
    TextDraw(smallFont,
             static_cast<int>(footerRect.x + 4.0f),
             static_cast<int>(footerRect.y + 6.0f),
             0xFFE0A0FF, 0x0,
             static_cast<int>(footerRect.w - 8.0f), 0, 3,
             "%s", "Close");

    TextDraw(rowFont,
             static_cast<int>(pickerRect.x + 7.0f),
             static_cast<int>(pickerRect.y + 7.0f),
             0xFFFFFFFF, 0x0,
             static_cast<int>(pickerRect.w - 14.0f), 0, 3,
             "%s", "Select Target");

    EndBitmap();
}

void RenderAndroidTradePicker()
{
    if (!g_androidTradePicker.visible || !IsVirtualPadAvailable())
    {
        return;
    }

    RefreshAndroidTradePickerEntries();
    const AndroidUiRect pickerRect = GetAndroidTradePickerRect();

    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    DrawVirtualRectFilled(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 0.0f, 0.0f, 0.24f);
    DrawVirtualRectFilled(pickerRect.x - 3.0f, pickerRect.y - 3.0f, pickerRect.w + 6.0f, pickerRect.h + 6.0f, 0.0f, 0.0f, 0.0f, 0.38f);
    DrawVirtualRectFilled(pickerRect.x, pickerRect.y, pickerRect.w, pickerRect.h, 0.04f, 0.08f, 0.14f, 0.76f);
    DrawVirtualRectFilled(pickerRect.x + 2.0f, pickerRect.y + 2.0f, pickerRect.w - 4.0f, pickerRect.h - 4.0f, 0.09f, 0.17f, 0.30f, 0.62f);
    DrawVirtualRectFilled(pickerRect.x + 3.0f, pickerRect.y + 3.0f, pickerRect.w - 6.0f, kAndroidTradePickerHeaderH - 6.0f, 0.24f, 0.38f, 0.68f, 0.34f);
    DrawVirtualRectOutline(pickerRect.x, pickerRect.y, pickerRect.w, pickerRect.h, 0.36f, 0.58f, 0.96f, 0.94f, 2.0f);
    DrawVirtualRectOutline(pickerRect.x + 2.0f, pickerRect.y + 2.0f, pickerRect.w - 4.0f, pickerRect.h - 4.0f, 0.05f, 0.12f, 0.24f, 0.94f, 1.0f);

    for (int row = 0; row < GetAndroidTradePickerRowCount(); ++row)
    {
        const int entryIndex = g_androidTradePicker.scrollOffset + row;
        const bool pending = entryIndex < g_androidTradePicker.entryCount
            && g_androidTradePicker.autoMoving
            && g_androidTradePicker.entries[entryIndex].key == g_androidTradePicker.pendingKey;
        const AndroidUiRect rowRect = GetAndroidTradePickerRowRect(row);
        DrawVirtualRightPanelButtonBox(rowRect, pending);
    }

    DrawVirtualRightPanelButtonBox(GetAndroidTradePickerFooterRect(), false);
    TextDraw(
        g_hFontBold != nullptr ? g_hFontBold : g_hFont,
        static_cast<int>(pickerRect.x),
        static_cast<int>(pickerRect.y + 8.0f),
        0xFFFFFFFF,
        0x0,
        static_cast<int>(pickerRect.w),
        0,
        3,
        GetAndroidPlayerCommandPickerTitle());

    if (g_androidTradePicker.entryCount <= 0)
    {
        const AndroidUiRect rowRect = GetAndroidTradePickerRowRect(0);
        TextDraw(
            g_hFontBold != nullptr ? g_hFontBold : g_hFont,
            static_cast<int>(rowRect.x),
            static_cast<int>(rowRect.y + 8.0f),
            0xBFC8D8FF,
            0x0,
            static_cast<int>(rowRect.w),
            0,
            3,
            "NO PLAYER",
            0);
    }
    else
    {
        for (int row = 0; row < GetAndroidTradePickerRowCount(); ++row)
        {
            const int entryIndex = g_androidTradePicker.scrollOffset + row;
            if (entryIndex >= g_androidTradePicker.entryCount)
            {
                break;
            }

            const AndroidTradePickerEntry& entry = g_androidTradePicker.entries[entryIndex];
            const AndroidUiRect rowRect = GetAndroidTradePickerRowRect(row);
            const bool pending = g_androidTradePicker.autoMoving && entry.key == g_androidTradePicker.pendingKey;
            TextDraw(
                g_hFontBold != nullptr ? g_hFontBold : g_hFont,
                static_cast<int>(rowRect.x + 7.0f),
                static_cast<int>(rowRect.y + 8.0f),
                pending ? 0xFFD07AFF : 0xFFFFFFFF,
                0x0,
                static_cast<int>(rowRect.w - 8.0f),
                0,
                1,
                "%s",
                entry.id);
        }
    }

    const AndroidUiRect footerRect = GetAndroidTradePickerFooterRect();
    if (g_androidTradePicker.entryCount > kAndroidTradePickerVisibleRows)
    {
        const int from = std::min(g_androidTradePicker.entryCount, g_androidTradePicker.scrollOffset + 1);
        const int to = std::min(g_androidTradePicker.entryCount, g_androidTradePicker.scrollOffset + GetAndroidTradePickerRowCount());
        TextDraw(
            g_hFontMini != nullptr ? g_hFontMini : g_hFont,
            static_cast<int>(footerRect.x + 4.0f),
            static_cast<int>(footerRect.y + 8.0f),
            0xFFE0A0FF,
            0x0,
            static_cast<int>(footerRect.w - 8.0f),
            0,
            1,
            "%d-%d/%d",
            from,
            to,
            g_androidTradePicker.entryCount);
    }

    TextDraw(
        g_hFontBold != nullptr ? g_hFontBold : g_hFont,
        static_cast<int>(footerRect.x),
        static_cast<int>(footerRect.y + 8.0f),
        0xFFFFFFFF,
        0x0,
        static_cast<int>(footerRect.w),
        0,
        3,
        g_androidTradePicker.autoMoving ? "HUY MOVE" : "HUY");
    EndBitmap();
}

// Lit state for a top bar slot. The menu entry tracks whether the grid it opens
// is showing; the rest track their own window.
bool IsTopBarSlotActive(int slot)
{
    if (slot < 0 || slot >= kTopBarButtonCount)
    {
        return false;
    }

    if (kTopBarActions[slot] == kTopBarActionMenu)
    {
        return g_virtualRightPanelUtilityMode;
    }

    // Lit while the helper is actually running, not while its window is open -
    // that is what the Helper slot next to it already reports.
    if (kTopBarActions[slot] == kTopBarActionHelperPlay)
    {
        return IsAndroidMuHelperRunning();
    }

    return IsVirtualRightPanelUtilityActionActive(kTopBarActions[slot]);
}

// Current opacity for one of the four hideable slots (1-4 => local index
// 0-3, left to right). 1 = fully shown, 0 = fully hidden, skip drawing.
// While g_topBarRowAnimActive, each slot fades in or out over
// kTopBarRowAnimFadeMs, staggered by kTopBarRowAnimStaggerMs in an order
// that depends on which way the toggle just went: showing sweeps from the
// slot nearest the toggle (closest to Menu) outward to the left, hiding
// sweeps from the leftmost slot back toward the toggle - see the caller's
// comment by g_topBarRowAnimActive for why.
float GetTopBarRowSlotAlpha(int localIndex)
{
    if (!g_topBarRowAnimActive)
    {
        return g_topBarRowIconsVisible ? 1.0f : 0.0f;
    }

    constexpr int kRowCount = kTopBarRowButtonCount - 1;
    const int order = g_topBarRowIconsVisible ? (kRowCount - 1 - localIndex) : localIndex;
    const float startDelay = static_cast<float>(order) * kTopBarRowAnimStaggerMs;
    const float elapsed = static_cast<float>(GetTickCount() - g_topBarRowAnimStartTick);
    const float localT = std::clamp((elapsed - startDelay) / kTopBarRowAnimFadeMs, 0.0f, 1.0f);
    return g_topBarRowIconsVisible ? localT : (1.0f - localT);
}

// Opacity for any top bar slot - 1 for Menu and the Helper/Play stack, which
// never hide, and GetTopBarRowSlotAlpha's animated value for the four
// hideable slots (1-4).
float GetTopBarSlotAlpha(int slot)
{
    if (slot < 1 || slot >= kTopBarRowButtonCount)
    {
        return 1.0f;
    }
    return GetTopBarRowSlotAlpha(slot - 1);
}

// Scales a 0xAARRGGBB color's alpha byte by a 0-1 factor, for fading
// TextDraw labels in step with the boxes/icons under them.
DWORD ScaleColorAlpha(DWORD color, float alpha)
{
    const DWORD baseA = (color >> 24) & 0xFF;
    const DWORD a = static_cast<DWORD>(static_cast<float>(baseA) * std::clamp(alpha, 0.0f, 1.0f));
    return (a << 24) | (color & 0x00FFFFFFu);
}

// Placeholder circle to the row's left, toggling g_topBarRowIconsVisible - a
// procedural GL shape (ring + fill + a chevron-ish glyph) rather than a PNG,
// same technique DrawVirtualChatUtilityButton already uses for its own
// circular button. Swap for real icon art later; nothing else needs to
// change when that happens, this function is the only place the look lives.
void DrawTopBarRowToggleButton()
{
    const AndroidUiRect rect = GetTopBarRowToggleButtonRect();
    const float uiCx = rect.x + rect.w * 0.5f;
    const float uiCy = rect.y + rect.h * 0.5f;
    const float uiRadius = rect.w * 0.5f;
    const float cx = UiToScreenX(uiCx);
    const float cy = static_cast<float>(WindowHeight) - UiToScreenY(uiCy);
    const float rx = UiToScreenX(uiRadius);
    const float ry = UiToScreenY(uiRadius);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(0.06f, 0.06f, 0.08f, 0.78f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 24; ++i)
    {
        const float angle = (static_cast<float>(i) / 24.0f) * 6.28318530718f;
        glVertex2f(cx + (std::cos(angle) * rx), cy + (std::sin(angle) * ry));
    }
    glEnd();

    glLineWidth(2.0f);
    glColor4f(0.85f, 0.85f, 0.90f, 0.90f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 24; ++i)
    {
        const float angle = (static_cast<float>(i) / 24.0f) * 6.28318530718f;
        glVertex2f(cx + (std::cos(angle) * rx), cy + (std::sin(angle) * ry));
    }
    glEnd();
    glLineWidth(1.0f);

    // Small glyph telling the two states apart: a down chevron (row shown,
    // tap collapses it) or a right chevron (row hidden, tap expands it).
    glColor4f(0.92f, 0.92f, 0.85f, 0.95f);
    glBegin(GL_TRIANGLES);
    if (g_topBarRowIconsVisible)
    {
        glVertex2f(cx - rx * 0.35f, cy + ry * 0.20f);
        glVertex2f(cx + rx * 0.35f, cy + ry * 0.20f);
        glVertex2f(cx, cy - ry * 0.30f);
    }
    else
    {
        glVertex2f(cx - rx * 0.20f, cy - ry * 0.35f);
        glVertex2f(cx - rx * 0.20f, cy + ry * 0.35f);
        glVertex2f(cx + rx * 0.30f, cy);
    }
    glEnd();
}

// Placeholder dropdown chevron on the location chip, hinting it is now
// tappable (opens the same move/map window the utility grid's MAP action
// does - see kTopBarActionLocation). Procedural triangle for the same reason
// as DrawTopBarRowToggleButton above; swap for real art later.
void DrawTopBarLocationChevron()
{
    const AndroidUiRect rect = GetTopBarLocationChipRect();
    const float uiCx = rect.x + rect.w - 10.0f;
    const float uiCy = rect.y + rect.h * 0.5f;
    const float cx = UiToScreenX(uiCx);
    const float cy = static_cast<float>(WindowHeight) - UiToScreenY(uiCy);
    const float halfW = UiToScreenX(4.0f) - UiToScreenX(0.0f);
    const float halfH = UiToScreenY(3.0f) - UiToScreenY(0.0f);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.85f, 0.88f, 0.95f, 0.85f);
    glBegin(GL_TRIANGLES);
    glVertex2f(cx - halfW, cy + halfH);
    glVertex2f(cx + halfW, cy + halfH);
    glVertex2f(cx, cy - halfH);
    glEnd();
}

// The always-visible labelled row plus the coin and location chips beneath it.
// Icons are optional: DrawIconButton skips a texture that failed to load, and
// the box and label are drawn either way, so the row works before any art
// exists. Once an icon is present it covers the label.
void RenderVirtualTopBar()
{
    if (!IsVirtualUtilityButtonsAvailable())
    {
        return;
    }

    if (g_topBarRowAnimActive)
    {
        constexpr float kTotalAnimMs =
            static_cast<float>(kTopBarRowButtonCount - 2) * kTopBarRowAnimStaggerMs + kTopBarRowAnimFadeMs;
        if (static_cast<float>(GetTickCount() - g_topBarRowAnimStartTick) >= kTotalAnimMs)
        {
            g_topBarRowAnimActive = false;
        }
    }

    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Plain semi-transparent black plate, drawn inline rather than through
    // DrawVirtualRightPanelButtonBox - that one is shared with the utility grid,
    // the target picker and the item menu, which all still want the decorated
    // blue style. Now that the icons carry the meaning, these only need to hold
    // the art clear of the world behind them. Active slots go more opaque, which
    // is the whole state cue left once the labels are gone.
    for (int slot = 0; slot < kTopBarButtonCount; ++slot)
    {
        const float slotAlpha = GetTopBarSlotAlpha(slot);
        if (slotAlpha <= 0.0f)
        {
            continue;
        }

        const AndroidUiRect rect = GetTopBarButtonRect(slot);
        const bool active = IsTopBarSlotActive(slot);
        DrawVirtualRectFilled(rect.x, rect.y, rect.w, rect.h,
                              0.0f, 0.0f, 0.0f, (active ? 0.78f : 0.45f) * slotAlpha);
        DrawVirtualRectOutline(rect.x, rect.y, rect.w, rect.h,
                               0.85f, 0.85f, 0.90f, (active ? 0.85f : 0.35f) * slotAlpha, 1.0f);
    }

    const AndroidUiRect locRect = GetTopBarLocationChipRect();
    DrawVirtualRectFilled(locRect.x, locRect.y, locRect.w, locRect.h, 0.05f, 0.05f, 0.08f, 0.62f);
    DrawVirtualRectOutline(locRect.x, locRect.y, locRect.w, locRect.h, 0.22f, 0.36f, 0.62f, 0.90f, 1.0f);

    HFONT chipFont = g_hFontMini != nullptr ? g_hFontMini : g_hFont;

    const char* mapName = gMapManager.GetMapName(gMapManager.WorldActive);
    if (mapName != nullptr && mapName[0] != '\0' && Hero != nullptr)
    {
        TextDraw(chipFont,
                 static_cast<int>(locRect.x + 4.0f),
                 static_cast<int>(locRect.y + 4.0f),
                 0xFFFFFFFF,
                 0x0,
                 static_cast<int>(locRect.w - 8.0f),
                 0, 3, "%s (%d, %d)", mapName, Hero->PositionX, Hero->PositionY);
    }

    // Icons last so they sit over the box and its label - but still inside the
    // BeginBitmap/EndBitmap pair. DrawIconButton emits raw glVertex2f in screen
    // pixels, which only lands correctly under the 2D ortho projection
    // BeginBitmap sets up; EndBitmap pops it, so drawing these afterwards put
    // them under the world projection where nothing was visible.
    for (int slot = 0; slot < kTopBarButtonCount; ++slot)
    {
        const float slotAlpha = GetTopBarSlotAlpha(slot);
        if (slotAlpha <= 0.0f)
        {
            continue;
        }

        const AndroidUiRect rect = GetTopBarButtonRect(slot);
        DrawIconButton(rect.x + 2.0f, rect.y + 2.0f, rect.w - 4.0f, rect.h - 4.0f,
                       GetTopBarIconTexture(slot), slotAlpha);
    }

    // Labels drawn by the engine rather than baked into the art, onto the empty
    // nameplate at the bottom of each icon. Baked text cannot stay sharp here:
    // UI space is 640x480 but the screen is not 4:3, so the two axes scale by
    // different factors (3.875x vs 2.325x on a 2480x1116 panel) and any art is
    // stretched sideways - fatal for letterforms, survivable for symbols. Engine
    // text is rasterised at native pixel size and skips that entirely, which is
    // why the map name and chat read cleanly in the same frame.
    //
    // Positioned off the bottom edge rather than rect.y like
    // RenderVirtualRightPanelButtonLabel does, since that one centres in the
    // whole button and is shared with the utility grid.
    //
    for (int slot = 0; slot < kTopBarButtonCount; ++slot)
    {
        const float slotAlpha = GetTopBarSlotAlpha(slot);
        if (slotAlpha <= 0.0f)
        {
            continue;
        }

        const AndroidUiRect rect = GetTopBarButtonRect(slot);
        TextDraw(g_hFontMini != nullptr ? g_hFontMini : g_hFont,
                 static_cast<int>(rect.x),
                 static_cast<int>(rect.y + rect.h - 11.0f),
                 ScaleColorAlpha(IsTopBarSlotActive(slot) ? 0xFFFFF0C0 : 0xFFF0E4CC, slotAlpha),
                 0x0,
                 static_cast<int>(rect.w),
                 0, 3,
                 "%s", kTopBarLabels[slot]);
    }

    // Both placeholders until real icon art exists for them - see
    // DrawTopBarRowToggleButton/DrawTopBarLocationChevron just below. Must
    // stay inside the BeginBitmap/EndBitmap pair, same reason DrawIconButton
    // above does: their raw glVertex2f calls only land correctly under the 2D
    // ortho projection BeginBitmap sets up.
    DrawTopBarRowToggleButton();
    DrawTopBarLocationChevron();

    EndBitmap();
}

void RenderVirtualRightPanelUtilityMode()
{
    const AndroidUiRect panelRect = GetVirtualRightPanelRect();

    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawVirtualRectFilled(panelRect.x - 2.0f, panelRect.y - 2.0f, panelRect.w + 4.0f, panelRect.h + 4.0f, 0.02f, 0.05f, 0.12f, 0.28f);
    DrawVirtualRectFilled(panelRect.x, panelRect.y, panelRect.w, panelRect.h, 0.14f, 0.28f, 0.62f, 0.42f);
    DrawVirtualRectFilled(panelRect.x + 2.0f, panelRect.y + 2.0f, panelRect.w - 4.0f, panelRect.h - 4.0f, 0.08f, 0.18f, 0.40f, 0.50f);
    DrawVirtualRectFilled(panelRect.x + 3.0f, panelRect.y + 3.0f, panelRect.w - 6.0f, 10.0f, 0.54f, 0.74f, 1.0f, 0.12f);
    DrawVirtualRectOutline(panelRect.x, panelRect.y, panelRect.w, panelRect.h, 0.10f, 0.22f, 0.56f, 0.96f, 2.0f);
    DrawVirtualRectOutline(panelRect.x + 2.0f, panelRect.y + 2.0f, panelRect.w - 4.0f, panelRect.h - 4.0f, 0.18f, 0.36f, 0.72f, 0.86f, 1.0f);

    for (int button = 0; button < kVirtualRightPanelUtilityActionCount; ++button)
    {
        const AndroidUiRect rect = GetVirtualRightPanelGridRect(button);
        DrawVirtualRightPanelButtonBox(rect, IsVirtualRightPanelUtilityActionActive(button));
    }

    for (int button = 0; button < kVirtualRightPanelUtilityActionCount; ++button)
    {
        RenderVirtualRightPanelButtonLabel(
            GetVirtualRightPanelGridRect(button),
            kVirtualRightPanelUtilityLabels[button],
            IsVirtualRightPanelUtilityActionActive(button));
    }
    EndBitmap();
}

void RenderVirtualRightPanelModeButton()
{
    const AndroidUiRect rect = GetVirtualRightPanelCombatModeButtonRect();

    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawVirtualRightPanelButtonBox(rect, true);
    EndBitmap();
    RenderVirtualRightPanelModeButtonLabel(rect);
}

// Helper: draw a solid filled axis-aligned rectangle in screen space (GL y-up).
static void DrawFilledRect(float x, float y, float w, float h)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x,     y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x,     y + h);
    glEnd();
}

static void EmitVirtualUiVertex(float uiX, float uiY)
{
    glVertex2f(UiToScreenX(uiX), static_cast<float>(WindowHeight) - UiToScreenY(uiY));
}

static void DrawVirtualUtilityLabel(int button, float uiCx, float uiCy, float uiRadius)
{
    const float w = uiRadius * 0.95f;
    const float h = uiRadius * 1.15f;
    glColor4f(1.0f, 1.0f, 1.0f, 0.96f);

    switch (button)
    {
    case 0: // I = Inventory
        glBegin(GL_LINES);
        EmitVirtualUiVertex(uiCx, uiCy - h * 0.45f);
        EmitVirtualUiVertex(uiCx, uiCy + h * 0.45f);
        glEnd();
        break;
    case 1: // C = Character
        glBegin(GL_LINE_STRIP);
        EmitVirtualUiVertex(uiCx + w * 0.40f, uiCy + h * 0.42f);
        EmitVirtualUiVertex(uiCx - w * 0.35f, uiCy + h * 0.42f);
        EmitVirtualUiVertex(uiCx - w * 0.45f, uiCy);
        EmitVirtualUiVertex(uiCx - w * 0.35f, uiCy - h * 0.42f);
        EmitVirtualUiVertex(uiCx + w * 0.40f, uiCy - h * 0.42f);
        glEnd();
        break;
    case 2: // S = Settings
        glBegin(GL_LINE_STRIP);
        EmitVirtualUiVertex(uiCx + w * 0.40f, uiCy + h * 0.42f);
        EmitVirtualUiVertex(uiCx - w * 0.20f, uiCy + h * 0.42f);
        EmitVirtualUiVertex(uiCx - w * 0.40f, uiCy + h * 0.15f);
        EmitVirtualUiVertex(uiCx + w * 0.20f, uiCy);
        EmitVirtualUiVertex(uiCx + w * 0.40f, uiCy - h * 0.20f);
        EmitVirtualUiVertex(uiCx + w * 0.15f, uiCy - h * 0.42f);
        EmitVirtualUiVertex(uiCx - w * 0.40f, uiCy - h * 0.42f);
        glEnd();
        break;
    default:
        break;
    }
}

// Render state for extra-sharp skill icons:
// keep alpha-test cutout but disable blending to avoid soft/washed edges.
static void ConfigureVirtualSkillIconNoBlendState()
{
    EnableAlphaTest();
    DisableCullFace();
    // DrawIconButton() temporarily restores additive blending for the rest of the
    // HUD but does not update the legacy state tracker. Force the real GL blend
    // mode back to standard alpha before any skill/icon atlas render.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

// Draw one 7-segment digit at screen coordinates (sx,sy) with size (sw,sh).
// In OpenGL y-up: sy = bottom of digit box, sy+sh = top.
// Each segment is rendered as a filled rectangle 鑺掗埀顑解偓?always crisp on any DPI.
void DrawVirtualDigit(float sx, float sy, float sw, float sh, int digit)
{
    if (digit < 0 || digit > 9)
        return;
    // Bitmask: bit0=top, bit1=topRight, bit2=botRight, bit3=bottom, bit4=botLeft, bit5=topLeft, bit6=middle
    static constexpr uint8_t kSegMap[10] = {
        0b0111111u, // 0
        0b0000110u, // 1
        0b1011011u, // 2
        0b1001111u, // 3
        0b1100110u, // 4
        0b1101101u, // 5
        0b1111101u, // 6
        0b0000111u, // 7
        0b1111111u, // 8
        0b1101111u, // 9
    };
    const uint8_t s = kSegMap[digit];

    // Segment thickness and gap (in screen pixels)
    const float t  = sw * 0.20f;          // segment bar thickness
    const float g  = t  * 0.30f;          // gap at corners

    // Key coordinates
    const float x0 = sx;                   // left edge of left vertical column
    const float x1 = sx + sw - t;          // left edge of right vertical column
    const float y0 = sy;                   // bottom horizontal
    const float yM = sy + sh * 0.5f;      // middle
    const float y1 = sy + sh - t;         // bottom edge of top horizontal

    // Width/height of each sub-segment
    const float hW = sw - 2.0f * t - 2.0f * g;  // horizontal bar inner width
    const float vH = sh * 0.5f - t - 2.0f * g;   // vertical bar half-height

    // Horizontal bars
    if (s & 0x01u) DrawFilledRect(x0 + t + g,  y1,            hW, t);  // top
    if (s & 0x40u) DrawFilledRect(x0 + t + g,  yM - t * 0.5f, hW, t);  // middle
    if (s & 0x08u) DrawFilledRect(x0 + t + g,  y0,            hW, t);  // bottom

    // Vertical bars 鑺掗埀顑解偓?top half
    if (s & 0x20u) DrawFilledRect(x0, yM + g,       t, vH);  // top-left
    if (s & 0x02u) DrawFilledRect(x1, yM + g,       t, vH);  // top-right

    // Vertical bars 鑺掗埀顑解偓?bottom half
    if (s & 0x10u) DrawFilledRect(x0, y0 + t + g,  t, vH);  // bot-left
    if (s & 0x04u) DrawFilledRect(x1, y0 + t + g,  t, vH);  // bot-right
}

// Render an integer number centered at (uiCenterX, uiTopY) in UI coordinates.
// Uses filled-rect 7-segment digits so they are always crisp regardless of DPI.
// Draws a dark shadow first, then white on top for maximum contrast over any bar color.
// digitW/digitH control the size of each digit in virtual UI units.
void DrawVirtualNumber(float uiCenterX, float uiTopY, int number,
                       float digitW = 4.0f, float digitH = 5.0f)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", number);
    const int numDigits = static_cast<int>(strlen(buf));
    if (numDigits <= 0)
        return;

    const float kGap = digitW * 0.25f;  // gap scales with digit size

    const float totalW_ui = numDigits * (digitW + kGap) - kGap;
    const float startX0_ui = uiCenterX - totalW_ui * 0.5f;

    // Pre-compute screen sizes (same for every digit)
    const float sw = UiToScreenX(digitW);
    const float sh = UiToScreenY(digitH);

    // Pass 1 鑺掗埀顑解偓?dark shadow, offset +1px right and -1px up (screen space)
    glColor4f(0.0f, 0.0f, 0.0f, 0.85f);
    {
        float startX_ui = startX0_ui;
        for (int i = 0; i < numDigits; ++i)
        {
            const int d = buf[i] - '0';
            const float sx = UiToScreenX(startX_ui) + 1.0f;
            const float sy = static_cast<float>(WindowHeight) - UiToScreenY(uiTopY + digitH) - 1.0f;
            DrawVirtualDigit(sx, sy, sw, sh, d);
            startX_ui += digitW + kGap;
        }
    }

    // Pass 2 鑺掗埀顑解偓?bright white foreground
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    {
        float startX_ui = startX0_ui;
        for (int i = 0; i < numDigits; ++i)
        {
            const int d = buf[i] - '0';
            const float sx = UiToScreenX(startX_ui);
            const float sy = static_cast<float>(WindowHeight) - UiToScreenY(uiTopY + digitH);
            DrawVirtualDigit(sx, sy, sw, sh, d);
            startX_ui += digitW + kGap;
        }
    }
}

ITEM* GetVirtualMirrorHotKeyItem(int slot)
{
    if (slot < 0
        || slot >= kVirtualMirrorHotKeySlotCount
        || g_pMainFrame == nullptr
        || g_pMyInventory == nullptr)
    {
        return nullptr;
    }

    if (g_androidHotKeySlotCleared[slot])
    {
        return nullptr;
    }

    const int itemIndex = g_pMainFrame->GetItemHotKeyInventoryIndex(kVirtualMirrorHotKeys[slot]);
    return itemIndex >= 0 ? g_pMyInventory->FindItem(itemIndex) : nullptr;
}

void RenderVirtualMirrorHotKeySlots()
{
    // Window hiding is centralised in RenderVirtualPad; see the note on
    // HitTestVirtualMirrorHotKeySlot.
    if (g_pMainFrame == nullptr)
    {
        return;
    }


    // Circular frames matching the attack button and the skill arc, rather than
    // the legacy square IMAGE_SKILLBOX art. Pure GL, so no asset is involved.
    BeginBitmap();
    DisableTexture();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int slot = 0; slot < kVirtualMirrorHotKeySlotCount; ++slot)
    {
        const VirtualButtonLayout& layout = kVirtualMirrorHotKeySlots[slot];
        DrawVirtualCombatButtonFrame(layout.cx, layout.cy, layout.radius, false, false);
    }
    EndBitmap();

    bool anyItem = false;
    for (int slot = 0; slot < kVirtualMirrorHotKeySlotCount; ++slot)
    {
        if (GetVirtualMirrorHotKeyItem(slot) != nullptr)
        {
            anyItem = true;
            break;
        }
    }

    if (anyItem)
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glViewport2(0, 0, WindowWidth, WindowHeight);
        gluPerspective2(
            1.0f,
            static_cast<float>(WindowWidth) / static_cast<float>(WindowHeight),
            RENDER_ITEMVIEW_NEAR,
            RENDER_ITEMVIEW_FAR);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        GetOpenGLMatrix(CameraMatrix);
        EnableDepthTest();
        EnableDepthMask();
        glDisable(GL_BLEND);
        glClear(GL_DEPTH_BUFFER_BIT);

        for (int slot = 0; slot < kVirtualMirrorHotKeySlotCount; ++slot)
        {
            ITEM* item = GetVirtualMirrorHotKeyItem(slot);
            if (item == nullptr)
            {
                continue;
            }

            const AndroidUiRect rect = GetVirtualMirrorHotKeyRect(slot);
            const float iconW = (std::max)(12.0f, rect.w - 6.0f);
            const float iconH = (std::max)(12.0f, rect.h - 10.0f);
            RenderItem3D(
                rect.x + (rect.w - iconW) * 0.5f,
                rect.y + 6.0f,
                iconW,
                iconH,
                item->Type,
                item->Level,
                0,
                0);
        }

        UpdateMousePositionn();

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }

    BeginBitmap();
    EnableAlphaTest();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    for (int slot = 0; slot < kVirtualMirrorHotKeySlotCount; ++slot)
    {
        const AndroidUiRect rect = GetVirtualMirrorHotKeyRect(slot);
        TextDraw(
            g_hFontBold != nullptr ? g_hFontBold : g_hFont,
            static_cast<int>(rect.x),
            static_cast<int>(rect.y - 10.0f),
            0xFFFFFFFF,
            0x0,
            static_cast<int>(rect.w),
            0,
            3,
            "%s",
            kVirtualMirrorHotKeyLabels[slot]);

        // Empty slot: a '+' invites binding something, and doubles as the
        // affordance for the picker - tapping it opens the inventory armed to
        // assign whatever consumable is tapped next to this slot.
        if (GetVirtualMirrorHotKeyItem(slot) == nullptr)
        {
            TextDraw(
                g_hFontBold != nullptr ? g_hFontBold : g_hFont,
                static_cast<int>(rect.x),
                static_cast<int>(rect.y + rect.h * 0.5f - 6.0f),
                (g_androidPendingHotKeyBindSlot == slot) ? 0xFF80FFB0 : 0xFFB0C4DE,
                0x0,
                static_cast<int>(rect.w),
                0,
                3,
                "%s",
                "+");
            continue;
        }

        const int itemCount = g_pMainFrame->GetItemHotKeyInventoryIndex(kVirtualMirrorHotKeys[slot], true);
        if (itemCount > 0)
        {
            SEASON3B::RenderNumber(rect.x + rect.w - 8.0f, rect.y + rect.h - 9.0f, itemCount);
        }
    }
    EndBitmap();
}

// Fraction of the way through the current level, 0..1.
//
// Experience and NextExperince are both totals accumulated since level 1, so
// the naive Experience/NextExperince sits near 1.0 for most of the game and is
// useless as a bar. The level's own span is the difference between the two
// thresholds, so the previous level's total has to come off both. That is the
// same arithmetic CGFxMainUi does for the desktop gauge, including the master
// level case, which runs on a separate curve offset by 400 levels.
float GetAndroidExperienceRatio()
{
    if (CharacterAttribute == nullptr)
    {
        return 0.0f;
    }

    const bool masterLevel = gCharacterManager.IsMasterLevel(CharacterAttribute->Class);

    double current = 0.0;
    double next = 0.0;
    double prior = 0.0;

    if (masterLevel)
    {
        current = static_cast<double>(Master_Level_Data.lMasterLevel_Experince);
        next = static_cast<double>(Master_Level_Data.lNext_MasterLevel_Experince);

        // Master levels continue the normal curve from 400, and the threshold
        // for the level below is the base this level is measured from.
        const __int64 total = static_cast<__int64>(Master_Level_Data.nMLevel) + 400;
        const __int64 priorLevel = total - 1;
        if (priorLevel > 0)
        {
            __int64 base = ((9 + priorLevel) * priorLevel * priorLevel * 10);
            if (priorLevel > 255)
            {
                const __int64 over = priorLevel - 255;
                base += (9 + over) * over * over * 1000;
            }
            prior = static_cast<double>((base - static_cast<__int64>(3892250000)) / 2);
        }
    }
    else
    {
        current = static_cast<double>(CharacterAttribute->Experience);
        next = static_cast<double>(CharacterAttribute->NextExperince);

        const __int64 priorLevel = static_cast<__int64>(CharacterAttribute->Level) - 1;
        if (priorLevel > 0)
        {
            __int64 base = (9 + priorLevel) * priorLevel * priorLevel * 10;
            if (priorLevel > 255)
            {
                const __int64 over = priorLevel - 255;
                base += (9 + over) * over * over * 1000;
            }
            prior = static_cast<double>(base);
        }
    }

    const double span = next - prior;
    if (span <= 0.0)
    {
        return 0.0;
    }

    const double gained = current - prior;
    return static_cast<float>(std::clamp(gained / span, 0.0, 1.0));
}

// Top-left status panel. Callers must already have passed IsVirtualPadAvailable,
// which is what guarantees CharacterAttribute is non-null here.
void RenderVirtualPortraitHud()
{
    // These are the same smoothed values the rest of the UI reads. The raw
    // CharacterAttribute->Life/LifeMax pair disagrees with them during regen
    // ticks, which would make the bar and the inventory show different numbers.
    const PRINT_PLAYER_S6& view = CharacterAttribute->PrintPlayer;

    const int maxHP = std::max(1, static_cast<int>(view.ViewMaxHP));
    const int curHP = std::clamp(static_cast<int>(view.ViewCurHP), 0, maxHP);
    const int maxMP = std::max(1, static_cast<int>(view.ViewMaxMP));
    const int curMP = std::clamp(static_cast<int>(view.ViewCurMP), 0, maxMP);
    const int maxSD = std::max(1, static_cast<int>(view.ViewMaxSD));
    const int curSD = std::clamp(static_cast<int>(view.ViewCurSD), 0, maxSD);

    // AG is carried in the BP fields, the same pair the legacy AG gauge read.
    const int maxAG = std::max(1, static_cast<int>(view.ViewMaxBP));
    const int curAG = std::clamp(static_cast<int>(view.ViewCurBP), 0, maxAG);

    const float yHP = kPortraitBarTop;
    const float yMP = yHP + kPortraitBarH + kPortraitBarGap;
    const float ySD = yMP + kPortraitBarH + kPortraitBarGap;
    const float yAG = ySD + kPortraitBarH + kPortraitBarGap;

    BeginBitmap();
    // EnableAlphaTest, not EnableAlphaBlend: the latter sets glBlendFunc(GL_ONE,
    // GL_ONE), so these bars were drawn additively and could never be opaque -
    // whatever terrain sat behind them was added straight through, however
    // solid the colour. Standard alpha makes them read as real bars.
    EnableAlphaTest();
    DisableTexture();

    // Panel backing, so the bars stay readable over bright terrain.
    DrawVirtualRectFilled(
        kPortraitPanelLeft,
        kPortraitPanelTop,
        kPortraitPanelW,
        kPortraitPanelH,
        0.03f, 0.03f, 0.05f, 0.92f);

    DrawVirtualBarH(kPortraitBarLeft, yHP, kPortraitBarW, kPortraitBarH,
                    static_cast<float>(curHP) / static_cast<float>(maxHP),
                    0.85f, 0.16f, 0.16f, 0.16f, 0.05f, 0.05f);

    DrawVirtualBarH(kPortraitBarLeft, yMP, kPortraitBarW, kPortraitBarH,
                    static_cast<float>(curMP) / static_cast<float>(maxMP),
                    0.20f, 0.45f, 0.90f, 0.05f, 0.07f, 0.16f);

    DrawVirtualBarH(kPortraitBarLeft, ySD, kPortraitBarW, kPortraitBarH,
                    static_cast<float>(curSD) / static_cast<float>(maxSD),
                    0.30f, 0.75f, 0.90f, 0.05f, 0.13f, 0.16f);

    // AG. The legacy frame had a gauge for this and removing the frame took it
    // with it - which matters because skills check it and refuse silently when
    // it runs dry, leaving no way to tell why a cast did nothing.
    DrawVirtualBarH(kPortraitBarLeft, yAG, kPortraitBarW, kPortraitBarH,
                    static_cast<float>(curAG) / static_cast<float>(maxAG),
                    0.90f, 0.78f, 0.30f, 0.16f, 0.13f, 0.04f);

    // Experience, in the strip under the panel. Light green, so it does not
    // read as another resource gauge alongside the four above it.
    DrawVirtualBarH(kExpBarX, kStatRowY, kExpBarW, kStatRowH,
                    GetAndroidExperienceRatio(),
                    0.55f, 0.92f, 0.45f, 0.10f, 0.20f, 0.10f);

    // No pet bar here: the game already draws a proper Fenrir/helper gauge in
    // CNewUIItemEnduranceInfo, complete with the pet's name. It is repositioned
    // there rather than duplicated here.

    // Per-class portrait, falling back to character.png for any class without
    // art yet. This has to stay inside the BeginBitmap/EndBitmap pair: like the
    // top bar icons, DrawIconButton emits raw glVertex2f in screen pixels and
    // only lands under the 2D ortho projection. There used to be an EndBitmap()
    // right here, which is why the avatar never appeared at all.
    //
    // Fills the whole slot without distorting the face. Rather than shrink the
    // portrait to fit, which left bare backing around it, draw it across the
    // full slot and sample a sub-rect of the texture instead: the slot is
    // covered edge to edge, the proportions stay true, and the overflow is
    // cropped rather than squashed.
    //
    // The comparison is between the slot's shape *on screen* and the art's own
    // shape. UI space is a fixed 640x480 stretched over a panel that is rarely
    // 4:3, so a slot that looks square in UI units is not square in pixels -
    // here the axes scale by 3.875x and 2.325x, and stretching a face by 67% is
    // immediately obvious. Both terms are measured rather than assumed: the
    // slot is no longer square in UI units either, and while the portraits are
    // all 92x92 today, an oblong one would otherwise be silently distorted.
    {
        const UITexture& portrait = GetClassPortraitTexture();

        const float boxScreenW = UiToScreenX(kPortraitAvatarX + kPortraitAvatarW) - UiToScreenX(kPortraitAvatarX);
        const float boxScreenH = UiToScreenY(kPortraitAvatarY + kPortraitAvatarH) - UiToScreenY(kPortraitAvatarY);
        const float texAspect  = (portrait.h > 0)
            ? (static_cast<float>(portrait.w) / static_cast<float>(portrait.h))
            : 1.0f;

        const float boxAspect = (boxScreenH > 0.0f && texAspect > 0.0f)
            ? ((boxScreenW / boxScreenH) / texAspect)
            : 1.0f;

        float u0 = 0.0f, uW = 1.0f;
        float v0 = 0.0f, vH = 1.0f;
        if (boxAspect > 1.0f)
        {
            // Slot is wider than the source: full width, crop height.
            //
            // v runs bottom-up here - the loader calls
            // stbi_set_flip_vertically_on_load - so v = 1 is the top of the
            // art. Anchoring the sampled window there takes the whole crop off
            // the bottom. Splitting it evenly, which is what (1 - vH) * 0.5
            // did, shaved the hair as well as the chest, and on a 16:9 panel
            // that is a fifth of the image gone at each end. The art carries a
            // few pixels of headroom above the hair, so a hard top anchor keeps
            // the head intact; the chest is the part that can go without the
            // portrait reading as cut off.
            vH = 1.0f / boxAspect;
            v0 = (1.0f - vH) * kPortraitCropFromBottom;
        }
        else if (boxAspect < 1.0f)
        {
            uW = boxAspect;
            u0 = (1.0f - uW) * 0.5f;
        }

        DrawIconButtonUv(
            kPortraitAvatarX,
            kPortraitAvatarY,
            kPortraitAvatarW,
            kPortraitAvatarH,
            portrait,
            u0, v0, uW, vH,
            1.0f);
    }

    if (Hero != nullptr && Hero->m_pPet != nullptr)
    {
        DrawIconButton(
            kPortraitPetX,
            kPortraitPetY,
            kPortraitPetSize,
            kPortraitPetSize,
            g_uiTex_character,
            0.85f,
            0.06f, 0.06f, 0.09f);
    }

    EndBitmap();

    // Numbers last, and inside their own bitmap pass: DrawIconButton above left
    // additive blending set and the font atlas bound.
    BeginBitmap();
    EnableAlphaBlend();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    HFONT barFont = g_hFontMini != nullptr ? g_hFontMini : g_hFont;
    const int textX = static_cast<int>(kPortraitBarLeft);
    const int textW = static_cast<int>(kPortraitBarW);

    // Align 3 centres the string in textW, the same call shape the trade picker
    // and the utility labels use.
    TextDraw(barFont, textX, static_cast<int>(yHP + 1.0f), 0xFFFFFFFF, 0x0, textW, 0, 3,
             "%d/%d", curHP, maxHP);
    TextDraw(barFont, textX, static_cast<int>(yMP + 1.0f), 0xFFFFFFFF, 0x0, textW, 0, 3,
             "%d/%d", curMP, maxMP);
    TextDraw(barFont, textX, static_cast<int>(ySD + 1.0f), 0xFFFFFFFF, 0x0, textW, 0, 3,
             "%d/%d", curSD, maxSD);
    TextDraw(barFont, textX, static_cast<int>(yAG + 1.0f), 0xFFFFFFFF, 0x0, textW, 0, 3,
             "%d/%d", curAG, maxAG);

    // Percentage over the experience bar, and the level pair beside it. ML is
    // only meaningful once the class has a master level, so it is left off
    // entirely below 400 rather than shown as a permanent "ML:0".
    const int expPercent =
        static_cast<int>(GetAndroidExperienceRatio() * 100.0f + 0.5f);
    TextDraw(barFont, static_cast<int>(kExpBarX), static_cast<int>(kStatRowY + 1.0f),
             0xFFFFFFFF, 0x0, static_cast<int>(kExpBarW), 0, 3,
             "EXP %d%%", expPercent);

    TextDraw(barFont, static_cast<int>(kStatLevelX), static_cast<int>(kStatRowY + 1.0f),
             0xFFF0E4CC, 0x0, static_cast<int>(kStatLevelW), 0, 3,
             "LVL:%d | ML:%d",
             CharacterAttribute->Level,
             gCharacterManager.IsMasterLevel(CharacterAttribute->Class)
                 ? Master_Level_Data.nMLevel
                 : 0);

    // Skill/cast diagnostic. Off in normal play; flip kShowAndroidSkillDebug to
    // bring it back when something in the cast path needs tracing again, since
    // logcat does not reach these devices.
    if (kShowAndroidSkillDebug)
    {
        const int slot = g_virtualSelectedSkillSlot;
        const int skillIndex = GetVirtualOverlayHotKeySkillIndex(slot);
        const int skillType = (CharacterAttribute != nullptr && IsValidSkillIndex(skillIndex))
            ? CharacterAttribute->Skill[skillIndex]
            : -1;
        const bool ground = IsGroundTargetedSkillIndex(skillIndex);

        const char* castText = "";
        switch (g_androidGroundCastResult)
        {
        // "sent" not "ok"/"refused": ExecuteSkill's return value reflects a
        // server reply that cannot have arrived yet, so it says nothing about
        // whether the request went out.
        case kGroundCastOk:       castText = "sent";     break;
        case kGroundCastRefused:  castText = "sent";     break;
        case kGroundCastNoTile:   castText = "notile";   break;
        case kGroundCastOffMap:   castText = "offmap";   break;
        case kGroundCastBadSkill: castText = "badskill"; break;
        default:                  castText = "-";        break;
        }

        const char* reasonText = "";
        switch (g_androidGroundCastReason)
        {
        case kGroundReasonSafeZone:   reasonText = "SAFEZONE"; break;
        case kGroundReasonDemand:     reasonText = "DEMAND";   break;
        case kGroundReasonUseCond:    reasonText = "USECOND";  break;
        case kGroundReasonMana:       reasonText = "MANA";     break;
        case kGroundReasonNoSkillIdx: reasonText = "NOIDX";    break;
        default:                      reasonText = "gate-ok";  break;
        }

        TextDraw(barFont,
                 static_cast<int>(kPortraitPanelX),
                 static_cast<int>(yAG + kPortraitBarH + 3.0f),
                 ground ? 0xFF80FFFF : 0xFFC8C8C8,
                 0x0,
                 static_cast<int>(kPortraitBarRight - kPortraitPanelX),
                 0, 1,
                 "SENT=%d dly%d chr%d/%d from(%d,%d) now(%d,%d)",
                 g_androidGroundCastLatchAfter,
                 g_androidGroundCastDelay,
                 g_androidGroundCastReqChar,
                 g_androidGroundCastHaveChar,
                 g_androidGroundCastFromX,
                 g_androidGroundCastFromY,
                 Hero != nullptr ? Hero->PositionX : -1,
                 Hero != nullptr ? Hero->PositionY : -1);
    }

    EndBitmap();
}

void RenderVirtualPad()
{
    // Keep only the virtual joystick overlay on mobile.
    // The rest of the custom Android HUD is intentionally disabled so the
    // original MU UI can render and handle input again.
    if (!IsVirtualPadAvailable())
    {
        static uint32_t s_lastUnavailableLog = 0;
        const uint32_t nowMs = MU_MobileGetTicks();
        if (SceneFlag == MAIN_SCENE && (nowMs - s_lastUnavailableLog) > 3000)
        {
            s_lastUnavailableLog = nowMs;
            LOGI(
                "VirtualPad unavailable scene=%d hero=%d attr=%d mainFrame=%d focusedInput=%d",
                static_cast<int>(SceneFlag),
                Hero != nullptr ? 1 : 0,
                CharacterAttribute != nullptr ? 1 : 0,
                g_pMainFrame != nullptr ? 1 : 0,
                AndroidHasFocusedTextInput() ? 1 : 0);
        }
        return;
    }

    // Cancel a pending Q/W/E/R bind once the bag closes again. Tracked as a
    // transition rather than "is it shut now", because this runs on the frame
    // the '+' was tapped too - before Show() has taken effect - and a plain
    // check would disarm it immediately. Left armed, it would hijack whatever
    // consumable the player tapped next, however much later.
    {
        static bool s_inventoryWasVisible = false;
        const bool inventoryVisible = (g_pNewUISystem != nullptr)
            && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INVENTORY);

        if (s_inventoryWasVisible && !inventoryVisible)
        {
            g_androidPendingHotKeyBindSlot = -1;
        }
        s_inventoryWasVisible = inventoryVisible;
    }

    // A bag, character sheet or any other MU window owns the screen: draw none
    // of the touch controls over it. The two modal pickers still render, since
    // they are windows in their own right rather than overlay controls.
    if (IsAndroidGameWindowOpen())
    {
        RenderAndroidTradePicker();
        RenderAndroidTargetPicker();
        return;
    }

    BeginBitmap();
    EnableAlphaBlend();
    DisableTexture();
    const bool joystickActive = g_virtualJoystick.fingerId != static_cast<SDL_FingerID>(-1);
    EnsureUITextures();

    // The stick is always on screen at its one home, half visible when idle and
    // full while held, so there is something to aim at before the thumb lands.
    // It used to be invisible until touched and then recentred on the finger,
    // which left nothing to push against and no way to see which way you were
    // pushing. Sizes come from the geometry helper, which works in device pixels
    // - specified in UI units the ring draws as a wide ellipse.
    {
        const VirtualJoystickGeometry geometry = GetVirtualJoystickGeometry();
        const float joystickCenterX = std::round(geometry.centerX);
        const float joystickCenterY = std::round(geometry.centerY);
        const float joystickThumbX = std::round(joystickCenterX + g_virtualJoystick.thumbOffsetX);
        const float joystickThumbY = std::round(joystickCenterY + g_virtualJoystick.thumbOffsetY);
        const float joystickAlpha = joystickActive ? 1.0f : 0.5f;

        DrawIconButtonUv(
            joystickCenterX - geometry.ringDiameterUiX * 0.5f,
            joystickCenterY - geometry.ringDiameterUiY * 0.5f,
            geometry.ringDiameterUiX,
            geometry.ringDiameterUiY,
            g_uiTex_joystick2,
            kJoystickRingU,
            kJoystickRingV,
            kJoystickRingUW,
            kJoystickRingVH,
            joystickAlpha);

        DrawIconButtonUv(
            joystickThumbX - geometry.knobDiameterUiX * 0.5f,
            joystickThumbY - geometry.knobDiameterUiY * 0.5f,
            geometry.knobDiameterUiX,
            geometry.knobDiameterUiY,
            g_uiTex_joystick1,
            kJoystickKnobU,
            kJoystickKnobV,
            kJoystickKnobUW,
            kJoystickKnobVH,
            joystickAlpha);
    }
    EndBitmap();

    RenderVirtualPortraitHud();
    RenderVirtualTopBar();
    RenderAndroidChatTabs();

    RenderVirtualTopRightControls();

    if (kUseLegacyMainHud && !kEnableVirtualCombatOverlay)
    {
        return;
    }

    if (kShowVirtualAttackButton && !g_virtualRightPanelUtilityMode)
    {
        BeginBitmap();
        EnsureUITextures();
        const VirtualButtonLayout& attackButton = kVirtualButtons[kVirtualAttackButton];
        const bool pressed = IsVirtualButtonPressed(kVirtualAttackButton);
        DrawVirtualCombatButtonFrame(
            attackButton.cx,
            attackButton.cy,
            attackButton.radius,
            pressed,
            false);

        // Full diameter, so the art covers the button instead of floating in
        // the middle of it - at 1.18x the radius it filled barely half the
        // circle and left a thick empty ring.
        const float attackIconSize = attackButton.radius * 2.0f;
        DrawIconButton(
            attackButton.cx - attackIconSize * 0.5f,
            attackButton.cy - attackIconSize * 0.5f,
            attackIconSize,
            attackIconSize,
            g_uiTex_attack,
            pressed ? 1.0f : 0.94f);
        EndBitmap();
    }

    if (kShowVirtualSkillButtons && !g_virtualRightPanelUtilityMode)
    {
        BeginBitmap();
        EnsureUITextures();
        const float skillIconScale = std::min(
            kVirtualSkillFrameW / kVirtualSkillBaseFrameW,
            kVirtualSkillFrameH / kVirtualSkillBaseFrameH);
        const float renderSkillIconW = kVirtualSkillSourceIconW * skillIconScale;
        const float renderSkillIconH = kVirtualSkillSourceIconH * skillIconScale;
        const bool assignModeActive = IsVirtualAssignModeActive()
            || IsVirtualOverlayHotKeySkillIndex(g_virtualAssignPickerSkillIndex);
        for (int visualSlot = 0; visualSlot < kVirtualVisibleSkillButtonCount; ++visualSlot)
        {
            const int buttonIndex = kVirtualSkillButtonBase + visualSlot;
            const VirtualButtonLayout& button = kVirtualButtons[buttonIndex];
            const bool pressed = IsVirtualButtonPressed(buttonIndex);
            const bool isSelector = (visualSlot == kVirtualSkillSelectorVisualSlot);
            const int hotKeySkillIndex = isSelector ? -1 : GetVirtualOverlayHotKeySkillIndex(visualSlot);

            // Armed state comes from the overlay's own selection, not from
            // Hero->CurrentSkill: two slots can hold the same skill, and the
            // combo rewrites CurrentSkill as it steps.
            const bool selected = (!isSelector && g_virtualSelectedSkillSlot == visualSlot);

            // The picker button lights while the picker is open, so it reads as
            // a toggle rather than another skill.
            const bool selectorOpen = isSelector
                && g_pSkillList != nullptr
                && g_pSkillList->IsSkillPickerOpen();

            // Same circular frame as the attack button (DrawVirtualCombatButtonFrame
            // is pure GL, no art needed) instead of the old desktop square skill-box
            // art, so the skill row and the attack button read as one family of
            // controls. assignGlow is the same "available to assign" ring the frame
            // already supports, just never used here before.
            DrawVirtualCombatButtonFrame(
                button.cx,
                button.cy,
                button.radius,
                pressed,
                assignModeActive && !isSelector);

            // Armed skill / open picker is a persistent state, not a touch flash,
            // so it needs its own marker rather than sharing the frame's "pressed"
            // feedback - that's only a slight alpha/scale nudge, too subtle to read
            // as "this is the one currently in use" (this was lost when the old
            // IMAGE_SKILLBOX_USE/IMAGE_SKILLBOX texture swap was replaced with the
            // circular frame). Same gold as the active chat tab, for one consistent
            // "this one's active" language across the touch UI.
            if (selected || selectorOpen)
            {
                glLineWidth(2.6f);
                DrawVirtualCircle(button.cx, button.cy, button.radius + 2.0f, 1.0f, 0.82f, 0.10f, 0.95f, false);
                glLineWidth(1.0f);
            }

            if (!isSelector && g_pSkillList != nullptr && hotKeySkillIndex >= 0)
            {
                ConfigureVirtualSkillIconNoBlendState();
                g_pSkillList->RenderSkillIcon(
                    hotKeySkillIndex,
                    button.cx - renderSkillIconW * 0.5f,
                    button.cy - renderSkillIconH * 0.5f,
                    renderSkillIconW,
                    renderSkillIconH,
                    0,
                    false);
            }
        }

        // Labels last, so they sit over the frames rather than under the next
        // one drawn. The picker has no skill icon of its own to identify it.
        {
            HFONT slotFont = g_hFontMini != nullptr ? g_hFontMini : g_hFont;
            for (int visualSlot = 0; visualSlot < kVirtualVisibleSkillButtonCount; ++visualSlot)
            {
                const VirtualButtonLayout& button = kVirtualButtons[kVirtualSkillButtonBase + visualSlot];
                const bool isSelector = (visualSlot == kVirtualSkillSelectorVisualSlot);

                // Positioned off the circle's own radius now rather than the old
                // rectangular frame's fixed width/height, so it stays anchored to
                // the bottom of the (larger) circular frame instead of floating
                // above it.
                TextDraw(slotFont,
                         static_cast<int>(button.cx - button.radius),
                         static_cast<int>(button.cy + button.radius - 9.0f),
                         isSelector ? 0xFFC0E0FF : 0xFFFFFFFF,
                         0x0,
                         static_cast<int>(button.radius * 2.0f),
                         0, 3,
                         "%s", isSelector ? "SKL" : std::to_string(visualSlot + 1).c_str());
            }
        }

        EndBitmap();
    }

    // The CHG button that used to sit in the bottom right corner is gone - the
    // MEN entry on the top bar toggles this grid now, so only the grid itself
    // is drawn here.
    if ((kShowVirtualAttackButton || kShowVirtualSkillButtons)
        && g_virtualRightPanelUtilityMode
        && !ShouldYieldVirtualRightPanelUtilityOverlay())
    {
        RenderVirtualRightPanelUtilityMode();
    }

    RenderVirtualMirrorHotKeySlots();
    RenderComboToggle();
    RenderTargetSelectButton();
    RenderAndroidGroundAim();
    RenderAndroidTradePicker();

    // Last, so the panel and its dimming layer sit over every other control.
    RenderAndroidTargetPicker();

#if 0
    // Disabled: custom Android HUD. We keep this code commented for now so it
    // can be restored later if needed, but the active mobile UI should remain
    // the original MU interface plus joystick movement only.

    for (int i = 0; i < static_cast<int>(kVirtualButtons.size()); ++i)
    {
        const bool isAttack = (i == kVirtualAttackButton);
        const bool isAssignGlow = assignModeActive && !isAttack;

        if (isAssignGlow)
        {
            // Skill button in assign-mode: keep a soft fill under the decorative ring
            // so the user still gets clear feedback.
            const float fillA = 0.68f + 0.08f * assignPulse;
            DrawVirtualCircle(kVirtualButtons[i].cx, kVirtualButtons[i].cy,
                kVirtualButtons[i].radius,
                0.18f, 0.55f, 0.80f, fillA, true);
        }
        else
        {
            // Decorative ring is drawn in the skill loop so there is no extra GL circle here.
        }
    }

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Top-right utility buttons (Inventory / Character / Settings) 鑺掗埀顑解偓?PNG icons 鑺掗垾婵冨亾鑺掗垾婵冨亾
    {
        const VirtualButtonLayout& attackButton = kVirtualButtons[kVirtualAttackButton];
        const float attackRingSize = attackButton.radius * 2.0f + 34.0f;
        const float attackIconSize = attackButton.radius * 1.48f + 5.0f;
        DrawIconButton(
            attackButton.cx - attackRingSize * 0.5f,
            attackButton.cy - attackRingSize * 0.5f,
            attackRingSize,
            attackRingSize,
            g_uiTex_skillline,
            IsVirtualButtonPressed(kVirtualAttackButton) ? 1.0f : 0.98f);
        DrawIconButton(
            attackButton.cx - attackIconSize * 0.5f,
            attackButton.cy - attackIconSize * 0.5f,
            attackIconSize,
            attackIconSize,
            g_uiTex_attack,
            IsVirtualButtonPressed(kVirtualAttackButton) ? 1.0f : 0.96f);

        const UITexture* kUtilIcons[kVirtualUtilityButtonCount] = {
            &g_uiTex_balo,
            &g_uiTex_character,
            &g_uiTex_setting,
            &g_uiTex_map,
        };
        for (int i = 0; i < kVirtualUtilityButtonCount; ++i)
        {
            if (i == kVirtualUtilityButtonChat)
            {
                continue;
            }
            const AndroidUiRect rect = GetVirtualUtilityButtonRect(i);
            DrawIconButton(rect.x, rect.y, rect.w, rect.h, *kUtilIcons[i], 1.0f);
        }
    }

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Bottom HUD 鑺掗埀顑解偓?HP/MP/AG/EXP (1/3 width, solid, numbers on bar) 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    // Solid bars, no blending artefacts. Numbers centered on each bar.
    {
        // Switch to normal alpha blend so solid colors render correctly over game world
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const float barW = kHudBarRight - kHudBarLeft;

        // Row y positions (stacked top-down from kHudStripY)
        const float yHP  = kHudStripY;
        const float yMP  = yHP  + kHudBarH + kHudBarGap;
        const float yAG  = yMP  + kHudBarH + kHudBarGap;
        const float yEXP = yAG  + kHudBarH + kHudBarGap;

        // HP 鑺掗埀顑解偓?bright red
        const DWORD curHP = CharacterAttribute->Life;
        const DWORD maxHP = std::max(1u, CharacterAttribute->LifeMax);
        DrawVirtualBarH(kHudBarLeft, yHP, barW, kHudBarH,
                        static_cast<float>(curHP) / static_cast<float>(maxHP),
                        0.95f, 0.15f, 0.15f,  0.22f, 0.04f, 0.04f);

        // MP 鑺掗埀顑解偓?bright blue
        const DWORD curMP = CharacterAttribute->Mana;
        const DWORD maxMP = std::max(1u, CharacterAttribute->ManaMax);
        DrawVirtualBarH(kHudBarLeft, yMP, barW, kHudBarH,
                        static_cast<float>(curMP) / static_cast<float>(maxMP),
                        0.15f, 0.45f, 1.00f,  0.04f, 0.08f, 0.28f);

        // AG 鑺掗埀顑解偓?cyan
        const DWORD curAG = CharacterAttribute->SkillMana;
        const DWORD maxAG = std::max(1u, CharacterAttribute->SkillManaMax);
        DrawVirtualBarH(kHudBarLeft, yAG, barW, kHudBarH,
                        static_cast<float>(curAG) / static_cast<float>(maxAG),
                        0.10f, 0.95f, 0.90f,  0.04f, 0.20f, 0.18f);

        // EXP 鑺掗埀顑解偓?gold
        float expRatio = 0.0f;
        if (CharacterAttribute->NextExperience > 0)
            expRatio = static_cast<float>(
                static_cast<double>(CharacterAttribute->Experience) /
                static_cast<double>(CharacterAttribute->NextExperience));
        DrawVirtualBarH(kHudBarLeft, yEXP, barW, kHudBarH,
                        expRatio,
                        1.00f, 0.85f, 0.05f,  0.22f, 0.18f, 0.02f);

        // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Numbers centered ON each bar 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
        // Use the legacy number atlas for sharper, anti-aliased digits like old UI.
        constexpr float kHudNumberScale   = 0.90f;
        constexpr float kHudNumberOffsetY = 0.50f;
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        SEASON3B::RenderNumber(kHudNumCenterX, yHP  + kHudNumberOffsetY, static_cast<int>(curHP), kHudNumberScale);
        SEASON3B::RenderNumber(kHudNumCenterX, yMP  + kHudNumberOffsetY, static_cast<int>(curMP), kHudNumberScale);
        SEASON3B::RenderNumber(kHudNumCenterX, yAG  + kHudNumberOffsetY, static_cast<int>(curAG), kHudNumberScale);
        const int expPercent = static_cast<int>(std::clamp(expRatio, 0.0f, 1.0f) * 100.0f + 0.5f);
        SEASON3B::RenderNumber(kHudNumCenterX, yEXP + kHudNumberOffsetY, expPercent, kHudNumberScale);

        // Restore additive blend used by the rest of the virtual pad rendering
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
    }

    // Top utility controls (map/minimap/zoom) must use regular alpha blending.
    // Additive blend can make them appear "missing" on bright backgrounds.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Map list button (top-left red square, in game area) 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    DrawVirtualMapButton();

    if (IsMiniMapToggleAvailable())
    {
        const AndroidUiRect rect = GetMiniMapButtonRect();
        // minimap.png icon 鑺掗埀顑解偓?full alpha always
        DrawIconButton(rect.x, rect.y, rect.w, rect.h, g_uiTex_minimap, 1.0f);
    }

    DrawVirtualChatUtilityButton();

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 ZOOM -/+ buttons (top center) 鑺掗垾婵冨亾鑺掗垾婵冨亾
    {
        auto drawZoomBtn = [](float uiLeft, float uiTop, float uiW, float uiH,
                              const char* label, bool isMinus)
        {
            const float x = UiToScreenX(uiLeft);
            const float yTop = UiToScreenY(uiTop);
            const float w = UiToScreenX(uiW);
            const float h = UiToScreenY(uiH);
            const float y = static_cast<float>(WindowHeight) - yTop - h;

            // Background
            glColor4f(0.10f, 0.10f, 0.10f, 0.82f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(x, y);
            glVertex2f(x + w, y);
            glVertex2f(x + w, y + h);
            glVertex2f(x, y + h);
            glEnd();

            // Border
            glColor4f(1.0f, 1.0f, 1.0f, 0.96f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(x + 0.5f, y + 0.5f);
            glVertex2f(x + w - 0.5f, y + 0.5f);
            glVertex2f(x + w - 0.5f, y + h - 0.5f);
            glVertex2f(x + 0.5f, y + h - 0.5f);
            glEnd();

            // Symbol: horizontal line (minus) + vertical line (plus)
            const float cx = x + w * 0.5f;
            const float cy = y + h * 0.5f;
            const float armLen = std::min(w, h) * 0.25f;
            glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
            glBegin(GL_LINES);
            glVertex2f(cx - armLen, cy);
            glVertex2f(cx + armLen, cy);
            if (!isMinus)
            {
                glVertex2f(cx, cy - armLen);
                glVertex2f(cx, cy + armLen);
            }
            glEnd();
        };

        drawZoomBtn(kZoomMinusX, kZoomButtonY, kZoomButtonW, kZoomButtonH, "ZOOM-", true);
        drawZoomBtn(kZoomPlusX,  kZoomButtonY, kZoomButtonW, kZoomButtonH, "ZOOM+", false);
    }

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Current skill box (legacy-like UI element on custom Android HUD) 鑺掗垾婵冨亾鑺掗垾婵冨亾
    if (kShowVirtualCurrentSkillBox)
    {
        const bool pickerOpen = (g_pSkillList != nullptr) && g_pSkillList->IsSkillPickerOpen();
        const GLuint frameImage = pickerOpen
            ? SEASON3B::CNewUISkillList::IMAGE_SKILLBOX_USE
            : SEASON3B::CNewUISkillList::IMAGE_SKILLBOX;

        ConfigureVirtualSkillIconNoBlendState();
        SEASON3B::RenderImage(
            frameImage,
            kVirtualCurrentSkillBoxX,
            kVirtualCurrentSkillBoxY,
            kVirtualCurrentSkillBoxW,
            kVirtualCurrentSkillBoxH);

        if (g_pSkillList != nullptr && Hero != nullptr)
        {
            g_pSkillList->RenderSkillIcon(
                static_cast<int>(Hero->CurrentSkill),
                kVirtualCurrentSkillBoxX + 6.0f,
                kVirtualCurrentSkillBoxY + 6.0f,
                20.0f,
                28.0f);
        }
    }

    // Restore standard blend state for anything that follows (consumable icons, etc.)
    EnableAlphaBlend();

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Consumable item icons 鑺掗垾婵冨亾鑺掗垾婵冨亾
    // RenderItem3D is a 3D function 鑺掗埀顑解偓?needs perspective projection, NOT the
    // 2D ortho mode set up by BeginBitmap.  Follow the same pattern used by
    // CNewUI3DCamera::Render() / CNewUIGoldBowmanLena::Render3D().
    {
        bool anyItem = false;
        for (int i = 0; i < kVirtualConsumableSlotCount; ++i)
            if (g_virtualConsumableSlots[i].itemType >= 0) { anyItem = true; break; }

        if (anyItem)
        {
            EndBitmap(); // leave 2D ortho

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glViewport2(0, 0, WindowWidth, WindowHeight);
            gluPerspective2(1.f,
                            static_cast<float>(WindowWidth) / static_cast<float>(WindowHeight),
                            RENDER_ITEMVIEW_NEAR,
                            RENDER_ITEMVIEW_FAR);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();
            GetOpenGLMatrix(CameraMatrix);
            EnableDepthTest();
            EnableDepthMask();
            glDisable(GL_BLEND);
            glClear(GL_DEPTH_BUFFER_BIT);

            for (int i = 0; i < kVirtualConsumableSlotCount; ++i)
            {
                const VirtualConsumableSlot& cs = g_virtualConsumableSlots[i];
                if (cs.itemType < 0) continue;

                const VirtualButtonLayout& layout = kVirtualConsumableSlots[i];
                const float iconHalf = layout.radius * 0.85f;
                RenderItem3D(
                    layout.cx - iconHalf,
                    layout.cy - iconHalf,
                    iconHalf * 2.0f,
                    iconHalf * 2.0f,
                    cs.itemType, cs.itemLevel,
                    0, 0);
            }

            UpdateMousePositionn();

            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();

            BeginBitmap(); // back to 2D ortho for quantity numbers
            glEnable(GL_BLEND);
        }
    }

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Consumable rings (same skillline frame for visual consistency) 鑺掗垾婵冨亾鑺掗垾婵冨亾
    for (int i = 0; i < kVirtualConsumableSlotCount; ++i)
    {
        const VirtualButtonLayout& layout = kVirtualConsumableSlots[i];
        DrawVirtualCircle(
            layout.cx,
            layout.cy,
            layout.radius + 11.0f,
            0.05f,
            0.05f,
            0.05f,
            (g_virtualConsumableSlots[i].itemType >= 0) ? 0.55f : 0.32f,
            true);
        const float ringSize = layout.radius * 2.0f + 20.0f;
        const float ringAlpha = (g_virtualConsumableSlots[i].itemType >= 0) ? 1.0f : 0.84f;
        DrawIconButton(
            layout.cx - ringSize * 0.5f,
            layout.cy - ringSize * 0.5f,
            ringSize,
            ringSize,
            g_uiTex_skillline,
            ringAlpha);
    }

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Consumable quantity numbers 鑺掗垾婵冨亾鑺掗垾婵冨亾
    // RenderNumber expects 640x480 virtual coords (same as inventory panel).
    // Durability==0 in ITEM means a stack of 1 (see CNewUIInventoryCtrl::GetItemCount).
    EnableAlphaTest();
    glColor4f(1.0f, 0.9f, 0.7f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    for (int i = 0; i < kVirtualConsumableSlotCount; ++i)
    {
        VirtualConsumableSlot& cs = g_virtualConsumableSlots[i];
        if (cs.itemType < 0)
            continue;

        const int idx = FindConsumableInInventory(cs.itemType, cs.itemLevel);
        if (idx < 0)
            continue;  // not found this frame 鑺掗埀顑解偓?keep slot, skip quantity

        // Get item data from the UI inventory (authoritative source)
        const ITEM* pFoundItem = (g_pMyInventory != nullptr)
                                  ? g_pMyInventory->FindItem(idx)
                                  : nullptr;
        if (pFoundItem == nullptr)
            continue;  // skip quantity display; do NOT clear slot here

        // Durability==0 means exactly 1 item in the slot (not empty)
        const int qty = (pFoundItem->Durability == 0)
                         ? 1
                         : static_cast<int>(pFoundItem->Durability);

        const VirtualButtonLayout& layout = kVirtualConsumableSlots[i];
        // Position number at bottom-right of the circle
        SEASON3B::RenderNumber(
            layout.cx + layout.radius * 0.5f,
            layout.cy + layout.radius * 0.65f,
            qty);
    }
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    EndBitmap();
#endif
}
} // namespace

// "The stick currently owns the hero's movement." It used to mean "the stick is
// faking a held mouse button", which is how it drove movement before it started
// issuing paths itself; the callers in ZzzInterface.cpp want the same answer
// either way - keep the click-to-move loop from re-pathing the hero at whatever
// the touch cursor was last left pointing at.
bool IsAndroidVirtualJoystickHoldingMovement()
{
    return g_virtualJoystickHoldingMovement;
}

// Called on map change and disconnect. The lock does clear itself once its
// target can no longer be resolved, but character Keys are reused between maps,
// so without an explicit reset a lock could survive a teleport and land on
// whatever new monster inherited that Key.
void AndroidClearTargetLock()
{
    ClearAndroidTargetLock("map-change");
    CancelAndroidGroundAim("map-change");
}

// Called from the scene phase right after MoveHero. It has to run there rather
// than from the touch handler: turning the aim point into a terrain tile uses
// the same camera matrices and terrain pick that MoveHero relies on, and those
// are only valid inside the frame.
void AndroidUpdateGroundAimCast()
{
    UpdateAndroidGroundAimCast();
}

bool AndroidShowCommandTradePicker()
{
    return ShowAndroidTradePickerFromCommand();
}

bool AndroidShowCommandPartyPicker()
{
    return ShowAndroidPartyPickerFromCommand();
}

bool AndroidShowCommandGuildPicker()
{
    return ShowAndroidGuildPickerFromCommand();
}

bool AndroidShowCommandDuelPicker()
{
    return ShowAndroidDuelPickerFromCommand();
}

bool AndroidShowCommandFriendPicker()
{
    return ShowAndroidFriendPickerFromCommand();
}

bool AndroidTriggerNormalAttackButton()
{
    return AndroidTriggerNormalAttackButtonInternal();
}

bool AndroidTriggerHotKeySkillTap(int hotKeySkillIndex)
{
    return AndroidTriggerHotKeySkillTapInternal(hotKeySkillIndex);
}

// RenderItemMenu lives in the anonymous namespace above, so it has internal
// linkage and cannot be called from ZzzScene directly. Same wrapper pattern as
// the trigger functions above.
void AndroidRenderItemMenu()
{
    RenderItemMenu();
}

float AndroidGetCompactMiniMapTopY()
{
    return GetAndroidCompactMiniMapTopYInternal();
}

float AndroidGetCompactMiniMapLeftX()
{
    return GetAndroidCompactMiniMapLeftXInternal();
}

bool AndroidGetMoveMapWindowPosition(int panelWidth, int panelHeight, int* outX, int* outY)
{
    return GetAndroidMoveMapWindowPositionInternal(panelWidth, panelHeight, outX, outY);
}

bool AndroidGetMiniMapPanelRect(float* outX, float* outY, float* outW, float* outH)
{
    if (outX == nullptr || outY == nullptr || outW == nullptr || outH == nullptr)
    {
        return false;
    }

    const AndroidUiRect rect = GetTopBarMiniMapPanelRect();
    *outX = rect.x;
    *outY = rect.y;
    *outW = rect.w;
    *outH = rect.h;
    return true;
}

static void UpdateMouseFromPixel(int pixelX, int pixelY, int screenW, int screenH)
{
    const int safeW = (screenW > 0) ? screenW : 1;
    const int safeH = (screenH > 0) ? screenH : 1;

    const int clampedX = std::clamp(pixelX, 0, safeW - 1);
    const int clampedY = std::clamp(pixelY, 0, safeH - 1);

    MouseX = (int)((float)clampedX * 640.0f / (float)safeW);
    MouseY = (int)((float)clampedY * 480.0f / (float)safeH);
    MouseX = std::clamp(MouseX, 0, 640);
    MouseY = std::clamp(MouseY, 0, 480);
}

static void UpdateMouseFromTouch(const SDL_TouchFingerEvent& touch, int screenW, int screenH)
{
    const int safeW = (screenW > 0) ? screenW : 1;
    const int safeH = (screenH > 0) ? screenH : 1;

    const int pixelX = std::clamp((int)(touch.x * (float)safeW), 0, safeW - 1);
    const int pixelY = std::clamp((int)(touch.y * (float)safeH), 0, safeH - 1);

    UpdateMouseFromPixel(pixelX, pixelY, safeW, safeH);
}

BOOL Util_CheckOption(std::wstring /*lpszCommandLine*/, wchar_t /*cOption*/, std::wstring& /*lpszString*/) {
    return FALSE;
}

// Application-level DestroyWindow() 鑺掗埀顑解偓?not the Win32 API (which takes HWND)
// Declared in Winmain.h as extern void DestroyWindow();
void DestroyWindow() {}

// =============================================================================
// Audio - MuAudio (SoundPool + MediaPlayer) replaces wzAudio + DirectSound
//
// SDL_mixer used to sit here and could never work: sokol_app replaces SDL_main,
// so SDL's Android bootstrap never runs and no subsystem will start. The
// bridges below live in android_link_stubs.cpp next to the rest of the JNI.
// =============================================================================

extern "C" void AndroidAudioInit();
extern "C" void AndroidAudioPlayMusic(const char* absolutePath, bool loop);
extern "C" void AndroidAudioStopMusic();
extern "C" bool AndroidAudioIsMusicPlaying();

static char g_LastFailedMp3Name[256] = {};
static uint32_t g_LastFailedMp3Tick = 0;
static bool g_AndroidAudioAvailable = false;
static std::unordered_map<std::string, std::string> g_MusicPathCache;

static inline bool EqualsIgnoreCaseAscii(std::string_view lhs, std::string_view rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (size_t i = 0; i < lhs.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
            std::tolower(static_cast<unsigned char>(rhs[i])))
        {
            return false;
        }
    }

    return true;
}

static std::string ToAbsoluteGenericPath(const std::filesystem::path& path)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path absolute = fs::absolute(path, ec);
    if (!ec)
    {
        return absolute.generic_string();
    }

    return path.generic_string();
}

static std::string NormalizeMusicPath(const char* name)
{
    std::string normalized = name ? name : "";
    for (char& c : normalized)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }

    return normalized;
}

static std::string ResolveMusicPath(const char* name)
{
    namespace fs = std::filesystem;

    const std::string requested = NormalizeMusicPath(name);
    if (requested.empty())
    {
        return requested;
    }

    const auto cacheIt = g_MusicPathCache.find(requested);
    if (cacheIt != g_MusicPathCache.end())
    {
        return cacheIt->second;
    }

    if (fs::exists(requested))
    {
        const std::string resolved = ToAbsoluteGenericPath(fs::path(requested));
        g_MusicPathCache[requested] = resolved;
        return resolved;
    }

    static constexpr std::array<const char*, 2> kExternalBaseDirs = {
        "/sdcard/Android/data/com.muonline.client/files",
        "/storage/emulated/0/Android/data/com.muonline.client/files"
    };

    for (const char* baseRaw : kExternalBaseDirs)
    {
        const fs::path absoluteCandidate = fs::path(baseRaw) / requested;
        if (fs::exists(absoluteCandidate))
        {
            const std::string resolved = ToAbsoluteGenericPath(absoluteCandidate);
            g_MusicPathCache[requested] = resolved;
            return resolved;
        }
    }

    const std::string fileName = fs::path(requested).filename().string();
    static constexpr std::array<const char*, 4> kMusicRoots = {
        "Data/Music",
        "data/music",
        "Data/music",
        "data/Music"
    };

    for (const char* baseRaw : kExternalBaseDirs)
    {
        for (const char* rootRaw : kMusicRoots)
        {
            const fs::path root = fs::path(baseRaw) / rootRaw;
            if (!fs::exists(root) || !fs::is_directory(root))
            {
                continue;
            }

            const fs::path directCandidate = root / fileName;
            if (fs::exists(directCandidate))
            {
                const std::string resolved = ToAbsoluteGenericPath(directCandidate);
                g_MusicPathCache[requested] = resolved;
                return resolved;
            }

            for (const auto& entry : fs::directory_iterator(root))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                const std::string entryName = entry.path().filename().string();
                if (EqualsIgnoreCaseAscii(entryName, fileName))
                {
                    const std::string resolved = ToAbsoluteGenericPath(entry.path());
                    g_MusicPathCache[requested] = resolved;
                    return resolved;
                }
            }
        }
    }

    for (const char* rootRaw : kMusicRoots)
    {
        const fs::path root(rootRaw);
        if (!fs::exists(root) || !fs::is_directory(root))
        {
            continue;
        }

        const fs::path directCandidate = root / fileName;
        if (fs::exists(directCandidate))
        {
            const std::string resolved = ToAbsoluteGenericPath(directCandidate);
            g_MusicPathCache[requested] = resolved;
            return resolved;
        }

        for (const auto& entry : fs::directory_iterator(root))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::string entryName = entry.path().filename().string();
            if (EqualsIgnoreCaseAscii(entryName, fileName))
            {
                const std::string resolved = ToAbsoluteGenericPath(entry.path());
                g_MusicPathCache[requested] = resolved;
                return resolved;
            }
        }
    }

    g_MusicPathCache[requested] = requested;
    return requested;
}

void StopMp3(char* Name, BOOL bEnforce)
{
    if (!g_AndroidAudioAvailable) return;
    if (!m_MusicOnOff && !bEnforce) return;
    if (!Name) return;

    if (Mp3FileName[0] != '\0' && strcmp(Name, Mp3FileName) == 0)
    {
        AndroidAudioStopMusic();
        Mp3FileName[0] = '\0';
    }
}

void PlayMp3(char* Name, BOOL bEnforce)
{
    if (!g_AndroidAudioAvailable) return;
    if (Destroy) return;
    if (!Name || Name[0] == '\0') return;
    if (!m_MusicOnOff && !bEnforce) return;

    // Same track already running: nothing to do. MediaPlayer loops on its own,
    // so unlike the old mixer path there is no halted-but-loaded state to
    // resume from.
    if (strcmp(Name, Mp3FileName) == 0 && AndroidAudioIsMusicPlaying())
    {
        return;
    }

    const uint32_t now = MU_MobileGetTicks();
    constexpr uint32_t kFailedRetryMs = 3000;
    if (g_LastFailedMp3Name[0] != '\0'
        && strcmp(Name, g_LastFailedMp3Name) == 0
        && (now - g_LastFailedMp3Tick) < kFailedRetryMs)
    {
        return;
    }

    // ResolveMusicPath returns an absolute path, which MediaPlayer requires:
    // it resolves a relative one against the JVM's user.dir ("/"), not against
    // the working directory this process chdir'd to.
    const std::string path = ResolveMusicPath(Name);

    if (!std::filesystem::exists(path))
    {
        static int s_missLogCount = 0;
        if (s_missLogCount < 8)
        {
            char cwd[512] = {};
            const char* cwdPtr = getcwd(cwd, sizeof(cwd));
            LOGE("PlayMp3 missing requested=[%s] resolved=[%s] cwd=[%s]",
                Name, path.c_str(), cwdPtr ? cwdPtr : "(getcwd failed)");
            ++s_missLogCount;
        }
        strncpy(g_LastFailedMp3Name, Name, sizeof(g_LastFailedMp3Name) - 1);
        g_LastFailedMp3Name[sizeof(g_LastFailedMp3Name) - 1] = '\0';
        g_LastFailedMp3Tick = now;

        // Record it as current anyway so this does not retry every frame.
        strncpy(Mp3FileName, Name, sizeof(Mp3FileName) - 1);
        Mp3FileName[sizeof(Mp3FileName) - 1] = '\0';
        return;
    }

    AndroidAudioPlayMusic(path.c_str(), true);

    static int s_playOkLogCount = 0;
    if (s_playOkLogCount < 8)
    {
        LOGI("PlayMp3 requested=[%s] resolved=[%s]", Name, path.c_str());
        ++s_playOkLogCount;
    }

    strncpy(Mp3FileName, Name, sizeof(Mp3FileName) - 1);
    Mp3FileName[sizeof(Mp3FileName) - 1] = '\0';
    g_LastFailedMp3Name[0] = '\0';
    g_LastFailedMp3Tick = 0;
}

bool IsEndMp3()           { return !g_AndroidAudioAvailable || !AndroidAudioIsMusicPlaying(); }
int  GetMp3PlayPosition() { return (g_AndroidAudioAvailable && AndroidAudioIsMusicPlaying()) ? 50 : 100; }

// =============================================================================
// Stubs for Windows-only features
// =============================================================================

void CheckHack()
{
#ifdef NEW_PROTOCOL_SYSTEM
    gProtocolSend.SendCheckOnline();
#else
    if (!g_bGameServerConnected || CharacterAttribute == nullptr)
    {
        return;
    }

    const SOCKET socket = SocketClient.GetSocket();
    if (socket == INVALID_SOCKET)
    {
        return;
    }

    WORD physSpeed = CharacterAttribute->AttackSpeed;
    WORD magicSpeed = CharacterAttribute->MagicSpeed;
    const DWORD currentTick = static_cast<DWORD>(MU_MobileGetTicks());

    if ((CharacterAttribute->Ability & ABILITY_FAST_ATTACK_SPEED) != 0
        || (CharacterAttribute->Ability & ABILITY_FAST_ATTACK_SPEED2) != 0)
    {
        physSpeed = static_cast<WORD>(std::max<int>(0, static_cast<int>(physSpeed) - 20));
        magicSpeed = static_cast<WORD>(std::max<int>(0, static_cast<int>(magicSpeed) - 20));
    }

    uint8_t packet[12] = {};
    packet[0] = 0xC1;
    packet[1] = sizeof(packet);
    packet[2] = 0x0E;
    std::memcpy(packet + 4, &currentTick, sizeof(DWORD));
    std::memcpy(packet + 8, &physSpeed, sizeof(WORD));
    std::memcpy(packet + 10, &magicSpeed, sizeof(WORD));

    mu::Xor32Encrypt(packet, static_cast<int>(sizeof(packet)), 2);

    BYTE encryptSource[MAX_SPE_BUFFERSIZE_] = {};
    std::memcpy(encryptSource, packet, sizeof(packet));
    encryptSource[1] = g_byPacketSerialSend++;

    PBMSG_ENCRYPTED encrypted {};
    const int encryptedBodySize = g_SimpleModulusCS.Encrypt(nullptr, encryptSource + 1, static_cast<int>(sizeof(packet)) - 1);
    if (encryptedBodySize <= 0 || encryptedBodySize >= 256)
    {
        return;
    }

    encrypted.Code = 0xC3;
    encrypted.Size = static_cast<BYTE>(encryptedBodySize + 2);
    g_SimpleModulusCS.Encrypt(encrypted.byBuffer, encryptSource + 1, static_cast<int>(sizeof(packet)) - 1);
    SocketClient.sSend(socket, reinterpret_cast<char*>(&encrypted), encrypted.Size);

    if (!First)
    {
        First = true;
        FirstTime = static_cast<int>(currentTick);
    }
#endif
}
DWORD GetCheckSum(WORD /*wKey*/) { return 0; }  // GameGuard not on Android

void SetMaxMessagePerCycle(int messages)
{
    constexpr int custom_min = 3;
    g_MaxMessagePerCycle = (messages > 0) ? std::max<int>(messages, custom_min) : messages;
}

namespace
{
    float g_currentAdaptiveEffectScale = 1.0f;

    struct AdaptivePerfState
    {
        bool enabled = true;
        bool initialized = false;
        bool isEmulator = false;
        // User controls render-level slider. Adaptive keeps full effects ON
        // and only changes packet budget under stress.
        int lowFpsStreak = 0;
        int highFpsStreak = 0;
        int defaultMessageBudget = 90;
        int minMessageBudget = 35;
        // Software frame cap (VSync is deliberately off - see SDL_GL_SetSwapInterval(0)
        // below - so this is the only thing that stops the loop rendering flat-out).
        // Capped at 60: doesn't affect areas that are already below 60fps (the CPU/GPU
        // bottleneck there needs the draw-call/shader/skinning work, not a cap), but
        // avoids burning GPU cycles - and the resulting heat/throttling - in menus and
        // lighter scenes that could otherwise render past what the display can show.
        double targetFps = 60.0;
        double lowFpsThreshold = 50.0;
        double highFpsThreshold = 58.0;
        double minEffectScale = 0.42;
        double effectScaleStep = 0.15;
        double effectScaleHysteresis = 0.06;
        uint32_t fxAdjustCooldownMs = 650;
        uint32_t lastFxAdjustTick = 0;
        int minAdaptiveRenderLevel = 0;
        int maxAdaptiveRenderLevel = 4;
        int renderDownStreak = 0;
        int renderUpStreak = 0;
        uint32_t renderAdjustCooldownMs = 1500;
        uint32_t lastRenderAdjustTick = 0;
    };

    AdaptivePerfState g_adaptivePerf;

    static int ClampRenderLevel(int level)
    {
        return std::clamp(level, 0, 4);
    }

    static int CountLiveCharacters()
    {
        if (!CharactersClient) return 0;
        int count = 0;
        for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
        {
            if (CharactersClient[i].Object.Live) ++count;
        }
        return count;
    }

    struct CrowdTypeEntry
    {
        int type = -1;
        int count = 0;
    };

    struct SceneCrowdSnapshot
    {
        int liveCharacters = 0;
        int visibleCharacters = 0;
        int visiblePlayers = 0;
        int visibleMonsters = 0;
        int visibleNpcs = 0;
        int visiblePets = 0;
        int visiblePriorityCharacters = 0;
        int liveWorldObjects = 0;
        int visibleWorldObjects = 0;
        int visibleOperateObjects = 0;
        int visibleTrapObjects = 0;
        int dominantMonsterType = -1;
        int dominantMonsterCount = 0;
        int dominantObjectType = -1;
        int dominantObjectCount = 0;
    };

    static void TrackCrowdType(std::array<CrowdTypeEntry, 8>& slots, const int type)
    {
        for (CrowdTypeEntry& slot : slots)
        {
            if (slot.count > 0 && slot.type == type)
            {
                ++slot.count;
                return;
            }
        }

        for (CrowdTypeEntry& slot : slots)
        {
            if (slot.count == 0)
            {
                slot.type = type;
                slot.count = 1;
                return;
            }
        }

        CrowdTypeEntry* weakest = &slots[0];
        for (CrowdTypeEntry& slot : slots)
        {
            if (slot.count < weakest->count)
            {
                weakest = &slot;
            }
        }

        if (weakest->count <= 1)
        {
            weakest->type = type;
            weakest->count = 1;
        }
    }

    static CrowdTypeEntry FindDominantCrowdType(const std::array<CrowdTypeEntry, 8>& slots)
    {
        CrowdTypeEntry dominant;
        for (const CrowdTypeEntry& slot : slots)
        {
            if (slot.count > dominant.count)
            {
                dominant = slot;
            }
        }
        return dominant;
    }

    static SceneCrowdSnapshot CaptureSceneCrowdSnapshot()
    {
        SceneCrowdSnapshot snapshot;
        std::array<CrowdTypeEntry, 8> monsterTypes = {};
        std::array<CrowdTypeEntry, 8> objectTypes = {};

        if (CharactersClient)
        {
            for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
            {
                CHARACTER* c = &CharactersClient[i];
                OBJECT* o = &c->Object;
                if (!o->Live)
                {
                    continue;
                }

                ++snapshot.liveCharacters;
                if (!o->Visible)
                {
                    continue;
                }

                ++snapshot.visibleCharacters;
                const bool isPriority =
                    (c == Hero) ||
                    (i == SelectedCharacter) ||
                    (i == SelectedNpc) ||
                    (Hero != nullptr && Hero->TargetCharacter == c->Key);
                if (isPriority)
                {
                    ++snapshot.visiblePriorityCharacters;
                }

                switch (o->Kind)
                {
                case KIND_PLAYER:
                    ++snapshot.visiblePlayers;
                    break;
                case KIND_MONSTER:
                    ++snapshot.visibleMonsters;
                    TrackCrowdType(monsterTypes, o->Type);
                    break;
                case KIND_NPC:
                    ++snapshot.visibleNpcs;
                    break;
                case KIND_PET:
                    ++snapshot.visiblePets;
                    break;
                default:
                    break;
                }
            }
        }

        for (int block = 0; block < 256; ++block)
        {
            OBJECT* o = ObjectBlock[block].Head;
            while (o != nullptr)
            {
                if (o->Live)
                {
                    ++snapshot.liveWorldObjects;
                    if (o->Visible)
                    {
                        ++snapshot.visibleWorldObjects;
                        if (o->Kind == KIND_OPERATE)
                        {
                            ++snapshot.visibleOperateObjects;
                        }
                        else if (o->Kind == KIND_TRAP)
                        {
                            ++snapshot.visibleTrapObjects;
                        }

                        if (o->Kind != KIND_PLAYER && o->Kind != KIND_MONSTER)
                        {
                            TrackCrowdType(objectTypes, o->Type);
                        }
                    }
                }
                o = o->Next;
            }
        }

        const CrowdTypeEntry dominantMonster = FindDominantCrowdType(monsterTypes);
        snapshot.dominantMonsterType = dominantMonster.type;
        snapshot.dominantMonsterCount = dominantMonster.count;

        const CrowdTypeEntry dominantObject = FindDominantCrowdType(objectTypes);
        snapshot.dominantObjectType = dominantObject.type;
        snapshot.dominantObjectCount = dominantObject.count;

        return snapshot;
    }

    static const char* GetPerfMonsterDebugName(const int type)
    {
        if (type < 0)
        {
            return "n/a";
        }

        const char* const name = getMonsterName(type);
        return (name != nullptr && name[0] != '\0') ? name : "unnamed";
    }

    static const char* GetPerfObjectDebugName(const int type)
    {
        if (type < 0 || type >= MAX_MODELS)
        {
            return "n/a";
        }

        const char* const name = Models[type].Name;
        return (name != nullptr && name[0] != '\0') ? name : "unnamed";
    }

    static const char* GetPerfWorldDebugName(const int world)
    {
        if (world < 0 || world >= NUM_WD)
        {
            return "unknown";
        }

        const char* const name = gMapManager.GetMapName(world);
        return (name != nullptr && name[0] != '\0') ? name : "unknown";
    }

    static void UpdateAdaptivePerformance(double fps, double frameMs)
    {
        if (!g_adaptivePerf.enabled)
            return;

        // Full-effect mode: do not thin effects or change render level.
        // Render-level slider remains user-controlled.
        g_currentAdaptiveEffectScale = 1.0f;

        if (fps < g_adaptivePerf.lowFpsThreshold)
        {
            ++g_adaptivePerf.lowFpsStreak;
            g_adaptivePerf.highFpsStreak = 0;
        }
        else if (fps > g_adaptivePerf.highFpsThreshold)
        {
            ++g_adaptivePerf.highFpsStreak;
            g_adaptivePerf.lowFpsStreak = 0;
        }
        else
        {
            g_adaptivePerf.lowFpsStreak  = std::max(0, g_adaptivePerf.lowFpsStreak  - 1);
            g_adaptivePerf.highFpsStreak = std::max(0, g_adaptivePerf.highFpsStreak - 1);
        }

        // Reduce per-frame packet budget if FPS stays low for 3+ windows.
        if (g_adaptivePerf.lowFpsStreak >= 3 &&
            g_MaxMessagePerCycle > g_adaptivePerf.minMessageBudget)
        {
            const int newBudget = std::max(g_adaptivePerf.minMessageBudget,
                g_MaxMessagePerCycle - 10);
            LOGW("ADAPT net budget down: fps=%.1f frameMs=%.2f maxMsg %d->%d",
                fps, frameMs, g_MaxMessagePerCycle, newBudget);
            SetMaxMessagePerCycle(newBudget);
            return;
        }

        // Recover packet budget once FPS is stable.
        if (g_adaptivePerf.highFpsStreak >= 4 &&
            g_MaxMessagePerCycle < g_adaptivePerf.defaultMessageBudget)
        {
            g_adaptivePerf.highFpsStreak = 0;
            g_adaptivePerf.lowFpsStreak  = 0;
            const int newBudget = std::min(g_adaptivePerf.defaultMessageBudget,
                g_MaxMessagePerCycle + 5);
            LOGI("ADAPT net budget up: fps=%.1f frameMs=%.2f maxMsg %d->%d",
                fps, frameMs, g_MaxMessagePerCycle, newBudget);
            SetMaxMessagePerCycle(newBudget);
        }
    }
}

GLvoid KillGLWindow(GLvoid)
{
    g_hDC = nullptr;
    g_hRC = nullptr;
}

void DestroySound()
{
    if (!g_AndroidAudioAvailable)
    {
        return;
    }

    AndroidAudioStopMusic();
    AllStopSound();
    Mp3FileName[0] = '\0';
    g_AndroidAudioAvailable = false;
    LOGI("Audio destroyed");
}

void DestroyWindow_Android()
{
    GameConfig::GetInstance().SetVolumeLevel(g_pOption ? g_pOption->GetVolumeLevel() : 5);
    GameConfig::GetInstance().Save();

    CUIMng::Instance().Release();
    ReleaseCharacters();

    SAFE_DELETE(GateAttribute);
    SAFE_DELETE(SkillAttribute);
    SAFE_DELETE(CharacterMachine);
    

    gMapManager.DeleteObjects();
    for (int i = MODEL_LOGO; i < MAX_MODELS; i++) Models[i].Release();
    Bitmaps.UnloadAllImages();

    SAFE_DELETE_ARRAY(CharacterMemoryDump);
    SAFE_DELETE_ARRAY(ItemAttRibuteMemoryDump);
    SAFE_DELETE_ARRAY(RendomMemoryDump);
    SAFE_DELETE_ARRAY(ModelsDump);

    SAFE_DELETE(g_pMercenaryInputBox);
    SAFE_DELETE(g_pSingleTextInputBox);
    SAFE_DELETE(g_pSinglePasswdInputBox);
    SAFE_DELETE(g_pUIMapName);
    SAFE_DELETE(g_pTimer);
    SAFE_DELETE(g_pUIManager);
    SAFE_DELETE(pMultiLanguage);




    if (g_hFont) {
        DeleteObject((HGDIOBJ)g_hFont);
        g_hFont = nullptr;
    }
    if (g_hFontBold) {
        DeleteObject((HGDIOBJ)g_hFontBold);
        g_hFontBold = nullptr;
    }
    if (g_hFontBig) {
        DeleteObject((HGDIOBJ)g_hFontBig);
        g_hFontBig = nullptr;
    }
    if (g_hFixFont) {
        DeleteObject((HGDIOBJ)g_hFixFont);
        g_hFixFont = nullptr;
    }
    AndroidGDI_Shutdown();

    LOGI("Game systems destroyed");
}

static SDL_Keymod ConvertSappModifiersToSDLKeymod(uint32_t modifiers)
{
    int result = KMOD_NONE;
    if ((modifiers & SAPP_MODIFIER_SHIFT) != 0) result |= KMOD_SHIFT;
    if ((modifiers & SAPP_MODIFIER_CTRL) != 0)  result |= KMOD_CTRL;
    if ((modifiers & SAPP_MODIFIER_ALT) != 0)   result |= KMOD_ALT;
    if ((modifiers & SAPP_MODIFIER_SUPER) != 0) result |= KMOD_GUI;
    return static_cast<SDL_Keymod>(result);
}

static SDL_Keycode ConvertSappKeycodeToSDLKeycode(sapp_keycode keycode)
{
    if ((keycode >= SAPP_KEYCODE_SPACE && keycode <= SAPP_KEYCODE_GRAVE_ACCENT) ||
        (keycode >= SAPP_KEYCODE_0 && keycode <= SAPP_KEYCODE_9) ||
        (keycode >= SAPP_KEYCODE_A && keycode <= SAPP_KEYCODE_Z))
    {
        return static_cast<SDL_Keycode>(keycode);
    }

    switch (keycode)
    {
    case SAPP_KEYCODE_ENTER:        return SDLK_RETURN;
    case SAPP_KEYCODE_TAB:          return SDLK_TAB;
    case SAPP_KEYCODE_BACKSPACE:    return SDLK_BACKSPACE;
    case SAPP_KEYCODE_INSERT:       return SDLK_INSERT;
    case SAPP_KEYCODE_DELETE:       return SDLK_DELETE;
    case SAPP_KEYCODE_RIGHT:        return SDLK_RIGHT;
    case SAPP_KEYCODE_LEFT:         return SDLK_LEFT;
    case SAPP_KEYCODE_DOWN:         return SDLK_DOWN;
    case SAPP_KEYCODE_UP:           return SDLK_UP;
    case SAPP_KEYCODE_PAGE_UP:      return SDLK_PAGEUP;
    case SAPP_KEYCODE_PAGE_DOWN:    return SDLK_PAGEDOWN;
    case SAPP_KEYCODE_HOME:         return SDLK_HOME;
    case SAPP_KEYCODE_END:          return SDLK_END;
    case SAPP_KEYCODE_CAPS_LOCK:    return SDLK_CAPSLOCK;
    case SAPP_KEYCODE_SCROLL_LOCK:  return SDLK_SCROLLLOCK;
    case SAPP_KEYCODE_NUM_LOCK:     return SDLK_NUMLOCKCLEAR;
    case SAPP_KEYCODE_PRINT_SCREEN: return SDLK_PRINTSCREEN;
    case SAPP_KEYCODE_PAUSE:        return SDLK_PAUSE;
    case SAPP_KEYCODE_F1:           return SDLK_F1;
    case SAPP_KEYCODE_F2:           return SDLK_F2;
    case SAPP_KEYCODE_F3:           return SDLK_F3;
    case SAPP_KEYCODE_F4:           return SDLK_F4;
    case SAPP_KEYCODE_F5:           return SDLK_F5;
    case SAPP_KEYCODE_F6:           return SDLK_F6;
    case SAPP_KEYCODE_F7:           return SDLK_F7;
    case SAPP_KEYCODE_F8:           return SDLK_F8;
    case SAPP_KEYCODE_F9:           return SDLK_F9;
    case SAPP_KEYCODE_F10:          return SDLK_F10;
    case SAPP_KEYCODE_F11:          return SDLK_F11;
    case SAPP_KEYCODE_F12:          return SDLK_F12;
    case SAPP_KEYCODE_KP_0:         return SDLK_KP_0;
    case SAPP_KEYCODE_KP_1:         return SDLK_KP_1;
    case SAPP_KEYCODE_KP_2:         return SDLK_KP_2;
    case SAPP_KEYCODE_KP_3:         return SDLK_KP_3;
    case SAPP_KEYCODE_KP_4:         return SDLK_KP_4;
    case SAPP_KEYCODE_KP_5:         return SDLK_KP_5;
    case SAPP_KEYCODE_KP_6:         return SDLK_KP_6;
    case SAPP_KEYCODE_KP_7:         return SDLK_KP_7;
    case SAPP_KEYCODE_KP_8:         return SDLK_KP_8;
    case SAPP_KEYCODE_KP_9:         return SDLK_KP_9;
    case SAPP_KEYCODE_KP_DECIMAL:   return SDLK_KP_PERIOD;
    case SAPP_KEYCODE_KP_DIVIDE:    return SDLK_KP_DIVIDE;
    case SAPP_KEYCODE_KP_MULTIPLY:  return SDLK_KP_MULTIPLY;
    case SAPP_KEYCODE_KP_SUBTRACT:  return SDLK_KP_MINUS;
    case SAPP_KEYCODE_KP_ADD:       return SDLK_KP_PLUS;
    case SAPP_KEYCODE_KP_ENTER:     return SDLK_KP_ENTER;
    case SAPP_KEYCODE_LEFT_SHIFT:   return SDLK_LSHIFT;
    case SAPP_KEYCODE_LEFT_CONTROL: return SDLK_LCTRL;
    case SAPP_KEYCODE_LEFT_ALT:     return SDLK_LALT;
    case SAPP_KEYCODE_LEFT_SUPER:   return SDLK_LGUI;
    case SAPP_KEYCODE_RIGHT_SHIFT:  return SDLK_RSHIFT;
    case SAPP_KEYCODE_RIGHT_CONTROL:return SDLK_RCTRL;
    case SAPP_KEYCODE_RIGHT_ALT:    return SDLK_RALT;
    case SAPP_KEYCODE_RIGHT_SUPER:  return SDLK_RGUI;
    case SAPP_KEYCODE_MENU:         return SDLK_MENU;
    case SAPP_KEYCODE_ESCAPE:       return SDLK_AC_BACK;
    default:                        return SDLK_UNKNOWN;
    }
}

static SDL_Scancode ConvertSappKeycodeToSDLScancode(sapp_keycode keycode)
{
    if (keycode >= SAPP_KEYCODE_A && keycode <= SAPP_KEYCODE_Z)
    {
        return static_cast<SDL_Scancode>(SDL_SCANCODE_A + (keycode - SAPP_KEYCODE_A));
    }
    if (keycode >= SAPP_KEYCODE_0 && keycode <= SAPP_KEYCODE_9)
    {
        return static_cast<SDL_Scancode>(SDL_SCANCODE_0 + (keycode - SAPP_KEYCODE_0));
    }
    if (keycode >= SAPP_KEYCODE_F1 && keycode <= SAPP_KEYCODE_F12)
    {
        return static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + (keycode - SAPP_KEYCODE_F1));
    }
    if (keycode >= SAPP_KEYCODE_KP_0 && keycode <= SAPP_KEYCODE_KP_9)
    {
        return static_cast<SDL_Scancode>(SDL_SCANCODE_KP_0 + (keycode - SAPP_KEYCODE_KP_0));
    }

    switch (keycode)
    {
    case SAPP_KEYCODE_SPACE:         return SDL_SCANCODE_SPACE;
    case SAPP_KEYCODE_APOSTROPHE:    return SDL_SCANCODE_APOSTROPHE;
    case SAPP_KEYCODE_COMMA:         return SDL_SCANCODE_COMMA;
    case SAPP_KEYCODE_MINUS:         return SDL_SCANCODE_MINUS;
    case SAPP_KEYCODE_PERIOD:        return SDL_SCANCODE_PERIOD;
    case SAPP_KEYCODE_SLASH:         return SDL_SCANCODE_SLASH;
    case SAPP_KEYCODE_SEMICOLON:     return SDL_SCANCODE_SEMICOLON;
    case SAPP_KEYCODE_EQUAL:         return SDL_SCANCODE_EQUALS;
    case SAPP_KEYCODE_LEFT_BRACKET:  return SDL_SCANCODE_LEFTBRACKET;
    case SAPP_KEYCODE_BACKSLASH:     return SDL_SCANCODE_BACKSLASH;
    case SAPP_KEYCODE_RIGHT_BRACKET: return SDL_SCANCODE_RIGHTBRACKET;
    case SAPP_KEYCODE_GRAVE_ACCENT:  return SDL_SCANCODE_GRAVE;
    case SAPP_KEYCODE_ENTER:         return SDL_SCANCODE_RETURN;
    case SAPP_KEYCODE_TAB:           return SDL_SCANCODE_TAB;
    case SAPP_KEYCODE_BACKSPACE:     return SDL_SCANCODE_BACKSPACE;
    case SAPP_KEYCODE_INSERT:        return SDL_SCANCODE_INSERT;
    case SAPP_KEYCODE_DELETE:        return SDL_SCANCODE_DELETE;
    case SAPP_KEYCODE_RIGHT:         return SDL_SCANCODE_RIGHT;
    case SAPP_KEYCODE_LEFT:          return SDL_SCANCODE_LEFT;
    case SAPP_KEYCODE_DOWN:          return SDL_SCANCODE_DOWN;
    case SAPP_KEYCODE_UP:            return SDL_SCANCODE_UP;
    case SAPP_KEYCODE_PAGE_UP:       return SDL_SCANCODE_PAGEUP;
    case SAPP_KEYCODE_PAGE_DOWN:     return SDL_SCANCODE_PAGEDOWN;
    case SAPP_KEYCODE_HOME:          return SDL_SCANCODE_HOME;
    case SAPP_KEYCODE_END:           return SDL_SCANCODE_END;
    case SAPP_KEYCODE_CAPS_LOCK:     return SDL_SCANCODE_CAPSLOCK;
    case SAPP_KEYCODE_SCROLL_LOCK:   return SDL_SCANCODE_SCROLLLOCK;
    case SAPP_KEYCODE_NUM_LOCK:      return SDL_SCANCODE_NUMLOCKCLEAR;
    case SAPP_KEYCODE_PRINT_SCREEN:  return SDL_SCANCODE_PRINTSCREEN;
    case SAPP_KEYCODE_PAUSE:         return SDL_SCANCODE_PAUSE;
    case SAPP_KEYCODE_KP_DECIMAL:    return SDL_SCANCODE_KP_PERIOD;
    case SAPP_KEYCODE_KP_DIVIDE:     return SDL_SCANCODE_KP_DIVIDE;
    case SAPP_KEYCODE_KP_MULTIPLY:   return SDL_SCANCODE_KP_MULTIPLY;
    case SAPP_KEYCODE_KP_SUBTRACT:   return SDL_SCANCODE_KP_MINUS;
    case SAPP_KEYCODE_KP_ADD:        return SDL_SCANCODE_KP_PLUS;
    case SAPP_KEYCODE_KP_ENTER:      return SDL_SCANCODE_KP_ENTER;
    case SAPP_KEYCODE_LEFT_SHIFT:    return SDL_SCANCODE_LSHIFT;
    case SAPP_KEYCODE_LEFT_CONTROL:  return SDL_SCANCODE_LCTRL;
    case SAPP_KEYCODE_LEFT_ALT:      return SDL_SCANCODE_LALT;
    case SAPP_KEYCODE_LEFT_SUPER:    return SDL_SCANCODE_LGUI;
    case SAPP_KEYCODE_RIGHT_SHIFT:   return SDL_SCANCODE_RSHIFT;
    case SAPP_KEYCODE_RIGHT_CONTROL: return SDL_SCANCODE_RCTRL;
    case SAPP_KEYCODE_RIGHT_ALT:     return SDL_SCANCODE_RALT;
    case SAPP_KEYCODE_RIGHT_SUPER:   return SDL_SCANCODE_RGUI;
    case SAPP_KEYCODE_MENU:          return SDL_SCANCODE_APPLICATION;
    case SAPP_KEYCODE_ESCAPE:        return SDL_SCANCODE_ESCAPE;
    default:                         return SDL_SCANCODE_UNKNOWN;
    }
}

static void UpdateKeyboardStateFromSappEvent(const sapp_event* event)
{
    if (!event || ((event->type != SAPP_EVENTTYPE_KEY_DOWN) && (event->type != SAPP_EVENTTYPE_KEY_UP)))
    {
        return;
    }

    const SDL_Scancode scancode = ConvertSappKeycodeToSDLScancode(event->key_code);
    if (scancode != SDL_SCANCODE_UNKNOWN)
    {
        MU_MobileSetKeyState(scancode, event->type == SAPP_EVENTTYPE_KEY_DOWN);
    }
}

static void EncodeUtf32ToUtf8(uint32_t charCode, char out[SDL_TEXTINPUTEVENT_TEXT_SIZE])
{
    std::memset(out, 0, SDL_TEXTINPUTEVENT_TEXT_SIZE);
    if (charCode == 0 || charCode > 0x10FFFF)
    {
        return;
    }

    if (charCode <= 0x7F)
    {
        out[0] = static_cast<char>(charCode);
    }
    else if (charCode <= 0x7FF)
    {
        out[0] = static_cast<char>(0xC0 | ((charCode >> 6) & 0x1F));
        out[1] = static_cast<char>(0x80 | (charCode & 0x3F));
    }
    else if (charCode <= 0xFFFF)
    {
        out[0] = static_cast<char>(0xE0 | ((charCode >> 12) & 0x0F));
        out[1] = static_cast<char>(0x80 | ((charCode >> 6) & 0x3F));
        out[2] = static_cast<char>(0x80 | (charCode & 0x3F));
    }
    else
    {
        out[0] = static_cast<char>(0xF0 | ((charCode >> 18) & 0x07));
        out[1] = static_cast<char>(0x80 | ((charCode >> 12) & 0x3F));
        out[2] = static_cast<char>(0x80 | ((charCode >> 6) & 0x3F));
        out[3] = static_cast<char>(0x80 | (charCode & 0x3F));
    }
}

// =============================================================================
// Input mapping: SDL events 鑺掗垾鐘偓?game mouse/key globals
// Maps SDL touch/mouse events to the same globals that WndProc sets on Windows.
// =============================================================================

static wchar_t TranslateKeyToAsciiFallback(const SDL_KeyboardEvent& keyEvent)
{
    const SDL_Keycode key = keyEvent.keysym.sym;
    const SDL_Keymod modifiers = static_cast<SDL_Keymod>(keyEvent.keysym.mod);
    const bool shift = (modifiers & KMOD_SHIFT) != 0;
    const bool caps = (modifiers & KMOD_CAPS) != 0;

    if ((modifiers & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) != 0)
    {
        return 0;
    }

    if (key >= SDLK_a && key <= SDLK_z)
    {
        const wchar_t base = (shift ^ caps) ? L'A' : L'a';
        return static_cast<wchar_t>(base + (key - SDLK_a));
    }

    if (key >= SDLK_0 && key <= SDLK_9)
    {
        if (!shift)
        {
            return static_cast<wchar_t>(L'0' + (key - SDLK_0));
        }

        static const wchar_t shiftedDigits[] = L")!@#$%^&*(";
        return shiftedDigits[key - SDLK_0];
    }

    if (key >= SDLK_KP_0 && key <= SDLK_KP_9)
    {
        return static_cast<wchar_t>(L'0' + (key - SDLK_KP_0));
    }

    switch (key)
    {
    case SDLK_SPACE:      return L' ';
    case SDLK_MINUS:      return shift ? L'_' : L'-';
    case SDLK_EQUALS:     return shift ? L'+' : L'=';
    case SDLK_LEFTBRACKET:return shift ? L'{' : L'[';
    case SDLK_RIGHTBRACKET:return shift ? L'}' : L']';
    case SDLK_BACKSLASH:  return shift ? L'|' : L'\\';
    case SDLK_SEMICOLON:  return shift ? L':' : L';';
    case SDLK_QUOTE:      return shift ? L'"' : L'\'';
    case SDLK_COMMA:      return shift ? L'<' : L',';
    case SDLK_PERIOD:     return shift ? L'>' : L'.';
    case SDLK_SLASH:      return shift ? L'?' : L'/';
    case SDLK_BACKQUOTE:  return shift ? L'~' : L'`';
    case SDLK_KP_PERIOD:  return L'.';
    case SDLK_KP_DIVIDE:  return L'/';
    case SDLK_KP_MULTIPLY:return L'*';
    case SDLK_KP_MINUS:   return L'-';
    case SDLK_KP_PLUS:    return L'+';
    default:
        break;
    }

    return 0;
}

struct AndroidFocusedTextFallbackState
{
    Uint32 tick = 0;
    HWND handle = nullptr;
    char text[SDL_TEXTINPUTEVENT_TEXT_SIZE] = { 0 };
};

static AndroidFocusedTextFallbackState g_recentFocusedTextFallback;

static bool FocusedTextInputHasOption(int option)
{
    if (!AndroidHasFocusedTextInput())
    {
        return false;
    }

    const HWND focused = GetFocus();
    if (!focused)
    {
        return false;
    }

    CUITextInputBox* input = reinterpret_cast<CUITextInputBox*>(GetWindowLongW(focused, GWL_USERDATA));
    if (input == nullptr)
    {
        return false;
    }

    return input->CheckOption(option) == TRUE;
}

static void RememberFocusedTextFallback(HWND handle, wchar_t character)
{
    g_recentFocusedTextFallback.tick = SDL_GetTicks();
    g_recentFocusedTextFallback.handle = handle;
    EncodeUtf32ToUtf8(static_cast<uint32_t>(character), g_recentFocusedTextFallback.text);
}

static bool ShouldSuppressDuplicateFocusedTextInput(const char* textUtf8)
{
    if (textUtf8 == nullptr || textUtf8[0] == '\0')
    {
        return false;
    }

    if (!AndroidHasFocusedTextInput() || g_recentFocusedTextFallback.handle == nullptr)
    {
        return false;
    }

    if (GetFocus() != g_recentFocusedTextFallback.handle)
    {
        return false;
    }

    const Uint32 elapsed = SDL_GetTicks() - g_recentFocusedTextFallback.tick;
    if (elapsed > 250)
    {
        return false;
    }

    return std::strcmp(textUtf8, g_recentFocusedTextFallback.text) == 0;
}

static constexpr bool kLogInputEvents = false;

bool IsAggressiveMobilePerfModeEnabled()
{
#if defined(__ANDROID__) || defined(MU_IOS)
    return g_adaptivePerf.isEmulator;
#else
    return false;
#endif
}

// ── Render scale ───────────────────────────────────────────────────────────
// Renders the game at a fraction of the physical resolution and upscales the
// result to fill the display, trading sharpness for fill-rate.
//
// This works by telling the ENGINE its screen is the smaller size: WindowWidth/
// WindowHeight feed every glViewport2() call, the UI scale factor
// (g_fScreenRate_x = WindowWidth/640) and the projection, so scaling them keeps
// all of that self-consistent. RenderBackend then renders into an FBO of that
// size and blits it up to the physical surface.
//
// Do NOT instead leave the engine at native size and just shrink the render
// target - the engine resets the viewport to WindowWidth/WindowHeight itself
// all over the place (glViewport2, ZzzOpenglUtil.cpp), so a native-sized
// viewport in a smaller buffer renders zoomed and misaligns touch input.
//
// Touch is unaffected: sokol reports touch in PHYSICAL pixels, so the event
// handler normalises against g_NativePresentWidth/Height (below) rather than
// the scaled drawable size.
//
// 1.0 = native (feature off). 0.75 renders ~44% fewer pixels.
static float g_RenderScaleX = 0.75f;
static float g_RenderScaleY = 0.75f;

// TEMP profiling: worst frame in a rolling window, with its bucket breakdown
// latched, so a single screenshot can show what a hitch consisted of.
double g_ProfWorstSceneMs = 0.0;
unsigned long long g_ProfWorstObjTicks = 0;
unsigned long long g_ProfWorstCharTicks = 0;
unsigned long long g_ProfWorstUiTicks = 0;
unsigned long long g_ProfWorstParticleTicks = 0;
unsigned long long g_ProfWorstTerrainTicks = 0;
unsigned long long g_ProfWorstObjMoveTicks = 0;
int g_ProfHitchCount = 0;
int g_ProfHitchFrames = 0;
double g_ProfHitchSceneMsSum = 0.0;
int g_ProfWorstTexDefines = 0;
unsigned long long g_ProfWorstTexDefineTicks = 0;
int g_ProfWorstObjRendered = 0;
int g_ProfWorstObjCandidates = 0;
int g_ProfWorstVisualCalls = 0;
int g_ProfWorstCharRendered = 0;
int g_ProfWorstDrawCalls = 0;
int g_ProfVisualCallsNow = 0;

// TEMP: on-screen overlay switch, and the file-based drift log that replaces
// it. The overlay draws ~8 long strings a frame that change every frame, so it
// can never be cached and measured ~4-5ms/frame on Mali - it distorts exactly
// what it is measuring. mu_drift_log.txt lets a long run be measured clean.
bool g_ShowPerfOverlay = false;

// Plain FPS readout only - the shipping-friendly option. One short string a
// frame instead of the eight long profiling lines.
bool g_ShowFpsOnly = true;
int g_DriftFrames = 0;
double g_DriftSceneMsSum = 0.0;
double g_DriftWorstMs = 0.0;
int g_DriftHitches = 0;
double g_DriftStartSec = 0.0;
double g_DriftLastLogSec = 0.0;
double g_DriftTextExtentMs = 0.0;
double g_DriftTextOutMs = 0.0;
double g_DriftTextUpMs = 0.0;
int g_DriftTextCalls = 0;
int g_DriftTextHits = 0;
int g_DriftTextMisses = 0;

extern unsigned long long g_ProfTextExtentTicks;
extern unsigned long long g_ProfTextOutTicks;
extern unsigned long long g_ProfTextWriteTicks;
extern unsigned long long g_ProfTextUploadTicks;
extern int g_ProfTextCalls;
extern int g_ProfTextCacheHits;
extern int g_ProfTextCacheMisses;
extern int g_ProfTextSlotCollisions;

// Defined in Platform/gl_compat.cpp - texture definitions (asset loads) this
// frame, and the time spent in them.
extern int g_ProfTexDefineCount;
extern unsigned long long g_ProfTexDefineTicks;

// TEMP A/B for the Mali/MediaTek stall investigation - see the call site.
// Tested true on MT6878/Mali-G615: catastrophically worse, not better. The 2D
// quad path went from 0.2ms to 14-56ms per frame and the frame from ~35ms to
// 112-278ms, because glBufferSubData then overwrites a buffer the GPU is still
// reading and the driver stalls. Per-draw orphaning is correct on Mali too.
static bool g_ForceSkipVBOOrphan = false;

// Physical surface size, as reported by sokol - the blit target, and the basis
// for normalising touch coordinates.
static int g_NativePresentWidth = 0;
static int g_NativePresentHeight = 0;

// TEMP profiling: last frame's top-level phase split, read by the FPS overlay.
unsigned long long g_ProfSceneTicks = 0;
unsigned long long g_ProfPadTicks = 0;
unsigned long long g_ProfPresentTicks = 0;

// TEMP profiling: Scene() sub-phase breakdown, taken from the engine's own
// existing per-frame snapshots (which are otherwise only ever PERF_LOGI'd, and
// logcat is unavailable on retail "user" builds). Scene() is ~97ms of a ~111ms
// frame with Present() at 0ms, so the cost is CPU-side inside here somewhere.
unsigned long long g_ProfObjMoveTicks = 0;
unsigned long long g_ProfObjRenderTicks = 0;
unsigned long long g_ProfCharMoveTicks = 0;
unsigned long long g_ProfCharRenderTicks = 0;
unsigned long long g_ProfTerrainTicks = 0;
unsigned long long g_ProfEffectsTicks = 0;
unsigned long long g_ProfParticlesTicks = 0;
unsigned long long g_ProfUiTicks = 0;

// The four buckets above plus objR/chrR/ter only account for ~32ms of a ~51ms
// Scene(). These are the rest of the MainScenePerfSnapshot, so that line 4 of
// the overlay closes the ~19ms that was previously unattributed.
// Frustum/distance culling already runs at three levels in RenderObjects (per
// terrain block, per object, plus adaptive distance buckets). These surface how
// much it actually rejects, so "render only what the camera sees" can be
// checked rather than assumed.
int g_ProfObjCandidates = 0;
int g_ProfObjRendered = 0;
int g_ProfObjCulled = 0;
int g_ProfCharCandidates = 0;
int g_ProfCharRendered = 0;

unsigned long long g_ProfShadowTicks = 0;
unsigned long long g_ProfBoidsTicks = 0;
unsigned long long g_ProfMiscWorldTicks = 0;
unsigned long long g_ProfJointsTicks = 0;
unsigned long long g_ProfBlursTicks = 0;
unsigned long long g_ProfSpritesTicks = 0;
unsigned long long g_ProfPointsTicks = 0;
unsigned long long g_ProfAfterEffectsTicks = 0;

// Character render is ~50ms of a ~110ms frame and is NOT the skinning math
// (0.8ms) - an A/B with GPU skinning on vs off left it unchanged at ~52ms.
// These are its sub-phases, to find what actually costs that.
unsigned long long g_ProfCharShadowTicks = 0;
unsigned long long g_ProfCharMonsterObjTicks = 0;
unsigned long long g_ProfCharAttachTicks = 0;
unsigned long long g_ProfCharPostTicks = 0;

// TEMP profiling: draw-path split for the frame (see the note where these are
// assigned). High imDrawCalls means work is going through the slow per-vertex
// immediate-mode emulation rather than the batched VBO path.
int g_ProfDrawCalls = 0;
int g_ProfImDrawCalls = 0;
int g_ProfVaConvertedDrawCalls = 0;
int g_ProfVaDirectDrawCalls = 0;
int g_ProfQuadIndexedDrawCalls = 0;
int g_ProfQuadExpandedDrawCalls = 0;
int g_ProfVerts = 0;

// TEMP profiling: batch flush causes for the frame. Immediate-mode batching
// already merges consecutive spans and survives modelview changes, so what is
// left is state churn - this says which state.
int g_ProfFlushCauses[12] = { 0 };
int g_ProfDrawSites[10] = { 0 };

static void ApplyAndroidDrawableSize(int screenW, int screenH, const char* reason)
{
    if ((screenW <= 1) || (screenH <= 1))
    {
        return;
    }

    const bool changed =
        (screenW != g_DrawableWidth) ||
        (screenH != g_DrawableHeight) ||
        (static_cast<int>(WindowWidth) != screenW) ||
        (static_cast<int>(WindowHeight) != screenH);
    if (!changed)
    {
        return;
    }

    g_DrawableWidth = screenW;
    g_DrawableHeight = screenH;
    UpdateAndroidScreenMetrics(screenW, screenH);

    if (g_RenderBackend)
    {
        g_RenderBackend->OnDrawableSizeChanged(screenW, screenH);
    }
    else
    {
        glViewport(0, 0, screenW, screenH);
    }

    if (g_hWnd)
    {
        CInput::Instance().Create(g_hWnd, static_cast<long>(WindowWidth), static_cast<long>(WindowHeight));
    }

    LOGI(
        "Drawable sync (%s) -> %dx%d",
        (reason && reason[0]) ? reason : "unknown",
        screenW,
        screenH);
}

static void SyncAndroidDrawableSizeFromSokol(const char* reason)
{
    const int screenW = sapp_width();
    const int screenH = sapp_height();
    if ((screenW <= 1) || (screenH <= 1))
    {
        return;
    }

    // Physical size drives the upscale blit and touch normalisation.
    g_NativePresentWidth = screenW;
    g_NativePresentHeight = screenH;
    RenderBackend_SetNativePresentSize(screenW, screenH);

    // The engine is told the SCALED size - see the g_RenderScale* comment above.
    int renderW = static_cast<int>(screenW * g_RenderScaleX);
    int renderH = static_cast<int>(screenH * g_RenderScaleY);
    if (renderW < 1) renderW = 1;
    if (renderH < 1) renderH = 1;

    ApplyAndroidDrawableSize(renderW, renderH, reason);
}

static void HandleSDLEvent(const SDL_Event& ev, int& screenW, int& screenH)
{
    switch (ev.type)
    {
    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 App lifecycle 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    case SDL_QUIT:
        Destroy = true;
        break;

    case SDL_APP_WILLENTERBACKGROUND:
    case SDL_APP_DIDENTERBACKGROUND:
        g_bWndActive = false;
        g_primaryTouchFinger = -1;
        MU_MobileClearKeyboardState();
        ClearVirtualJoystick();
        ClearAndroidLongPressRightClick(false);
        // Release held buttons when minimized
        MouseLButton = MouseRButton = MouseMButton = false;
        MouseLButtonPop = MouseRButtonPop = MouseMButtonPop = false;
        MouseLButtonPush = MouseRButtonPush = MouseMButtonPush = false;
        MouseWheel = 0;
        break;

    case SDL_APP_WILLENTERFOREGROUND:
    case SDL_APP_DIDENTERFOREGROUND:
        g_bWndActive = true;
        break;

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Mouse motion (SDL2 maps single-touch 鑺掗垾鐘偓?mouse on Android) 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    case SDL_MOUSEMOTION:
    {
        if (ev.motion.which == SDL_TOUCH_MOUSEID && g_seenFingerInput) break;
        if (g_joystickPcMouseCaptured)
        {
            const float uiX = std::clamp(static_cast<float>(ev.motion.x) * 640.0f / static_cast<float>(screenW > 0 ? screenW : 1), 0.0f, 640.0f);
            const float uiY = std::clamp(static_cast<float>(ev.motion.y) * 480.0f / static_cast<float>(screenH > 0 ? screenH : 1), 0.0f, 480.0f);
            if (ev.motion.state & SDL_BUTTON_LMASK)
                UpdateVirtualJoystickByUi(uiX, uiY);
            else
            { g_joystickPcMouseCaptured = false; ClearVirtualJoystick(); }
            break;
        }
        UpdateMouseFromPixel(ev.motion.x, ev.motion.y, screenW, screenH);
        break;
    }

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Mouse buttons 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    case SDL_MOUSEBUTTONDOWN:
        if (ev.button.which == SDL_TOUCH_MOUSEID && g_seenFingerInput) break;
        g_iNoMouseTime = 0;
        UpdateMouseFromPixel(ev.button.x, ev.button.y, screenW, screenH);
        if (ev.button.button == SDL_BUTTON_LEFT) {
            AndroidHideKeyboardForOutsideTap(static_cast<float>(MouseX), static_cast<float>(MouseY));
            if (IsVirtualPadAvailable() && HitTestVirtualJoystick(static_cast<float>(MouseX), static_cast<float>(MouseY)))
            {
                g_joystickPcMouseCaptured = true;
                StartVirtualJoystick(kPcMouseJoystickFingerId, static_cast<float>(MouseX), static_cast<float>(MouseY));
            }
            else
            {
                g_joystickPcMouseCaptured = false;
                MouseLButtonPop  = false;
                MouseLButtonPush = !MouseLButton;
                MouseLButton     = true;
            }
        } else if (ev.button.button == SDL_BUTTON_RIGHT) {
            MouseRButtonPop  = false;
            MouseRButtonPush = !MouseRButton;
            MouseRButton     = true;
        } else if (ev.button.button == SDL_BUTTON_MIDDLE) {
            MouseMButtonPop  = false;
            MouseMButtonPush = !MouseMButton;
            MouseMButton     = true;
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if (ev.button.which == SDL_TOUCH_MOUSEID && g_seenFingerInput) break;
        g_iNoMouseTime = 0;
        UpdateMouseFromPixel(ev.button.x, ev.button.y, screenW, screenH);
        if (ev.button.button == SDL_BUTTON_LEFT) {
            if (g_joystickPcMouseCaptured)
            { g_joystickPcMouseCaptured = false; ClearVirtualJoystick(); }
            else
            {
                MouseLButtonPush = false;
                if (MouseLButton) { MouseLButtonPop = true; g_iMousePopPosition_x = MouseX; g_iMousePopPosition_y = MouseY; }
                MouseLButton = false;
            }
        } else if (ev.button.button == SDL_BUTTON_RIGHT) {
            MouseRButtonPush = false;
            if (MouseRButton) MouseRButtonPop = true;
            MouseRButton = false;
        } else if (ev.button.button == SDL_BUTTON_MIDDLE) {
            MouseMButtonPush = false;
            if (MouseMButton) MouseMButtonPop = true;
            MouseMButton = false;
        }
        break;

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Mouse wheel 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    case SDL_MOUSEWHEEL:
        MouseWheel = ev.wheel.y;
        break;

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Multi-touch (additional fingers) 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    // SDL2 maps first finger to mouse; additional fingers need manual mapping
    // For now: second finger 鑺掗垾鐘偓?right-click
    case SDL_FINGERDOWN:
        g_seenFingerInput = true;
        if (HandleVirtualPickerFingerDown(ev.tfinger))
        {
            break;
        }
        if (HandleVirtualFingerDown(ev.tfinger))
        {
            break;
        }
        g_iNoMouseTime = 0;
        UpdateMouseFromTouch(ev.tfinger, screenW, screenH);

        // Dropped items open a menu rather than being interacted with directly,
        // so touching an item never makes the character walk. The menu itself
        // gets first refusal on the tap while it is open.
        if (SceneFlag == MAIN_SCENE)
        {
            if (HandleItemMenuTap(static_cast<float>(MouseX), static_cast<float>(MouseY)))
            {
                break;
            }

            // The menu opens by itself when a drop is near the character, so a
            // tap on the world no longer needs to hunt for items.
        }
#if defined(__ANDROID__) || defined(MU_IOS)
        if (kLogInputEvents)
        {
            LOGI("INPUT finger down id=%lld x=%.3f y=%.3f mapped=%d,%d primaryTouch=%lld",
                static_cast<long long>(ev.tfinger.fingerId),
                ev.tfinger.x,
                ev.tfinger.y,
                MouseX,
                MouseY,
                static_cast<long long>(g_primaryTouchFinger));
        }
#endif
        if (g_primaryTouchFinger == -1 || ev.tfinger.fingerId == g_primaryTouchFinger) {
            g_primaryTouchFinger = ev.tfinger.fingerId;
            StartAndroidLongPressRightClick(ev.tfinger);

            // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Double-tap detection (Android replacement for WM_LBUTTONDBLCLK) 鑺掗垾婵冨亾鑺掗垾婵冨亾
            {
                const uint32_t nowMs = MU_MobileGetTicks();
                const float dx = ev.tfinger.x - s_doubleTapLastUpNX;
                const float dy = ev.tfinger.y - s_doubleTapLastUpNY;
                if (s_doubleTapLastUpMs > 0
                    && (nowMs - s_doubleTapLastUpMs) <= kDoubleTapMaxMs
                    && (dx * dx + dy * dy) <= (kDoubleTapMaxDist * kDoubleTapMaxDist))
                {
                    MouseLButtonDBClick = true;
                    s_doubleTapLastUpMs = 0; // consume: don't triple-fire
                }
            }

            MouseLButtonPop  = false;
            MouseLButtonPush = !MouseLButton;
            MouseLButton     = true;
        } else {
            MouseRButtonPop  = false;
            MouseRButtonPush = !MouseRButton;
            MouseRButton     = true;
        }
        break;

    case SDL_FINGERMOTION:
        g_seenFingerInput = true;
        if (HandleVirtualPickerFingerMotion(ev.tfinger))
        {
            break;
        }
        if (HandleVirtualFingerMotion(ev.tfinger))
        {
            break;
        }
        UpdateAndroidLongPressRightClickMotion(ev.tfinger);
        UpdateMouseFromTouch(ev.tfinger, screenW, screenH);
        break;

    case SDL_FINGERUP:
        g_seenFingerInput = true;
        if (HandleVirtualPickerFingerUp(ev.tfinger))
        {
            break;
        }
        if (HandleVirtualFingerUp(ev.tfinger))
        {
            break;
        }
        g_iNoMouseTime = 0;
        UpdateMouseFromTouch(ev.tfinger, screenW, screenH);
#if defined(__ANDROID__) || defined(MU_IOS)
        if (kLogInputEvents)
        {
            LOGI("INPUT finger up id=%lld x=%.3f y=%.3f mapped=%d,%d primaryTouch=%lld",
                static_cast<long long>(ev.tfinger.fingerId),
                ev.tfinger.x,
                ev.tfinger.y,
                MouseX,
                MouseY,
                static_cast<long long>(g_primaryTouchFinger));
        }
#endif
        if (ev.tfinger.fingerId == g_primaryTouchFinger) {
            const bool suppressLeftPop = FinishAndroidLongPressRightClickFingerUp(ev.tfinger.fingerId);
            MouseLButtonPush = false;
            if (MouseLButton && !suppressLeftPop) {
                MouseLButtonPop = true;
                g_iMousePopPosition_x = MouseX;
                g_iMousePopPosition_y = MouseY;
            }
            MouseLButton = false;
            g_primaryTouchFinger = -1;

            // Record this finger-up so the next finger-down can detect double-tap
            s_doubleTapLastUpMs = MU_MobileGetTicks();
            s_doubleTapLastUpNX = ev.tfinger.x;
            s_doubleTapLastUpNY = ev.tfinger.y;
        } else {
            MouseRButtonPush = false;
            if (MouseRButton) MouseRButtonPop = true;
            MouseRButton = false;
        }
        break;

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Keyboard 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    case SDL_TEXTINPUT:
#if !defined(MU_ANDROID_DISABLE_LOG)
        LOGI(
            "CHATIME SDL_TEXTINPUT text='%s' hasFocusedText=%d",
            ev.text.text,
            AndroidHasFocusedTextInput() ? 1 : 0);
#endif
        if (ShouldSuppressDuplicateFocusedTextInput(ev.text.text))
        {
            break;
        }
        if (g_charNameInputActive) {
            // Route directly into the custom char-name buffer
            const unsigned char* p = reinterpret_cast<const unsigned char*>(ev.text.text);
            while (*p && g_charNameLen < 10) {
                wchar_t ch = 0;
                if ((*p & 0x80u) == 0) {
                    ch = static_cast<wchar_t>(*p++);
                } else if ((*p & 0xE0u) == 0xC0u && p[1]) {
                    ch = static_cast<wchar_t>(((*p & 0x1Fu) << 6) | (p[1] & 0x3Fu));
                    p += 2;
                } else if ((*p & 0xF0u) == 0xE0u && p[1] && p[2]) {
                    ch = static_cast<wchar_t>(((*p & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu));
                    p += 3;
                } else { p++; continue; }
                if (ch >= 0x20) {
                    g_charNameBuf[g_charNameLen++] = ch;
                    g_charNameBuf[g_charNameLen]   = L'\0';
                }
            }
        } else if (AndroidHasFocusedTextInput()) {
            if (std::strchr(ev.text.text, '\n') != nullptr
                || std::strchr(ev.text.text, '\r') != nullptr)
            {
                if (g_pendingImeEnterTextInput)
                {
                    g_pendingImeEnterTextInput = false;
                    break;
                }

                const HWND focusedBeforeTextInput = GetFocus();
                AndroidInjectUtf8ToFocusedTextInput(ev.text.text);
                if (GetFocus() == focusedBeforeTextInput) {
                    SetEnterPressed(true);
                }
            } else {
                g_pendingImeEnterTextInput = false;
                AndroidInjectUtf8ToFocusedTextInput(ev.text.text);
            }
        }
        break;

    case SDL_KEYDOWN:
        if (ev.key.keysym.scancode != SDL_SCANCODE_UNKNOWN)
        {
            MU_MobileSetKeyState(ev.key.keysym.scancode, true);
        }
#if !defined(MU_ANDROID_DISABLE_LOG)
        LOGI(
            "CHATIME SDL_KEYDOWN sym=%d repeat=%d hasFocusedText=%d sdlTextInput=%d",
            static_cast<int>(ev.key.keysym.sym),
            ev.key.repeat,
            AndroidHasFocusedTextInput() ? 1 : 0,
            MU_MobileIsTextInputActive() ? 1 : 0);
#endif
        switch (ev.key.keysym.sym) {
        case SDLK_AC_BACK:   // Android back button
            Destroy = true;
            break;
        case SDLK_BACKSPACE:
            if (g_charNameInputActive) {
                if (g_charNameLen > 0) g_charNameBuf[--g_charNameLen] = L'\0';
            } else if (AndroidHasFocusedTextInput()) {
                AndroidInjectCharToFocusedTextInput(VK_BACK);
            }
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (g_charNameInputActive) {
                // Let the game loop detect Enter via CInput::IsKeyDown(VK_RETURN)
            } else if (AndroidHasFocusedTextInput()) {
                const HWND focusedBeforeReturn = GetFocus();
                if (AndroidInjectCharToFocusedTextInput(VK_RETURN)) {
                    g_pendingImeEnterTextInput = true;
                }
                if (GetFocus() == focusedBeforeReturn) {
                    SetEnterPressed(true);
                }
            } else {
                g_pendingImeEnterTextInput = false;
                SetEnterPressed(true);
            }
            break;
        default:
            if (AndroidHasFocusedTextInput()
                && ev.key.repeat == 0
                && FocusedTextInputHasOption(UIOPTION_NUMBERONLY))
            {
                const wchar_t fallbackCharacter = TranslateKeyToAsciiFallback(ev.key);
                if (fallbackCharacter != 0)
                {
                    const HWND focusedBeforeFallback = GetFocus();
                    if (AndroidInjectCharToFocusedTextInput(fallbackCharacter))
                    {
                        RememberFocusedTextFallback(focusedBeforeFallback, fallbackCharacter);
                    }
                    break;
                }
            }

            // LDPlayer sends SDLK_UNKNOWN (keycode=0) with no SDL_TEXTINPUT.
            // Real devices send valid keycodes AND also fire SDL_TEXTINPUT.
            // Only use TranslateKeyToAsciiFallback for LDPlayer (SDLK_UNKNOWN),
            // otherwise SDL_TEXTINPUT will handle it and we'd double-inject.
            if (ev.key.keysym.sym == SDLK_UNKNOWN && ev.key.repeat == 0) {
                if (g_charNameInputActive) {
                    const wchar_t fallbackCharacter = TranslateKeyToAsciiFallback(ev.key);
                    if (fallbackCharacter != 0 && g_charNameLen < 10) {
                        g_charNameBuf[g_charNameLen++] = fallbackCharacter;
                        g_charNameBuf[g_charNameLen]   = L'\0';
                    }
                } else if (AndroidHasFocusedTextInput()) {
                    const wchar_t fallbackCharacter = TranslateKeyToAsciiFallback(ev.key);
                    if (fallbackCharacter != 0)
                        AndroidInjectCharToFocusedTextInput(fallbackCharacter);
                }
            }
            break;
        }
        break;

    case SDL_KEYUP:
        if (ev.key.keysym.scancode != SDL_SCANCODE_UNKNOWN)
        {
            MU_MobileSetKeyState(ev.key.keysym.scancode, false);
        }
        break;

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Window resize 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    case SDL_WINDOWEVENT:
        if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            screenW = (ev.window.data1 > 0) ? ev.window.data1 : g_DrawableWidth;
            screenH = (ev.window.data2 > 0) ? ev.window.data2 : g_DrawableHeight;
            ApplyAndroidDrawableSize(screenW, screenH, "event");
        }
        break;

    default:
        break;
    }
}

namespace
{
struct AndroidFrameState
{
    Uint64 perfFrequency = 0;
    Uint64 perfWindowStart = 0;
    int perfFrames = 0;
    int drawCallsTotal = 0;
    int vertsTotal = 0;
    int detailLogWindowCounter = 0;
    int objRenderCandidatesTotal = 0;
    int objRenderRenderedTotal = 0;
    int objRenderCulledTotal = 0;
    int objRenderBaseCallsTotal = 0;
    int objVisualCallsTotal = 0;
    int objRenderAfterCallsTotal = 0;
    int charRenderCandidatesTotal = 0;
    int charRenderRenderedTotal = 0;
    int charRenderDeferredTotal = 0;
    int charRenderPlayersTotal = 0;
    int charRenderMonstersTotal = 0;
    int terrainNormalBlocksTotal = 0;
    int terrainNormalTilesTotal = 0;
    int terrainGrassBlocksTotal = 0;
    int terrainGrassTilesTotal = 0;
    int terrainAfterBlocksTotal = 0;
    int terrainAfterTilesTotal = 0;
    Uint64 sceneTicksTotal = 0;
    Uint64 padTicksTotal = 0;
    Uint64 presentTicksTotal = 0;
    Uint64 objMoveTicksTotal = 0;
    Uint64 objRenderTicksTotal = 0;
    Uint64 terrainRenderTicksTotal = 0;
    Uint64 terrainAfterTicksTotal = 0;
    Uint64 charMoveTicksTotal = 0;
    Uint64 charRenderTicksTotal = 0;
    Uint64 mainShadowTicksTotal = 0;
    Uint64 mainBoidsTicksTotal = 0;
    Uint64 mainMiscWorldTicksTotal = 0;
    Uint64 mainJointsTicksTotal = 0;
    Uint64 mainEffectsTicksTotal = 0;
    Uint64 mainBlursTicksTotal = 0;
    Uint64 mainSpritesTicksTotal = 0;
    Uint64 mainParticlesTicksTotal = 0;
    Uint64 mainPointsTicksTotal = 0;
    Uint64 mainAfterEffectsTicksTotal = 0;
    Uint64 mainUiTicksTotal = 0;
    int dbgFrameCount = 0;
    uint32_t muHelperLastTickMs = 0;
    uint32_t hackLastTickMs = 0;
};

std::mutex g_PendingSdlEventsMutex;
std::vector<SDL_Event> g_PendingSdlEvents;
AndroidFrameState g_AndroidFrameState = {};

constexpr jint kAndroidKeyActionDown = 0;
constexpr jint kAndroidKeyActionUp = 1;
constexpr jint kAndroidKeyActionMultiple = 2;
} // namespace

static void QueueSyntheticSDLEvent(const SDL_Event& event)
{
    std::lock_guard<std::mutex> lock(g_PendingSdlEventsMutex);
    g_PendingSdlEvents.push_back(event);
}

static std::vector<SDL_Event> DrainSyntheticSDLEvents()
{
    std::vector<SDL_Event> events;
    std::lock_guard<std::mutex> lock(g_PendingSdlEventsMutex);
    events.swap(g_PendingSdlEvents);
    return events;
}

static SDL_Keymod ConvertAndroidMetaStateToSDLKeymod(jint metaState)
{
    SDL_Keymod modifiers = KMOD_NONE;
    if ((metaState & AMETA_SHIFT_ON) != 0) modifiers = static_cast<SDL_Keymod>(modifiers | KMOD_SHIFT);
    if ((metaState & AMETA_CTRL_ON) != 0) modifiers = static_cast<SDL_Keymod>(modifiers | KMOD_CTRL);
    if ((metaState & AMETA_ALT_ON) != 0) modifiers = static_cast<SDL_Keymod>(modifiers | KMOD_ALT);
    if ((metaState & AMETA_META_ON) != 0) modifiers = static_cast<SDL_Keymod>(modifiers | KMOD_GUI);
    if ((metaState & AMETA_CAPS_LOCK_ON) != 0) modifiers = static_cast<SDL_Keymod>(modifiers | KMOD_CAPS);
    return modifiers;
}

static SDL_Keycode ConvertAndroidKeycodeToSDLKeycode(jint keyCode)
{
    switch (keyCode)
    {
    case AKEYCODE_A: return SDLK_a;
    case AKEYCODE_B: return SDLK_b;
    case AKEYCODE_C: return SDLK_c;
    case AKEYCODE_D: return SDLK_d;
    case AKEYCODE_E: return SDLK_e;
    case AKEYCODE_F: return SDLK_f;
    case AKEYCODE_G: return SDLK_g;
    case AKEYCODE_H: return SDLK_h;
    case AKEYCODE_I: return SDLK_i;
    case AKEYCODE_J: return SDLK_j;
    case AKEYCODE_K: return SDLK_k;
    case AKEYCODE_L: return SDLK_l;
    case AKEYCODE_M: return SDLK_m;
    case AKEYCODE_N: return SDLK_n;
    case AKEYCODE_O: return SDLK_o;
    case AKEYCODE_P: return SDLK_p;
    case AKEYCODE_Q: return SDLK_q;
    case AKEYCODE_R: return SDLK_r;
    case AKEYCODE_S: return SDLK_s;
    case AKEYCODE_T: return SDLK_t;
    case AKEYCODE_U: return SDLK_u;
    case AKEYCODE_V: return SDLK_v;
    case AKEYCODE_W: return SDLK_w;
    case AKEYCODE_X: return SDLK_x;
    case AKEYCODE_Y: return SDLK_y;
    case AKEYCODE_Z: return SDLK_z;
    case AKEYCODE_0: return SDLK_0;
    case AKEYCODE_1: return SDLK_1;
    case AKEYCODE_2: return SDLK_2;
    case AKEYCODE_3: return SDLK_3;
    case AKEYCODE_4: return SDLK_4;
    case AKEYCODE_5: return SDLK_5;
    case AKEYCODE_6: return SDLK_6;
    case AKEYCODE_7: return SDLK_7;
    case AKEYCODE_8: return SDLK_8;
    case AKEYCODE_9: return SDLK_9;
    case AKEYCODE_SPACE: return SDLK_SPACE;
    case AKEYCODE_TAB: return SDLK_TAB;
    case AKEYCODE_ENTER: return SDLK_RETURN;
    case AKEYCODE_NUMPAD_ENTER: return SDLK_KP_ENTER;
    case AKEYCODE_DEL: return SDLK_BACKSPACE;
    case AKEYCODE_ESCAPE: return SDLK_ESCAPE;
    case AKEYCODE_BACK: return SDLK_AC_BACK;
    case AKEYCODE_COMMA: return SDLK_COMMA;
    case AKEYCODE_PERIOD: return SDLK_PERIOD;
    case AKEYCODE_MINUS: return SDLK_MINUS;
    case AKEYCODE_EQUALS: return SDLK_EQUALS;
    case AKEYCODE_SEMICOLON: return SDLK_SEMICOLON;
    case AKEYCODE_APOSTROPHE: return SDLK_QUOTE;
    case AKEYCODE_SLASH: return SDLK_SLASH;
    case AKEYCODE_BACKSLASH: return SDLK_BACKSLASH;
    case AKEYCODE_LEFT_BRACKET: return SDLK_LEFTBRACKET;
    case AKEYCODE_RIGHT_BRACKET: return SDLK_RIGHTBRACKET;
    case AKEYCODE_GRAVE: return SDLK_BACKQUOTE;
    case AKEYCODE_DPAD_LEFT: return SDLK_LEFT;
    case AKEYCODE_DPAD_RIGHT: return SDLK_RIGHT;
    case AKEYCODE_DPAD_UP: return SDLK_UP;
    case AKEYCODE_DPAD_DOWN: return SDLK_DOWN;
    case AKEYCODE_PAGE_UP: return SDLK_PAGEUP;
    case AKEYCODE_PAGE_DOWN: return SDLK_PAGEDOWN;
    case AKEYCODE_MOVE_HOME: return SDLK_HOME;
    case AKEYCODE_MOVE_END: return SDLK_END;
    case AKEYCODE_INSERT: return SDLK_INSERT;
    case AKEYCODE_FORWARD_DEL: return SDLK_DELETE;
    case AKEYCODE_SHIFT_LEFT: return SDLK_LSHIFT;
    case AKEYCODE_SHIFT_RIGHT: return SDLK_RSHIFT;
    case AKEYCODE_CTRL_LEFT: return SDLK_LCTRL;
    case AKEYCODE_CTRL_RIGHT: return SDLK_RCTRL;
    case AKEYCODE_ALT_LEFT: return SDLK_LALT;
    case AKEYCODE_ALT_RIGHT: return SDLK_RALT;
    default: return SDLK_UNKNOWN;
    }
}

static SDL_Scancode ConvertAndroidKeycodeToSDLScancode(jint keyCode)
{
    switch (keyCode)
    {
    case AKEYCODE_A: return SDL_SCANCODE_A;
    case AKEYCODE_B: return SDL_SCANCODE_B;
    case AKEYCODE_C: return SDL_SCANCODE_C;
    case AKEYCODE_D: return SDL_SCANCODE_D;
    case AKEYCODE_E: return SDL_SCANCODE_E;
    case AKEYCODE_F: return SDL_SCANCODE_F;
    case AKEYCODE_G: return SDL_SCANCODE_G;
    case AKEYCODE_H: return SDL_SCANCODE_H;
    case AKEYCODE_I: return SDL_SCANCODE_I;
    case AKEYCODE_J: return SDL_SCANCODE_J;
    case AKEYCODE_K: return SDL_SCANCODE_K;
    case AKEYCODE_L: return SDL_SCANCODE_L;
    case AKEYCODE_M: return SDL_SCANCODE_M;
    case AKEYCODE_N: return SDL_SCANCODE_N;
    case AKEYCODE_O: return SDL_SCANCODE_O;
    case AKEYCODE_P: return SDL_SCANCODE_P;
    case AKEYCODE_Q: return SDL_SCANCODE_Q;
    case AKEYCODE_R: return SDL_SCANCODE_R;
    case AKEYCODE_S: return SDL_SCANCODE_S;
    case AKEYCODE_T: return SDL_SCANCODE_T;
    case AKEYCODE_U: return SDL_SCANCODE_U;
    case AKEYCODE_V: return SDL_SCANCODE_V;
    case AKEYCODE_W: return SDL_SCANCODE_W;
    case AKEYCODE_X: return SDL_SCANCODE_X;
    case AKEYCODE_Y: return SDL_SCANCODE_Y;
    case AKEYCODE_Z: return SDL_SCANCODE_Z;
    case AKEYCODE_0: return SDL_SCANCODE_0;
    case AKEYCODE_1: return SDL_SCANCODE_1;
    case AKEYCODE_2: return SDL_SCANCODE_2;
    case AKEYCODE_3: return SDL_SCANCODE_3;
    case AKEYCODE_4: return SDL_SCANCODE_4;
    case AKEYCODE_5: return SDL_SCANCODE_5;
    case AKEYCODE_6: return SDL_SCANCODE_6;
    case AKEYCODE_7: return SDL_SCANCODE_7;
    case AKEYCODE_8: return SDL_SCANCODE_8;
    case AKEYCODE_9: return SDL_SCANCODE_9;
    case AKEYCODE_SPACE: return SDL_SCANCODE_SPACE;
    case AKEYCODE_TAB: return SDL_SCANCODE_TAB;
    case AKEYCODE_ENTER: return SDL_SCANCODE_RETURN;
    case AKEYCODE_NUMPAD_ENTER: return SDL_SCANCODE_KP_ENTER;
    case AKEYCODE_DEL: return SDL_SCANCODE_BACKSPACE;
    case AKEYCODE_ESCAPE: return SDL_SCANCODE_ESCAPE;
    case AKEYCODE_BACK: return SDL_SCANCODE_AC_BACK;
    case AKEYCODE_COMMA: return SDL_SCANCODE_COMMA;
    case AKEYCODE_PERIOD: return SDL_SCANCODE_PERIOD;
    case AKEYCODE_MINUS: return SDL_SCANCODE_MINUS;
    case AKEYCODE_EQUALS: return SDL_SCANCODE_EQUALS;
    case AKEYCODE_SEMICOLON: return SDL_SCANCODE_SEMICOLON;
    case AKEYCODE_APOSTROPHE: return SDL_SCANCODE_APOSTROPHE;
    case AKEYCODE_SLASH: return SDL_SCANCODE_SLASH;
    case AKEYCODE_BACKSLASH: return SDL_SCANCODE_BACKSLASH;
    case AKEYCODE_LEFT_BRACKET: return SDL_SCANCODE_LEFTBRACKET;
    case AKEYCODE_RIGHT_BRACKET: return SDL_SCANCODE_RIGHTBRACKET;
    case AKEYCODE_GRAVE: return SDL_SCANCODE_GRAVE;
    case AKEYCODE_DPAD_LEFT: return SDL_SCANCODE_LEFT;
    case AKEYCODE_DPAD_RIGHT: return SDL_SCANCODE_RIGHT;
    case AKEYCODE_DPAD_UP: return SDL_SCANCODE_UP;
    case AKEYCODE_DPAD_DOWN: return SDL_SCANCODE_DOWN;
    case AKEYCODE_PAGE_UP: return SDL_SCANCODE_PAGEUP;
    case AKEYCODE_PAGE_DOWN: return SDL_SCANCODE_PAGEDOWN;
    case AKEYCODE_MOVE_HOME: return SDL_SCANCODE_HOME;
    case AKEYCODE_MOVE_END: return SDL_SCANCODE_END;
    case AKEYCODE_INSERT: return SDL_SCANCODE_INSERT;
    case AKEYCODE_FORWARD_DEL: return SDL_SCANCODE_DELETE;
    case AKEYCODE_SHIFT_LEFT: return SDL_SCANCODE_LSHIFT;
    case AKEYCODE_SHIFT_RIGHT: return SDL_SCANCODE_RSHIFT;
    case AKEYCODE_CTRL_LEFT: return SDL_SCANCODE_LCTRL;
    case AKEYCODE_CTRL_RIGHT: return SDL_SCANCODE_RCTRL;
    case AKEYCODE_ALT_LEFT: return SDL_SCANCODE_LALT;
    case AKEYCODE_ALT_RIGHT: return SDL_SCANCODE_RALT;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

static void QueueTextInputCodepoint(uint32_t charCode)
{
    if ((charCode == 0) || (charCode > 0x10FFFF))
    {
        return;
    }

    SDL_Event event {};
    event.type = SDL_TEXTINPUT;
    EncodeUtf32ToUtf8(charCode, event.text.text);
    if (event.text.text[0] != '\0')
    {
        QueueSyntheticSDLEvent(event);
    }
}

static void QueueTextInputUtf8(const char* textUtf8)
{
    if (!textUtf8 || !textUtf8[0])
    {
        return;
    }

    SDL_Event event {};
    event.type = SDL_TEXTINPUT;
    std::strncpy(event.text.text, textUtf8, SDL_TEXTINPUTEVENT_TEXT_SIZE - 1);
    event.text.text[SDL_TEXTINPUTEVENT_TEXT_SIZE - 1] = '\0';
    QueueSyntheticSDLEvent(event);
}

static bool ShouldEmitTextInputForAndroidKey(jint unicodeChar, SDL_Keycode keycode)
{
    if (unicodeChar == 0)
    {
        return false;
    }

    if (keycode == SDLK_BACKSPACE || keycode == SDLK_RETURN || keycode == SDLK_KP_ENTER
        || keycode == SDLK_AC_BACK || keycode == SDLK_TAB)
    {
        return false;
    }

    return unicodeChar >= 0x20;
}

static void QueueAndroidKeyEvent(
    jint action,
    jint keyCode,
    jint unicodeChar,
    jint metaState,
    jint repeatCount)
{
    if ((action != kAndroidKeyActionDown) && (action != kAndroidKeyActionUp))
    {
        return;
    }

    SDL_Event event {};
    event.type = (action == kAndroidKeyActionDown) ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.state = (action == kAndroidKeyActionDown) ? SDL_PRESSED : SDL_RELEASED;
    event.key.repeat = (repeatCount > 0) ? 1 : 0;
    event.key.keysym.sym = ConvertAndroidKeycodeToSDLKeycode(keyCode);
    event.key.keysym.scancode = ConvertAndroidKeycodeToSDLScancode(keyCode);
    event.key.keysym.mod = ConvertAndroidMetaStateToSDLKeymod(metaState);
    QueueSyntheticSDLEvent(event);

    if ((action == kAndroidKeyActionDown) && ShouldEmitTextInputForAndroidKey(unicodeChar, event.key.keysym.sym))
    {
        QueueTextInputCodepoint(static_cast<uint32_t>(unicodeChar));
    }
}

static SDL_FingerID ToSdlFingerId(uintptr_t identifier)
{
    return static_cast<SDL_FingerID>(identifier);
}

static void QueueSappEventAsSDL(const sapp_event* event)
{
    if (!event)
    {
        return;
    }

    switch (event->type)
    {
    case SAPP_EVENTTYPE_QUIT_REQUESTED:
        {
            SDL_Event sdlEvent {};
            sdlEvent.type = SDL_QUIT;
            QueueSyntheticSDLEvent(sdlEvent);
        }
        break;

    case SAPP_EVENTTYPE_SUSPENDED:
    case SAPP_EVENTTYPE_UNFOCUSED:
    case SAPP_EVENTTYPE_ICONIFIED:
        {
            // Stop here, synchronously, rather than through the queued SDL
            // event below: sokol_app's Android backend stops calling
            // desc.frame_cb the moment the activity is paused/unfocused
            // (has_resumed/has_focus gate _sapp_android_should_update), so
            // nothing ever drains that queue - and therefore nothing ever
            // stopped the sound - until the app was foregrounded again.
            // SoundPool/MediaPlayer keep playing under Android's process
            // lifecycle regardless, so combat SFX and map BGM were audible
            // for the entire time the app sat in the background/switcher.
            AllStopSound();
            AndroidAudioStopMusic();

            SDL_Event sdlEvent {};
            sdlEvent.type = SDL_APP_DIDENTERBACKGROUND;
            QueueSyntheticSDLEvent(sdlEvent);
        }
        break;

    case SAPP_EVENTTYPE_RESUMED:
    case SAPP_EVENTTYPE_FOCUSED:
    case SAPP_EVENTTYPE_RESTORED:
        {
            SDL_Event sdlEvent {};
            sdlEvent.type = SDL_APP_DIDENTERFOREGROUND;
            QueueSyntheticSDLEvent(sdlEvent);
        }
        break;

    case SAPP_EVENTTYPE_RESIZED:
        {
            g_DrawableWidth = (event->framebuffer_width > 0) ? event->framebuffer_width : g_DrawableWidth;
            g_DrawableHeight = (event->framebuffer_height > 0) ? event->framebuffer_height : g_DrawableHeight;
            SDL_Event sdlEvent {};
            sdlEvent.type = SDL_WINDOWEVENT;
            sdlEvent.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
            sdlEvent.window.data1 = g_DrawableWidth;
            sdlEvent.window.data2 = g_DrawableHeight;
            QueueSyntheticSDLEvent(sdlEvent);
        }
        break;

    case SAPP_EVENTTYPE_MOUSE_MOVE:
        {
            SDL_Event sdlEvent {};
            sdlEvent.type = SDL_MOUSEMOTION;
            sdlEvent.motion.x = static_cast<Sint32>(event->mouse_x);
            sdlEvent.motion.y = static_cast<Sint32>(event->mouse_y);
            if ((event->modifiers & SAPP_MODIFIER_LMB) != 0) sdlEvent.motion.state |= SDL_BUTTON_LMASK;
            if ((event->modifiers & SAPP_MODIFIER_RMB) != 0) sdlEvent.motion.state |= SDL_BUTTON_RMASK;
            if ((event->modifiers & SAPP_MODIFIER_MMB) != 0) sdlEvent.motion.state |= SDL_BUTTON_MMASK;
            QueueSyntheticSDLEvent(sdlEvent);
        }
        break;

    case SAPP_EVENTTYPE_MOUSE_DOWN:
    case SAPP_EVENTTYPE_MOUSE_UP:
        {
            SDL_Event sdlEvent {};
            sdlEvent.type = (event->type == SAPP_EVENTTYPE_MOUSE_DOWN) ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            sdlEvent.button.state = (event->type == SAPP_EVENTTYPE_MOUSE_DOWN) ? SDL_PRESSED : SDL_RELEASED;
            sdlEvent.button.x = static_cast<Sint32>(event->mouse_x);
            sdlEvent.button.y = static_cast<Sint32>(event->mouse_y);
            switch (event->mouse_button)
            {
            case SAPP_MOUSEBUTTON_LEFT: sdlEvent.button.button = SDL_BUTTON_LEFT; break;
            case SAPP_MOUSEBUTTON_RIGHT: sdlEvent.button.button = SDL_BUTTON_RIGHT; break;
            case SAPP_MOUSEBUTTON_MIDDLE: sdlEvent.button.button = SDL_BUTTON_MIDDLE; break;
            default: sdlEvent.button.button = 0; break;
            }
            QueueSyntheticSDLEvent(sdlEvent);
        }
        break;

    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        {
            SDL_Event sdlEvent {};
            sdlEvent.type = SDL_MOUSEWHEEL;
            sdlEvent.wheel.x = static_cast<Sint32>(std::lround(event->scroll_x));
            sdlEvent.wheel.y = static_cast<Sint32>(std::lround(event->scroll_y));
            QueueSyntheticSDLEvent(sdlEvent);
        }
        break;

    case SAPP_EVENTTYPE_TOUCHES_BEGAN:
    case SAPP_EVENTTYPE_TOUCHES_MOVED:
    case SAPP_EVENTTYPE_TOUCHES_ENDED:
    case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
        {
            // Normalise against the PHYSICAL surface, not g_DrawableWidth/Height:
            // sokol reports touch positions in physical pixels, while the
            // drawable size is the (possibly smaller) render size once render
            // scaling is enabled. Using the scaled size here would push every
            // touch past 1.0 and clamp it to the screen edge, which is exactly
            // how an earlier attempt at render scaling broke the login screen.
            const float safeW = static_cast<float>((g_NativePresentWidth > 0) ? g_NativePresentWidth : g_DrawableWidth > 0 ? g_DrawableWidth : 1);
            const float safeH = static_cast<float>((g_NativePresentHeight > 0) ? g_NativePresentHeight : g_DrawableHeight > 0 ? g_DrawableHeight : 1);
            Uint32 sdlType = SDL_FINGERMOTION;
            if (event->type == SAPP_EVENTTYPE_TOUCHES_BEGAN) sdlType = SDL_FINGERDOWN;
            else if ((event->type == SAPP_EVENTTYPE_TOUCHES_ENDED) || (event->type == SAPP_EVENTTYPE_TOUCHES_CANCELLED)) sdlType = SDL_FINGERUP;

            for (int i = 0; i < event->num_touches; ++i)
            {
                const sapp_touchpoint& touch = event->touches[i];
                if (!touch.changed)
                {
                    continue;
                }

                SDL_Event sdlEvent {};
                sdlEvent.type = sdlType;
                sdlEvent.tfinger.touchId = 0;
                sdlEvent.tfinger.fingerId = ToSdlFingerId(touch.identifier);
                sdlEvent.tfinger.x = std::clamp(touch.pos_x / safeW, 0.0f, 1.0f);
                sdlEvent.tfinger.y = std::clamp(touch.pos_y / safeH, 0.0f, 1.0f);
                sdlEvent.tfinger.dx = 0.0f;
                sdlEvent.tfinger.dy = 0.0f;
                sdlEvent.tfinger.pressure = 1.0f;
                QueueSyntheticSDLEvent(sdlEvent);
            }
        }
        break;

    case SAPP_EVENTTYPE_KEY_DOWN:
    case SAPP_EVENTTYPE_KEY_UP:
        {
            SDL_Event sdlEvent {};
            sdlEvent.type = (event->type == SAPP_EVENTTYPE_KEY_DOWN) ? SDL_KEYDOWN : SDL_KEYUP;
            sdlEvent.key.state = (event->type == SAPP_EVENTTYPE_KEY_DOWN) ? SDL_PRESSED : SDL_RELEASED;
            sdlEvent.key.repeat = event->key_repeat ? 1 : 0;
            sdlEvent.key.keysym.sym = ConvertSappKeycodeToSDLKeycode(event->key_code);
            sdlEvent.key.keysym.scancode = ConvertSappKeycodeToSDLScancode(event->key_code);
            sdlEvent.key.keysym.mod = ConvertSappModifiersToSDLKeymod(event->modifiers);
            QueueSyntheticSDLEvent(sdlEvent);
        }
        break;

    case SAPP_EVENTTYPE_CHAR:
        QueueTextInputCodepoint(event->char_code);
        break;

    default:
        break;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_muonline_client_MuMainNativeActivity_nativeOnTextInput(
    JNIEnv* env,
    jclass,
    jstring text)
{
    if ((env == nullptr) || (text == nullptr))
    {
        return;
    }

    const char* textUtf8 = env->GetStringUTFChars(text, nullptr);
    if (textUtf8 != nullptr)
    {
        QueueTextInputUtf8(textUtf8);
        env->ReleaseStringUTFChars(text, textUtf8);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_muonline_client_MuMainNativeActivity_nativeOnKeyEvent(
    JNIEnv*,
    jclass,
    jint action,
    jint keyCode,
    jint unicodeChar,
    jint metaState,
    jint repeatCount)
{
    if (action == kAndroidKeyActionMultiple)
    {
        return;
    }
    QueueAndroidKeyEvent(action, keyCode, unicodeChar, metaState, repeatCount);
}

static void ProcessAndroidEventQueue()
{
    int screenW = g_DrawableWidth;
    int screenH = g_DrawableHeight;
    std::vector<SDL_Event> events = DrainSyntheticSDLEvents();
    for (const SDL_Event& event : events)
    {
        HandleSDLEvent(event, screenW, screenH);
        if (Destroy)
        {
            break;
        }
    }
    g_DrawableWidth = screenW;
    g_DrawableHeight = screenH;
}

static void ShutdownAndroidGame();

static bool InitializeAndroidGame()
{
    if (g_AndroidGameInitialized)
    {
        return true;
    }

    LOGI("=== MuMain Android sokol bootstrap ===");
    Destroy = false;
    g_bWndActive = true;
    g_AndroidQuitRequested = false;
    g_pendingImeEnterTextInput = false;
    g_DrawableWidth = (g_DrawableWidth > 0) ? g_DrawableWidth : 1280;
    g_DrawableHeight = (g_DrawableHeight > 0) ? g_DrawableHeight : 720;

    MU_MobilePlatformInit();
    SetWorkingDirectoryToMobileDataRoot();
    InitializeTakumiProtectState();
    InitializeTakumiPacketKeys();
    // Audio is available: MuAudio (SoundPool + MediaPlayer) needs neither SDL
    // nor a device handle. This was false because the SDL_mixer path it used to
    // gate could never open, which switched sound and music off here before
    // anything downstream had a chance.
    g_AndroidAudioAvailable = true;

    const char* backendEnv = std::getenv("MU_RENDER_BACKEND");
    const RenderBackendType requestedBackend = ParseRenderBackendType(backendEnv);
    LOGI(
        "Render backend request env=%s -> %s",
        backendEnv ? backendEnv : "(null)",
        RenderBackendTypeToString(requestedBackend));

    int screenW = (g_DrawableWidth > 0) ? g_DrawableWidth : 1280;
    int screenH = (g_DrawableHeight > 0) ? g_DrawableHeight : 720;
    UpdateAndroidScreenMetrics(screenW, screenH);
    LOGI(
        "Screen size (drawable): %dx%d (scale: %.2f x %.2f)",
        screenW,
        screenH,
        g_fScreenRate_x,
        g_fScreenRate_y);

    auto initializeRenderBackend = [&](RenderBackendType backendType) -> bool
    {
        std::unique_ptr<IRenderBackend> backend = CreateRenderBackend(backendType);
        if (!backend)
        {
            LOGE("CreateRenderBackend failed for type=%s", RenderBackendTypeToString(backendType));
            return false;
        }

        if (!backend->Initialize(screenW, screenH))
        {
            LOGW("Render backend init failed: %s", backend->GetName());
            return false;
        }

        LOGI("Render backend active: %s", backend->GetName());
        g_RenderBackend = std::move(backend);
        return true;
    };

    if (!initializeRenderBackend(requestedBackend))
    {
        if ((requestedBackend != RenderBackendType::OpenGLCompat)
            && initializeRenderBackend(RenderBackendType::OpenGLCompat))
        {
            (void)0;
        }
        else
        {
            LOGE("No render backend could be initialized");
            ShutdownAndroidGame();
            return false;
        }
    }

    const bool preferDirectVertexArrays = IsLikelyAndroidEmulator();
    g_adaptivePerf.isEmulator = preferDirectVertexArrays;
    GL_SetPreferDirectVertexArrays(preferDirectVertexArrays);
    // TEMP A/B: orphaning the streaming VBO per draw is a large win on Adreno
    // (see the 298739f commit), but Mali recycles buffer allocations from a
    // pool, and exhausting that pool makes the driver wait on the GPU. The
    // MediaTek/Mali device shows objR and chrR spiking 6-7x on some frames
    // while pure-CPU work stays flat, which is the shape of a driver stall
    // rather than the CPU slowing down. Set true to fall back to a grow-only
    // buffer with glBufferSubData, with no other change.
    GL_SetSkipVBOOrphan(preferDirectVertexArrays || g_ForceSkipVBOOrphan);
    LOGI(
        "GL compat VA policy: preferDirect=%d (emulator=%d)",
        preferDirectVertexArrays ? 1 : 0,
        preferDirectVertexArrays ? 1 : 0);

    {
        int fontSize = static_cast<int>(std::ceil(12.0f + (static_cast<float>(WindowHeight) - 480.0f) / 200.0f));
        if (fontSize < 10) fontSize = 10;
        AndroidGDI_Init(fontSize);
        g_hFont = AndroidCreateFont(fontSize, 400);
        g_hFontBold = AndroidCreateFont(fontSize, 600);
        g_hFontBig = AndroidCreateFont(fontSize * 2, 600);
        g_hFixFont = AndroidCreateFont((static_cast<int>(WindowHeight) <= 600) ? 13 : 14, 400);
        LOGI("GDI fonts created: size=%d big=%d", fontSize, fontSize * 2);

        // See the matching block further down for why this matters - nameplate
        // and chat-bubble line spacing divides by FontHeight, which otherwise
        // stays at its zero default on Android.
        FontHeight = fontSize + 1;
    }

    if (!g_hWnd) g_hWnd = reinterpret_cast<HWND>(0x1);
    CInput::Instance().Create(g_hWnd, static_cast<long>(WindowWidth), static_cast<long>(WindowHeight));

    GameConfig::GetInstance().Load();

    const bool requestedSoundEnabled = GameConfig::GetInstance().GetSoundEnabled();
    const bool requestedMusicEnabled = GameConfig::GetInstance().GetMusicEnabled();
    m_SoundOnOff = requestedSoundEnabled && g_AndroidAudioAvailable;
    m_MusicOnOff = requestedMusicEnabled && g_AndroidAudioAvailable;
    m_RememberMe = GameConfig::GetInstance().GetRememberMe() ? 1 : 0;
    LOGI(
        "Audio config requested(sound=%d music=%d) active(sound=%d music=%d) rememberMe=%d",
        requestedSoundEnabled ? 1 : 0,
        requestedMusicEnabled ? 1 : 0,
        static_cast<int>(m_SoundOnOff),
        static_cast<int>(m_MusicOnOff),
        static_cast<int>(m_RememberMe));

    // Brings up SoundPool and registers the sound table. The working directory
    // is already the data root (SetWorkingDirectoryToMobileDataRoot above), and
    // registration makes each path absolute from it, which is what SoundPool
    // needs - it resolves relative paths against the JVM's user.dir, not ours.
    AndroidAudioInit();
    SetEnableSound(m_SoundOnOff != 0);

    static std::wstring serverIP = L"139.99.24.220";
    //static std::wstring serverIP = GameConfig::GetInstance().GetServerIP();
    int configuredPort = GameConfig::GetInstance().GetServerPort();
    // if (serverIP.empty() || serverIP == L"127.127.127.127" || serverIP == L"192.168.1.33" || serverIP == L"172.22.71.136")
    // {
    //     serverIP = L"172.22.71.136";
    // }
    if ((configuredPort <= 0) || (configuredPort == 55901) || (configuredPort == 44405) || (configuredPort == 44406))
    {
        configuredPort = 63000;
    }
    GameConfig::GetInstance().SetServerIP(serverIP);
    GameConfig::GetInstance().SetServerPort(configuredPort);

    static char androidServerIpA[64] = {};
    std::wcstombs(androidServerIpA, serverIP.c_str(), sizeof(androidServerIpA) - 1);
    szServerIpAddress = androidServerIpA;
    g_ServerPort = static_cast<WORD>(configuredPort);
    LOGI("Network target = %s:%u", szServerIpAddress, g_ServerPort);

    if (m_RememberMe)
    {
        wchar_t usernameW[_countof(m_Username)] = {};
        wchar_t passwordW[_countof(m_Password)] = {};
        GameConfig::GetInstance().DecryptCredentials(usernameW, passwordW, _countof(usernameW), _countof(passwordW));
        std::wcstombs(m_Username, usernameW, _countof(m_Username) - 1);
        std::wcstombs(m_Password, passwordW, _countof(m_Password) - 1);
    }

    std::wstring langSel = GameConfig::GetInstance().GetLanguageSelection();
    std::wcstombs(g_aszMLSelection, langSel.c_str(), MAX_LANGUAGE_NAME_LENGTH - 1);
    g_aszMLSelection[MAX_LANGUAGE_NAME_LENGTH - 1] = '\0';
    if (g_aszMLSelection[0] == '\0')
    {
        std::strcpy(g_aszMLSelection, "Eng");
    }
    g_strSelectedML = g_aszMLSelection;
    pMultiLanguage = new CMultiLanguage(g_strSelectedML);

    LOGI("INIT: srand");
    srand(static_cast<unsigned int>(time(nullptr)));
    for (int& value : RandomTable)
    {
        value = rand() % 360;
    }

    LOGI("INIT: alloc GateAttribute");
    RendomMemoryDump = new BYTE[rand() % 100 + 1];
    GateAttribute = new GATE_ATTRIBUTE[MAX_GATES] {};
    LOGI("INIT: alloc SkillAttribute");
    SkillAttribute = new SKILL_ATTRIBUTE[MAX_SKILLS] {};
    LOGI("INIT: alloc ItemAttRibute");
    ItemAttRibuteMemoryDump = new ITEM_ATTRIBUTE[MAX_ITEM + 1024] {};
    ItemAttribute = ItemAttRibuteMemoryDump + rand() % 1024;
    LOGI("INIT: alloc CharacterMemoryDump");
    CharacterMemoryDump = new CHARACTER[MAX_CHARACTERS_CLIENT + 1 + 128] {};
    CharactersClient = CharacterMemoryDump + rand() % 128;
    LOGI("INIT: alloc CharacterMachine");
    CharacterMachine = new CHARACTER_MACHINE;

    std::memset(GateAttribute, 0, sizeof(GATE_ATTRIBUTE) * MAX_GATES);
    std::memset(ItemAttribute, 0, sizeof(ITEM_ATTRIBUTE) * MAX_ITEM);
    std::memset(SkillAttribute, 0, sizeof(SKILL_ATTRIBUTE) * MAX_SKILLS);
    std::memset(CharacterMachine, 0, sizeof(CHARACTER_MACHINE));

    LOGI("INIT: CharacterMachine->Init()");
    CharacterAttribute = &CharacterMachine->Character;
    CharacterMachine->Init();
    Hero = &CharactersClient[0];

    LOGI("INIT: new UI objects");
    g_pMercenaryInputBox = new CUIMercenaryInputBox;
    g_pSingleTextInputBox = new CUITextInputBox;
    g_pSinglePasswdInputBox = new CUITextInputBox;
    g_pUIManager = new CUIManager;
    g_pUIMapName = new CUIMapName;

    LOGI("INIT: BuffStateSystem::Make()");
    g_BuffSystem = BuffStateSystem::Make();
    LOGI("INIT: MapProcess::Make()");
    g_MapProcess = MapProcess::Make();
    LOGI("INIT: PetProcess::Make()");
    g_petProcess = PetProcess::Make();

    LOGI("INIT: CUIMng::Create()");
    CUIMng::Instance().Create();
    LOGI("INIT: g_pNewUISystem->Create()");
    g_pNewUISystem->Create();
    LOGI("INIT: UI creation done");
    if (gCB_MUHelper == nullptr)
    {
        gCB_MUHelper = new CB_MUHelper;
    }
    LoadVirtualSkillSlots();
    LOGI("VirtualPad slots: %s", BuildVirtualSkillArrayString(g_virtualSkillSlots).c_str());

    if (g_adaptivePerf.isEmulator)
    {
        g_adaptivePerf.defaultMessageBudget = 65;
        g_adaptivePerf.minMessageBudget = 25;
    }

    SetMaxMessagePerCycle(g_adaptivePerf.defaultMessageBudget);
    SetTargetFps(g_adaptivePerf.targetFps);
    LOGI(
        "Android perf defaults: maxMsgPerCycle=%d targetFps=%.1f fxScale=%.2f adaptive=on isEmulator=%d",
        g_MaxMessagePerCycle,
        g_adaptivePerf.targetFps,
        1.0f,
        g_adaptivePerf.isEmulator ? 1 : 0);

    if (g_AndroidAudioAvailable && (m_SoundOnOff || m_MusicOnOff) && g_pOption)
    {
        int vol = GameConfig::GetInstance().GetVolumeLevel();
        if ((vol < 0) || (vol > 10)) vol = 10;
        g_pOption->SetVolumeLevel(vol);
        SetEffectVolumeLevel(vol);
        LOGI("SDL mixer volume set: level=%d", vol);
    }
    else if (g_AndroidAudioAvailable && (m_MusicOnOff || m_SoundOnOff))
    {
        SetEffectVolumeLevel(10);
        LOGI("SDL mixer fallback volume applied: level=10");
    }

    g_AndroidFrameState.perfFrequency = static_cast<Uint64>(MU_MobilePerfFrequency());
    g_AndroidFrameState.perfWindowStart = static_cast<Uint64>(MU_MobilePerfNow());
    g_AndroidFrameState.perfFrames = 0;
    g_AndroidFrameState.drawCallsTotal = 0;
    g_AndroidFrameState.vertsTotal = 0;
    g_AndroidFrameState.dbgFrameCount = 0;
    g_AndroidFrameState.muHelperLastTickMs = MU_MobileGetTicks();
    g_AndroidFrameState.hackLastTickMs = MU_MobileGetTicks();

    g_AndroidGameInitialized = true;
    LOGI("All systems initialized - sokol frame loop active");
    return true;
}

static void RunAndroidGameFrame()
{
    if (!g_AndroidGameInitialized)
    {
        if (!InitializeAndroidGame())
        {
            if (!g_AndroidQuitRequested)
            {
                g_AndroidQuitRequested = true;
                sapp_request_quit();
            }
            return;
        }
    }

    if (Destroy)
    {
        if (!g_AndroidQuitRequested)
        {
            g_AndroidQuitRequested = true;
            sapp_request_quit();
        }
        return;
    }

    SyncAndroidDrawableSizeFromSokol("frame");

    MouseLButtonDBClick = false;
    if (MouseLButtonPop && ((g_iMousePopPosition_x != MouseX) || (g_iMousePopPosition_y != MouseY)))
    {
        MouseLButtonPop = false;
    }

    ProcessAndroidEventQueue();
    if (Destroy)
    {
        if (!g_AndroidQuitRequested)
        {
            g_AndroidQuitRequested = true;
            sapp_request_quit();
        }
        return;
    }

    const bool hasFocusedTextInput = AndroidHasFocusedTextInput() || g_charNameInputActive;
#if !defined(MU_ANDROID_DISABLE_LOG)
    static int s_lastImeState = -1;
    const int imeState = hasFocusedTextInput ? 1 : 0;
    if (s_lastImeState != imeState)
    {
        LOGI(
            "CHATIME IME state changed focused=%d sdlTextInput=%d",
            imeState,
            MU_MobileIsTextInputActive() ? 1 : 0);
        s_lastImeState = imeState;
    }
#endif
    if (hasFocusedTextInput)
    {
        if (!MU_MobileIsTextInputActive())
        {
            MU_MobileStartTextInput();
        }
    }
    else if (MU_MobileIsTextInputActive())
    {
        MU_MobileStopTextInput();
    }

    // The chat bar has no business staying on screen once the keyboard is gone
    // - on desktop it closes with the input, but here dismissing the keyboard
    // only dropped focus and left the bar sitting over the hotkey row. Closing
    // it on the same transition keeps the two together.
    if (!hasFocusedTextInput
        && g_pNewUISystem != nullptr
        && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX))
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_CHATINPUTBOX);
    }

    AndroidDrainPackets();
    UpdateVirtualPadHolds();

    const uint32_t nowTicks = MU_MobileGetTicks();
    int hackTickBudget = 4;
    while ((nowTicks - g_AndroidFrameState.hackLastTickMs) >= 20000u && (hackTickBudget-- > 0))
    {
        CheckHack();
        g_AndroidFrameState.hackLastTickMs += 20000u;
    }

    int muHelperTickBudget = 4;
    while ((nowTicks - g_AndroidFrameState.muHelperLastTickMs) >= 250u && (muHelperTickBudget-- > 0))
    {
        if (SceneFlag == MAIN_SCENE && gCB_MUHelper != nullptr)
        {
            gCB_MUHelper->Work();
        }
        g_AndroidFrameState.muHelperLastTickMs += 250u;
    }

    // NOTE: this is the live per-frame path (sokol_app desc.frame_cb =
    // OnAndroidSappFrame -> RunAndroidGameFrame). The old SDL "while
    // (!Destroy)" loop further down this file (with its own s_crfResult/
    // CheckRenderNextFrame/SDL_GL_SetSwapInterval calls, and the whole
    // Scenes/*.cpp frame-timing system CheckRenderNextFrame lived in) is
    // dead code inside a #if 0 block AND excluded from the CMake build
    // (see ".*Scenes.*\\.cpp$" in CMakeLists.txt) - never runs, doesn't
    // even link. The actually-live frame cap is the sleep-based limiter in
    // ZzzScene.cpp's RenderScene() (target_fps/ms_per_frame), which reads
    // the g_adaptivePerf.targetFps default set above via SetTargetFps().
    if (g_bWndActive)
    {
        const Uint64 renderSceneStart = static_cast<Uint64>(MU_MobilePerfNow());
        Scene(nullptr);
        const Uint64 virtualPadStart = static_cast<Uint64>(MU_MobilePerfNow());
        RenderVirtualPad();
        const Uint64 presentStart = static_cast<Uint64>(MU_MobilePerfNow());
        if (g_RenderBackend)
        {
            g_RenderBackend->Present();
        }
        else
        {
            GL_FlushPending();
        }
        const Uint64 presentEnd = static_cast<Uint64>(MU_MobilePerfNow());

        int frameDrawCalls = 0;
        int frameVerts = 0;
        if (g_RenderBackend)
        {
            const RenderBackendStats stats = g_RenderBackend->GetAndResetStats();
            frameDrawCalls = stats.drawCalls;
            frameVerts = stats.vertices;

            // TEMP profiling: draw-path split. imDrawCalls counts draws that
            // went through the immediate-mode emulation, where GL_Vertex3f
            // does a full matrix transform PER VERTEX on the CPU. Overlay
            // passes using RENDER_METAL / RENDER_CHROME2 / RENDER_LIGHTMAP are
            // rejected by CanUseMobileDirectMeshBatch (ZzzBMD.cpp) and land
            // there, which is the suspected cost behind chrR's 39ms "post".
            g_ProfDrawCalls = stats.drawCalls;
            g_ProfImDrawCalls = stats.imDrawCalls;
            g_ProfVaConvertedDrawCalls = stats.vaConvertedDrawCalls;
            g_ProfVaDirectDrawCalls = stats.vaDirectDrawCalls;
            g_ProfQuadIndexedDrawCalls = stats.quadIndexedDrawCalls;
            g_ProfQuadExpandedDrawCalls = stats.quadExpandedDrawCalls;
            g_ProfVerts = stats.vertices;
            for (int fc = 0; fc < 12; ++fc) g_ProfFlushCauses[fc] = stats.flushCauses[fc];
            for (int ds = 0; ds < 10; ++ds) g_ProfDrawSites[ds] = stats.drawSites[ds];
        }
        else
        {
            GL_GetDrawStats(&frameDrawCalls, &frameVerts);
            GL_ResetDrawStats();
        }

        const ObjectPerfSnapshot objectPerf = ConsumeObjectPerfSnapshot();
        const CharacterPerfSnapshot characterPerf = ConsumeCharacterPerfSnapshot();
        const TerrainPerfSnapshot terrainPerf = ConsumeTerrainPerfSnapshot();
        const MainScenePerfSnapshot mainScenePerf = ConsumeMainScenePerfSnapshot();

        // TEMP profiling: mirror this frame's sub-phase costs into globals the
        // FPS overlay can read (see g_Prof* declarations above).
        g_ProfObjMoveTicks = static_cast<unsigned long long>(objectPerf.moveTicks);
        g_ProfObjRenderTicks = static_cast<unsigned long long>(objectPerf.renderTicks);
        g_ProfCharMoveTicks = static_cast<unsigned long long>(characterPerf.moveTicks);
        g_ProfCharRenderTicks = static_cast<unsigned long long>(characterPerf.renderTicks);
        g_ProfTerrainTicks =
            static_cast<unsigned long long>(terrainPerf.renderTicks) +
            static_cast<unsigned long long>(terrainPerf.afterTicks);
        g_ProfEffectsTicks = static_cast<unsigned long long>(mainScenePerf.effectsTicks);
        g_ProfParticlesTicks = static_cast<unsigned long long>(mainScenePerf.particlesTicks);
        g_ProfUiTicks = static_cast<unsigned long long>(mainScenePerf.uiTicks);
        g_ProfObjCandidates = objectPerf.renderCandidates;
        g_ProfObjRendered = objectPerf.renderRendered;
        g_ProfVisualCallsNow = objectPerf.visualCalls;
        g_ProfObjCulled = objectPerf.renderDistanceCulled;
        g_ProfCharCandidates = characterPerf.renderCandidates;
        g_ProfCharRendered = characterPerf.renderRendered;
        g_ProfShadowTicks = static_cast<unsigned long long>(mainScenePerf.shadowTicks);
        g_ProfBoidsTicks = static_cast<unsigned long long>(mainScenePerf.boidsTicks);
        g_ProfMiscWorldTicks = static_cast<unsigned long long>(mainScenePerf.miscWorldTicks);
        g_ProfJointsTicks = static_cast<unsigned long long>(mainScenePerf.jointsTicks);
        g_ProfBlursTicks = static_cast<unsigned long long>(mainScenePerf.blursTicks);
        g_ProfSpritesTicks = static_cast<unsigned long long>(mainScenePerf.spritesTicks);
        g_ProfPointsTicks = static_cast<unsigned long long>(mainScenePerf.pointsTicks);
        g_ProfAfterEffectsTicks = static_cast<unsigned long long>(mainScenePerf.afterEffectsTicks);
        g_ProfCharShadowTicks = static_cast<unsigned long long>(characterPerf.renderShadowTicks);
        g_ProfCharMonsterObjTicks = static_cast<unsigned long long>(characterPerf.renderMonsterObjectTicks);
        g_ProfCharAttachTicks = static_cast<unsigned long long>(characterPerf.renderAttachmentTicks);
        g_ProfCharPostTicks = static_cast<unsigned long long>(characterPerf.renderPostTicks);

        ++g_AndroidFrameState.perfFrames;
        g_AndroidFrameState.drawCallsTotal += frameDrawCalls;
        g_AndroidFrameState.vertsTotal += frameVerts;
        g_AndroidFrameState.objRenderCandidatesTotal += objectPerf.renderCandidates;
        g_AndroidFrameState.objRenderRenderedTotal += objectPerf.renderRendered;
        g_AndroidFrameState.objRenderCulledTotal += objectPerf.renderDistanceCulled;
        g_AndroidFrameState.objRenderBaseCallsTotal += objectPerf.renderBaseCalls;
        g_AndroidFrameState.objVisualCallsTotal += objectPerf.visualCalls;
        g_AndroidFrameState.objRenderAfterCallsTotal += objectPerf.renderAfterCalls;
        g_AndroidFrameState.charRenderCandidatesTotal += characterPerf.renderCandidates;
        g_AndroidFrameState.charRenderRenderedTotal += characterPerf.renderRendered;
        g_AndroidFrameState.charRenderDeferredTotal += characterPerf.renderDeferred;
        g_AndroidFrameState.charRenderPlayersTotal += characterPerf.renderPlayers;
        g_AndroidFrameState.charRenderMonstersTotal += characterPerf.renderMonsters;
        g_AndroidFrameState.terrainNormalBlocksTotal += terrainPerf.normalBlocks;
        g_AndroidFrameState.terrainNormalTilesTotal += terrainPerf.normalTiles;
        g_AndroidFrameState.terrainGrassBlocksTotal += terrainPerf.grassBlocks;
        g_AndroidFrameState.terrainGrassTilesTotal += terrainPerf.grassTiles;
        g_AndroidFrameState.terrainAfterBlocksTotal += terrainPerf.afterBlocks;
        g_AndroidFrameState.terrainAfterTilesTotal += terrainPerf.afterTiles;
        g_AndroidFrameState.sceneTicksTotal += virtualPadStart - renderSceneStart;
        g_AndroidFrameState.padTicksTotal += presentStart - virtualPadStart;
        g_AndroidFrameState.presentTicksTotal += presentEnd - presentStart;

        // TEMP profiling: last frame's top-level split, surfaced in the FPS
        // overlay (ZzzScene.cpp). logcat is unavailable on retail "user"
        // builds, and PERF_LOGI therefore never reaches us on the test devices.
        // Scene() covers all game update + 3D + UI drawing; Present() is the
        // flush/blit/swap, where GPU back-pressure shows up.
        g_ProfSceneTicks = virtualPadStart - renderSceneStart;

        // TEMP profiling: a screenshot only samples whatever instant it lands
        // on, so it almost never catches a hitch. Track the worst frame in a
        // rolling window and latch its bucket breakdown, so one screenshot
        // shows what the bad frame actually looked like, plus how often the
        // frame overran badly.
        {
            const double frameMs =
                static_cast<double>(g_ProfSceneTicks) * 1000.0 /
                static_cast<double>(MU_MobilePerfFrequency());

            ++g_ProfHitchFrames;
            g_ProfHitchSceneMsSum += frameMs;

            const double meanMs =
                (g_ProfHitchFrames > 0)
                    ? (g_ProfHitchSceneMsSum / static_cast<double>(g_ProfHitchFrames))
                    : frameMs;
            if ((g_ProfHitchFrames > 30) && (frameMs > (meanMs * 2.0)))
            {
                ++g_ProfHitchCount;
            }

            if (frameMs > g_ProfWorstSceneMs)
            {
                g_ProfWorstSceneMs = frameMs;
                g_ProfWorstObjTicks = g_ProfObjRenderTicks;
                g_ProfWorstCharTicks = g_ProfCharRenderTicks;
                g_ProfWorstUiTicks = g_ProfUiTicks;
                g_ProfWorstParticleTicks = g_ProfParticlesTicks;
                g_ProfWorstTerrainTicks = g_ProfTerrainTicks;
                g_ProfWorstObjMoveTicks = g_ProfObjMoveTicks;
                g_ProfWorstTexDefines = g_ProfTexDefineCount;
                g_ProfWorstTexDefineTicks = g_ProfTexDefineTicks;
                // How much work the bad frame actually did, so "slow frame"
                // can be told apart from "frame that rendered far more".
                g_ProfWorstObjRendered = objectPerf.renderRendered;
                g_ProfWorstObjCandidates = objectPerf.renderCandidates;
                g_ProfWorstVisualCalls = objectPerf.visualCalls;
                g_ProfWorstCharRendered = characterPerf.renderRendered;
                g_ProfWorstDrawCalls = frameDrawCalls;
            }

            // Accumulate the text pipeline over the log window, then clear it
            // for the next frame. Done here rather than in the overlay so it
            // still happens with the overlay switched off.
            g_DriftTextExtentMs += static_cast<double>(g_ProfTextExtentTicks) * 1000.0 / static_cast<double>(MU_MobilePerfFrequency());
            g_DriftTextOutMs += static_cast<double>(g_ProfTextOutTicks) * 1000.0 / static_cast<double>(MU_MobilePerfFrequency());
            g_DriftTextUpMs += static_cast<double>(g_ProfTextUploadTicks) * 1000.0 / static_cast<double>(MU_MobilePerfFrequency());
            g_DriftTextCalls += g_ProfTextCalls;
            g_DriftTextHits += g_ProfTextCacheHits;
            g_DriftTextMisses += g_ProfTextCacheMisses;
            g_ProfTextExtentTicks = 0;
            g_ProfTextOutTicks = 0;
            g_ProfTextWriteTicks = 0;
            g_ProfTextUploadTicks = 0;
            g_ProfTextCalls = 0;
            g_ProfTextCacheHits = 0;
            g_ProfTextCacheMisses = 0;
            // Snapshot then clear: the drift log prints further down in the same
            // frame, so zeroing in place would always log zeros. Mirrors how
            // g_ProfDrawSites holds the last frame is values at print time.
            for (int mp = 0; mp < 5; ++mp)
            {
                g_ProfMeshPassLast[mp] = g_ProfMeshPass[mp];
                g_ProfMeshPass[mp] = 0;
            }

            // Reset the per-frame asset-load counters for the next frame.
            g_ProfTexDefineCount = 0;
            g_ProfTexDefineTicks = 0;

            // Drift log: one line every 10s to mu_drift_log.txt in the app's
            // external files dir (the process chdir's there at startup). This
            // is how a long run is measured without an on-screen overlay
            // distorting the very thing being measured.
            {
                g_DriftFrames++;
                g_DriftSceneMsSum += frameMs;
                if (frameMs > g_DriftWorstMs) g_DriftWorstMs = frameMs;
                if ((g_DriftFrames > 30) && (frameMs > (g_DriftSceneMsSum / g_DriftFrames) * 2.0))
                {
                    g_DriftHitches++;
                }

                const double nowSec =
                    static_cast<double>(MU_MobilePerfNow()) / static_cast<double>(MU_MobilePerfFrequency());
                if (g_DriftStartSec <= 0.0) g_DriftStartSec = nowSec;
                if (g_DriftLastLogSec <= 0.0) g_DriftLastLogSec = nowSec;

                if ((nowSec - g_DriftLastLogSec) >= 10.0)
                {
                    const double windowSec = nowSec - g_DriftLastLogSec;
                    if (FILE* f = fopen("mu_drift_log.txt", "a"))
                    {
                        fprintf(f,
                            "t=%.0fs fps=%.1f scnAvg=%.1f worst=%.0f hitch=%d/%d obj=%.1f chr=%.1f ui=%.1f objN=%d draws=%d text=%.1f(ext%.1f out%.1f up%.1f)/%d overlay=%d"
                            " txt[hit%d miss%d coll%d]"
                            " path[im%d vaConv%d vaDir%d qIdx%d qExp%d]"
                            " site[bIM%d cli%d qi%d lva%d bIdx%d bTri%d skin%d]"
                            " skinTexSw%d charN%d"
                            " pass[shd%d chr%d brt%d tex%d oth%d]"
                            " meshN%d"
                            " cut[tex%d up%d bl%d dep%d en%d af%d proj%d unb%d pm%d full%d oth%d]\n",
                            nowSec - g_DriftStartSec,
                            g_DriftFrames / windowSec,
                            g_DriftSceneMsSum / (g_DriftFrames > 0 ? g_DriftFrames : 1),
                            g_DriftWorstMs,
                            g_DriftHitches, g_DriftFrames,
                            static_cast<double>(g_ProfObjRenderTicks) * 1000.0 / static_cast<double>(MU_MobilePerfFrequency()),
                            static_cast<double>(g_ProfCharRenderTicks) * 1000.0 / static_cast<double>(MU_MobilePerfFrequency()),
                            static_cast<double>(g_ProfUiTicks) * 1000.0 / static_cast<double>(MU_MobilePerfFrequency()),
                            g_ProfObjRendered, frameDrawCalls,
                            (g_DriftTextExtentMs + g_DriftTextOutMs + g_DriftTextUpMs) / (g_DriftFrames > 0 ? g_DriftFrames : 1),
                            g_DriftTextExtentMs / (g_DriftFrames > 0 ? g_DriftFrames : 1),
                            g_DriftTextOutMs / (g_DriftFrames > 0 ? g_DriftFrames : 1),
                            g_DriftTextUpMs / (g_DriftFrames > 0 ? g_DriftFrames : 1),
                            (g_DriftFrames > 0) ? (g_DriftTextCalls / g_DriftFrames) : 0,
                            g_ShowPerfOverlay ? 1 : 0,
                            // Last frame of the window rather than a sum: these
                            // are per-frame counts and a 10s total would just
                            // scale with frame count.
                            // Draw-path split. draws is the total; im is how
                            // many of those came out of the immediate-mode
                            // batcher. The remainder are per-mesh vertex-array
                            // draws, which the batcher never sees - so this is
                            // what says whether batching or instancing is the
                            // job.
                            // coll is cumulative, not per window: one occurrence is
                            // the whole finding.
                            g_DriftTextHits, g_DriftTextMisses, g_ProfTextSlotCollisions,
                            g_ProfImDrawCalls, g_ProfVaConvertedDrawCalls,
                            g_ProfVaDirectDrawCalls, g_ProfQuadIndexedDrawCalls,
                            g_ProfQuadExpandedDrawCalls,
                            g_ProfDrawSites[0], g_ProfDrawSites[1], g_ProfDrawSites[2],
                            g_ProfDrawSites[3], g_ProfDrawSites[4], g_ProfDrawSites[5],
                            g_ProfDrawSites[6],
                            g_ProfDrawSites[7], g_ProfCharRendered,
                            g_ProfMeshPassLast[0], g_ProfMeshPassLast[1], g_ProfMeshPassLast[2],
                            g_ProfMeshPassLast[3], g_ProfMeshPassLast[4],
                            g_ProfDrawSites[8],
                            g_ProfFlushCauses[1], g_ProfFlushCauses[2], g_ProfFlushCauses[3],
                            g_ProfFlushCauses[4], g_ProfFlushCauses[5], g_ProfFlushCauses[6],
                            g_ProfFlushCauses[7],
                            g_ProfFlushCauses[9], g_ProfFlushCauses[10], g_ProfFlushCauses[11],
                            g_ProfFlushCauses[0]);
                        fclose(f);
                    }
                    g_DriftLastLogSec = nowSec;
                    g_DriftFrames = 0;
                    g_DriftSceneMsSum = 0.0;
                    g_DriftWorstMs = 0.0;
                    g_DriftHitches = 0;
                    g_DriftTextExtentMs = 0.0;
                    g_DriftTextOutMs = 0.0;
                    g_DriftTextUpMs = 0.0;
                    g_DriftTextCalls = 0;
                    g_DriftTextHits = 0;
                    g_DriftTextMisses = 0;
                }
            }

            // Roll the window so a single startup stall does not dominate the
            // reading forever.
            if (g_ProfHitchFrames >= 600)
            {
                g_ProfHitchFrames = 0;
                g_ProfHitchSceneMsSum = 0.0;
                g_ProfHitchCount = 0;
                g_ProfWorstSceneMs = 0.0;
            }
        }
        g_ProfPadTicks = presentStart - virtualPadStart;
        g_ProfPresentTicks = presentEnd - presentStart;
        g_AndroidFrameState.objMoveTicksTotal += static_cast<Uint64>(objectPerf.moveTicks);
        g_AndroidFrameState.objRenderTicksTotal += static_cast<Uint64>(objectPerf.renderTicks);
        g_AndroidFrameState.terrainRenderTicksTotal += static_cast<Uint64>(terrainPerf.renderTicks);
        g_AndroidFrameState.terrainAfterTicksTotal += static_cast<Uint64>(terrainPerf.afterTicks);
        g_AndroidFrameState.charMoveTicksTotal += static_cast<Uint64>(characterPerf.moveTicks);
        g_AndroidFrameState.charRenderTicksTotal += static_cast<Uint64>(characterPerf.renderTicks);
        g_AndroidFrameState.mainShadowTicksTotal += static_cast<Uint64>(mainScenePerf.shadowTicks);
        g_AndroidFrameState.mainBoidsTicksTotal += static_cast<Uint64>(mainScenePerf.boidsTicks);
        g_AndroidFrameState.mainMiscWorldTicksTotal += static_cast<Uint64>(mainScenePerf.miscWorldTicks);
        g_AndroidFrameState.mainJointsTicksTotal += static_cast<Uint64>(mainScenePerf.jointsTicks);
        g_AndroidFrameState.mainEffectsTicksTotal += static_cast<Uint64>(mainScenePerf.effectsTicks);
        g_AndroidFrameState.mainBlursTicksTotal += static_cast<Uint64>(mainScenePerf.blursTicks);
        g_AndroidFrameState.mainSpritesTicksTotal += static_cast<Uint64>(mainScenePerf.spritesTicks);
        g_AndroidFrameState.mainParticlesTicksTotal += static_cast<Uint64>(mainScenePerf.particlesTicks);
        g_AndroidFrameState.mainPointsTicksTotal += static_cast<Uint64>(mainScenePerf.pointsTicks);
        g_AndroidFrameState.mainAfterEffectsTicksTotal += static_cast<Uint64>(mainScenePerf.afterEffectsTicks);
        g_AndroidFrameState.mainUiTicksTotal += static_cast<Uint64>(mainScenePerf.uiTicks);

        const Uint64 nowCounter = static_cast<Uint64>(MU_MobilePerfNow());
        const double elapsedSec =
            static_cast<double>(nowCounter - g_AndroidFrameState.perfWindowStart)
            / static_cast<double>((g_AndroidFrameState.perfFrequency > 0) ? g_AndroidFrameState.perfFrequency : 1);
        if ((elapsedSec >= 0.25) && (g_AndroidFrameState.perfFrames > 0))
        {
            const double perfFreq =
                (g_AndroidFrameState.perfFrequency > 0)
                    ? static_cast<double>(g_AndroidFrameState.perfFrequency)
                    : 1.0;
            const double tickToMs = 1000.0 / perfFreq;
            const double fps = static_cast<double>(g_AndroidFrameState.perfFrames) / elapsedSec;
            const double avgFrameMs = (elapsedSec * 1000.0) / static_cast<double>(g_AndroidFrameState.perfFrames);
            const int avgDrawCalls = g_AndroidFrameState.drawCallsTotal / g_AndroidFrameState.perfFrames;
            const int avgVerts = g_AndroidFrameState.vertsTotal / g_AndroidFrameState.perfFrames;
            const double avgSceneMs =
                (static_cast<double>(g_AndroidFrameState.sceneTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgPadMs =
                (static_cast<double>(g_AndroidFrameState.padTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgPresentMs =
                (static_cast<double>(g_AndroidFrameState.presentTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const int avgObjRenderCandidates =
                g_AndroidFrameState.objRenderCandidatesTotal / g_AndroidFrameState.perfFrames;
            const int avgObjRenderRendered =
                g_AndroidFrameState.objRenderRenderedTotal / g_AndroidFrameState.perfFrames;
            const int avgObjRenderCulled =
                g_AndroidFrameState.objRenderCulledTotal / g_AndroidFrameState.perfFrames;
            const int avgObjRenderBaseCalls =
                g_AndroidFrameState.objRenderBaseCallsTotal / g_AndroidFrameState.perfFrames;
            const int avgObjVisualCalls =
                g_AndroidFrameState.objVisualCallsTotal / g_AndroidFrameState.perfFrames;
            const int avgObjRenderAfterCalls =
                g_AndroidFrameState.objRenderAfterCallsTotal / g_AndroidFrameState.perfFrames;
            const int avgCharRenderCandidates =
                g_AndroidFrameState.charRenderCandidatesTotal / g_AndroidFrameState.perfFrames;
            const int avgCharRenderRendered =
                g_AndroidFrameState.charRenderRenderedTotal / g_AndroidFrameState.perfFrames;
            const int avgCharRenderDeferred =
                g_AndroidFrameState.charRenderDeferredTotal / g_AndroidFrameState.perfFrames;
            const int avgCharRenderPlayers =
                g_AndroidFrameState.charRenderPlayersTotal / g_AndroidFrameState.perfFrames;
            const int avgCharRenderMonsters =
                g_AndroidFrameState.charRenderMonstersTotal / g_AndroidFrameState.perfFrames;
            const int avgTerrainNormalBlocks =
                g_AndroidFrameState.terrainNormalBlocksTotal / g_AndroidFrameState.perfFrames;
            const int avgTerrainNormalTiles =
                g_AndroidFrameState.terrainNormalTilesTotal / g_AndroidFrameState.perfFrames;
            const int avgTerrainGrassBlocks =
                g_AndroidFrameState.terrainGrassBlocksTotal / g_AndroidFrameState.perfFrames;
            const int avgTerrainGrassTiles =
                g_AndroidFrameState.terrainGrassTilesTotal / g_AndroidFrameState.perfFrames;
            const int avgTerrainAfterBlocks =
                g_AndroidFrameState.terrainAfterBlocksTotal / g_AndroidFrameState.perfFrames;
            const int avgTerrainAfterTiles =
                g_AndroidFrameState.terrainAfterTilesTotal / g_AndroidFrameState.perfFrames;
            const double avgObjMoveMs =
                (static_cast<double>(g_AndroidFrameState.objMoveTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgObjRenderMs =
                (static_cast<double>(g_AndroidFrameState.objRenderTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgTerrainRenderMs =
                (static_cast<double>(g_AndroidFrameState.terrainRenderTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgTerrainAfterMs =
                (static_cast<double>(g_AndroidFrameState.terrainAfterTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgCharMoveMs =
                (static_cast<double>(g_AndroidFrameState.charMoveTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgCharRenderMs =
                (static_cast<double>(g_AndroidFrameState.charRenderTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgMainShadowMs =
                (static_cast<double>(g_AndroidFrameState.mainShadowTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgMainBoidsMs =
                (static_cast<double>(g_AndroidFrameState.mainBoidsTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgMainMiscWorldMs =
                (static_cast<double>(g_AndroidFrameState.mainMiscWorldTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgMainJointsMs =
                (static_cast<double>(g_AndroidFrameState.mainJointsTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgMainEffectsMs =
                (static_cast<double>(g_AndroidFrameState.mainEffectsTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgMainBlursMs =
                (static_cast<double>(g_AndroidFrameState.mainBlursTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgMainSpritesMs =
                (static_cast<double>(g_AndroidFrameState.mainSpritesTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgMainParticlesMs =
                (static_cast<double>(g_AndroidFrameState.mainParticlesTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgMainPointsMs =
                (static_cast<double>(g_AndroidFrameState.mainPointsTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgMainAfterEffectsMs =
                (static_cast<double>(g_AndroidFrameState.mainAfterEffectsTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgMainUiMs =
                (static_cast<double>(g_AndroidFrameState.mainUiTicksTotal) * tickToMs)
                / static_cast<double>(g_AndroidFrameState.perfFrames);
            const double avgTrackedSceneMs =
                avgObjMoveMs + avgObjRenderMs +
                avgCharMoveMs + avgCharRenderMs +
                avgTerrainRenderMs + avgTerrainAfterMs;
            const double avgOtherSceneMs =
                (avgSceneMs > avgTrackedSceneMs) ? (avgSceneMs - avgTrackedSceneMs) : 0.0;
            const double avgPhaseTrackedMs =
                avgMainShadowMs + avgMainBoidsMs + avgMainMiscWorldMs +
                avgMainJointsMs + avgMainEffectsMs + avgMainBlursMs +
                avgMainSpritesMs + avgMainParticlesMs + avgMainPointsMs +
                avgMainAfterEffectsMs + avgMainUiMs;
            const double avgPhaseRemainderMs =
                (avgOtherSceneMs > avgPhaseTrackedMs) ? (avgOtherSceneMs - avgPhaseTrackedMs) : 0.0;
            UpdateAdaptivePerformance(fps, avgFrameMs);
            PERF_LOGI(
                "PERF fps=%.1f frameMs=%.2f sceneMs=%.2f padMs=%.2f presentMs=%.2f avg(draw=%d verts=%d)",
                fps,
                avgFrameMs,
                avgSceneMs,
                avgPadMs,
                avgPresentMs,
                avgDrawCalls,
                avgVerts);
            if (((g_AndroidFrameState.detailLogWindowCounter++ % 4) == 0) || fps < 15.0)
            {
                const SceneCrowdSnapshot crowdSnapshot = CaptureSceneCrowdSnapshot();
                PERF_LOGI(
                    "PERF_SCENE world=%d:%s fxScale=%.2f char(vis=%d ply=%d mon=%d npc=%d pet=%d topMon=%d:%s x%d) obj(vis=%d op=%d trap=%d topObj=%d:%s x%d)",
                    gMapManager.WorldActive,
                    GetPerfWorldDebugName(gMapManager.WorldActive),
                    GetAdaptiveEffectSpawnScale(),
                    crowdSnapshot.visibleCharacters,
                    crowdSnapshot.visiblePlayers,
                    crowdSnapshot.visibleMonsters,
                    crowdSnapshot.visibleNpcs,
                    crowdSnapshot.visiblePets,
                    crowdSnapshot.dominantMonsterType,
                    GetPerfMonsterDebugName(crowdSnapshot.dominantMonsterType),
                    crowdSnapshot.dominantMonsterCount,
                    crowdSnapshot.visibleWorldObjects,
                    crowdSnapshot.visibleOperateObjects,
                    crowdSnapshot.visibleTrapObjects,
                    crowdSnapshot.dominantObjectType,
                    GetPerfObjectDebugName(crowdSnapshot.dominantObjectType),
                    crowdSnapshot.dominantObjectCount);
                PERF_LOGI(
                    "PERF_CPU obj(move=%.2f render=%.2f cand=%d draw=%d cull=%d calls=%d/%d/%d) char(move=%.2f render=%.2f cand=%d draw=%d def=%d ply=%d mon=%d) terrain(render=%.2f after=%.2f blk=%d/%d grass=%d/%d after=%d/%d)",
                    avgObjMoveMs,
                    avgObjRenderMs,
                    avgObjRenderCandidates,
                    avgObjRenderRendered,
                    avgObjRenderCulled,
                    avgObjRenderBaseCalls,
                    avgObjVisualCalls,
                    avgObjRenderAfterCalls,
                    avgCharMoveMs,
                    avgCharRenderMs,
                    avgCharRenderCandidates,
                    avgCharRenderRendered,
                    avgCharRenderDeferred,
                    avgCharRenderPlayers,
                    avgCharRenderMonsters,
                    avgTerrainRenderMs,
                    avgTerrainAfterMs,
                    avgTerrainNormalBlocks,
                    avgTerrainNormalTiles,
                    avgTerrainGrassBlocks,
                    avgTerrainGrassTiles,
                    avgTerrainAfterBlocks,
                    avgTerrainAfterTiles);
                PERF_LOGI(
                    "PERF_PHASE shadow=%.2f boids=%.2f misc=%.2f joints=%.2f effects=%.2f blurs=%.2f sprites=%.2f particles=%.2f points=%.2f afterfx=%.2f ui=%.2f rem=%.2f",
                    avgMainShadowMs,
                    avgMainBoidsMs,
                    avgMainMiscWorldMs,
                    avgMainJointsMs,
                    avgMainEffectsMs,
                    avgMainBlursMs,
                    avgMainSpritesMs,
                    avgMainParticlesMs,
                    avgMainPointsMs,
                    avgMainAfterEffectsMs,
                    avgMainUiMs,
                    avgPhaseRemainderMs);
            }
            g_AndroidFrameState.perfWindowStart = nowCounter;
            g_AndroidFrameState.perfFrames = 0;
            g_AndroidFrameState.drawCallsTotal = 0;
            g_AndroidFrameState.vertsTotal = 0;
            g_AndroidFrameState.objRenderCandidatesTotal = 0;
            g_AndroidFrameState.objRenderRenderedTotal = 0;
            g_AndroidFrameState.objRenderCulledTotal = 0;
            g_AndroidFrameState.objRenderBaseCallsTotal = 0;
            g_AndroidFrameState.objVisualCallsTotal = 0;
            g_AndroidFrameState.objRenderAfterCallsTotal = 0;
            g_AndroidFrameState.charRenderCandidatesTotal = 0;
            g_AndroidFrameState.charRenderRenderedTotal = 0;
            g_AndroidFrameState.charRenderDeferredTotal = 0;
            g_AndroidFrameState.charRenderPlayersTotal = 0;
            g_AndroidFrameState.charRenderMonstersTotal = 0;
            g_AndroidFrameState.terrainNormalBlocksTotal = 0;
            g_AndroidFrameState.terrainNormalTilesTotal = 0;
            g_AndroidFrameState.terrainGrassBlocksTotal = 0;
            g_AndroidFrameState.terrainGrassTilesTotal = 0;
            g_AndroidFrameState.terrainAfterBlocksTotal = 0;
            g_AndroidFrameState.terrainAfterTilesTotal = 0;
            g_AndroidFrameState.sceneTicksTotal = 0;
            g_AndroidFrameState.padTicksTotal = 0;
            g_AndroidFrameState.presentTicksTotal = 0;
            g_AndroidFrameState.objMoveTicksTotal = 0;
            g_AndroidFrameState.objRenderTicksTotal = 0;
            g_AndroidFrameState.terrainRenderTicksTotal = 0;
            g_AndroidFrameState.terrainAfterTicksTotal = 0;
            g_AndroidFrameState.charMoveTicksTotal = 0;
            g_AndroidFrameState.charRenderTicksTotal = 0;
            g_AndroidFrameState.mainShadowTicksTotal = 0;
            g_AndroidFrameState.mainBoidsTicksTotal = 0;
            g_AndroidFrameState.mainMiscWorldTicksTotal = 0;
            g_AndroidFrameState.mainJointsTicksTotal = 0;
            g_AndroidFrameState.mainEffectsTicksTotal = 0;
            g_AndroidFrameState.mainBlursTicksTotal = 0;
            g_AndroidFrameState.mainSpritesTicksTotal = 0;
            g_AndroidFrameState.mainParticlesTicksTotal = 0;
            g_AndroidFrameState.mainPointsTicksTotal = 0;
            g_AndroidFrameState.mainAfterEffectsTicksTotal = 0;
            g_AndroidFrameState.mainUiTicksTotal = 0;
        }

        if (g_AndroidFrameState.dbgFrameCount < 10)
        {
            LOGI(
                "Frame %d - drawCalls=%d verts=%d",
                g_AndroidFrameState.dbgFrameCount,
                frameDrawCalls,
                frameVerts);
            ++g_AndroidFrameState.dbgFrameCount;
        }
    }
    else
    {
        MU_MobileSleep(16);
    }

    ProtocolCompiler();

    MouseLButtonPush = false;
    MouseRButtonPush = false;
    MouseMButtonPush = false;
    MouseWheel = 0;
}

static void ShutdownAndroidGame()
{
    if (!g_AndroidGameInitialized)
    {
        MU_MobilePlatformShutdown();
        return;
    }

    LOGI("Main loop exited - cleaning up");
    SaveVirtualSkillSlots();

    if (g_RenderBackend)
    {
        g_RenderBackend->Shutdown();
        g_RenderBackend.reset();
    }

    DestroyWindow_Android();
    if (g_AndroidAudioAvailable)
    {
        DestroySound();
    }
    KillGLWindow();
    MU_MobilePlatformShutdown();
    MU_MobileClearKeyboardState();

    if (gCB_MUHelper != nullptr)
    {
        delete gCB_MUHelper;
        gCB_MUHelper = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(g_PendingSdlEventsMutex);
        g_PendingSdlEvents.clear();
    }

    g_AndroidGameInitialized = false;
    LOGI("=== MuMain shutdown complete ===");
}

static void OnAndroidSappInit()
{
    if (!InitializeAndroidGame() && !g_AndroidQuitRequested)
    {
        g_AndroidQuitRequested = true;
        sapp_request_quit();
    }
}

static void OnAndroidSappFrame()
{
    RunAndroidGameFrame();
}

static void OnAndroidSappCleanup()
{
    ShutdownAndroidGame();
}

static void OnAndroidSappEvent(const sapp_event* event)
{
    QueueSappEventAsSDL(event);
}

// =============================================================================
// SDL2 main 鑺掗埀顑解偓?replaces WinMain
// SDL2 renames this to android_main internally via SDL_main.h macro
// =============================================================================
#if 0
int SDL_main(int argc, char* argv[])
{
    LOGI("=== MuMain Android v1.0 starting ===");
    (void)argc;
    (void)argv;

    SetWorkingDirectoryToMobileDataRoot();
    InitializeTakumiProtectState();
    InitializeTakumiPacketKeys();

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Initialize SDL2 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER) < 0) {
        LOGE("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }
    LOGI("SDL2 initialized");

    const char* backendEnv = SDL_getenv("MU_RENDER_BACKEND");
    const RenderBackendType requestedBackend = ParseRenderBackendType(backendEnv);
    LOGI(
        "Render backend request env=%s -> %s",
        backendEnv ? backendEnv : "(null)",
        RenderBackendTypeToString(requestedBackend));

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 OpenGL ES 3.1 context attributes 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,  SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,    1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,      16);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,    8);   // 8-bit stencil for circular icon clipping
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,        5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,      6);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,       5);

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Create fullscreen window 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    g_SDLWindow = SDL_CreateWindow(
        "MU Online",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        0, 0,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN
    );
    if (!g_SDLWindow) {
        LOGE("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Create OpenGL ES context 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    g_SDLGLContext = SDL_GL_CreateContext(g_SDLWindow);
    if (!g_SDLGLContext) {
        LOGE("SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(g_SDLWindow);
        SDL_Quit();
        return -1;
    }

    SDL_GL_MakeCurrent(g_SDLWindow, g_SDLGLContext);

    // VSync OFF 鑺掗埀顑解偓?allow software frame cap to drive render pacing.
    SDL_GL_SetSwapInterval(0);

    // Initialize OpenGL ES 2.0 compatibility layer (immediate mode emulation)

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Get actual GL drawable resolution (may differ from window size on Android
    //    due to system bars 鑺掗埀顑解偓?always use drawable size for viewport/rendering)
    int screenW = 1280, screenH = 720;
    SDL_GL_GetDrawableSize(g_SDLWindow, &screenW, &screenH);
    UpdateAndroidScreenMetrics(screenW, screenH);
    LOGI("Screen size (drawable): %dx%d (scale: %.2f x %.2f)",
        screenW, screenH, g_fScreenRate_x, g_fScreenRate_y);

    auto initializeRenderBackend = [&](RenderBackendType backendType) -> bool
    {
        std::unique_ptr<IRenderBackend> backend = CreateRenderBackend(backendType);
        if (!backend)
        {
            LOGE("CreateRenderBackend failed for type=%s", RenderBackendTypeToString(backendType));
            return false;
        }

        if (!backend->Initialize(g_SDLWindow, screenW, screenH))
        {
            LOGW("Render backend init failed: %s", backend->GetName());
            return false;
        }

        LOGI("Render backend active: %s", backend->GetName());
        g_RenderBackend = std::move(backend);
        return true;
    };

    if (!initializeRenderBackend(requestedBackend))
    {
        if (requestedBackend != RenderBackendType::OpenGLCompat)
        {
            LOGW("Falling back to OpenGLCompat backend");
            if (!initializeRenderBackend(RenderBackendType::OpenGLCompat))
            {
                LOGE("No render backend could be initialized");
                KillGLWindow();
                SDL_Quit();
                return -1;
            }
        }
        else
        {
            LOGE("OpenGLCompat backend failed to initialize");
            KillGLWindow();
            SDL_Quit();
            return -1;
        }
    }

    const bool preferDirectVertexArrays = IsLikelyAndroidEmulator();
    g_adaptivePerf.isEmulator = preferDirectVertexArrays;
    GL_SetPreferDirectVertexArrays(preferDirectVertexArrays);
    // SwiftShader (emulator) has no real GPU pipeline 鑺掗垾鐘偓?skip VBO orphaning.
    // Eliminates ~105 unnecessary driver-level malloc() calls per frame.
    // TEMP A/B: orphaning the streaming VBO per draw is a large win on Adreno
    // (see the 298739f commit), but Mali recycles buffer allocations from a
    // pool, and exhausting that pool makes the driver wait on the GPU. The
    // MediaTek/Mali device shows objR and chrR spiking 6-7x on some frames
    // while pure-CPU work stays flat, which is the shape of a driver stall
    // rather than the CPU slowing down. Set true to fall back to a grow-only
    // buffer with glBufferSubData, with no other change.
    GL_SetSkipVBOOrphan(preferDirectVertexArrays || g_ForceSkipVBOOrphan);
    LOGI(
        "GL compat VA policy: preferDirect=%d (emulator=%d)",
        preferDirectVertexArrays ? 1 : 0,
        preferDirectVertexArrays ? 1 : 0);

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Init Android GDI (SDL2_ttf text rendering)
    {
        int fontSize = (int)std::ceil(12.0f + ((float)WindowHeight - 480.0f) / 200.0f);
        if (fontSize < 10) fontSize = 10;
        AndroidGDI_Init(fontSize);

        // Create Windows-like font handles for the game
        g_hFont     = AndroidCreateFont(fontSize,     400); // FW_NORMAL
        g_hFontBold = AndroidCreateFont(fontSize,     600); // FW_SEMIBOLD
        g_hFontBig  = AndroidCreateFont(fontSize * 2, 600);
        g_hFixFont  = AndroidCreateFont(
            (int)WindowHeight <= 600 ? 13 : 14, 400);
        LOGI("GDI fonts created: size=%d big=%d", fontSize, fontSize*2);

        // Winmain.cpp sets this from WindowWidth on desktop (never runs here,
        // so it stayed at its zero-initialized default). RenderBoolean divides
        // by it to space nameplate lines - name, guild, and any chat bubble
        // text all landed on the same row and rendered as garbled overlapping
        // text until this was set. Mirrors desktop's iFontSize = FontHeight-1.
        FontHeight = fontSize + 1;
    }

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Init input system with actual screen size so dialogs position correctly
    // g_hWnd is null on Android; use a dummy non-null value so Create() doesn't
    // bail out before setting m_lScreenWidth/m_lScreenHeight.
    if (!g_hWnd) g_hWnd = (HWND)0x1;
    CInput::Instance().Create(g_hWnd, (long)WindowWidth, (long)WindowHeight);

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Load game config 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    GameConfig::GetInstance().Load();

    m_SoundOnOff = GameConfig::GetInstance().GetSoundEnabled();
    m_MusicOnOff = GameConfig::GetInstance().GetMusicEnabled();
    m_RememberMe = GameConfig::GetInstance().GetRememberMe() ? 1 : 0;
    LOGI("Audio config: sound=%d music=%d rememberMe=%d", (int)m_SoundOnOff, (int)m_MusicOnOff, (int)m_RememberMe);

    static std::wstring serverIP = L"139.99.24.220";
    //static std::wstring serverIP = GameConfig::GetInstance().GetServerIP();
    int configuredPort = GameConfig::GetInstance().GetServerPort();
    // if (serverIP.empty() || serverIP == L"172.22.71.136")
    // {
    //     serverIP = L"172.22.71.136";
    // }
    if (configuredPort <= 0 || configuredPort == 55901 || configuredPort == 44405 || configuredPort == 44406)
    {
        configuredPort = 63000;
    }
    GameConfig::GetInstance().SetServerIP(serverIP);
    GameConfig::GetInstance().SetServerPort(configuredPort);

    static char androidServerIpA[64] = {}; std::wcstombs(androidServerIpA, serverIP.c_str(), sizeof(androidServerIpA) - 1); szServerIpAddress = androidServerIpA;
    g_ServerPort = static_cast<WORD>(configuredPort);
    LOGI("Network target = %s:%u", szServerIpAddress, g_ServerPort);

    if (m_RememberMe)
    {
        wchar_t usernameW[_countof(m_Username)] = {};
        wchar_t passwordW[_countof(m_Password)] = {};
        GameConfig::GetInstance().DecryptCredentials(usernameW, passwordW,
            _countof(usernameW), _countof(passwordW));
        std::wcstombs(m_Username, usernameW, _countof(m_Username) - 1);
        std::wcstombs(m_Password, passwordW, _countof(m_Password) - 1);
    }

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Multi-language setup 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    std::wstring langSel = GameConfig::GetInstance().GetLanguageSelection();
    std::wcstombs(g_aszMLSelection, langSel.c_str(), MAX_LANGUAGE_NAME_LENGTH - 1);
    g_aszMLSelection[MAX_LANGUAGE_NAME_LENGTH - 1] = '\0';
    if (g_aszMLSelection[0] == '\0')
        strcpy(g_aszMLSelection, "Eng");
    g_strSelectedML = g_aszMLSelection;
    pMultiLanguage  = new CMultiLanguage(g_strSelectedML);

    // Audio is brought up in InitializeAndroidGame, not here. This function is
    // unreachable: sokol_main is the entry point and nothing calls SDL_main, so
    // the Mix_Init/Mix_OpenAudio that used to sit here never ran - which is why
    // no MIXOPEN line ever appeared in mu_sound_log.txt.

    // Text loaded by OpenTextData() -> GlobalText.Load(text_eng.bmd) in OpenBasicData()

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Init game data 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    LOGI("INIT: srand");
    srand((unsigned)time(nullptr));
    for (int& v : RandomTable) v = rand() % 360;

    LOGI("INIT: alloc GateAttribute");
    RendomMemoryDump        = new BYTE[rand() % 100 + 1];
    GateAttribute           = new GATE_ATTRIBUTE[MAX_GATES]{};
    LOGI("INIT: alloc SkillAttribute");
    SkillAttribute          = new SKILL_ATTRIBUTE[MAX_SKILLS]{};
    LOGI("INIT: alloc ItemAttRibute");
    ItemAttRibuteMemoryDump = new ITEM_ATTRIBUTE[MAX_ITEM + 1024]{};
    ItemAttribute           = ItemAttRibuteMemoryDump + rand() % 1024;
    LOGI("INIT: alloc CharacterMemoryDump");
    CharacterMemoryDump     = new CHARACTER[MAX_CHARACTERS_CLIENT + 1 + 128]{};
    CharactersClient        = CharacterMemoryDump + rand() % 128;
    LOGI("INIT: alloc CharacterMachine");
    CharacterMachine        = new CHARACTER_MACHINE;

    memset(GateAttribute,    0, sizeof(GATE_ATTRIBUTE)    * MAX_GATES);
    memset(ItemAttribute,    0, sizeof(ITEM_ATTRIBUTE)    * MAX_ITEM);
    memset(SkillAttribute,   0, sizeof(SKILL_ATTRIBUTE)   * MAX_SKILLS);
    memset(CharacterMachine, 0, sizeof(CHARACTER_MACHINE));

    LOGI("INIT: CharacterMachine->Init()");
    CharacterAttribute = &CharacterMachine->Character;
    CharacterMachine->Init();
    Hero = &CharactersClient[0];

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Init UI 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    LOGI("INIT: new UI objects");
    g_pMercenaryInputBox    = new CUIMercenaryInputBox;
    g_pSingleTextInputBox   = new CUITextInputBox;
    g_pSinglePasswdInputBox = new CUITextInputBox;
    g_pUIManager            = new CUIManager;
    g_pUIMapName            = new CUIMapName;

    LOGI("INIT: BuffStateSystem::Make()");
    g_BuffSystem = BuffStateSystem::Make();
    LOGI("INIT: MapProcess::Make()");
    g_MapProcess = MapProcess::Make();
    LOGI("INIT: PetProcess::Make()");
    g_petProcess = PetProcess::Make();

    LOGI("INIT: CUIMng::Create()");
    CUIMng::Instance().Create();
    LOGI("INIT: g_pNewUISystem->Create()");
    g_pNewUISystem->Create();
    LOGI("INIT: UI creation done");
    LoadVirtualSkillSlots();
    {
        const std::string slotText = BuildVirtualSkillArrayString(g_virtualSkillSlots);
        LOGI("VirtualPad slots: %s", slotText.c_str());
    }

    // On emulator: tighten packet budget to free CPU for rendering.
    if (g_adaptivePerf.isEmulator)
    {
        g_adaptivePerf.defaultMessageBudget = 65;
        g_adaptivePerf.minMessageBudget     = 25;
    }

    // Keep effects ON globally. Adaptive may only trim spawn density under load.
    if (g_pOption)
    {
        (void)0;
    }

    (void)0;
    SetMaxMessagePerCycle(g_adaptivePerf.defaultMessageBudget);
    SetTargetFps(g_adaptivePerf.targetFps);  // 60 by default; see AdaptivePerfState::targetFps
    LOGI(
        "Android perf defaults: maxMsgPerCycle=%d targetFps=%.1f fxScale=%.2f adaptive=on isEmulator=%d",
        g_MaxMessagePerCycle,
        g_adaptivePerf.targetFps,
        1.0f,
        g_adaptivePerf.isEmulator ? 1 : 0);

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Volume 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    if ((m_SoundOnOff || m_MusicOnOff) && g_pOption)
    {
        int vol = GameConfig::GetInstance().GetVolumeLevel();
        if (vol < 0 || vol > 10) vol = 10;
        g_pOption->SetVolumeLevel(vol);
        SetEffectVolumeLevel(vol);
        LOGI("SDL mixer volume set: level=%d", vol);
    }
    else if (m_MusicOnOff || m_SoundOnOff)
    {
        SetEffectVolumeLevel(10);
        LOGI("SDL mixer fallback volume applied: level=10");
    }

    // Keep FPS overlay visible for runtime performance measurement on device.
    (void)0;
    LOGI("FPS overlay enabled");

    LOGI("All systems initialized 鑺掗埀顑解偓?entering main loop");

    // ==========================================================================
    // MAIN LOOP
    // Replaces WinMain's: while(PeekMessage) + WndProc + RenderScene(g_hDC)
    // ==========================================================================
    SDL_Event event;
    const Uint64 s_perfFreq = static_cast<Uint64>(MU_MobilePerfFrequency());
    Uint64 s_perfWindowStart = static_cast<Uint64>(MU_MobilePerfNow());
    int s_perfFrames = 0;
    int s_perfDrawCallsTotal = 0;
    int s_perfVertsTotal = 0;
    int s_perfDrawCallsMax = 0;
    int s_perfVertsMax = 0;
    int s_perfImDrawCallsTotal = 0;
    int s_perfVaDirectTotal = 0;
    int s_perfVaConvertedTotal = 0;
    int s_perfQuadIndexedTotal = 0;
    int s_perfQuadExpandedTotal = 0;
    int s_objMoveCandidatesTotal = 0;
    int s_objMoveUpdatedTotal = 0;
    int s_objMoveDeferredTotal = 0;
    int s_objMoveNearTotal = 0;
    int s_objMoveMidTotal = 0;
    int s_objMoveFarTotal = 0;
                int s_objRenderCandidatesTotal = 0;
                int s_objRenderRenderedTotal = 0;
                int s_objRenderCulledTotal = 0;
                int s_objRenderNearTotal = 0;
    int s_objRenderMidTotal = 0;
    int s_objRenderFarTotal = 0;
    int s_objVisualRenderedTotal = 0;
    int s_objVisualDeferredTotal = 0;
    int s_objRenderBaseCallsTotal = 0;
    int s_objVisualCallsTotal = 0;
    int s_objRenderAfterCallsTotal = 0;
    int s_charMoveCandidatesTotal = 0;
    int s_charMoveUpdatedTotal = 0;
    int s_charMoveDeferredTotal = 0;
    int s_charMoveNearTotal = 0;
    int s_charMoveMidTotal = 0;
    int s_charMoveFarTotal = 0;
    int s_charRenderCandidatesTotal = 0;
    int s_charRenderRenderedTotal = 0;
    int s_charRenderDeferredTotal = 0;
                int s_charRenderCulledTotal = 0;
                int s_charRenderNearTotal = 0;
                int s_charRenderMidTotal = 0;
                int s_charRenderFarTotal = 0;
    int s_charRenderPlayersTotal = 0;
    int s_charRenderMonstersTotal = 0;
    int s_charMonsterObjectCallsTotal = 0;
                int s_terrainNormalBlocksTotal = 0;
                int s_terrainNormalTilesTotal = 0;
    int s_terrainGrassBlocksTotal = 0;
    int s_terrainGrassTilesTotal = 0;
    int s_terrainAfterBlocksTotal = 0;
    int s_terrainAfterTilesTotal = 0;
    Uint64 s_perfRenderSceneTicksTotal = 0;
    Uint64 s_perfVirtualPadTicksTotal = 0;
    Uint64 s_perfPresentTicksTotal = 0;
    Uint64 s_objMoveTicksTotal = 0;
    Uint64 s_objRenderTicksTotal = 0;
    Uint64 s_objRenderBaseTicksTotal = 0;
    Uint64 s_objRenderVisualTicksTotal = 0;
    Uint64 s_objRenderAfterTicksTotal = 0;
    Uint64 s_charMoveTicksTotal = 0;
    Uint64 s_charRenderTicksTotal = 0;
    Uint64 s_charMonsterObjectTicksTotal = 0;
    Uint64 s_charRenderShadowTicksTotal = 0;
    Uint64 s_charRenderPostTicksTotal = 0;
    Uint64 s_charRenderAttachmentTicksTotal = 0;
    Uint64 s_terrainRenderTicksTotal = 0;
    Uint64 s_terrainAfterTicksTotal = 0;
    Uint64 s_mainShadowTicksTotal = 0;
    Uint64 s_mainBoidsTicksTotal = 0;
    Uint64 s_mainMiscWorldTicksTotal = 0;
    Uint64 s_mainJointsTicksTotal = 0;
    Uint64 s_mainEffectsTicksTotal = 0;
    Uint64 s_mainBlursTicksTotal = 0;
    Uint64 s_mainSpritesTicksTotal = 0;
    Uint64 s_mainParticlesTicksTotal = 0;
    Uint64 s_mainPointsTicksTotal = 0;
    Uint64 s_mainAfterEffectsTicksTotal = 0;
    Uint64 s_mainUiTicksTotal = 0;
    double s_cameraDistanceTotal = 0.0;
    double s_cameraDistanceTargetTotal = 0.0;
    double s_cameraOverrideTotal = 0.0;
    double s_cameraViewNearTotal = 0.0;
    double s_cameraViewFarTotal = 0.0;
    double s_cameraPitchTotal = 0.0;
    double s_cameraYawTotal = 0.0;
    float s_cameraDistanceMin = (std::numeric_limits<float>::max)();
    float s_cameraDistanceMax = (std::numeric_limits<float>::lowest)();
    float s_cameraTargetMin = (std::numeric_limits<float>::max)();
    float s_cameraTargetMax = (std::numeric_limits<float>::lowest)();
    float s_cameraFarMin = (std::numeric_limits<float>::max)();
    float s_cameraFarMax = (std::numeric_limits<float>::lowest)();
    float s_cameraPitchMin = (std::numeric_limits<float>::max)();
    float s_cameraPitchMax = (std::numeric_limits<float>::lowest)();
    float s_cameraYawMin = (std::numeric_limits<float>::max)();
    float s_cameraYawMax = (std::numeric_limits<float>::lowest)();
    int s_cameraTopViewFrames = 0;
    int s_cameraSettlingFrames = 0;
    uint32_t s_muHelperLastTickMs = MU_MobileGetTicks();
    uint32_t s_hackLastTickMs = MU_MobileGetTicks();

    while (!Destroy)
    {
        // Reset per-frame transient mouse state
        MouseLButtonDBClick = false;
        if (MouseLButtonPop &&
            (g_iMousePopPosition_x != MouseX || g_iMousePopPosition_y != MouseY))
            MouseLButtonPop = false;

        // Process all pending events
        while (SDL_PollEvent(&event))
        {
            HandleSDLEvent(event, screenW, screenH);
            if (Destroy) break;
        }
        if (Destroy) break;

        const bool hasFocusedTextInput = AndroidHasFocusedTextInput() || g_charNameInputActive;
#if !defined(MU_ANDROID_DISABLE_LOG)
        static int s_lastImeState = -1;
        const int imeState = hasFocusedTextInput ? 1 : 0;
        if (s_lastImeState != imeState)
        {
            LOGI(
                "CHATIME IME state changed focused=%d sdlTextInput=%d",
                imeState,
                SDL_IsTextInputActive() ? 1 : 0);
            s_lastImeState = imeState;
        }
#endif
        if (hasFocusedTextInput)
        {
            if (!SDL_IsTextInputActive())
            {
                SDL_StartTextInput();
            }
        }
        else if (SDL_IsTextInputActive())
        {
            SDL_StopTextInput();
        }

        AndroidDrainPackets();
        UpdateVirtualPadHolds();

        // Windows builds drive MU Helper via Win32 SetTimer(MUHELPER_TIMER, 250ms).
        // On Android, SetTimer is a stub, so tick MU Helper from the main loop.
        const uint32_t nowTicks = MU_MobileGetTicks();
        int hackTickBudget = 4;
        while ((nowTicks - s_hackLastTickMs) >= 20000u && hackTickBudget-- > 0)
        {
            CheckHack();
            s_hackLastTickMs += 20000u;
        }

        int muHelperTickBudget = 4;
        while ((nowTicks - s_muHelperLastTickMs) >= 250u && muHelperTickBudget-- > 0)
        {
            (void)nowTicks;
            s_muHelperLastTickMs += 250u;
        }
        if (g_bWndActive)
        {
            static bool s_crfResult = false;
            s_crfResult = CheckRenderNextFrame();

            if (s_crfResult)
            {
                const Uint64 renderSceneStart = static_cast<Uint64>(MU_MobilePerfNow());
                Scene(nullptr);
                const Uint64 virtualPadStart = static_cast<Uint64>(MU_MobilePerfNow());
                RenderVirtualPad();
                const Uint64 presentStart = static_cast<Uint64>(MU_MobilePerfNow());
                if (g_RenderBackend)
                {
                    g_RenderBackend->Present(g_SDLWindow);
                }
                else
                {
                    SDL_GL_SwapWindow(g_SDLWindow);
                }
                const Uint64 presentEnd = static_cast<Uint64>(MU_MobilePerfNow());

                s_perfRenderSceneTicksTotal += virtualPadStart - renderSceneStart;
                s_perfVirtualPadTicksTotal += presentStart - virtualPadStart;
                s_perfPresentTicksTotal += presentEnd - presentStart;

                int frameDrawCalls = 0;
                int frameVerts = 0;
                int frameImDrawCalls = 0;
                int frameVaDirect = 0;
                int frameVaConverted = 0;
                int frameQuadIndexed = 0;
                int frameQuadExpanded = 0;
                static int s_perfHeartbeatFrameCounter = 0;
                if (g_RenderBackend)
                {
                    const RenderBackendStats stats = g_RenderBackend->GetAndResetStats();
                    frameDrawCalls = stats.drawCalls;
                    frameVerts = stats.vertices;
                    frameImDrawCalls = stats.imDrawCalls;
                    frameVaDirect = stats.vaDirectDrawCalls;
                    frameVaConverted = stats.vaConvertedDrawCalls;
                    frameQuadIndexed = stats.quadIndexedDrawCalls;
                    frameQuadExpanded = stats.quadExpandedDrawCalls;
                }
                else
                {
                    GL_GetDrawStats(&frameDrawCalls, &frameVerts);
                    GL_ResetDrawStats();
                }

                const ObjectPerfSnapshot objectPerf = ConsumeObjectPerfSnapshot();
                const CharacterPerfSnapshot characterPerf = ConsumeCharacterPerfSnapshot();
                const TerrainPerfSnapshot terrainPerf = ConsumeTerrainPerfSnapshot();
                const MainScenePerfSnapshot mainScenePerf = ConsumeMainScenePerfSnapshot();

                ++s_perfHeartbeatFrameCounter;
                if ((s_perfHeartbeatFrameCounter % 120) == 0)
                {
                    PERF_LOGI("HEARTBEAT frame=%d draw=%d verts=%d im=%d vaD=%d vaC=%d qI=%d qE=%d wndActive=%d scene=%d world=%d fpsAvg=%.1f",
                        s_perfHeartbeatFrameCounter,
                        frameDrawCalls,
                        frameVerts,
                        frameImDrawCalls,
                        frameVaDirect,
                        frameVaConverted,
                        frameQuadIndexed,
                        frameQuadExpanded,
                        g_bWndActive ? 1 : 0,
                        SceneFlag,
                        gMapManager.WorldActive,
                        FPS_AVG);
                }

                s_perfFrames++;
                s_perfDrawCallsTotal += frameDrawCalls;
                s_perfVertsTotal += frameVerts;
                s_perfImDrawCallsTotal += frameImDrawCalls;
                s_perfVaDirectTotal += frameVaDirect;
                s_perfVaConvertedTotal += frameVaConverted;
                s_perfQuadIndexedTotal += frameQuadIndexed;
                s_perfQuadExpandedTotal += frameQuadExpanded;
                s_objMoveCandidatesTotal += objectPerf.moveCandidates;
                s_objMoveUpdatedTotal += objectPerf.moveUpdated;
                s_objMoveDeferredTotal += objectPerf.moveDeferred;
                s_objMoveNearTotal += objectPerf.moveNearCandidates;
                s_objMoveMidTotal += objectPerf.moveMidCandidates;
                s_objMoveFarTotal += objectPerf.moveFarCandidates;
                s_objRenderCandidatesTotal += objectPerf.renderCandidates;
                s_objRenderRenderedTotal += objectPerf.renderRendered;
                s_objRenderCulledTotal += objectPerf.renderDistanceCulled;
                s_objRenderNearTotal += objectPerf.renderNearCandidates;
                s_objRenderMidTotal += objectPerf.renderMidCandidates;
                s_objRenderFarTotal += objectPerf.renderFarCandidates;
                s_objVisualRenderedTotal += objectPerf.visualRendered;
                s_objVisualDeferredTotal += objectPerf.visualDeferred;
                s_objRenderBaseCallsTotal += objectPerf.renderBaseCalls;
                s_objVisualCallsTotal += objectPerf.visualCalls;
                s_objRenderAfterCallsTotal += objectPerf.renderAfterCalls;
                s_objMoveTicksTotal += static_cast<Uint64>(objectPerf.moveTicks);
                s_objRenderTicksTotal += static_cast<Uint64>(objectPerf.renderTicks);
                s_objRenderBaseTicksTotal += static_cast<Uint64>(objectPerf.renderBaseTicks);
                s_objRenderVisualTicksTotal += static_cast<Uint64>(objectPerf.renderVisualTicks);
                s_objRenderAfterTicksTotal += static_cast<Uint64>(objectPerf.renderAfterTicks);
                s_charMoveCandidatesTotal += characterPerf.moveCandidates;
                s_charMoveUpdatedTotal += characterPerf.moveUpdated;
                s_charMoveDeferredTotal += characterPerf.moveDeferred;
                s_charMoveNearTotal += characterPerf.moveNearCandidates;
                s_charMoveMidTotal += characterPerf.moveMidCandidates;
                s_charMoveFarTotal += characterPerf.moveFarCandidates;
                s_charRenderCandidatesTotal += characterPerf.renderCandidates;
                s_charRenderRenderedTotal += characterPerf.renderRendered;
                s_charRenderDeferredTotal += characterPerf.renderDeferred;
                s_charRenderCulledTotal += characterPerf.renderDistanceCulled;
                s_charRenderNearTotal += characterPerf.renderNearCandidates;
                s_charRenderMidTotal += characterPerf.renderMidCandidates;
                s_charRenderFarTotal += characterPerf.renderFarCandidates;
                s_charRenderPlayersTotal += characterPerf.renderPlayers;
                s_charRenderMonstersTotal += characterPerf.renderMonsters;
                s_charMonsterObjectCallsTotal += characterPerf.renderMonsterObjectCalls;
                s_charMoveTicksTotal += static_cast<Uint64>(characterPerf.moveTicks);
                s_charRenderTicksTotal += static_cast<Uint64>(characterPerf.renderTicks);
                s_charMonsterObjectTicksTotal += static_cast<Uint64>(characterPerf.renderMonsterObjectTicks);
                s_charRenderShadowTicksTotal += static_cast<Uint64>(characterPerf.renderShadowTicks);
                s_charRenderPostTicksTotal += static_cast<Uint64>(characterPerf.renderPostTicks);
                s_charRenderAttachmentTicksTotal += static_cast<Uint64>(characterPerf.renderAttachmentTicks);
                s_terrainNormalBlocksTotal += terrainPerf.normalBlocks;
                s_terrainNormalTilesTotal += terrainPerf.normalTiles;
                s_terrainGrassBlocksTotal += terrainPerf.grassBlocks;
                s_terrainGrassTilesTotal += terrainPerf.grassTiles;
                s_terrainAfterBlocksTotal += terrainPerf.afterBlocks;
                s_terrainAfterTilesTotal += terrainPerf.afterTiles;
                s_terrainRenderTicksTotal += static_cast<Uint64>(terrainPerf.renderTicks);
                s_terrainAfterTicksTotal += static_cast<Uint64>(terrainPerf.afterTicks);
                s_mainShadowTicksTotal += static_cast<Uint64>(mainScenePerf.shadowTicks);
                s_mainBoidsTicksTotal += static_cast<Uint64>(mainScenePerf.boidsTicks);
                s_mainMiscWorldTicksTotal += static_cast<Uint64>(mainScenePerf.miscWorldTicks);
                s_mainJointsTicksTotal += static_cast<Uint64>(mainScenePerf.jointsTicks);
                s_mainEffectsTicksTotal += static_cast<Uint64>(mainScenePerf.effectsTicks);
                s_mainBlursTicksTotal += static_cast<Uint64>(mainScenePerf.blursTicks);
                s_mainSpritesTicksTotal += static_cast<Uint64>(mainScenePerf.spritesTicks);
                s_mainParticlesTicksTotal += static_cast<Uint64>(mainScenePerf.particlesTicks);
                s_mainPointsTicksTotal += static_cast<Uint64>(mainScenePerf.pointsTicks);
                s_mainAfterEffectsTicksTotal += static_cast<Uint64>(mainScenePerf.afterEffectsTicks);
                s_mainUiTicksTotal += static_cast<Uint64>(mainScenePerf.uiTicks);
                const float perfCameraDistance = CameraDistance;
                const float perfCameraTarget = CameraDistanceTarget;
                const float perfCameraOverride = g_androidZoomOverride;
                const float perfCameraNear = CameraViewNear;
                const float perfCameraFar = CameraViewFar;
                const float perfCameraPitch = CameraAngle[0];
                const float perfCameraYaw = CameraAngle[2];
                s_cameraDistanceTotal += perfCameraDistance;
                s_cameraDistanceTargetTotal += perfCameraTarget;
                s_cameraOverrideTotal += perfCameraOverride;
                s_cameraViewNearTotal += perfCameraNear;
                s_cameraViewFarTotal += perfCameraFar;
                s_cameraPitchTotal += perfCameraPitch;
                s_cameraYawTotal += perfCameraYaw;
                s_cameraDistanceMin = (std::min)(s_cameraDistanceMin, perfCameraDistance);
                s_cameraDistanceMax = (std::max)(s_cameraDistanceMax, perfCameraDistance);
                s_cameraTargetMin = (std::min)(s_cameraTargetMin, perfCameraTarget);
                s_cameraTargetMax = (std::max)(s_cameraTargetMax, perfCameraTarget);
                s_cameraFarMin = (std::min)(s_cameraFarMin, perfCameraFar);
                s_cameraFarMax = (std::max)(s_cameraFarMax, perfCameraFar);
                s_cameraPitchMin = (std::min)(s_cameraPitchMin, perfCameraPitch);
                s_cameraPitchMax = (std::max)(s_cameraPitchMax, perfCameraPitch);
                s_cameraYawMin = (std::min)(s_cameraYawMin, perfCameraYaw);
                s_cameraYawMax = (std::max)(s_cameraYawMax, perfCameraYaw);
                if (CameraTopViewEnable)
                {
                    ++s_cameraTopViewFrames;
                }
                if (std::fabs(perfCameraDistance - perfCameraTarget) > 8.0f)
                {
                    ++s_cameraSettlingFrames;
                }
                if (frameDrawCalls > s_perfDrawCallsMax) s_perfDrawCallsMax = frameDrawCalls;
                if (frameVerts > s_perfVertsMax) s_perfVertsMax = frameVerts;

                const Uint64 nowCounter = static_cast<Uint64>(MU_MobilePerfNow());
                const double elapsedSec = (double)(nowCounter - s_perfWindowStart) / (double)s_perfFreq;
                if (elapsedSec >= 0.25 && s_perfFrames > 0)
                {
                    const double fps = (double)s_perfFrames / elapsedSec;
                    const double avgFrameMs = (elapsedSec * 1000.0) / (double)s_perfFrames;
                    const int avgDrawCalls = s_perfDrawCallsTotal / s_perfFrames;
                    const int avgVerts = s_perfVertsTotal / s_perfFrames;
                    const int avgImDrawCalls = s_perfImDrawCallsTotal / s_perfFrames;
                    const int avgVaDirect = s_perfVaDirectTotal / s_perfFrames;
                    const int avgVaConverted = s_perfVaConvertedTotal / s_perfFrames;
                    const int avgQuadIndexed = s_perfQuadIndexedTotal / s_perfFrames;
                    const int avgQuadExpanded = s_perfQuadExpandedTotal / s_perfFrames;
                    const int avgObjMoveCandidates = s_objMoveCandidatesTotal / s_perfFrames;
                    const int avgObjMoveUpdated = s_objMoveUpdatedTotal / s_perfFrames;
                    const int avgObjMoveDeferred = s_objMoveDeferredTotal / s_perfFrames;
                    const int avgObjMoveNear = s_objMoveNearTotal / s_perfFrames;
                    const int avgObjMoveMid = s_objMoveMidTotal / s_perfFrames;
                    const int avgObjMoveFar = s_objMoveFarTotal / s_perfFrames;
                    const int avgObjRenderCandidates = s_objRenderCandidatesTotal / s_perfFrames;
                    const int avgObjRenderRendered = s_objRenderRenderedTotal / s_perfFrames;
                    const int avgObjRenderCulled = s_objRenderCulledTotal / s_perfFrames;
                    const int avgObjRenderNear = s_objRenderNearTotal / s_perfFrames;
                    const int avgObjRenderMid = s_objRenderMidTotal / s_perfFrames;
                    const int avgObjRenderFar = s_objRenderFarTotal / s_perfFrames;
                    const int avgObjVisualRendered = s_objVisualRenderedTotal / s_perfFrames;
                    const int avgObjVisualDeferred = s_objVisualDeferredTotal / s_perfFrames;
                    const int avgObjRenderBaseCalls = s_objRenderBaseCallsTotal / s_perfFrames;
                    const int avgObjVisualCalls = s_objVisualCallsTotal / s_perfFrames;
                    const int avgObjRenderAfterCalls = s_objRenderAfterCallsTotal / s_perfFrames;
                    const int avgCharMoveCandidates = s_charMoveCandidatesTotal / s_perfFrames;
                    const int avgCharMoveUpdated = s_charMoveUpdatedTotal / s_perfFrames;
                    const int avgCharMoveDeferred = s_charMoveDeferredTotal / s_perfFrames;
                    const int avgCharMoveNear = s_charMoveNearTotal / s_perfFrames;
                    const int avgCharMoveMid = s_charMoveMidTotal / s_perfFrames;
                    const int avgCharMoveFar = s_charMoveFarTotal / s_perfFrames;
                    const int avgCharRenderCandidates = s_charRenderCandidatesTotal / s_perfFrames;
                    const int avgCharRenderRendered = s_charRenderRenderedTotal / s_perfFrames;
                    const int avgCharRenderDeferred = s_charRenderDeferredTotal / s_perfFrames;
                    const int avgCharRenderCulled = s_charRenderCulledTotal / s_perfFrames;
                    const int avgCharRenderNear = s_charRenderNearTotal / s_perfFrames;
                    const int avgCharRenderMid = s_charRenderMidTotal / s_perfFrames;
                    const int avgCharRenderFar = s_charRenderFarTotal / s_perfFrames;
                    const int avgCharRenderPlayers = s_charRenderPlayersTotal / s_perfFrames;
                    const int avgCharRenderMonsters = s_charRenderMonstersTotal / s_perfFrames;
                    const int avgCharMonsterObjectCalls = s_charMonsterObjectCallsTotal / s_perfFrames;
                    const int avgTerrainNormalBlocks = s_terrainNormalBlocksTotal / s_perfFrames;
                    const int avgTerrainNormalTiles = s_terrainNormalTilesTotal / s_perfFrames;
                    const int avgTerrainGrassBlocks = s_terrainGrassBlocksTotal / s_perfFrames;
                    const int avgTerrainGrassTiles = s_terrainGrassTilesTotal / s_perfFrames;
                    const int avgTerrainAfterBlocks = s_terrainAfterBlocksTotal / s_perfFrames;
                    const int avgTerrainAfterTiles = s_terrainAfterTilesTotal / s_perfFrames;
                    const double tickToMs = 1000.0 / static_cast<double>(s_perfFreq);
                    const double avgRenderSceneMs =
                        (static_cast<double>(s_perfRenderSceneTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgVirtualPadMs =
                        (static_cast<double>(s_perfVirtualPadTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgPresentMs =
                        (static_cast<double>(s_perfPresentTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgObjMoveMs =
                        (static_cast<double>(s_objMoveTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgObjRenderMs =
                        (static_cast<double>(s_objRenderTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgObjRenderBaseMs =
                        (static_cast<double>(s_objRenderBaseTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgObjRenderVisualMs =
                        (static_cast<double>(s_objRenderVisualTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgObjRenderAfterMs =
                        (static_cast<double>(s_objRenderAfterTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgCharMoveMs =
                        (static_cast<double>(s_charMoveTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgCharRenderMs =
                        (static_cast<double>(s_charRenderTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgCharMonsterObjectMs =
                        (static_cast<double>(s_charMonsterObjectTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgCharRenderShadowMs =
                        (static_cast<double>(s_charRenderShadowTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgCharRenderPostMs =
                        (static_cast<double>(s_charRenderPostTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgCharRenderAttachmentMs =
                        (static_cast<double>(s_charRenderAttachmentTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgTerrainRenderMs =
                        (static_cast<double>(s_terrainRenderTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgTerrainAfterMs =
                        (static_cast<double>(s_terrainAfterTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgMainShadowMs =
                        (static_cast<double>(s_mainShadowTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgMainBoidsMs =
                        (static_cast<double>(s_mainBoidsTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgMainMiscWorldMs =
                        (static_cast<double>(s_mainMiscWorldTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgMainJointsMs =
                        (static_cast<double>(s_mainJointsTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgMainEffectsMs =
                        (static_cast<double>(s_mainEffectsTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgMainBlursMs =
                        (static_cast<double>(s_mainBlursTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgMainSpritesMs =
                        (static_cast<double>(s_mainSpritesTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgMainParticlesMs =
                        (static_cast<double>(s_mainParticlesTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgMainPointsMs =
                        (static_cast<double>(s_mainPointsTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgMainAfterEffectsMs =
                        (static_cast<double>(s_mainAfterEffectsTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgMainUiMs =
                        (static_cast<double>(s_mainUiTicksTotal) * tickToMs) / static_cast<double>(s_perfFrames);
                    const double avgCameraDistance =
                        s_cameraDistanceTotal / static_cast<double>(s_perfFrames);
                    const double avgCameraTarget =
                        s_cameraDistanceTargetTotal / static_cast<double>(s_perfFrames);
                    const double avgCameraOverride =
                        s_cameraOverrideTotal / static_cast<double>(s_perfFrames);
                    const double avgCameraNear =
                        s_cameraViewNearTotal / static_cast<double>(s_perfFrames);
                    const double avgCameraFar =
                        s_cameraViewFarTotal / static_cast<double>(s_perfFrames);
                    const double avgCameraPitch =
                        s_cameraPitchTotal / static_cast<double>(s_perfFrames);
                    const double avgCameraYaw =
                        s_cameraYawTotal / static_cast<double>(s_perfFrames);
                    const double avgTrackedSceneMs =
                        avgObjMoveMs + avgObjRenderMs +
                        avgCharMoveMs + avgCharRenderMs +
                        avgTerrainRenderMs + avgTerrainAfterMs;
                    const double avgOtherSceneMs =
                        (avgRenderSceneMs > avgTrackedSceneMs) ? (avgRenderSceneMs - avgTrackedSceneMs) : 0.0;
                    const double avgObjRenderLoopMs =
                        (avgObjRenderMs > (avgObjRenderBaseMs + avgObjRenderVisualMs + avgObjRenderAfterMs))
                            ? (avgObjRenderMs - (avgObjRenderBaseMs + avgObjRenderVisualMs + avgObjRenderAfterMs))
                            : 0.0;
                    const double avgCharRenderOtherMs =
                        (avgCharRenderMs > (avgCharMonsterObjectMs + avgCharRenderShadowMs + avgCharRenderPostMs + avgCharRenderAttachmentMs))
                            ? (avgCharRenderMs - (avgCharMonsterObjectMs + avgCharRenderShadowMs + avgCharRenderPostMs + avgCharRenderAttachmentMs))
                            : 0.0;
                    const double avgPhaseTrackedMs =
                        avgMainShadowMs + avgMainBoidsMs + avgMainMiscWorldMs +
                        avgMainJointsMs + avgMainEffectsMs + avgMainBlursMs +
                        avgMainSpritesMs + avgMainParticlesMs + avgMainPointsMs +
                        avgMainAfterEffectsMs + avgMainUiMs;
                    const double avgPhaseRemainderMs =
                        (avgOtherSceneMs > avgPhaseTrackedMs) ? (avgOtherSceneMs - avgPhaseTrackedMs) : 0.0;
                    UpdateAdaptivePerformance(fps, avgFrameMs);
                    static int s_perfLogWindowCounter = 0;
                    if ((s_perfLogWindowCounter++ % 4) == 0)
                    {
                        const SceneCrowdSnapshot crowdSnapshot = CaptureSceneCrowdSnapshot();
                        PERF_LOGI(
                            "PERF fps=%.1f frameMs=%.2f phase(render=%.2f pad=%.2f present=%.2f) renderLevel=%d fxScale=%.2f maxMsg=%d avg(draw=%d verts=%d im=%d vaD=%d vaC=%d qI=%d qE=%d) max(draw=%d verts=%d)",
                            fps,
                            avgFrameMs,
                            avgRenderSceneMs,
                            avgVirtualPadMs,
                            avgPresentMs,
                            g_pOption ? ClampRenderLevel(g_pOption->GetRenderLevel()) : -1,
                            GetAdaptiveEffectSpawnScale(),
                            g_MaxMessagePerCycle,
                            avgDrawCalls,
                            avgVerts,
                            avgImDrawCalls,
                            avgVaDirect,
                            avgVaConverted,
                            avgQuadIndexed,
                            avgQuadExpanded,
                            s_perfDrawCallsMax,
                            s_perfVertsMax);

                        PERF_LOGI(
                            "PERF_SCENE world=%d char(live=%d vis=%d ply=%d mon=%d npc=%d pet=%d prio=%d topMon=%d x%d) obj(live=%d vis=%d op=%d trap=%d topObj=%d x%d)",
                            gMapManager.WorldActive,
                            crowdSnapshot.liveCharacters,
                            crowdSnapshot.visibleCharacters,
                            crowdSnapshot.visiblePlayers,
                            crowdSnapshot.visibleMonsters,
                            crowdSnapshot.visibleNpcs,
                            crowdSnapshot.visiblePets,
                            crowdSnapshot.visiblePriorityCharacters,
                            crowdSnapshot.dominantMonsterType,
                            crowdSnapshot.dominantMonsterCount,
                            crowdSnapshot.liveWorldObjects,
                            crowdSnapshot.visibleWorldObjects,
                            crowdSnapshot.visibleOperateObjects,
                            crowdSnapshot.visibleTrapObjects,
                            crowdSnapshot.dominantObjectType,
                            crowdSnapshot.dominantObjectCount);

                        PERF_LOGI(
                            "PERF_BUCKET objMove(cand=%d upd=%d def=%d n=%d m=%d f=%d) objRender(cand=%d draw=%d cull=%d n=%d m=%d f=%d visDraw=%d visDef=%d) charMove(cand=%d upd=%d def=%d n=%d m=%d f=%d) charRender(cand=%d draw=%d def=%d cull=%d n=%d m=%d f=%d)",
                            avgObjMoveCandidates,
                            avgObjMoveUpdated,
                            avgObjMoveDeferred,
                            avgObjMoveNear,
                            avgObjMoveMid,
                            avgObjMoveFar,
                            avgObjRenderCandidates,
                            avgObjRenderRendered,
                            avgObjRenderCulled,
                            avgObjRenderNear,
                            avgObjRenderMid,
                            avgObjRenderFar,
                            avgObjVisualRendered,
                            avgObjVisualDeferred,
                            avgCharMoveCandidates,
                            avgCharMoveUpdated,
                            avgCharMoveDeferred,
                            avgCharMoveNear,
                            avgCharMoveMid,
                            avgCharMoveFar,
                            avgCharRenderCandidates,
                            avgCharRenderRendered,
                            avgCharRenderDeferred,
                            avgCharRenderCulled,
                            avgCharRenderNear,
                            avgCharRenderMid,
                            avgCharRenderFar);

                        PERF_LOGI(
                            "PERF_CPU terrain=%.2f after=%.2f obj(move=%.2f render=%.2f) char(move=%.2f render=%.2f) other=%.2f",
                            avgTerrainRenderMs,
                            avgTerrainAfterMs,
                            avgObjMoveMs,
                            avgObjRenderMs,
                            avgCharMoveMs,
                            avgCharRenderMs,
                            avgOtherSceneMs);

                        PERF_LOGI(
                            "PERF_DETAIL obj(base=%.2f visual=%.2f after=%.2f loop=%.2f calls=%d/%d/%d) char(monObj=%.2f shadow=%.2f post=%.2f attach=%.2f other=%.2f draw=ply:%d mon:%d monObj:%d)",
                            avgObjRenderBaseMs,
                            avgObjRenderVisualMs,
                            avgObjRenderAfterMs,
                            avgObjRenderLoopMs,
                            avgObjRenderBaseCalls,
                            avgObjVisualCalls,
                            avgObjRenderAfterCalls,
                            avgCharMonsterObjectMs,
                            avgCharRenderShadowMs,
                            avgCharRenderPostMs,
                            avgCharRenderAttachmentMs,
                            avgCharRenderOtherMs,
                            avgCharRenderPlayers,
                            avgCharRenderMonsters,
                            avgCharMonsterObjectCalls);

                        PERF_LOGI(
                            "PERF_PHASE shadow=%.2f boids=%.2f misc=%.2f joints=%.2f effects=%.2f blurs=%.2f sprites=%.2f particles=%.2f points=%.2f afterfx=%.2f ui=%.2f rem=%.2f",
                            avgMainShadowMs,
                            avgMainBoidsMs,
                            avgMainMiscWorldMs,
                            avgMainJointsMs,
                            avgMainEffectsMs,
                            avgMainBlursMs,
                            avgMainSpritesMs,
                            avgMainParticlesMs,
                            avgMainPointsMs,
                            avgMainAfterEffectsMs,
                            avgMainUiMs,
                            avgPhaseRemainderMs);

                        PERF_LOGI(
                            "PERF_CAM zoom(cur=%.0f min=%.0f max=%.0f tgt=%.0f tmin=%.0f tmax=%.0f ov=%.0f) clip(near=%.0f far=%.0f fmin=%.0f fmax=%.0f) ang(p=%.1f pmin=%.1f pmax=%.1f y=%.1f ymin=%.1f ymax=%.1f) top=%d/%d settling=%d/%d",
                            avgCameraDistance,
                            s_cameraDistanceMin,
                            s_cameraDistanceMax,
                            avgCameraTarget,
                            s_cameraTargetMin,
                            s_cameraTargetMax,
                            avgCameraOverride,
                            avgCameraNear,
                            avgCameraFar,
                            s_cameraFarMin,
                            s_cameraFarMax,
                            avgCameraPitch,
                            s_cameraPitchMin,
                            s_cameraPitchMax,
                            avgCameraYaw,
                            s_cameraYawMin,
                            s_cameraYawMax,
                            s_cameraTopViewFrames,
                            s_perfFrames,
                            s_cameraSettlingFrames,
                            s_perfFrames);

                        PERF_LOGI(
                            "PERF_TERRAIN world=%d normal(block=%d tile=%d) grass(block=%d tile=%d) after(block=%d tile=%d)",
                            gMapManager.WorldActive,
                            avgTerrainNormalBlocks,
                            avgTerrainNormalTiles,
                            avgTerrainGrassBlocks,
                            avgTerrainGrassTiles,
                            avgTerrainAfterBlocks,
                            avgTerrainAfterTiles);
                    }
                    s_perfWindowStart = nowCounter;
                    s_perfFrames = 0;
                    s_perfDrawCallsTotal = 0;
                    s_perfVertsTotal = 0;
                    s_perfDrawCallsMax = 0;
                    s_perfVertsMax = 0;
                    s_perfImDrawCallsTotal = 0;
                    s_perfVaDirectTotal = 0;
                    s_perfVaConvertedTotal = 0;
                    s_perfQuadIndexedTotal = 0;
                    s_perfQuadExpandedTotal = 0;
                    s_objMoveCandidatesTotal = 0;
                    s_objMoveUpdatedTotal = 0;
                    s_objMoveDeferredTotal = 0;
                    s_objMoveNearTotal = 0;
                    s_objMoveMidTotal = 0;
                    s_objMoveFarTotal = 0;
                    s_objRenderCandidatesTotal = 0;
                    s_objRenderRenderedTotal = 0;
                    s_objRenderCulledTotal = 0;
                    s_objRenderNearTotal = 0;
                    s_objRenderMidTotal = 0;
                    s_objRenderFarTotal = 0;
                    s_objVisualRenderedTotal = 0;
                    s_objVisualDeferredTotal = 0;
                    s_objRenderBaseCallsTotal = 0;
                    s_objVisualCallsTotal = 0;
                    s_objRenderAfterCallsTotal = 0;
                    s_charMoveCandidatesTotal = 0;
                    s_charMoveUpdatedTotal = 0;
                    s_charMoveDeferredTotal = 0;
                    s_charMoveNearTotal = 0;
                    s_charMoveMidTotal = 0;
                    s_charMoveFarTotal = 0;
                    s_charRenderCandidatesTotal = 0;
                    s_charRenderRenderedTotal = 0;
                    s_charRenderDeferredTotal = 0;
                    s_charRenderCulledTotal = 0;
                    s_charRenderNearTotal = 0;
                    s_charRenderMidTotal = 0;
                    s_charRenderFarTotal = 0;
                    s_charRenderPlayersTotal = 0;
                    s_charRenderMonstersTotal = 0;
                    s_charMonsterObjectCallsTotal = 0;
                    s_terrainNormalBlocksTotal = 0;
                    s_terrainNormalTilesTotal = 0;
                    s_terrainGrassBlocksTotal = 0;
                    s_terrainGrassTilesTotal = 0;
                    s_terrainAfterBlocksTotal = 0;
                    s_terrainAfterTilesTotal = 0;
                    s_objMoveTicksTotal = 0;
                    s_objRenderTicksTotal = 0;
                    s_objRenderBaseTicksTotal = 0;
                    s_objRenderVisualTicksTotal = 0;
                    s_objRenderAfterTicksTotal = 0;
                    s_charMoveTicksTotal = 0;
                    s_charRenderTicksTotal = 0;
                    s_charMonsterObjectTicksTotal = 0;
                    s_charRenderShadowTicksTotal = 0;
                    s_charRenderPostTicksTotal = 0;
                    s_charRenderAttachmentTicksTotal = 0;
                    s_terrainRenderTicksTotal = 0;
                    s_terrainAfterTicksTotal = 0;
                    s_mainShadowTicksTotal = 0;
                    s_mainBoidsTicksTotal = 0;
                    s_mainMiscWorldTicksTotal = 0;
                    s_mainJointsTicksTotal = 0;
                    s_mainEffectsTicksTotal = 0;
                    s_mainBlursTicksTotal = 0;
                    s_mainSpritesTicksTotal = 0;
                    s_mainParticlesTicksTotal = 0;
                    s_mainPointsTicksTotal = 0;
                    s_mainAfterEffectsTicksTotal = 0;
                    s_mainUiTicksTotal = 0;
                    s_cameraDistanceTotal = 0.0;
                    s_cameraDistanceTargetTotal = 0.0;
                    s_cameraOverrideTotal = 0.0;
                    s_cameraViewNearTotal = 0.0;
                    s_cameraViewFarTotal = 0.0;
                    s_cameraPitchTotal = 0.0;
                    s_cameraYawTotal = 0.0;
                    s_cameraDistanceMin = (std::numeric_limits<float>::max)();
                    s_cameraDistanceMax = (std::numeric_limits<float>::lowest)();
                    s_cameraTargetMin = (std::numeric_limits<float>::max)();
                    s_cameraTargetMax = (std::numeric_limits<float>::lowest)();
                    s_cameraFarMin = (std::numeric_limits<float>::max)();
                    s_cameraFarMax = (std::numeric_limits<float>::lowest)();
                    s_cameraPitchMin = (std::numeric_limits<float>::max)();
                    s_cameraPitchMax = (std::numeric_limits<float>::lowest)();
                    s_cameraYawMin = (std::numeric_limits<float>::max)();
                    s_cameraYawMax = (std::numeric_limits<float>::lowest)();
                    s_cameraTopViewFrames = 0;
                    s_cameraSettlingFrames = 0;
                    s_perfRenderSceneTicksTotal = 0;
                    s_perfVirtualPadTicksTotal = 0;
                    s_perfPresentTicksTotal = 0;
                }

                // Debug: log first few frames to verify render loop runs
                static int s_dbgFrameCount = 0;
                if (s_dbgFrameCount < 10) {
                    const char* err = SDL_GetError();
                    LOGI("Frame %d 鑺掗埀顑解偓?drawCalls=%d verts=%d paths(im=%d vaD=%d vaC=%d qI=%d qE=%d) SDL_err='%s'",
                         s_dbgFrameCount,
                         frameDrawCalls,
                         frameVerts,
                         frameImDrawCalls,
                         frameVaDirect,
                         frameVaConverted,
                         frameQuadIndexed,
                         frameQuadExpanded,
                         err ? err : "");
                    SDL_ClearError();
                    ++s_dbgFrameCount;
                }
            }
            else
            {
                MU_MobileSleep(1);   // yield 鑺掗埀顑解偓?avoid busy loop
            }
        }
        else
        {
            MU_MobileSleep(16);  // minimize CPU when inactive
        }

        ProtocolCompiler();

        // Reset push states AFTER render so they're valid for one full frame
        MouseLButtonPush = false;
        MouseRButtonPush = false;
        MouseMButtonPush = false;
        MouseWheel       = 0;
    }

    LOGI("Main loop exited 鑺掗埀顑解偓?cleaning up");
    SaveVirtualSkillSlots();

    // 鑺掗垾婵冨亾鑺掗垾婵冨亾 Cleanup 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
    if (g_RenderBackend)
    {
        g_RenderBackend->Shutdown(g_SDLWindow);
        g_RenderBackend.reset();
    }

    DestroyWindow_Android();
    DestroySound();
    KillGLWindow();
    SDL_Quit();

    LOGI("=== MuMain shutdown complete ===");
    return 0;
}
#endif

sapp_desc sokol_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    sapp_desc desc {};
    desc.init_cb = OnAndroidSappInit;
    desc.frame_cb = OnAndroidSappFrame;
    desc.cleanup_cb = OnAndroidSappCleanup;
    desc.event_cb = OnAndroidSappEvent;
    desc.width = 1280;
    desc.height = 720;
    desc.fullscreen = true;
    // Render at 80% resolution per dimension (~64% of native pixel count)
    // rather than sokol_app's built-in high_dpi=false half-res mode (~25%
    // of pixels, too soft) - the OS compositor upscales it to fill the
    // screen. Touch coordinates are scaled by the same ratio internally by
    // sokol_app, so input mapping stays correct automatically.
    desc.high_dpi = true;
    desc.android_fb_scale = 0.8f;
    desc.window_title = "MU Online";
    desc.swap_interval = 0;
    desc.gl.major_version = 3;
    desc.gl.minor_version = 1;
    return desc;
}

static bool IsHealingHotKeyItemType(int itemType)
{
    return itemType >= ITEM_POTION + 0 && itemType <= ITEM_POTION + 3;
}

static bool IsManaHotKeyItemType(int itemType)
{
    return itemType >= ITEM_POTION + 4 && itemType <= ITEM_POTION + 6;
}

static bool IsShieldHotKeyItemType(int itemType)
{
    return itemType >= ITEM_POTION + 35 && itemType <= ITEM_POTION + 37;
}

static bool IsComplexHotKeyItemType(int itemType)
{
    return itemType >= ITEM_POTION + 38 && itemType <= ITEM_POTION + 40;
}
static bool IsSameHotKeyItemFamily(int lhsType, int rhsType)
{
    if (lhsType < 0 || rhsType < 0)
    {
        return false;
    }

    if (lhsType == rhsType)
    {
        return true;
    }

    return (IsHealingHotKeyItemType(lhsType) && IsHealingHotKeyItemType(rhsType))
        || (IsManaHotKeyItemType(lhsType) && IsManaHotKeyItemType(rhsType))
        || (IsShieldHotKeyItemType(lhsType) && IsShieldHotKeyItemType(rhsType))
        || (IsComplexHotKeyItemType(lhsType) && IsComplexHotKeyItemType(rhsType));
}

static std::array<int, SEASON3B::HOTKEY_COUNT> GetAutoBindHotKeyOrder(int itemType)
{
    if (IsManaHotKeyItemType(itemType))
    {
        return { SEASON3B::HOTKEY_W, SEASON3B::HOTKEY_Q, SEASON3B::HOTKEY_E, SEASON3B::HOTKEY_R };
    }

    if (IsShieldHotKeyItemType(itemType) || IsComplexHotKeyItemType(itemType))
    {
        return { SEASON3B::HOTKEY_Q, SEASON3B::HOTKEY_W, SEASON3B::HOTKEY_E, SEASON3B::HOTKEY_R };
    }

    if (IsHealingHotKeyItemType(itemType))
    {
        return { SEASON3B::HOTKEY_Q, SEASON3B::HOTKEY_W, SEASON3B::HOTKEY_E, SEASON3B::HOTKEY_R };
    }

    return { SEASON3B::HOTKEY_Q, SEASON3B::HOTKEY_W, SEASON3B::HOTKEY_E, SEASON3B::HOTKEY_R };
}

static int FindBestAutoBindHotKeySlot(int itemType, int itemLevel)
{
    if (g_pMainFrame == nullptr)
    {
        return -1;
    }

    const auto order = GetAutoBindHotKeyOrder(itemType);

    for (const int hotKey : order)
    {
        if (g_pMainFrame->GetItemHotKey(hotKey) == itemType
            && g_pMainFrame->GetItemHotKeyLevel(hotKey) == itemLevel)
        {
            return hotKey;
        }
    }

    for (const int hotKey : order)
    {
        if (g_pMainFrame->GetItemHotKey(hotKey) < 0)
        {
            return hotKey;
        }
    }

    for (const int hotKey : order)
    {
        if (IsSameHotKeyItemFamily(g_pMainFrame->GetItemHotKey(hotKey), itemType))
        {
            return hotKey;
        }
    }

    return order.front();
}

// 鑺掗垾婵冨亾鑺掗垾婵冨亾 Consumable potion slot binding 鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾鑺掗垾婵冨亾
// Binds directly to the legacy MU Q/W/E/R bar so the old mainframe UI stays
// authoritative for both rendering and use. Returns the mirror slot (0..3,
// see kVirtualMirrorHotKeys) it bound, or -1 on failure - the caller needs
// that to clear g_androidHotKeySlotCleared for the right slot.
int AndroidBindVirtualPotionSlotFromInventory(int itemType, int itemLevel)
{
    if (g_pMainFrame == nullptr
        || !SEASON3B::CNewUIMyInventory::CanRegisterItemHotKey(itemType))
    {
        return -1;
    }

    const int hotKey = FindBestAutoBindHotKeySlot(itemType, itemLevel);
    if (hotKey < 0)
    {
        return -1;
    }

    g_pMainFrame->SetItemHotKey(hotKey, itemType, itemLevel);
    LOGI("Android auto-bind consumable type=%d level=%d -> hotkey=%d", itemType, itemLevel, hotKey);

    for (int slot = 0; slot < kVirtualMirrorHotKeySlotCount; ++slot)
    {
        if (kVirtualMirrorHotKeys[slot] == hotKey)
        {
            return slot;
        }
    }
    return -1;
}

float GetAdaptiveEffectSpawnScale()
{
    const float fps = static_cast<float>(FPS);
    if (fps <= 0.0f || fps >= 26.0f)
    {
        return 1.0f;
    }
    if (fps >= 18.0f)
    {
        return 0.6f;
    }
    if (fps >= 12.0f)
    {
        return 0.4f;
    }
    return 0.25f;
}

bool ShouldThrottleAdaptiveEffectSpawn(int kind, int type, vec3_t Position, int SubType, float Scale, OBJECT* Owner)
{
    (void)kind;
    (void)type;
    (void)Position;
    (void)SubType;
    (void)Scale;
    (void)Owner;

    const float scale = GetAdaptiveEffectSpawnScale();
    if (scale >= 1.0f)
    {
        return false;
    }

    // Deterministic rotating counter rather than rand() so this stays cheap
    // and doesn't perturb any RNG state the gameplay code depends on.
    static uint32_t s_spawnCounter = 0;
    ++s_spawnCounter;
    const uint32_t threshold = static_cast<uint32_t>(scale * 1000.0f);
    return (s_spawnCounter * 2654435761u) % 1000u >= threshold;
}

#endif // __ANDROID__


