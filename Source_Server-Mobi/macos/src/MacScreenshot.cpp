// Debug aid for the macOS build: saves what the game just drew to a file when
// asked, so the window can be inspected without screen-recording permission.
//
// Ask by creating  <data root>/mu_shot.request  (e.g. `touch`). The next
// polled frame writes  <data root>/mu_shot.ppm  and deletes the request.
// Convert with: sips -s format png mu_shot.ppm --out mu_shot.png
//
// Kept free of the game headers, which redefine Win32 types.

#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <vector>

void MU_MacDiagnostics();
void MU_MacScreenshotPoll(int width, int height)
{
    static int s_frame = 0;
    if (++s_frame % 20 != 0 || width <= 0 || height <= 0)
    {
        return;
    }
    if (access("mu_shot.request", F_OK) != 0)
    {
        return;
    }
    unlink("mu_shot.request");
    MU_MacDiagnostics();

    // The game renders into its own offscreen target and blits that onto the
    // window's buffer in Present(), leaving the offscreen one bound for the
    // next frame. The picture is on the window's buffer (framebuffer 0).
    while (glGetError() != GL_NO_ERROR) {}
    GLint previousRead = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const GLenum error = glGetError();
    // The offscreen target as well, so a failed blit can be told from an empty
    // scene. Its size is unknown here; the window's size is read from the
    // top-left corner, which only reports pixels that were really drawn.
    if (previousRead != 0)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousRead));
        GLint attachmentWidth = 0, attachmentHeight = 0;
        GLint tex = 0;
        glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex));
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &attachmentWidth);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &attachmentHeight);
        if (attachmentWidth > 0 && attachmentHeight > 0)
        {
            std::vector<unsigned char> fbo(static_cast<size_t>(attachmentWidth) * attachmentHeight * 4);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glReadPixels(0, 0, attachmentWidth, attachmentHeight, GL_RGBA, GL_UNSIGNED_BYTE, fbo.data());
            size_t fboLit = 0;
            for (size_t i = 0; i < fbo.size(); i += 4)
            {
                fboLit += (fbo[i] | fbo[i + 1] | fbo[i + 2]) != 0;
            }
            fprintf(stderr, "I/MuMain: screenshot: offscreen %dx%d, %zu pixels not black, alpha[0]=%u\n",
                    attachmentWidth, attachmentHeight, fboLit, fbo.empty() ? 0u : fbo[3]);
        }
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousRead));

    size_t lit = 0;
    for (size_t i = 0; i < pixels.size(); i += 4)
    {
        lit += (pixels[i] | pixels[i + 1] | pixels[i + 2]) != 0;
    }
    fprintf(stderr, "I/MuMain: screenshot: %zu of %zu pixels not black\n", lit, pixels.size() / 4);

    FILE* out = fopen("mu_shot.ppm", "wb");
    if (out == nullptr)
    {
        return;
    }
    fprintf(out, "P6\n%d %d\n255\n", width, height);
    std::vector<unsigned char> row(static_cast<size_t>(width) * 3);
    for (int y = height - 1; y >= 0; --y)   // GL rows run bottom-up
    {
        const unsigned char* src = &pixels[static_cast<size_t>(y) * width * 4];
        for (int x = 0; x < width; ++x)
        {
            row[x * 3 + 0] = src[x * 4 + 0];
            row[x * 3 + 1] = src[x * 4 + 1];
            row[x * 3 + 2] = src[x * 4 + 2];
        }
        fwrite(row.data(), 1, row.size(), out);
    }
    fclose(out);
    fprintf(stderr, "I/MuMain: screenshot %dx%d written (glGetError=0x%x)\n", width, height, error);
}
