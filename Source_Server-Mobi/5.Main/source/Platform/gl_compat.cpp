// =============================================================================
// Platform/gl_compat.cpp
// OpenGL fixed-function emulation on GLES 3.2 for Android.
// Implements immediate mode, matrix stack, and basic shaders.
// =============================================================================

#if defined(__ANDROID__) || defined(MU_IOS)

#include "gl_compat.h"
#include "MobileTime.h"
#include <GLES3/gl32.h>
#include <EGL/egl.h>
#include <android/log.h>

// Legacy client-state enums not present in GLES2 headers
#ifndef GL_VERTEX_ARRAY
#  define GL_VERTEX_ARRAY        0x8074
#endif
#ifndef GL_TEXTURE_COORD_ARRAY
#  define GL_TEXTURE_COORD_ARRAY 0x8078
#endif
#ifndef GL_COLOR_ARRAY
#  define GL_COLOR_ARRAY         0x8076
#endif
#ifndef GL_NORMAL_ARRAY
#  define GL_NORMAL_ARRAY        0x8075
#endif
#include <cmath>
#include <cstring>
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <algorithm>

#define LOG_TAG "GL_Compat"

// Completely wipe out logging to ensure the phone spends 0% CPU processing text strings
#define LOGI(...) ((void)0)
#define LOGE(...) ((void)0)

/* Comment out the old block completely
#if defined(MU_ANDROID_DISABLE_LOG)
#define LOGI(...) ((void)0)
#define LOGE(...) ((void)0)
#else
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#endif
*/

// =============================================================================
// Matrix math (column-major, same as OpenGL)
// =============================================================================
typedef float Mat4[16];

static void mat4_identity(Mat4 m) {
    memset(m, 0, sizeof(Mat4));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_copy(Mat4 dst, const Mat4 src) {
    memcpy(dst, src, sizeof(Mat4));
}

static void mat4_multiply(Mat4 out, const Mat4 a, const Mat4 b) {
    Mat4 tmp;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[k*4+r] * b[c*4+k];
            tmp[c*4+r] = s;
        }
    mat4_copy(out, tmp);
}

// =============================================================================
// Matrix stacks
// =============================================================================
static const int MAX_STACK = 32;

static Mat4  s_mvStack[MAX_STACK];
static int   s_mvDepth = 0;
static Mat4  s_projStack[8];
static int   s_projDepth = 0;

static int   s_matrixMode = 0x1700; // GL_MODELVIEW

static Mat4* CurrentStack()  { return s_matrixMode == 0x1701 ? s_projStack : s_mvStack; }
static int&  CurrentDepth()  { return s_matrixMode == 0x1701 ? s_projDepth : s_mvDepth; }

// Combined MVP = Proj * Modelview
static void GetMVP(Mat4 mvp) {
    mat4_multiply(mvp, s_projStack[s_projDepth], s_mvStack[s_mvDepth]);
}

// =============================================================================
// Vertex format
// =============================================================================
struct IMVertex {
    float x, y, z;
    float r, g, b, a;
    float u, v;
};

// =============================================================================
// Immediate mode state
// =============================================================================
static std::vector<IMVertex> s_verts;
static std::vector<IMVertex> s_expandedVerts;
static std::vector<IMVertex> s_arrayVerts;
static std::vector<IMVertex> s_batchVerts;
static std::vector<IMVertex> s_batchConvertedVerts;
static IMVertex s_cur;
static GLenum   s_primMode;
static bool     s_inBegin = false;
static bool     s_hasBatch = false;
static GLenum   s_batchPrimMode = 0;
static const size_t kMaxBatchVerts = 300000;
static constexpr bool kBakeModelViewForImmediate = true;
// When false, legacy glDrawArrays always goes through the converted batch path.
// This reduces driver draw-call pressure on Android emulators at the cost of
// some CPU-side vertex conversion work.
// Disabled: Mali GLES 3.x DMA reads client pointers asynchronously, causing SEGV_ACCERR
// when the pointer sits at a page boundary. Force all draws through VBO upload instead.
static constexpr bool kDefaultPreferDirectVertexArrays = false;

// When true, skip glBufferData(nullptr) buffer orphaning before each VBO upload.
// Safe on software renderers (SwiftShader) where there is no real GPU pipeline;
// the driver-side malloc it triggers wastes ~105 cycles/frame on the emulator.
// Must remain false on real hardware (Adreno/Mali) to avoid GPU pipeline stalls.
static bool s_skipVBOOrphan = false;

void GL_SetSkipVBOOrphan(bool skip) { s_skipVBOOrphan = skip; }
static constexpr GLenum kCompatModelViewMode = 0x1700;  // GL_MODELVIEW
static constexpr GLenum kCompatProjectionMode = 0x1701; // GL_PROJECTION

// =============================================================================
// Shader programs
// =============================================================================
static GLuint s_prog    = 0;   // alpha-tested/discard textured+colored shader
static GLuint s_progOpaque = 0; // same, but no discard - keeps Early-Z/LRZ alive on Adreno/Mali
static GLuint s_vbo     = 0;
static GLuint s_ebo     = 0;

// Attribute locations
static GLint  s_aPos    = -1;
static GLint  s_aColor  = -1;
static GLint  s_aUV     = -1;

// Uniform locations
static GLint  s_uMVP        = -1;
static GLint  s_uSampler    = -1;
static GLint  s_uUseTexture = -1;
static GLint  s_uAlphaTest  = -1;
static GLint  s_uAlphaRef   = -1;

// Debug counters
static int    s_drawCallCount = 0;
static int    s_totalVertices = 0;
static int    s_imDrawCalls = 0;
static int    s_vaDirectDrawCalls = 0;
static int    s_vaConvertedDrawCalls = 0;
static int    s_quadIndexedDrawCalls = 0;
static int    s_quadExpandedDrawCalls = 0;
static bool   s_preferDirectVertexArrays = kDefaultPreferDirectVertexArrays;

// Why the pending immediate batch got cut short. Batching itself already works
// - consecutive glBegin/glEnd spans merge, and modelview changes deliberately
// do not break a batch because GL_Vertex3f bakes the modelview into the
// position. What is left is state changes, and this says which ones actually
// cost us, so the fix goes where the draws are instead of where they look like
// they should be. Temporary scaffolding, same as the g_Prof* counters.
enum GLFlushCause {
    kFlushCauseOther = 0,
    kFlushCauseTexBind,     // glBindTexture with a different texture
    kFlushCauseTexUpload,   // glTexImage2D / glTexSubImage2D
    kFlushCauseBlend,       // glBlendFunc
    kFlushCauseDepth,       // glDepthFunc / glDepthMask
    kFlushCauseEnable,      // glEnable / glDisable of texturing or alpha test
    kFlushCauseAlphaRef,    // glAlphaFunc reference value
    kFlushCauseProjection,  // projection matrix mutation
    kFlushCauseFrame,       // end of frame / explicit GL_FlushPending
    kFlushCauseUnbatchable, // primitive that cannot be merged at all
    kFlushCausePrimMode,    // different primitive mode than the pending batch
    kFlushCauseBatchFull,   // pending batch hit its vertex limit
    kFlushCauseCount
};
static int s_flushCauseCounts[kFlushCauseCount] = { 0 };

// Which GL draw-call site each draw came from. Two rounds of inferring the
// draw-path split from aggregate counters produced two wrong answers - im/vaConv
// count inputs (glEnd spans, mesh appends), not draws - so this counts the
// actual glDraw* calls where they happen.
// 0 batched immediate (DrawPreparedVerts)   1 client-array direct
// 2 quad indexed   3 legacy vertex-array   4 bulk indexed tris
// 5 bulk tris      6 skinned mesh
static int s_drawSite[10] = { 0 };

// State flags
static bool   s_texture2DEnabled = true;
static bool   s_alphaTestEnabled = false;
static float  s_alphaRef         = 0.0f;
static GLenum s_alphaFunc        = 0x0207; // GL_ALWAYS

// ── Current bound texture (to detect whether we need texture sampling) ──────
// We rely on the game calling glBindTexture to set this, which is a real GLES2
// call. We only need to know if ANY texture is bound.
static GLuint s_boundTexture = 0;
static GLsizeiptr s_vboCapacity = 0;

// =============================================================================
// Persistent-mapped streaming ring (attempt 3 on this bottleneck)
//
// Measured 2026-09-09 with simpleperf --app: the per-draw glBufferData(...,
// GL_STREAM_DRAW) orphan just above costs ~62% of ALL CPU cycles in the
// process (gsl_memory_alloc_pure_64 -> kgsl_sharedmem_alloc -> ioctl), on real
// Adreno hardware, in a normal busy scene - not a stress test. libmain.so
// itself was only 11%. The orphan trades that for safety: a fresh allocation
// can never collide with whatever the GPU is still reading from the previous
// draw's storage.
//
// Two earlier attempts at a plain ring (3 buffers, rotated per frame) both
// made things worse, and both failed for the same underlying reason - a GL
// call was still made *per draw* to get data in:
//   - glMapBufferRange/glUnmapBuffer per draw: the map/unmap pair itself has
//     real per-call driver cost (cache-direction bookkeeping) that scaled
//     with ~900 draws/frame. Measured: StreamVertexData 93.6% inclusive,
//     libc 21.5% (vs. 3% baseline). ~2 FPS.
//   - glBufferSubData at an advancing offset, no map: GL's spec cannot know
//     the ring guarantees non-overlap, so the driver must protect draws still
//     queued against that buffer - it stalls or shadow-copies, the same class
//     of cost the orphan exists to avoid. Visible flicker, FPS still worse.
//
// This is the standard fix for exactly this problem: map each ring buffer
// ONCE, at creation (glBufferStorageEXT + GL_MAP_PERSISTENT_BIT), and keep
// the returned pointer for the buffer's whole lifetime. Every draw after that
// is a plain memcpy into already-mapped memory - no GL call, so none of the
// per-call cost above exists. Safety no longer depends on "3 buffers should
// be enough frames" - a real GPU fence (glFenceSync/glClientWaitSync) is
// placed on a slot when its frame's draws are done, and waited on the next
// time that same slot comes back around, so reuse is provably safe rather
// than assumed.
//
// GL_EXT_buffer_storage is an extension, not core GLES - some devices (older
// Mali in particular, matching the caution in the comment on
// g_ForceSkipVBOOrphan in android_main.cpp) will not have it. Detected at
// init; when absent, s_streamRingAvailable stays false and every draw below
// falls straight back to the plain per-draw orphan above, unchanged from
// today's shipped behaviour. This is a strict opt-in fast path, never a
// required one.
static constexpr int kStreamRingSize = 3;

// Comfortably above one full kMaxBatchVerts batch (300000 * 36 bytes =
// ~10.3 MB) so a single oversized batch does not by itself force the
// overflow fallback below. 3 slots * 12 MB = 36 MB, trivial next to this
// app's LARGE_HEAP manifest flag, and only ever allocated on devices where
// the extension check below actually passes.
static constexpr GLsizeiptr kStreamRingSlotBytes = 12 * 1024 * 1024;

struct StreamRingSlot {
    GLuint vbo = 0;
    unsigned char* mapped = nullptr;
    GLsync fence = nullptr;   // sync object: "GPU is done reading this slot's LAST frame of data"
};

static StreamRingSlot s_streamRing[kStreamRingSize];
static int            s_streamIdx = 0;
static GLsizeiptr     s_streamOffset = 0;
static bool           s_streamRingAvailable = false;

bool GL_IsStreamRingActive() { return s_streamRingAvailable; }

#ifndef GL_MAP_PERSISTENT_BIT_EXT
#define GL_MAP_PERSISTENT_BIT_EXT 0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT_EXT
#define GL_MAP_COHERENT_BIT_EXT 0x0080
#endif

typedef void (GL_APIENTRY* MU_PFNGLBUFFERSTORAGEEXTPROC)(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags);
static MU_PFNGLBUFFERSTORAGEEXTPROC s_glBufferStorageEXT = nullptr;

// Base vertex is ES 3.2. It links fine on-device (the driver exposes it via
// eglGetProcAddress) but the NDK's API-21 stub library (minSdk here) has no
// static entry point for it, so the direct symbol fails at link time - see
// the build failure this produced the first time this was tried. Resolved at
// runtime instead; devices where it comes back null simply never set
// s_streamRingAvailable (see GL_Compat_Init), so DrawQuadIndicesFrom's
// fallback branch below is unreachable with a non-zero firstVertex on them.
typedef void (GL_APIENTRY* MU_PFNGLDRAWELEMENTSBASEVERTEX)(
    GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex);
static MU_PFNGLDRAWELEMENTSBASEVERTEX s_glDrawElementsBaseVertex = nullptr;

// Every indexed-quad draw site funnels through here so there is exactly one
// place that knows whether base-vertex is actually available.
static inline void DrawQuadIndicesFrom(GLsizei indexCount, GLint firstVertex) {
    if (firstVertex != 0 && s_glDrawElementsBaseVertex) {
        s_glDrawElementsBaseVertex(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, 0, firstVertex);
    } else {
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, 0);
    }
}

static void DestroyStreamRing() {
    for (int i = 0; i < kStreamRingSize; ++i) {
        StreamRingSlot& slot = s_streamRing[i];
        if (slot.fence) {
            glDeleteSync(slot.fence);
            slot.fence = nullptr;
        }
        if (slot.vbo) {
            if (slot.mapped) {
                glBindBuffer(GL_ARRAY_BUFFER, slot.vbo);
                glUnmapBuffer(GL_ARRAY_BUFFER);
            }
            glDeleteBuffers(1, &slot.vbo);
            slot.vbo = 0;
        }
        slot.mapped = nullptr;
    }
    s_streamIdx = 0;
    s_streamOffset = 0;
    s_streamRingAvailable = false;
}

