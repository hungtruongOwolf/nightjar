#import "NightjarEngine.h"

#import <TargetConditionals.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>  // NSValue.pointValue
#else
#import <UIKit/UIKit.h>     // NSValue.CGPointValue
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "nightjar/alert_sink.h"
#include "nightjar/motion_gate.h"
#include "nightjar/pipeline.h"
#include "nightjar/predicate_vlm.h"
#include "nightjar/telemetry.h"
#include "nightjar/temporal_rule.h"
#include "nightjar/temporal_rule_engine.h"

#if NIGHTJAR_HAS_VLM
#include "mtmd_vlm_worker.h"  // real SmolVLM Tier-2 (Mac target; links llama.cpp/mtmd)
#endif

using namespace nightjar;

namespace {

using steady = std::chrono::steady_clock;
uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(steady::now().time_since_epoch()).count();
}

// Alert sink -> UI callback on the main queue.
class BlockSink : public IAlertSink {
public:
    explicit BlockSink(void (^cb)(NSString*)) : cb_(cb) {}
    void send(const Alert& a) override {
        NSString* s = [NSString stringWithUTF8String:a.one_liner.c_str()];
        void (^cb)(NSString*) = cb_;
        dispatch_async(dispatch_get_main_queue(), ^{ cb(s); });
    }
private:
    void (^cb_)(NSString*);
};

TemporalRule rule_for(const std::string& subject, const std::string& trigger) {
    TemporalRule r;
    r.predicate = subject;
    r.zone_id = "any";
    r.time_window = TimeWindow{0, 24 * 60};  // any time (real camera is tested by day)
    r.cooldown_s = 8;
    r.actions = {Action{ActionType::Ntfy, "nightjar"}};
    if (trigger == "loiter") {
        r.id = "loiter";
        r.raw_text = "tell me if a " + subject + " loiters in the backyard";
        r.trigger = Trigger::Sustained;
        r.dwell_s = 3;
    } else {
        r.id = "appears";
        r.raw_text = "notify me if a " + subject + " appears in the backyard";
        r.trigger = Trigger::Appears;
    }
    return r;
}

PipelineConfig make_cfg(const std::string& subject) {
    PipelineConfig cfg;
    cfg.predicates = {{subject, "Is there a " + subject + " in this image? Answer y or n."}};
    return cfg;
}

using ZonePoly = std::vector<std::pair<float, float>>;

bool point_in_poly(float x, float y, const ZonePoly& p) {
    bool in = false;
    for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) {
        float xi = p[i].first, yi = p[i].second, xj = p[j].first, yj = p[j].second;
        if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) in = !in;
    }
    return in;
}

// Rasterize a normalized polygon to the gate's block grid (block=16).
BlockBitmap rasterize_zone(const ZonePoly& poly, int w, int h) {
    const int block = 16, gw = w / block, gh = h / block;
    BlockBitmap m((size_t)gw * gh, 0);
    for (int gy = 0; gy < gh; ++gy)
        for (int gx = 0; gx < gw; ++gx) {
            float cx = (gx + 0.5f) * block / w, cy = (gy + 0.5f) * block / h;
            m[(size_t)gy * gw + gx] = point_in_poly(cx, cy, poly) ? 1 : 0;
        }
    return m;
}

// Owns the whole engine, wired once. Destruction order matters: pipe (declared
// last) is torn down first, before the objects it points at.
struct LiveEngine {
    TemporalRuleEngine rules;
    BlockSink sink;
    Telemetry tel;
    MotionGate overlay_gate;
    ZonePoly zone;
    int zgw = -1, zgh = -1;
    Pipeline pipe;

