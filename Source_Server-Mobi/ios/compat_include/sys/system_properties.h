#pragma once
// iOS stand-in for bionic's <sys/system_properties.h>. Android reads build
// properties to detect emulators; iOS has none, so every lookup is empty.

#define PROP_VALUE_MAX 92

#ifdef __cplusplus
extern "C" {
#endif

static inline int __system_property_get(const char* name, char* value)
{
    (void)name;
    if (value) value[0] = '\0';
    return 0;
}

#ifdef __cplusplus
}
#endif
