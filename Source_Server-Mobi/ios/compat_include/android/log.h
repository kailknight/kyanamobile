#pragma once
// iOS stand-in for the NDK's <android/log.h>. Shared mobile code logs through
// __android_log_print; on iOS that goes to stderr, which Xcode's console shows.

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum android_LogPriority {
    ANDROID_LOG_UNKNOWN = 0,
    ANDROID_LOG_DEFAULT,
    ANDROID_LOG_VERBOSE,
    ANDROID_LOG_DEBUG,
    ANDROID_LOG_INFO,
    ANDROID_LOG_WARN,
    ANDROID_LOG_ERROR,
    ANDROID_LOG_FATAL,
    ANDROID_LOG_SILENT,
} android_LogPriority;

static inline int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap)
{
    static const char kLevel[] = "??VDIWEFS";
    fprintf(stderr, "%c/%s: ", kLevel[(prio >= 0 && prio <= 8) ? prio : 0], tag ? tag : "");
    int n = vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    return n;
}

static inline int __android_log_print(int prio, const char* tag, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = __android_log_vprint(prio, tag, fmt, ap);
    va_end(ap);
    return n;
}

static inline int __android_log_write(int prio, const char* tag, const char* text)
{
    return __android_log_print(prio, tag, "%s", text ? text : "");
}

#ifdef __cplusplus
}
#endif
