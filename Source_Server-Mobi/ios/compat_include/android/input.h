#pragma once
// iOS stand-in for the NDK's <android/input.h>: the meta-state bits the shared
// Android key mapping reads. See keycodes.h for why these exist on iOS.
#include <android/keycodes.h>

enum {
    AMETA_NONE = 0,
    AMETA_ALT_ON = 0x02,
    AMETA_SHIFT_ON = 0x01,
    AMETA_CTRL_ON = 0x1000,
    AMETA_META_ON = 0x10000,
    AMETA_CAPS_LOCK_ON = 0x100000,
};
