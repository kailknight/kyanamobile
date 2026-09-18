#ifdef __ANDROID__

#include "stdafx.h"
#include "APICB.h"
#include "DSPlaySound.h"
#include "WSctlc.h"
#include "wsctlc_addon.h"
#include "ZzzBMD.h"
#include "ZzzLodTerrain.h"
#include "SimpleModulus.h"
#include "Jpeglib.h"
#include "UIManager.h"
#include "ExternalObject/Leaf/xstreambuf.h"
#include "ExternalObject/curl/curl.h"
#include "TrayMode.h"
#include "Reconnect.h"
#include "ZzzScene.h"
#include "android/SimpleModulusCrypt.h"

#include <jni.h>
#include <android/log.h>
// SDL_mixer is deliberately absent: SDL never starts in this app (sokol_app
// replaces SDL_main), so every Mix_ call here was a no-op. Audio is Java side.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

extern "C" int32_t ConnectionManager_Connect(
    const wchar_t* host,
    int32_t port,
    BYTE isEncrypted,
    void(*onPacket)(int32_t, int32_t, uint8_t*),
    void(*onDisconnect)(int32_t));
extern "C" void ConnectionManager_Disconnect(int32_t handle);
extern "C" void ConnectionManager_Send(int32_t handle, const uint8_t* data, int32_t size);
extern "C" void SendServerListRequest(int32_t handle);

// Declared at file scope on purpose: inside the anonymous namespace below an
// extern declaration picks up internal linkage and cannot resolve.
void OpenSounds();

extern BOOL g_bGameServerConnected;

char* szServerIpAddress = (char*)"127.0.0.1";
std::string g_strSelectedML = "Eng";
int m_SavePassOnOff = 0;
unsigned short g_ServerPort = 63000;
BYTE Version[5] = { '1' + 1, '0' + 2, '4' + 3, '0' + 4, '5' + 5 };
BYTE Serial[17] = "TbYehR2hFUPBKgZj";
char m_ID[11] = {};
int mShowName = 1;
int mShowHPBar = 1;
int mShowMiniMap = 1;
int mShowDanhHieu = 1;
float g_androidZoomOverride = 0.0f;
HFONT g_hFontMini = NULL;

// ---- TEMPORARY CONNECTION DIAGNOSTIC - REMOVE WHEN SOLVED ----
// Which server the client is actually talking to. Global scope on purpose so
// the on-screen readout in ZzzScene.cpp can reach them.
char g_androidCurrentHost[64] = "-";
int  g_androidCurrentPort = 0;
int  g_androidCurrentIsGame = 0;
int  g_androidConnectCount = 0;

class CChatRoomSocketList;
CChatRoomSocketList* g_pChatRoomSocketList = nullptr;

extern int TextNum;
extern int g_iItemInfo[16][17];
extern int ItemHelp;

namespace SEASON3B
{
    float g_fScreenRate_x = 1.0f;
    float g_fScreenRate_y = 1.0f;
    int MouseY = 0;
    int SelectedCharacter = -1;
    int g_iCancelSkillTarget = 0;
    int g_iLengthAuthorityCode = 20;
    char TextList[60][512] = {};
    int TextListColor[60] = {};
    int TextBold[60] = {};
    int& TextNum = ::TextNum;
    int (&g_iItemInfo)[16][17] = ::g_iItemInfo;
    int& ItemHelp = ::ItemHelp;
    JewelHarmonyInfo*& g_pUIJewelHarmonyinfo = ::g_pUIJewelHarmonyinfo;
}

CTrayMode gTrayMode;

CTrayMode::CTrayMode()
{
    m_TrayIcon = NULL;
    m_MainWndProc = NULL;
}

void CTrayMode::Init(HINSTANCE) {}
void CTrayMode::Toggle() {}
LONG CTrayMode::GetMainWndProc() { return 0; }
void CTrayMode::ShowNotify(bool) {}
LRESULT CALLBACK CTrayMode::TrayModeWndProc(HWND, UINT, WPARAM, LPARAM) { return 0; }

extern "C" CURL* curl_easy_init(void) { return nullptr; }
extern "C" CURLcode curl_easy_setopt(CURL*, CURLoption, ...) { return CURLE_FAILED_INIT; }
extern "C" CURLcode curl_easy_perform(CURL*) { return CURLE_FAILED_INIT; }
extern "C" void curl_easy_cleanup(CURL*) {}

void SetAPIDATA_SEND(DATA_SEND) {}
void CBAnihack_Work() {}
void CBAnihack_Init() {}
void CBAnihack_Attack() {}
void CBAnihack_Recv(BYTE*) {}

namespace
{
constexpr long kAndroidDsVolumeMin = -10000;
constexpr long kAndroidDsVolumeMax = 0;

bool g_androidSoundEnabled = true;
long g_androidMasterVolume = 0;

// Silenced because the window lost focus - a floating window the player tapped
// outside of, or the notification shade over a fullscreen one. Deliberately a
// separate flag from g_androidSoundEnabled, which is the player's own sound
// setting: this one has to be able to come and go without overwriting, or
// being overwritten by, what they chose in the options.
bool g_androidFocusMuted = false;

// Defined further down, next to the rest of the JNI bridge. Declared here
// because the volume and enable/disable entry points sit above it and an
// anonymous namespace is one namespace across the whole file.
void CallMuAudioVoid(const char* method, const char* sig, ...);

// The engine speaks DirectSound's scale: hundredths of a decibel, -10000
// silent and 0 full. MuAudio wants a plain 0-100 percentage.
int ConvertDirectSoundVolumeToPercent(long volume)
{
    if (!g_androidSoundEnabled || g_androidFocusMuted)
    {
        return 0;
    }

    volume = std::clamp<long>(volume, kAndroidDsVolumeMin, kAndroidDsVolumeMax);

    if (volume <= kAndroidDsVolumeMin)
    {
        return 0;
    }

    if (volume >= kAndroidDsVolumeMax)
    {
        return 100;
    }

    const double gain = std::pow(10.0, static_cast<double>(volume) / 2000.0);
    return std::clamp(static_cast<int>(std::lround(gain * 100.0)), 0, 100);
}

void ApplyAndroidMasterVolume()
{
    CallMuAudioVoid("setVolume", "(I)V",
        static_cast<jint>(ConvertDirectSoundVolumeToPercent(g_androidMasterVolume)));
}

bool ShouldSuppressAndroidRuntimeLog(const char* format)
{
    if (format == nullptr)
    {
        return true;
    }

    static const char* kParsedPacketPrefix = "[Android Socket] parsed packet";
    static const char* kSendPacketPrefix = "[Android Socket] send packet";
    static const char* kOpenMonsterPrefix = "OpenMonsterModel(";

    return
        std::strncmp(format, kParsedPacketPrefix, std::strlen(kParsedPacketPrefix)) == 0 ||
        std::strncmp(format, kSendPacketPrefix, std::strlen(kSendPacketPrefix)) == 0 ||
        std::strncmp(format, kOpenMonsterPrefix, std::strlen(kOpenMonsterPrefix)) == 0;
}
}