    // vlm is borrowed (owned by the bridge, reused across guard sessions so the
    // model loads only once).
    LiveEngine(const std::string& subject, const std::string& trig, IPredicateVlm* vlm,
               void (^onAlert)(NSString*), ZonePoly z)
        : sink(onAlert),
          overlay_gate(GateConfig{}),
          zone(std::move(z)),
          pipe(make_cfg(subject), &rules, vlm, &sink, &tel) {
        rules.set_rules({rule_for(subject, trig)});
        pipe.set_clock([] { return Clock{12 * 60, (int64_t)std::time(nullptr)}; });
        pipe.start();
    }
    ~LiveEngine() { pipe.stop(); }

    NJStats process(const FrameView& fv) {
        if (zone.size() >= 3) {
            int gw = fv.width / 16, gh = fv.height / 16;
            if (gw != zgw || gh != zgh) {  // rasterize once per frame geometry
                zgw = gw; zgh = gh;
                BlockBitmap m = rasterize_zone(zone, fv.width, fv.height);
                overlay_gate.set_zone_mask(m);
                pipe.set_zone_mask(m);
            }
        }
        GateResult g = overlay_gate.evaluate(fv);
        pipe.on_frame(fv);
        Report rep = tel.make_report();
        auto seg = [&](Segment s) { return rep.segments[(size_t)s]; };
        auto cnt = [&](Counter c) { return rep.counters[(size_t)c]; };
        int captured = (int)cnt(Counter::FramesCaptured);
        int gated = (int)cnt(Counter::FramesGated);
        NJStats st{};
        st.framesProcessed = captured;
        st.framesGated = gated;
        st.framesSkippedPct = captured > 0 ? (int)std::lround(100.0 * (captured - gated) / captured) : 0;
        st.vlmChecks = (int)cnt(Counter::VlmInferences);
        st.conflationDrops = (int)cnt(Counter::ConflationDrops);
        st.eventToAlertMs = seg(Segment::EndToEnd).p50;
        st.gateMs = seg(Segment::GateCost).p50;
        st.gateP99Ms = seg(Segment::GateCost).p99;
        st.motion = g.motion;
        st.motionRect = g.motion ? CGRectMake((double)g.blob_bbox.x / fv.width, (double)g.blob_bbox.y / fv.height,
                                              (double)g.blob_bbox.w / fv.width, (double)g.blob_bbox.h / fv.height)
                                 : CGRectZero;
        return st;
    }
};

// ── synthetic night scene (Simulator only) ──
constexpr int SW = 384, SH = 288;
inline int grain(int x, int y, int f) {
    uint32_t h = uint32_t(x) * 73856093u ^ uint32_t(y) * 19349663u ^ uint32_t(f) * 83492791u;
    return int(h % 7u) - 3;
}
void disc(std::vector<uint8_t>& px, int cx, int cy, int r, uint8_t v) {
    for (int y = cy - r; y <= cy + r; ++y) for (int x = cx - r; x <= cx + r; ++x)
        if (x >= 0 && x < SW && y >= 0 && y < SH && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) px[y * SW + x] = v;
}
void rect(std::vector<uint8_t>& px, int x0, int y0, int w, int h, uint8_t v) {
    for (int y = y0; y < y0 + h; ++y) for (int x = x0; x < x0 + w; ++x)
        if (x >= 0 && x < SW && y >= 0 && y < SH) px[y * SW + x] = v;
}
void render_scene(std::vector<uint8_t>& px, double t, int frame) {
    for (int y = 0; y < SH; ++y) { uint8_t b = y > SH / 2 ? uint8_t(26 + (y - SH / 2) * 26 / (SH / 2)) : 26;
        for (int x = 0; x < SW; ++x) px[y * SW + x] = b; }
    rect(px, 0, SH / 2, SW, 2, 60);
    for (int x = 20; x < SW; x += 64) rect(px, x, SH / 2 - 34, 5, 34, 66);
    if (t >= 2.0 && t <= 12.0) {
        double nx = t < 5.0 ? 0.85 - (t - 2.0) / 3.0 * 0.35 : (t < 10.0 ? 0.50 : 0.50 - (t - 10.0) / 2.0 * 0.35);
        nx += std::sin(t * 2.3) * 0.006;
        int cx = int(nx * SW), feet = int(SH * 0.88), hr = 15, top = feet - 118;
        disc(px, cx, top + hr, hr, 128);
        rect(px, cx - 19, top + hr * 2, 38, 62, 120);
        rect(px, cx - 15, top + hr * 2 + 62, 12, 40, 118);
        rect(px, cx + 3, top + hr * 2 + 62, 12, 40, 118);
    }
    for (int y = 0; y < SH; ++y) for (int x = 0; x < SW; ++x) {
        int v = px[y * SW + x] + grain(x, y, frame);
        px[y * SW + x] = uint8_t(v < 0 ? 0 : (v > 255 ? 255 : v));
    }
}
CGImageRef make_gray_image_wh(const uint8_t* px, int w, int h) {
    CFDataRef data = CFDataCreate(nullptr, px, (CFIndex)w * h);
    CGDataProviderRef p = CGDataProviderCreateWithCFData(data);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceGray();
    CGImageRef img = CGImageCreate(w, h, 8, 8, w, cs, kCGImageAlphaNone, p, nullptr, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(cs); CGDataProviderRelease(p); CFRelease(data);
    return img;
}
CGImageRef make_gray_image(const std::vector<uint8_t>& px) { return make_gray_image_wh(px.data(), SW, SH); }

BOOL contains_any(NSString* s, NSArray<NSString*>* keys) {
    for (NSString* k in keys) if ([s containsString:k]) return YES;
    return NO;
}

}  // namespace

