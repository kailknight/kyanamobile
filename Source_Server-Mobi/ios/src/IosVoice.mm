// iOS device layer for proximity voice: the counterpart of Android's
// VoiceAudioOpenSL.cpp. Playback is an AVAudioSourceNode pulling from the
// voice client; capture is a tap on the input node, resampled to the codec's
// 8kHz mono frames. Both live in IosAudio.mm's engine - see IosVoiceEngine.h
// for why there is only one.

#import <AVFoundation/AVFoundation.h>

#include "IosVoice.h"
#include "IosVoiceEngine.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <mutex>
#include <vector>

namespace
{
constexpr double kVoiceRate = 8000.0;
constexpr int kPullChunk = 1024;

// Consecutive all-zero microphone buffers (about 100ms each) before the
// microphone is reported as silenced rather than merely quiet. A live
// built-in microphone never gives exact digital zero for that long; iOS does
// when another app (a call, most often) holds the input, or permission is off.
constexpr int kSilentBuffers = 12;

std::atomic<IosVoicePullFn> g_pull{ nullptr };
std::atomic<IosVoiceCaptureFn> g_capture{ nullptr };
std::atomic<int> g_frameSamples{ 160 };
std::atomic<int> g_captureState{ IOS_VOICE_CAPTURE_IDLE };

// Set once per process when the mixable session gave only zeros: the next
// capture session is built without MixWithOthers, which either takes the
// microphone back or makes iOS say who holds it (an activation error).
std::atomic<bool> g_exclusiveCapture{ false };
std::atomic<bool> g_exclusiveTried{ false };

// Format the current capture tap and converter were built for.
std::mutex g_captureFormatLock;
AVAudioFormat* g_captureFormat = nil;

// Render-thread scratch for PullPlayback.
short g_pullScratch[kPullChunk];

// Guarded by g_routeLock; mirrors what the game asked for.
std::mutex g_routeLock;
bool g_wantPlayback = false;
bool g_wantCapture = false;

AVAudioFormat* VoiceFormat()
{
    static AVAudioFormat* s_format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:kVoiceRate channels:1];
    return s_format;
}

// Called with g_routeLock held, so two threads changing the route cannot
// apply them out of order.
void PushRouteLocked()
{
    IosAudio_SetVoiceRoute(g_wantPlayback, g_wantCapture);
}

const char* PermissionName(int permission)
{
    return permission == 1 ? "granted" : (permission == 0 ? "denied" : "undetermined");
}

// Everything that decides whether iOS hands the app real microphone samples.
void LogSessionSnapshot(const char* why)
{
    AVAudioSession* session = [AVAudioSession sharedInstance];
    NSMutableString* inputs = [NSMutableString string];
    for (AVAudioSessionPortDescription* port in session.currentRoute.inputs)
    {
        [inputs appendFormat:@"%@(%@) ", port.portType, port.portName];
    }
    int appMuted = -1;
    if (@available(iOS 17.0, *))
    {
        appMuted = [AVAudioApplication sharedInstance].isInputMuted ? 1 : 0;
    }
    IosVoiceEngine_Log("snapshot (%s): permission=%s appInputMuted=%d category=%s mode=%s options=0x%lx "
                       "inputAvailable=%d inputs=[%s] inCh=%ld gain=%.2f otherAudioPlaying=%d silencedHint=%d",
                       why, PermissionName(IosVoice_MicPermission()), appMuted, session.category.UTF8String,
                       session.mode.UTF8String, (unsigned long)session.categoryOptions,
                       session.isInputAvailable ? 1 : 0, inputs.UTF8String, (long)session.inputNumberOfChannels,
                       session.inputGain, session.isOtherAudioPlaying ? 1 : 0,
                       session.secondaryAudioShouldBeSilencedHint ? 1 : 0);
}
}

// Console output from a device is easy to lose, so every line also goes to
// Documents/mu_voice_log.txt, which can be copied off the phone. Appended
// across launches (a test is often followed by a restart), with a marker per
// launch, and started afresh once it passes 512KB.
static void VoiceLogV(const char* fmt, va_list args)
{
    static std::mutex s_lock;
    static FILE* s_file = nullptr;
    static int s_lines = 0;
    std::lock_guard<std::mutex> lock(s_lock);
    if (s_lines >= 2000)
    {
        return;
    }
    ++s_lines;
    if (s_file == nullptr)
    {
        NSString* path = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents/mu_voice_log.txt"];
        struct stat st;
        const bool tooBig = stat(path.fileSystemRepresentation, &st) == 0 && st.st_size > 512 * 1024;
        s_file = fopen(path.fileSystemRepresentation, tooBig ? "w" : "a");
        if (s_file != nullptr)
        {
            fprintf(s_file, "===== launch %.3f build %s %s =====\n", CFAbsoluteTimeGetCurrent(), __DATE__, __TIME__);
        }
    }
    char line[768];
    vsnprintf(line, sizeof(line), fmt, args);
    fprintf(stderr, "I/MuVoice: %s\n", line);
    if (s_file != nullptr)
    {
        fprintf(s_file, "%.3f %s\n", CFAbsoluteTimeGetCurrent(), line);
        fflush(s_file);
    }
}

