// iOS counterpart of MuAudio.java: sound effects on an AVAudioEngine voice
// pool, music on one AVAudioPlayer. android_link_stubs.cpp's MU_IOS branch
// maps the engine's DirectSound-style API onto the IosAudio_* calls below.
//
// The engine assumes DirectSound semantics, which this has to reproduce:
// one buffer per sound, and Play() on a playing buffer is a no-op. The scene
// update re-requests the ambient beds every frame, so a looping id that is
// already playing is ignored, and a one-shot is held off until its previous
// instance has run its length. Without this the pool fills with copies of the
// same sample and drones.

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#include "IosAudio.h"

#include <strings.h>
#include <sys/stat.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

void OpenSounds();

namespace
{
constexpr int kVoiceCount = 24;
constexpr double kCanonicalRate = 44100.0;

struct Voice
{
    AVAudioPlayerNode* node = nil;
    int soundId = -1;
    bool looped = false;
    int64_t endMs = 0; // one-shots only
    int64_t startMs = 0;
};

struct Sound
{
    std::string path;
    AVAudioPCMBuffer* buffer = nil;
    int64_t durationMs = 0;
    bool decoding = false;
    bool failed = false;
    bool pendingPlay = false;
    bool pendingLoop = false;
};

std::recursive_mutex g_lock;
AVAudioEngine* g_engine = nil;
AVAudioFormat* g_format = nil;
Voice g_voices[kVoiceCount];
std::unordered_map<int, Sound> g_sounds;
std::unordered_map<int, int> g_loopVoice;      // sound id -> voice index
std::unordered_map<int, int64_t> g_busyUntil;  // sound id -> ms
dispatch_queue_t g_decodeQueue = nullptr;

AVAudioPlayer* g_music = nil;
float g_volume = 1.0f;
bool g_enabled = true;
bool g_initialised = false;
bool g_inBackground = false;
bool g_musicWasPlaying = false;

int64_t NowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void Log(const char* fmt, const char* a = "", int b = 0)
{
    static int s_lines = 0;
    if (s_lines++ < 40)
    {
        fprintf(stderr, "I/MuAudio: ");
        fprintf(stderr, fmt, a, b);
        fputc('\n', stderr);
    }
}

bool FileExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

// The sound tables use Windows paths with loose casing; iOS is case-sensitive.
// Walk each component, matching case-insensitively where the exact name misses.
std::string ResolveCaseInsensitive(const std::string& path)
{
    if (FileExists(path))
    {
        return path;
    }

    std::string resolved = (!path.empty() && path[0] == '/') ? "/" : "";
    size_t start = resolved.size();
    while (start <= path.size())
    {
        size_t end = path.find('/', start);
        if (end == std::string::npos)
        {
            end = path.size();
        }
        const std::string part = path.substr(start, end - start);
        start = end + 1;
        if (part.empty())
        {
            continue;
        }

        std::string candidate = resolved + part;
        struct stat st;
        if (stat(candidate.c_str(), &st) != 0)
        {
            NSString* dir = [NSString stringWithUTF8String:(resolved.empty() ? "." : resolved.c_str())];
            NSArray<NSString*>* entries = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:nil];
            bool matched = false;
            for (NSString* entry in entries)
            {
                if (strcasecmp(entry.fileSystemRepresentation, part.c_str()) == 0)
                {
                    candidate = resolved + entry.fileSystemRepresentation;
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                return path;
            }
        }
        resolved = candidate;
        if (end < path.size())
        {
            resolved.push_back('/');
        }
    }
    return resolved;
}

AVAudioPCMBuffer* DecodeToCanonical(const std::string& path)
{
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
    NSError* error = nil;
    AVAudioFile* file = [[AVAudioFile alloc] initForReading:url error:&error];
    if (file == nil || file.length <= 0)
    {
        return nil;
    }

    AVAudioFormat* sourceFormat = file.processingFormat;
    AVAudioPCMBuffer* source = [[AVAudioPCMBuffer alloc] initWithPCMFormat:sourceFormat
                                                             frameCapacity:(AVAudioFrameCount)file.length];
    if (source == nil || ![file readIntoBuffer:source error:&error])
    {
        return nil;
    }

    if ([sourceFormat isEqual:g_format])
    {
        return source;
    }

    AVAudioConverter* converter = [[AVAudioConverter alloc] initFromFormat:sourceFormat toFormat:g_format];
    if (converter == nil)
    {
        return nil;
    }
    // Mono sources go to both speakers rather than the left one only.
    if (sourceFormat.channelCount == 1)
    {
        converter.channelMap = @[ @0, @0 ];
    }

    const double ratio = g_format.sampleRate / sourceFormat.sampleRate;
    const AVAudioFrameCount capacity = (AVAudioFrameCount)(source.frameLength * ratio) + 1024;
    AVAudioPCMBuffer* out = [[AVAudioPCMBuffer alloc] initWithPCMFormat:g_format frameCapacity:capacity];
    __block bool supplied = false;
    AVAudioConverterOutputStatus status = [converter convertToBuffer:out
                                                               error:&error
                                                  withInputFromBlock:^AVAudioBuffer*(AVAudioPacketCount, AVAudioConverterInputStatus* inputStatus) {
        if (supplied)
        {
            *inputStatus = AVAudioConverterInputStatus_EndOfStream;
            return nil;
        }
        supplied = true;
        *inputStatus = AVAudioConverterInputStatus_HaveData;
        return source;
    }];
    if (status == AVAudioConverterOutputStatus_Error || out.frameLength == 0)
    {
        return nil;
    }
    return out;
}

bool EnsureEngineRunning()
{
    if (g_engine == nil)
    {
        return false;
    }
    if (g_engine.isRunning)
    {
        return true;
    }
    NSError* error = nil;
    [[AVAudioSession sharedInstance] setActive:YES error:nil];
    if (![g_engine startAndReturnError:&error])
    {
        Log("engine start failed: %s", error.localizedDescription.UTF8String);
        return false;
    }
    return true;
}

// Loops die with the engine. Forgetting them is enough: the scene update
// re-requests them every frame, so they come straight back once it runs.
void ForgetActiveVoices()
{
    for (Voice& voice : g_voices)
    {
        voice.soundId = -1;
        voice.looped = false;
        voice.endMs = 0;
    }
    g_loopVoice.clear();
    g_busyUntil.clear();
}

int PickVoice(int64_t now)
{
    int oldest = -1;
    for (int i = 0; i < kVoiceCount; ++i)
    {
        const Voice& voice = g_voices[i];
        if (voice.soundId < 0 || (!voice.looped && now >= voice.endMs))
        {
            return i;
        }
        if (!voice.looped && (oldest < 0 || voice.startMs < g_voices[oldest].startMs))
        {
            oldest = i;
        }
    }
    return oldest; // steal the oldest one-shot; never a loop
}

void StartVoice(int soundId, Sound& sound, bool looped)
{
    if (!EnsureEngineRunning())
    {
        return;
    }

    const int64_t now = NowMs();
    const int index = PickVoice(now);
    if (index < 0)
    {
        return;
    }

    Voice& voice = g_voices[index];
    if (voice.soundId >= 0)
    {
        auto loop = g_loopVoice.find(voice.soundId);
        if (loop != g_loopVoice.end() && loop->second == index)
        {
            g_loopVoice.erase(loop);
        }
    }
    [voice.node stop];
    [voice.node scheduleBuffer:sound.buffer
                        atTime:nil
                       options:looped ? AVAudioPlayerNodeBufferLoops : 0
             completionHandler:nil];
    [voice.node play];

    voice.soundId = soundId;
    voice.looped = looped;
    voice.startMs = now;
    voice.endMs = now + sound.durationMs;
    if (looped)
    {
        g_loopVoice[soundId] = index;
    }
    else if (sound.durationMs > 0)
    {
        g_busyUntil[soundId] = voice.endMs;
    }
}

void ApplyVolume()
{
    const float volume = g_enabled ? g_volume : 0.0f;
    if (g_engine != nil)
    {
        g_engine.mainMixerNode.outputVolume = volume;
    }
    if (g_music != nil)
    {
        g_music.volume = g_volume;
    }
}

void OnWillResignActive()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_inBackground = true;
    g_musicWasPlaying = (g_music != nil) && g_music.isPlaying;
    [g_music pause];
    if (g_engine != nil)
    {
        for (Voice& voice : g_voices)
        {
            [voice.node stop];
        }
        [g_engine pause];
    }
    ForgetActiveVoices();
}

