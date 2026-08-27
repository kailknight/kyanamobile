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

// Battery percentage 0-100, or -1 if not yet known.
int MU_MobileGetBatteryPercent();
// Wifi RSSI in dBm (roughly -30 excellent .. -90 unusable), or INT32_MIN if
// wifi is off/disconnected.
int MU_MobileGetWifiRssiDbm();

#endif // defined(__ANDROID__) || defined(MU_IOS)