void IosVoiceEngine_Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    VoiceLogV(fmt, args);
    va_end(args);
}

void IosVoice_Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    VoiceLogV(fmt, args);
    va_end(args);
}

bool IosVoiceEngine_PrepareSession(bool capture)
{
    AVAudioSession* session = [AVAudioSession sharedInstance];
    NSString* wanted = capture ? AVAudioSessionCategoryPlayAndRecord : AVAudioSessionCategoryAmbient;
    NSError* error = nil;
    bool ok = true;

    // Speaker, not the earpiece PlayAndRecord defaults to; mixed with other
    // apps like the Ambient category it replaces - unless mixing gave only
    // silence, see g_exclusiveCapture.
    const bool exclusive = capture && g_exclusiveCapture.load();
    AVAudioSessionCategoryOptions options = AVAudioSessionCategoryOptionDefaultToSpeaker
        | AVAudioSessionCategoryOptionAllowBluetooth;
    if (!exclusive)
    {
        options |= AVAudioSessionCategoryOptionMixWithOthers;
    }

    // Only when it really has to change: every setCategory reconfigures the
    // audio hardware.
    const bool changing = ![session.category isEqualToString:wanted]
        || (capture && session.categoryOptions != options);
    if (changing)
    {
        if (capture)
        {
            IosVoiceEngine_Log("session -> PlayAndRecord%s", exclusive ? " (exclusive, no MixWithOthers)" : "");
            if (![session setCategory:wanted mode:AVAudioSessionModeDefault options:options error:&error])
            {
                IosVoiceEngine_Log("PlayAndRecord refused: %s", error.localizedDescription.UTF8String);
                ok = false;
            }
        }
        else if (![session setCategory:wanted error:&error])
        {
            IosVoiceEngine_Log("Ambient refused: %s", error.localizedDescription.UTF8String);
        }
    }

    // Activation is where recording is refused - another app hosting a call
    // makes it fail with InsufficientPriority - so its error matters.
    error = nil;
    if (![session setActive:YES error:&error])
    {
        const uint32_t code = (uint32_t)error.code;
        const char fourcc[5] = { (char)(code >> 24), (char)(code >> 16), (char)(code >> 8), (char)code, 0 };
        IosVoiceEngine_Log("session activation failed: %s (code %ld '%s')", error.localizedDescription.UTF8String,
                           (long)error.code, fourcc);
        if (capture)
        {
            ok = false;
        }
    }

    if (capture && ok)
    {
        // The app-wide input mute (iOS 17+) zeroes every microphone sample. The
        // game never sets it, but it persists, so make sure it is off.
        if (@available(iOS 17.0, *))
        {
            if ([AVAudioApplication sharedInstance].isInputMuted)
            {
                IosVoiceEngine_Log("app input was muted - unmuting");
                [[AVAudioApplication sharedInstance] setInputMuted:NO error:nil];
            }
        }

        // The switch finishes asynchronously: an engine built straight after
        // it sees no microphone (a 0Hz input format), or is stopped a moment
        // later when the route change lands.
        if (changing)
        {
            for (int i = 0; i < 75 && session.inputNumberOfChannels <= 0; ++i)
            {
                usleep(20 * 1000);
            }
            usleep(250 * 1000);
        }
        LogSessionSnapshot("capture starting");
    }
    else
    {
        IosVoiceEngine_Log("session %s inCh=%ld rate=%.0f", session.category.UTF8String,
                           (long)session.inputNumberOfChannels, session.sampleRate);
    }

    if (capture && !ok)
    {
        g_captureState.store(IOS_VOICE_CAPTURE_UNAVAILABLE);
    }
    return ok;
}

