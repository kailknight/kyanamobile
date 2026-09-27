#pragma once
// iOS OpenGL ES stops at 3.0. Shared code includes <GLES3/gl32.h> for the
// Android build; it only uses ES 3.0 entry points apart from what gl_compat.cpp
// loads at runtime through eglGetProcAddress (and falls back without).
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>

#ifndef GL_APIENTRY
#define GL_APIENTRY
#endif
#ifndef GL_APIENTRYP
#define GL_APIENTRYP GL_APIENTRY*
#endif
