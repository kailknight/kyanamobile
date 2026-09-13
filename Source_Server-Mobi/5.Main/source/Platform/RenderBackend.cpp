#if defined(__ANDROID__) || defined(MU_IOS)

#include "stdafx.h"

#include "Platform/MobilePlatform.h"
#include "Platform/RenderBackend.h"

#include "Platform/gl_compat.h"

#include <GLES3/gl32.h>
#include <android/log.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace
{
#define RENDER_LOG_TAG "MuRender"
#if defined(MU_ANDROID_DISABLE_LOG)
#define RENDER_LOGI(...) ((void)0)
#define RENDER_LOGW(...) ((void)0)
#else
#define RENDER_LOGI(...) __android_log_print(ANDROID_LOG_INFO, RENDER_LOG_TAG, __VA_ARGS__)
#define RENDER_LOGW(...) __android_log_print(ANDROID_LOG_WARN, RENDER_LOG_TAG, __VA_ARGS__)
#endif

// ── Render scale ──────────────────────────────────────────────────────────
// The engine renders into an offscreen FBO sized to the drawable size it was
// given (android_main.cpp shrinks that by g_RenderScaleX/Y), and Present()
// blits that FBO up to the physical surface with GL_LINEAR so it fills the
// whole screen.
//
// glBlitFramebuffer is used rather than a fullscreen shader pass: it is a
// single driver-side scaled copy needing no program/VAO/attrib state, so it
// cannot disturb gl_compat's cached GL state the way a shader pass would.
//
// Set by android_main.cpp once sokol reports the physical surface size.
static int s_nativePresentWidth = 0;
static int s_nativePresentHeight = 0;

class OpenGLCompatBackend final : public IRenderBackend
{
public:
    const char* GetName() const override
    {
        return "OpenGLCompat";
    }

    RenderBackendType GetType() const override
    {
        return RenderBackendType::OpenGLCompat;
    }

    bool Initialize(int drawableWidth, int drawableHeight) override
    {
        GL_Compat_Init();
        SetupRenderTarget(drawableWidth, drawableHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RENDER_LOGI("OpenGLCompat initialized (render=%dx%d)", drawableWidth, drawableHeight);
        return true;
    }

    void OnDrawableSizeChanged(int drawableWidth, int drawableHeight) override
    {
        SetupRenderTarget(drawableWidth, drawableHeight);
    }

    void Present() override
    {
        GL_FlushPending();
        BlitToScreen();
    }

    RenderBackendStats GetAndResetStats() override
    {
        RenderBackendStats stats {};
        GL_GetDrawStats(&stats.drawCalls, &stats.vertices);
        GL_GetDrawPathStats(&stats.imDrawCalls,
                            &stats.vaDirectDrawCalls,
                            &stats.vaConvertedDrawCalls,
                            &stats.quadIndexedDrawCalls,
                            &stats.quadExpandedDrawCalls);
        GL_GetFlushCauseStats(stats.flushCauses, 12);
        GL_GetDrawSiteStats(stats.drawSites, 10);
        GL_ResetDrawStats();
        return stats;
    }
    void Shutdown() override
    {
        DestroyFBO();
        GL_Compat_Shutdown();
    }

private:
    int m_renderWidth = 0;
    int m_renderHeight = 0;
    GLuint m_fbo = 0;
    GLuint m_colorTex = 0;
    GLuint m_depthRbo = 0;

    void DestroyFBO()
    {
        if (m_fbo)      { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
        if (m_colorTex) { glDeleteTextures(1, &m_colorTex); m_colorTex = 0; }
        if (m_depthRbo) { glDeleteRenderbuffers(1, &m_depthRbo); m_depthRbo = 0; }
        m_renderWidth = 0;
        m_renderHeight = 0;
    }

    // Creates/resizes the offscreen target and leaves it bound with a matching
    // viewport. If the physical size equals the render size (scale == 1) or the
    // FBO cannot be created, falls back to drawing straight to the backbuffer -
    // a failure here costs sharpness/perf, never a black screen.
    void SetupRenderTarget(int width, int height)
    {
        if (width <= 0 || height <= 0)
        {
            return;
        }

        const bool scaling =
            (s_nativePresentWidth > 0 && s_nativePresentHeight > 0) &&
            (s_nativePresentWidth != width || s_nativePresentHeight != height);

        if (!scaling)
        {
            DestroyFBO();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            GL_InvalidateCachedGLState();
            return;
        }

        if (m_fbo != 0 && width == m_renderWidth && height == m_renderHeight)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
            glViewport(0, 0, m_renderWidth, m_renderHeight);
            return;
        }

        DestroyFBO();

        glGenTextures(1, &m_colorTex);
        glBindTexture(GL_TEXTURE_2D, m_colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenRenderbuffers(1, &m_depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            RENDER_LOGW("Render-scale FBO incomplete (0x%x) - rendering at native resolution", status);
            DestroyFBO();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            GL_InvalidateCachedGLState();
            return;
        }

        m_renderWidth = width;
        m_renderHeight = height;
        glViewport(0, 0, m_renderWidth, m_renderHeight);
        GL_InvalidateCachedGLState();
        RENDER_LOGI("Render scale active: %dx%d -> %dx%d",
            m_renderWidth, m_renderHeight, s_nativePresentWidth, s_nativePresentHeight);
    }

    void BlitToScreen()
    {
        if (m_fbo == 0)
        {
            return; // Already rendering straight to the backbuffer.
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        // Scissor would clip the blit.
        glDisable(GL_SCISSOR_TEST);
        glBlitFramebuffer(
            0, 0, m_renderWidth, m_renderHeight,
            0, 0, s_nativePresentWidth, s_nativePresentHeight,
            GL_COLOR_BUFFER_BIT, GL_LINEAR);

        // Re-bind the offscreen target so the next frame's clear/draws land in
        // it rather than on the backbuffer.
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_renderWidth, m_renderHeight);

        // Raw GL above bypassed gl_compat's cached state tracking.
        GL_InvalidateCachedGLState();
    }
};

class BgfxBackend final : public IRenderBackend
{
public:
    const char* GetName() const override
    {
        return "BGFX";
    }

    RenderBackendType GetType() const override
    {
        return RenderBackendType::Bgfx;
    }

    bool Initialize(int /*drawableWidth*/, int /*drawableHeight*/) override
    {
        const void* nativeWindow = MU_MobileGetNativeWindow();
        const void* eglDisplay = MU_MobileGetEglDisplay();
        const void* eglContext = MU_MobileGetEglContext();

#if defined(MU_BGFX_SUPPORT)
        RENDER_LOGI(
            "BGFX support compiled in (nwh=%p display=%p context=%p)",
            nativeWindow,
            eglDisplay,
            eglContext);
        RENDER_LOGW(
            "BGFX runtime port is still blocked: renderer code still emits legacy gl_compat/raw-GL calls, so fallback is required until the draw path is ported");
#else
        RENDER_LOGW(
            "BGFX backend requested but MU_ENABLE_BGFX is OFF (nwh=%p display=%p context=%p); fallback is required",
            nativeWindow,
            eglDisplay,
            eglContext);
#endif
        return false;
    }

    void OnDrawableSizeChanged(int /*drawableWidth*/, int /*drawableHeight*/) override {}

    void Present() override {}

    RenderBackendStats GetAndResetStats() override
    {
        return {};
    }

    void Shutdown() override {}
};
} // namespace

void RenderBackend_SetNativePresentSize(int width, int height)
{
    if (width > 0 && height > 0)
    {
        s_nativePresentWidth = width;
        s_nativePresentHeight = height;
    }
}

RenderBackendType ParseRenderBackendType(const char* value)
{
    if (!value || !value[0])
    {
        return RenderBackendType::OpenGLCompat;
    }

    std::string normalized(value);
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (normalized == "bgfx")
    {
        return RenderBackendType::Bgfx;
    }

    return RenderBackendType::OpenGLCompat;
}

const char* RenderBackendTypeToString(RenderBackendType type)
{
    switch (type)
    {
    case RenderBackendType::Bgfx:
        return "bgfx";
    case RenderBackendType::OpenGLCompat:
    default:
        return "opengl";
    }
}

std::unique_ptr<IRenderBackend> CreateRenderBackend(RenderBackendType type)
{
    switch (type)
    {
    case RenderBackendType::Bgfx:
        return std::make_unique<BgfxBackend>();
    case RenderBackendType::OpenGLCompat:
    default:
        return std::make_unique<OpenGLCompatBackend>();
    }
}

#endif // __ANDROID__