void IosVoiceEngine_EnableVoiceProcessing(AVAudioEngine* engine)
{
    // MU_VOICE_VP=0 (launch environment) records without it, for isolating
    // voice processing when the microphone misbehaves.
    const char* env = getenv("MU_VOICE_VP");
    if (env != nullptr && atoi(env) == 0)
    {
        return;
    }

    NSError* error = nil;
    if (![engine.inputNode setVoiceProcessingEnabled:YES error:&error])
    {
        // Still records, only without echo cancellation.
        IosVoiceEngine_Log("voice processing refused: %s", error.localizedDescription.UTF8String);
        return;
    }
    engine.inputNode.voiceProcessingInputMuted = NO;
    if (@available(iOS 17.0, *))
    {
        // Voice processing ducks every other sound by default, which would
        // bury the game's own effects while the player talks.
        AVAudioVoiceProcessingOtherAudioDuckingConfiguration ducking;
        ducking.enableAdvancedDucking = NO;
        ducking.duckingLevel = AVAudioVoiceProcessingOtherAudioDuckingLevelMin;
        engine.inputNode.voiceProcessingOtherAudioDuckingConfiguration = ducking;
    }
}

void IosVoiceEngine_AttachPlayback(AVAudioEngine* engine)
{
    AVAudioSourceNode* source = [[AVAudioSourceNode alloc] initWithFormat:VoiceFormat()
                                                               renderBlock:^OSStatus(BOOL*, const AudioTimeStamp*, AVAudioFrameCount frameCount, AudioBufferList* output) {
        float* out = static_cast<float*>(output->mBuffers[0].mData);
        const IosVoicePullFn pull = g_pull.load();
        AVAudioFrameCount done = 0;
        while (done < frameCount)
        {
            const int n = static_cast<int>(std::min<AVAudioFrameCount>(frameCount - done, kPullChunk));
            if (pull != nullptr)
            {
                pull(g_pullScratch, n);
                for (int i = 0; i < n; ++i)
                {
                    out[done + i] = g_pullScratch[i] / 32768.0f;
                }
            }
            else
            {
                std::fill(out + done, out + done + n, 0.0f);
            }
            done += n;
        }
        return noErr;
    }];
    [engine attachNode:source];
    [engine connect:source to:engine.mainMixerNode format:VoiceFormat()];
}

