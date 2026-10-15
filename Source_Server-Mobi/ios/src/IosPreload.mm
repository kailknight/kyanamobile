// First-launch data.zip download for iOS: the networking and UI half of
// android/.../PreloadActivity.java (the file-system half is IosPreloadCore.cpp).
//
// Android runs this as its own activity before the game's native activity
// starts. On iOS sokol_app owns startup, so the same screen is a UIKit overlay
// laid over the game view while the game's init waits (android_main.cpp's
// g_IosPreloadPending).

#import <TargetConditionals.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif
#import <QuartzCore/QuartzCore.h>

#include "Platform/IosPlatform.h"
#include "IosPreloadCore.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace mu_preload;

#if TARGET_OS_OSX
#define MU_FOREGROUND_NOTIFICATION NSApplicationDidBecomeActiveNotification
#else
#define MU_FOREGROUND_NOTIFICATION UIApplicationWillEnterForegroundNotification
#endif

namespace
{
// Plain HTTP on purpose, as on Android: port 8888 only serves an expired
// self-signed certificate. Info.plist carries the ATS exception.
NSString* const kDataZipUrl = @"http://139.99.24.220:8888/data.zip";
NSString* const kBasicAuthUser = @"admin";
NSString* const kBasicAuthPassword = @"openmu";

constexpr int kChunkThreads = 4;
constexpr long long kChunkBytes = 8LL * 1024 * 1024;
constexpr int kChunkAttempts = 3;
constexpr long long kParallelMinBytes = 8LL * 1024 * 1024;
constexpr double kDownloadWeight = 72.0;
constexpr double kParseWeight = 28.0;
constexpr auto kUiInterval = std::chrono::milliseconds(110);
constexpr double kBannerSwitchSeconds = 2.4;

std::atomic<bool> g_complete { false };
std::atomic<bool> g_running { false };
std::atomic<bool> g_cancel { false };
std::atomic<bool> g_awaitingRetry { false };
#if !TARGET_OS_OSX
UIBackgroundTaskIdentifier g_backgroundTask = UIBackgroundTaskInvalid;
#endif

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "I/MuPreload: ");
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
}

std::string FormatBytes(long long bytes)
{
    if (bytes < 1024)
    {
        return std::to_string(bytes) + " B";
    }
    static const char* const kUnits[] = { "KB", "MB", "GB", "TB" };
    double value = bytes / 1024.0;
    int unit = 0;
    while (value >= 1024.0 && unit < 3)
    {
        value /= 1024.0;
        ++unit;
    }
    char text[32];
    snprintf(text, sizeof(text), "%.2f %s", value, kUnits[unit]);
    return text;
}

std::string FormatDuration(long long millis)
{
    const long long total = std::max(0LL, millis / 1000);
    char text[32];
    if (total >= 3600)
    {
        snprintf(text, sizeof(text), "%lld:%02lld:%02lld", total / 3600, (total % 3600) / 60, total % 60);
    }
    else
    {
        snprintf(text, sizeof(text), "%02lld:%02lld", total / 60, total % 60);
    }
    return text;
}

long long NowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void ExcludeFromBackup(const std::string& path)
{
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
    [url setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];
}

// A network failure as text, tagged with the code ErrorCode() shows for it.
// The tag is taken from the NSError code because its description is localised.
std::string NetworkErrorText(NSError* error)
{
    const char* tag = "";
    if ([error.domain isEqualToString:NSURLErrorDomain])
    {
        switch (error.code)
        {
        case NSURLErrorCannotFindHost:
        case NSURLErrorDNSLookupFailed:
            tag = "[E101] ";
            break;
        case NSURLErrorCannotConnectToHost:
        case NSURLErrorNotConnectedToInternet:
            tag = "[E102] ";
            break;
        case NSURLErrorTimedOut:
            tag = "[E103] ";
            break;
        case NSURLErrorSecureConnectionFailed:
        case NSURLErrorServerCertificateHasBadDate:
        case NSURLErrorServerCertificateUntrusted:
        case NSURLErrorServerCertificateHasUnknownRoot:
        case NSURLErrorServerCertificateNotYetValid:
        case NSURLErrorClientCertificateRejected:
        case NSURLErrorClientCertificateRequired:
        case NSURLErrorAppTransportSecurityRequiresSecureConnection:
            tag = "[E104] ";
            break;
        default:
            break;
        }
    }
    return std::string(tag) + (error.localizedDescription.UTF8String ?: "network error");
}

bool Contains(const std::string& text, const char* part)
{
    return text.find(part) != std::string::npos;
}

// What the player sees when the sync fails: a short code instead of the error
// text, which can carry the server's address. The full text still goes to the
// log. Same codes as Android's PreloadActivity.errorCode:
//   E101 host name not resolved       E102 cannot connect (down / port closed)
//   E103 timed out                    E104 secure connection failed
//   E4xx / E5xx the server's HTTP status
//   E301 data empty or incomplete     E302 archive damaged
//   E303 cannot write to storage      E199 anything else
std::string ErrorCode(const std::string& message)
{
    if (message.size() >= 6 && message[0] == '[' && message[1] == 'E' && message[5] == ']')
    {
        return message.substr(1, 4);
    }
    const size_t http = message.find("HTTP ");
    if (http != std::string::npos)
    {
        const int status = atoi(message.c_str() + http + 5);
        if (status >= 100 && status <= 599)
        {
            return "E" + std::to_string(status);
        }
    }
    std::string lower = message;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    if (Contains(lower, "cannot write") || Contains(lower, "cannot create") || Contains(lower, "cannot save")
        || Contains(lower, "cannot install") || Contains(lower, "cannot read downloaded") || Contains(lower, "storage")
        || Contains(lower, "finalise") || Contains(lower, "write callback"))
    {
        return "E303";
    }
    if (Contains(lower, "cannot open data.zip") || Contains(lower, "failed to extract") || Contains(lower, "invalid zip"))
    {
        return "E302";
    }
    if (Contains(lower, "empty") || Contains(lower, "incomplete") || Contains(lower, "not found in downloaded")
        || Contains(lower, "short chunk"))
    {
        return "E301";
    }
    return "E199";
}

// Safety net for every status line: no address or port reaches the screen,
// whatever a message happens to contain.
NSString* HideAddress(NSString* text)
{
    if (text.length == 0)
    {
        return text;
    }
    static NSRegularExpression* s_url = [NSRegularExpression
        regularExpressionWithPattern:@"https?://\\S+" options:NSRegularExpressionCaseInsensitive error:nil];
    static NSRegularExpression* s_ip = [NSRegularExpression
        regularExpressionWithPattern:@"\\b\\d{1,3}(\\.\\d{1,3}){3}(:\\d+)?\\b" options:0 error:nil];
    NSString* out = [s_url stringByReplacingMatchesInString:text options:0
                                                      range:NSMakeRange(0, text.length) withTemplate:@"server"];
    return [s_ip stringByReplacingMatchesInString:out options:0
                                            range:NSMakeRange(0, out.length) withTemplate:@"server"];
}
}

// ---------------------------------------------------------------------------
// Download screen (Android: PreloadActivity.buildUi)
// ---------------------------------------------------------------------------