void OnDidBecomeActive()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_inBackground = false;
    EnsureEngineRunning();
    if (g_musicWasPlaying && g_music != nil)
    {
        [g_music play];
    }
    g_musicWasPlaying = false;
}

void InstallObservers()
{
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:nil
                    usingBlock:^(NSNotification*) { OnWillResignActive(); }];
    [center addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:nil
                    usingBlock:^(NSNotification*) { OnDidBecomeActive(); }];
    // A call or Siri stops the engine and pauses the player.
    [center addObserverForName:AVAudioSessionInterruptionNotification object:nil queue:nil
                    usingBlock:^(NSNotification* note) {
        const NSUInteger type = [note.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
        if (type == AVAudioSessionInterruptionTypeBegan)
        {
            OnWillResignActive();
        }
        else if ([UIApplication sharedApplication].applicationState == UIApplicationStateActive)
        {
            OnDidBecomeActive();
        }
    }];
    // Route changes (headphones in/out) stop the engine; nodes stay attached.
    [center addObserverForName:AVAudioEngineConfigurationChangeNotification object:g_engine queue:nil
                    usingBlock:^(NSNotification*) {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        ForgetActiveVoices();
        if (!g_inBackground)
        {
            EnsureEngineRunning();
        }
    }];
}
}

void IosAudio_LoadSound(int id, const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    Sound& sound = g_sounds[id];
    sound = Sound{};
    sound.path = path;
}

