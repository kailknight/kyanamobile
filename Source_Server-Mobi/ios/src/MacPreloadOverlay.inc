// AppKit counterpart of the UIKit download screen in IosPreload.mm. Included
// only when TARGET_OS_OSX; the class name and the three methods the preload
// flow calls (setStage:detail:timer:percent:, showRetry:, dismiss) match the
// iOS one, so the networking code above and below is shared as is.
//
// The screen lives in a borderless child window laid over the game window
// rather than as a subview: the game window's content view is an OpenGL
// surface, which does not composite subviews reliably.

static NSColor* ColorFromArgb(uint32_t argb)
{
    return [NSColor colorWithSRGBRed:((argb >> 16) & 0xFF) / 255.0
                               green:((argb >> 8) & 0xFF) / 255.0
                                blue:(argb & 0xFF) / 255.0
                               alpha:((argb >> 24) & 0xFF) / 255.0];
}

static CGColorRef CGColorFromArgb(uint32_t argb)
{
    return (CGColorRef)CFAutorelease(CGColorCreateSRGB(((argb >> 16) & 0xFF) / 255.0,
                                                       ((argb >> 8) & 0xFF) / 255.0,
                                                       (argb & 0xFF) / 255.0,
                                                       ((argb >> 24) & 0xFF) / 255.0));
}

@interface MuPreloadProgressBar : NSView
@property(nonatomic) double progress;
@end

@implementation MuPreloadProgressBar
{
    CAGradientLayer* _fill;
}

- (BOOL)isFlipped { return YES; }

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]))
    {
        self.wantsLayer = YES;
        self.layer.backgroundColor = CGColorFromArgb(0x2DFFFFFF);
        self.layer.masksToBounds = YES;
        _fill = [CAGradientLayer layer];
        _fill.colors = @[ (__bridge id)CGColorFromArgb(0xFF5EC9FF), (__bridge id)CGColorFromArgb(0xFF56FFA4),
                          (__bridge id)CGColorFromArgb(0xFFFFD85B) ];
        _fill.startPoint = CGPointMake(0, 0.5);
        _fill.endPoint = CGPointMake(1, 0.5);
        [self.layer addSublayer:_fill];
    }
    return self;
}

- (void)setProgress:(double)progress
{
    _progress = std::clamp(progress, 0.0, 1.0);
    self.needsLayout = YES;
}

- (void)layout
{
    [super layout];
    const NSRect bounds = self.bounds;
    self.layer.cornerRadius = bounds.size.height * 0.5;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    _fill.frame = CGRectMake(0, 0, bounds.size.width * _progress, bounds.size.height);
    _fill.cornerRadius = bounds.size.height * 0.5;
    [CATransaction commit];
}
@end

@interface MuPreloadOverlay : NSView
- (void)setStage:(NSString*)stage detail:(NSString*)detail timer:(NSString*)timer percent:(double)percent;
- (void)showRetry:(BOOL)show;
- (void)dismiss;
@end

@implementation MuPreloadOverlay
{
    NSView* _banner;
    CAGradientLayer* _shade;
    NSView* _panel;
    NSTextField* _title;
    NSTextField* _stage;
    MuPreloadProgressBar* _progress;
    NSTextField* _detail;
    NSTextField* _timer;
    NSButton* _retry;
    NSArray* _banners;          // CGImageRef, bridged
    NSUInteger _bannerIndex;
    NSTimer* _bannerTimer;
}

- (BOOL)isFlipped { return YES; }

- (NSTextField*)addLabelWithSize:(CGFloat)size color:(uint32_t)argb bold:(BOOL)bold
{
    NSTextField* label = [NSTextField labelWithString:@""];
    label.font = bold ? [NSFont boldSystemFontOfSize:size] : [NSFont systemFontOfSize:size];
    label.textColor = ColorFromArgb(argb);
    label.lineBreakMode = NSLineBreakByTruncatingMiddle;
    [_panel addSubview:label];
    return label;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if (!(self = [super initWithFrame:frame]))
    {
        return nil;
    }

    self.wantsLayer = YES;
    self.layer.backgroundColor = CGColorFromArgb(0xFF080B14);
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    NSMutableArray* banners = [NSMutableArray array];
    for (NSString* name in @[ @"banner_preload_1", @"banner_preload_2" ])
    {
        NSString* path = [[NSBundle mainBundle] pathForResource:name ofType:@"jpg"];
        NSImage* image = path ? [[NSImage alloc] initWithContentsOfFile:path] : nil;
        CGImageRef cg = [image CGImageForProposedRect:nil context:nil hints:nil];
        if (cg)
        {
            [banners addObject:(__bridge id)cg];
        }
    }
    _banners = banners;

    _banner = [[NSView alloc] init];
    _banner.wantsLayer = YES;
    _banner.layer.contentsGravity = kCAGravityResizeAspectFill;
    _banner.layer.masksToBounds = YES;
    _banner.layer.contents = _banners.firstObject;
    [self addSubview:_banner];

    _shade = [CAGradientLayer layer];
    _shade.colors = @[ (__bridge id)CGColorFromArgb(0x22000000), (__bridge id)CGColorFromArgb(0xB8000000) ];
    // The layer's y axis points up: the first colour is the bottom edge.
    _shade.startPoint = CGPointMake(0.5, 1.0);
    _shade.endPoint = CGPointMake(0.5, 0.0);
    [self.layer addSublayer:_shade];

    _panel = [[NSView alloc] init];
    _panel.wantsLayer = YES;
    _panel.layer.backgroundColor = CGColorFromArgb(0xB21A2230);
    _panel.layer.cornerRadius = 18;
    _panel.layer.borderWidth = 1;
    _panel.layer.borderColor = CGColorFromArgb(0x52FFFFFF);
    [self addSubview:_panel];

    _title = [self addLabelWithSize:18 color:0xFFF3F8FF bold:YES];
    _title.attributedStringValue =
        [[NSAttributedString alloc] initWithString:@"MU DATA SYNC"
                                        attributes:@{ NSKernAttributeName : @1.1,
                                                      NSFontAttributeName : [NSFont boldSystemFontOfSize:18],
                                                      NSForegroundColorAttributeName : ColorFromArgb(0xFFF3F8FF) }];
    _stage = [self addLabelWithSize:14 color:0xFFDCE8FF bold:NO];
    _progress = [[MuPreloadProgressBar alloc] init];
    [_panel addSubview:_progress];
    _detail = [self addLabelWithSize:12 color:0xFFC8D6EA bold:NO];
    _timer = [self addLabelWithSize:12 color:0xFFB7C9E8 bold:NO];

    _retry = [NSButton buttonWithTitle:@"Retry" target:self action:@selector(retryTapped)];
    _retry.bezelStyle = NSBezelStyleRounded;
    _retry.hidden = YES;
    [_panel addSubview:_retry];

    _stage.stringValue = @"Preparing data sync...";
    _detail.stringValue = @"Contacting server...";

    if (_banners.count > 1)
    {
        __weak MuPreloadOverlay* weakSelf = self;
        _bannerTimer = [NSTimer scheduledTimerWithTimeInterval:kBannerSwitchSeconds
                                                       repeats:YES
                                                         block:^(NSTimer*) { [weakSelf switchBanner]; }];
    }
    return self;
}