#if TARGET_OS_OSX
#include "MacPreloadOverlay.inc"
#else
static UIColor* ColorFromArgb(uint32_t argb)
{
    return [UIColor colorWithRed:((argb >> 16) & 0xFF) / 255.0
                           green:((argb >> 8) & 0xFF) / 255.0
                            blue:(argb & 0xFF) / 255.0
                           alpha:((argb >> 24) & 0xFF) / 255.0];
}

@interface MuPreloadProgressBar : UIView
@property(nonatomic) double progress;
@end

@implementation MuPreloadProgressBar
{
    CAGradientLayer* _fill;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    if ((self = [super initWithFrame:frame]))
    {
        self.backgroundColor = ColorFromArgb(0x2DFFFFFF);
        self.clipsToBounds = YES;
        _fill = [CAGradientLayer layer];
        _fill.colors = @[ (id)ColorFromArgb(0xFF5EC9FF).CGColor,
                          (id)ColorFromArgb(0xFF56FFA4).CGColor,
                          (id)ColorFromArgb(0xFFFFD85B).CGColor ];
        _fill.startPoint = CGPointMake(0, 0.5);
        _fill.endPoint = CGPointMake(1, 0.5);
        [self.layer addSublayer:_fill];
    }
    return self;
}

- (void)setProgress:(double)progress
{
    _progress = std::clamp(progress, 0.0, 1.0);
    [self setNeedsLayout];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    const CGRect bounds = self.bounds;
    self.layer.cornerRadius = bounds.size.height * 0.5;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    _fill.frame = CGRectMake(0, 0, bounds.size.width * _progress, bounds.size.height);
    _fill.cornerRadius = bounds.size.height * 0.5;
    [CATransaction commit];
}
@end

@interface MuPreloadOverlay : UIView
- (void)setStage:(NSString*)stage detail:(NSString*)detail timer:(NSString*)timer percent:(double)percent;
- (void)showRetry:(BOOL)show;
- (void)dismiss;
@end

@implementation MuPreloadOverlay
{
    UIImageView* _banner;
    CAGradientLayer* _shade;
    UIView* _panel;
    UILabel* _title;
    UILabel* _stage;
    MuPreloadProgressBar* _progress;
    UILabel* _detail;
    UILabel* _timer;
    UIButton* _retry;
    NSArray<UIImage*>* _banners;
    NSUInteger _bannerIndex;
    NSTimer* _bannerTimer;
}

- (UILabel*)addLabelWithSize:(CGFloat)size color:(uint32_t)argb bold:(BOOL)bold
{
    UILabel* label = [[UILabel alloc] init];
    label.font = bold ? [UIFont boldSystemFontOfSize:size] : [UIFont systemFontOfSize:size];
    label.textColor = ColorFromArgb(argb);
    label.numberOfLines = 1;
    label.lineBreakMode = NSLineBreakByTruncatingMiddle;
    [_panel addSubview:label];
    return label;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    if (!(self = [super initWithFrame:frame]))
    {
        return nil;
    }

    self.backgroundColor = ColorFromArgb(0xFF080B14);
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    NSMutableArray<UIImage*>* banners = [NSMutableArray array];
    for (NSString* name in @[ @"banner_preload_1", @"banner_preload_2" ])
    {
        NSString* path = [[NSBundle mainBundle] pathForResource:name ofType:@"jpg"];
        UIImage* image = path ? [UIImage imageWithContentsOfFile:path] : nil;
        if (image)
        {
            [banners addObject:image];
        }
    }
    _banners = banners;

    _banner = [[UIImageView alloc] init];
    _banner.contentMode = UIViewContentModeScaleAspectFill;
    _banner.clipsToBounds = YES;
    _banner.image = _banners.firstObject;
    [self addSubview:_banner];

    _shade = [CAGradientLayer layer];
    _shade.colors = @[ (id)ColorFromArgb(0x22000000).CGColor, (id)ColorFromArgb(0xB8000000).CGColor ];
    [self.layer addSublayer:_shade];

    _panel = [[UIView alloc] init];
    _panel.backgroundColor = ColorFromArgb(0xB21A2230);
    _panel.layer.cornerRadius = 18;
    _panel.layer.borderWidth = 1;
    _panel.layer.borderColor = ColorFromArgb(0x52FFFFFF).CGColor;
    [self addSubview:_panel];

    _title = [self addLabelWithSize:18 color:0xFFF3F8FF bold:YES];
    _title.attributedText = [[NSAttributedString alloc] initWithString:@"DATA SYNC"
                                                            attributes:@{ NSKernAttributeName : @1.1 }];
    _stage = [self addLabelWithSize:14 color:0xFFDCE8FF bold:NO];
    _progress = [[MuPreloadProgressBar alloc] init];
    [_panel addSubview:_progress];
    _detail = [self addLabelWithSize:12 color:0xFFC8D6EA bold:NO];
    _timer = [self addLabelWithSize:12 color:0xFFB7C9E8 bold:NO];

    _retry = [UIButton buttonWithType:UIButtonTypeSystem];
    [_retry setTitle:@"Retry" forState:UIControlStateNormal];
    _retry.titleLabel.font = [UIFont boldSystemFontOfSize:14];
    [_retry setTitleColor:ColorFromArgb(0xFF080B14) forState:UIControlStateNormal];
    _retry.backgroundColor = ColorFromArgb(0xFF56FFA4);
    _retry.layer.cornerRadius = 8;
    _retry.hidden = YES;
    [_retry addTarget:self action:@selector(retryTapped) forControlEvents:UIControlEventTouchUpInside];
    [_panel addSubview:_retry];

    _stage.text = @"Preparing data sync...";
    _detail.text = @"Contacting server...";

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
    // No UIView animateWithDuration crossfade here: animating _banner.alpha
    // triggers UIKit's focus-engine environment walk on iPad
    // (-[UIView _focusBehavior] -> ... -> -[UIWindow parentFocusEnvironment]),
    // which hangs indefinitely on this sokol_app.h-managed window and gets the
    // app killed by the watchdog (XPC_EXIT_REASON_SIGTERM_TIMEOUT). A direct
    // image swap is cosmetically slightly less smooth but doesn't hit that path.
    _bannerIndex = (_bannerIndex + 1) % _banners.count;
    _banner.image = _banners[_bannerIndex];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    const CGRect bounds = self.bounds;
    _banner.frame = bounds;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    _shade.frame = bounds;
    [CATransaction commit];

    const UIEdgeInsets safe = self.safeAreaInsets;
    const CGFloat margin = 20;
    const CGFloat padH = 18, padV = 14;
    const CGFloat left = safe.left + margin;
    const CGFloat width = bounds.size.width - left - safe.right - margin;
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
        _retry.frame = CGRectMake(padH, y, 120, 34);
        y += 34;
    }
    y += padV;

    const CGFloat bottom = bounds.size.height - safe.bottom - margin;
    _panel.frame = CGRectMake(left, bottom - y, width, y);
}

- (void)setStage:(NSString*)stage detail:(NSString*)detail timer:(NSString*)timer percent:(double)percent
{
    if (stage) _stage.text = HideAddress(stage);
    if (detail) _detail.text = HideAddress(detail);
    if (timer) _timer.text = HideAddress(timer);
    if (percent >= 0.0) _progress.progress = percent / 100.0;
}

