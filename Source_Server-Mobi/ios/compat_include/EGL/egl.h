#pragma once
// iOS has no EGL: sokol_app owns the EAGL context. This covers the few EGL
// names shared code references; extension lookups report "not available".

#ifdef __cplusplus
extern "C" {
#endif

typedef void* EGLDisplay;
typedef void* EGLContext;
typedef void* EGLSurface;
typedef unsigned int EGLBoolean;
typedef void (*__eglMustCastToProperFunctionPointerType)(void);

#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_FALSE 0
#define EGL_TRUE 1

static inline __eglMustCastToProperFunctionPointerType eglGetProcAddress(const char* name)
{
    (void)name;
    return 0;
}

#ifdef __cplusplus
}
#endif