// Called once from GL_Compat_Init, after s_vbo/s_ebo exist (the ring's
// fallback path binds s_vbo, so it must be valid before this can decide
// anything). Leaves s_streamRingAvailable false - i.e. today's unchanged
// per-draw-orphan behaviour - on any failure at any step.
static void InitStreamRing() {
    s_glDrawElementsBaseVertex = reinterpret_cast<MU_PFNGLDRAWELEMENTSBASEVERTEX>(
        eglGetProcAddress("glDrawElementsBaseVertex"));
    s_glBufferStorageEXT = reinterpret_cast<MU_PFNGLBUFFERSTORAGEEXTPROC>(
        eglGetProcAddress("glBufferStorageEXT"));

    int glMajor = 0, glMinor = 0;
    if (const char* glVer = reinterpret_cast<const char*>(glGetString(GL_VERSION))) {
        sscanf(glVer, "OpenGL ES %d.%d", &glMajor, &glMinor);
    }
    const bool hasBaseVertex = s_glDrawElementsBaseVertex != nullptr
        && ((glMajor > 3) || (glMajor == 3 && glMinor >= 2));

    bool hasBufferStorage = false;
    if (s_glBufferStorageEXT != nullptr) {
        if (const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS))) {
            hasBufferStorage = strstr(ext, "GL_EXT_buffer_storage") != nullptr;
        }
    }

    LOGI("Stream ring capability: baseVertex=%d bufferStorage=%d (GLES %d.%d)",
         hasBaseVertex ? 1 : 0, hasBufferStorage ? 1 : 0, glMajor, glMinor);

    if (!hasBaseVertex || !hasBufferStorage) {
        return;
    }

    const GLbitfield storageFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT;
    const GLbitfield mapFlags     = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT;

    for (int i = 0; i < kStreamRingSize; ++i) {
        StreamRingSlot& slot = s_streamRing[i];
        glGenBuffers(1, &slot.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, slot.vbo);
        s_glBufferStorageEXT(GL_ARRAY_BUFFER, kStreamRingSlotBytes, nullptr, storageFlags);
        if (glGetError() != GL_NO_ERROR) {
            LOGE("Stream ring: glBufferStorageEXT failed on slot %d, falling back", i);
            DestroyStreamRing();
            return;
        }

        slot.mapped = reinterpret_cast<unsigned char*>(
            glMapBufferRange(GL_ARRAY_BUFFER, 0, kStreamRingSlotBytes, mapFlags));
        if (slot.mapped == nullptr) {
            LOGE("Stream ring: glMapBufferRange failed on slot %d, falling back", i);
            DestroyStreamRing();
            return;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    s_streamIdx = 0;
    s_streamOffset = 0;
    s_streamRingAvailable = true;
    LOGI("Stream ring: active, %d slots x %lld bytes", kStreamRingSize, (long long)kStreamRingSlotBytes);
}
static bool   s_samplerUniformInitialized = false;
static bool   s_hasLastMvp = false;
static Mat4   s_lastMvp = { 0 };
static bool   s_mvpDirty = true;
static Mat4   s_cachedMvp = { 0 };
static int    s_lastUseTex = -1;
static int    s_lastAlphaTest = -1;
static float  s_lastAlphaRef = -1.0f;
// ── Bitfield cap state cache (replaces unordered_map for zero-alloc perf) ──
enum CapBit : uint32_t {
    CAP_DEPTH_TEST   = 1u << 0,
    CAP_BLEND        = 1u << 1,
    CAP_CULL_FACE    = 1u << 2,
    CAP_SCISSOR_TEST = 1u << 3,
    CAP_STENCIL_TEST = 1u << 4,
    CAP_DITHER       = 1u << 5,
    CAP_POLYGON_OFFSET_FILL = 1u << 6,
    CAP_SAMPLE_ALPHA_TO_COVERAGE = 1u << 7,
    CAP_SAMPLE_COVERAGE = 1u << 8,
};
static uint32_t s_capBits = 0;
static uint32_t s_capBitsKnown = 0;  // tracks which bits have been set at least once

static inline uint32_t CapToMask(GLenum cap) {
    switch (cap) {
        case GL_DEPTH_TEST:   return CAP_DEPTH_TEST;
        case GL_BLEND:        return CAP_BLEND;
        case GL_CULL_FACE:    return CAP_CULL_FACE;
        case GL_SCISSOR_TEST: return CAP_SCISSOR_TEST;
        case GL_STENCIL_TEST: return CAP_STENCIL_TEST;
        case GL_DITHER:       return CAP_DITHER;
        case GL_POLYGON_OFFSET_FILL: return CAP_POLYGON_OFFSET_FILL;
        case GL_SAMPLE_ALPHA_TO_COVERAGE: return CAP_SAMPLE_ALPHA_TO_COVERAGE;
        case GL_SAMPLE_COVERAGE: return CAP_SAMPLE_COVERAGE;
        default: return 0;
    }
}
static GLenum s_lastBlendSrc = 0xFFFFFFFFu;
static GLenum s_lastBlendDst = 0xFFFFFFFFu;
static GLenum s_lastDepthFunc = 0xFFFFFFFFu;
static GLboolean s_lastDepthMask = 2;
static GLboolean s_lastColorMaskR = 2;
static GLboolean s_lastColorMaskG = 2;
static GLboolean s_lastColorMaskB = 2;
static GLboolean s_lastColorMaskA = 2;
static bool s_hasClearColor = false;
static float s_lastClearR = 0.0f;
static float s_lastClearG = 0.0f;
static float s_lastClearB = 0.0f;
static float s_lastClearA = 0.0f;
static GLuint s_boundArrayBuffer = 0;
static GLuint s_boundElementArrayBuffer = 0;
static GLuint s_currentProgram = 0;
static bool s_imAttribValid = false;
static std::vector<uint16_t> s_quadIndices;
static size_t s_quadIndexCapacityQuads = 0;

static void DrawVertexList(const std::vector<IMVertex>& inputVerts, GLenum primMode, bool modelViewBaked);
static void FlushPendingImmediateBatch(GLFlushCause cause = kFlushCauseOther);

static inline bool IsProjectionMatrixActive() {
    return s_matrixMode == static_cast<int>(kCompatProjectionMode);
}

static inline bool MatrixMutationRequiresImmediateFlush() {
    return !kBakeModelViewForImmediate || IsProjectionMatrixActive();
}

static inline void TransformByCurrentModelView(float x, float y, float z, float& outX, float& outY, float& outZ) {
    if (!kBakeModelViewForImmediate) {
        outX = x;
        outY = y;
        outZ = z;
        return;
    }

    const Mat4& mv = s_mvStack[s_mvDepth];
    const float tx = mv[0] * x + mv[4] * y + mv[8]  * z + mv[12];
    const float ty = mv[1] * x + mv[5] * y + mv[9]  * z + mv[13];
    const float tz = mv[2] * x + mv[6] * y + mv[10] * z + mv[14];
    const float tw = mv[3] * x + mv[7] * y + mv[11] * z + mv[15];

    if (fabsf(tw) > 1e-6f && fabsf(tw - 1.0f) > 1e-6f) {
        const float invW = 1.0f / tw;
        outX = tx * invW;
        outY = ty * invW;
        outZ = tz * invW;
    } else {
        outX = tx;
        outY = ty;
        outZ = tz;
    }
}

static bool IsBatchableImmediateMode(GLenum mode) {
    return mode == GL_TRIANGLES ||
           mode == GL_LINES ||
           mode == GL_POINTS ||
           mode == 0x0007 /* GL_QUADS */;
}

static bool ConvertPrimitiveForBatching(const std::vector<IMVertex>& inputVerts,
                                        GLenum inMode,
                                        const IMVertex*& outVerts,
                                        size_t& outCount,
                                        GLenum& outMode) {
    outVerts = inputVerts.data();
    outCount = inputVerts.size();
    outMode = inMode;
    s_batchConvertedVerts.clear();

    switch (inMode) {
    case GL_TRIANGLES:
    case GL_LINES:
    case GL_POINTS:
    case 0x0007: /* GL_QUADS */
        return outCount > 0;

    case GL_TRIANGLE_FAN:
    case 0x0009: /* GL_POLYGON */ {
        if (inputVerts.size() < 3u) {
            return false;
        }
        const size_t triCount = inputVerts.size() - 2u;
        s_batchConvertedVerts.reserve(triCount * 3u);
        const IMVertex& v0 = inputVerts[0u];
        for (size_t i = 1u; i + 1u < inputVerts.size(); ++i) {
            s_batchConvertedVerts.push_back(v0);
            s_batchConvertedVerts.push_back(inputVerts[i]);
            s_batchConvertedVerts.push_back(inputVerts[i + 1u]);
        }
        outVerts = s_batchConvertedVerts.data();
        outCount = s_batchConvertedVerts.size();
        outMode = GL_TRIANGLES;
        return outCount > 0;
    }

    case GL_TRIANGLE_STRIP: {
        if (inputVerts.size() < 3u) {
            return false;
        }
        const size_t triCount = inputVerts.size() - 2u;
        s_batchConvertedVerts.reserve(triCount * 3u);
        for (size_t i = 0u; i + 2u < inputVerts.size(); ++i) {
            if ((i & 1u) == 0u) {
                s_batchConvertedVerts.push_back(inputVerts[i + 0u]);
                s_batchConvertedVerts.push_back(inputVerts[i + 1u]);
                s_batchConvertedVerts.push_back(inputVerts[i + 2u]);
            } else {
                // Keep winding consistent with strip parity.
                s_batchConvertedVerts.push_back(inputVerts[i + 1u]);
                s_batchConvertedVerts.push_back(inputVerts[i + 0u]);
                s_batchConvertedVerts.push_back(inputVerts[i + 2u]);
            }
        }
        outVerts = s_batchConvertedVerts.data();
        outCount = s_batchConvertedVerts.size();
        outMode = GL_TRIANGLES;
        return outCount > 0;
    }

    case GL_LINE_STRIP: {
        if (inputVerts.size() < 2u) {
            return false;
        }
        s_batchConvertedVerts.reserve((inputVerts.size() - 1u) * 2u);
        for (size_t i = 0u; i + 1u < inputVerts.size(); ++i) {
            s_batchConvertedVerts.push_back(inputVerts[i]);
            s_batchConvertedVerts.push_back(inputVerts[i + 1u]);
        }
        outVerts = s_batchConvertedVerts.data();
        outCount = s_batchConvertedVerts.size();
        outMode = GL_LINES;
        return outCount > 0;
    }

    case GL_LINE_LOOP: {
        if (inputVerts.size() < 2u) {
            return false;
        }
        s_batchConvertedVerts.reserve(inputVerts.size() * 2u);
        for (size_t i = 0u; i + 1u < inputVerts.size(); ++i) {
            s_batchConvertedVerts.push_back(inputVerts[i]);
            s_batchConvertedVerts.push_back(inputVerts[i + 1u]);
        }
        s_batchConvertedVerts.push_back(inputVerts.back());
        s_batchConvertedVerts.push_back(inputVerts.front());
        outVerts = s_batchConvertedVerts.data();
        outCount = s_batchConvertedVerts.size();
        outMode = GL_LINES;
        return outCount > 0;
    }

    default:
        return false;
    }
}

static size_t MaxBatchVertsForMode(GLenum mode) {
    if (mode == 0x0007 /* GL_QUADS */) {
        // uint16 index buffer: max 65535 indices => 16383 full quads (4 verts each).
        return 16383u * 4u;
    }
    return kMaxBatchVerts;
}

// Intercept glBindTexture to track current texture
// (We override via a #define in PlatformGL.h after this header is included)
void GL_TrackBindTexture(GLenum target, GLuint tex) {
    if (target == GL_TEXTURE_2D) {
        if (s_boundTexture == tex) {
            return;
        }
        FlushPendingImmediateBatch(kFlushCauseTexBind);
        s_boundTexture = tex;
    }
    glBindTexture(target, tex);
}

// Text rendering reuses the same GL texture ID and uploads different glyph
// bitmaps into it between RenderBitmap() calls. Without a flush here, all
// quads queued into the immediate batch end up sampling the last-uploaded
// content (visual: every label shows the final string).
// TEMP profiling: counts texture *definitions* (asset loads), not the
// per-string glTexSubImage2D updates the text path does. A frame that hitches
// while this is non-zero is stalling on asset streaming, not on rendering.
int g_ProfTexDefineCount = 0;
unsigned long long g_ProfTexDefineTicks = 0;

// TEMP profiling: Early-Z program split, and the actual depth/stencil format
// the driver gave us for the currently bound framebuffer.
int g_ProfEarlyZOpaqueDraws = 0;
int g_ProfEarlyZDiscardDraws = 0;
// Tested true on MT6878/Mali-G615 and the frame collapsed (FPS 28 -> 6.4), but
// that measurement is NOT trustworthy: the device had drifted into a degraded
// state during the run, and reverting this flag did not restore the frame rate
// either. Only a force-stop and relaunch did. So the collapse was the drift,
// not this flag. The experiment needs redoing from a freshly restarted app if
// anyone wants a real answer.
//
// What the counters next to it did establish, and this part is solid: only
// ~15 of ~190 draws per frame take the no-discard program, so the Early-Z
// split is currently inert for ~92% of the scene.
bool g_ForceEarlyZOpaqueProgram = false;
int g_ProfDepthBits = -1;
int g_ProfStencilBits = -1;

void GL_SampleDepthStencilFormat()
{
    GLint d = 0, s = 0;
    glGetIntegerv(GL_DEPTH_BITS, &d);
    glGetIntegerv(GL_STENCIL_BITS, &s);
    g_ProfDepthBits = static_cast<int>(d);
    g_ProfStencilBits = static_cast<int>(s);
}

void GL_TexImage2D_Compat(GLenum target, GLint level, GLint internalformat,
                          GLsizei width, GLsizei height, GLint border,
                          GLenum format, GLenum type, const void* pixels) {
    const unsigned long long profTexStart =
        static_cast<unsigned long long>(MU_MobilePerfNow());
    ++g_ProfTexDefineCount;
    struct ProfTexScope
    {
        unsigned long long start;
        ~ProfTexScope()
        {
            g_ProfTexDefineTicks +=
                static_cast<unsigned long long>(MU_MobilePerfNow()) - start;
        }
    } profTexScope = { profTexStart };

    if (target == GL_TEXTURE_2D) {
        FlushPendingImmediateBatch(kFlushCauseTexUpload);
    }
#if defined(__ANDROID__) || defined(MU_IOS)
    if (format == GL_RGB || format == GL_RGBA) {
        if (internalformat == 3 || internalformat == 4 || internalformat != static_cast<GLint>(format)) {
            internalformat = static_cast<GLint>(format);
        }
    } else if (internalformat == 3 || internalformat == 4) {
        internalformat = (internalformat == 4) ? GL_RGBA : GL_RGB;
    }
#endif
    glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void GL_TexSubImage2D_Compat(GLenum target, GLint level,
                             GLint xoffset, GLint yoffset,
                             GLsizei width, GLsizei height,
                             GLenum format, GLenum type, const void* pixels) {
    if (target == GL_TEXTURE_2D) {
        FlushPendingImmediateBatch(kFlushCauseTexUpload);
    }
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void GL_BlendFunc_Compat(GLenum sfactor, GLenum dfactor) {
    if (s_lastBlendSrc == sfactor && s_lastBlendDst == dfactor) {
        return;
    }
    FlushPendingImmediateBatch(kFlushCauseBlend);
    glBlendFunc(sfactor, dfactor);
    s_lastBlendSrc = sfactor;
    s_lastBlendDst = dfactor;
}

void GL_DepthFunc_Compat(GLenum func) {
    if (s_lastDepthFunc == func) {
        return;
    }
    FlushPendingImmediateBatch(kFlushCauseDepth);
    glDepthFunc(func);
    s_lastDepthFunc = func;
}

void GL_DepthMask_Compat(GLboolean flag) {
    if (s_lastDepthMask == flag) {
        return;
    }
    FlushPendingImmediateBatch(kFlushCauseDepth);
    glDepthMask(flag);
    s_lastDepthMask = flag;
}

void GL_ColorMask_Compat(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    if (s_lastColorMaskR == red &&
        s_lastColorMaskG == green &&
        s_lastColorMaskB == blue &&
        s_lastColorMaskA == alpha) {
        return;
    }
    FlushPendingImmediateBatch();
    glColorMask(red, green, blue, alpha);
    s_lastColorMaskR = red;
    s_lastColorMaskG = green;
    s_lastColorMaskB = blue;
    s_lastColorMaskA = alpha;
}

void GL_ClearColor_Compat(float red, float green, float blue, float alpha) {
    if (s_hasClearColor &&
        s_lastClearR == red &&
        s_lastClearG == green &&
        s_lastClearB == blue &&
        s_lastClearA == alpha) {
        return;
    }
    glClearColor(red, green, blue, alpha);
    s_lastClearR = red;
    s_lastClearG = green;
    s_lastClearB = blue;
    s_lastClearA = alpha;
    s_hasClearColor = true;
}

void GL_Clear_Compat(GLbitfield mask) {
    FlushPendingImmediateBatch();
    glClear(mask);
}

static inline void BindArrayBufferCached(GLuint buffer) {
    if (s_boundArrayBuffer != buffer) {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        s_boundArrayBuffer = buffer;
        // glVertexAttribPointer captures whichever buffer is bound when it is
        // called. Until the stream ring, this was always the same s_vbo, so
        // BindImmediateVertexAttribLayout's cache was safe to leave untouched
        // by a rebind - now that draws rotate across ring slots (and can fall
        // back to s_vbo mid-session), a real rebind must force the layout to
        // be re-issued against the newly bound buffer.
        s_imAttribValid = false;
    }
}

static inline void BindElementArrayBufferCached(GLuint buffer) {
    if (s_boundElementArrayBuffer != buffer) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
        s_boundElementArrayBuffer = buffer;
    }
}

static inline void UseProgramCached(GLuint program) {
    if (s_currentProgram != program) {
        glUseProgram(program);
        s_currentProgram = program;
        // Each GL program object owns independent uniform storage, so
        // switching between s_prog/s_progOpaque invalidates every
        // "already set to this value" cache below even though both share
        // the same explicit uniform locations - force the next
        // ApplyShaderStateCommon to resync onto the newly bound program.
        s_hasLastMvp = false;
        s_lastUseTex = -1;
        s_lastAlphaTest = -1;
        s_lastAlphaRef = -1.0f;
    }
}

static bool EnsureQuadIndexCapacity(size_t quadCount) {
    if (quadCount == 0 || s_ebo == 0) {
        return false;
    }

    static constexpr size_t kMaxQuads = 65535u / 4u;
    if (quadCount > kMaxQuads) {
        return false;
    }

    if (quadCount <= s_quadIndexCapacityQuads) {
        return true;
    }

    size_t newCap = (s_quadIndexCapacityQuads > 0) ? s_quadIndexCapacityQuads : 256u;
    while (newCap < quadCount && newCap < kMaxQuads) {
        newCap <<= 1;
    }
    if (newCap > kMaxQuads) {
        newCap = kMaxQuads;
    }
    if (newCap < quadCount) {
        return false;
    }

    s_quadIndices.resize(newCap * 6u);
    for (size_t q = 0; q < newCap; ++q) {
        const uint16_t base = static_cast<uint16_t>(q * 4u);
        const size_t i = q * 6u;
        s_quadIndices[i + 0u] = base + 0u;
        s_quadIndices[i + 1u] = base + 1u;
        s_quadIndices[i + 2u] = base + 2u;
        s_quadIndices[i + 3u] = base + 0u;
        s_quadIndices[i + 4u] = base + 2u;
        s_quadIndices[i + 5u] = base + 3u;
    }

    BindElementArrayBufferCached(s_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(s_quadIndices.size() * sizeof(uint16_t)),
                 s_quadIndices.data(),
                 GL_STATIC_DRAW);
    s_quadIndexCapacityQuads = newCap;
    return true;
}

static inline void BindImmediateVertexAttribLayout() {
    if (s_imAttribValid) {
        return;
    }

    const int stride = sizeof(IMVertex);
    glEnableVertexAttribArray(s_aPos);
    glVertexAttribPointer(s_aPos, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(IMVertex, x));
    glEnableVertexAttribArray(s_aColor);
    glVertexAttribPointer(s_aColor, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(IMVertex, r));
    glEnableVertexAttribArray(s_aUV);
    glVertexAttribPointer(s_aUV, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(IMVertex, u));
    s_imAttribValid = true;
}

// =============================================================================
// Shader helpers
// =============================================================================
static GLuint CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetShaderInfoLog(s, 512, nullptr, buf);
        LOGE("Shader compile error: %s", buf);
        glDeleteShader(s); return 0;
    }
    return s;
}

static GLuint LinkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetProgramInfoLog(p, 512, nullptr, buf);
        LOGE("Program link error: %s", buf);
        glDeleteProgram(p); return 0;
    }
    return p;
}

// =============================================================================
// Shaders source
// =============================================================================

// Vertex shader: transforms by MVP, passes color + texcoord through.
// Explicit layout locations so both fragment-shader variants below link into
// programs with identical attribute/uniform locations - lets the rest of the
// file treat s_aPos/s_uMVP/etc as valid no matter which program is bound.
// Position stays highp (geometry precision matters over large world coords);
// the color/uv varyings are mediump since fragment work only touches color math.
static const char* s_vertSrc = R"(#version 310 es
layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_uv;
layout(location = 0) uniform highp mat4 u_mvp;
out mediump vec4 v_color;
out mediump vec2 v_uv;
void main() {
    gl_Position = u_mvp * a_pos;
    // Clamped to [0,1] to match desktop fixed-function GL, which clamps colour
    // values on input to glColor*/the colour array. GLES3 varyings do NOT
    // clamp, so without this the raw value reaches the fragment shader intact.
    //
    // It matters because AddTerrainLight (ZzzLodTerrain.cpp) ACCUMULATES light
    // per source with only a lower clamp (`b[i] += Light[i]*lf`, then
    // `if (b[i] < 0) b[i] = 0`) - no upper bound. Anywhere several lights
    // overlap (the Devias wedding hall's candelabras, torch clusters) the
    // terrain light exceeds 1.0, and objects lit from it then rendered
    // texture*colour at >1x and saturated to white: pink garlands and flowers
    // turning white, foliage blowing out, the carpet losing its pattern. PC
    // never showed it because fixed-function clamped the same values to 1.0.
    v_color = clamp(a_color, 0.0, 1.0);
    v_uv = a_uv;
}
)";

// Fragment shader (alpha-tested / blended draws): texture * color, with
// optional alpha test. Used for particles, foliage/fences, and anything else
// with GL_ALPHA_TEST or GL_BLEND enabled. mediump: Adreno runs mediump ~2x
// highp for simple color math like this, and color values don't need highp range.
// Fragment uniforms start at location 4: u_mvp above is a mat4, which
// consumes 4 consecutive locations (one per column, locations 0-3) in the
// linked program's shared uniform-location space - starting fragment
// uniforms at 1 would silently alias u_mvp's columns and fail to link.
static const char* s_fragSrc = R"(#version 310 es
precision mediump float;
layout(location = 4) uniform sampler2D u_sampler;
layout(location = 5) uniform int u_useTex;
layout(location = 6) uniform int u_alphaTest;
layout(location = 7) uniform float u_alphaRef;
in vec4 v_color;
in vec2 v_uv;
out vec4 outFragColor;
void main() {
    vec4 c;
    if (u_useTex == 1) {
        c = texture(u_sampler, v_uv) * v_color;
    } else {
        c = v_color;
    }
    if (c.a <= 0.001) {
        discard;
    }
    if (u_alphaTest == 1 && c.a <= u_alphaRef) {
        discard;
    }
    outFragColor = c;
}
)";

