#pragma once

// Sound effect backend used by android_link_stubs.cpp on iOS. Volume is the
// 0-100 value ConvertDirectSoundVolumeToPercent already produces for Android.
void IosAudio_LoadSound(int id, const char* path);
void IosAudio_Play(int id, bool looped);
void IosAudio_Stop(int id);
void IosAudio_StopAll();
void IosAudio_SetEnabled(bool enabled);
void IosAudio_SetMasterVolumePercent(int percent);

// Same names as the Android JNI bridge, so android_main.cpp stays shared.
extern "C" void AndroidAudioInit();
extern "C" void AndroidAudioPlayMusic(const char* absolutePath, bool loop);
extern "C" void AndroidAudioStopMusic();
extern "C" void AndroidAudioSetFocusMuted(bool muted);
extern "C" bool AndroidAudioIsMusicPlaying();
