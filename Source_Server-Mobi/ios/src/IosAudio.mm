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
#include "IosVoice.h"
#include "IosVoiceEngine.h"

#include <strings.h>
#include <sys/stat.h>

#include <algorithm>
#include <atomic>
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
// Kept apart from g_inBackground: an interruption whose end is never reported
// must not leave audio off for good, so Pump recovers from this one.
bool g_interrupted = false;
int64_t g_interruptedMs = 0;
bool g_musicWasPlaying = false;

// The engine is the app's only one and carries proximity voice as well (see
// IosVoiceEngine.h). It is rebuilt, never reconfigured in place, whenever the
// voice route changes or something stops it: voice processing can only be set
// before an engine starts, and an engine whose input node has been used keeps
// trying to open the microphone under the playback-only Ambient session.
dispatch_queue_t g_buildQueue = nullptr;
std::atomic<bool> g_rebuildQueued{ false };
bool g_rebuilding = false;       // g_lock; the build queue is starting/stopping the engine
bool g_engineRunning = false;    // g_lock; tracked here so the game thread never queries the engine
bool g_restartQueued = false;    // g_lock
bool g_voicePlayback = false;    // g_lock
bool g_voiceCapture = false;     // g_lock
int64_t g_lastBuildMs = 0;       // g_lock

int64_t NowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Into the same file log as voice (Documents/mu_voice_log.txt).
void Log(const char* fmt, const char* a = "", int b = 0)
{
    static int s_lines = 0;
    if (s_lines++ < 40)
    {
        char line[512];
        snprintf(line, sizeof(line), fmt, a, b);
        IosVoiceEngine_Log("audio: %s", line);
    }
}