- (void)showRetry:(BOOL)show
{
    _retry.hidden = !show;
    [self setNeedsLayout];
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
    [UIView animateWithDuration:0.25
        animations:^{ self.alpha = 0.0; }
        completion:^(BOOL) { [self removeFromSuperview]; }];
}
@end
#endif

namespace
{
MuPreloadOverlay* g_overlay = nil;

#if !TARGET_OS_OSX
UIWindow* FindHostWindow()
{
    for (UIScene* scene in [UIApplication sharedApplication].connectedScenes)
    {
        if (![scene isKindOfClass:[UIWindowScene class]])
        {
            continue;
        }
        for (UIWindow* window in ((UIWindowScene*)scene).windows)
        {
            if (window.isKeyWindow)
            {
                return window;
            }
        }
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    UIWindow* window = [UIApplication sharedApplication].keyWindow;
    if (window == nil)
    {
        window = [UIApplication sharedApplication].windows.firstObject;
    }
#pragma clang diagnostic pop
    return window;
}
#endif

void PostUi(NSString* stage, NSString* detail, NSString* timer, double percent)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [g_overlay setStage:stage detail:detail timer:timer percent:percent];
    });
}

NSString* NS(const std::string& text)
{
    return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

void UpdateDownloadUi(long long downloaded, long long total, double speed, double overallPercent)
{
    long long etaMs = 0;
    if (total > 0 && speed > 0.0)
    {
        etaMs = std::max(0LL, static_cast<long long>((total - downloaded) / speed * 1000.0));
    }
    char stage[64];
    if (total > 0)
    {
        snprintf(stage, sizeof(stage), "Downloading data.zip %.2f%%", downloaded * 100.0 / std::max(1LL, total));
    }
    else
    {
        snprintf(stage, sizeof(stage), "Downloading data.zip");
    }
    const std::string detail = "Downloaded " + FormatBytes(downloaded) + (total > 0 ? " / " + FormatBytes(total) : "");
    const std::string timer = "Speed " + FormatBytes(static_cast<long long>(speed)) + "/s | ETA "
        + (total > 0 ? FormatDuration(etaMs) : "--:--");
    PostUi(NS(stage), NS(detail), NS(timer), overallPercent);
}

// ---------------------------------------------------------------------------
// HTTP (Android: HttpURLConnection calls in PreloadActivity)
// ---------------------------------------------------------------------------

NSURLSession* Session()
{
    static NSURLSession* s_session = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSURLSessionConfiguration* config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        config.HTTPMaximumConnectionsPerHost = kChunkThreads + 2;
        config.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
        config.timeoutIntervalForRequest = 30;
        config.timeoutIntervalForResource = 24 * 60 * 60;
        s_session = [NSURLSession sessionWithConfiguration:config];
    });
    return s_session;
}

NSMutableURLRequest* MakeRequest(bool withAuth, NSTimeInterval timeout)
{
    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:kDataZipUrl]];
    request.timeoutInterval = timeout;
    [request setValue:@"identity" forHTTPHeaderField:@"Accept-Encoding"];
    [request setValue:@"MuMain-iOS-Preload/1.0" forHTTPHeaderField:@"User-Agent"];
    if (withAuth)
    {
        NSString* raw = [NSString stringWithFormat:@"%@:%@", kBasicAuthUser, kBasicAuthPassword];
        NSString* token = [[raw dataUsingEncoding:NSUTF8StringEncoding] base64EncodedStringWithOptions:0];
        [request setValue:[@"Basic " stringByAppendingString:token] forHTTPHeaderField:@"Authorization"];
    }
    return request;
}

struct HttpResult
{
    NSInteger status = 0;
    NSHTTPURLResponse* response = nil;
    NSData* body = nil;
    std::string error;
};

HttpResult RunDataTask(NSURLRequest* request)
{
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block HttpResult result;
    NSURLSessionDataTask* task = [Session() dataTaskWithRequest:request
        completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
            if (error)
            {
                result.error = NetworkErrorText(error);
            }
            else if ([response isKindOfClass:[NSHTTPURLResponse class]])
            {
                result.response = (NSHTTPURLResponse*)response;
                result.status = result.response.statusCode;
                result.body = data;
            }
            dispatch_semaphore_signal(done);
        }];
    [task resume];
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    return result;
}

std::string Header(NSHTTPURLResponse* response, NSString* name)
{
    NSString* value = [response valueForHTTPHeaderField:name];
    return value ? std::string([value stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet].UTF8String) : std::string();
}

std::string MakeSignature(long long length, NSHTTPURLResponse* response)
{
    return "len=" + std::to_string(length) + ";etag=" + Header(response, @"ETag")
        + ";mod=" + Header(response, @"Last-Modified");
}

// fetchRemoteDataSignature: HEAD for length/ETag/Last-Modified. "" means
// "cannot tell" (offline, host down), never "changed".
std::string FetchRemoteDataSignature()
{
    for (bool withAuth : { false, true })
    {
        NSMutableURLRequest* request = MakeRequest(withAuth, 8);
        request.HTTPMethod = @"HEAD";
        const HttpResult result = RunDataTask(request);
        if (!result.error.empty())
        {
            Log("HEAD failed (auth=%d): %s", withAuth ? 1 : 0, result.error.c_str());
            continue;
        }
        if (result.status == 401 && !withAuth)
        {
            continue;
        }
        if (result.status < 200 || result.status >= 300)
        {
            break;
        }

        const long long length = result.response.expectedContentLength;
        if (length < 0 && Header(result.response, @"ETag").empty() && Header(result.response, @"Last-Modified").empty())
        {
            return {};
        }
        return MakeSignature(length, result.response);
    }
    return {};
}

struct RemoteFileInfo
{
    bool valid = false;
    bool unauthorized = false;
    bool supportsRanges = false;
    long long totalBytes = -1;
    std::string signature;
    std::string error;
};

// probeRemoteFile: a one-byte ranged GET. A 206 proves range support and its
// Content-Range carries the true total length.
RemoteFileInfo ProbeRemoteFile(bool withAuth)
{
    RemoteFileInfo info;
    NSMutableURLRequest* request = MakeRequest(withAuth, 15);
    [request setValue:@"bytes=0-0" forHTTPHeaderField:@"Range"];
    const HttpResult result = RunDataTask(request);
    if (!result.error.empty())
    {
        info.error = result.error;
        return info;
    }
    if (result.status == 401)
    {
        info.unauthorized = true;
        return info;
    }

    if (result.status == 206)
    {
        info.supportsRanges = true;
        const std::string contentRange = Header(result.response, @"Content-Range");
        const size_t slash = contentRange.rfind('/');
        if (slash != std::string::npos)
        {
            info.totalBytes = atoll(contentRange.c_str() + slash + 1);
        }
    }
    else if (result.status >= 200 && result.status < 300)
    {
        info.totalBytes = result.response.expectedContentLength;
    }
    else
    {
        info.error = "HTTP " + std::to_string(result.status) + " while probing data.zip";
        return info;
    }

    info.signature = MakeSignature(info.totalBytes, result.response);
    info.valid = info.totalBytes > 0;
    Log("probe: total=%lld ranges=%d", info.totalBytes, info.supportsRanges ? 1 : 0);
    return info;
}