bool IosVoiceEngine_AttachCapture(AVAudioEngine* engine)
{
    AVAudioInputNode* input = engine.inputNode;
    AVAudioFormat* inputFormat = [input outputFormatForBus:0];
    IosVoiceEngine_Log("mic format %.0fHz %uch vp=%d", inputFormat.sampleRate, inputFormat.channelCount,
                       input.isVoiceProcessingEnabled ? 1 : 0);
    if (inputFormat.sampleRate <= 0 || inputFormat.channelCount == 0)
    {
        g_captureState.store(IOS_VOICE_CAPTURE_UNAVAILABLE);
        return false;
    }

    AVAudioFormat* voiceFormat = VoiceFormat();
    AVAudioConverter* converter = [[AVAudioConverter alloc] initFromFormat:inputFormat toFormat:voiceFormat];
    if (converter == nil)
    {
        IosVoiceEngine_Log("no converter for the microphone format");
        g_captureState.store(IOS_VOICE_CAPTURE_UNAVAILABLE);
        return false;
    }
    converter.downmix = YES;

    // The input node has to be part of the rendered graph, or the engine never
    // pulls from it and the tap is never called. A mixer at zero volume keeps
    // it pulled without playing the microphone back through the speaker.
    AVAudioMixerNode* micSink = [[AVAudioMixerNode alloc] init];
    [engine attachNode:micSink];
    [engine connect:input to:micSink format:inputFormat];
    [engine connect:micSink to:engine.mainMixerNode format:nil];
    micSink.outputVolume = 0.0f;

    {
        std::lock_guard<std::mutex> lock(g_captureFormatLock);
        g_captureFormat = inputFormat;
    }
    g_captureState.store(IOS_VOICE_CAPTURE_STARTING);

    const int frameSamples = g_frameSamples.load();
    // Tap-thread state: only the block below touches these.
    auto pending = std::make_shared<std::vector<float>>();
    pending->reserve(frameSamples * 8);
    auto frame = std::make_shared<std::vector<short>>(frameSamples);
    auto taps = std::make_shared<int>(0);
    auto zeroRun = std::make_shared<int>(0);
    auto snapshotTaken = std::make_shared<bool>(false);
    AVAudioPCMBuffer* chunk = [[AVAudioPCMBuffer alloc] initWithPCMFormat:voiceFormat frameCapacity:512];

    [input installTapOnBus:0 bufferSize:1024 format:inputFormat block:^(AVAudioPCMBuffer* buffer, AVAudioTime*) {
        ++*taps;

        // The raw level before any conversion, and how many samples are exact
        // zeros: that tells "iOS is silencing the input" from "the room is quiet".
        float rawPeak = 0.0f;
        AVAudioFrameCount zeros = 0;
        const float* raw = buffer.floatChannelData ? buffer.floatChannelData[0] : nullptr;
        for (AVAudioFrameCount i = 0; raw != nullptr && i < buffer.frameLength; ++i)
        {
            const float a = std::fabs(raw[i]);
            rawPeak = std::max(rawPeak, a);
            zeros += (a == 0.0f) ? 1 : 0;
        }
        const bool allZero = buffer.frameLength > 0 && zeros == buffer.frameLength;
        *zeroRun = allZero ? *zeroRun + 1 : 0;
        if (!allZero)
        {
            g_captureState.store(IOS_VOICE_CAPTURE_LIVE);
        }
        else if (*zeroRun >= kSilentBuffers)
        {
            g_captureState.store(IOS_VOICE_CAPTURE_SILENT);
        }
        if (*zeroRun == 3 && !*snapshotTaken)
        {
            *snapshotTaken = true;
            LogSessionSnapshot("microphone gives only zeros");
        }
        // Only after a sustained run: voice processing's first buffers can be
        // silent while it settles, and the retry costs an engine rebuild.
        if (*zeroRun == kSilentBuffers && !g_exclusiveTried.exchange(true))
        {
            IosVoiceEngine_Log("retrying the microphone without MixWithOthers");
            g_exclusiveCapture.store(true);
            IosAudio_RequestRebuild();
        }

        // Drained in a loop: when the output fills before the input runs out,
        // the converter would otherwise read the rest of this buffer on the
        // next call, after the engine may have reused its memory.
        __block bool supplied = false;
        for (;;)
        {
            chunk.frameLength = 0;
            NSError* error = nil;
            const AVAudioConverterOutputStatus status =
                [converter convertToBuffer:chunk error:&error withInputFromBlock:^AVAudioBuffer*(AVAudioPacketCount, AVAudioConverterInputStatus* inputStatus) {
                    // NoDataNow rather than EndOfStream: the resampler keeps its
                    // state between taps, so there is no click at each boundary.
                    if (supplied)
                    {
                        *inputStatus = AVAudioConverterInputStatus_NoDataNow;
                        return nil;
                    }
                    supplied = true;
                    *inputStatus = AVAudioConverterInputStatus_HaveData;
                    return buffer;
                }];
            if (chunk.frameLength > 0)
            {
                const float* samples = chunk.floatChannelData[0];
                pending->insert(pending->end(), samples, samples + chunk.frameLength);
            }
            if (status != AVAudioConverterOutputStatus_HaveData)
            {
                break;
            }
        }

        const IosVoiceCaptureFn capture = g_capture.load();
        int bufferPeak = 0;
        size_t offset = 0;
        while (pending->size() - offset >= static_cast<size_t>(frameSamples))
        {
            int peak = 0;
            for (int i = 0; i < frameSamples; ++i)
            {
                const float v = std::clamp((*pending)[offset + i], -1.0f, 1.0f);
                const short s = static_cast<short>(v * 32767.0f);
                (*frame)[i] = s;
                peak = std::max(peak, s < 0 ? -static_cast<int>(s) : static_cast<int>(s));
            }
            bufferPeak = std::max(bufferPeak, peak);
            if (capture != nullptr)
            {
                capture(frame->data(), frameSamples, peak);
            }
            offset += frameSamples;
        }
        pending->erase(pending->begin(), pending->begin() + offset);

        if (*taps <= 3 || (*taps % 20) == 0)
        {
            IosVoiceEngine_Log("mic buffer #%d frames=%u rawPeak=%g zeros=%u peak=%d", *taps, buffer.frameLength,
                               rawPeak, zeros, bufferPeak);
        }
    }];
    return true;
}

bool IosVoiceEngine_CaptureFormatMatches(AVAudioEngine* engine)
{
    std::lock_guard<std::mutex> lock(g_captureFormatLock);
    if (g_captureFormat == nil)
    {
        return true;
    }
    return [[engine.inputNode outputFormatForBus:0] isEqual:g_captureFormat];
}

void IosVoiceEngine_CaptureDetached(bool refused)
{
    std::lock_guard<std::mutex> lock(g_captureFormatLock);
    g_captureFormat = nil;
    g_captureState.store(refused ? IOS_VOICE_CAPTURE_UNAVAILABLE : IOS_VOICE_CAPTURE_IDLE);
}

