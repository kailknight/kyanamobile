#pragma once

// Proximity-voice device layer for iOS, used by 5.Main/source/VoiceAudioIos.cpp.
// Kept free of the game headers: this side is Objective-C++, and the game's
// Win32 compatibility typedefs (BOOL among them) clash with Objective-C's.

// Called on the audio render thread; must always fill count samples.
typedef void (*IosVoicePullFn)(short* out, int count);
// Called with one frame of 8kHz mono samples and its peak level (0-32767).
typedef void (*IosVoiceCaptureFn)(const short* samples, int count, int peak);

// Engine work runs on a background queue, so these return immediately.
void IosVoice_OpenPlayback(IosVoicePullFn pull);
void IosVoice_ClosePlayback();
void IosVoice_OpenCapture(IosVoiceCaptureFn capture, int frameSamples, int sampleRate);
void IosVoice_CloseCapture();

// Restarts the engine after an interruption or route change. Cheap; call
// once a frame.
void IosVoice_Pump();

// What the microphone is delivering, for the voice readout.
enum
{
    IOS_VOICE_CAPTURE_IDLE = 0,
    IOS_VOICE_CAPTURE_STARTING = 1,   // asked for, no samples yet
    IOS_VOICE_CAPTURE_LIVE = 2,       // real audio arriving
    IOS_VOICE_CAPTURE_SILENT = 3,     // buffers arrive but iOS zeroes them (mic held elsewhere)
    IOS_VOICE_CAPTURE_UNAVAILABLE = 4 // session refused recording, or no input format
};
int IosVoice_CaptureState();

// 1 granted, 0 denied, -1 not asked yet.
int IosVoice_MicPermission();
// Shows the system prompt if it has not been answered yet. Asynchronous.
void IosVoice_RequestMicPermission();

// Debug: MU_VOICE_SELFTEST=1 in the launch environment exercises the
// microphone at startup and logs levels. No-op otherwise.
void IosVoice_SelfTestIfRequested();

// Appends a line to Documents/mu_voice_log.txt (and stderr), for the game-side
// half of the voice path.
void IosVoice_Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