// Tasks currently in flight, so the progress readout can include the bytes of
// chunks that have not finished yet.
std::mutex g_inflightMutex;
NSURLSessionTask* g_inflight[kChunkThreads + 1];

long long InflightBytes()
{
    std::lock_guard<std::mutex> lock(g_inflightMutex);
    long long bytes = 0;
    for (NSURLSessionTask* task : g_inflight)
    {
        bytes += task ? task.countOfBytesReceived : 0;
    }
    return bytes;
}

// Runs one download task to completion on the calling thread. `onFile` gets
// the temporary file NSURLSession wrote, which is deleted once it returns.
bool RunDownloadTask(NSURLRequest* request, int slot, NSInteger expectedStatus,
                     bool (^onFile)(NSURL* location, std::string* error), NSInteger* statusOut, std::string* error)
{
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block bool ok = false;
    __block NSInteger status = 0;
    __block std::string failure;

    NSURLSessionDownloadTask* task = [Session() downloadTaskWithRequest:request
        completionHandler:^(NSURL* location, NSURLResponse* response, NSError* taskError) {
            if (taskError)
            {
                failure = NetworkErrorText(taskError);
            }
            else
            {
                status = [response isKindOfClass:[NSHTTPURLResponse class]] ? ((NSHTTPURLResponse*)response).statusCode : 0;
                if (status != expectedStatus)
                {
                    failure = "HTTP " + std::to_string(status);
                }
                else
                {
                    ok = onFile(location, &failure);
                }
            }
            dispatch_semaphore_signal(done);
        }];

    {
        std::lock_guard<std::mutex> lock(g_inflightMutex);
        g_inflight[slot] = task;
    }
    [task resume];
    while (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC)) != 0)
    {
        if (g_cancel.load())
        {
            [task cancel];
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_inflightMutex);
        g_inflight[slot] = nil;
    }

    if (statusOut) *statusOut = status;
    if (!ok && error) *error = failure;
    return ok;
}

bool CopyFileIntoRange(NSURL* location, int fd, long long offset, long long expectedLength, std::string* error)
{
    FILE* in = fopen(location.path.fileSystemRepresentation, "rb");
    if (in == nullptr)
    {
        *error = "cannot read downloaded chunk";
        return false;
    }
    std::vector<char> buffer(128 * 1024);
    long long written = 0;
    size_t n = 0;
    while ((n = fread(buffer.data(), 1, buffer.size(), in)) > 0)
    {
        if (pwrite(fd, buffer.data(), n, offset + written) != static_cast<ssize_t>(n))
        {
            fclose(in);
            *error = "cannot write data.zip.part (storage full?)";
            return false;
        }
        written += static_cast<long long>(n);
    }
    fclose(in);
    if (written != expectedLength)
    {
        *error = "short chunk: " + std::to_string(written) + " of " + std::to_string(expectedLength);
        return false;
    }
    return true;
}

std::string ReadFirstLine(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr)
    {
        return {};
    }
    char buffer[8192] = {};
    const size_t n = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    std::string line(buffer, n);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
    {
        line.pop_back();
    }
    return line;
}

void WriteChunkState(const std::string& path, const std::string& signature, const std::vector<char>& done)
{
    std::string state = signature + "|";
    for (char flag : done)
    {
        state.push_back(flag ? '1' : '0');
    }
    if (FILE* f = fopen(path.c_str(), "wb"))
    {
        fwrite(state.data(), 1, state.size(), f);
        fclose(f);
    }
}

// downloadZipParallel: work-stealing 8 MB chunks over 4 connections, written
// straight into a pre-sized .part file, with a sidecar recording finished
// chunks so an interrupted download resumes. Keyed on the remote signature, so
// a data.zip replaced mid-download starts over instead of mixing two archives.
bool DownloadZipParallel(bool withAuth, const std::string& zipPath, const RemoteFileInfo& info, std::string* error)
{
    const long long startMs = NowMs();
    const long long total = info.totalBytes;
    const std::string partPath = zipPath + ".part";
    const std::string statePath = zipPath + ".part.state";
    const int chunkCount = static_cast<int>((total + kChunkBytes - 1) / kChunkBytes);
    auto chunkLength = [&](int index) {
        const long long begin = index * kChunkBytes;
        return std::max(0LL, std::min(begin + kChunkBytes, total) - begin);
    };

    std::vector<char> chunkDone(chunkCount, 0);
    long long resumedBytes = 0;
    struct stat partStat {};
    if (stat(partPath.c_str(), &partStat) == 0 && partStat.st_size == total)
    {
        const std::string stored = ReadFirstLine(statePath);
        if (stored.compare(0, info.signature.size() + 1, info.signature + "|") == 0)
        {
            const std::string flags = stored.substr(info.signature.size() + 1);
            for (int i = 0; i < chunkCount && i < static_cast<int>(flags.size()); ++i)
            {
                if (flags[i] == '1')
                {
                    chunkDone[i] = 1;
                    resumedBytes += chunkLength(i);
                }
            }
            Log("resuming download, %lld of %lld already present", resumedBytes, total);
        }
    }
    if (resumedBytes == 0)
    {
        unlink(statePath.c_str());
    }

    const int fd = open(partPath.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0 || ftruncate(fd, total) != 0)
    {
        if (fd >= 0) close(fd);
        *error = "cannot create data.zip.part (storage full?)";
        return false;
    }

    std::atomic<long long> completedBytes { resumedBytes };
    std::atomic<int> nextChunk { 0 };
    std::mutex stateMutex;
    std::string failure;

    std::vector<std::thread> workers;
    const int workerCount = std::min(kChunkThreads, std::max(1, chunkCount));
    std::atomic<int> activeWorkers { workerCount };
    for (int w = 0; w < workerCount; ++w)
    {
        workers.emplace_back([&, w]() {
            struct ExitCounter
            {
                std::atomic<int>& count;
                ~ExitCounter() { count.fetch_sub(1); }
            } exitCounter { activeWorkers };

            for (;;)
            {
                {
                    std::lock_guard<std::mutex> lock(stateMutex);
                    if (!failure.empty() || g_cancel.load())
                    {
                        return;
                    }
                }
                const int index = nextChunk.fetch_add(1);
                if (index >= chunkCount)
                {
                    return;
                }
                {
                    std::lock_guard<std::mutex> lock(stateMutex);
                    if (chunkDone[index])
                    {
                        continue;
                    }
                }

                const long long begin = index * kChunkBytes;
                const long long length = chunkLength(index);
                std::string chunkError;
                bool ok = false;
                for (int attempt = 0; attempt < kChunkAttempts && !g_cancel.load(); ++attempt)
                {
                    NSMutableURLRequest* request = MakeRequest(withAuth, 30);
                    [request setValue:[NSString stringWithFormat:@"bytes=%lld-%lld", begin, begin + length - 1]
                        forHTTPHeaderField:@"Range"];
                    ok = RunDownloadTask(request, w, 206,
                        ^bool(NSURL* location, std::string* copyError) {
                            return CopyFileIntoRange(location, fd, begin, length, copyError);
                        },
                        nullptr, &chunkError);
                    if (ok)
                    {
                        break;
                    }
                    Log("chunk %d attempt %d failed: %s", index, attempt + 1, chunkError.c_str());
                    std::this_thread::sleep_for(std::chrono::milliseconds(400 * (attempt + 1)));
                }

                std::lock_guard<std::mutex> lock(stateMutex);
                if (!ok)
                {
                    if (failure.empty())
                    {
                        failure = g_cancel.load() ? std::string("Download cancelled") : chunkError;
                    }
                    return;
                }
                chunkDone[index] = 1;
                completedBytes.fetch_add(length);
                WriteChunkState(statePath, info.signature, chunkDone);
            }
        });
    }

    // Progress: finished chunks plus what the in-flight ones have so far, with
    // the speed over a sliding one-second window rather than since the start.
    long long windowStartMs = startMs;
    long long windowStartBytes = resumedBytes;
    double recentSpeed = 0.0;
    for (;;)
    {
        const bool alive = activeWorkers.load() > 0;
        const long long now = NowMs();
        const long long soFar = std::min(total, completedBytes.load() + InflightBytes());
        if (now - windowStartMs >= 1000)
        {
            recentSpeed = (soFar - windowStartBytes) / ((now - windowStartMs) / 1000.0);
            windowStartMs = now;
            windowStartBytes = soFar;
        }
        UpdateDownloadUi(soFar, total, recentSpeed, std::min(1.0, soFar / static_cast<double>(total)) * kDownloadWeight);
        if (!alive)
        {
            break;
        }
        std::this_thread::sleep_for(kUiInterval);
    }

    for (std::thread& worker : workers)
    {
        worker.join();
    }
    close(fd);

    if (!failure.empty() || g_cancel.load())
    {
        *error = failure.empty() ? std::string("Download cancelled") : failure;
        return false; // .part and its state stay behind, so the next try resumes
    }

    unlink(zipPath.c_str());
    if (rename(partPath.c_str(), zipPath.c_str()) != 0)
    {
        *error = "unable to finalise downloaded archive";
        return false;
    }
    unlink(statePath.c_str());
    UpdateDownloadUi(total, total, recentSpeed, kDownloadWeight);
    Log("parallel download complete: %lld bytes in %lld ms (%lld resumed)", total, NowMs() - startMs, resumedBytes);
    return true;
}

