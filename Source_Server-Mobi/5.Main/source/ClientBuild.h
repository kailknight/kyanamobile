// ClientBuild.h - what this client tells the server it is.
//
// BOTH platforms report the same thing in the same unit: their own build time.
// That is deliberate, and it is a change from how this started.
//
// It first used the APK's versionCode on Android and a link-time number on PC.
// Two different units behind two similarly-named config keys is a trap, and it
// caught its first victim immediately - MinClientBuildPC = 20 looked like
// "build 20" and silently allowed everything, because the PC unit was a date.
//
// Now both report YYMMDDHHMM from build time, which is the same information the
// displayed stamp carries, just ordered so that larger means newer. The server's
// config takes the stamp's own 1.0.DDMMYY.HHMM spelling and converts, so what
// you type in the ini is what you read in the client.
//
//   PC       the timestamp the linker wrote into the exe's PE header
//   Android  compile time, with CMakeLists.txt touching ClientBuild.cpp before
//            every build so it cannot go stale
//
// Both are automatic and monotonic - a later build always yields a larger
// number - so there is no constant to maintain and no way to forget to bump one.
//
// The APK's versionCode is still available as MU_ANDROID_VERSION_CODE and is
// still what the Play Store cares about; it just is not what the gate compares,
// because matching the displayed stamp matters more here than matching the
// store listing.
//
// A client too old to send this packet at all reports nothing, and the server
// treats "said nothing" as its own refusal case - that is how builds predating
// the feature are caught, on both platforms.
//
// Sent in packet 0xD3:0x7D, immediately before the login packet
// (SendRequestLogIn in wsclientinline.h).

#pragma once

// Human-readable "which build is this", as v.1.0.DDMMYY.HHMM from compile time.
// Shown in the PC window title and in the Android status line.
//
// NOT the same thing as MU_CLIENT_BUILD below, and not interchangeable with it:
// this is an identifier for a person reading it, never compared by a server.
// Implemented in ClientBuild.cpp because __DATE__/__TIME__ expand per
// translation unit, so a header would hand every file a slightly different one.
const char* MU_GetClientBuildStamp();

// Platform tag on the wire. The server only applies a minimum to Android, but
// it logs the tag either way, so a refusal line says which client it was.
#if defined(__ANDROID__) || defined(MU_IOS)
#define MU_CLIENT_PLATFORM      1
#else
#define MU_CLIENT_PLATFORM      0
#endif

#if defined(__ANDROID__) || defined(MU_IOS)

// Set by CMake from app/build.gradle's versionCode, so this IS the APK's own
// number and cannot drift from it.
#ifndef MU_ANDROID_VERSION_CODE
// A hard error rather than a default. A default of 0 would compile an APK that
// reports "oldest possible build" and is refused by every server with a minimum
// set - silently, and looking like a server fault.
#error "MU_ANDROID_VERSION_CODE is not defined - the CMake argument in app/build.gradle (forwarded by cpp/CMakeLists.txt) is missing, and without it this APK would report build 0 and be refused by the outdated-client gate."
#endif

#endif

// YYMMDDHHMM of this build - the number the server compares against
// MinClientBuildAndroid / MinClientBuildPC, and the same instant the stamp above
// shows. DWORD rather than int because 2609191943 is past the signed maximum.
DWORD MU_GetClientBuildNumber();