// Fragment shader (opaque draws, no GL_ALPHA_TEST/GL_BLEND): identical color
// math but WITHOUT any discard. On Adreno/Mali, a `discard` in the shader
// disables Early-Z/LRZ for every draw using that program - even draws that
// never actually take the discard branch, e.g. fully opaque terrain/character
// meshes with alpha always 1. Routing those through this variant instead lets
// the GPU reject occluded fragments before shading them. Same uniform
// locations as s_fragSrc (see layout(location=...) above) so both programs
// share one set of cached uniform-location variables.
static const char* s_fragSrcOpaque = R"(#version 310 es
precision mediump float;
layout(location = 4) uniform sampler2D u_sampler;
layout(location = 5) uniform int u_useTex;
layout(location = 6) uniform int u_alphaTest;
layout(location = 7) uniform float u_alphaRef;
in vec4 v_color;
in vec2 v_uv;
out vec4 outFragColor;
void main() {
    vec4 c;
    if (u_useTex == 1) {
        c = texture(u_sampler, v_uv) * v_color;
    } else {
        c = v_color;
    }
    outFragColor = c;
}
)";

// =============================================================================
// Init / Shutdown
// =============================================================================
bool GL_SkinInit();       // defined further down - GPU skinning, additive/unused for now
void GL_SkinShutdown();

void GL_Compat_Init() {
    // Init matrix stacks with identity
    for (int i = 0; i < MAX_STACK; ++i) mat4_identity(s_mvStack[i]);
    for (int i = 0; i < 8;         ++i) mat4_identity(s_projStack[i]);
    s_mvDepth   = 0;
    s_projDepth = 0;
    s_matrixMode = 0x1700; // GL_MODELVIEW

    // Init current vertex
    s_cur = {0,0,0, 1,1,1,1, 0,0};
    s_inBegin = false;
    s_verts.reserve(8192);
    s_expandedVerts.reserve(12288);
    s_arrayVerts.reserve(8192);
    s_batchVerts.reserve(kMaxBatchVerts);
    s_batchConvertedVerts.reserve(65536);
    s_hasBatch = false;
    s_batchPrimMode = 0;
    s_vboCapacity = 0;
    s_samplerUniformInitialized = false;
    s_hasLastMvp = false;
    s_mvpDirty = true;
    s_lastUseTex = -1;
    s_lastAlphaTest = -1;
    s_lastAlphaRef = -1.0f;
    s_capBits = 0;
    s_capBitsKnown = 0;
    s_lastBlendSrc = 0xFFFFFFFFu;
    s_lastBlendDst = 0xFFFFFFFFu;
    s_lastDepthFunc = 0xFFFFFFFFu;
    s_lastDepthMask = 2;
    s_lastColorMaskR = 2;
    s_lastColorMaskG = 2;
    s_lastColorMaskB = 2;
    s_lastColorMaskA = 2;
    s_hasClearColor = false;
    s_boundArrayBuffer = 0;
    s_boundElementArrayBuffer = 0;
    s_currentProgram = 0;
    s_imAttribValid = false;
    s_quadIndices.clear();
    s_quadIndexCapacityQuads = 0;

    // Compile shaders. Both programs share one vertex shader object and use
    // matching explicit layout(location=...) on every attribute/uniform (see
    // s_vertSrc/s_fragSrc/s_fragSrcOpaque above), so the single set of
    // location variables queried below is valid for either program.
    GLuint vs          = CompileShader(GL_VERTEX_SHADER,   s_vertSrc);
    GLuint fsAlphaTest = CompileShader(GL_FRAGMENT_SHADER, s_fragSrc);
    GLuint fsOpaque    = CompileShader(GL_FRAGMENT_SHADER, s_fragSrcOpaque);
    if (!vs || !fsAlphaTest || !fsOpaque) { LOGE("GL_Compat_Init: shader compile failed"); return; }
    s_prog       = LinkProgram(vs, fsAlphaTest);
    s_progOpaque = LinkProgram(vs, fsOpaque);
    glDeleteShader(vs); glDeleteShader(fsAlphaTest); glDeleteShader(fsOpaque);
    if (!s_prog || !s_progOpaque) { LOGE("GL_Compat_Init: link failed"); return; }

    s_aPos    = glGetAttribLocation(s_prog,  "a_pos");
    s_aColor  = glGetAttribLocation(s_prog,  "a_color");
    s_aUV     = glGetAttribLocation(s_prog,  "a_uv");
    s_uMVP    = glGetUniformLocation(s_prog, "u_mvp");
    s_uSampler    = glGetUniformLocation(s_prog, "u_sampler");
    s_uUseTexture = glGetUniformLocation(s_prog, "u_useTex");
    s_uAlphaTest  = glGetUniformLocation(s_prog, "u_alphaTest");
    s_uAlphaRef   = glGetUniformLocation(s_prog, "u_alphaRef");

    // Create VBO
    glGenBuffers(1, &s_vbo);
    glGenBuffers(1, &s_ebo);

    // Sampler uniform is constant (texture unit 0) - each program has its own
    // uniform storage, so it must be set once per program, not just once overall.
    UseProgramCached(s_prog);
    glUniform1i(s_uSampler, 0);
    UseProgramCached(s_progOpaque);
    glUniform1i(s_uSampler, 0);
    UseProgramCached(0);
    s_samplerUniformInitialized = true;

    LOGI("GL_Compat_Init: OK (prog=%u, progOpaque=%u, vbo=%u, ebo=%u)", s_prog, s_progOpaque, s_vbo, s_ebo);

    InitStreamRing();

    // GPU skinning: compiles/links the shader and allocates its UBO.
    //
    // This used to say GL_DrawSkinnedMesh was never called and a failure here
    // affected nothing. That has not been true for a while: measured
    // 2026-08-19, the skinned path is 662 of 906 draws a frame, 71% of the
    // total, and the largest single cost in the frame. A failure here is a
    // fallback to the CPU path, not a no-op.
    if (!GL_SkinInit()) {
        LOGE("GL_Compat_Init: GL_SkinInit failed (non-fatal, falls back to the CPU mesh path)");
    }
}

void GL_GetDrawStats(int* drawCalls, int* vertices) {
    FlushPendingImmediateBatch();
    if (drawCalls) *drawCalls = s_drawCallCount;
    if (vertices)  *vertices  = s_totalVertices;
}
void GL_GetFlushCauseStats(int* out, int count) {
    if (!out) return;
    const int n = (count < kFlushCauseCount) ? count : kFlushCauseCount;
    for (int i = 0; i < n; ++i) out[i] = s_flushCauseCounts[i];
}

void GL_GetDrawSiteStats(int* out, int count) {
    if (!out) return;
    const int n = (count < 10) ? count : 10;
    for (int i = 0; i < n; ++i) out[i] = s_drawSite[i];
}

void GL_ResetDrawStats() {
    for (int i = 0; i < 10; ++i) s_drawSite[i] = 0;
    for (int i = 0; i < kFlushCauseCount; ++i) s_flushCauseCounts[i] = 0;
    s_drawCallCount = 0;
    s_totalVertices = 0;
    s_imDrawCalls = 0;
    s_vaDirectDrawCalls = 0;
    s_vaConvertedDrawCalls = 0;
    s_quadIndexedDrawCalls = 0;
    s_quadExpandedDrawCalls = 0;
}

void GL_GetDrawPathStats(int* imDrawCalls,
                         int* vaDirectDrawCalls,
                         int* vaConvertedDrawCalls,
                         int* quadIndexedDrawCalls,
                         int* quadExpandedDrawCalls) {
    if (imDrawCalls) *imDrawCalls = s_imDrawCalls;
    if (vaDirectDrawCalls) *vaDirectDrawCalls = s_vaDirectDrawCalls;
    if (vaConvertedDrawCalls) *vaConvertedDrawCalls = s_vaConvertedDrawCalls;
    if (quadIndexedDrawCalls) *quadIndexedDrawCalls = s_quadIndexedDrawCalls;
    if (quadExpandedDrawCalls) *quadExpandedDrawCalls = s_quadExpandedDrawCalls;
}

void GL_SetPreferDirectVertexArrays(bool preferDirect) {
    if (s_preferDirectVertexArrays == preferDirect) {
        return;
    }
    // Keep draw order deterministic if toggled mid-frame.
    FlushPendingImmediateBatch();
    s_preferDirectVertexArrays = preferDirect;
    LOGI("GL path switch: preferDirectVA=%d", preferDirect ? 1 : 0);
}

bool GL_GetPreferDirectVertexArrays() {
    return s_preferDirectVertexArrays;
}