DWORD API_TypeMain = 0;
DWORD API_CharLevel = 0;
DWORD API_CharWeaponFirstSlot = 0;
DWORD API_CharWeaponSecondSlot = 0;
DWORD API_CharHelmSlot = 0;
DWORD API_CharArmorSlot = 0;
DWORD API_CharPantsSlot = 0;
DWORD API_CharGlovesSlot = 0;
DWORD API_CharBootsSlot = 0;
DWORD API_CharWingsSlot = 0;
DWORD API_CharPetSlot = 0;
DWORD API_CharAnimationID = 0;
DWORD API_CharActiveMagic = 0;
DWORD* API_GameState = nullptr;
DWORD* API_FrameValue = nullptr;
DWORD* API_SpeedValue = nullptr;
DWORD* API_MainTickCount = nullptr;
DWORD* API_SyncTickCount = nullptr;
DWORD* API_CountModifier = nullptr;
DWORD* API_DelayModifier = nullptr;
DWORD* API_HasteModifier = nullptr;
DWORD* API_SleepModifier = nullptr;
DWORD* API_SpeedModifier1 = nullptr;
DWORD* API_SpeedModifier2 = nullptr;
DWORD* API_ModelModifier1 = nullptr;
DWORD* API_ModelModifier2 = nullptr;
DWORD* API_ModelModifier3 = nullptr;
DWORD* API_ViewPoint = nullptr;
DWORD* API_ViewStrength = nullptr;
DWORD* API_ViewDexterity = nullptr;
DWORD* API_ViewVitality = nullptr;
DWORD* API_ViewEnergy = nullptr;
DWORD* API_ViewLeadership = nullptr;
DWORD* API_ViewAddStrength = nullptr;
DWORD* API_ViewAddDexterity = nullptr;
DWORD* API_ViewAddVitality = nullptr;
DWORD* API_ViewAddEnergy = nullptr;
DWORD* API_ViewAddLeadership = nullptr;
DWORD* API_ViewPhysiSpeed = nullptr;
DWORD* API_ViewMagicSpeed = nullptr;

HRESULT InitDirectSound(HWND) { return S_OK; }
void SetEnableSound(bool enabled)
{
    g_androidSoundEnabled = enabled;
    CallMuAudioVoid("setEnabled", "(Z)V", enabled ? JNI_TRUE : JNI_FALSE);
    if (enabled)
    {
        ApplyAndroidMasterVolume();
    }
}
void FreeDirectSound()
{
    g_androidSoundEnabled = false;
    CallMuAudioVoid("stopAll", "()V");
    CallMuAudioVoid("stopMusic", "()V");
}
// ---------------------------------------------------------------------------
// JNI bridge to MuAudio (Java)
//
// SDL_mixer cannot be used here - see MuAudio.java for why - so effects and
// music go through SoundPool and MediaPlayer instead. JNI_OnLoad captures the
// VM; the game thread is not a Java thread, so calls attach on demand.
// ---------------------------------------------------------------------------
namespace
{
    JavaVM*  g_androidJavaVm = nullptr;
    jclass   g_muAudioClass = nullptr;

    JNIEnv* AndroidAudioEnv()
    {
        if (g_androidJavaVm == nullptr)
        {
            return nullptr;
        }

        JNIEnv* env = nullptr;
        if (g_androidJavaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK)
        {
            return env;
        }
        // The render thread is created by native code, so it is not attached.
        if (g_androidJavaVm->AttachCurrentThread(&env, nullptr) == JNI_OK)
        {
            return env;
        }
        return nullptr;
    }

    void CallMuAudioVoid(const char* method, const char* sig, ...)
    {
        JNIEnv* env = AndroidAudioEnv();
        if (env == nullptr || g_muAudioClass == nullptr)
        {
            return;
        }

        jmethodID id = env->GetStaticMethodID(g_muAudioClass, method, sig);
        if (id == nullptr)
        {
            env->ExceptionClear();
            return;
        }

        va_list args;
        va_start(args, sig);
        env->CallStaticVoidMethodV(g_muAudioClass, id, args);
        va_end(args);

        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
        }
    }
}

// Attached from an existing JNI entry point rather than JNI_OnLoad.
// Adding a JNI_OnLoad to this library broke the soft keyboard: the library had
// never defined one, and introducing it at System.loadLibrary time disturbed
// the JNI setup the keyboard bridge in MuMainNativeActivity depends on.
// nativeSetKeyboardBridge already runs during activity setup and is known good,
// so the VM and class reference are captured from there instead.
extern "C" void AndroidAudioAttachJni(JNIEnv* env)
{
    if (env == nullptr || g_androidJavaVm != nullptr)
    {
        return;
    }

    if (env->GetJavaVM(&g_androidJavaVm) != JNI_OK)
    {
        g_androidJavaVm = nullptr;
        return;
    }

    jclass local = env->FindClass("com/muonline/client/MuAudio");
    if (local == nullptr)
    {
        env->ExceptionClear();
        return;
    }
    g_muAudioClass = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
}

// ---------------------------------------------------------------------------
// Sound effects
//
// Two separate faults kept this silent, and the second hid the first.
//
// LoadWaveFile and PlayBuffer were empty stubs, so the ~826 WAVs shipped in
// Data/Sound were never touched. Implementing them was not enough, because
// PlatformDefs.h also carried an inline no-op overload of LoadWaveFile taking
// const char*. Every call site passes a string literal, which since C++11
// cannot bind to the real function's writable TCHAR*, so the no-op was an
// exact match and won overload resolution at all ~950 call sites: OpenSounds()
// compiled down to almost nothing and the registration count stayed at 0 no
// matter what happened below it. That overload now forwards here instead.
//
// Registration records the path only; SoundPool decodes on first play. Loading
// all ~826 eagerly would put their PCM resident before the title screen, and
// most of the table is monsters a given session never meets.
// ---------------------------------------------------------------------------
namespace
{
    // MAX_BUFFER is the engine's own sound count (DSPlaySound.h). This was a
    // hardcoded 512, which silently dropped every id above it at registration -
    // the device log showed play requests for id=690..697 that could never have
    // a path, so those sounds were mute even after the rest of the chain worked.
    constexpr int kAndroidSoundSlotCount = MAX_BUFFER;

    // The tables use Windows paths - "Data\Sound\aWind.wav". Separators have
    // to become '/', and the result has to be absolute: SoundPool.load resolves
    // a relative path against the JVM's user.dir, which is "/" for an app
    // process, not against the working directory the client chdir's to at
    // startup. fopen() on the same relative string succeeds here, which is what
    // makes this an easy one to miss.
    std::string NormalizeAndroidSoundPath(const TCHAR* raw)
    {
        if (raw == nullptr)
        {
            return std::string();
        }

        std::string path(raw);
        for (char& c : path)
        {
            if (c == '\\')
            {
                c = '/';
            }
        }

        if (path.empty() || path[0] == '/')
        {
            return path;
        }

        char cwd[1024] = {};
        if (getcwd(cwd, sizeof(cwd)) == nullptr)
        {
            return path;
        }

        std::string absolute(cwd);
        if (absolute.empty() || absolute.back() != '/')
        {
            absolute.push_back('/');
        }
        absolute += path;
        return absolute;
    }

    // logcat returns nothing on these retail devices, so the sound path reports
    // to a file instead. Same approach as mu_drift_log.txt.
    int g_androidSoundRegistered = 0;
    int g_androidSoundPlayRequests = 0;

    void AndroidSoundLog(const char* fmt, ...)
    {
        static int s_lines = 0;
        if (s_lines >= 60)
        {
            return;
        }
        ++s_lines;

        if (FILE* f = fopen("mu_sound_log.txt", "a"))
        {
            va_list args;
            va_start(args, fmt);
            vfprintf(f, fmt, args);
            va_end(args);
            fputc('\n', f);
            fclose(f);
        }
    }

