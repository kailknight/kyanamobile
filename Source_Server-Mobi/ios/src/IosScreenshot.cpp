// Debug aid for the iOS build: saves what the game just drew to a file when
// asked, so a layout change can be checked on a phone without a screenshot
// tool. The iOS twin of macos/src/MacScreenshot.cpp.
//
// Ask by putting  mu_shot.request  in the app's Documents folder:
//   xcrun devicectl device copy to --device <udid> --domain-type appDataContainer
//         --domain-identifier com.worldofkira --source <empty file>
//         --destination Documents/mu_shot.request
// The next polled frame writes  Documents/mu_shot.ppm  and deletes the request.
// Copy it back with `devicectl device copy from`, then convert with
//   sips -s format png mu_shot.ppm --out mu_shot.png
//
// Kept free of the game headers, which redefine Win32 types.

#include <OpenGLES/ES3/gl.h>

#include <cstdio>
#include <unistd.h>
#include <vector>

// width/height are the engine's render size, not the screen's. Called right
// after Present(), which leaves the offscreen target bound for the next frame,
// so this reads the picture before it is scaled up onto the screen.
void MU_IosScreenshotPoll(int width, int height)
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

    while (glGetError() != GL_NO_ERROR) {}
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const GLenum error = glGetError();

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
