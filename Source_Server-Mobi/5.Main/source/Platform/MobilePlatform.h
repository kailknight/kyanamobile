#pragma once

#if defined(__ANDROID__) || defined(MU_IOS)

#include <SDL.h>

#include <string>

void MU_MobilePlatformInit();
void MU_MobilePlatformShutdown();

const Uint8* MU_MobileGetKeyboardState();
void MU_MobileSetKeyState(SDL_Scancode scancode, bool isDown);
void MU_MobileClearKeyboardState();

void MU_MobileStartTextInput();
void MU_MobileStopTextInput();
bool MU_MobileIsTextInputActive();
void MU_MobileSetTextInputRect(const SDL_Rect* rect);

std::string MU_MobileGetExternalDataPath();
std::string MU_MobileGetInternalDataPath();

const void* MU_MobileGetNativeWindow();
const void* MU_MobileGetEglDisplay();
const void* MU_MobileGetEglContext();

// sokol_app's Android backend never checks sapp_request_quit()/sapp_quit()
// from its frame loop (_sapp_android_frame has no such check at all - that
// pump only exists for the win32/x11/emscripten backends), so calling those
// from game code on Android is a no-op: the frame loop just keeps returning
// early forever without the process ever actually exiting. The only real
// exit path Android has is ANativeActivity_finish(), which sokol calls
// itself from its own (now-neutralised) back-key handler. This wraps that
// same public NDK call for our own "really quit" flow (Destroy == true).
void MU_MobileRequestAppQuit();

// Battery percentage 0-100, or -1 if not yet known.
int MU_MobileGetBatteryPercent();
// Wifi RSSI in dBm (roughly -30 excellent .. -90 unusable), or INT32_MIN if
// wifi is off/disconnected.
int MU_MobileGetWifiRssiDbm();

#endif // defined(__ANDROID__) || defined(MU_IOS)