@implementation NJParsedRule
@end

@implementation NightjarEngine {
    std::unique_ptr<LiveEngine> _engine;
    std::atomic<bool> _running;
    std::thread _loop;
    uint64_t _seq;
    void (^_onStats)(NJStats);
    ZonePoly _zone;
    std::string _subject;
    std::shared_ptr<IPredicateVlm> _vlm;  // loaded once, reused across sessions
    NSString* _vlmName;
    std::mutex _frameMu;
    std::vector<uint8_t> _lastY;  // most recent frame (packed), for alert snapshots
    int _lastW;
    int _lastH;
    std::deque<std::vector<uint8_t>> _ring;  // recent frames for event clips
}

- (CGImageRef)currentSnapshotCopy {
    std::lock_guard<std::mutex> lk(_frameMu);
    if (_lastY.empty()) return nullptr;
    return make_gray_image_wh(_lastY.data(), _lastW, _lastH);
}

- (NSArray*)recentClipFrames {
    std::lock_guard<std::mutex> lk(_frameMu);
    NSMutableArray* out = [NSMutableArray array];
    if (_ring.empty() || _lastW == 0) return out;
    // subsample to ~24 frames across the ring
    const int want = 24;
    const int n = (int)_ring.size();
    const int step = n > want ? n / want : 1;
    for (int i = 0; i < n; i += step) {
        CGImageRef img = make_gray_image_wh(_ring[(size_t)i].data(), _lastW, _lastH);
        [out addObject:(__bridge id)img];
        CGImageRelease(img);  // array retains
    }
    return out;
}

- (void)setSubject:(NSString*)subjectKey {
    _subject = subjectKey.length ? subjectKey.UTF8String : "person";
}

- (NSString*)tier2Name {
    return _vlmName ?: @"scripted";
}

