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

// TEMP measurement build: render the whole frame (3D + UI together - no
// separate UI pass yet, so UI will look soft at scale<1) into an offscreen
// FBO at kRenderScale x native resolution, then upscale it to the real
// backbuffer. This exists to answer "are we GPU fillrate/overdraw-bound or
// CPU-bound" per the classic "halve the resolution and compare FPS" test,
// before committing to the much larger GPU-skinning rewrite. Set to 1.0 to
// effectively disable (falls back to direct native rendering).
static constexpr float kRenderScale = 1.0f;

// Minimal blit shader, deliberately using attribute/uniform locations gl_compat.cpp
// never uses (0-2 are taken by a_pos/a_color/a_uv there) so this program's state
// never aliases gl_compat's own attribute setup.
static const char* s_blitVertSrc = R"(#version 310 es
layout(location = 10) in vec2 a_pos;
layout(location = 11) in vec2 a_uv;
out vec2 v_uv;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
)";
static const char* s_blitFragSrc = R"(#version 310 es
precision mediump float;
layout(location = 0) uniform sampler2D u_tex;
in vec2 v_uv;
out vec4 outColor;
void main() {
    outColor = texture(u_tex, v_uv);
}
)";

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
        m_nativeWidth = drawableWidth;
        m_nativeHeight = drawableHeight;

        if (SetupRenderScaleFBO(drawableWidth, drawableHeight))
        {
            glViewport(0, 0, m_fboWidth, m_fboHeight);
            RENDER_LOGI(
                "Render-scale FBO active: %dx%d -> %dx%d (scale=%.2f) [TEMP measurement build]",
                m_fboWidth, m_fboHeight, drawableWidth, drawableHeight, kRenderScale);
        }
        else
        {
            glViewport(0, 0, drawableWidth, drawableHeight);
        }
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        RENDER_LOGI("OpenGLCompat initialized (drawable=%dx%d)", drawableWidth, drawableHeight);
        return true;
    }

    void OnDrawableSizeChanged(int drawableWidth, int drawableHeight) override
    {
        m_nativeWidth = drawableWidth;
        m_nativeHeight = drawableHeight;
        if (SetupRenderScaleFBO(drawableWidth, drawableHeight))
        {
            glViewport(0, 0, m_fboWidth, m_fboHeight);
        }
        else
        {
            glViewport(0, 0, drawableWidth, drawableHeight);
        }
    }

    void Present() override
    {
        GL_FlushPending();

        if (m_fbo != 0)
        {
            BlitFboToScreen();
        }
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
        GL_ResetDrawStats();
        return stats;
    }

    void Shutdown() override
    {
        if (m_blitProg) { glDeleteProgram(m_blitProg); m_blitProg = 0; }
        if (m_blitVbo)  { glDeleteBuffers(1, &m_blitVbo); m_blitVbo = 0; }
        DestroyFBO();
        GL_Compat_Shutdown();
    }

