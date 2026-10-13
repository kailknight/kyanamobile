#pragma once
// macOS has desktop OpenGL only (4.1 core at most, and deprecated). Shared code
// includes <GLES3/gl32.h> for the Android and iOS builds; gl_compat.cpp
// translates the ES shaders and emulates the few ES-only calls, so this only
// has to declare the core GL names. Everything the code uses beyond ES 3.0 is
// loaded at runtime and falls back without it.
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#ifndef GL_APIENTRY
#define GL_APIENTRY
#endif
#ifndef GL_APIENTRYP
#define GL_APIENTRYP GL_APIENTRY*
#endif