// downloadZipFromUrl: one plain GET, for a host that will not do ranges.
bool DownloadZipSingle(bool withAuth, const std::string& zipPath, NSInteger* status, std::string* error)
{
    // Never the address or the download login - players see neither.
    PostUi(withAuth ? @"Connecting with Basic Auth..." : @"Connecting to download host...",
           @"Please wait...", @"", -1);

    const long long startMs = NowMs();
    std::atomic<bool> finished { false };
    std::thread monitor([&]() {
        while (!finished.load())
        {
            long long received = 0;
            long long expected = -1;
            {
                std::lock_guard<std::mutex> lock(g_inflightMutex);
                if (NSURLSessionTask* task = g_inflight[kChunkThreads])
                {
                    received = task.countOfBytesReceived;
                    expected = task.countOfBytesExpectedToReceive;
                }
            }
            const double speed = received / std::max(0.001, (NowMs() - startMs) / 1000.0);
            UpdateDownloadUi(received, expected, speed,
                expected > 0 ? std::min(1.0, received / static_cast<double>(expected)) * kDownloadWeight : 0.0);
            std::this_thread::sleep_for(kUiInterval);
        }
    });

    NSString* target = [NSString stringWithUTF8String:zipPath.c_str()];
    const bool ok = RunDownloadTask(MakeRequest(withAuth, 30), kChunkThreads, 200,
        ^bool(NSURL* location, std::string* moveError) {
            NSError* nsError = nil;
            [[NSFileManager defaultManager] removeItemAtPath:target error:nil];
            if (![[NSFileManager defaultManager] moveItemAtURL:location toURL:[NSURL fileURLWithPath:target] error:&nsError])
            {
                *moveError = nsError.localizedDescription.UTF8String ?: "cannot save data.zip";
                return false;
            }
            return true;
        },
        status, error);

    finished = true;
    monitor.join();
    return ok;
}

// downloadZip. Unlike Android, a failed parallel transfer does not fall back to
// a single stream from byte zero: the chunk state is kept, and the retry (or
// the next launch) resumes where it stopped. The single stream is only used
// when the host does not support ranges at all.
bool DownloadZip(const std::string& zipPath, std::string* error)
{
    for (bool withAuth : { false, true })
    {
        const RemoteFileInfo info = ProbeRemoteFile(withAuth);
        if (info.unauthorized)
        {
            continue;
        }
        if (!info.error.empty())
        {
            *error = info.error;
            return false;
        }
        if (info.valid && info.supportsRanges && info.totalBytes >= kParallelMinBytes)
        {
            return DownloadZipParallel(withAuth, zipPath, info, error);
        }
        break;
    }

    NSInteger status = 0;
    if (DownloadZipSingle(false, zipPath, &status, error))
    {
        return true;
    }
    return status == 401 && DownloadZipSingle(true, zipPath, &status, error);
}

// ---------------------------------------------------------------------------
// The flow (Android: runPreloadFlow)
// ---------------------------------------------------------------------------

void EndBackgroundTask()
{
#if !TARGET_OS_OSX
    dispatch_async(dispatch_get_main_queue(), ^{
        if (g_backgroundTask != UIBackgroundTaskInvalid)
        {
            [[UIApplication sharedApplication] endBackgroundTask:g_backgroundTask];
            g_backgroundTask = UIBackgroundTaskInvalid;
        }
    });
#endif
}

void Finish(NSString* stage, NSString* detail, NSString* timer, double delaySeconds)
{
    PostUi(stage, detail, timer, 100.0);
    EndBackgroundTask();
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(delaySeconds * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        [g_overlay dismiss];
        g_overlay = nil;
        g_running = false;
        g_complete = true;
    });
}

void Fail(const std::string& message)
{
    Log("preload failed: %s", message.c_str());
    EndBackgroundTask();
    // Worked out here, not in the block: a block captures a C++ reference as a
    // reference, and by the time it runs the caller's string is gone (TestFlight
    // 1.0.28 (29) crashed in ErrorCode reading the dead stack).
    NSString* detail = NS("Error code: " + ErrorCode(message));
    dispatch_async(dispatch_get_main_queue(), ^{
        [g_overlay setStage:@"Preload failed"
                     detail:detail
                      timer:@"Check your connection, then tap Retry."
                    percent:-1];
        [g_overlay showRetry:YES];
        g_running = false;
        g_awaitingRetry = true;
    });
}

// ---------------------------------------------------------------------------
// Incremental update (Android: tryIncrementalUpdate / runIncrementalUpdate).
// Anything unexpected - no range support, a zip feature this does not read, a
// CRC mismatch, most of the archive changed - returns false, and the caller
// does the full download it always did.
// ---------------------------------------------------------------------------