    // Length of a PCM WAV in milliseconds, read from its header.
    //
    // The engine gives each sound one DirectSound buffer and never advances the
    // channel index (`BufferChannel[Buffer]` is set to 0 in DSplaysound.cpp and
    // only ever wrapped back to it), and DirectSound's Play() on a buffer that
    // is already playing is a no-op. So no sound can overlap itself, looped or
    // not. SoundPool has no such rule - it starts a new stream per call - and
    // the scene update asks for the ambient beds every frame, which is what
    // turned Noria's forest and wind into a drone. Java needs the length to
    // know when a sound has finished and may be triggered again.
    //
    // Returns 0 if the header cannot be parsed, which lets that sound retrigger
    // freely rather than falling silent.
    int AndroidWavDurationMs(const std::string& path)
    {
        FILE* f = fopen(path.c_str(), "rb");
        if (f == nullptr)
        {
            return 0;
        }

        unsigned char header[12];
        if (fread(header, 1, sizeof(header), f) != sizeof(header)
            || std::memcmp(header, "RIFF", 4) != 0
            || std::memcmp(header + 8, "WAVE", 4) != 0)
        {
            fclose(f);
            return 0;
        }

        auto le32 = [](const unsigned char* p) -> uint32_t {
            return static_cast<uint32_t>(p[0])
                 | (static_cast<uint32_t>(p[1]) << 8)
                 | (static_cast<uint32_t>(p[2]) << 16)
                 | (static_cast<uint32_t>(p[3]) << 24);
        };

        uint32_t byteRate = 0;
        int durationMs = 0;
        unsigned char chunk[8];
        while (fread(chunk, 1, sizeof(chunk), f) == sizeof(chunk))
        {
            const uint32_t size = le32(chunk + 4);

            if (std::memcmp(chunk, "fmt ", 4) == 0)
            {
                unsigned char fmt[16];
                const size_t want = (size < sizeof(fmt)) ? size : sizeof(fmt);
                if (fread(fmt, 1, want, f) != want)
                {
                    break;
                }
                if (want == sizeof(fmt))
                {
                    byteRate = le32(fmt + 8);
                }
                if (size > want)
                {
                    fseek(f, static_cast<long>(size - want), SEEK_CUR);
                }
            }
            else if (std::memcmp(chunk, "data", 4) == 0)
            {
                if (byteRate > 0)
                {
                    durationMs = static_cast<int>((static_cast<uint64_t>(size) * 1000ULL) / byteRate);
                }
                break;
            }
            else
            {
                fseek(f, static_cast<long>(size), SEEK_CUR);
            }

            // RIFF chunks are word aligned.
            if ((size & 1u) != 0)
            {
                fseek(f, 1, SEEK_CUR);
            }
        }

        fclose(f);
        return durationMs;
    }

    // The table is held here first and handed to Java in one pass, rather than
    // each entry going across as it is registered. Registration runs on the
    // render thread while the JNI bridge is attached from the activity's
    // onCreate on the UI thread, so a per-entry push could land before the
    // bridge exists and be dropped without trace - the same silent-failure
    // shape as the overload bug this replaced.
    std::string g_androidSoundPaths[kAndroidSoundSlotCount];
    bool g_androidSoundTablePushed = false;

    bool PushAndroidSoundTable()
    {
        if (g_androidSoundTablePushed)
        {
            return true;
        }

        JNIEnv* env = AndroidAudioEnv();
        if (env == nullptr || g_muAudioClass == nullptr)
        {
            return false;
        }

        // One pass over the table: read each header for its length and treat a
        // file that will not open as missing. SoundPool.load failures go to
        // logcat, which returns nothing on these devices, so a bad path would
        // otherwise look exactly like "no sound" all over again.
        int pushed = 0;
        int missing = 0;
        for (int id = 0; id < kAndroidSoundSlotCount; ++id)
        {
            if (g_androidSoundPaths[id].empty())
            {
                continue;
            }

            const int durationMs = AndroidWavDurationMs(g_androidSoundPaths[id]);
            if (durationMs == 0)
            {
                if (missing < 5)
                {
                    AndroidSoundLog("NOHEADER id=%d path=[%s]", id, g_androidSoundPaths[id].c_str());
                }
                ++missing;
            }

            jstring jpath = env->NewStringUTF(g_androidSoundPaths[id].c_str());
            if (jpath == nullptr)
            {
                continue;
            }
            CallMuAudioVoid("register", "(ILjava/lang/String;I)V",
                static_cast<jint>(id), jpath, static_cast<jint>(durationMs));
            env->DeleteLocalRef(jpath);
            ++pushed;
        }

        g_androidSoundTablePushed = true;
        AndroidSoundLog("PUSHED registered=%d pushed=%d noheader=%d",
            g_androidSoundRegistered, pushed, missing);

        // The volume set during init would have been dropped for the same
        // reason if the bridge was not up yet.
        ApplyAndroidMasterVolume();
        return true;
    }
}

void LoadWaveFile(int Buffer, TCHAR* strFileName, int, bool)
{
    if (Buffer < 0 || Buffer >= kAndroidSoundSlotCount)
    {
        return;
    }

    g_androidSoundPaths[Buffer] = NormalizeAndroidSoundPath(strFileName);
    ++g_androidSoundRegistered;
    if (g_androidSoundRegistered <= 3)
    {
        AndroidSoundLog("REGISTER id=%d path=[%s] enabled=%d vol=%ld",
            Buffer, g_androidSoundPaths[Buffer].c_str(),
            g_androidSoundEnabled ? 1 : 0, g_androidMasterVolume);
    }
}

// Object is ignored: it carries the emitter's world position for the 3D mixing
// the desktop client does. SoundPool has no positional equivalent worth the
// CPU here on a client that is already CPU-bound. Sounds play centred.
HRESULT PlayBuffer(int Buffer, OBJECT*, BOOL bLooped)
{
    if (!g_androidSoundEnabled)
    {
        return S_OK;
    }

    // Normally a no-op: the table went across during init. This is the retry
    // for the case where the JNI bridge was not attached yet at that point.
    if (!PushAndroidSoundTable())
    {
        return S_OK;
    }

    ++g_androidSoundPlayRequests;
    if (g_androidSoundPlayRequests <= 3 || (g_androidSoundPlayRequests % 200) == 0)
    {
        AndroidSoundLog("PLAY id=%d enabled=%d registered=%d",
            Buffer, g_androidSoundEnabled ? 1 : 0, g_androidSoundRegistered);
    }

    CallMuAudioVoid("play", "(IZ)V", static_cast<jint>(Buffer),
        bLooped ? JNI_TRUE : JNI_FALSE);
    return S_OK;
}

// Named sound, not everything: the looping ambient and walk sounds are stopped
// individually, and halting the whole pool here would cut off every other sound
// mixing at that moment.
void StopBuffer(int Buffer, BOOL)
{
    CallMuAudioVoid("stop", "(I)V", static_cast<jint>(Buffer));
}
void AllStopSound(void) { CallMuAudioVoid("stopAll", "()V"); }
void Set3DSoundPosition() {}
HRESULT ReleaseBuffer(int) { return S_OK; }
HRESULT RestoreBuffers(int, int) { return S_OK; }
void SetVolume(int, long vol)
{
    g_androidMasterVolume = std::clamp<long>(vol, kAndroidDsVolumeMin, kAndroidDsVolumeMax);
    ApplyAndroidMasterVolume();
}
void SetMasterVolume(long vol)
{
    g_androidMasterVolume = std::clamp<long>(vol, kAndroidDsVolumeMin, kAndroidDsVolumeMax);
    ApplyAndroidMasterVolume();
}

// ---------------------------------------------------------------------------
// Music
//
// PlayMp3/StopMp3 in android_main.cpp used to drive Mix_LoadMUS against a mixer
// that never opened. These give it MediaPlayer instead. Paths arriving here are
// already absolute - android_main resolves them against the data root.
// ---------------------------------------------------------------------------
extern "C" void AndroidAudioPlayMusic(const char* absolutePath, bool loop)
{
    if (absolutePath == nullptr || absolutePath[0] == '\0')
    {
        return;
    }

    JNIEnv* env = AndroidAudioEnv();
    if (env == nullptr || g_muAudioClass == nullptr)
    {
        return;
    }

    jstring jpath = env->NewStringUTF(absolutePath);
    if (jpath == nullptr)
    {
        return;
    }
    CallMuAudioVoid("playMusic", "(Ljava/lang/String;Z)V", jpath,
        loop ? JNI_TRUE : JNI_FALSE);
    env->DeleteLocalRef(jpath);
}

