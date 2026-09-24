#pragma once
// =============================================================================
// Platform/gl_compat.h
// OpenGL fixed-function emulation on top of OpenGL ES 3.2 for Android.
// Provides: immediate mode (glBegin/glEnd), matrix stack, color state.
// Linked via gl_compat.cpp — NOT header-only.
// =============================================================================

#if defined(__ANDROID__) || defined(MU_IOS)

#include <GLES3/gl32.h>
#include <stdint.h>
#include <vector>

// ── Must call once after GL context is created ─────────────────────────────
void GL_Compat_Init();
void GL_Compat_Shutdown();

// ── glBindTexture wrapper — tracks currently bound texture for shader ───────
void GL_TrackBindTexture(GLenum target, GLuint texture);
// Upload wrappers — must flush pending batch before mutating texture content,
// otherwise batched UI quads end up sampling the post-upload pixels (bug:
// all font glyphs rendered as the last-uploaded string).
void GL_TexImage2D_Compat(GLenum target, GLint level, GLint internalformat,
                          GLsizei width, GLsizei height, GLint border,
                          GLenum format, GLenum type, const void* pixels);
void GL_TexSubImage2D_Compat(GLenum target, GLint level,
                             GLint xoffset, GLint yoffset,
                             GLsizei width, GLsizei height,
                             GLenum format, GLenum type, const void* pixels);