// Past this share of the archive changed, one full parallel download is the
// quicker way.
constexpr double kIncrementalMaxChangedShare = 0.5;
// Changed files next to each other in the archive go in one range, up to this.
constexpr uint64_t kIncrementalGroupBytes = 8ULL * 1024 * 1024;

NSMutableURLRequest* MakeRangeRequest(bool withAuth, uint64_t start, uint64_t endInclusive, NSTimeInterval timeout)
{
    NSMutableURLRequest* request = MakeRequest(withAuth, timeout);
    [request setValue:[NSString stringWithFormat:@"bytes=%llu-%llu", start, endInclusive] forHTTPHeaderField:@"Range"];
    return request;
}

bool FetchRangeBytes(bool withAuth, uint64_t start, uint64_t endInclusive, std::vector<uint8_t>* out, std::string* error)
{
    const uint64_t length = endInclusive - start + 1;
    if (endInclusive < start || length > 64ULL * 1024 * 1024)
    {
        *error = "bad range " + std::to_string(start) + "-" + std::to_string(endInclusive);
        return false;
    }
    const HttpResult result = RunDataTask(MakeRangeRequest(withAuth, start, endInclusive, 30));
    if (!result.error.empty())
    {
        *error = result.error;
        return false;
    }
    if (result.status != 206)
    {
        *error = "HTTP " + std::to_string(result.status) + " for range " + std::to_string(start) + "-"
            + std::to_string(endInclusive);
        return false;
    }
    if (result.body.length != length)
    {
        *error = "range cut short: " + std::to_string(result.body.length) + " of " + std::to_string(length);
        return false;
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(result.body.bytes);
    out->assign(bytes, bytes + length);
    return true;
}

bool ReadRemoteCentralDirectory(bool withAuth, uint64_t total, std::vector<RemoteZipEntry>* entries, std::string* error)
{
    // The end record sits in the last 22 bytes plus a comment of up to 64 KB;
    // the zip64 locator, when there is one, is the 20 bytes before it.
    const uint64_t tailLength = std::min<uint64_t>(total, 65536 + 22 + 20);
    std::vector<uint8_t> tail;
    ZipEndInfo end;
    if (!FetchRangeBytes(withAuth, total - tailLength, total - 1, &tail, error) || !ParseZipEnd(tail, &end, error))
    {
        return false;
    }
    if (end.needsZip64)
    {
        std::vector<uint8_t> record;
        if (!FetchRangeBytes(withAuth, end.zip64EndOffset, end.zip64EndOffset + 55, &record, error)
            || !ParseZip64End(record, &end, error))
        {
            return false;
        }
    }
    if (end.centralSize == 0 || end.centralSize > 64ULL * 1024 * 1024 || end.centralOffset + end.centralSize > total)
    {
        *error = "zip central directory out of range";
        return false;
    }

    std::vector<uint8_t> central;
    return FetchRangeBytes(withAuth, end.centralOffset, end.centralOffset + end.centralSize - 1, &central, error)
        && ParseCentralDirectory(central, end.centralOffset, end.entryCount, entries, error);
}

// One worker's download of a group's range into partPath, counted in `slot`
// of g_inflight while it runs.
bool FetchRangeToFile(bool withAuth, uint64_t start, uint64_t endInclusive, const std::string& partPath, int slot,
                      std::string* error)
{
    const long long expected = static_cast<long long>(endInclusive - start + 1);
    NSString* target = [NSString stringWithUTF8String:partPath.c_str()];
    return RunDownloadTask(MakeRangeRequest(withAuth, start, endInclusive, 30), slot, 206,
        ^bool(NSURL* location, std::string* moveError) {
            NSError* nsError = nil;
            [[NSFileManager defaultManager] removeItemAtPath:target error:nil];
            if (![[NSFileManager defaultManager] moveItemAtURL:location toURL:[NSURL fileURLWithPath:target] error:&nsError])
            {
                *moveError = "cannot save downloaded chunk";
                return false;
            }
            struct stat st {};
            if (stat(partPath.c_str(), &st) != 0 || st.st_size != expected)
            {
                *moveError = "range cut short: " + std::to_string(static_cast<long long>(st.st_size)) + " of "
                    + std::to_string(expected);
                return false;
            }
            return true;
        },
        nullptr, error);
}

bool DownloadChangedEntries(bool withAuth, const std::vector<const RemoteZipEntry*>& changed, const std::string& dataDir,
                            const std::string& root, std::string* error)
{
    const long long startMs = NowMs();

    // Entries back to back in the archive go in one ranged request.
    std::vector<const RemoteZipEntry*> sorted = changed;
    std::sort(sorted.begin(), sorted.end(),
              [](const RemoteZipEntry* a, const RemoteZipEntry* b) { return a->localOffset < b->localOffset; });
    std::vector<std::vector<RemoteZipEntry>> groups;
    uint64_t groupStart = 0;
    uint64_t groupEnd = 0;
    long long totalBytes = 0;
    for (const RemoteZipEntry* entry : sorted)
    {
        if (!groups.empty() && entry->localOffset == groupEnd && entry->spanEnd - groupStart <= kIncrementalGroupBytes)
        {
            groups.back().push_back(*entry);
            groupEnd = entry->spanEnd;
        }
        else
        {
            groups.push_back({ *entry });
            groupStart = entry->localOffset;
            groupEnd = entry->spanEnd;
        }
        totalBytes += static_cast<long long>(entry->spanEnd - entry->localOffset);
    }

    const std::string workDir = JoinPath(root, "preload_data_patch");
    DeleteRecursively(workDir);
    if (!EnsureDirectory(workDir))
    {
        *error = "cannot create patch folder";
        return false;
    }

    std::atomic<long long> doneBytes { 0 };
    std::atomic<int> filesDone { 0 };
    std::atomic<size_t> nextGroup { 0 };
    std::mutex failureMutex;
    std::string failure;
    auto failed = [&]() {
        std::lock_guard<std::mutex> lock(failureMutex);
        return !failure.empty();
    };

    std::vector<std::thread> workers;
    const int workerCount = static_cast<int>(std::min<size_t>(kChunkThreads, groups.size()));
    std::atomic<int> running { workerCount };
    auto work = [&](int w) {
            while (!failed() && !g_cancel.load())
            {
                const size_t index = nextGroup.fetch_add(1);
                if (index >= groups.size())
                {
                    return;
                }
                const std::vector<RemoteZipEntry>& group = groups[index];
                const uint64_t start = group.front().localOffset;
                const uint64_t endInclusive = group.back().spanEnd - 1;
                const std::string part = JoinPath(workDir, "g" + std::to_string(index) + ".bin");

                std::string groupError;
                bool fetched = false;
                for (int attempt = 0; attempt < kChunkAttempts && !g_cancel.load(); ++attempt)
                {
                    groupError.clear();
                    if (FetchRangeToFile(withAuth, start, endInclusive, part, w, &groupError))
                    {
                        fetched = true;
                        break;
                    }
                    Log("update group %zu attempt %d failed: %s", index, attempt + 1, groupError.c_str());
                    std::this_thread::sleep_for(std::chrono::milliseconds(400 * (attempt + 1)));
                }
                if (fetched)
                {
                    doneBytes.fetch_add(static_cast<long long>(endInclusive - start + 1));
                    if (ApplyEntriesFromPart(part, start, group, dataDir, g_cancel, &groupError))
                    {
                        filesDone.fetch_add(static_cast<int>(group.size()));
                    }
                    else
                    {
                        fetched = false;
                    }
                }
                unlink(part.c_str());
                if (!fetched)
                {
                    std::lock_guard<std::mutex> lock(failureMutex);
                    if (failure.empty())
                    {
                        failure = groupError.empty() ? std::string("Download cancelled") : groupError;
                    }
                    return;
                }
            }
    };
    for (int w = 0; w < workerCount; ++w)
    {
        workers.emplace_back([&, w]() {
            work(w);
            running.fetch_sub(1);
        });
    }

    const int fileTotal = static_cast<int>(changed.size());
    long long windowStartMs = startMs;
    long long windowStartBytes = 0;
    double recentSpeed = 0.0;
    for (;;)
    {
        const bool alive = running.load() > 0;
        const long long soFar = std::min(totalBytes, doneBytes.load() + InflightBytes());
        const long long now = NowMs();
        if (now - windowStartMs >= 1000)
        {
            recentSpeed = (soFar - windowStartBytes) / ((now - windowStartMs) / 1000.0);
            windowStartMs = now;
            windowStartBytes = soFar;
        }
        const double share = std::min(1.0, soFar / static_cast<double>(std::max(1LL, totalBytes)));
        const long long etaMs = recentSpeed > 0.0 ? static_cast<long long>((totalBytes - soFar) / recentSpeed * 1000.0) : 0;
        char stage[64];
        snprintf(stage, sizeof(stage), "Updating changed files %.1f%%", share * 100.0);
        PostUi(NS(stage),
               NS(std::to_string(filesDone.load()) + " / " + std::to_string(fileTotal) + " files | "
                  + FormatBytes(soFar) + " / " + FormatBytes(totalBytes)),
               NS("Speed " + FormatBytes(static_cast<long long>(std::max(0.0, recentSpeed))) + "/s | ETA "
                  + FormatDuration(std::max(0LL, etaMs))),
               20.0 + share * 78.0);
        if (!alive)
        {
            break;
        }
        std::this_thread::sleep_for(kUiInterval);
    }
    for (std::thread& worker : workers)
    {
        worker.join();
    }
    DeleteRecursively(workDir);

    if (g_cancel.load())
    {
        *error = "Download cancelled";
        return false;
    }
    if (!failure.empty())
    {
        *error = failure;
        return false;
    }
    return true;
}

bool RunIncrementalUpdate(const std::string& root, const std::string& dataDir, std::string* error)
{
    const long long startMs = NowMs();
    PostUi(@"Checking for updated files...", @"Reading the file list", @"", 1.0);

    bool withAuth = false;
    RemoteFileInfo info;
    bool probed = false;
    for (bool auth : { false, true })
    {
        RemoteFileInfo candidate = ProbeRemoteFile(auth);
        if (candidate.unauthorized || !candidate.error.empty())
        {
            if (!candidate.error.empty())
            {
                Log("incremental probe failed: %s", candidate.error.c_str());
            }
            continue;
        }
        info = candidate;
        withAuth = auth;
        probed = true;
        break;
    }
    if (!probed || !info.valid || !info.supportsRanges)
    {
        *error = "incremental update needs range support";
        return false;
    }
    const uint64_t total = static_cast<uint64_t>(info.totalBytes);

    std::vector<RemoteZipEntry> entries;
    if (!ReadRemoteCentralDirectory(withAuth, total, &entries, error))
    {
        return false;
    }

    const std::unordered_map<std::string, std::string> localFiles = ListLocalFiles(dataDir);
    const std::unordered_map<std::string, IndexEntry> index = ReadDataIndex(root);
    const bool verifyByCrc = index.empty();

    int fileCount = 0;
    for (const RemoteZipEntry& entry : entries)
    {
        if (!entry.key.empty() && !entry.isDirectory)
        {
            ++fileCount;
        }
    }

    std::vector<const RemoteZipEntry*> changed;
    uint64_t changedBytes = 0;
    int checked = 0;
    long long lastUiMs = 0;
    for (const RemoteZipEntry& entry : entries)
    {
        if (entry.key.empty() || entry.isDirectory)
        {
            continue;
        }
        if (g_cancel.load())
        {
            *error = "Download cancelled";
            return false;
        }

        bool same = false;
        const auto local = localFiles.find(entry.key);
        struct stat st {};
        if (local != localFiles.end() && stat(local->second.c_str(), &st) == 0
            && static_cast<uint64_t>(st.st_size) == entry.size)
        {
            if (!verifyByCrc)
            {
                const auto known = index.find(entry.key);
                same = known != index.end() && known->second.crc == entry.crc && known->second.size == entry.size;
            }
            else
            {
                uint32_t crc = 0;
                same = CrcOfFile(local->second, &crc) && crc == entry.crc;
            }
        }
        if (!same)
        {
            changed.push_back(&entry);
            changedBytes += entry.spanEnd - entry.localOffset;
        }

        ++checked;
        const long long now = NowMs();
        if (verifyByCrc && now - lastUiMs >= kUiInterval.count())
        {
            lastUiMs = now;
            PostUi(@"Checking local files...",
                   NS("Checked " + std::to_string(checked) + " / " + std::to_string(fileCount) + " files"),
                   NS(std::to_string(changed.size()) + " to update so far"),
                   std::min(1.0, checked / static_cast<double>(std::max(1, fileCount))) * 20.0);
        }
    }

    Log("incremental: %zu of %d files changed, %llu of %llu bytes (verifyByCrc=%d)", changed.size(), fileCount,
        static_cast<unsigned long long>(changedBytes), static_cast<unsigned long long>(total), verifyByCrc ? 1 : 0);

    if (changedBytes > total * kIncrementalMaxChangedShare)
    {
        *error = "most of data.zip changed";
        return false;
    }
    if (!changed.empty() && !DownloadChangedEntries(withAuth, changed, dataDir, root, error))
    {
        return false;
    }

    WriteDataIndex(root, entries);
    WriteReadyMarker(root, dataDir, FetchRemoteDataSignature());

    const size_t count = changed.size();
    Finish(count > 0 ? @"Data updated. Launching game..." : @"Data up to date. Launching game...",
           NS(std::to_string(count) + (count == 1 ? " file" : " files") + " updated (" + FormatBytes(static_cast<long long>(changedBytes)) + ")"),
           NS("Update time: " + FormatDuration(NowMs() - startMs)), 0.65);
    return true;
}

bool TryIncrementalUpdate(const std::string& root, const std::string& dataDir)
{
    std::string error;
    if (RunIncrementalUpdate(root, dataDir, &error))
    {
        return true;
    }
    Log("incremental update not used, doing the full download: %s", error.c_str());
    return false;
}

bool ExtractAndInstall(const std::string& root, const std::string& cacheRoot, const std::string& zipPath, std::string* error)
{
    const std::string extractRoot = JoinPath(cacheRoot, "extract_stage");
    DeleteRecursively(extractRoot);
    if (!EnsureDirectory(extractRoot))
    {
        *error = "cannot create extract folder";
        return false;
    }

    ExtractProgress progress;
    if (!InspectZip(zipPath, progress, error))
    {
        return false;
    }

    std::atomic<bool> done { false };
    bool extracted = false;
    std::string extractError;
    std::thread worker([&]() {
        extracted = ExtractZip(zipPath, extractRoot, progress, g_cancel, &extractError);
        done = true;
    });

    const long long startMs = NowMs();
    while (!done.load())
    {
        const long long bytes = progress.bytes.load();
        const int files = progress.files.load();
        const long long elapsed = std::max(1LL, NowMs() - startMs);
        const double speed = bytes / (elapsed / 1000.0);
        const double fraction = progress.totalBytes > 0 ? std::min(1.0, bytes / static_cast<double>(progress.totalBytes))
            : (progress.fileCount > 0 ? std::min(1.0, files / static_cast<double>(progress.fileCount)) : 1.0);
        const long long etaMs = (progress.totalBytes > 0 && speed > 0)
            ? std::max(0LL, static_cast<long long>((progress.totalBytes - bytes) / speed * 1000.0)) : 0;

        char stage[64];
        snprintf(stage, sizeof(stage), "Parsing data folder %.2f%%", fraction * 100.0);
        const std::string detail = progress.totalBytes > 0
            ? "Parsed " + FormatBytes(bytes) + " / " + FormatBytes(progress.totalBytes)
            : "Parsed files " + std::to_string(files) + " / " + std::to_string(progress.fileCount);
        const std::string timer = progress.totalBytes > 0
            ? "Speed " + FormatBytes(static_cast<long long>(speed)) + "/s | ETA " + FormatDuration(etaMs)
            : "Elapsed " + FormatDuration(elapsed);
        PostUi(NS(stage), NS(detail), NS(timer), kDownloadWeight + fraction * kParseWeight * 0.95);
        std::this_thread::sleep_for(kUiInterval);
    }
    worker.join();
    if (!extracted)
    {
        *error = extractError;
        return false;
    }

    PostUi(@"Installing parsed data into old folder...", @"Applying into device data location...", @"Finalizing...", 99.0);
    return InstallExtractedData(extractRoot, root, error);
}

void RunPreloadFlow()
{
    const long long totalStartMs = NowMs();
    const std::string root = MU_IosDataRoot();
    EnsureDirectory(root);

    PostUi(@"Checking local data...", NS(root), @"", 0.0);
    std::string dataDir = FindChildIgnoreCase(root, kDataFolderName, true);
    const bool usable = IsDataFolderUsable(dataDir);

    // shouldSkipDownload: usable local data is kept unless the server now
    // serves a different data.zip. No answer from the server keeps it too.
    if (usable)
    {
        const std::string stored = ReadStoredSignature(root);
        const std::string remote = FetchRemoteDataSignature();
        if (remote.empty() || remote == stored)
        {
            if (remote.empty())
            {
                Log("could not read remote data.zip signature, keeping local data");
            }
            WriteReadyMarker(root, dataDir, stored);
            Finish(@"Data already present. Skip download.", NS(dataDir), @"Fast start mode", 0.35);
            return;
        }
        Log("remote data.zip changed. stored=%s remote=%s", stored.c_str(), remote.c_str());

        // Fetch only the files that differ; falls through to the full
        // download when that cannot be done.
        if (TryIncrementalUpdate(root, dataDir))
        {
            return;
        }
    }

    const std::string cacheRoot = JoinPath(root, "preload_data_sync");
    const std::string zipPath = JoinPath(cacheRoot, "data.zip");
    EnsureDirectory(cacheRoot);
    ExcludeFromBackup(cacheRoot);

    std::string error;
    const long long downloadStartMs = NowMs();
    if (!DownloadZip(zipPath, &error))
    {
        if (usable)
        {
            Log("sync failed, using existing data: %s", error.c_str());
            Finish(@"Sync failed, using existing data...", NS("Error code: " + ErrorCode(error)),
                   @"Launching with local data", 0.45);
            return;
        }
        Fail(error);
        return;
    }
    const long long downloadMs = NowMs() - downloadStartMs;

    const long long parseStartMs = NowMs();
    if (!ExtractAndInstall(root, cacheRoot, zipPath, &error))
    {
        // The finished archive stays in the cache; only a broken one is removed.
        if (usable)
        {
            Log("sync failed, using existing data: %s", error.c_str());
            Finish(@"Sync failed, using existing data...", NS("Error code: " + ErrorCode(error)),
                   @"Launching with local data", 0.45);
            return;
        }
        Fail(error);
        return;
    }
    const long long parseMs = NowMs() - parseStartMs;

    dataDir = FindChildIgnoreCase(root, kDataFolderName, true);
    WriteReadyMarker(root, dataDir, FetchRemoteDataSignature());
    // What was just installed, so the next data.zip change can be fetched
    // file by file instead of whole.
    WriteDataIndexFromZip(zipPath, root);
    DeleteRecursively(cacheRoot);
    ExcludeFromBackup(dataDir);

    Finish(@"Data ready. Launching game...",
           NS("Download: " + FormatDuration(downloadMs) + " | Parse+Install: " + FormatDuration(parseMs)),
           NS("Total sync time: " + FormatDuration(NowMs() - totalStartMs)), 0.65);
}
} // namespace