void GL_Compat_Shutdown() {
    FlushPendingImmediateBatch();
    GL_SkinShutdown();
    if (s_prog) { glDeleteProgram(s_prog); s_prog = 0; }
    if (s_progOpaque) { glDeleteProgram(s_progOpaque); s_progOpaque = 0; }
    if (s_vbo)  { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
    if (s_ebo)  { glDeleteBuffers(1, &s_ebo); s_ebo = 0; }
    DestroyStreamRing();
    s_verts.clear();
    s_expandedVerts.clear();
    s_arrayVerts.clear();
    s_batchVerts.clear();
    s_hasBatch = false;
    s_batchPrimMode = 0;
    s_vboCapacity = 0;
    s_samplerUniformInitialized = false;
    s_hasLastMvp = false;
    s_mvpDirty = true;
    s_lastUseTex = -1;
    s_lastAlphaTest = -1;
    s_lastAlphaRef = -1.0f;
    s_capBits = 0;
    s_capBitsKnown = 0;
    s_lastBlendSrc = 0xFFFFFFFFu;
    s_lastBlendDst = 0xFFFFFFFFu;
    s_lastDepthFunc = 0xFFFFFFFFu;
    s_lastDepthMask = 2;
    s_lastColorMaskR = 2;
    s_lastColorMaskG = 2;
    s_lastColorMaskB = 2;
    s_lastColorMaskA = 2;
    s_hasClearColor = false;
    s_boundArrayBuffer = 0;
    s_boundElementArrayBuffer = 0;
    s_currentProgram = 0;
    s_imAttribValid = false;
    s_quadIndices.clear();
    s_quadIndexCapacityQuads = 0;
}

// ZzzOpenglUtil.cpp's BindTexture() keeps its OWN texture-binding shadow,
// separate from this file's s_boundTexture. Declared here at true file scope
// (gl_compat.cpp has no enclosing namespace, but an extern written inside a
// function body has bitten this codebase before - it resolves into the
// enclosing namespace and links to nothing).
extern int CachTexture;

// TEMP profiling: cost and vertex volume of the per-vertex CPU transform in
// GL_BatchAppendIndexedTrianglesLitTex. Reset each drift-log window.
unsigned long long g_ProfBatchAppendTicks = 0;
unsigned long long g_ProfBatchAppendVerts = 0;

void GL_InvalidateCachedGLState() {
    // Call this after any code OUTSIDE gl_compat.cpp makes raw GL calls that
    // change program/texture/buffer bindings or cap (enable/disable) state -
    // e.g. RenderBackend.cpp's render-scale FBO blit pass. Without this, the
    // cached-state checks throughout this file (UseProgramCached,
    // GL_TrackBindTexture, GL_Enable_Compat/Disable_Compat, BindArrayBufferCached,
    // ApplyShaderStateCommon's uniform caching) would wrongly believe GL is
    // still in the state gl_compat last set it to, and skip re-issuing the
    // real GL calls needed to actually restore it.
    s_currentProgram = 0;
    s_boundTexture = 0;
    // ...and ZzzOpenglUtil.cpp's separate shadow, for exactly the same reason.
    // GL_DrawSkinnedMesh raw-binds character textures (units 0-2) without
    // going through BindTexture(), so CachTexture keeps naming whatever index
    // was bound BEFORE that draw. Terrain/grass then call BindTexture(N) with
    // N still matching the stale shadow, the "already bound" check short-
    // circuits, the real glBindTexture never happens, and the tile draws with
    // the character texture the skinned draw left bound - the intermittent
    // white/wrong-texture flicker on map tiles and grass while walking, which
    // varies with draw order and so comes and goes. 0x7FFFFFFF is the same
    // "impossible index, force a re-bind" value the other fixes for this bug
    // class already use (UIControls.cpp, android_main.cpp).
    CachTexture = 0x7FFFFFFF;
    s_boundArrayBuffer = 0;
    s_boundElementArrayBuffer = 0;
    s_capBitsKnown = 0;
    s_lastBlendSrc = 0xFFFFFFFFu;
    s_lastBlendDst = 0xFFFFFFFFu;
    s_lastDepthFunc = 0xFFFFFFFFu;
    s_lastDepthMask = 2;
    s_lastColorMaskR = 2;
    s_lastColorMaskG = 2;
    s_lastColorMaskB = 2;
    s_lastColorMaskA = 2;
    s_hasLastMvp = false;
    s_lastUseTex = -1;
    s_lastAlphaTest = -1;
    s_lastAlphaRef = -1.0f;
}

// =============================================================================
// Internal: flush immediate mode vertices
// Handles GL_QUADS (not in GLES2) by converting each quad to 2 triangles.
// =============================================================================
static inline void ApplyShaderStateCommon(bool modelViewBaked) {
    // Fully opaque draws (no alpha test, no blending - terrain, character
    // meshes, opaque UI) go through the no-discard program so Early-Z/LRZ can
    // reject occluded fragments before shading. Alpha-tested or blended draws
    // (foliage/fences, particles/effects) need the discard-capable variant.
    const bool needsDiscard = s_alphaTestEnabled || (s_capBits & CAP_BLEND) != 0;
    // TEMP A/B: forces every draw onto the no-discard program to measure the
    // upper bound of an Early-Z fix. Visually wrong - alpha-tested foliage and
    // fences render as opaque quads - but it bounds the win before anyone
    // refactors the engine's blend state to earn it honestly.
    const GLuint targetProg =
        (g_ForceEarlyZOpaqueProgram && s_progOpaque)
            ? s_progOpaque
            : ((needsDiscard || !s_progOpaque) ? s_prog : s_progOpaque);

    // TEMP profiling: the Early-Z split only helps if draws actually land on
    // the no-discard program. A shader containing discard forces Mali/Adreno
    // to disable early depth test, so if almost everything is taking the
    // discard path (blend left enabled, say) the optimisation is inert.
    if (targetProg == s_progOpaque) {
        ++g_ProfEarlyZOpaqueDraws;
    } else {
        ++g_ProfEarlyZDiscardDraws;
    }
    UseProgramCached(targetProg);
    if (!s_samplerUniformInitialized) {
        glUniform1i(s_uSampler, 0);
        s_samplerUniformInitialized = true;
    }

    Mat4 desiredMvp;
    if (modelViewBaked) {
        mat4_copy(desiredMvp, s_projStack[s_projDepth]);
    } else {
        if (s_mvpDirty) {
            GetMVP(s_cachedMvp);
            s_mvpDirty = false;
        }
        mat4_copy(desiredMvp, s_cachedMvp);
    }

    if (!s_hasLastMvp || memcmp(s_lastMvp, desiredMvp, sizeof(Mat4)) != 0) {
        glUniformMatrix4fv(s_uMVP, 1, GL_FALSE, desiredMvp);
        memcpy(s_lastMvp, desiredMvp, sizeof(Mat4));
        s_hasLastMvp = true;
    }

    const int useTex = (s_texture2DEnabled && s_boundTexture != 0) ? 1 : 0;
    if (useTex != s_lastUseTex) {
        glUniform1i(s_uUseTexture, useTex);
        s_lastUseTex = useTex;
    }

    const int alphaTest = s_alphaTestEnabled ? 1 : 0;
    if (alphaTest != s_lastAlphaTest) {
        glUniform1i(s_uAlphaTest, alphaTest);
        s_lastAlphaTest = alphaTest;
    }
    if (s_alphaRef != s_lastAlphaRef) {
        glUniform1f(s_uAlphaRef, s_alphaRef);
        s_lastAlphaRef = s_alphaRef;
    }
}

// Stream one draw's worth of vertices and bind whichever buffer they landed
// in. Returns the first-vertex index that draw should read from (always 0 for
// every path except the ring), or -1 if nothing usable is bound at all.
// `bytes` is always a whole number of IMVertex - every call site below
// computes it as vertCount * sizeof(IMVertex) - so the ring's running offset
// stays vertex-aligned and dividing it back into a vertex index is exact.
//
// Fast path (s_streamRingAvailable): the ring slot for the current frame was
// mapped once at init (see InitStreamRing) and never unmapped - this is a
// plain memcpy into already-mapped memory, no GL call. See the long comment
// above kStreamRingSize for why this exists and what two earlier attempts at
// it got wrong.
//
// Fallback (ring unavailable, or this one draw is larger than a whole ring
// slot): the original per-draw glBufferData orphan. Buffer orphaning gives
// the driver a fresh allocation so overwriting storage the GPU may still be
// reading does not stall - the ~62% CPU cost this file's stream-ring comment
// describes, but still strictly safe, which is why it stays as the fallback
// rather than being removed.
static GLint StreamVertexData(const void* data, GLsizeiptr bytes) {
    if (s_skipVBOOrphan) {
        // Software renderers (SwiftShader): a fresh allocation per draw is pure
        // malloc cost with no pipeline to stall, so keep a grow-only buffer.
        BindArrayBufferCached(s_vbo);
        if (bytes > s_vboCapacity) {
            GLsizeiptr newCapacity = std::max<GLsizeiptr>(65536, s_vboCapacity);
            while (newCapacity < bytes) {
                newCapacity <<= 1;
            }
            s_vboCapacity = newCapacity;
            glBufferData(GL_ARRAY_BUFFER, s_vboCapacity, nullptr, GL_STREAM_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, data);
        return 0;
    }

    if (s_streamRingAvailable && bytes <= kStreamRingSlotBytes) {
        StreamRingSlot& slot = s_streamRing[s_streamIdx];
        if (s_streamOffset + bytes > kStreamRingSlotBytes) {
            // This frame has filled the slot. Falling back to the orphan path
            // for the rest of the frame is safe and simply costs what today's
            // shipped build already costs for that one draw - wrapping back
            // to offset 0 within the same frame would not be: earlier draws
            // this frame already point the GPU at data earlier in this same
            // slot, and it has not necessarily read it yet.
        } else {
            memcpy(slot.mapped + s_streamOffset, data, static_cast<size_t>(bytes));
            const GLint firstVertex = static_cast<GLint>(s_streamOffset / static_cast<GLsizeiptr>(sizeof(IMVertex)));
            s_streamOffset += bytes;
            BindArrayBufferCached(slot.vbo);
            return firstVertex;
        }
    }

    BindArrayBufferCached(s_vbo);
    glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_STREAM_DRAW);
    s_vboCapacity = bytes;
    return 0;
}

static void DrawPreparedVerts(const IMVertex* verts, size_t vertCount, GLenum drawMode, bool modelViewBaked) {
    if (!verts || vertCount == 0 || !s_prog || !s_vbo) {
        return;
    }

    const GLsizeiptr drawBytes = static_cast<GLsizeiptr>(vertCount * sizeof(IMVertex));

    const GLint first = StreamVertexData(verts, drawBytes);
    if (first < 0) {
        return;
    }

    ApplyShaderStateCommon(modelViewBaked);

    BindImmediateVertexAttribLayout();

    glDrawArrays(drawMode, first, (GLsizei)vertCount);
    ++s_drawCallCount; ++s_drawSite[0];
    s_totalVertices += (int)vertCount;
}

static bool DrawPreparedVertsDirect(const IMVertex* verts,
                                    size_t vertCount,
                                    GLenum drawMode,
                                    bool modelViewBaked) {
    if (!verts || vertCount == 0 || !s_prog) {
        return false;
    }

    ApplyShaderStateCommon(modelViewBaked);

    BindArrayBufferCached(0);
    BindElementArrayBufferCached(0);
    s_imAttribValid = false;

    const GLsizei stride = static_cast<GLsizei>(sizeof(IMVertex));
    const uint8_t* base = reinterpret_cast<const uint8_t*>(verts);

    glEnableVertexAttribArray(s_aPos);
    glVertexAttribPointer(
        s_aPos,
        3,
        GL_FLOAT,
        GL_FALSE,
        stride,
        base + offsetof(IMVertex, x));

    glEnableVertexAttribArray(s_aColor);
    glVertexAttribPointer(
        s_aColor,
        4,
        GL_FLOAT,
        GL_FALSE,
        stride,
        base + offsetof(IMVertex, r));

    glEnableVertexAttribArray(s_aUV);
    glVertexAttribPointer(
        s_aUV,
        2,
        GL_FLOAT,
        GL_FALSE,
        stride,
        base + offsetof(IMVertex, u));

    glDrawArrays(drawMode, 0, static_cast<GLsizei>(vertCount));

    ++s_drawCallCount; ++s_drawSite[1];
    s_totalVertices += static_cast<int>(vertCount);
    ++s_vaDirectDrawCalls;
    return true;
}

static bool DrawPreparedQuadsIndexed(const IMVertex* verts, size_t vertCount, bool modelViewBaked) {
    const size_t quadCount = vertCount / 4u;
    if (quadCount == 0u) {
        return false;
    }
    if (!EnsureQuadIndexCapacity(quadCount)) {
        return false;
    }

    const GLsizeiptr drawBytes = static_cast<GLsizeiptr>(vertCount * sizeof(IMVertex));

    const GLint first = StreamVertexData(verts, drawBytes);
    if (first < 0) {
        return false;
    }

    ApplyShaderStateCommon(modelViewBaked);

    BindImmediateVertexAttribLayout();

    BindElementArrayBufferCached(s_ebo);
    DrawQuadIndicesFrom(static_cast<GLsizei>(quadCount * 6u), first);
    ++s_drawCallCount; ++s_drawSite[2];
    s_totalVertices += (int)(quadCount * 6u);
    ++s_quadIndexedDrawCalls;
    return true;
}

static void DrawVertexList(const std::vector<IMVertex>& inputVerts, GLenum primMode, bool modelViewBaked) {
    if (inputVerts.empty()) {
        return;
    }

    const IMVertex* drawData = inputVerts.data();
    size_t drawCount = inputVerts.size();
    GLenum drawMode = primMode;

    // GLES2 has no GL_QUADS / GL_POLYGON, expand to triangles.
    s_expandedVerts.clear();
    if (primMode == 0x0007 /* GL_QUADS */) {
        const size_t usableVerts = (inputVerts.size() / 4u) * 4u;
        if (usableVerts == 0u) {
            return;
        }

        if (DrawPreparedQuadsIndexed(inputVerts.data(), usableVerts, modelViewBaked)) {
            return;
        }

        // Fallback: expand quads if index-buffer path is unavailable.
        const size_t quadCount = usableVerts / 4u;
        s_expandedVerts.reserve(quadCount * 6u);
        for (size_t i = 0; i + 3 < usableVerts; i += 4u) {
            const IMVertex& v0 = inputVerts[i + 0u];
            const IMVertex& v1 = inputVerts[i + 1u];
            const IMVertex& v2 = inputVerts[i + 2u];
            const IMVertex& v3 = inputVerts[i + 3u];
            s_expandedVerts.push_back(v0);
            s_expandedVerts.push_back(v1);
            s_expandedVerts.push_back(v2);
            s_expandedVerts.push_back(v0);
            s_expandedVerts.push_back(v2);
            s_expandedVerts.push_back(v3);
        }
        drawData = s_expandedVerts.data();
        drawCount = s_expandedVerts.size();
        drawMode = GL_TRIANGLES;
        ++s_quadExpandedDrawCalls;
    } else if (primMode == 0x0009 /* GL_POLYGON */) {
        if (inputVerts.size() >= 3) {
            const size_t triCount = inputVerts.size() - 2;
            s_expandedVerts.reserve(triCount * 3);
            const IMVertex& v0 = inputVerts[0];
            for (size_t i = 1; i + 1 < inputVerts.size(); ++i) {
                s_expandedVerts.push_back(v0);
                s_expandedVerts.push_back(inputVerts[i]);
                s_expandedVerts.push_back(inputVerts[i + 1]);
            }
            drawData = s_expandedVerts.data();
            drawCount = s_expandedVerts.size();
            drawMode = GL_TRIANGLES;
        } else {
            drawCount = 0;
        }
    }

    if (drawCount == 0) {
        return;
    }

    if (s_preferDirectVertexArrays &&
        drawMode != 0x0007 /* GL_QUADS */ &&
        drawMode != 0x0009 /* GL_POLYGON */ &&
        DrawPreparedVertsDirect(drawData, drawCount, drawMode, modelViewBaked)) {
        return;
    }

    DrawPreparedVerts(drawData, drawCount, drawMode, modelViewBaked);
}

static void AppendVertsToPendingImmediateBatch(const std::vector<IMVertex>& inputVerts, GLenum primMode) {
    if (inputVerts.empty() || !s_prog) {
        return;
    }

    // Fan/strip/polygon/line-loop được chuyển sang TRIANGLES/LINES để có thể merge
    // vào cùng một batch. MV đã được bake vào vị trí ở GL_Vertex*, nên việc nối
    // các quad UI từ nhiều RenderBitmap() liên tiếp là hợp lệ.
    const IMVertex* convertedVerts = nullptr;
    size_t convertedCount = 0;
    GLenum convertedMode = primMode;
    if (!ConvertPrimitiveForBatching(inputVerts, primMode, convertedVerts, convertedCount, convertedMode)
        || convertedCount == 0) {
        if (s_hasBatch) {
            ++s_flushCauseCounts[kFlushCauseUnbatchable];
            DrawVertexList(s_batchVerts, s_batchPrimMode, true);
            s_batchVerts.clear();
            s_hasBatch = false;
            s_batchPrimMode = 0;
        }
        DrawVertexList(inputVerts, primMode, kBakeModelViewForImmediate);
        return;
    }

    // Primitive mode khác → flush batch hiện tại trước
    if (s_hasBatch && s_batchPrimMode != convertedMode) {
        ++s_flushCauseCounts[kFlushCausePrimMode];
        DrawVertexList(s_batchVerts, s_batchPrimMode, true);
        s_batchVerts.clear();
        s_hasBatch = false;
        s_batchPrimMode = 0;
    }

    // Batch đầy → flush rồi bắt đầu batch mới
    const size_t limit = MaxBatchVertsForMode(convertedMode);
    if (s_batchVerts.size() + convertedCount > limit) {
        if (s_hasBatch) {
            ++s_flushCauseCounts[kFlushCauseBatchFull];
            DrawVertexList(s_batchVerts, s_batchPrimMode, true);
            s_batchVerts.clear();
            s_hasBatch = false;
            s_batchPrimMode = 0;
        }
    }

    // convertedVerts có thể trỏ vào inputVerts.data() (primitive gốc đã batchable)
    // hoặc vào s_batchConvertedVerts (đã được convert). Cả hai đều khác s_batchVerts
    // nên insert thẳng vào pending batch là an toàn.
    s_batchVerts.insert(s_batchVerts.end(), convertedVerts, convertedVerts + convertedCount);
    s_hasBatch = true;
    s_batchPrimMode = convertedMode;
}

static void FlushIM() {
    if (s_verts.empty() || !s_prog) {
        return;
    }
    AppendVertsToPendingImmediateBatch(s_verts, s_primMode);
    s_verts.clear();
}

static void FlushPendingImmediateBatch(GLFlushCause cause) {
    if (!s_hasBatch || s_batchVerts.empty()) {
        return;
    }
    // Counted only when a batch was actually cut. The call sites fire on every
    // state change whether or not anything is pending, and counting those would
    // report state churn rather than lost batching.
    if (cause >= 0 && cause < kFlushCauseCount) {
        ++s_flushCauseCounts[cause];
    }
    // Draw toàn bộ batch tích lũy bằng 1 draw call (modelViewBaked=true vì MV đã baked vào verts)
    DrawVertexList(s_batchVerts, s_batchPrimMode, true);
    s_batchVerts.clear();
    s_hasBatch = false;
    s_batchPrimMode = 0;
}

void GL_FlushPending() {
    FlushPendingImmediateBatch(kFlushCauseFrame);

    if (!s_streamRingAvailable) {
        return;
    }

    // Mark this slot's contents as "in flight" - draws are queued, not
    // executed synchronously, so the GPU can still be reading this slot for a
    // while after this point (see the stream-ring comment above
    // kStreamRingSize for the full design).
    StreamRingSlot& finishedSlot = s_streamRing[s_streamIdx];
    if (finishedSlot.fence) {
        glDeleteSync(finishedSlot.fence);
    }
    finishedSlot.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    s_streamIdx = (s_streamIdx + 1) % kStreamRingSize;
    s_streamOffset = 0;

    // Before the new frame writes into this slot, confirm the GPU is
    // actually done with what it held kStreamRingSize-1 frames ago - this is
    // what makes reuse provably safe rather than "3 buffers should be
    // enough". Finite timeout, not GL_TIMEOUT_IGNORED: with 3 slots the GPU
    // normally has two whole frames of headroom to already be done, so this
    // returns immediately in the ordinary case - if a fence is somehow still
    // unsignaled after 2 seconds (a stalled/lost GPU context), freezing the
    // game waiting on it forever is a worse failure than proceeding and
    // risking one torn frame.
    StreamRingSlot& nextSlot = s_streamRing[s_streamIdx];
    if (nextSlot.fence) {
        const GLuint64 kTwoSecondsNs = 2ull * 1000ull * 1000ull * 1000ull;
        const GLenum waitResult = glClientWaitSync(nextSlot.fence, GL_SYNC_FLUSH_COMMANDS_BIT, kTwoSecondsNs);
        if (waitResult == GL_TIMEOUT_EXPIRED || waitResult == GL_WAIT_FAILED) {
            LOGE("Stream ring: fence wait on slot %d did not signal in time (result=0x%x)", s_streamIdx, waitResult);
        }
        glDeleteSync(nextSlot.fence);
        nextSlot.fence = nullptr;
    }
}

// =============================================================================
// Matrix operations
// =============================================================================
void GL_MatrixMode(GLenum mode) {
    if (s_matrixMode != (int)mode) {
        const bool touchesProjection =
            s_matrixMode == static_cast<int>(kCompatProjectionMode) ||
            mode == kCompatProjectionMode;
        if (!kBakeModelViewForImmediate || touchesProjection) {
            FlushPendingImmediateBatch(kFlushCauseProjection);
        }
        s_matrixMode = (int)mode;
    }
}

void GL_LoadIdentity() {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    mat4_identity(CurrentStack()[CurrentDepth()]);
    s_mvpDirty = true;
}

void GL_Ortho(double l, double r, double b, double t, double n, double f) {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    float rl = (float)(r - l), tb = (float)(t - b), fn = (float)(f - n);
    Mat4 m = {
        2.0f/rl, 0, 0, 0,
        0, 2.0f/tb, 0, 0,
        0, 0, -2.0f/fn, 0,
        -(float)(r+l)/rl, -(float)(t+b)/tb, -(float)(f+n)/fn, 1
    };
    Mat4 res;
    mat4_multiply(res, CurrentStack()[CurrentDepth()], m);
    mat4_copy(CurrentStack()[CurrentDepth()], res);
    s_mvpDirty = true;
}

void GL_gluOrtho2D(double l, double r, double b, double t) {
    GL_Ortho(l, r, b, t, -1.0, 1.0);
}

void GL_Frustum(double l, double r, double b, double t, double n, double f) {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    float rl = (float)(r-l), tb = (float)(t-b), fn = (float)(f-n);
    float n2 = 2.0f*(float)n;
    Mat4 m = {
        n2/rl, 0, 0, 0,
        0, n2/tb, 0, 0,
        (float)(r+l)/rl, (float)(t+b)/tb, -(float)(f+n)/fn, -1,
        0, 0, -2.0f*(float)f*(float)n/fn, 0
    };
    Mat4 res;
    mat4_multiply(res, CurrentStack()[CurrentDepth()], m);
    mat4_copy(CurrentStack()[CurrentDepth()], res);
    s_mvpDirty = true;
}

void GL_gluPerspective(double fovy, double aspect, double zNear, double zFar) {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    double f = 1.0 / tan(fovy * 3.14159265358979323846 / 360.0);
    double fn = zNear - zFar;
    int& d = CurrentDepth();
    mat4_identity(CurrentStack()[d]);
    CurrentStack()[d][0]  = (float)(f / aspect);
    CurrentStack()[d][5]  = (float)f;
    CurrentStack()[d][10] = (float)((zFar + zNear) / fn);
    CurrentStack()[d][11] = -1.0f;
    CurrentStack()[d][14] = (float)(2.0 * zFar * zNear / fn);
    CurrentStack()[d][15] = 0.0f;
    s_mvpDirty = true;
}

void GL_gluLookAt(double ex,double ey,double ez,
                  double cx,double cy,double cz,
                  double ux,double uy,double uz) {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    double fx=cx-ex, fy=cy-ey, fz=cz-ez;
    double fl=sqrt(fx*fx+fy*fy+fz*fz); fx/=fl; fy/=fl; fz/=fl;
    double sx=fy*uz-fz*uy, sy=fz*ux-fx*uz, sz=fx*uy-fy*ux;
    double sl=sqrt(sx*sx+sy*sy+sz*sz); sx/=sl; sy/=sl; sz/=sl;
    double vx=sy*fz-sz*fy, vy=sz*fx-sx*fz, vz=sx*fy-sy*fx;
    Mat4 m = {
        (float)sx,(float)vx,-(float)fx,0,
        (float)sy,(float)vy,-(float)fy,0,
        (float)sz,(float)vz,-(float)fz,0,
        (float)-(sx*ex+sy*ey+sz*ez),(float)-(vx*ex+vy*ey+vz*ez),(float)(fx*ex+fy*ey+fz*ez),1
    };
    Mat4 res;
    mat4_multiply(res, CurrentStack()[CurrentDepth()], m);
    mat4_copy(CurrentStack()[CurrentDepth()], res);
    s_mvpDirty = true;
}

void GL_PushMatrix() {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    int& d = CurrentDepth();
    if (d < MAX_STACK-1) {
        mat4_copy(CurrentStack()[d+1], CurrentStack()[d]);
        ++d;
        s_mvpDirty = true;
    }
}
void GL_PopMatrix() {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    int& d = CurrentDepth();
    if (d > 0) {
        --d;
        s_mvpDirty = true;
    }
}
void GL_LoadMatrixf(const float* m) {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    memcpy(CurrentStack()[CurrentDepth()], m, sizeof(Mat4));
    s_mvpDirty = true;
}
void GL_MultMatrixf(const float* m) {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    Mat4 res;
    mat4_multiply(res, CurrentStack()[CurrentDepth()], *(const Mat4*)m);
    mat4_copy(CurrentStack()[CurrentDepth()], res);
    s_mvpDirty = true;
}
void GL_Translatef(float x, float y, float z) {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    Mat4 m; mat4_identity(m);
    m[12]=x; m[13]=y; m[14]=z;
    Mat4 res;
    mat4_multiply(res, CurrentStack()[CurrentDepth()], m);
    mat4_copy(CurrentStack()[CurrentDepth()], res);
    s_mvpDirty = true;
}
void GL_Rotatef(float angle, float x, float y, float z) {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    float a = angle * (float)(3.14159265358979323846 / 180.0);
    float c = cosf(a), s = sinf(a);
    float len = sqrtf(x*x+y*y+z*z);
    if (len > 0) { x/=len; y/=len; z/=len; }
    Mat4 m = {
        c+x*x*(1-c),   y*x*(1-c)+z*s, z*x*(1-c)-y*s, 0,
        x*y*(1-c)-z*s, c+y*y*(1-c),   z*y*(1-c)+x*s, 0,
        x*z*(1-c)+y*s, y*z*(1-c)-x*s, c+z*z*(1-c),   0,
        0,             0,             0,              1
    };
    Mat4 res;
    mat4_multiply(res, CurrentStack()[CurrentDepth()], m);
    mat4_copy(CurrentStack()[CurrentDepth()], res);
    s_mvpDirty = true;
}
void GL_Scalef(float x, float y, float z) {
    if (MatrixMutationRequiresImmediateFlush()) {
        FlushPendingImmediateBatch();
    }
    Mat4 m; mat4_identity(m);
    m[0]=x; m[5]=y; m[10]=z;
    Mat4 res;
    mat4_multiply(res, CurrentStack()[CurrentDepth()], m);
    mat4_copy(CurrentStack()[CurrentDepth()], res);
    s_mvpDirty = true;
}
void GL_GetFloatv(GLenum pname, float* params) {
    if (!params) return;
    if (pname == 0x0BA6 /*GL_MODELVIEW_MATRIX*/)  { memcpy(params, s_mvStack[s_mvDepth],   sizeof(Mat4)); return; }
    if (pname == 0x0BA7 /*GL_PROJECTION_MATRIX*/) { memcpy(params, s_projStack[s_projDepth], sizeof(Mat4)); return; }
    // GL_CURRENT_COLOR is fixed-function state that GLES3 does not have, so the
    // fall-through below used to raise GL_INVALID_ENUM and leave `params`
    // untouched. Callers that read the colour back to drive something other
    // than a vertex - the item-name renderers pick their text colour from it
    // (ZzzInventory.cpp RenderItemName/RenderItemNameS6) - therefore saw
    // whatever they had pre-initialised the buffer to, which is why excellent
    // and ancient drops printed white on mobile while PC coloured them.
    // s_cur IS this layer's current colour, so serving the query from it is
    // both correct and cheaper than the rejected driver round trip.
    if (pname == 0x0B00 /*GL_CURRENT_COLOR*/) {
        params[0] = s_cur.r; params[1] = s_cur.g; params[2] = s_cur.b; params[3] = s_cur.a;
        return;
    }
    glGetFloatv(pname, params);  // fall through to real GL for other params
}

// =============================================================================
// Immediate mode
// =============================================================================
void GL_Begin(GLenum mode) {
    s_primMode = mode;
    s_verts.clear();
    s_inBegin = true;
}
void GL_End() {
    s_inBegin = false;
    if (!s_verts.empty()) {
        ++s_imDrawCalls;
    }
    FlushIM();
}
void GL_Vertex2f(float x, float y)           {
    float tx = x;
    float ty = y;
    float tz = 0.0f;
    TransformByCurrentModelView(x, y, 0.0f, tx, ty, tz);
    s_cur.x = tx;
    s_cur.y = ty;
    s_cur.z = tz;
    s_verts.push_back(s_cur);
}
void GL_Vertex2i(int x, int y)               { GL_Vertex2f((float)x,(float)y); }
void GL_Vertex3f(float x, float y, float z)  {
    float tx = x;
    float ty = y;
    float tz = z;
    TransformByCurrentModelView(x, y, z, tx, ty, tz);
    s_cur.x = tx;
    s_cur.y = ty;
    s_cur.z = tz;
    s_verts.push_back(s_cur);
}
void GL_Vertex3fv(const float* v)            { GL_Vertex3f(v[0],v[1],v[2]); }
void GL_TexCoord2f(float s, float t)         { s_cur.u=s; s_cur.v=t; }
void GL_TexCoord2fv(const float* v)          { GL_TexCoord2f(v[0],v[1]); }
void GL_Color3f(float r, float g, float b)   { s_cur.r=r; s_cur.g=g; s_cur.b=b; s_cur.a=1.0f; }
void GL_Color3fv(const float* v)             { GL_Color3f(v[0],v[1],v[2]); }
void GL_Color3ub(uint8_t r, uint8_t g, uint8_t b) { GL_Color3f(r/255.0f, g/255.0f, b/255.0f); }
void GL_Color4f(float r, float g, float b, float a) { s_cur.r=r; s_cur.g=g; s_cur.b=b; s_cur.a=a; }
void GL_Color4ub(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { GL_Color4f(r/255.0f, g/255.0f, b/255.0f, a/255.0f); }
void GL_Normal3f(float, float, float)        {}  // ignored in basic shader
void GL_Normal3fv(const float*)              {}

// =============================================================================
// Enable / Disable
// =============================================================================
void GL_Enable_Compat(GLenum cap) {
    switch (cap) {
    case 0x0DE1: // GL_TEXTURE_2D — not a real GLES2 state but we track it
        if (!s_texture2DEnabled) {
            FlushPendingImmediateBatch(kFlushCauseEnable);
            s_texture2DEnabled = true;
        }
        break;
    case 0x0BC0: // GL_ALPHA_TEST
        if (!s_alphaTestEnabled) {
            FlushPendingImmediateBatch(kFlushCauseEnable);
            s_alphaTestEnabled = true;
        }
        break;
    case 0x0B60: // GL_FOG — ignore
    case 0x0B50: // GL_LIGHTING — ignore
        break;
    default:
    {
        const uint32_t mask = CapToMask(cap);
        if (mask != 0) {
            if (!(s_capBitsKnown & mask) || !(s_capBits & mask)) {
                FlushPendingImmediateBatch(kFlushCauseEnable);
                glEnable(cap);
                s_capBits |= mask;
                s_capBitsKnown |= mask;
            }
        } else {
            FlushPendingImmediateBatch(kFlushCauseEnable);
            glEnable(cap);
        }
        break;
    }
    }
}
void GL_Disable_Compat(GLenum cap) {
    switch (cap) {
    case 0x0DE1: // GL_TEXTURE_2D
        if (s_texture2DEnabled) {
            FlushPendingImmediateBatch(kFlushCauseEnable);
            s_texture2DEnabled = false;
        }
        break;
    case 0x0BC0: // GL_ALPHA_TEST
        if (s_alphaTestEnabled) {
            FlushPendingImmediateBatch(kFlushCauseEnable);
            s_alphaTestEnabled = false;
        }
        break;
    case 0x0B60: // GL_FOG
    case 0x0B50: // GL_LIGHTING
        break;
    default:
    {
        const uint32_t mask = CapToMask(cap);
        if (mask != 0) {
            if (!(s_capBitsKnown & mask) || (s_capBits & mask)) {
                FlushPendingImmediateBatch(kFlushCauseEnable);
                glDisable(cap);
                s_capBits &= ~mask;
                s_capBitsKnown |= mask;
            }
        } else {
            FlushPendingImmediateBatch(kFlushCauseEnable);
            glDisable(cap);
        }
        break;
    }
    }
}

// =============================================================================
// Alpha test
// =============================================================================
void GL_AlphaFunc(GLenum func, float ref) {
    if (s_alphaFunc != func || s_alphaRef != ref) {
        FlushPendingImmediateBatch(kFlushCauseAlphaRef);
    }
    s_alphaFunc = func;
    s_alphaRef  = ref;
}

// =============================================================================
// Fog (stub — complex implementation would need shader uniforms)
// =============================================================================
void GL_Fogf(GLenum, float)        {}
void GL_Fogi(GLenum, int)          {}
void GL_Fogfv(GLenum, const float*) {}

// =============================================================================
// Lighting (stub)
// =============================================================================
void GL_Lightfv(GLenum, GLenum, const float*) {}

// =============================================================================
// Legacy vertex arrays — proper emulation via our shader
// =============================================================================

struct VAState {
    const void* ptr    = nullptr;
    int         size   = 3;          // components
    GLenum      type   = GL_FLOAT;
    int         stride = 0;          // byte stride (0 = tightly packed)
    bool        enabled = false;
};

static VAState s_vaVertex;
static VAState s_vaTexCoord;
static VAState s_vaColor;

// Extract one float component from raw pointer with type conversion
static float VA_ReadFloat(const void* base, GLenum type) {
    switch (type) {
        case GL_FLOAT:          return *(const float*)base;
        case GL_UNSIGNED_BYTE:  return *(const GLubyte*)base / 255.0f;
        case GL_BYTE:           return (*(const GLbyte*)base + 0.5f) / 127.5f;
        case GL_SHORT:          return *(const GLshort*)base / 32767.0f;
        case GL_UNSIGNED_SHORT: return *(const GLushort*)base / 65535.0f;
        default:                return 0.0f;
    }
}

static int VA_TypeSize(GLenum type) {
    switch (type) {
        case GL_FLOAT:          return 4;
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:           return 1;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT: return 2;
        default:                return 4;
    }
}

// Read one vertex from a VA pointer at element index.
static void VA_ReadVertex(const VAState& va, int index, float* out, int maxComp) {
    const int ts = VA_TypeSize(va.type);
    const int stride = va.stride ? va.stride : va.size * ts;
    const uint8_t* p = (const uint8_t*)va.ptr + (size_t)index * stride;

    for (int c = 0; c < maxComp && c < va.size; ++c) {
        out[c] = VA_ReadFloat(p + c * ts, va.type);
    }
    for (int c = va.size; c < maxComp; ++c) {
        out[c] = (c < 3) ? 0.0f : 1.0f; // z=0, a=1
    }
}

// =============================================================================
// TryDrawArraysDirect - Force tối ưu cho Mu Online (LDPlayer/PC mạnh)
// =============================================================================
static bool TryDrawArraysDirect(GLenum mode, int first, int count) {
    if (count <= 0 || !s_prog || !s_vaVertex.enabled || !s_vaVertex.ptr) {
        return false;
    }

    // Bỏ qua một số primitive không hỗ trợ direct (QUADS, POLYGON vẫn cần expand)
    if (mode == 0x0007 /* GL_QUADS */ || mode == 0x0009 /* GL_POLYGON */) {
        return false;
    }

    // Mu Online hay dùng vertex array với type khác nhau → nới lỏng điều kiện
    // Chỉ bắt buộc position phải là GL_FLOAT và size >= 2
    if (s_vaVertex.type != GL_FLOAT || s_vaVertex.size < 2) {
        return false;
    }

    const bool hasTex   = s_vaTexCoord.enabled && s_vaTexCoord.ptr;
    const bool hasColor = s_vaColor.enabled && s_vaColor.ptr;

    // Nới lỏng color và texcoord: nếu không phải float thì vẫn cho chạy (dùng fallback)
    // Chỉ skip nếu stride quá lạ hoặc pointer null (đã check ở trên)

    // Flush batch pending để giữ thứ tự vẽ
    FlushPendingImmediateBatch();

    ApplyShaderStateCommon(false);   // Dùng full MVP (không bake)

    BindArrayBufferCached(0);
    BindElementArrayBufferCached(0);
    s_imAttribValid = false;

    // ==================== Position ====================
    const int posComps = std::min(3, s_vaVertex.size);
    const int posStride = s_vaVertex.stride ? s_vaVertex.stride : (s_vaVertex.size * (int)sizeof(float));
    const uint8_t* posPtr = (const uint8_t*)s_vaVertex.ptr + (size_t)first * posStride;

    glEnableVertexAttribArray(s_aPos);
    glVertexAttribPointer(s_aPos, posComps, GL_FLOAT, GL_FALSE, posStride, posPtr);

    // ==================== Color ====================
    if (hasColor && s_vaColor.type == GL_FLOAT) {
        const int colorComps = std::min(4, s_vaColor.size);
        const int colorStride = s_vaColor.stride ? s_vaColor.stride : (s_vaColor.size * (int)sizeof(float));
        const uint8_t* colorPtr = (const uint8_t*)s_vaColor.ptr + (size_t)first * colorStride;

        glEnableVertexAttribArray(s_aColor);
        glVertexAttribPointer(s_aColor, colorComps, GL_FLOAT, GL_FALSE, colorStride, colorPtr);
    } else {
        glDisableVertexAttribArray(s_aColor);
        glVertexAttrib4f(s_aColor, s_cur.r, s_cur.g, s_cur.b, s_cur.a);
    }

    // ==================== TexCoord ====================
    if (hasTex && s_vaTexCoord.type == GL_FLOAT && s_vaTexCoord.size >= 2) {
        const int texStride = s_vaTexCoord.stride ? s_vaTexCoord.stride : (s_vaTexCoord.size * (int)sizeof(float));
        const uint8_t* texPtr = (const uint8_t*)s_vaTexCoord.ptr + (size_t)first * texStride;

        glEnableVertexAttribArray(s_aUV);
        glVertexAttribPointer(s_aUV, 2, GL_FLOAT, GL_FALSE, texStride, texPtr);
    } else {
        glDisableVertexAttribArray(s_aUV);
        glVertexAttrib2f(s_aUV, 0.0f, 0.0f);
    }

    // Draw trực tiếp
    glDrawArrays(mode, 0, count);

    ++s_drawCallCount; ++s_drawSite[3];
    s_totalVertices += count;
    ++s_vaDirectDrawCalls;

    return true;
}

static void BatchAppendTrianglesFromFloatArrays(const uint8_t* posBase,
                                                int posStride,
                                                const uint8_t* texBase,
                                                int texStride,
                                                const uint8_t* colBase,
                                                int colStride,
                                                int colorSize,
                                                int count) {
    if (!posBase || count <= 0 || !s_prog) return;

    if (s_hasBatch && s_batchPrimMode != GL_TRIANGLES) {
        FlushPendingImmediateBatch();
    }

    if (s_batchVerts.size() + static_cast<size_t>(count) > kMaxBatchVerts) {
        FlushPendingImmediateBatch();
    }

    const size_t oldSize = s_batchVerts.size();
    s_batchVerts.resize(oldSize + static_cast<size_t>(count));
    IMVertex* dst = s_batchVerts.data() + oldSize;

    const Mat4& mv = s_mvStack[s_mvDepth];
    const float m00=mv[0], m10=mv[4], m20=mv[8],  m30=mv[12];
    const float m01=mv[1], m11=mv[5], m21=mv[9],  m31=mv[13];
    const float m02=mv[2], m12=mv[6], m22=mv[10], m32=mv[14];

    if (colBase) {
        for (int i = 0; i < count; ++i) {
            const float* p = reinterpret_cast<const float*>(posBase + static_cast<size_t>(i) * posStride);
            const float* c = reinterpret_cast<const float*>(colBase + static_cast<size_t>(i) * colStride);
            dst[i].x = m00*p[0] + m10*p[1] + m20*p[2] + m30;
            dst[i].y = m01*p[0] + m11*p[1] + m21*p[2] + m31;
            dst[i].z = m02*p[0] + m12*p[1] + m22*p[2] + m32;
            dst[i].r = c[0];
            dst[i].g = c[1];
            dst[i].b = c[2];
            dst[i].a = (colorSize >= 4) ? c[3] : 1.0f;
            if (texBase) {
                const float* t = reinterpret_cast<const float*>(texBase + static_cast<size_t>(i) * texStride);
                dst[i].u = t[0];
                dst[i].v = t[1];
            } else {
                dst[i].u = 0.0f;
                dst[i].v = 0.0f;
            }
        }
    } else {
        const float cr=s_cur.r, cg=s_cur.g, cb=s_cur.b, ca=s_cur.a;
        for (int i = 0; i < count; ++i) {
            const float* p = reinterpret_cast<const float*>(posBase + static_cast<size_t>(i) * posStride);
            dst[i].x = m00*p[0] + m10*p[1] + m20*p[2] + m30;
            dst[i].y = m01*p[0] + m11*p[1] + m21*p[2] + m31;
            dst[i].z = m02*p[0] + m12*p[1] + m22*p[2] + m32;
            dst[i].r = cr;
            dst[i].g = cg;
            dst[i].b = cb;
            dst[i].a = ca;
            if (texBase) {
                const float* t = reinterpret_cast<const float*>(texBase + static_cast<size_t>(i) * texStride);
                dst[i].u = t[0];
                dst[i].v = t[1];
            } else {
                dst[i].u = 0.0f;
                dst[i].v = 0.0f;
            }
        }
    }

    s_hasBatch = true;
    s_batchPrimMode = GL_TRIANGLES;
    ++s_vaConvertedDrawCalls;
}

void GL_EnableClientState(GLenum cap) {
    if      (cap == GL_VERTEX_ARRAY)        s_vaVertex.enabled   = true;
    else if (cap == GL_TEXTURE_COORD_ARRAY) s_vaTexCoord.enabled = true;
    else if (cap == GL_COLOR_ARRAY)         s_vaColor.enabled    = true;
}
void GL_DisableClientState(GLenum cap) {
    if      (cap == GL_VERTEX_ARRAY)        s_vaVertex.enabled   = false;
    else if (cap == GL_TEXTURE_COORD_ARRAY) s_vaTexCoord.enabled = false;
    else if (cap == GL_COLOR_ARRAY)         s_vaColor.enabled    = false;
}
void GL_VertexPointer(int size, GLenum type, int stride, const void* ptr) {
    s_vaVertex = {ptr, size, type, stride, true};
}
void GL_TexCoordPointer(int size, GLenum type, int stride, const void* ptr) {
    s_vaTexCoord = {ptr, size, type, stride, true};
}
void GL_ColorPointer(int size, GLenum type, int stride, const void* ptr) {
    s_vaColor = {ptr, size, type, stride, true};
}
void GL_NormalPointer(GLenum, int, const void*) {}  // normals not used by shader

// Draw from legacy vertex arrays via our shader (mirrors FlushIM)
void GL_DrawArrays_Compat(GLenum mode, int first, int count) {
    if (!s_vaVertex.enabled || !s_vaVertex.ptr || count <= 0 || !s_prog) {
        FlushPendingImmediateBatch();
        // fallback: attempt raw draw (likely won't show but won't crash)
        glDrawArrays(mode, first, count);
        return;
    }

    if (s_preferDirectVertexArrays && TryDrawArraysDirect(mode, first, count)) {
        return;
    }

    const bool hasTex = s_vaTexCoord.enabled && s_vaTexCoord.ptr;
    const bool hasColor = s_vaColor.enabled && s_vaColor.ptr;

#if defined(__ANDROID__) || defined(MU_IOS)
    // Fast-path for the dominant Mu geometry pattern:
    // glVertexPointer(3,float,0) + glTexCoordPointer(2,float,0) [+ optional color]
    // Route directly to GL_BatchAppendTriangles to avoid the per-vertex conversion
    // loop + temporary s_arrayVerts writes in this function.
    if (kBakeModelViewForImmediate &&
        mode == GL_TRIANGLES &&
        s_vaVertex.type == GL_FLOAT && s_vaVertex.size == 3 &&
        (!hasTex || (s_vaTexCoord.type == GL_FLOAT && s_vaTexCoord.size >= 2)))
    {
        const int posStride = s_vaVertex.stride ? s_vaVertex.stride : (s_vaVertex.size * (int)sizeof(float));
        const int texStride = hasTex ? (s_vaTexCoord.stride ? s_vaTexCoord.stride : (s_vaTexCoord.size * (int)sizeof(float))) : 0;
        const bool tightlyPackedPos = (posStride == 3 * (int)sizeof(float));
        const bool tightlyPackedTex = !hasTex || (texStride == 2 * (int)sizeof(float));

        const bool useColorFastPath =
            hasColor &&
            s_vaColor.type == GL_FLOAT &&
            s_vaColor.size >= 4 &&
            ((s_vaColor.stride ? s_vaColor.stride : (s_vaColor.size * (int)sizeof(float))) == 4 * (int)sizeof(float));

        const bool compatibleStridedColor =
            !hasColor ||
            (s_vaColor.type == GL_FLOAT && s_vaColor.size >= 3);

        if (tightlyPackedPos && tightlyPackedTex)
        {
            const float* posPtr = (const float*)((const uint8_t*)s_vaVertex.ptr + (size_t)first * posStride);
            const float* texPtr = hasTex ? (const float*)((const uint8_t*)s_vaTexCoord.ptr + (size_t)first * texStride) : nullptr;
            const float* colPtr = nullptr;
            if (useColorFastPath)
            {
                const int colStride = s_vaColor.stride ? s_vaColor.stride : (s_vaColor.size * (int)sizeof(float));
                colPtr = (const float*)((const uint8_t*)s_vaColor.ptr + (size_t)first * colStride);
            }

            GL_BatchAppendTriangles(posPtr, colPtr, texPtr, count);
            return;
        }

        if (compatibleStridedColor)
        {
            const uint8_t* posBase = (const uint8_t*)s_vaVertex.ptr + (size_t)first * posStride;
            const uint8_t* texBase = (const uint8_t*)s_vaTexCoord.ptr + (size_t)first * texStride;
            const uint8_t* colBase = nullptr;
            int colStride = 0;
            int colorSize = 0;
            if (hasColor)
            {
                colStride = s_vaColor.stride ? s_vaColor.stride : (s_vaColor.size * (int)sizeof(float));
                colBase = (const uint8_t*)s_vaColor.ptr + (size_t)first * colStride;
                colorSize = s_vaColor.size;
            }

            BatchAppendTrianglesFromFloatArrays(posBase,
                                                posStride,
                                                texBase,
                                                texStride,
                                                colBase,
                                                colStride,
                                                colorSize,
                                                count);
            return;
        }
    }
#endif

    s_arrayVerts.clear();
    s_arrayVerts.reserve((size_t)count);

    const bool fastPos =
        (s_vaVertex.type == GL_FLOAT) &&
        (s_vaVertex.size >= 3);
    const bool fastTex =
        !hasTex ||
        ((s_vaTexCoord.type == GL_FLOAT) && (s_vaTexCoord.size >= 2));
    const bool fastColor =
        !hasColor ||
        ((s_vaColor.type == GL_FLOAT) && (s_vaColor.size >= 3));

    if (fastPos && fastTex && fastColor) {
        const int posStride = s_vaVertex.stride ? s_vaVertex.stride : (s_vaVertex.size * (int)sizeof(float));
        const int texStride = hasTex ? (s_vaTexCoord.stride ? s_vaTexCoord.stride : (s_vaTexCoord.size * (int)sizeof(float))) : 0;
        const int colStride = hasColor ? (s_vaColor.stride ? s_vaColor.stride : (s_vaColor.size * (int)sizeof(float))) : 0;

        const uint8_t* posBase = (const uint8_t*)s_vaVertex.ptr + (size_t)first * posStride;
        const uint8_t* texBase = hasTex ? ((const uint8_t*)s_vaTexCoord.ptr + (size_t)first * texStride) : nullptr;
        const uint8_t* colBase = hasColor ? ((const uint8_t*)s_vaColor.ptr + (size_t)first * colStride) : nullptr;

        for (int i = 0; i < count; ++i) {
            IMVertex v;
            const float* p = (const float*)(posBase + (size_t)i * posStride);
            if (kBakeModelViewForImmediate) {
                TransformByCurrentModelView(p[0], p[1], p[2], v.x, v.y, v.z);
            } else {
                v.x = p[0];
                v.y = p[1];
                v.z = p[2];
            }

            if (hasTex) {
                const float* t = (const float*)(texBase + (size_t)i * texStride);
                v.u = t[0];
                v.v = t[1];
            } else {
                v.u = 0.0f;
                v.v = 0.0f;
            }

            if (hasColor) {
                const float* c = (const float*)(colBase + (size_t)i * colStride);
                v.r = c[0];
                v.g = c[1];
                v.b = c[2];
                v.a = (s_vaColor.size >= 4) ? c[3] : 1.0f;
            } else {
                v.r = s_cur.r;
                v.g = s_cur.g;
                v.b = s_cur.b;
                v.a = s_cur.a;
            }

            s_arrayVerts.push_back(v);
        }
    } else {
        // Generic conversion path for uncommon VA formats.
        for (int i = 0; i < count; ++i) {
            IMVertex v;
            float tmp[4];

            VA_ReadVertex(s_vaVertex, first + i, tmp, 3);
            if (kBakeModelViewForImmediate) {
                TransformByCurrentModelView(tmp[0], tmp[1], tmp[2], v.x, v.y, v.z);
            } else {
                v.x = tmp[0];
                v.y = tmp[1];
                v.z = tmp[2];
            }

            if (hasTex) {
                VA_ReadVertex(s_vaTexCoord, first + i, tmp, 2);
                v.u = tmp[0];
                v.v = tmp[1];
            } else {
                v.u = 0.0f;
                v.v = 0.0f;
            }

            if (hasColor) {
                VA_ReadVertex(s_vaColor, first + i, tmp, 4);
                v.r = tmp[0];
                v.g = tmp[1];
                v.b = tmp[2];
                v.a = tmp[3];
            } else {
                v.r = s_cur.r;
                v.g = s_cur.g;
                v.b = s_cur.b;
                v.a = s_cur.a;
            }

            s_arrayVerts.push_back(v);
        }
    }

    if (kBakeModelViewForImmediate) {
        AppendVertsToPendingImmediateBatch(s_arrayVerts, mode);
    } else {
        DrawVertexList(s_arrayVerts, mode, false);
    }
    ++s_vaConvertedDrawCalls;
}

// =============================================================================
// Bulk draw — bypasses immediate mode entirely for maximum throughput.
// Vertex layout: x,y,z, r,g,b,a, u,v  (9 floats = sizeof(IMVertex))
// =============================================================================
void GL_DrawQuadsBulk(const float* vertexData, int quadCount) {
    if (!vertexData || quadCount <= 0 || !s_prog || !s_vbo) return;

    FlushPendingImmediateBatch();

    if (!EnsureQuadIndexCapacity((size_t)quadCount)) return;

    const GLsizei vertCount = quadCount * 4;
    const GLsizeiptr dataBytes = (GLsizeiptr)(vertCount * sizeof(IMVertex));

    const GLint first = StreamVertexData(vertexData, dataBytes);
    if (first < 0) return;

    ApplyShaderStateCommon(false);  // use full MVP (not baked)

    BindImmediateVertexAttribLayout();

    BindElementArrayBufferCached(s_ebo);
    const GLsizei indexCount = quadCount * 6;
    DrawQuadIndicesFrom(indexCount, first);
    ++s_drawCallCount; ++s_drawSite[4];
    s_totalVertices += indexCount;
}

void GL_DrawTrisBulk(const float* vertexData, int triCount) {
    if (!vertexData || triCount <= 0 || !s_prog || !s_vbo) return;

    FlushPendingImmediateBatch();

    const GLsizei vertCount = triCount * 3;
    const GLsizeiptr dataBytes = (GLsizeiptr)(vertCount * sizeof(IMVertex));

    const GLint first = StreamVertexData(vertexData, dataBytes);
    if (first < 0) return;

    ApplyShaderStateCommon(false);

    BindImmediateVertexAttribLayout();

    BindElementArrayBufferCached(0);
    glDrawArrays(GL_TRIANGLES, first, vertCount);
    ++s_drawCallCount; ++s_drawSite[5];
    s_totalVertices += vertCount;
}

// =============================================================================
// GL_BatchAppendTriangles — high-perf single-pass mesh batch append
// Combines 3 separate arrays (positions, colors, texcoords) directly into the
// pending IMVertex batch in ONE pass, baking ModelView into positions.
// Compared to glVertexPointer + glDrawArrays path:
//   OLD: RenderMesh builds 3 arrays → GL_DrawArrays_Compat reads them → s_arrayVerts
//        → AppendVertsToPendingImmediateBatch → s_batchVerts  (2 full passes over data)
//   NEW: RenderMesh builds 3 arrays → GL_BatchAppendTriangles → s_batchVerts directly
//        (1 pass over data, eliminates s_arrayVerts intermediate copy)
// =============================================================================
void GL_BatchAppendTriangles(const float* positions,
                             const float* colors,
                             const float* texcoords,
                             int numVerts)
{
    if (!positions || numVerts <= 0 || !s_prog) return;

    // Flush if current batch uses a different primitive mode
    if (s_hasBatch && s_batchPrimMode != GL_TRIANGLES) {
        FlushPendingImmediateBatch();
    }

    // Flush if batch would overflow capacity limit
    if (s_batchVerts.size() + static_cast<size_t>(numVerts) > kMaxBatchVerts) {
        FlushPendingImmediateBatch();
    }

    // Append directly into s_batchVerts (no intermediate s_arrayVerts allocation)
    const size_t oldSize = s_batchVerts.size();
    s_batchVerts.resize(oldSize + static_cast<size_t>(numVerts));
    IMVertex* dst = s_batchVerts.data() + oldSize;

    // ── Fast matrix pre-cache ──────────────────────────────────────────────
    // Pre-load ModelView elements into locals so the compiler can keep them in
    // registers for the entire loop (avoids repeated s_mvStack[depth] loads).
    // Also hoists the per-vertex tw-divide check: game matrices are always
    // affine (row 3 = [0,0,0,1]), so the divide is never needed — skip it.
    const Mat4& mv = s_mvStack[s_mvDepth];
    const float m00=mv[0], m10=mv[4], m20=mv[8],  m30=mv[12];
    const float m01=mv[1], m11=mv[5], m21=mv[9],  m31=mv[13];
    const float m02=mv[2], m12=mv[6], m22=mv[10], m32=mv[14];

    // Hoist color branch outside the inner loop — avoids per-vertex branch.
    if (colors) {
        for (int i = 0; i < numVerts; ++i) {
            const float px = positions[i*3+0], py = positions[i*3+1], pz = positions[i*3+2];
            dst[i].x = m00*px + m10*py + m20*pz + m30;
            dst[i].y = m01*px + m11*py + m21*pz + m31;
            dst[i].z = m02*px + m12*py + m22*pz + m32;
            // memcpy is recognized by the compiler as a 16-byte vector store
            __builtin_memcpy(&dst[i].r, colors + i*4, 4*sizeof(float));
            if (texcoords) {
                __builtin_memcpy(&dst[i].u, texcoords + i*2, 2*sizeof(float));
            } else {
                dst[i].u = 0.0f;
                dst[i].v = 0.0f;
            }
        }
    } else {
        const float cr=s_cur.r, cg=s_cur.g, cb=s_cur.b, ca=s_cur.a;
        for (int i = 0; i < numVerts; ++i) {
            const float px = positions[i*3+0], py = positions[i*3+1], pz = positions[i*3+2];
            dst[i].x = m00*px + m10*py + m20*pz + m30;
            dst[i].y = m01*px + m11*py + m21*pz + m31;
            dst[i].z = m02*px + m12*py + m22*pz + m32;
            dst[i].r=cr; dst[i].g=cg; dst[i].b=cb; dst[i].a=ca;
            if (texcoords) {
                __builtin_memcpy(&dst[i].u, texcoords + i*2, 2*sizeof(float));
            } else {
                dst[i].u = 0.0f;
                dst[i].v = 0.0f;
            }
        }
    }

    s_hasBatch     = true;
    s_batchPrimMode = GL_TRIANGLES;
    ++s_vaConvertedDrawCalls;
}

void GL_BatchAppendTrianglesConstColor(const float* positions,
                                       const float* texcoords,
                                       int numVerts,
                                       const float color[4])
{
    if (!positions || numVerts <= 0 || !s_prog || color == nullptr) return;

    if (s_hasBatch && s_batchPrimMode != GL_TRIANGLES) {
        FlushPendingImmediateBatch();
    }

    if (s_batchVerts.size() + static_cast<size_t>(numVerts) > kMaxBatchVerts) {
        FlushPendingImmediateBatch();
    }

    const size_t oldSize = s_batchVerts.size();
    s_batchVerts.resize(oldSize + static_cast<size_t>(numVerts));
    IMVertex* dst = s_batchVerts.data() + oldSize;

    const Mat4& mv = s_mvStack[s_mvDepth];
    const float m00=mv[0], m10=mv[4], m20=mv[8],  m30=mv[12];
    const float m01=mv[1], m11=mv[5], m21=mv[9],  m31=mv[13];
    const float m02=mv[2], m12=mv[6], m22=mv[10], m32=mv[14];

    const float cr = color[0];
    const float cg = color[1];
    const float cb = color[2];
    const float ca = color[3];

    if (texcoords) {
        for (int i = 0; i < numVerts; ++i) {
            const float px = positions[i*3+0], py = positions[i*3+1], pz = positions[i*3+2];
            dst[i].x = m00*px + m10*py + m20*pz + m30;
            dst[i].y = m01*px + m11*py + m21*pz + m31;
            dst[i].z = m02*px + m12*py + m22*pz + m32;
            dst[i].r = cr;
            dst[i].g = cg;
            dst[i].b = cb;
            dst[i].a = ca;
            __builtin_memcpy(&dst[i].u, texcoords + i*2, 2*sizeof(float));
        }
    } else {
        for (int i = 0; i < numVerts; ++i) {
            const float px = positions[i*3+0], py = positions[i*3+1], pz = positions[i*3+2];
            dst[i].x = m00*px + m10*py + m20*pz + m30;
            dst[i].y = m01*px + m11*py + m21*pz + m31;
            dst[i].z = m02*px + m12*py + m22*pz + m32;
            dst[i].r = cr;
            dst[i].g = cg;
            dst[i].b = cb;
            dst[i].a = ca;
            dst[i].u = 0.0f;
            dst[i].v = 0.0f;
        }
    }

    s_hasBatch = true;
    s_batchPrimMode = GL_TRIANGLES;
    ++s_vaConvertedDrawCalls;
}

void GL_BatchAppendIndexedTrianglesLitTex(const float* positions3,
                                          const float* lights3,
                                          const float* texcoords2,
                                          const short* vertexIndexBase,
                                          const short* normalIndexBase,
                                          const short* texCoordIndexBase,
                                          int triangleStrideBytes,
                                          int triangleCount,
                                          float alpha,
                                          float texOffsetU,
                                          float texOffsetV)
{
    if (!positions3 || !lights3 || !texcoords2 ||
        !vertexIndexBase || !normalIndexBase || !texCoordIndexBase ||
        triangleStrideBytes <= 0 || triangleCount <= 0 || !s_prog) {
        return;
    }

    // TEMP profiling: how much of the object-render bucket is this per-vertex
    // CPU transform loop? Sets the ceiling for moving static meshes to a
    // persistent VBO drawn with a GPU-side MVP, which removes this work.
    const uint64_t profT0 = MU_MobilePerfNow();

    const int numVerts = triangleCount * 3;
    g_ProfBatchAppendVerts += numVerts;
    if (s_hasBatch && s_batchPrimMode != GL_TRIANGLES) {
        FlushPendingImmediateBatch();
    }
    if (s_batchVerts.size() + static_cast<size_t>(numVerts) > kMaxBatchVerts) {
        FlushPendingImmediateBatch();
    }

    const size_t oldSize = s_batchVerts.size();
    s_batchVerts.resize(oldSize + static_cast<size_t>(numVerts));
    IMVertex* dst = s_batchVerts.data() + oldSize;

    const Mat4& mv = s_mvStack[s_mvDepth];
    const float m00=mv[0], m10=mv[4], m20=mv[8],  m30=mv[12];
    const float m01=mv[1], m11=mv[5], m21=mv[9],  m31=mv[13];
    const float m02=mv[2], m12=mv[6], m22=mv[10], m32=mv[14];
    const float vertexAlpha = (alpha >= 0.99f) ? 1.0f : alpha;

    int out = 0;
    const uint8_t* vertexBytes = reinterpret_cast<const uint8_t*>(vertexIndexBase);
    const uint8_t* normalBytes = reinterpret_cast<const uint8_t*>(normalIndexBase);
    const uint8_t* texBytes = reinterpret_cast<const uint8_t*>(texCoordIndexBase);
    for (int tri = 0; tri < triangleCount; ++tri) {
        const short* vi = reinterpret_cast<const short*>(vertexBytes + static_cast<size_t>(tri) * triangleStrideBytes);
        const short* ni = reinterpret_cast<const short*>(normalBytes + static_cast<size_t>(tri) * triangleStrideBytes);
        const short* ti = reinterpret_cast<const short*>(texBytes + static_cast<size_t>(tri) * triangleStrideBytes);
        for (int corner = 0; corner < 3; ++corner) {
            const float* p = positions3 + static_cast<int>(vi[corner]) * 3;
            const float* l = lights3 + static_cast<int>(ni[corner]) * 3;
            const float* t = texcoords2 + static_cast<int>(ti[corner]) * 2;
            IMVertex& v = dst[out++];
            v.x = m00*p[0] + m10*p[1] + m20*p[2] + m30;
            v.y = m01*p[0] + m11*p[1] + m21*p[2] + m31;
            v.z = m02*p[0] + m12*p[1] + m22*p[2] + m32;
            v.r = l[0];
            v.g = l[1];
            v.b = l[2];
            v.a = vertexAlpha;
            v.u = t[0] + texOffsetU;
            v.v = t[1] + texOffsetV;
        }
    }

    s_hasBatch = true;
    s_batchPrimMode = GL_TRIANGLES;
    ++s_vaConvertedDrawCalls;
    g_ProfBatchAppendTicks += (MU_MobilePerfNow() - profT0);
}

// =============================================================================
// Object-mesh material queue
//
// Object rendering measured ~37ms/frame in a decoration-heavy scene, of which
// the per-vertex CPU transform was only ~0.36ms. The cost was ~530 GL draw
// calls, each with its own vertex-buffer upload: a mesh's texture differs from
// the previous mesh's, which cuts the pending immediate batch, so nearly every
// mesh became its own draw (cut[tex] ~420/frame). Those cuts are WITHIN each
// object - an object's meshes use different textures - so ordering objects by
// model type does not help; the draws have to be regrouped by material.
//
// This collects eligible object mesh draws into per-texture buckets and issues
// one draw per bucket, which is the same thing TerrainBatch_* already does for
// terrain faces. Opaque draws are safe to reorder because they depth-test and
// depth-write; they are flushed before the transparent list, which preserves
// its original submission order (alpha blending is order-dependent).
// =============================================================================
// Appends the same per-vertex work GL_BatchAppendIndexedTrianglesLitTex does -
// including baking the modelview, so meshes from different objects can share
// one buffer - but into a caller-owned float array instead of the single
// pending batch. The caller (ZzzBMD.cpp) keeps the per-texture buckets and
// owns the GL state, exactly as ZzzLodTerrain.cpp's TerrainBatch does; this
// side only supplies the vertex work, which needs the modelview stack.
// Returns the number of vertices appended.
int GL_AppendMeshVertsBaked(std::vector<float>& out,
                            const float* positions3,
                            const float* lights3,
                            const float* texcoords2,
                            const short* vertexIndexBase,
                            const short* normalIndexBase,
                            const short* texCoordIndexBase,
                            int triangleStrideBytes,
                            int triangleCount,
                            float alpha,
                            float texOffsetU,
                            float texOffsetV)
{
    if (!positions3 || !lights3 || !texcoords2 ||
        !vertexIndexBase || !normalIndexBase || !texCoordIndexBase ||
        triangleStrideBytes <= 0 || triangleCount <= 0) {
        return 0;
    }

    const int numVerts = triangleCount * 3;
    const size_t oldFloats = out.size();
    out.resize(oldFloats + static_cast<size_t>(numVerts) * 9);
    IMVertex* dst = reinterpret_cast<IMVertex*>(out.data() + oldFloats);

    const Mat4& mv = s_mvStack[s_mvDepth];
    const float m00=mv[0], m10=mv[4], m20=mv[8],  m30=mv[12];
    const float m01=mv[1], m11=mv[5], m21=mv[9],  m31=mv[13];
    const float m02=mv[2], m12=mv[6], m22=mv[10], m32=mv[14];
    const float vertexAlpha = (alpha >= 0.99f) ? 1.0f : alpha;

    int outIdx = 0;
    const uint8_t* vertexBytes = reinterpret_cast<const uint8_t*>(vertexIndexBase);
    const uint8_t* normalBytes = reinterpret_cast<const uint8_t*>(normalIndexBase);
    const uint8_t* texBytes = reinterpret_cast<const uint8_t*>(texCoordIndexBase);
    for (int tri = 0; tri < triangleCount; ++tri) {
        const short* vi = reinterpret_cast<const short*>(vertexBytes + static_cast<size_t>(tri) * triangleStrideBytes);
        const short* ni = reinterpret_cast<const short*>(normalBytes + static_cast<size_t>(tri) * triangleStrideBytes);
        const short* ti = reinterpret_cast<const short*>(texBytes + static_cast<size_t>(tri) * triangleStrideBytes);
        for (int corner = 0; corner < 3; ++corner) {
            const float* p = positions3 + static_cast<int>(vi[corner]) * 3;
            const float* l = lights3 + static_cast<int>(ni[corner]) * 3;
            const float* t = texcoords2 + static_cast<int>(ti[corner]) * 2;
            IMVertex& v = dst[outIdx++];
            v.x = m00*p[0] + m10*p[1] + m20*p[2] + m30;
            v.y = m01*p[0] + m11*p[1] + m21*p[2] + m31;
            v.z = m02*p[0] + m12*p[1] + m22*p[2] + m32;
            v.r = l[0];
            v.g = l[1];
            v.b = l[2];
            v.a = vertexAlpha;
            v.u = t[0] + texOffsetU;
            v.v = t[1] + texOffsetV;
        }
    }
    return numVerts;
}

// Draws a caller-owned vertex array that ALREADY has the modelview baked in
// (GL_AppendMeshVertsBaked above). GL_DrawTrisBulk cannot be used for this: it
// applies the full MVP, which would transform the vertices a second time.
void GL_DrawTrisBulkBaked(const float* vertexData, int triCount) {
    if (!vertexData || triCount <= 0 || !s_prog || !s_vbo) return;

    FlushPendingImmediateBatch(kFlushCauseFrame);

    const GLsizei vertCount = triCount * 3;
    const GLsizeiptr dataBytes = (GLsizeiptr)(vertCount * sizeof(IMVertex));

    const GLint first = StreamVertexData(vertexData, dataBytes);
    if (first < 0) return;

    ApplyShaderStateCommon(true);   // modelview already baked into the verts

    BindImmediateVertexAttribLayout();

    BindElementArrayBufferCached(0);
    glDrawArrays(GL_TRIANGLES, first, vertCount);
    ++s_drawCallCount; ++s_drawSite[5];
    s_totalVertices += vertCount;
}

void GL_BatchAppendIndexedTrianglesConstColor(const float* positions3,
                                              const float* texcoords2,
                                              const short* vertexIndexBase,
                                              const short* texCoordIndexBase,
                                              int triangleStrideBytes,
                                              int triangleCount,
                                              const float color[4],
                                              float texOffsetU,
                                              float texOffsetV)
{
    if (!positions3 || !vertexIndexBase || !color ||
        triangleStrideBytes <= 0 || triangleCount <= 0 || !s_prog) {
        return;
    }

    const bool hasTex = texcoords2 != nullptr && texCoordIndexBase != nullptr;
    const int numVerts = triangleCount * 3;
    if (s_hasBatch && s_batchPrimMode != GL_TRIANGLES) {
        FlushPendingImmediateBatch();
    }
    if (s_batchVerts.size() + static_cast<size_t>(numVerts) > kMaxBatchVerts) {
        FlushPendingImmediateBatch();
    }

    const size_t oldSize = s_batchVerts.size();
    s_batchVerts.resize(oldSize + static_cast<size_t>(numVerts));
    IMVertex* dst = s_batchVerts.data() + oldSize;

    const Mat4& mv = s_mvStack[s_mvDepth];
    const float m00=mv[0], m10=mv[4], m20=mv[8],  m30=mv[12];
    const float m01=mv[1], m11=mv[5], m21=mv[9],  m31=mv[13];
    const float m02=mv[2], m12=mv[6], m22=mv[10], m32=mv[14];
    const float cr = color[0];
    const float cg = color[1];
    const float cb = color[2];
    const float ca = color[3];

    int out = 0;
    const uint8_t* vertexBytes = reinterpret_cast<const uint8_t*>(vertexIndexBase);
    const uint8_t* texBytes = reinterpret_cast<const uint8_t*>(texCoordIndexBase);
    for (int tri = 0; tri < triangleCount; ++tri) {
        const short* vi = reinterpret_cast<const short*>(vertexBytes + static_cast<size_t>(tri) * triangleStrideBytes);
        const short* ti = hasTex
            ? reinterpret_cast<const short*>(texBytes + static_cast<size_t>(tri) * triangleStrideBytes)
            : nullptr;
        for (int corner = 0; corner < 3; ++corner) {
            const float* p = positions3 + static_cast<int>(vi[corner]) * 3;
            IMVertex& v = dst[out++];
            v.x = m00*p[0] + m10*p[1] + m20*p[2] + m30;
            v.y = m01*p[0] + m11*p[1] + m21*p[2] + m31;
            v.z = m02*p[0] + m12*p[1] + m22*p[2] + m32;
            v.r = cr;
            v.g = cg;
            v.b = cb;
            v.a = ca;
            if (hasTex) {
                const float* t = texcoords2 + static_cast<int>(ti[corner]) * 2;
                v.u = t[0] + texOffsetU;
                v.v = t[1] + texOffsetV;
            } else {
                v.u = 0.0f;
                v.v = 0.0f;
            }
        }
    }

    s_hasBatch = true;
    s_batchPrimMode = GL_TRIANGLES;
    ++s_vaConvertedDrawCalls;
}

// =============================================================================
// GPU skinning (additive, NOT wired into any render path yet)
// =============================================================================
// Status: infrastructure only. Nothing in the game calls GL_DrawSkinnedMesh
// yet - BMD::RenderMesh/BMD::Transform still do CPU skinning exactly as
// before. This exists so the shader/UBO plumbing can be built and verified
// (compiles, links, runs standalone) before any character's render path is
// touched. See BMD::Transform (ZzzBMD.cpp) for the CPU reference this must
// match: out[i] = dot(restVec, boneRow[i]) [+ boneRow[i].w for position].
//
// Own attribute/uniform locations (20+) and UBO binding (0), deliberately
// far from gl_compat's own program (locations 0-7) and RenderBackend.cpp's
// blit program (locations 10-11) so none of the three ever alias.
// Not ZzzBMD.h's MAX_BONES directly - this file stays free of game-layer
// includes. Keep this in sync with MAX_BONES (currently 200) if that changes.
static constexpr int kMaxSkinBones = 200;

// NOTE: vertex attribute locations MUST be < GL_MAX_VERTEX_ATTRIBS, which
// GLES 3.x only guarantees to be 16 (and Adreno reports exactly 16). Using
// 20-23 here made the program fail to LINK on-device - silently, since
// LOGE is compiled out in this file. Locations 4-7 stay clear of
// gl_compat's own program (0-2) and RenderBackend.cpp's blit (10-11).
// Uniform locations below are a separate, much larger namespace
// (GL_MAX_UNIFORM_LOCATIONS, min 1024) so those can stay at 20+.
static const char* s_skinVertSrc = R"(#version 310 es
layout(location = 4) in vec3 a_restPos;
layout(location = 5) in vec3 a_restNormal;
layout(location = 6) in vec2 a_uv;
layout(location = 7) in float a_boneIndex;

// Uniform locations: a mat4 consumes FOUR consecutive locations (one per
// column), so u_mvp occupies 0-3 and everything else starts at 4. An earlier
// version put u_lightDir at 21 while u_mvp sat at 20 - overlapping columns.
// Adreno tolerated it, but it is invalid GLSL and would fail elsewhere.
layout(location = 0) uniform highp mat4 u_mvp;
layout(location = 4) uniform highp vec3 u_lightDir;
layout(location = 5) uniform int u_useVertexLight;  // 0 = flat body light
layout(location = 6) uniform vec2 u_texOffset;      // wave/scroll offset
layout(location = 7) uniform int u_chromeMode;      // 1 = env-map uv from normal

layout(std140, binding = 0) uniform BoneBlock {
    highp vec4 u_boneRows[600]; // kMaxSkinBones(200) * 3 rows - keep in sync
};

out mediump vec2 v_uv;
out mediump vec2 v_chromeUv;
out mediump float v_light;

void main() {
    int b = int(a_boneIndex) * 3;
    vec4 row0 = u_boneRows[b + 0];
    vec4 row1 = u_boneRows[b + 1];
    vec4 row2 = u_boneRows[b + 2];

    vec3 skinnedPos;
    skinnedPos.x = dot(row0.xyz, a_restPos) + row0.w;
    skinnedPos.y = dot(row1.xyz, a_restPos) + row1.w;
    skinnedPos.z = dot(row2.xyz, a_restPos) + row2.w;

    vec3 skinnedNormal;
    skinnedNormal.x = dot(row0.xyz, a_restNormal);
    skinnedNormal.y = dot(row1.xyz, a_restNormal);
    skinnedNormal.z = dot(row2.xyz, a_restNormal);

    gl_Position = u_mvp * vec4(skinnedPos, 1.0);

    // Chrome/metal passes derive their texcoords from the transformed normal
    // (environment mapping) instead of the mesh UVs - matches the CPU path's
    // g_chrome[] lookup, which is built from the same rotated normal.
    if (u_chromeMode == 1) {
        v_uv = normalize(skinnedNormal).xy * 0.5 + 0.5 + u_texOffset;
    } else {
        v_uv = a_uv + u_texOffset;
    }

    // Always computed (a couple of extra dot products - the GPU sits idle
    // per this codebase's own diagnosis) so a fused chrome overlay (Phase B)
    // has its env-map UV ready regardless of u_chromeMode, which stays
    // exactly as it was for the existing standalone chrome/metal draws this
    // shader already served before Phase B existed.
    v_chromeUv = normalize(skinnedNormal).xy * 0.5 + 0.5 + u_texOffset;

    if (u_useVertexLight == 1) {
        // Matches BMD::Transform's per-vertex luminosity EXACTLY, including NOT
        // normalizing either vector: dot(rotatedNormal, LightPosition)*0.8+0.4,
        // floored at 0.2. LightPosition on the CPU side is a fixed-magnitude
        // (~1.5) rotated vector, not a unit direction - normalizing here would
        // change the brightness compared to the CPU path, not just its precision.
        float luminosity = dot(skinnedNormal, u_lightDir) * 0.8 + 0.4;
        v_light = max(luminosity, 0.2);
    } else {
        v_light = 1.0;   // unlit pass: colour comes straight from u_bodyLight
    }
}
)";

// Fragment uniforms continue the vertex shader's numbering (u_mvp took 0-3).
static const char* s_skinFragSrc = R"(#version 310 es
precision mediump float;
layout(location = 8)  uniform sampler2D u_sampler;
layout(location = 9)  uniform vec3 u_bodyLight;
layout(location = 10) uniform float u_alpha;
layout(location = 11) uniform int u_useTexture;   // 0 = untextured colour pass
layout(location = 12) uniform vec3 u_glowColor;   // additive glow tint (0,0,0 = none)
layout(location = 13) uniform sampler2D u_chromeSampler;
layout(location = 14) uniform vec3 u_chromeBodyLight;
layout(location = 15) uniform int u_hasChrome;    // 1 = fold a chrome/metal overlay into this draw
layout(location = 16) uniform sampler2D u_overlay2Sampler;
layout(location = 17) uniform vec3 u_overlay2BodyLight;
layout(location = 18) uniform int u_hasOverlay2;  // 1 = fold a SECOND overlay (typically METAL) in too

in vec2 v_uv;
in vec2 v_chromeUv;
in float v_light;
out vec4 outFragColor;

void main() {
    if (u_useTexture == 1) {
        vec4 texColor = texture(u_sampler, v_uv);
        // Equivalent to drawing the plain lit pass then a second additive
        // RENDER_TEXTURE|RENDER_BRIGHT pass on top (glBlendFunc(GL_ONE,GL_ONE),
        // depth write off) in one draw: both passes share the same v_light
        // factor (RenderPartObjectEffect never changes LightEnable between
        // them), so texColor*bodyLight*light + texColor*glow*light factors to
        // texColor*(bodyLight+glow)*light - see BMD::RenderMesh's pending-glow
        // comment for the depth-write equivalence argument.
        // u_bodyLight is clamped for the same reason the immediate-mode vertex
        // shader clamps a_color: on the CPU path this value arrives via
        // glColor3fv and desktop fixed-function clamps it to [0,1], but as a
        // uniform here nothing does. Terrain light accumulates past 1.0 wherever
        // lights overlap (AddTerrainLight has no upper clamp), which blew
        // lit surfaces out to white. The additive overlay terms below are left
        // unclamped on purpose - they stand in for separate additive passes
        // that clamped at framebuffer-write time, not at the vertex.
        vec3 rgb = texColor.rgb * (clamp(u_bodyLight, 0.0, 1.0) * v_light + u_glowColor);
        if (u_hasChrome == 1) {
            // Mirrors a separate RENDER_CHROME|RENDER_BRIGHT pass drawn on top
            // (same additive glBlendFunc(GL_ONE,GL_ONE), depth write off, per
            // BMD::RenderMesh's chrome+bright branch) - see g_PendingChromeActive
            // in ZzzBMD.cpp for the CPU-side equivalence argument.
            vec4 chromeColor = texture(u_chromeSampler, v_chromeUv);
            rgb += chromeColor.rgb * u_chromeBodyLight * v_light;
        }
        if (u_hasOverlay2 == 1) {
            // Second stacked overlay (the METAL pass on +9 and up). Additive
            // exactly like the first, and sampled at the same env-mapped UV -
            // both variants resolve to Render==RENDER_CHROME and already share
            // this one approximated formula on the GPU path.
            vec4 overlay2Color = texture(u_overlay2Sampler, v_chromeUv);
            rgb += overlay2Color.rgb * u_overlay2BodyLight * v_light;
        }
        outFragColor = vec4(rgb, texColor.a * u_alpha);
    } else {
        outFragColor = vec4(u_bodyLight * v_light, u_alpha);
    }
}
)";