private:
    int m_nativeWidth = 0;
    int m_nativeHeight = 0;
    int m_fboWidth = 0;
    int m_fboHeight = 0;
    GLuint m_fbo = 0;
    GLuint m_colorTex = 0;
    GLuint m_depthRbo = 0;
    GLuint m_blitProg = 0;
    GLuint m_blitVbo = 0;

    void DestroyFBO()
    {
        if (m_fbo)      { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
        if (m_colorTex) { glDeleteTextures(1, &m_colorTex); m_colorTex = 0; }
        if (m_depthRbo) { glDeleteRenderbuffers(1, &m_depthRbo); m_depthRbo = 0; }
        m_fboWidth = 0;
        m_fboHeight = 0;
    }

    // Returns true and leaves the FBO bound as the current draw target on
    // success. Returns false (and leaves framebuffer 0 bound) if scale is
    // disabled or the FBO couldn't be created - callers must fall back to
    // rendering straight to the backbuffer at native resolution.
    bool SetupRenderScaleFBO(int drawableWidth, int drawableHeight)
    {
        if (kRenderScale >= 1.0f || drawableWidth <= 0 || drawableHeight <= 0)
        {
            DestroyFBO();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }

        // Not std::max: PlatformDefs.h #defines a textual `max` macro (Windows.h-style)
        // that clobbers std::max here, same reason android_main.cpp #undefs it up top.
        const int scaledW = static_cast<int>(drawableWidth * kRenderScale);
        const int scaledH = static_cast<int>(drawableHeight * kRenderScale);
        const int w = (scaledW > 1) ? scaledW : 1;
        const int h = (scaledH > 1) ? scaledH : 1;
        if (m_fbo != 0 && w == m_fboWidth && h == m_fboHeight)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
            return true;
        }

        DestroyFBO();

        glGenTextures(1, &m_colorTex);
        glBindTexture(GL_TEXTURE_2D, m_colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenRenderbuffers(1, &m_depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            RENDER_LOGW("Render-scale FBO incomplete (status=0x%x) - falling back to native resolution", status);
            DestroyFBO();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            GL_InvalidateCachedGLState();
            return false;
        }

        if (!EnsureBlitResources())
        {
            RENDER_LOGW("Render-scale blit shader failed to compile - falling back to native resolution");
            DestroyFBO();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            GL_InvalidateCachedGLState();
            return false;
        }

        m_fboWidth = w;
        m_fboHeight = h;
        GL_InvalidateCachedGLState();
        return true;
    }

    bool EnsureBlitResources()
    {
        if (m_blitProg != 0)
        {
            return true;
        }

        const GLuint vs = CompileBlitShader(GL_VERTEX_SHADER, s_blitVertSrc);
        const GLuint fs = CompileBlitShader(GL_FRAGMENT_SHADER, s_blitFragSrc);
        if (!vs || !fs)
        {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return false;
        }

        m_blitProg = glCreateProgram();
        glAttachShader(m_blitProg, vs);
        glAttachShader(m_blitProg, fs);
        glLinkProgram(m_blitProg);
        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint linked = 0;
        glGetProgramiv(m_blitProg, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            char buf[512];
            glGetProgramInfoLog(m_blitProg, sizeof(buf), nullptr, buf);
            RENDER_LOGW("Blit program link error: %s", buf);
            glDeleteProgram(m_blitProg);
            m_blitProg = 0;
            return false;
        }

        // Fullscreen quad in NDC, triangle strip: pos.xy, uv.xy per vertex.
        static constexpr float kQuad[] = {
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f,  1.0f,  1.0f, 1.0f,
        };
        glGenBuffers(1, &m_blitVbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_blitVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
        return true;
    }

    static GLuint CompileBlitShader(GLenum type, const char* src)
    {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char buf[512];
            glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
            RENDER_LOGW("Blit shader compile error: %s", buf);
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    void BlitFboToScreen()
    {
        // Everything below is raw GL, deliberately bypassing gl_compat.cpp's
        // cached-state tracking (different program/attrib locations, own
        // VBO) - GL_InvalidateCachedGLState() at the end tells gl_compat to
        // re-sync instead of trusting stale cached values next frame.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_nativeWidth, m_nativeHeight);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        glUseProgram(m_blitProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_colorTex);
        glUniform1i(0, 0); // u_tex explicit location 0 -> texture unit 0

        glBindBuffer(GL_ARRAY_BUFFER, m_blitVbo);
        glEnableVertexAttribArray(10);
        glVertexAttribPointer(10, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
        glEnableVertexAttribArray(11);
        glVertexAttribPointer(11, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableVertexAttribArray(10);
        glDisableVertexAttribArray(11);
        glUseProgram(0);

        // Rebind the scaled FBO so next frame's glClear()/draws target it
        // again, and restore the depth-test-on/blend-off default the game
        // expects for 3D rendering to start a frame with.
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_fboWidth, m_fboHeight);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

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