void IosAudio_Play(int id, bool looped)
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (!g_enabled || !g_initialised || g_inBackground)
    {
        return;
    }

    if (looped)
    {
        if (g_loopVoice.count(id) != 0)
        {
            return;
        }
    }
    else
    {
        auto busy = g_busyUntil.find(id);
        if (busy != g_busyUntil.end() && NowMs() < busy->second)
        {
            return;
        }
    }

    auto it = g_sounds.find(id);
    if (it == g_sounds.end())
    {
        return;
    }
    Sound& sound = it->second;
    if (sound.failed)
    {
        return;
    }
    if (sound.buffer != nil)
    {
        StartVoice(id, sound, looped);
        return;
    }

    // First use: decode off the render thread, then play from the callback,
    // so the first occurrence of each sound is not lost.
    sound.pendingPlay = true;
    sound.pendingLoop = looped;
    if (sound.decoding)
    {
        return;
    }
    sound.decoding = true;
    const std::string path = sound.path;
    dispatch_async(g_decodeQueue, ^{
        AVAudioPCMBuffer* buffer = nil;
        const std::string resolved = ResolveCaseInsensitive(path);
        @autoreleasepool
        {
            buffer = DecodeToCanonical(resolved);
        }

        std::lock_guard<std::recursive_mutex> innerLock(g_lock);
        auto found = g_sounds.find(id);
        if (found == g_sounds.end() || found->second.path != path)
        {
            return;
        }
        Sound& target = found->second;
        target.decoding = false;
        if (buffer == nil)
        {
            target.failed = true;
            Log("decode failed: %s", resolved.c_str());
            return;
        }
        target.buffer = buffer;
        target.durationMs = static_cast<int64_t>(buffer.frameLength * 1000.0 / g_format.sampleRate);
        if (target.pendingPlay && g_enabled && !g_inBackground)
        {
            target.pendingPlay = false;
            IosAudio_Play(id, target.pendingLoop);
        }
        target.pendingPlay = false;
    });
}

void IosAudio_Stop(int id)
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    for (Voice& voice : g_voices)
    {
        if (voice.soundId == id)
        {
            [voice.node stop];
            voice.soundId = -1;
            voice.looped = false;
        }
    }
    g_loopVoice.erase(id);
    g_busyUntil.erase(id);
    auto it = g_sounds.find(id);
    if (it != g_sounds.end())
    {
        it->second.pendingPlay = false;
    }
}