void IosVoice_OpenPlayback(IosVoicePullFn pull)
{
    g_pull.store(pull);
    std::lock_guard<std::mutex> lock(g_routeLock);
    if (g_wantPlayback)
    {
        return;
    }
    g_wantPlayback = true;
    PushRouteLocked();
}

void IosVoice_ClosePlayback()
{
    std::lock_guard<std::mutex> lock(g_routeLock);
    if (!g_wantPlayback)
    {
        return;
    }
    g_wantPlayback = false;
    PushRouteLocked();
}

void IosVoice_OpenCapture(IosVoiceCaptureFn capture, int frameSamples, int)
{
    IosVoiceEngine_Log("open capture requested (permission=%s)", PermissionName(IosVoice_MicPermission()));
    g_capture.store(capture);
    g_frameSamples.store(frameSamples);
    std::lock_guard<std::mutex> lock(g_routeLock);
    if (g_wantCapture)
    {
        return;
    }
    g_wantCapture = true;
    g_captureState.store(IOS_VOICE_CAPTURE_STARTING);
    PushRouteLocked();
}

void IosVoice_CloseCapture()
{
    std::lock_guard<std::mutex> lock(g_routeLock);
    if (!g_wantCapture)
    {
        return;
    }
    g_wantCapture = false;
    PushRouteLocked();
}

int IosVoice_CaptureState()
{
    return g_captureState.load();
}

void IosVoice_Pump()
{
    IosAudio_Pump();
}

int IosVoice_MicPermission()
{
    if (@available(iOS 17.0, *))
    {
        switch ([AVAudioApplication sharedInstance].recordPermission)
        {
        case AVAudioApplicationRecordPermissionGranted:
            return 1;
        case AVAudioApplicationRecordPermissionDenied:
            return 0;
        default:
            return -1;
        }
    }
    switch ([AVAudioSession sharedInstance].recordPermission)
    {
    case AVAudioSessionRecordPermissionGranted:
        return 1;
    case AVAudioSessionRecordPermissionDenied:
        return 0;
    default:
        return -1;
    }
}

void IosVoice_RequestMicPermission()
{
    if (IosVoice_MicPermission() != -1)
    {
        return;
    }
    void (^done)(BOOL) = ^(BOOL granted) {
        IosVoiceEngine_Log("microphone permission %s", granted ? "granted" : "denied");
    };
    if (@available(iOS 17.0, *))
    {
        [AVAudioApplication requestRecordPermissionWithCompletionHandler:done];
        return;
    }
    [[AVAudioSession sharedInstance] requestRecordPermission:done];
}

// Launched with MU_VOICE_SELFTEST=1: opens the speaker, then the microphone
// twice for six seconds each (push to talk), logging levels - so the device
// layer can be tested from a Mac without logging in. Somebody should talk
// near the phone while it runs; a quiet room behind voice processing can
// legitimately read as silence.
void IosVoice_SelfTestIfRequested()
{
    const char* env = getenv("MU_VOICE_SELFTEST");
    if (env == nullptr || atoi(env) == 0)
    {
        return;
    }
    IosVoiceEngine_Log("self-test starting");
    // A 1kHz tone at -12dBFS through the speaker, so the microphone has
    // something to hear with nobody in the room (run with MU_VOICE_VP=0, or
    // echo cancellation removes it by design).
    IosVoice_OpenPlayback([](short* out, int count) {
        static int s_phase = 0;
        for (int i = 0; i < count; ++i)
        {
            out[i] = (short)(8000.0 * std::sin(2.0 * M_PI * 1000.0 * (s_phase++ % 8) / 8000.0));
        }
    });
    dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);
    const int opens[2] = { 2, 12 };
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        const int at = opens[cycle];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, at * NSEC_PER_SEC), queue, ^{
            IosVoiceEngine_Log("self-test: talk %d on", cycle + 1);
            IosVoice_OpenCapture([](const short*, int, int) {}, 160, 8000);
        });
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (at + 6) * NSEC_PER_SEC), queue, ^{
            IosVoiceEngine_Log("self-test: talk %d off", cycle + 1);
            IosVoice_CloseCapture();
        });
    }
    for (int second = 3; second <= 24; ++second)
    {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, second * NSEC_PER_SEC), queue, ^{
            IosAudio_Pump();
        });
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 24 * NSEC_PER_SEC), queue, ^{
        IosVoiceEngine_Log("self-test done");
    });
}