// Load the Tier-2 VLM once: the real SmolVLM (mtmd) when the model files and
// llama.cpp are available (Mac), otherwise the scripted stand-in.
- (void)ensureVlm {
    if (_vlm) return;
#if NIGHTJAR_HAS_VLM
    NSArray<NSString*>* dirs = @[
        [[NSBundle mainBundle] resourcePath] ?: @"",
        [NSString stringWithUTF8String:(getenv("NIGHTJAR_MODELS") ?: "")],
        @"/Users/hung.truong/Claude/Hackathon/nightjar/models",
    ];
    for (NSString* d in dirs) {
        if (!d.length) continue;
        NSString* model = [d stringByAppendingPathComponent:@"SmolVLM-500M-Instruct-Q4_0.gguf"];
        NSString* mmproj = [d stringByAppendingPathComponent:@"mmproj-SmolVLM-500M-Instruct-f16.gguf"];
        NSFileManager* fm = [NSFileManager defaultManager];
        if (![fm fileExistsAtPath:model] || ![fm fileExistsAtPath:mmproj]) continue;
        MtmdConfig cfg;
        cfg.model_path = model.UTF8String;
        cfg.mmproj_path = mmproj.UTF8String;
        cfg.encoder_use_gpu = true;
        auto worker = std::make_shared<MtmdVlmWorker>(cfg);
        if (worker->ok()) { _vlm = worker; _vlmName = @"SmolVLM-500M (INT4)"; return; }
    }
#endif
    _vlm = std::make_shared<ScriptedPredicateVlm>(std::map<std::string, bool>{{"person", true}}, 120);
    _vlmName = @"scripted";
}

- (void)setZonePolygon:(NSArray<NSValue*>*)points {
    _zone.clear();
    for (NSValue* v in points) {
#if TARGET_OS_OSX
        NSPoint p = v.pointValue;
#else
        CGPoint p = v.CGPointValue;
#endif
        _zone.emplace_back((float)p.x, (float)p.y);
    }
}

- (void)startCameraWithTrigger:(NSString*)trigger
                       onStats:(void (^)(NJStats))onStats
                       onAlert:(void (^)(NSString*))onAlert {
    [self stop];
    if (_subject.empty()) _subject = "person";
    [self ensureVlm];
    _engine = std::make_unique<LiveEngine>(_subject, trigger ? trigger.UTF8String : "appears", _vlm.get(), onAlert, _zone);
    _onStats = [onStats copy];
    _seq = 0;
}

- (void)submitPixelBuffer:(CVPixelBufferRef)pb {
    if (!_engine) return;
    CVPixelBufferLockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
    FrameView fv;
    fv.y_plane = (const uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pb, 0);  // luma plane
    fv.width = (int)CVPixelBufferGetWidthOfPlane(pb, 0);
    fv.height = (int)CVPixelBufferGetHeightOfPlane(pb, 0);
    fv.stride = (int)CVPixelBufferGetBytesPerRowOfPlane(pb, 0);
    fv.ts_mono_ns = now_ns();
    fv.seq = _seq++;
    NJStats st = _engine->process(fv);
    {  // keep a packed copy (snapshot) + a rolling ring (clip)
        std::lock_guard<std::mutex> lk(_frameMu);
        _lastW = fv.width; _lastH = fv.height;
        _lastY.resize((size_t)fv.width * fv.height);
        for (int y = 0; y < fv.height; ++y)
            std::memcpy(&_lastY[(size_t)y * fv.width], fv.y_plane + (size_t)y * fv.stride, fv.width);
        _ring.push_back(_lastY);
        while (_ring.size() > 60) _ring.pop_front();  // ~2s at 30fps
    }
    CVPixelBufferUnlockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
    void (^cb)(NJStats) = _onStats;
    if (cb) dispatch_async(dispatch_get_main_queue(), ^{ cb(st); });
}