void IosAudio_StopAll()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    for (Voice& voice : g_voices)
    {
        [voice.node stop];
    }
    ForgetActiveVoices();
    for (auto& entry : g_sounds)
    {
        entry.second.pendingPlay = false;
    }
}

void IosAudio_SetEnabled(bool enabled)
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_enabled = enabled;
    if (!enabled)
    {
        IosAudio_StopAll();
        AndroidAudioStopMusic();
    }
    ApplyVolume();
}

void IosAudio_SetMasterVolumePercent(int percent)
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_volume = std::clamp(percent, 0, 100) / 100.0f;
    ApplyVolume();
}

extern "C" void AndroidAudioInit()
{
    {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        if (!g_initialised)
        {
            // Ambient: follows the silent switch and mixes with other apps.
            NSError* error = nil;
            AVAudioSession* session = [AVAudioSession sharedInstance];
            [session setCategory:AVAudioSessionCategoryAmbient error:&error];
            [session setActive:YES error:&error];

            g_decodeQueue = dispatch_queue_create("mu.audio.decode", DISPATCH_QUEUE_SERIAL);
            g_engine = [[AVAudioEngine alloc] init];
            g_format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:kCanonicalRate channels:2];
            for (Voice& voice : g_voices)
            {
                voice.node = [[AVAudioPlayerNode alloc] init];
                [g_engine attachNode:voice.node];
                [g_engine connect:voice.node to:g_engine.mainMixerNode format:g_format];
            }
            [g_engine prepare];
            g_initialised = EnsureEngineRunning();
            InstallObservers();
            ApplyVolume();
            Log("engine ready=%s voices=%d", g_initialised ? "yes" : "no", kVoiceCount);
        }
    }

    // Fills the table through LoadWaveFile, same order as on Android.
    OpenSounds();
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    Log("sounds registered%s=%d", "", static_cast<int>(g_sounds.size()));
}

extern "C" void AndroidAudioPlayMusic(const char* absolutePath, bool loop)
{
    if (absolutePath == nullptr || absolutePath[0] == '\0')
    {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (!g_enabled)
    {
        return;
    }
    AndroidAudioStopMusic();

    std::string resolved = ResolveCaseInsensitive(absolutePath);
    // AVAudioPlayer has no Vorbis decoder before recent iOS. The Crywolf
    // tracks are Ogg, so the app bundles AAC copies (ios/assets/Music).
    const size_t dot = resolved.find_last_of('.');
    if (dot != std::string::npos && strcasecmp(resolved.c_str() + dot, ".ogg") == 0)
    {
        const size_t slash = resolved.find_last_of('/');
        const std::string stem = resolved.substr(slash + 1, dot - slash - 1);
        NSString* bundled = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:stem.c_str()]
                                                            ofType:@"m4a"
                                                       inDirectory:@"Music"];
        if (bundled != nil)
        {
            resolved = bundled.fileSystemRepresentation;
        }
    }
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:resolved.c_str()]];
    NSError* error = nil;
    AVAudioPlayer* player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];
    if (player == nil)
    {
        Log("music failed: %s", resolved.c_str());
        return;
    }
    player.numberOfLoops = loop ? -1 : 0;
    player.volume = g_volume;
    [player prepareToPlay];
    g_music = player;
    if (g_inBackground)
    {
        g_musicWasPlaying = true;
    }
    else
    {
        [player play];
    }
}

extern "C" void AndroidAudioStopMusic()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    [g_music stop];
    g_music = nil;
    g_musicWasPlaying = false;
}

// iOS delivers focus changes as app lifecycle notifications, handled above.
extern "C" void AndroidAudioSetFocusMuted(bool) {}

extern "C" bool AndroidAudioIsMusicPlaying()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (g_music == nil)
    {
        return false;
    }
    // Paused for the background still counts: the event maps use this to
    // chain tracks and must not skip ahead while the app is away.
    return g_music.isPlaying || g_musicWasPlaying;
}
