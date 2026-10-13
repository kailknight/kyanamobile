// Debug aid: describes the game window and its OpenGL context on the log,
// called from MU_MacScreenshotPoll when a screenshot is requested.

#import <AppKit/AppKit.h>
#include <dlfcn.h>
#include <vector>
#define GL_SILENCE_DEPRECATION 1
#import <OpenGL/gl3.h>

void MU_MacDiagnostics()
{
    @autoreleasepool {
        NSOpenGLContext* current = [NSOpenGLContext currentContext];
        fprintf(stderr, "I/MuDiag: GL_RENDERER=%s VERSION=%s currentContext=%p\n",
                (const char*)glGetString(GL_RENDERER), (const char*)glGetString(GL_VERSION), (__bridge void*)current);
        for (NSWindow* w in NSApp.windows)
        {
            NSView* view = w.contentView;
            NSOpenGLContext* ctx = [view isKindOfClass:[NSOpenGLView class]] ? ((NSOpenGLView*)view).openGLContext : nil;
            GLint swap = -1;
            if (ctx) { [ctx getValues:&swap forParameter:NSOpenGLContextParameterSwapInterval]; }
            fprintf(stderr,
                "I/MuDiag: window %p '%s' frame=%.0fx%.0f visible=%d occluded=%d mini=%d alpha=%.2f level=%ld parent=%p kids=%lu\n"
                "I/MuDiag:   contentView=%s layer=%p wantsLayer=%d opaque=%d backingScale=%.1f ctx=%p ctx.view==view=%d isCurrent=%d swap=%d\n",
                (__bridge void*)w, w.title.UTF8String ?: "", w.frame.size.width, w.frame.size.height,
                w.isVisible, !(w.occlusionState & NSWindowOcclusionStateVisible), w.isMiniaturized, w.alphaValue,
                (long)w.level, (__bridge void*)w.parentWindow, (unsigned long)w.childWindows.count,
                NSStringFromClass([view class]).UTF8String, (__bridge void*)view.layer, view.wantsLayer, view.isOpaque,
                w.backingScaleFactor, (__bridge void*)ctx, ctx ? (ctx.view == view) : -1, ctx ? (ctx == current) : -1, swap);
        }
    }

    // What the window server actually composited for the game window: the one
    // view of "what the player sees" this process can get without the Screen
    // Recording permission, since it is the process's own window. The symbol
    // is looked up at runtime because newer SDKs hide it.
    @autoreleasepool {
        typedef CGImageRef (*CreateImageFn)(CGRect, CGWindowListOption, CGWindowID, CGWindowImageOption);
        CreateImageFn createImage = (CreateImageFn)dlsym(RTLD_DEFAULT, "CGWindowListCreateImage");
        NSWindow* game = nil;
        for (NSWindow* w in NSApp.windows)
        {
            if ([w.contentView isKindOfClass:[NSOpenGLView class]]) { game = w; }
        }
        if (createImage == nullptr || game == nil)
        {
            fprintf(stderr, "I/MuDiag: composited capture unavailable (symbol=%p window=%p)\n", (void*)createImage, (__bridge void*)game);
            return;
        }
        CGImageRef image = createImage(CGRectNull, kCGWindowListOptionIncludingWindow, (CGWindowID)game.windowNumber,
                                       kCGWindowImageBoundsIgnoreFraming);
        if (image == nullptr)
        {
            fprintf(stderr, "I/MuDiag: composited capture returned no image\n");
            return;
        }
        const size_t w = CGImageGetWidth(image), h = CGImageGetHeight(image);
        std::vector<unsigned char> px(w * h * 4);
        CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
        CGContextRef bitmap = CGBitmapContextCreate(px.data(), w, h, 8, w * 4, rgb, kCGImageAlphaPremultipliedLast);
        CGContextDrawImage(bitmap, CGRectMake(0, 0, w, h), image);
        size_t lit = 0;
        for (size_t i = 0; i < px.size(); i += 4) { lit += (px[i] | px[i + 1] | px[i + 2]) != 0; }
        fprintf(stderr, "I/MuDiag: composited window image %zux%zu, %zu pixels not black\n", w, h, lit);
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:image];
        [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:@"mu_window.png" atomically:YES];
        CGContextRelease(bitmap);
        CGColorSpaceRelease(rgb);
        CGImageRelease(image);
    }
}