// g_lock held.
bool Suspended()
{
    return g_inBackground || g_interrupted;
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

void RequestRebuild();
void RequestRestart();

// g_lock held. Whether effects can be started right now. Never starts the
// engine itself: a start with voice processing takes a second or more, and
// this is called from the game thread, which froze for that long - taps were
// lost while the microphone came up. A stopped engine is restarted on the
// build queue instead and effects are dropped until it is back.
bool EnsureEngineRunning()
{
    if (g_engine == nil || g_rebuilding || g_rebuildQueued.load())
    {
        return false;
    }
    if (g_engineRunning)
    {
        return true;
    }
    if (!Suspended())
    {
        RequestRestart();
    }
    return false;
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
    // g_engineRunning is only a hint: iOS stops a voice-processing engine by
    // itself about 100ms after it starts, and the observer that clears the
    // flag needs g_lock, which is held here. Playing a node on a stopped
    // engine throws, so the failure is caught rather than crashing the game.
    @try
    {
        [voice.node stop];
        [voice.node scheduleBuffer:sound.buffer
                            atTime:nil
                           options:looped ? AVAudioPlayerNodeBufferLoops : 0
                 completionHandler:nil];
        [voice.node play];
    }
    @catch (NSException* exception)
    {
        IosVoiceEngine_Log("effect start failed (%s) - restarting the engine", exception.reason.UTF8String);
        g_engineRunning = false;
        ForgetActiveVoices();
        if (!Suspended())
        {
            RequestRestart();
        }
        return;
    }

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

// g_lock held. Stops the effect voices, unless the engine is changing hands:
// node calls then contend with the build queue stopping or starting it.
void StopVoiceNodesLocked()
{
    if (g_engineRunning && !g_rebuilding)
    {
        @try
        {
            for (Voice& voice : g_voices)
            {
                [voice.node stop];
            }
        }
        @catch (NSException*)
        {
            g_engineRunning = false;
        }
    }
}

// g_lock held. The mixer is left alone while the build queue is starting or
// stopping the engine; every path that clears g_rebuilding applies it again.
void ApplyVolume()
{
    const float volume = g_enabled ? g_volume : 0.0f;
    if (g_engine != nil && !g_rebuilding)
    {
        g_engine.mainMixerNode.outputVolume = volume;
    }
    if (g_music != nil)
    {
        g_music.volume = g_volume;
    }
}

// Builds and starts a complete engine for the given voice route. Runs on the
// build queue without g_lock: the session switch can wait a good fraction of a
// second for the microphone, and effects must not block on that.
AVAudioEngine* BuildEngine(bool voicePlayback, bool voiceCapture, NSMutableArray<AVAudioPlayerNode*>* nodes,
                           bool* startedOut)
{
    bool refused = false;
    if (!IosVoiceEngine_PrepareSession(voiceCapture) && voiceCapture)
    {
        // Recording refused (another app hosting a call, typically). The
        // effects and other players' voices still get an engine.
        IosVoiceEngine_Log("recording refused by iOS - building without the microphone");
        voiceCapture = false;
        refused = true;
        IosVoiceEngine_PrepareSession(false);
    }
    if (!voiceCapture)
    {
        // Kept as "unavailable" rather than idle when refused, so the voice
        // readout can say why nobody hears the player.
        IosVoiceEngine_CaptureDetached(refused);
    }

    AVAudioEngine* engine = [[AVAudioEngine alloc] init];
    if (voiceCapture)
    {
        IosVoiceEngine_EnableVoiceProcessing(engine);
    }

    for (int i = 0; i < kVoiceCount; ++i)
    {
        AVAudioPlayerNode* node = [[AVAudioPlayerNode alloc] init];
        [engine attachNode:node];
        [engine connect:node to:engine.mainMixerNode format:g_format];
        [nodes addObject:node];
    }
    if (voicePlayback || voiceCapture)
    {
        IosVoiceEngine_AttachPlayback(engine);
    }
    if (voiceCapture && !IosVoiceEngine_AttachCapture(engine))
    {
        IosVoiceEngine_Log("microphone unavailable - speaker only");
    }

    [engine prepare];

    // Not started from the background: iOS refuses or silences recording
    // begun there. OnDidBecomeActive starts it instead.
    bool suspended;
    {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        suspended = Suspended();
    }
    NSError* error = nil;
    const int64_t startMs = NowMs();
    const bool started = !suspended && [engine startAndReturnError:&error];
    IosVoiceEngine_Log("engine built voice=%d mic=%d started=%d in %lldms%s%s%s", voicePlayback ? 1 : 0,
                       voiceCapture ? 1 : 0, started ? 1 : 0, (long long)(NowMs() - startMs),
                       suspended ? " (app inactive)" : "", error ? " " : "",
                       error ? error.localizedDescription.UTF8String : "");
    *startedOut = started;
    return engine;
}

// Build queue only. Every start and stop of an engine happens here, outside
// g_lock; g_rebuilding keeps the game thread off the engine meanwhile.
void RebuildNow()
{
    bool playback;
    bool capture;
    AVAudioEngine* retired = nil;
    {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        g_rebuildQueued.store(false);
        g_rebuilding = true;
        g_engineRunning = false;
        playback = g_voicePlayback;
        capture = g_voiceCapture;
        retired = g_engine;
        // The only owners left are `retired` and its own node list, so the
        // release below really frees it - here, before the new one is built.
        g_engine = nil;
        for (Voice& voice : g_voices)
        {
            voice.node = nil;
        }
        ForgetActiveVoices();
    }

    // The old engine is stopped and released before the new one is built:
    // two engines never hold the audio hardware at once, and an old
    // voice-processing engine torn down after the new one started disturbed
    // it. Released only once everything autoreleased while stopping it has
    // drained - freeing an AVAudioEngine with such references pending crashed.
    @autoreleasepool
    {
        [retired stop];
    }
    retired = nil;

    NSMutableArray<AVAudioPlayerNode*>* nodes = [NSMutableArray arrayWithCapacity:kVoiceCount];
    bool started = false;
    AVAudioEngine* engine = BuildEngine(playback, capture, nodes, &started);

    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_engine = engine;
    for (int i = 0; i < kVoiceCount; ++i)
    {
        g_voices[i].node = nodes[i];
    }
    ForgetActiveVoices();
    g_engineRunning = started;
    g_lastBuildMs = NowMs();
    g_rebuilding = false;
    ApplyVolume();

    // The route changed again while this one was building.
    if (playback != g_voicePlayback || capture != g_voiceCapture)
    {
        RequestRebuild();
    }
}

void RequestRebuild()
{
    if (g_buildQueue == nullptr || g_rebuildQueued.exchange(true))
    {
        return;
    }
    dispatch_async(g_buildQueue, ^{
        @autoreleasepool
        {
            RebuildNow();
        }
    });
}

// Build queue only: restarts the current engine after something stopped it,
// or rebuilds when that cannot work.
void RestartNow()
{
    AVAudioEngine* engine;
    {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        g_restartQueued = false;
        if (g_engine == nil || g_engineRunning || g_rebuilding || g_rebuildQueued.load() || Suspended())
        {
            return;
        }
        // A new input format (Bluetooth HFP, say) leaves the tap and its
        // converter stale; only a rebuild recreates them.
        if (g_voiceCapture && !IosVoiceEngine_CaptureFormatMatches(g_engine))
        {
            IosVoiceEngine_Log("microphone format moved - rebuilding");
            RequestRebuild();
            return;
        }
        g_rebuilding = true;
        engine = g_engine;
    }

    NSError* error = nil;
    const int64_t startMs = NowMs();
    const bool started = [engine startAndReturnError:&error];

    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_rebuilding = false;
    if (engine != g_engine)
    {
        return;
    }
    g_engineRunning = started;
    ApplyVolume();
    if (started)
    {
        IosVoiceEngine_Log("engine restarted in %lldms", (long long)(NowMs() - startMs));
        return;
    }
    IosVoiceEngine_Log("engine restart failed (%s) - rebuilding", error.localizedDescription.UTF8String);
    RequestRebuild();
}

void RequestRestart()
{
    if (g_buildQueue == nullptr || g_restartQueued)
    {
        return;
    }
    g_restartQueued = true;
    dispatch_async(g_buildQueue, ^{
        @autoreleasepool
        {
            RestartNow();
        }
    });
}

// Build queue only.
void PauseNow()
{
    AVAudioEngine* engine;
    {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        if (g_engine == nil || g_rebuilding || !Suspended())
        {
            return;
        }
        engine = g_engine;
        g_rebuilding = true;
        g_engineRunning = false;
    }
    [engine pause];
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_rebuilding = false;
    ApplyVolume();
}

// g_lock held.
void PauseAudioLocked()
{
    if (g_music != nil && g_music.isPlaying)
    {
        g_musicWasPlaying = true;
    }
    [g_music pause];
    StopVoiceNodesLocked();
    ForgetActiveVoices();
    if (g_buildQueue != nullptr)
    {
        dispatch_async(g_buildQueue, ^{
            @autoreleasepool
            {
                PauseNow();
            }
        });
    }
}

// g_lock held.
void ResumeAudioLocked()
{
    if (Suspended())
    {
        return;
    }
    EnsureEngineRunning();
    if (g_musicWasPlaying && g_music != nil)
    {
        [g_music play];
    }
    g_musicWasPlaying = false;
}

void OnWillResignActive()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_inBackground = true;
    PauseAudioLocked();
}

void OnDidBecomeActive()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_inBackground = false;
    // Coming back to the foreground ends whatever interruption was running.
    g_interrupted = false;
    ResumeAudioLocked();
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
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        if (type == AVAudioSessionInterruptionTypeBegan)
        {
            IosVoiceEngine_Log("audio interrupted");
            g_interrupted = true;
            g_interruptedMs = NowMs();
            PauseAudioLocked();
        }
        else
        {
            IosVoiceEngine_Log("audio interruption ended");
            g_interrupted = false;
            ResumeAudioLocked();
        }
    }];
    [center addObserverForName:AVAudioSessionRouteChangeNotification object:nil queue:nil
                    usingBlock:^(NSNotification* note) {
        const NSUInteger reason = [note.userInfo[AVAudioSessionRouteChangeReasonKey] unsignedIntegerValue];
        NSMutableString* inputs = [NSMutableString string];
        for (AVAudioSessionPortDescription* port in [AVAudioSession sharedInstance].currentRoute.inputs)
        {
            [inputs appendFormat:@"%@ ", port.portType];
        }
        IosVoiceEngine_Log("route change reason=%lu inputs=[%s]", (unsigned long)reason, inputs.UTF8String);
    }];
    // The media server restarted: every audio object is dead.
    [center addObserverForName:AVAudioSessionMediaServicesWereResetNotification object:nil queue:nil
                    usingBlock:^(NSNotification*) {
        IosVoiceEngine_Log("media services were reset - rebuilding");
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        g_music = nil;
        g_musicWasPlaying = false;
        RequestRebuild();
    }];
    // Route changes (headphones in/out) stop the engine. So does starting one
    // with voice processing: it moves the hardware onto several microphones
    // (the input goes from 1 to 4 channels) and posts a configuration change
    // about 100ms after every start. Rebuilding in response restarted that
    // cycle forever, so the same engine is restarted (on the build queue), and
    // only an engine that will not restart is rebuilt.
    [center addObserverForName:AVAudioEngineConfigurationChangeNotification object:nil queue:nil
                    usingBlock:^(NSNotification* note) {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        if (note.object != g_engine || g_rebuilding)
        {
            return;
        }
        g_engineRunning = false;
        ForgetActiveVoices();
        if (!Suspended())
        {
            RequestRestart();
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
    if (!g_enabled || !g_initialised || Suspended())
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
        if (target.pendingPlay && g_enabled && !Suspended())
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
    const bool nodesUsable = g_engineRunning && !g_rebuilding;
    for (Voice& voice : g_voices)
    {
        if (voice.soundId == id)
        {
            if (nodesUsable)
            {
                [voice.node stop];
            }
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
    StopVoiceNodesLocked();
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
            g_decodeQueue = dispatch_queue_create("mu.audio.decode", DISPATCH_QUEUE_SERIAL);
            g_buildQueue = dispatch_queue_create("mu.audio.engine", DISPATCH_QUEUE_SERIAL);
            g_format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:kCanonicalRate channels:2];

            // Built here, synchronously, so the first sounds have an engine.
            // Effects only, under the Ambient session: follows the silent
            // switch and mixes with other apps.
            NSMutableArray<AVAudioPlayerNode*>* nodes = [NSMutableArray arrayWithCapacity:kVoiceCount];
            bool started = false;
            g_engine = BuildEngine(false, false, nodes, &started);
            g_engineRunning = started;
            for (int i = 0; i < kVoiceCount; ++i)
            {
                g_voices[i].node = nodes[i];
            }
            g_lastBuildMs = NowMs();
            g_initialised = true;
            InstallObservers();
            ApplyVolume();
            Log("engine ready=%s voices=%d", g_engineRunning ? "yes" : "no", kVoiceCount);
        }
    }

    IosVoice_SelfTestIfRequested();

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
    if (Suspended())
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

void IosAudio_SetVoiceRoute(bool playback, bool capture)
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (!g_initialised || (playback == g_voicePlayback && capture == g_voiceCapture))
    {
        return;
    }
    g_voicePlayback = playback;
    g_voiceCapture = capture;
    RequestRebuild();
}

void IosAudio_Pump()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    // An interruption iOS never reported the end of, while the app is in the
    // foreground: give it a few seconds, then take the audio back.
    if (g_initialised && g_interrupted && !g_inBackground && NowMs() - g_interruptedMs > 3000)
    {
        IosVoiceEngine_Log("interruption never ended - resuming audio");
        g_interrupted = false;
        // Cleared only if it really plays: while an interruption is still
        // active, play fails, and PlayMp3 would otherwise rebuild the player
        // every frame.
        if (g_musicWasPlaying && g_music != nil && [g_music play])
        {
            g_musicWasPlaying = false;
        }
        RequestRebuild();
        return;
    }
    if (!g_initialised || Suspended() || g_rebuilding || g_rebuildQueued.load() || g_restartQueued
        || g_engine == nil || g_engineRunning)
    {
        return;
    }
    // A call, Siri or a route change stopped it. Held off briefly after a
    // build, so an engine that is still settling is not torn down again.
    if (NowMs() - g_lastBuildMs < 1500)
    {
        return;
    }
    IosVoiceEngine_Log("engine found stopped - rebuilding");
    RequestRebuild();
}

void IosAudio_RequestRebuild()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (g_initialised)
    {
        RequestRebuild();
    }
}