extern "C" void AndroidAudioStopMusic()
{
    CallMuAudioVoid("stopMusic", "()V");
}

// Mute, not stop: MuAudio.setVolume walks the live SoundPool streams and the
// MediaPlayer and only changes their gain, so the map's music keeps its
// position and comes back where it left off when focus returns. Stopping it
// instead would restart the track from the top every time the player tapped
// away from a floating window.
//
// Called from the UI thread via the focus bridge. Safe from there: MuAudio's
// setVolume is static synchronized, and AndroidAudioEnv attaches whatever
// thread asks.
extern "C" void AndroidAudioSetFocusMuted(bool muted)
{
    if (g_androidFocusMuted == muted)
    {
        return;
    }

    g_androidFocusMuted = muted;
    ApplyAndroidMasterVolume();
}

extern "C" bool AndroidAudioIsMusicPlaying()
{
    JNIEnv* env = AndroidAudioEnv();
    if (env == nullptr || g_muAudioClass == nullptr)
    {
        return false;
    }

    jmethodID id = env->GetStaticMethodID(g_muAudioClass, "isMusicPlaying", "()Z");
    if (id == nullptr)
    {
        env->ExceptionClear();
        return false;
    }

    const jboolean playing = env->CallStaticBooleanMethod(g_muAudioClass, id);
    if (env->ExceptionCheck())
    {
        env->ExceptionClear();
        return false;
    }
    return playing == JNI_TRUE;
}

// Brings the Java side up and registers the whole sound table. Called from
// android_main once the data root is in place.
extern "C" void AndroidAudioInit()
{
    CallMuAudioVoid("init", "()V");

    // Declared at the top of this file. Redeclaring it here would give it C
    // linkage from the enclosing extern "C" and not match the definition.
    OpenSounds();

    const bool pushed = PushAndroidSoundTable();
    AndroidSoundLog("OPENSOUNDS registered=%d pushed=%d jvm=%d cls=%d",
        g_androidSoundRegistered,
        pushed ? 1 : 0,
        g_androidJavaVm != nullptr ? 1 : 0,
        g_muAudioClass != nullptr ? 1 : 0);

    // File presence is checked inside PushAndroidSoundTable, which already
    // opens each one to read its length - see the noheader= count it logs.
}

static BYTE g_wsctlcReadMessage[MAX_RECVBUF] = {};
namespace
{
    constexpr DWORD kSimpleModulusSaveLoadXor[SIZE_ENCRYPTION_KEY] = {
        0x3F08A79B,
        0xE25CC287,
        0x93D27AB9,
        0x20DEA7BF,
    };

    constexpr DWORD kTakumiModulus[SIZE_ENCRYPTION_KEY] = { 95770, 84999, 76807, 71847 };
    constexpr DWORD kTakumiEncryptKey[SIZE_ENCRYPTION_KEY] = { 29511, 17162, 10281, 17092 };
    constexpr DWORD kTakumiDecryptKey[SIZE_ENCRYPTION_KEY] = { 17521, 13625, 21598, 26083 };
    constexpr DWORD kTakumiXorKey[SIZE_ENCRYPTION_KEY] = { 24429, 15164, 17929, 12915 };

    std::mutex g_androidSocketMutex;
    std::unordered_map<int32_t, CWsctlc*> g_androidSocketOwners;

    // Set on the receive thread when the GameServer connection drops, drained
    // on the main thread by AndroidPumpPendingReconnect.
    std::atomic<bool> g_androidReconnectOnClosePending{ false };

    std::wstring ToWideString(const char* text)
    {
        std::wstring wide;
        if (!text)
        {
            return wide;
        }

        while (*text)
        {
            wide.push_back(static_cast<unsigned char>(*text));
            ++text;
        }

        return wide;
    }

    bool IsConnectServerPort(unsigned short port)
    {
        return (port == g_ServerPort || port == 63000);
    }

    void CopyPacketKey(DWORD* target, const DWORD* source)
    {
        if (target && source)
        {
            std::memcpy(target, source, sizeof(DWORD) * SIZE_ENCRYPTION_KEY);
        }
    }

    void SyncClientKey(const DWORD* modulus, const DWORD* key, const DWORD* xorKey)
    {
        mu::PacketKeySet keySet {};
        for (int i = 0; i < SIZE_ENCRYPTION_KEY; ++i)
        {
            keySet.modulus[static_cast<size_t>(i)] = modulus[i];
            keySet.key[static_cast<size_t>(i)] = key[i];
            keySet.xorKey[static_cast<size_t>(i)] = xorKey[i];
        }
        mu::SetClientToServerKey(keySet);
    }

    void SyncServerKey(const DWORD* modulus, const DWORD* key, const DWORD* xorKey)
    {
        mu::PacketKeySet keySet {};
        for (int i = 0; i < SIZE_ENCRYPTION_KEY; ++i)
        {
            keySet.modulus[static_cast<size_t>(i)] = modulus[i];
            keySet.key[static_cast<size_t>(i)] = key[i];
            keySet.xorKey[static_cast<size_t>(i)] = xorKey[i];
        }
        mu::SetServerToClientKey(keySet);
    }

    bool LoadSimpleModulusKeyFile(const char* path, DWORD* outModulus, DWORD* outKey, DWORD* outXorKey)
    {
        if (!path || !outModulus || !outKey || !outXorKey)
        {
            return false;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return false;
        }

        ChunkHeader header {};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (!file || header.m_sID != CHUNKID_ONEKEY || header.m_iSize != (sizeof(ChunkHeader) + sizeof(DWORD) * SIZE_ENCRYPTION_KEY * 3))
        {
            return false;
        }

        DWORD table[SIZE_ENCRYPTION_KEY] = {};

        for (int section = 0; section < 3; ++section)
        {
            file.read(reinterpret_cast<char*>(table), sizeof(table));
            if (!file)
            {
                return false;
            }

            for (int i = 0; i < SIZE_ENCRYPTION_KEY; ++i)
            {
                table[i] ^= kSimpleModulusSaveLoadXor[i];
            }

            if (section == 0)
            {
                std::memcpy(outModulus, table, sizeof(table));
            }
            else if (section == 1)
            {
                std::memcpy(outKey, table, sizeof(table));
            }
            else
            {
                std::memcpy(outXorKey, table, sizeof(table));
            }
        }

        return true;
    }

}

CWsctlc::CWsctlc()
{
    m_hWnd = NULL;
    m_bGame = FALSE;
    m_iMaxSockets = 0;
    m_socket = INVALID_SOCKET;
    std::memset(m_SendBuf, 0, sizeof(m_SendBuf));
    m_nSendBufLen = 0;
    std::memset(m_RecvBuf, 0, sizeof(m_RecvBuf));
    m_nRecvBufLen = 0;
    m_LogPrint = 0;
    m_logfp = NULL;
    m_pPacketQueue = new CPacketQueue;
}

CWsctlc::~CWsctlc()
{
    Close();
    delete m_pPacketQueue;
    m_pPacketQueue = nullptr;
}

void CWsctlc::AndroidClearPacketQueue()
{
    if (m_pPacketQueue == nullptr)
    {
        return;
    }

    while (!m_pPacketQueue->IsEmpty())
    {
        m_pPacketQueue->PopPacket();
    }

    m_pPacketQueue->ClearGarbage();
}

