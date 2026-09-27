#pragma once

// Objective-C++ only: the seam between IosAudio.mm, which owns the app's one
// AVAudioEngine, and IosVoice.mm, which supplies the voice nodes for it.
//
// One engine, not two: with a second engine for voice, turning on voice
// processing for the microphone reconfigured the audio hardware, the effects
// engine restarted in response, and that restart stopped the voice engine
// within milliseconds - every time, before a single microphone buffer arrived.
// A single engine also puts the game's own sounds through voice processing,
// so its echo canceller removes them from what the player sends.

#import <AVFoundation/AVFoundation.h>

// --- IosVoice.mm ---------------------------------------------------------

void IosVoiceEngine_Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Sets the session category for the mode (PlayAndRecord while capturing,
// Ambient otherwise) and, when it had to switch to PlayAndRecord, waits for
// the microphone to appear. Blocks; call from the audio build queue only.
// False when capture was asked for but iOS refused recording: build the
// engine without the microphone then.
bool IosVoiceEngine_PrepareSession(bool capture);

// Echo cancellation and noise suppression. Must precede connections and start.
void IosVoiceEngine_EnableVoiceProcessing(AVAudioEngine* engine);

// Source node that plays other players' voices (8kHz mono), attached and
// connected to the main mixer.
void IosVoiceEngine_AttachPlayback(AVAudioEngine* engine);

// Routes the microphone through a muted mixer and taps it. False if the
// input has no usable format.
bool IosVoiceEngine_AttachCapture(AVAudioEngine* engine);

// Whether the engine's input still has the format the tap was built for. A
// configuration change can move it (Bluetooth HFP at 16kHz, say), leaving the
// tap and its converter stale; the engine must be rebuilt then.
bool IosVoiceEngine_CaptureFormatMatches(AVAudioEngine* engine);

// The engine carrying the tap was retired without a replacement tap.
// refused: iOS would not allow recording, as opposed to nobody asking for it.
void IosVoiceEngine_CaptureDetached(bool refused);

// --- IosAudio.mm ---------------------------------------------------------

// What the shared engine should carry besides the effects. Rebuilds it
// asynchronously when that changes.
void IosAudio_SetVoiceRoute(bool playback, bool capture);

// Rebuilds the engine if something stopped it. Cheap; call once a frame.
void IosAudio_Pump();

// Rebuilds the engine with the current route, e.g. after a session option
// changed.
void IosAudio_RequestRebuild();