- (void)startSyntheticWithTrigger:(NSString*)trigger
                          onFrame:(void (^)(CGImageRef, NJStats))onFrame
                          onAlert:(void (^)(NSString*))onAlert {
    [self stop];
    if (_subject.empty()) _subject = "person";
    [self ensureVlm];
    _engine = std::make_unique<LiveEngine>(_subject, trigger ? trigger.UTF8String : "appears", _vlm.get(), onAlert, _zone);
    _running = true;
    std::atomic<bool>* running = &_running;
    LiveEngine* eng = _engine.get();
    _loop = std::thread([running, eng, onFrame] {
        std::vector<uint8_t> px(size_t(SW) * SH);
        const auto t0 = steady::now();
        int frame = 0;
        while (running->load()) {
            double loop_t = std::fmod(std::chrono::duration<double>(steady::now() - t0).count(), 14.0);
            render_scene(px, loop_t, frame);
            FrameView fv;
            fv.y_plane = px.data(); fv.width = SW; fv.height = SH; fv.stride = SW;
            fv.seq = frame; fv.ts_mono_ns = now_ns();
            NJStats st = eng->process(fv);
            CGImageRef img = make_gray_image(px);
            dispatch_async(dispatch_get_main_queue(), ^{ onFrame(img, st); CGImageRelease(img); });
            ++frame;
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    });
}

+ (NJParsedRule*)compileRule:(NSString*)english {
    NSString* s = [(english ?: @"") lowercaseString];
    NJParsedRule* r = [NJParsedRule new];

    NSString* who = @"A person"; NSString* subjectKey = @"person";
    if (contains_any(s, @[@"car", @"vehicle", @"truck", @"van"])) { who = @"A vehicle"; subjectKey = @"vehicle"; }
    else if (contains_any(s, @[@"animal", @"dog", @"cat", @"fox", @"raccoon", @"coyote"])) { who = @"An animal"; subjectKey = @"animal"; }
    else if (contains_any(s, @[@"package", @"parcel", @"delivery", @"box", @"mail"])) { who = @"A package"; subjectKey = @"package"; }

    NSString* trig = contains_any(s, @[@"loiter", @"linger", @"hang around", @"hangs around", @"stays",
                                       @"waiting", @"waits", @"lurk", @"stand around"]) ? @"loiter" : @"appears";

    NSString* where = @"The area";
    NSArray* zoneKeys = @[@"backyard", @"back yard", @"front door", @"doorstep", @"porch", @"driveway",
                          @"garage", @"gate", @"garden", @"window", @"mailbox", @"kitchen counter",
                          @"counter", @"yard", @"street"];
    NSDictionary* zoneName = @{@"backyard": @"Backyard", @"back yard": @"Backyard", @"front door": @"Front door",
                               @"doorstep": @"Front door", @"porch": @"Porch", @"driveway": @"Driveway",
                               @"garage": @"Garage", @"gate": @"Gate", @"garden": @"Garden", @"window": @"Window",
                               @"mailbox": @"Mailbox", @"kitchen counter": @"Kitchen counter", @"counter": @"Kitchen counter",
                               @"yard": @"Yard", @"street": @"Street"};
    for (NSString* k in zoneKeys) if ([s containsString:k]) { where = zoneName[k]; break; }

    NSString* when = @"Anytime";
    if (contains_any(s, @[@"night", @"after dark", @"overnight"])) when = @"10 PM – 6 AM";
    NSRegularExpression* re = [NSRegularExpression regularExpressionWithPattern:@"after\\s*(\\d{1,2})\\s*(am|pm)"
                                                                       options:NSRegularExpressionCaseInsensitive error:nil];
    NSTextCheckingResult* m = [re firstMatchInString:s options:0 range:NSMakeRange(0, s.length)];
    if (m) when = [NSString stringWithFormat:@"After %@ %@", [s substringWithRange:[m rangeAtIndex:1]],
                                             [[s substringWithRange:[m rangeAtIndex:2]] uppercaseString]];

    r.who = who;
    r.subjectKey = subjectKey;
    r.where = where;
    r.when = when;
    r.then = @"Ping your phone + photo";
    r.trigger = trig;
    NSString* verb = [trig isEqualToString:@"loiter"] ? @"loitering" : @"appears";
    r.title = [NSString stringWithFormat:@"%@ %@ · %@", who, verb, where];
    return r;
}

- (void)stop {
    if (_running.exchange(false) && _loop.joinable()) _loop.join();
    _onStats = nil;
    _engine.reset();
}

- (void)dealloc { [self stop]; }

@end