void CWsctlc::AndroidOnPacket(int32_t handle, int32_t size, uint8_t* data)
{
    if (handle <= 0 || size <= 0 || data == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(g_androidSocketMutex);
    const auto it = g_androidSocketOwners.find(handle);
    if (it == g_androidSocketOwners.end() || it->second == nullptr || it->second->m_pPacketQueue == nullptr)
    {
        return;
    }

    CWsctlc* client = it->second;

    if (size > MAX_RECVBUF || client->m_nRecvBufLen < 0 || client->m_nRecvBufLen >= MAX_RECVBUF)
    {
        g_ErrorReport.Write("[Android Socket] packet too large size=%d handle=%d\r\n", size, handle);
        return;
    }

    if ((client->m_nRecvBufLen + size) > MAX_RECVBUF)
    {
        g_ErrorReport.Write(
            "[Android Socket] recv buffer overflow size=%d current=%d handle=%d\r\n",
            size,
            client->m_nRecvBufLen,
            handle);
        client->m_nRecvBufLen = 0;
        return;
    }

    std::memcpy(client->m_RecvBuf + client->m_nRecvBufLen, data, static_cast<size_t>(size));

#if(ENCRYPT_STATE==1)
    if (gProtect.CheckSocketPort(client->m_socket) != 0)
    {
        gProtect.DecryptData(client->m_RecvBuf + client->m_nRecvBufLen, size);
    }
#endif

    client->m_nRecvBufLen += size;

    if (client->m_nRecvBufLen < 3)
    {
        return;
    }

    int offset = 0;
    int packetSize = 0;

    while (1)
    {
        if (client->m_RecvBuf[offset] == 0xC1 || client->m_RecvBuf[offset] == 0xC3)
        {
            packetSize = static_cast<int>(client->m_RecvBuf[offset + 1]);
        }
        else if (client->m_RecvBuf[offset] == 0xC2 || client->m_RecvBuf[offset] == 0xC4)
        {
            packetSize = (static_cast<int>(client->m_RecvBuf[offset + 1]) << 8) + client->m_RecvBuf[offset + 2];
        }
        else
        {
            g_ErrorReport.Write(
                "[Android Socket] packet sync lost handle=%d first=0x%02X len=%d\r\n",
                handle,
                client->m_RecvBuf[offset],
                client->m_nRecvBufLen);
            client->m_nRecvBufLen = 0;
            return;
        }

        if (packetSize <= 0)
        {
            g_ErrorReport.Write("[Android Socket] invalid packet size=%d handle=%d\r\n", packetSize, handle);
            client->m_nRecvBufLen = 0;
            return;
        }
        else if (packetSize <= client->m_nRecvBufLen)
        {
#if defined(MU_ANDROID_VERBOSE_NET_LOG)
            g_ErrorReport.Write(
                "[Android Socket] parsed packet handle=%d type=0x%02X size=%d head=0x%02X sub=0x%02X\r\n",
                handle,
                client->m_RecvBuf[offset],
                packetSize,
                (packetSize > 2) ? client->m_RecvBuf[offset + 2] : 0,
                (packetSize > 3) ? client->m_RecvBuf[offset + 3] : 0);
#endif
            client->m_pPacketQueue->PushPacket(client->m_RecvBuf + offset, packetSize);
            offset += packetSize;
            client->m_nRecvBufLen -= packetSize;

            if (client->m_nRecvBufLen <= 0)
            {
                break;
            }
        }
        else
        {
            if (offset > 0 && client->m_nRecvBufLen > 0)
            {
                std::memmove(client->m_RecvBuf, client->m_RecvBuf + offset, static_cast<size_t>(client->m_nRecvBufLen));
            }
            break;
        }
    }

    client->m_pPacketQueue->ClearGarbage();
}

void CWsctlc::AndroidOnDisconnect(int32_t handle)
{
    std::lock_guard<std::mutex> lock(g_androidSocketMutex);
    const auto it = g_androidSocketOwners.find(handle);
    if (it == g_androidSocketOwners.end())
    {
        return;
    }

    CWsctlc* client = it->second;
    g_androidSocketOwners.erase(it);

    if (client != nullptr)
    {
        client->m_socket = INVALID_SOCKET;
        client->m_nRecvBufLen = 0;
        client->m_nSendBufLen = 0;

        if (client->m_bGame)
        {
            g_bGameServerConnected = FALSE;

            // The Windows CWsctlc::Close() kicks the reconnect system here. This
            // path never did, so losing the GameServer just dropped the player.
            // It cannot be called from here though - this runs on the receive
            // thread, under g_androidSocketMutex - so hand it to the main thread.
            g_androidReconnectOnClosePending.store(true, std::memory_order_release);
        }
    }
}

// Drained once per frame from the main thread, next to ReconnectMainProc.
void AndroidPumpPendingReconnect()
{
#if(UseReconnect)
    if (!g_androidReconnectOnClosePending.exchange(false, std::memory_order_acq_rel))
    {
        return;
    }

    if (SceneFlag != MAIN_SCENE)
    {
        return;
    }

    // CheckSocketPort falls back to the port cached when the connection was
    // made, so a socket that is already gone still passes.
    if (g_pReconnect != nullptr && g_pReconnect->CheckSocketPort(INVALID_SOCKET))
    {
        g_pReconnect->ReconnectOnCloseSocket();
    }
#endif
}

SOCKET CWsctlc::GetSocket()
{
    return m_socket;
}

BOOL CWsctlc::Startup()
{
    m_socket = INVALID_SOCKET;
    m_iMaxSockets = 1;
    return TRUE;
}

void CWsctlc::Cleanup()
{
}

int CWsctlc::Create(HWND hWnd, BOOL bGame)
{
    m_hWnd = hWnd;
    m_bGame = bGame;
    m_socket = INVALID_SOCKET;
    m_nSendBufLen = 0;
    m_nRecvBufLen = 0;

    if (m_bGame)
    {
        g_bGameServerConnected = FALSE;
    }

    return TRUE;
}

BOOL CWsctlc::Close()
{
    const int32_t handle = (m_socket == INVALID_SOCKET) ? 0 : static_cast<int32_t>(m_socket);

    {
        std::lock_guard<std::mutex> lock(g_androidSocketMutex);
        if (handle > 0)
        {
            g_androidSocketOwners.erase(handle);
        }

        AndroidClearPacketQueue();
    }

    std::memset(m_SendBuf, 0, sizeof(m_SendBuf));
    std::memset(m_RecvBuf, 0, sizeof(m_RecvBuf));
    m_nSendBufLen = 0;
    m_nRecvBufLen = 0;
    m_socket = INVALID_SOCKET;

    if (m_bGame)
    {
        g_bGameServerConnected = FALSE;
    }

    if (handle > 0)
    {
        ConnectionManager_Disconnect(handle);
    }

    return TRUE;
}

BOOL CWsctlc::Close(SOCKET& socket)
{
    if (socket == m_socket)
    {
        Close();
    }
    else if (socket != INVALID_SOCKET)
    {
        const int32_t handle = static_cast<int32_t>(socket);
        {
            std::lock_guard<std::mutex> lock(g_androidSocketMutex);
            g_androidSocketOwners.erase(handle);
        }
        ConnectionManager_Disconnect(handle);
        socket = INVALID_SOCKET;
    }

    socket = INVALID_SOCKET;
    return TRUE;
}

BOOL CWsctlc::Connect(char* ipAddr, unsigned short port, DWORD)
{
    if (m_hWnd == NULL || ipAddr == nullptr || ipAddr[0] == '\0')
    {
        return FALSE;
    }

    if (m_socket != INVALID_SOCKET)
    {
        Close();
    }

    const std::wstring hostWide = ToWideString(ipAddr);
    const int32_t handle = ConnectionManager_Connect(
        hostWide.c_str(),
        static_cast<int32_t>(port),
        0,
        CWsctlc::AndroidOnPacket,
        CWsctlc::AndroidOnDisconnect);

    if (handle <= 0)
    {
        g_ErrorReport.Write("[Android Socket] connect failed ip=%s port=%d\r\n", ipAddr, port);
        return FALSE;
    }

    {
        std::lock_guard<std::mutex> lock(g_androidSocketMutex);
        g_androidSocketOwners[handle] = this;
        AndroidClearPacketQueue();
    }

    m_socket = static_cast<SOCKET>(handle);
    g_ErrorReport.Write("[Android Socket] connected ip=%s port=%d handle=%d\r\n", ipAddr, port, handle);

    // Recorded for the on-screen diagnostic: m_socket here is a handle from the
    // Android connection manager, not a real fd, so getpeername cannot report
    // which server we ended up on. Capture it at the one place that knows.
    {
        std::lock_guard<std::mutex> lock(g_androidSocketMutex);
        std::snprintf(g_androidCurrentHost, sizeof(g_androidCurrentHost), "%s", ipAddr);
        g_androidCurrentPort = port;
        g_androidCurrentIsGame = m_bGame ? 1 : 0;
        ++g_androidConnectCount;
    }

    if (IsConnectServerPort(port))
    {
        SendServerListRequest(handle);
        g_ErrorReport.Write("[Android Socket] server list requested handle=%d\r\n", handle);
    }

    return TRUE;
}

int CWsctlc::sSend(SOCKET socket, char* buf, int len)
{
    if (socket == INVALID_SOCKET || buf == nullptr || len <= 0)
    {
        return FALSE;
    }

#if defined(MU_ANDROID_VERBOSE_NET_LOG)
    g_ErrorReport.Write(
        "[Android Socket] send packet handle=%d type=0x%02X size=%d head=0x%02X sub=0x%02X\r\n",
        static_cast<int>(socket),
        static_cast<unsigned char>(buf[0]),
        len,
        (len > 2) ? static_cast<unsigned char>(buf[2]) : 0,
        (len > 3) ? static_cast<unsigned char>(buf[3]) : 0);
#endif

    std::vector<uint8_t> sendBuffer(reinterpret_cast<uint8_t*>(buf), reinterpret_cast<uint8_t*>(buf) + len);

#if(ENCRYPT_STATE==1)
    if (gProtect.CheckSocketPort(socket) != 0)
    {
        gProtect.EncryptData(sendBuffer.data(), len);
    }
#endif

    ConnectionManager_Send(static_cast<int32_t>(socket), sendBuffer.data(), len);
    return TRUE;
}

int CWsctlc::FDWriteSend()
{
    return TRUE;
}

int CWsctlc::nRecv()
{
    return 0;
}

BYTE* CWsctlc::GetReadMsg()
{
    std::lock_guard<std::mutex> lock(g_androidSocketMutex);

    if (m_pPacketQueue != nullptr && !m_pPacketQueue->IsEmpty())
    {
        CPacket* packet = m_pPacketQueue->FrontPacket();
        m_pPacketQueue->PopPacket();
        return packet->GetBuffer();
    }

    return g_wsctlcReadMessage[0] ? g_wsctlcReadMessage : NULL;
}

void CWsctlc::LogPrint(char*, ...) {}
void CWsctlc::LogHexPrint(BYTE*, int) {}
void CWsctlc::LogHexPrintS(BYTE*, int) {}
void CWsctlc::LogPrintOn() {}
void CWsctlc::LogPrintOff() {}

CErrorReport::CErrorReport() { Clear(); }
CErrorReport::~CErrorReport() {}
void CErrorReport::Clear(void) { m_hFile = INVALID_HANDLE_VALUE; m_lpszFileName[0] = 0; m_iKey = 0; }
void CErrorReport::Create(char*) {}
void CErrorReport::Destroy(void) {}
void CErrorReport::CutHead(void) {}
char* CErrorReport::CheckHeadToCut(char*, DWORD) { return nullptr; }
BOOL CErrorReport::WriteFile(HANDLE, void*, DWORD, LPDWORD, LPOVERLAPPED) { return TRUE; }
void CErrorReport::WriteDebugInfoStr(char*) {}
void CErrorReport::Write(const char* lpszFormat, ...)
{
    if (ShouldSuppressAndroidRuntimeLog(lpszFormat))
    {
        return;
    }

    va_list args;
    va_start(args, lpszFormat);
    __android_log_vprint(ANDROID_LOG_INFO, "TakumiErrorReport", lpszFormat ? lpszFormat : "", args);
    va_end(args);
}
void CErrorReport::HexWrite(void*, int) {}
void CErrorReport::AddSeparator(void) {}
void CErrorReport::WriteLogBegin(void) {}
void CErrorReport::WriteCurrentTime(BOOL) {}
void CErrorReport::WriteSystemInfo(ER_SystemInfo*) {}
void CErrorReport::WriteOpenGLInfo(void) {}
void CErrorReport::WriteImeInfo(HWND) {}
void CErrorReport::WriteSoundCardInfo(void) {}
void GetSystemInfo(ER_SystemInfo*) {}

int TERRAIN_INDEX_REPEAT(int x, int y)
{
    return (y & TERRAIN_SIZE_MASK) * TERRAIN_SIZE + (x & TERRAIN_SIZE_MASK);
}

int TERRAIN_INDEX(int x, int y)
{
    return y * TERRAIN_SIZE + x;
}

__attribute__((used, noinline)) void TEXCOORD(float* c, float u, float v)
{
    if (!c) return;
    c[0] = u;
    c[1] = v;
}

namespace SEASON4A
{
    float IntensityTransform[MAX_MESH][MAX_VERTICES];
    int g_iLimitAttackTime = 15;
}

CSimpleModulus::CSimpleModulus()
{
    Init();
}
CSimpleModulus::~CSimpleModulus() {}
void CSimpleModulus::Init(void)
{
    std::memset(m_dwModulus, 0, sizeof(m_dwModulus));
    std::memset(m_dwEncryptionKey, 0, sizeof(m_dwEncryptionKey));
    std::memset(m_dwDecryptionKey, 0, sizeof(m_dwDecryptionKey));
    std::memset(m_dwXORKey, 0, sizeof(m_dwXORKey));
}
int CSimpleModulus::Encrypt(void* lpTarget, void* lpSource, int iSize)
{
    if (lpSource == nullptr || iSize <= 0)
    {
        return 0;
    }

    if (lpTarget == nullptr)
    {
        return ((iSize + 7) / 8) * 11;
    }

    const auto* source = reinterpret_cast<const uint8_t*>(lpSource);
    const uint8_t counter = source[0];

    std::vector<uint8_t> plainPacket(static_cast<size_t>(iSize + 1), 0);
    plainPacket[0] = 0xC3;
    plainPacket[1] = static_cast<uint8_t>(plainPacket.size() & 0xFF);

    if (iSize > 1)
    {
        std::memcpy(plainPacket.data() + 2, source + 1, static_cast<size_t>(iSize - 1));
    }

    mu::PacketKeySet keySet {};
    for (int i = 0; i < SIZE_ENCRYPTION_KEY; ++i)
    {
        keySet.modulus[static_cast<size_t>(i)] = m_dwModulus[i];
        keySet.key[static_cast<size_t>(i)] = m_dwEncryptionKey[i];
        keySet.xorKey[static_cast<size_t>(i)] = m_dwXORKey[i];
    }

    const std::vector<uint8_t> encryptedPacket = mu::SM_EncryptPacketWithKey(plainPacket, counter, keySet);
    const int encryptedSize = static_cast<int>(encryptedPacket.size()) - 2;

    if (encryptedSize > 0)
    {
        std::memcpy(lpTarget, encryptedPacket.data() + 2, static_cast<size_t>(encryptedSize));
    }

    return encryptedSize;
}
int CSimpleModulus::Decrypt(void* lpTarget, void* lpSource, int iSize)
{
    if (lpSource == nullptr || iSize <= 0)
    {
        return 0;
    }

    std::vector<uint8_t> encryptedPacket;

    if ((iSize + 2) <= 0xFF)
    {
        encryptedPacket.resize(static_cast<size_t>(iSize + 2), 0);
        encryptedPacket[0] = 0xC3;
        encryptedPacket[1] = static_cast<uint8_t>(encryptedPacket.size() & 0xFF);
        std::memcpy(encryptedPacket.data() + 2, lpSource, static_cast<size_t>(iSize));
    }
    else
    {
        encryptedPacket.resize(static_cast<size_t>(iSize + 3), 0);
        encryptedPacket[0] = 0xC4;
        encryptedPacket[1] = static_cast<uint8_t>((encryptedPacket.size() >> 8) & 0xFF);
        encryptedPacket[2] = static_cast<uint8_t>(encryptedPacket.size() & 0xFF);
        std::memcpy(encryptedPacket.data() + 3, lpSource, static_cast<size_t>(iSize));
    }

    bool success = false;
    uint8_t packetCounter = 0;
    mu::PacketKeySet keySet {};
    for (int i = 0; i < SIZE_ENCRYPTION_KEY; ++i)
    {
        keySet.modulus[static_cast<size_t>(i)] = m_dwModulus[i];
        keySet.key[static_cast<size_t>(i)] = m_dwDecryptionKey[i];
        keySet.xorKey[static_cast<size_t>(i)] = m_dwXORKey[i];
    }

    const std::vector<uint8_t> plainPacket = mu::SM_DecryptPacketWithKey(encryptedPacket, keySet, &success, &packetCounter);

    if (!success || plainPacket.size() < 3)
    {
        return -1;
    }

    const int headerSize = (plainPacket[0] == 0xC4) ? 3 : 2;
    const int payloadSize = static_cast<int>(plainPacket.size()) - headerSize;
    const int resultSize = payloadSize + 1;

    if (lpTarget != nullptr)
    {
        auto* target = reinterpret_cast<uint8_t*>(lpTarget);
        target[0] = packetCounter;
        if (payloadSize > 0)
        {
            std::memcpy(target + 1, plainPacket.data() + headerSize, static_cast<size_t>(payloadSize));
        }
    }

    return resultSize;
}
void CSimpleModulus::EncryptBlock(void*, void*, int) {}
int CSimpleModulus::DecryptBlock(void*, void*) { return 0; }
int CSimpleModulus::AddBits(void*, int, void*, int, int) { return 0; }
void CSimpleModulus::Shift(void*, int, int) {}
int CSimpleModulus::GetByteOfBit(int nBit) { return nBit / 8; }
BOOL CSimpleModulus::SaveAllKey(char*) { return TRUE; }
BOOL CSimpleModulus::LoadAllKey(char* lpszFileName) { return (LoadEncryptionKey(lpszFileName) && LoadDecryptionKey(lpszFileName)); }
BOOL CSimpleModulus::SaveEncryptionKey(char*) { return TRUE; }
BOOL CSimpleModulus::LoadEncryptionKey(char* lpszFileName) { return LoadKey(lpszFileName, CHUNKID_ONEKEY, TRUE, TRUE, FALSE, TRUE); }
BOOL CSimpleModulus::SaveDecryptionKey(char*) { return TRUE; }
BOOL CSimpleModulus::LoadDecryptionKey(char* lpszFileName) { return LoadKey(lpszFileName, CHUNKID_ONEKEY, TRUE, FALSE, TRUE, TRUE); }
BOOL CSimpleModulus::SaveKey(char*, unsigned short, BOOL, BOOL, BOOL, BOOL) { return TRUE; }
BOOL CSimpleModulus::LoadKey(char* lpszFileName, unsigned short sID, BOOL bMod, BOOL bEnc, BOOL bDec, BOOL bXOR)
{
    if (sID != CHUNKID_ONEKEY)
    {
        return FALSE;
    }

    DWORD modulus[SIZE_ENCRYPTION_KEY] = {};
    DWORD key[SIZE_ENCRYPTION_KEY] = {};
    DWORD xorKey[SIZE_ENCRYPTION_KEY] = {};

    bool loaded = LoadSimpleModulusKeyFile(lpszFileName, modulus, key, xorKey);

    if (!loaded)
    {
        CopyPacketKey(modulus, kTakumiModulus);
        CopyPacketKey(xorKey, kTakumiXorKey);

        if (bEnc)
        {
            CopyPacketKey(key, kTakumiEncryptKey);
            g_ErrorReport.Write("[AndroidLogin] SimpleModulus encrypt key fallback active path=%s\r\n", lpszFileName ? lpszFileName : "(null)");
        }
        else if (bDec)
        {
            CopyPacketKey(key, kTakumiDecryptKey);
            g_ErrorReport.Write("[AndroidLogin] SimpleModulus decrypt key fallback active path=%s\r\n", lpszFileName ? lpszFileName : "(null)");
        }
        else
        {
            return FALSE;
        }
    }
    else
    {
        g_ErrorReport.Write("[AndroidLogin] SimpleModulus key loaded path=%s\r\n", lpszFileName ? lpszFileName : "(null)");
    }

    if (bMod)
    {
        std::memcpy(m_dwModulus, modulus, sizeof(m_dwModulus));
    }

    if (bEnc)
    {
        std::memcpy(m_dwEncryptionKey, key, sizeof(m_dwEncryptionKey));
    }

    if (bDec)
    {
        std::memcpy(m_dwDecryptionKey, key, sizeof(m_dwDecryptionKey));
    }

    if (bXOR)
    {
        std::memcpy(m_dwXORKey, xorKey, sizeof(m_dwXORKey));
    }

    if (bEnc)
    {
        SyncClientKey(m_dwModulus, m_dwEncryptionKey, m_dwXORKey);
    }

    if (bDec)
    {
        SyncServerKey(m_dwModulus, m_dwDecryptionKey, m_dwXORKey);
    }

    return TRUE;
}
BOOL CSimpleModulus::LoadKeyFromBuffer(BYTE* pbyBuffer, BOOL bMod, BOOL bEnc, BOOL bDec, BOOL bXOR)
{
    if (pbyBuffer == nullptr)
    {
        return FALSE;
    }

    DWORD temp[SIZE_ENCRYPTION_KEY] = {};
    std::memcpy(temp, pbyBuffer, sizeof(temp));

    if (bMod)
    {
        std::memcpy(m_dwModulus, temp, sizeof(m_dwModulus));
    }

    if (bEnc)
    {
        std::memcpy(m_dwEncryptionKey, temp, sizeof(m_dwEncryptionKey));
    }

    if (bDec)
    {
        std::memcpy(m_dwDecryptionKey, temp, sizeof(m_dwDecryptionKey));
    }

    if (bXOR)
    {
        std::memcpy(m_dwXORKey, temp, sizeof(m_dwXORKey));
    }

    if (bEnc)
    {
        SyncClientKey(m_dwModulus, m_dwEncryptionKey, m_dwXORKey);
    }

    if (bDec)
    {
        SyncServerKey(m_dwModulus, m_dwDecryptionKey, m_dwXORKey);
    }

    return TRUE;
}

DWORD CSimpleModulus::s_dwSaveLoadXOR[SIZE_ENCRYPTION_KEY] = {
    0x3F08A79B,
    0xE25CC287,
    0x93D27AB9,
    0x20DEA7BF,
};

namespace leaf
{
    void xstreambuf::init()
    {
        m_pBuffer = nullptr;
        m_offset = 0;
        m_size = 0;
        m_capacity = 0;
        m_chunksize = 4096;
    }

    xstreambuf::xstreambuf() { init(); }
    xstreambuf::xstreambuf(unsigned int size) { init(); resize(size); }
    xstreambuf::xstreambuf(const xstreambuf& xbuf) { init(); *this = xbuf; }
    xstreambuf::~xstreambuf() { std::free(m_pBuffer); }

    void xstreambuf::seek(unsigned int n, XBUF_POS pos)
    {
        m_offset = (pos == XBUF_END) ? (unsigned int)std::min<size_t>(m_size, n) : n;
        if (m_offset > m_size) m_offset = (unsigned int)m_size;
    }

    xstreambuf& xstreambuf::write(const void* src, unsigned int n)
    {
        if (m_offset + n > m_capacity) resize(m_offset + n);
        if (src && n > 0) std::memcpy((char*)m_pBuffer + m_offset, src, n);
        m_offset += n;
        if (m_offset > m_size) m_size = m_offset;
        return *this;
    }

    xstreambuf& xstreambuf::read(void* dest, unsigned int n)
    {
        const size_t available = (m_offset < m_size) ? (m_size - m_offset) : 0;
        const size_t readable = (available < n) ? available : n;
        if (dest && readable > 0) std::memcpy(dest, (char*)m_pBuffer + m_offset, readable);
        m_offset += (unsigned int)readable;
        return *this;
    }

    void xstreambuf::clear() { m_offset = 0; m_size = 0; }
    void xstreambuf::flush() {}
    const void* xstreambuf::data() const { return m_pBuffer; }
    void* xstreambuf::get_writebuf() const { return m_pBuffer; }
    unsigned int xstreambuf::get_curpos() const { return m_offset; }
    size_t xstreambuf::size() const { return m_size; }
    void xstreambuf::resize(size_t s)
    {
        if (s <= m_capacity) { if (s > m_size) m_size = s; return; }
        const size_t newCapacity = ((s + m_chunksize - 1) / m_chunksize) * m_chunksize;
        void* newBuffer = std::realloc(m_pBuffer, newCapacity);
        if (!newBuffer) return;
        m_pBuffer = newBuffer;
        if (newCapacity > m_capacity) std::memset((char*)m_pBuffer + m_capacity, 0, newCapacity - m_capacity);
        m_capacity = newCapacity;
        m_size = s;
    }
    size_t xstreambuf::capacity() const { return m_capacity; }
    bool xstreambuf::empty() const { return m_size == 0; }
    void xstreambuf::set_chunksize(size_t size) { m_chunksize = size ? size : 4096; }
    size_t xstreambuf::get_chunksize() { return m_chunksize; }

    xfstreambuf::xfstreambuf() {}
    xfstreambuf::xfstreambuf(const xstreambuf& xbuf) : xstreambuf(xbuf) {}
    xfstreambuf::xfstreambuf(const std::string& filename) { load(filename); }
    xfstreambuf::~xfstreambuf() {}
    bool xfstreambuf::load(const std::string&) { clear(); return false; }
    bool xfstreambuf::save(const std::string&) { return false; }
}

static void AndroidJpegNoopMessage(j_common_ptr) {}
static void AndroidJpegNoopEmit(j_common_ptr, int) {}
static void AndroidJpegNoopOutput(j_common_ptr) {}
static void AndroidJpegNoopFormat(j_common_ptr, char*) {}
static void AndroidJpegNoopReset(j_common_ptr) {}
static int AndroidJpegNoopTrace(j_common_ptr, int) { return 0; }
static void AndroidJpegNoopMemVoid(j_common_ptr, int) {}
static void* AndroidJpegAllocSmall(j_common_ptr, int, size_t size) { return std::calloc(1, size); }
static void* AndroidJpegAllocLarge(j_common_ptr, int, size_t size) { return std::calloc(1, size); }
static JSAMPARRAY AndroidJpegAllocSArray(j_common_ptr, int, JDIMENSION samplesperrow, JDIMENSION numrows)
{
    const JDIMENSION rowCount = (numrows > 0) ? numrows : 1;
    const JDIMENSION sampleCount = (samplesperrow > 0) ? samplesperrow : 1;
    JSAMPARRAY rows = (JSAMPARRAY)std::calloc(rowCount, sizeof(JSAMPROW));
    for (JDIMENSION i = 0; rows && i < rowCount; ++i)
        rows[i] = (JSAMPROW)std::calloc(sampleCount, sizeof(JSAMPLE));
    return rows;
}

static jpeg_memory_mgr g_androidJpegMemory = {};

extern "C" jpeg_error_mgr* jpeg_std_error(jpeg_error_mgr* err)
{
    if (!err) return nullptr;
    std::memset(err, 0, sizeof(*err));
    err->error_exit = AndroidJpegNoopMessage;
    err->emit_message = AndroidJpegNoopEmit;
    err->output_message = AndroidJpegNoopOutput;
    err->format_message = AndroidJpegNoopFormat;
    err->reset_error_mgr = AndroidJpegNoopReset;
    err->msg_code = 0;
    err->trace_level = 0;
    err->num_warnings = 0;
    return err;
}

extern "C" void jpeg_CreateDecompress(j_decompress_ptr cinfo, int, size_t)
{
    if (!cinfo) return;
    g_androidJpegMemory.alloc_small = AndroidJpegAllocSmall;
    g_androidJpegMemory.alloc_large = AndroidJpegAllocLarge;
    g_androidJpegMemory.alloc_sarray = AndroidJpegAllocSArray;
    g_androidJpegMemory.free_pool = AndroidJpegNoopMemVoid;
    cinfo->mem = &g_androidJpegMemory;
    cinfo->image_width = 1;
    cinfo->image_height = 1;
    cinfo->output_width = 1;
    cinfo->output_height = 1;
    cinfo->output_components = 3;
}

extern "C" void jpeg_destroy_decompress(j_decompress_ptr) {}
extern "C" void jpeg_stdio_src(j_decompress_ptr, FILE*) {}
extern "C" int jpeg_read_header(j_decompress_ptr, j_boolean) { return JPEG_HEADER_OK; }
extern "C" j_boolean jpeg_start_decompress(j_decompress_ptr cinfo)
{
    if (cinfo)
    {
        cinfo->output_width = cinfo->output_width ? cinfo->output_width : 1;
        cinfo->output_height = cinfo->output_height ? cinfo->output_height : 1;
        cinfo->output_components = cinfo->output_components ? cinfo->output_components : 3;
        cinfo->output_scanline = 0;
    }
    return TRUE;
}
extern "C" JDIMENSION jpeg_read_scanlines(j_decompress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION max_lines)
{
    if (!cinfo || max_lines == 0) return 0;
    if (scanlines && scanlines[0])
    {
        const JDIMENSION rowSize = cinfo->output_width * cinfo->output_components;
        std::memset(scanlines[0], 0xff, rowSize > 0 ? rowSize : 1);
    }
    cinfo->output_scanline++;
    return 1;
}
extern "C" j_boolean jpeg_finish_decompress(j_decompress_ptr) { return TRUE; }
extern "C" void jpeg_CreateCompress(j_compress_ptr cinfo, int, size_t)
{
    if (!cinfo) return;
    g_androidJpegMemory.alloc_small = AndroidJpegAllocSmall;
    g_androidJpegMemory.alloc_large = AndroidJpegAllocLarge;
    g_androidJpegMemory.alloc_sarray = AndroidJpegAllocSArray;
    g_androidJpegMemory.free_pool = AndroidJpegNoopMemVoid;
    cinfo->mem = &g_androidJpegMemory;
    cinfo->next_scanline = 0;
}
extern "C" void jpeg_destroy_compress(j_compress_ptr) {}
extern "C" void jpeg_stdio_dest(j_compress_ptr, FILE*) {}
extern "C" void jpeg_set_defaults(j_compress_ptr) {}
extern "C" void jpeg_set_quality(j_compress_ptr, int, j_boolean) {}
extern "C" void jpeg_start_compress(j_compress_ptr cinfo, j_boolean)
{
    if (cinfo) cinfo->next_scanline = 0;
}
extern "C" JDIMENSION jpeg_write_scanlines(j_compress_ptr cinfo, JSAMPARRAY, JDIMENSION num_lines)
{
    if (cinfo) cinfo->next_scanline += num_lines;
    return num_lines;
}
extern "C" void jpeg_finish_compress(j_compress_ptr) {}

BOOL Util_CheckOption(char*, unsigned char, char*)
{
    return FALSE;
}

#endif