bool MU_IosPreloadBegin()
{
    if (g_complete.load())
    {
        return true;
    }
    if (g_running.exchange(true))
    {
        return false;
    }
    g_awaitingRetry = false;
    g_cancel = false;

    if (g_overlay == nil)
    {
#if TARGET_OS_OSX
        g_overlay = MuShowPreloadOverlay();
#else
        UIWindow* window = FindHostWindow();
        g_overlay = [[MuPreloadOverlay alloc] initWithFrame:window ? window.bounds : [UIScreen mainScreen].bounds];
        [window addSubview:g_overlay];
#endif

        // A failed download is retried by itself when the player comes back to
        // the app, which is the usual way a transfer dies on a phone.
        static dispatch_once_t once;
        dispatch_once(&once, ^{
            [[NSNotificationCenter defaultCenter] addObserverForName:MU_FOREGROUND_NOTIFICATION
                                                              object:nil
                                                               queue:[NSOperationQueue mainQueue]
                                                          usingBlock:^(NSNotification*) {
                if (g_awaitingRetry.load() && !g_complete.load())
                {
                    [g_overlay showRetry:NO];
                    MU_IosPreloadBegin();
                }
            }];
        });
    }

#if !TARGET_OS_OSX
    // Asks for the few extra seconds iOS allows after the app is backgrounded.
    g_backgroundTask = [[UIApplication sharedApplication] beginBackgroundTaskWithName:@"MuDataSync" expirationHandler:^{
        [[UIApplication sharedApplication] endBackgroundTask:g_backgroundTask];
        g_backgroundTask = UIBackgroundTaskInvalid;
    }];
#endif

    std::thread(RunPreloadFlow).detach();
    return false;
}

bool MU_IosPreloadIsComplete()
{
    return g_complete.load();
}