void GL_BlendFunc_Compat(GLenum sfactor, GLenum dfactor);
void GL_DepthFunc_Compat(GLenum func);
void GL_DepthMask_Compat(GLboolean flag);
void GL_ColorMask_Compat(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void GL_ClearColor_Compat(float red, float green, float blue, float alpha);
void GL_Clear_Compat(GLbitfield mask);
void GL_FlushPending();
// Call after external code makes raw GL calls (program/texture/buffer binds,
// glEnable/glDisable) that bypass this file's cached-state tracking - e.g.
// a render-to-texture blit pass done outside gl_compat.cpp. Forces every
// subsequent gl_compat call to re-sync real GL state instead of trusting
// its now-stale cache.
void GL_InvalidateCachedGLState();

// ── Matrix stack ───────────────────────────────────────────────────────────
void GL_MatrixMode(GLenum mode);         // GL_MODELVIEW or GL_PROJECTION
void GL_LoadIdentity();
void GL_Ortho(double l, double r, double b, double t, double n, double f);
void GL_Frustum(double l, double r, double b, double t, double n, double f);
void GL_gluPerspective(double fovy, double aspect, double zNear, double zFar);
void GL_gluLookAt(double ex,double ey,double ez,
                  double cx,double cy,double cz,
                  double ux,double uy,double uz);
void GL_gluOrtho2D(double l,double r,double b,double t);
void GL_PushMatrix();
void GL_PopMatrix();
void GL_LoadMatrixf(const float* m);
void GL_MultMatrixf(const float* m);
void GL_Translatef(float x, float y, float z);
void GL_Rotatef(float angle, float x, float y, float z);
void GL_Scalef(float x, float y, float z);
void GL_GetFloatv(GLenum pname, float* params);  // supports GL_MODELVIEW_MATRIX / GL_PROJECTION_MATRIX

// ── Immediate mode ─────────────────────────────────────────────────────────
void GL_Begin(GLenum mode);  // GL_TRIANGLES, GL_QUADS, GL_TRIANGLE_FAN, GL_LINES ...
void GL_End();
void GL_Vertex2f(float x, float y);
void GL_Vertex2i(int x, int y);
void GL_Vertex3f(float x, float y, float z);
void GL_Vertex3fv(const float* v);
void GL_TexCoord2f(float s, float t);
void GL_TexCoord2fv(const float* v);
void GL_Color3f(float r, float g, float b);
void GL_Color3fv(const float* v);
void GL_Color3ub(uint8_t r, uint8_t g, uint8_t b);
void GL_Color4f(float r, float g, float b, float a);
void GL_Color4ub(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void GL_Normal3f(float x, float y, float z);
void GL_Normal3fv(const float* v);

// ── Bulk draw (bypasses immediate mode for max throughput) ─────────────────
// Vertex layout: x,y,z, r,g,b,a, u,v  (9 floats per vertex, 4 verts per quad)
// Draws quadCount quads with indexed triangles in ONE draw call.
void GL_DrawQuadsBulk(const float* vertexData, int quadCount);
// Same but for triangles (3 verts per tri, no index buffer)
void GL_DrawTrisBulk(const float* vertexData, int triCount);

// Object-mesh material queue support. Object rendering measured ~37ms/frame in
// a decoration-heavy scene, almost all of it draw-call + buffer-upload overhead
// from ~530 draws: a mesh's texture differs from the previous mesh's, cutting
// the pending batch, so nearly every mesh became its own draw. Those cuts are
// WITHIN each object (its meshes use different textures), so the draws have to
// be regrouped by material. The buckets and GL state live in ZzzBMD.cpp - the
// same split ZzzLodTerrain.cpp's TerrainBatch uses - and these two entry points
// supply the parts that need gl_compat internals (the modelview stack, and a
// draw that does not re-apply it).
int  GL_AppendMeshVertsBaked(std::vector<float>& out,
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
                             float texOffsetV);
void GL_DrawTrisBulkBaked(const float* vertexData, int triCount);
void GL_DebugReportAlphaState(char* out, int outSize);

// ── Debug stats ───────────────────────────────────────────────────────────
void GL_GetDrawStats(int* drawCalls, int* vertices);  // returns counts since last reset
void GL_ResetDrawStats();
void GL_GetDrawPathStats(int* imDrawCalls,
                         int* vaDirectDrawCalls,
                         int* vaConvertedDrawCalls,
                         int* quadIndexedDrawCalls,
                         int* quadExpandedDrawCalls);

// TEMP profiling: how many times the pending immediate batch was cut short, by
// cause. Order matches GLFlushCause in gl_compat.cpp: other, texbind, texup,
// blend, depth, enable, alpharef, proj, frame.
#define GL_FLUSH_CAUSE_COUNT 12
void GL_GetFlushCauseStats(int* out, int count);
void GL_GetDrawSiteStats(int* out, int count);

// Runtime toggle for legacy VA draw path:
// true  -> prefer direct client-array path when compatible
// false -> force converted/batched VBO upload path
void GL_SetPreferDirectVertexArrays(bool preferDirect);
bool GL_GetPreferDirectVertexArrays();

// Skip VBO buffer-orphaning (glBufferData nullptr) on software renderers like
// SwiftShader where there is no real GPU pipeline stall to avoid.
// Eliminates ~105 unnecessary driver-level malloc calls per frame on emulator.
void GL_SetSkipVBOOrphan(bool skip);

// A/B switch for the skinned-mesh redundant-state elimination (cached uniforms,
// per-mesh VAOs, bone upload skipped when unchanged). false = the older path
// that re-issues everything per draw. Measurement aid; default true.
void GL_SetSkinStateCache(bool enabled);
bool GL_GetSkinStateCache();

// True when the persistent-mapped streaming ring is in use (needs
// GL_EXT_buffer_storage + ES 3.2 base-vertex draws). False means every
// streamed draw takes the per-draw glBufferData orphan (or client arrays).
bool GL_IsStreamRingActive();

// TEMP profiling: reads GL_DEPTH_BITS / GL_STENCIL_BITS for whichever
// framebuffer is currently bound, so the real depth-stencil format can be
// confirmed rather than assumed.
void GL_SampleDepthStencilFormat();

// ── Enable/Disable for fixed-function states ───────────────────────────────
void GL_Enable_Compat(GLenum cap);
void GL_Disable_Compat(GLenum cap);

// ── Alpha test emulation (done in shader) ─────────────────────────────────
void GL_AlphaFunc(GLenum func, float ref);

// ── Fog emulation (done in shader) ────────────────────────────────────────
void GL_Fogf(GLenum pname, float param);
void GL_Fogi(GLenum pname, int param);
void GL_Fogfv(GLenum pname, const float* params);

// ── Lighting (stub — complex shading not implemented) ─────────────────────
void GL_Lightfv(GLenum light, GLenum pname, const float* params);

// ── Vertex arrays (fixed-function legacy) ─────────────────────────────────
void GL_EnableClientState(GLenum array);
void GL_DisableClientState(GLenum array);
void GL_VertexPointer(int size, GLenum type, int stride, const void* ptr);
void GL_TexCoordPointer(int size, GLenum type, int stride, const void* ptr);
void GL_ColorPointer(int size, GLenum type, int stride, const void* ptr);
void GL_NormalPointer(GLenum type, int stride, const void* ptr);
void GL_DrawArrays_Compat(GLenum mode, int first, int count);

// ── Direct batch append (high-perf path for BMD mesh rendering) ───────────
// Appends triangles DIRECTLY into the pending vertex batch, bypassing the
// intermediate s_arrayVerts copy step in GL_DrawArrays_Compat.
// This is a single-pass combine of the RenderMesh arrays into IMVertex format
// with ModelView baked in — eliminates one full memory-copy pass per mesh.
//
// positions : numVerts × 3 floats — world-space XYZ (ModelView will be baked)
// colors    : numVerts × 4 floats — RGBA, or nullptr to use current GL color
// texcoords : numVerts × 2 floats — UV, or nullptr for untextured color draws
// numVerts  : total vertex count (must be a multiple of 3 for GL_TRIANGLES)
void GL_BatchAppendTriangles(const float* positions,
                             const float* colors,
                             const float* texcoords,
                             int numVerts);
void GL_BatchAppendTrianglesConstColor(const float* positions,
                                       const float* texcoords,
                                       int numVerts,
                                       const float color[4]);
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
                                          float texOffsetV);
void GL_BatchAppendIndexedTrianglesConstColor(const float* positions3,
                                              const float* texcoords2,
                                              const short* vertexIndexBase,
                                              const short* texCoordIndexBase,
                                              int triangleStrideBytes,
                                              int triangleCount,
                                              const float color[4],
                                              float texOffsetU,
                                              float texOffsetV);

// ── GPU skinning (additive, not wired into any character's render path
//    yet - see the "GPU skinning" section in gl_compat.cpp) ────────────────
// Rest-pose (bone-local, not transformed) vertex layout for GL_DrawSkinnedMesh.
// Callers building this data (e.g. a future BMD model-load step) must match
// this layout exactly - it's uploaded to the GPU as raw bytes.
struct GLSkinVertex {
    float restPos[3];
    float restNormal[3];
    float uv[2];
    float boneIndex; // float, not int - matches the shader's vertex attrib type
};

// Current Projection * View (does NOT include any per-object model
// transform - the CPU vertex path bakes object position/scale directly
// into vertex positions instead of using a model matrix, so callers
// building their own model matrix for GL_DrawSkinnedMesh's mvp must
// multiply it onto this themselves).
void GL_GetCurrentMVP(float mvp[16]);
bool GL_SkinIsReady();
// boneMatrix3x4: BMD::Animation's output format, float[boneCount][3][4].
void GL_UpdateSkinningBones(const float boneMatrix3x4[][3][4], int boneCount);
// vertices: GLSkinVertex[vertexCount]
// mvp: object world transform composed with view*projection (16 floats, column-major).
//
// vboInOut/eboInOut: caller-owned handles, zero-initialised. The rest-pose
// vertices and indices never change once built, so they are uploaded to GL
// ONCE on the first draw and reused every frame after. Re-uploading them per
// draw (the previous behaviour) cost ~6.7MB of CPU->GPU traffic per frame
// across ~300 skinned meshes while the GPU sat idle.
// Call GL_DeleteSkinnedMeshBuffers when the mesh is released.
// Per-draw shader state, so passes that used to fall back to the CPU path
// (unlit/const-colour, untextured colour, chrome env-mapping, wave scroll)
// can run on the GPU too. Defaults describe the plain lit+textured pass.
struct GLSkinDrawState {
    bool  useTexture = true;      // false -> flat colour, no texture fetch
    bool  useVertexLight = true;  // false -> flat u_bodyLight, no per-vertex lighting
    bool  chromeMode = false;     // true  -> env-map uv from the skinned normal
    float texOffsetU = 0.0f;      // wave/scroll offset
    float texOffsetV = 0.0f;
    // Additive "excellent item" glow tint, folded into the same draw instead
    // of a separate second RENDER_TEXTURE|RENDER_BRIGHT pass over the same
    // mesh (see RenderPartObjectEffect's pending-glow mechanism, ZzzBMD.cpp).
    // (0,0,0) = no glow, matching every existing caller's default.
    float glowColor[3] = {0.0f, 0.0f, 0.0f};
    // Additive chrome/metal overlay, folded into the same draw instead of a
    // separate second RENDER_CHROME|RENDER_BRIGHT pass (mesh-pass collapse,
    // Phase B - see g_PendingChromeActive in ZzzBMD.cpp). hasChromeOverlay
    // false (the default) costs nothing extra - the fragment shader skips
    // the second texture fetch entirely.
    bool  hasChromeOverlay = false;
    GLuint chromeTextureId = 0;
    float chromeBodyLight[3] = {0.0f, 0.0f, 0.0f};
    // Second additive overlay slot, same idea. The tiers that stack overlays
    // pair CHROME with METAL (+9 and up), and at the mobile RenderLevel
    // default of 2 every high-tier item clamps into exactly that 2-overlay
    // branch - so two slots collapse the whole sequence into one draw there.
    // Both slots sample at the same env-mapped UV (v_chromeUv): CHROME and
    // METAL both resolve to Render==RENDER_CHROME, and the GPU path already
    // approximates all chrome variants with one generic env-map formula.
    // A third overlay, or a GPU-ineligible variant (CHROME4/OIL), falls back
    // to a separate follow-up draw instead of occupying a slot.
    bool  hasOverlay2 = false;
    GLuint overlay2TextureId = 0;
    float overlay2BodyLight[3] = {0.0f, 0.0f, 0.0f};
};

void GL_DrawSkinnedMesh(const void* vertices, int vertexCount,
                        const uint16_t* indices, int indexCount,
                        unsigned int* vboInOut, unsigned int* eboInOut,
                        const float mvp[16], const float lightDir[3],
                        const float bodyLight[3], float alpha,
                        GLuint textureId, const GLSkinDrawState& state);
void GL_DeleteSkinnedMeshBuffers(unsigned int* vboInOut, unsigned int* eboInOut);

#endif // __ANDROID__
