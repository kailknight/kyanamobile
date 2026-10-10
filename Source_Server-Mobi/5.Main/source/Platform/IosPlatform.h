#pragma once
// iOS platform hooks, implemented in Source_Server-Mobi/ios/src/*.mm.
// The Android equivalents live in Java (MuMainNativeActivity, PreloadActivity).

#if defined(MU_IOS)

// chdir() to the folder that holds Data/, ui/ and data/ ($HOME/Documents).
void MU_IosChdirToDataRoot();
const char* MU_IosDataRoot();

// Copies the bundled ui/ and data/ folders into the data root, overwriting only
// files whose contents differ (Android: MuMainNativeActivity.copyAssetFile).
void MU_IosCopyBundledAssets();

// Keeps the screen awake while the game runs (Android: FLAG_KEEP_SCREEN_ON).
void MU_IosSetIdleTimerDisabled(bool disabled);

// The window's safe-area insets (notch, rounded corners, home indicator) as
// fractions of its width/height, so they apply to any framebuffer scale.
void MU_IosGetSafeAreaFractions(float* left, float* top, float* right, float* bottom);

// True on an iPhone (iPod), false on an iPad and on the Mac. The phone's screen
// is about half as tall, so the same layout shows everything half the size.
bool MU_IosIsPhone();

// Battery 0-100, or -1 when unknown (simulator, or monitoring not ready yet).
int MU_IosGetBatteryPercent();

// File path of a system font for the SDL_ttf text layer, or nullptr. data.zip
// ships no TTF, so Android falls back to /system/fonts; this is iOS's fallback.
const char* MU_IosSystemFontPath(bool bold);

// First-launch data.zip download and install (Android: PreloadActivity).
// MU_IosPreloadBegin shows the download screen when Data/ is missing or out of
// date and returns false; it returns true when the game can start right away.
// While it returned false, poll MU_IosPreloadIsComplete once per frame.
bool MU_IosPreloadBegin();
bool MU_IosPreloadIsComplete();

// On-screen keyboard (Platform/SokolRuntime.cpp, beside sokol's iOS backend).
void MU_IosShowKeyboard(bool show);
bool MU_IosKeyboardHasFocus();

#endif