- (void)switchBanner
{
    _bannerIndex = (_bannerIndex + 1) % _banners.count;
    _banner.layer.contents = _banners[_bannerIndex];
}

- (void)layout
{
    [super layout];
    const NSRect bounds = self.bounds;
    _banner.frame = bounds;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    _shade.frame = bounds;
    [CATransaction commit];

    const CGFloat margin = 20;
    const CGFloat padH = 18, padV = 14;
    // A wide window would stretch the panel into a bar; keep it phone-sized.
    const CGFloat width = std::min<CGFloat>(bounds.size.width - margin * 2, 560);
    const CGFloat left = margin;
    const CGFloat inner = width - padH * 2;

    CGFloat y = padV;
    _title.frame = CGRectMake(padH, y, inner, 22);   y += 22 + 8;
    _stage.frame = CGRectMake(padH, y, inner, 18);   y += 18 + 10;
    _progress.frame = CGRectMake(padH, y, inner, 16); y += 16 + 8;
    _detail.frame = CGRectMake(padH, y, inner, 16);  y += 16 + 3;
    _timer.frame = CGRectMake(padH, y, inner, 16);   y += 16;
    if (!_retry.hidden)
    {
        y += 10;
        _retry.frame = CGRectMake(padH, y, 120, 28);
        y += 28;
    }
    y += padV;

    const CGFloat bottom = bounds.size.height - margin;
    _panel.frame = CGRectMake(left, bottom - y, width, y);
}

- (void)setStage:(NSString*)stage detail:(NSString*)detail timer:(NSString*)timer percent:(double)percent
{
    if (stage) _stage.stringValue = HideAddress(stage);
    if (detail) _detail.stringValue = HideAddress(detail);
    if (timer) _timer.stringValue = HideAddress(timer);
    if (percent >= 0.0) _progress.progress = percent / 100.0;
}

- (void)showRetry:(BOOL)show
{
    _retry.hidden = !show;
    self.needsLayout = YES;
}

- (void)retryTapped
{
    [self showRetry:NO];
    MU_IosPreloadBegin();
}

- (void)dismiss
{
    [_bannerTimer invalidate];
    _bannerTimer = nil;
    NSWindow* window = self.window;
    [window.parentWindow removeChildWindow:window];
    [window orderOut:nil];
}
@end

// Shows the screen over the game window and keeps it the size of that window's
// content area. Returns the overlay view.
static NSWindow* g_overlayWindow = nil;

static MuPreloadOverlay* MuShowPreloadOverlay()
{
    NSWindow* host = NSApp.mainWindow ?: NSApp.keyWindow ?: NSApp.windows.firstObject;
    const NSRect area = host ? [host contentRectForFrameRect:host.frame] : NSMakeRect(0, 0, 1280, 720);

    NSWindow* window = [[NSWindow alloc] initWithContentRect:area
                                                   styleMask:NSWindowStyleMaskBorderless
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    window.releasedWhenClosed = NO;
    window.backgroundColor = ColorFromArgb(0xFF080B14);
    window.hasShadow = NO;

    MuPreloadOverlay* overlay = [[MuPreloadOverlay alloc] initWithFrame:NSMakeRect(0, 0, area.size.width, area.size.height)];
    window.contentView = overlay;
    g_overlayWindow = window;

    if (host)
    {
        [host addChildWindow:window ordered:NSWindowAbove];
        auto follow = ^(NSNotification*) {
            [window setFrame:[host contentRectForFrameRect:host.frame] display:YES];
        };
        [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidResizeNotification
                                                          object:host queue:[NSOperationQueue mainQueue] usingBlock:follow];
        [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidEnterFullScreenNotification
                                                          object:host queue:[NSOperationQueue mainQueue] usingBlock:follow];
        [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidExitFullScreenNotification
                                                          object:host queue:[NSOperationQueue mainQueue] usingBlock:follow];
    }
    [window makeKeyAndOrderFront:nil];
    return overlay;
}