static GLuint s_skinProg = 0;
static GLuint s_skinBoneUbo = 0;
static GLuint s_skinVbo = 0;      // scratch VBO for callers that pass raw vertex data
static GLsizeiptr s_skinVboCapacity = 0;
static GLuint s_skinEbo = 0;      // scratch EBO - see the client-pointer note in GL_DrawSkinnedMesh
static GLsizeiptr s_skinEboCapacity = 0;

using SkinVertex = GLSkinVertex; // layout owned by gl_compat.h - callers build it directly

// Redundant-state elimination for the skinned path. Each skinned draw used to
// issue ~35 GL calls - every uniform, all four attribute pointers, enables,
// binds and unbinds - almost all of it identical to the draw before. Cheap on
// a flagship driver, but the emulator ships every call over its GL pipe and a
// low-end CPU (Samsung A12 class) pays driver overhead per call, and those are
// where FPS collapsed.
//
// Bone count last uploaded, -1 = nothing yet (GL_UpdateSkinningBones).
static int s_skinLastBoneCount = -1;

// A/B switch: false routes every skinned draw through DrawSkinnedMeshUncached
// (the pre-2026-09-24 path) so the two can be compared on a device without
// swapping builds. Kept for measurement, like the text-cache switches.
static bool s_skinStateCacheEnabled = true;

