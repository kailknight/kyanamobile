// First-launch data.zip download for iOS: the networking and UI half of
// android/.../PreloadActivity.java (the file-system half is IosPreloadCore.cpp).
//
// Android runs this as its own activity before the game's native activity
// starts. On iOS sokol_app owns startup, so the same screen is a UIKit overlay
// laid over the game view while the game's init waits (android_main.cpp's
// g_IosPreloadPending).

#import <UIKit/UIKit.h>
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
UIBackgroundTaskIdentifier g_backgroundTask = UIBackgroundTaskInvalid;

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
}

// ---------------------------------------------------------------------------
// Download screen (Android: PreloadActivity.buildUi)
// ---------------------------------------------------------------------------

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
        NSString* path = [[NSBundle mainBundle] pathForResource:name ofType:@"png"];
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
    _title.attributedText = [[NSAttributedString alloc] initWithString:@"MU DATA SYNC"
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
    UIImage* next = _banners[_bannerIndex];
    [UIView animateWithDuration:0.22
        animations:^{ self->_banner.alpha = 0.08; }
        completion:^(BOOL) {
            self->_banner.image = next;
            [UIView animateWithDuration:0.26 animations:^{ self->_banner.alpha = 1.0; }];
        }];
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
    if (stage) _stage.text = stage;
    if (detail) _detail.text = detail;
    if (timer) _timer.text = timer;
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

namespace
{
MuPreloadOverlay* g_overlay = nil;

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
    std::string error;
};

HttpResult RunDataTask(NSURLRequest* request)
{
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block HttpResult result;
    NSURLSessionDataTask* task = [Session() dataTaskWithRequest:request
        completionHandler:^(NSData*, NSURLResponse* response, NSError* error) {
            if (error)
            {
                result.error = error.localizedDescription.UTF8String ?: "network error";
            }
            else if ([response isKindOfClass:[NSHTTPURLResponse class]])
            {
                result.response = (NSHTTPURLResponse*)response;
                result.status = result.response.statusCode;
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
                failure = taskError.localizedDescription.UTF8String ?: "network error";
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
    PostUi(withAuth ? @"Connecting with Basic Auth..." : @"Connecting to download host...",
           kDataZipUrl, withAuth ? @"Auth mode: basic" : @"Auth mode: none", -1);

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
    dispatch_async(dispatch_get_main_queue(), ^{
        if (g_backgroundTask != UIBackgroundTaskInvalid)
        {
            [[UIApplication sharedApplication] endBackgroundTask:g_backgroundTask];
            g_backgroundTask = UIBackgroundTaskInvalid;
        }
    });
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
    // 1.0.28 (29) crashed reading the dead stack).
    NSString* detail = NS(message);
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
        Log("remote data.zip changed, re-downloading. stored=%s remote=%s", stored.c_str(), remote.c_str());
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
            Finish(@"Sync failed, using existing data...", NS(error), @"Launching with local data", 0.45);
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
            Finish(@"Sync failed, using existing data...", NS(error), @"Launching with local data", 0.45);
            return;
        }
        Fail(error);
        return;
    }
    const long long parseMs = NowMs() - parseStartMs;

    dataDir = FindChildIgnoreCase(root, kDataFolderName, true);
    WriteReadyMarker(root, dataDir, FetchRemoteDataSignature());
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
        UIWindow* window = FindHostWindow();
        g_overlay = [[MuPreloadOverlay alloc] initWithFrame:window ? window.bounds : [UIScreen mainScreen].bounds];
        [window addSubview:g_overlay];

        // A failed download is retried by itself when the player comes back to
        // the app, which is the usual way a transfer dies on a phone.
        static dispatch_once_t once;
        dispatch_once(&once, ^{
            [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationWillEnterForegroundNotification
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

    // Asks for the few extra seconds iOS allows after the app is backgrounded.
    g_backgroundTask = [[UIApplication sharedApplication] beginBackgroundTaskWithName:@"MuDataSync" expirationHandler:^{
        [[UIApplication sharedApplication] endBackgroundTask:g_backgroundTask];
        g_backgroundTask = UIBackgroundTaskInvalid;
    }];

    std::thread(RunPreloadFlow).detach();
    return false;
}

bool MU_IosPreloadIsComplete()
{
    return g_complete.load();
}
