// Proximity voice is not wired up on macOS yet: this satisfies the interface
// IosAudio.mm and VoiceAudioIos.cpp expect, reports the microphone as
// unavailable and plays nothing. The rest of the game (including the voice
// settings screens) works as if voice were switched off.

#import <AVFoundation/AVFoundation.h>

#include "IosVoice.h"
#include "IosVoiceEngine.h"

#include <cstdarg>
#include <cstdio>

void IosVoiceEngine_Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "I/MuVoice: ");
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
}

bool IosVoiceEngine_PrepareSession(bool) { return true; }
void IosVoiceEngine_EnableVoiceProcessing(AVAudioEngine*) {}
void IosVoiceEngine_AttachPlayback(AVAudioEngine*) {}
bool IosVoiceEngine_AttachCapture(AVAudioEngine*) { return false; }
bool IosVoiceEngine_CaptureFormatMatches(AVAudioEngine*) { return true; }
void IosVoiceEngine_CaptureDetached(bool) {}

void IosVoice_OpenPlayback(IosVoicePullFn) {}
void IosVoice_ClosePlayback() {}
void IosVoice_OpenCapture(IosVoiceCaptureFn, int, int) {}
void IosVoice_CloseCapture() {}
void IosVoice_Pump() {}
int IosVoice_CaptureState() { return IOS_VOICE_CAPTURE_UNAVAILABLE; }
int IosVoice_MicPermission() { return 0; }
void IosVoice_RequestMicPermission() {}
void IosVoice_SelfTestIfRequested() {}

void IosVoice_Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "I/MuVoice: ");
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
}