// Uniform values last set on s_skinProg. Uniforms belong to the program
// object and only GL_DrawSkinnedMesh sets this program's, so the cache stays
// true until the program is recreated.
struct SkinUniformCache {
    bool valid = false;
    float mvp[16];
    float lightDir[3];
    int vertexLight;
    float texOffset[2];
    int chromeMode;
    float bodyLight[3];
    float alpha;
    int useTexture;
    float glow[3];
    int hasChrome;
    float chromeLight[3];
    int hasOverlay2;
    float overlay2Light[3];
};
static SkinUniformCache s_skinUniforms;
static bool s_skinUboBound = false;

// One vertex array object per mesh, keyed by the mesh's VBO name: the four
// attribute pointers and the element buffer are recorded once, so a draw is
// bind VAO / draw / bind VAO 0. gl_compat's other paths all use VAO 0, whose
// attribute and element-buffer state is untouched by this.
static std::unordered_map<GLuint, GLuint> s_skinVaos;

static void ResetSkinStateCaches() {
    s_skinLastBoneCount = -1;
    s_skinUniforms.valid = false;
    s_skinUboBound = false;
}

static inline bool SkinFloatsDiffer(const float* a, const float* b, int n) {
    return memcmp(a, b, sizeof(float) * n) != 0;
}

