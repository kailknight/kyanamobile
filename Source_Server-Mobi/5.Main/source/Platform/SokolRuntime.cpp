#if defined(__ANDROID__) || defined(MU_IOS)

#define SOKOL_GLES3
#define SOKOL_APP_IMPL

#if defined(__ANDROID__)
// sokol_app's Android backend hard-quits on the back key:
//
//     if (AKeyEvent_getKeyCode(e) == AKEYCODE_BACK) {
//         /* FIXME: this should be hooked into a "really quit?" mechanism ... */
//         _sapp_android_shutdown();
//
// - its own FIXME admits the missing confirmation hook. That runs inside
// sokol's input callback, before any of this project's code sees the event,
// which is why remapping the key downstream (android_main.cpp's Android
// keycode tables) had no effect: the key never got that far.
//
// Neutralised by making that comparison unmatchable, since AKEYCODE_BACK is
// only ever used in that one shutdown test in the whole header. sokol then
// reports the event unhandled, and the framework falls back to normal
// dispatch - i.e. Activity.onBackPressed(), which MuMainNativeActivity
// overrides to forward the key through the existing Java key bridge
// (nativeOnKeyEvent) instead of finishing the activity. That bridge is
// already how every other key reaches the game: sokol's Android backend
// forwards no key events at all, only touch.
//
// Done here rather than by editing sokol_app.h because that header lives in
// a shared checkout outside this repo (SOKOL_ROOT in CMakeLists.txt), so a
// local edit there would be unversioned and would leak into anything else
// built against it. This file owns SOKOL_APP_IMPL, so the override is scoped
// to our build.
#include <android/keycodes.h>
#undef AKEYCODE_BACK
#define AKEYCODE_BACK (-0x7FFFFFFF)
#endif

#include <sokol_app.h>

const void* MU_MobileGetNativeWindow()
{
#if defined(__ANDROID__)
    return _sapp.android.current.window;
#else
    return nullptr;
#endif
}

const void* MU_MobileGetEglDisplay()
{
#if defined(__ANDROID__)
    return sapp_egl_get_display();
#else
    return nullptr;
#endif
}

const void* MU_MobileGetEglContext()
{
#if defined(__ANDROID__)
    return sapp_egl_get_context();
#else
    return nullptr;
#endif
}

void MU_MobileRequestAppQuit()
{
#if defined(__ANDROID__)
    if (_sapp.android.activity != nullptr)
    {
        ANativeActivity_finish(_sapp.android.activity);
    }
#endif
}

void MU_MobileKeepRenderingWhileUnfocused()
{
#if defined(__ANDROID__)
    // Only this file can reach _sapp_android_msg - it is _SOKOL_PRIVATE, and
    // this is the translation unit that defines SOKOL_APP_IMPL.
    //
    // MSG_FOCUS is the same message sokol posts for a real focus gain: the
    // sokol thread picks it up in its own switch, sets has_focus itself, and
    // the write wakes the looper out of its indefinite poll. Nothing here
    // touches _sapp from the UI thread beyond the pipe write, which is what
    // the pipe is for.
    //
    // Ordering is what makes this work. NativeActivity.onWindowFocusChanged
    // calls through to sokol first, so MSG_NO_FOCUS is already in the pipe and
    // our MSG_FOCUS lands behind it - the pump applies both in order and ends
    // on focused. A genuine background trip is unaffected: MSG_PAUSE clears
    // has_resumed, and no amount of focus says otherwise.
    if (!_sapp.valid || !_sapp.android.is_thread_started || _sapp.android.is_thread_stopping)
    {
        return;
    }

    _sapp_android_msg(_SOKOL_ANDROID_MSG_FOCUS);
#endif
}

#endif // defined(__ANDROID__) || defined(MU_IOS)
