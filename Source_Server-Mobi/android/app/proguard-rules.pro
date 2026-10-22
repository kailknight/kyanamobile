# This app's Java layer is a thin bridge, not app logic - the whole game
# runs in native C++ (Source_Server-Mobi/5.Main/source). Both Java packages
# below are called into from native code by string-resolved method name
# (JNIEnv::GetStaticMethodID/GetMethodID with a literal name+signature, not a
# compile-time-checked reference), so R8 renaming or stripping any of their
# members would silently break the JNI bridge at runtime - no compiler error,
# just a null jmethodID and a swallowed exception (see the ExceptionClear()
# calls next to every GetMethodID in the native side). Keeping both packages
# wholesale is deliberate, not a shortcut: this project's entire Java surface
# is these two packages, so there is effectively nothing else app-level for
# R8 to shrink/obfuscate anyway - the real size/security win from enabling R8
# is resource shrinking and optimizing the androidx.appcompat dependency,
# both of which still apply with these classes kept.

# com.muonline.client.MuAudio - looked up via
# env->FindClass("com/muonline/client/MuAudio") in
# Source_Server-Mobi/5.Main/source/android_link_stubs.cpp, then every method
# on it via env->GetStaticMethodID(g_muAudioClass, method, sig) where method/
# sig are passed in per call site (CallMuAudioVoid helper) - there is no
# single enumerable list of names to keep individually.
-keep class com.muonline.client.MuAudio { *; }

# com.muonline.client.MuMainNativeActivity - MU_RegisterKeyboardBridge in
# Platform/MobilePlatform.cpp takes the class via GetObjectClass(activity)
# (safe under obfuscation, reflects the real runtime type either way), but
# then resolves showKeyboardFromNative/hideKeyboardFromNative/
# getBatteryPercentFromNative/getWifiRssiFromNative by literal string name -
# those four specifically must survive renaming.
-keep class com.muonline.client.MuMainNativeActivity { *; }
-keep class com.muonline.client.MuMainActivity { *; }
-keep class com.muonline.client.PreloadActivity { *; }

# org.libsdl.app.* - vendored SDL2 Android glue. SDL's native side (in the
# prebuilt libSDL2.so, not this project's own source) calls into these
# classes the same way - by string-resolved method name - and is not
# something this project's proguard rules can trace call sites for. This is
# SDL's own long-standing recommendation for Android proguard configs.
-keep class org.libsdl.app.** { *; }

# Baseline safety net: any class anywhere with a native method keeps its
# name and that method's name+signature, so implicit JNI registration
# (matching by mangled name, the default when a class isn't registered via
# JNI_OnLoad's RegisterNatives) keeps resolving correctly.
-keepclasseswithmembernames class * {
    native <methods>;
}

# Standard Android component keep (AGP already applies an equivalent rule via
# its default consumer rules for manifest-declared components; kept here
# explicitly since this project's Activities are also covered by the
# broader -keep above and this makes the intent self-documenting).
-keep public class * extends android.app.Activity