void GL_SetSkinStateCache(bool enabled) {
    s_skinStateCacheEnabled = enabled;
    ResetSkinStateCaches();
}
bool GL_GetSkinStateCache() { return s_skinStateCacheEnabled; }

void GL_GetCurrentMVP(float mvp[16]) {
    GetMVP(mvp);
}

bool GL_SkinInit() {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, s_skinVertSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, s_skinFragSrc);
    if (!vs || !fs) {
        LOGE("GL_SkinInit: shader compile failed");
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    s_skinProg = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!s_skinProg) {
        LOGE("GL_SkinInit: link failed");
        return false;
    }

    glGenBuffers(1, &s_skinBoneUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, s_skinBoneUbo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 4 * 3 * kMaxSkinBones, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glGenBuffers(1, &s_skinVbo);
    glGenBuffers(1, &s_skinEbo);

    // Sampler units never change - set once here instead of on every draw.
    glUseProgram(s_skinProg);
    glUniform1i(8, 0);
    glUniform1i(13, 1);
    glUniform1i(16, 2);
    ResetSkinStateCaches();

    GL_InvalidateCachedGLState();
    LOGI("GL_SkinInit: OK (prog=%u, boneUbo=%u)", s_skinProg, s_skinBoneUbo);
    return true;
}

void GL_SkinShutdown() {
    if (s_skinProg)    { glDeleteProgram(s_skinProg); s_skinProg = 0; }
    if (s_skinBoneUbo) { glDeleteBuffers(1, &s_skinBoneUbo); s_skinBoneUbo = 0; }
    if (s_skinVbo)     { glDeleteBuffers(1, &s_skinVbo); s_skinVbo = 0; }
    if (s_skinEbo)     { glDeleteBuffers(1, &s_skinEbo); s_skinEbo = 0; }
    s_skinVboCapacity = 0;
    s_skinEboCapacity = 0;
    for (const auto& entry : s_skinVaos) {
        glDeleteVertexArrays(1, &entry.second);
    }
    s_skinVaos.clear();
    ResetSkinStateCaches();
}

bool GL_SkinIsReady() {
    return s_skinProg != 0 && s_skinBoneUbo != 0;
}

// boneMatrix3x4: boneCount entries of BMD's float[3][4] affine format (as
// produced by BMD::Animation) - same row-major, translation-in-column-3
// layout VectorTransform/VectorRotate use. Uploads directly, no conversion.
void GL_UpdateSkinningBones(const float boneMatrix3x4[][3][4], int boneCount) {
    if (!s_skinBoneUbo || boneCount <= 0) {
        return;
    }
    if (boneCount > kMaxSkinBones) {
        boneCount = kMaxSkinBones;
    }
    // The whole buffer is written every time, never just the boneCount bones
    // actually in use, with identity in the unused tail.
    //
    // Partially filling it was the cause of characters rendering wrong: some
    // draw indexes a bone slot past boneCount, and whatever happened to be in
    // that slot got used as a bone matrix. With the buffer reused in place
    // that was the previous character's leftover bones, which skinned the mesh
    // into a degenerate shape that collapsed off-screen - player bodies
    // vanishing in crowded areas while their wings (a separate, non-skinned
    // model that never takes this path) still drew. Orphaning alone made it
    // louder rather than better: the tail became freshly-undefined memory and
    // the same meshes exploded across the screen instead of disappearing,
    // which is what confirmed out-of-range slots were being read at all.
    //
    // Identity is the safe value for an unused slot: a vertex that lands on
    // one is drawn at its rest position instead of somewhere undefined, so a
    // stray index can no longer wreck the mesh (or leak another character's
    // pose into it). Cheap enough to do unconditionally - 200 bones is 9.6KB,
    // and this buffer is already fully rewritten once per skinned draw.
    static float s_boneUpload[kMaxSkinBones][3][4];

    // Every mesh of a character is drawn with that character's bones, and
    // the caller uploads before each mesh - so most calls repeat exactly the
    // bones already in the buffer. Skipping those is pure redundancy: the
    // GPU already has this data, and not writing also means not touching a
    // buffer queued draws may still be reading. Any change still rewrites the
    // whole buffer below, identity tail included. This was ~300-660
    // orphan+uploads of 9.6KB a frame; slow drivers and the emulator's GL
    // pipe paid for every one.
    if (s_skinStateCacheEnabled && boneCount == s_skinLastBoneCount &&
        memcmp(s_boneUpload, boneMatrix3x4, sizeof(float) * 3 * 4 * boneCount) == 0) {
        return;
    }
    s_skinLastBoneCount = boneCount;

    memcpy(s_boneUpload, boneMatrix3x4, sizeof(float) * 3 * 4 * boneCount);
    for (int i = boneCount; i < kMaxSkinBones; ++i) {
        memset(s_boneUpload[i], 0, sizeof(s_boneUpload[i]));
        s_boneUpload[i][0][0] = 1.0f;
        s_boneUpload[i][1][1] = 1.0f;
        s_boneUpload[i][2][2] = 1.0f;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, s_skinBoneUbo);
    // Orphan first: this single UBO is rewritten immediately before each
    // skinned draw, so writing in place can land in memory the GPU is still
    // reading for an already-queued draw. Same hazard the 2D quad path in this
    // file documents for its VBO, where per-draw orphaning measured as the
    // correct choice on Mali.
    glBufferData(GL_UNIFORM_BUFFER, sizeof(s_boneUpload), nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(s_boneUpload), s_boneUpload);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

// Draws one skinned mesh from caller-supplied vertex/index data using the
// bone matrices last uploaded via GL_UpdateSkinningBones. mvp should already
// include the object's world transform (BodyOrigin/Angle/Scale) composed
// with view*projection, since vertices here are bone-local rest-pose, not
// pre-baked to world space like gl_compat's other draw paths.
void GL_DeleteSkinnedMeshBuffers(unsigned int* vboInOut, unsigned int* eboInOut) {
    if (vboInOut && *vboInOut) {
        // Its VAO goes with it - the VBO name can be handed out again.
        const auto vao = s_skinVaos.find(*vboInOut);
        if (vao != s_skinVaos.end()) {
            glDeleteVertexArrays(1, &vao->second);
            s_skinVaos.erase(vao);
        }
    }
    if (vboInOut && *vboInOut) { glDeleteBuffers(1, vboInOut); *vboInOut = 0; }
    if (eboInOut && *eboInOut) { glDeleteBuffers(1, eboInOut); *eboInOut = 0; }
}

// The pre-2026-09-24 draw path, kept verbatim behind GL_SetSkinStateCache(false)
// so the redundant-state elimination above can be A/B'd on a device without
// swapping builds (same convention as g_TextSectionCacheEnabled). Every uniform,
// attribute pointer and bind is re-issued per draw, and the whole cached-state
// shadow is invalidated afterwards.
static void DrawSkinnedMeshUncached(const void* vertices, int vertexCount,
                                    const uint16_t* indices, int indexCount,
                                    unsigned int* vboInOut, unsigned int* eboInOut,
                                    const float mvp[16], const float lightDir[3],
                                    const float bodyLight[3], float alpha,
                                    GLuint textureId, const GLSkinDrawState& state) {
    UseProgramCached(s_skinProg);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, s_skinBoneUbo);

    glUniformMatrix4fv(0, 1, GL_FALSE, mvp);   // mat4 occupies locations 0-3
    glUniform3fv(4, 1, lightDir);
    glUniform1i(5, state.useVertexLight ? 1 : 0);
    glUniform2f(6, state.texOffsetU, state.texOffsetV);
    glUniform1i(7, state.chromeMode ? 1 : 0);
    glUniform3fv(9, 1, bodyLight);
    glUniform1f(10, alpha);
    glUniform1i(11, state.useTexture ? 1 : 0);
    glUniform3fv(12, 1, state.glowColor);
    glUniform1i(15, state.hasChromeOverlay ? 1 : 0);
    if (state.hasChromeOverlay) {
        glUniform3fv(14, 1, state.chromeBodyLight);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, state.chromeTextureId);
        glUniform1i(13, 1);
    }
    glUniform1i(18, state.hasOverlay2 ? 1 : 0);
    if (state.hasOverlay2) {
        glUniform3fv(17, 1, state.overlay2BodyLight);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, state.overlay2TextureId);
        glUniform1i(16, 2);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glUniform1i(8, 0);

    const bool firstUpload = (*vboInOut == 0);
    if (firstUpload) {
        glGenBuffers(1, vboInOut);
        glBindBuffer(GL_ARRAY_BUFFER, *vboInOut);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertexCount) * sizeof(SkinVertex),
                     vertices, GL_STATIC_DRAW);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, *vboInOut);
    }

    const GLsizei stride = sizeof(SkinVertex);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinVertex, restPos));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinVertex, restNormal));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinVertex, uv));
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinVertex, boneIndex));

    if (*eboInOut == 0) {
        glGenBuffers(1, eboInOut);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *eboInOut);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indexCount) * sizeof(uint16_t),
                     indices, GL_STATIC_DRAW);
    } else {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *eboInOut);
    }

    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisableVertexAttribArray(4);
    glDisableVertexAttribArray(5);
    glDisableVertexAttribArray(6);
    glDisableVertexAttribArray(7);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);

    GL_InvalidateCachedGLState();

    ++s_drawCallCount; ++s_drawSite[6];
    s_totalVertices += vertexCount;
}

void GL_DrawSkinnedMesh(const void* vertices, int vertexCount,
                        const uint16_t* indices, int indexCount,
                        unsigned int* vboInOut, unsigned int* eboInOut,
                        const float mvp[16], const float lightDir[3],
                        const float bodyLight[3], float alpha,
                        GLuint textureId, const GLSkinDrawState& state) {
    if (!GL_SkinIsReady() || !vertices || vertexCount <= 0 || !indices || indexCount <= 0 ||
        !vboInOut || !eboInOut) {
        return;
    }

    if (!s_skinStateCacheEnabled) {
        DrawSkinnedMeshUncached(vertices, vertexCount, indices, indexCount,
                               vboInOut, eboInOut, mvp, lightDir, bodyLight,
                               alpha, textureId, state);
        return;
    }

    UseProgramCached(s_skinProg);
    // Indexed binding point 0 is used by nothing else, and orphaning the UBO
    // in GL_UpdateSkinningBones keeps the same buffer name, so bind it once.
    if (!s_skinUboBound) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, s_skinBoneUbo);
        s_skinUboBound = true;
    }

    // Uniforms: only what differs from the last skinned draw. Samplers are
    // fixed at GL_SkinInit.
    SkinUniformCache& u = s_skinUniforms;
    const bool all = !u.valid;
    const int vertexLight = state.useVertexLight ? 1 : 0;
    const int chromeMode = state.chromeMode ? 1 : 0;
    const int useTexture = state.useTexture ? 1 : 0;
    const int hasChrome = state.hasChromeOverlay ? 1 : 0;
    const int hasOverlay2 = state.hasOverlay2 ? 1 : 0;
    const float texOffset[2] = { state.texOffsetU, state.texOffsetV };

    if (all || SkinFloatsDiffer(u.mvp, mvp, 16)) {
        glUniformMatrix4fv(0, 1, GL_FALSE, mvp);   // mat4 occupies locations 0-3
        memcpy(u.mvp, mvp, sizeof(u.mvp));
    }
    if (all || SkinFloatsDiffer(u.lightDir, lightDir, 3)) {
        glUniform3fv(4, 1, lightDir);
        memcpy(u.lightDir, lightDir, sizeof(u.lightDir));
    }
    if (all || u.vertexLight != vertexLight) { glUniform1i(5, vertexLight); u.vertexLight = vertexLight; }
    if (all || SkinFloatsDiffer(u.texOffset, texOffset, 2)) {
        glUniform2f(6, texOffset[0], texOffset[1]);
        memcpy(u.texOffset, texOffset, sizeof(u.texOffset));
    }
    if (all || u.chromeMode != chromeMode) { glUniform1i(7, chromeMode); u.chromeMode = chromeMode; }
    if (all || SkinFloatsDiffer(u.bodyLight, bodyLight, 3)) {
        glUniform3fv(9, 1, bodyLight);
        memcpy(u.bodyLight, bodyLight, sizeof(u.bodyLight));
    }
    if (all || u.alpha != alpha) { glUniform1f(10, alpha); u.alpha = alpha; }
    if (all || u.useTexture != useTexture) { glUniform1i(11, useTexture); u.useTexture = useTexture; }
    if (all || SkinFloatsDiffer(u.glow, state.glowColor, 3)) {
        glUniform3fv(12, 1, state.glowColor);
        memcpy(u.glow, state.glowColor, sizeof(u.glow));
    }
    if (all || u.hasChrome != hasChrome) { glUniform1i(15, hasChrome); u.hasChrome = hasChrome; }
    if (all || u.hasOverlay2 != hasOverlay2) { glUniform1i(18, hasOverlay2); u.hasOverlay2 = hasOverlay2; }
    // The overlay colours only matter while their overlay is on; an unused
    // one is not refreshed, so compare against a stale value only then.
    if (state.hasChromeOverlay) {
        if (all || SkinFloatsDiffer(u.chromeLight, state.chromeBodyLight, 3)) {
            glUniform3fv(14, 1, state.chromeBodyLight);
            memcpy(u.chromeLight, state.chromeBodyLight, sizeof(u.chromeLight));
        }
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, state.chromeTextureId);
    }
    if (state.hasOverlay2) {
        if (all || SkinFloatsDiffer(u.overlay2Light, state.overlay2BodyLight, 3)) {
            glUniform3fv(17, 1, state.overlay2BodyLight);
            memcpy(u.overlay2Light, state.overlay2BodyLight, sizeof(u.overlay2Light));
        }
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, state.overlay2TextureId);
    }
    if (all) {
        // First pass after a reset: make sure an overlay colour that was never
        // written is not mistaken for a cached one next time.
        if (!state.hasChromeOverlay) { memset(u.chromeLight, 0xFF, sizeof(u.chromeLight)); }
        if (!state.hasOverlay2) { memset(u.overlay2Light, 0xFF, sizeof(u.overlay2Light)); }
    }
    u.valid = true;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);

    // Rest-pose geometry is immutable: upload once, then only bind. Animation
    // is entirely in the bone-matrix UBO, so nothing here changes per frame.
    const bool firstUpload = (*vboInOut == 0);
    if (firstUpload) {
        glGenBuffers(1, vboInOut);
        glBindBuffer(GL_ARRAY_BUFFER, *vboInOut);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertexCount) * sizeof(SkinVertex),
                     vertices, GL_STATIC_DRAW);
        s_boundArrayBuffer = *vboInOut;
    }

    // The mesh's VAO holds its four attribute pointers and its element
    // buffer, so after the first draw this is a single bind.
    GLuint& vao = s_skinVaos[*vboInOut];
    if (vao == 0) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, *vboInOut);
        s_boundArrayBuffer = *vboInOut;

        const GLsizei stride = sizeof(SkinVertex);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinVertex, restPos));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinVertex, restNormal));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinVertex, uv));
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinVertex, boneIndex));

        // Indices live in their own immutable element buffer, uploaded once
        // with the vertices. Never pass the caller's pointer to
        // glDrawElements: mobile GLES drivers read client-side pointers
        // asynchronously (this file already documents SEGV_ACCERR from exactly
        // that on Mali - see GL_SetPreferDirectVertexArrays), so a pointer
        // into a caller-owned std::vector is a use-after-free the moment that
        // cache is rebuilt or its model unloaded while a frame is in flight.
        if (*eboInOut == 0) {
            glGenBuffers(1, eboInOut);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *eboInOut);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(indexCount) * sizeof(uint16_t),
                         indices, GL_STATIC_DRAW);
        } else {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *eboInOut);
        }
    } else {
        glBindVertexArray(vao);
    }
    // Does the mesh ahead use a different texture than the one before it?
    // This decides whether the 689 skinned draws a frame can be merged at all:
    // the meshes of one character share a skeleton, so texture is the only
    // thing forcing them apart. Switches far below the draw count means
    // consecutive meshes share art and merging is mostly free; switches equal
    // to the draw count means every mesh has its own texture and merging needs
    // an atlas first. Slot 7 of s_drawSite is otherwise unused.
    {
        static GLuint s_lastSkinTex = 0xFFFFFFFFu;
        if (s_boundTexture != s_lastSkinTex) {
            ++s_drawSite[7];
            s_lastSkinTex = s_boundTexture;
        }

        // Distinct-mesh counting lived here and has been removed: it cost a
        // hash insert per skinned draw, ~600 a frame, and it has already
        // answered its question. 662 draws over 182 distinct meshes with 16
        // characters on screen = 3.64 passes per mesh and 11.4 meshes per
        // character. Restore it with an unordered_set keyed on vboInOut if the
        // pass count ever needs re-measuring.
    }

    // Measured, do not "fix" the winding here: flipping the front face for
    // skinned draws turns characters dark (their back faces show) and does
    // NOT restore the Totem Golem's missing stone-body mesh. So skinned
    // geometry reaches the rasteriser wound correctly, and whatever drops that
    // body mesh is on a different path - it is not this one.
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, nullptr);

    // Back to VAO 0, which every other gl_compat path uses - its attribute
    // arrays and element buffer are exactly as they were before this draw.
    glBindVertexArray(0);

    // Only the texture bindings moved outside gl_compat's tracking. This used
    // to be a full GL_InvalidateCachedGLState(), which also forgot the cap,
    // blend, depth and colour-mask state this draw never touches, so the next
    // ordinary draw after every character mesh re-sent all of it. The program
    // went through UseProgramCached and the array buffer was recorded above.
    s_boundTexture = 0;
    CachTexture = 0x7FFFFFFF; // ZzzOpenglUtil's shadow - see GL_InvalidateCachedGLState

    ++s_drawCallCount; ++s_drawSite[6];
    s_totalVertices += vertexCount;
}

#endif // __ANDROID__

// TEMP diagnostic: reports gl_compat's own blend/alpha-test state alongside
// ZzzOpenglUtil.cpp's separate shadow flags. Those two must agree: the engine
// guards its glEnable(GL_ALPHA_TEST)/glEnable(GL_BLEND) calls behind its own
// shadows, so if a shadow says "already enabled" while gl_compat's state says
// otherwise, the real glEnable is never re-issued, s_alphaTestEnabled stays
// false, ApplyShaderStateCommon picks the no-discard program, and alpha-cutout
// foliage renders as opaque quads (the "tree leaves are rectangles" symptom).
void GL_DebugReportAlphaState(char* out, int outSize)
{
    extern bool TextureEnable;
    extern bool AlphaTestEnable;
    extern int  AlphaBlendType;
    snprintf(out, (size_t)outSize,
        "glc[alphaTest=%d blend=%d tex=%d] engine[AlphaTestEnable=%d AlphaBlendType=%d TextureEnable=%d] "
        "earlyZ[opaque=%d discard=%d]",
        s_alphaTestEnabled ? 1 : 0,
        (s_capBits & CAP_BLEND) != 0 ? 1 : 0,
        s_texture2DEnabled ? 1 : 0,
        AlphaTestEnable ? 1 : 0, AlphaBlendType, TextureEnable ? 1 : 0,
        g_ProfEarlyZOpaqueDraws, g_ProfEarlyZDiscardDraws);
    g_ProfEarlyZOpaqueDraws = 0;
    g_ProfEarlyZDiscardDraws = 0;
}
