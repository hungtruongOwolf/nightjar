#import "NightjarEngine.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <memory>
#include <thread>
#include <vector>

#include "nightjar/alert_sink.h"
#include "nightjar/motion_gate.h"
#include "nightjar/pipeline.h"
#include "nightjar/predicate_vlm.h"
#include "nightjar/telemetry.h"
#include "nightjar/temporal_rule.h"
#include "nightjar/temporal_rule_engine.h"

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

TemporalRule rule_for(const std::string& trigger) {
    TemporalRule r;
    r.predicate = "person";
    r.zone_id = "any";
    r.time_window = TimeWindow{0, 24 * 60};  // any time (real camera is tested by day)
    r.cooldown_s = 8;
    r.actions = {Action{ActionType::Ntfy, "nightjar"}};
    if (trigger == "loiter") {
        r.id = "loiter";
        r.raw_text = "tell me if someone loiters in the backyard";
        r.trigger = Trigger::Sustained;
        r.dwell_s = 3;
    } else {
        r.id = "appears";
        r.raw_text = "notify me if a person appears in the backyard";
        r.trigger = Trigger::Appears;
    }
    return r;
}

PipelineConfig make_cfg() {
    PipelineConfig cfg;
    cfg.predicates = {{"person", "Is there a person in this image? Answer y or n."}};
    return cfg;
}

// Owns the whole engine, wired once. Destruction order matters: pipe (declared
// last) is torn down first, before the objects it points at.
struct LiveEngine {
    TemporalRuleEngine rules;
    ScriptedPredicateVlm vlm;
    BlockSink sink;
    Telemetry tel;
    MotionGate overlay_gate;
    Pipeline pipe;

    LiveEngine(const std::string& trig, void (^onAlert)(NSString*))
        : vlm({{"person", true}}, 120),
          sink(onAlert),
          overlay_gate(GateConfig{}),
          pipe(make_cfg(), &rules, &vlm, &sink, &tel) {
        rules.set_rules({rule_for(trig)});
        pipe.set_clock([] { return Clock{12 * 60, (int64_t)std::time(nullptr)}; });
        pipe.start();
    }
    ~LiveEngine() { pipe.stop(); }

    NJStats process(const FrameView& fv) {
        GateResult g = overlay_gate.evaluate(fv);
        pipe.on_frame(fv);
        Report rep = tel.make_report();
        auto seg = [&](Segment s) { return rep.segments[(size_t)s]; };
        auto cnt = [&](Counter c) { return rep.counters[(size_t)c]; };
        int captured = (int)cnt(Counter::FramesCaptured);
        int gated = (int)cnt(Counter::FramesGated);
        NJStats st{};
        st.framesProcessed = captured;
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
CGImageRef make_gray_image(const std::vector<uint8_t>& px) {
    CFDataRef data = CFDataCreate(nullptr, px.data(), px.size());
    CGDataProviderRef p = CGDataProviderCreateWithCFData(data);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceGray();
    CGImageRef img = CGImageCreate(SW, SH, 8, 8, SW, cs, kCGImageAlphaNone, p, nullptr, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(cs); CGDataProviderRelease(p); CFRelease(data);
    return img;
}

}  // namespace

@implementation NightjarEngine {
    std::unique_ptr<LiveEngine> _engine;
    std::atomic<bool> _running;
    std::thread _loop;
    uint64_t _seq;
    void (^_onStats)(NJStats);
}

- (void)startCameraWithTrigger:(NSString*)trigger
                       onStats:(void (^)(NJStats))onStats
                       onAlert:(void (^)(NSString*))onAlert {
    [self stop];
    _engine = std::make_unique<LiveEngine>(trigger ? trigger.UTF8String : "appears", onAlert);
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
    CVPixelBufferUnlockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
    void (^cb)(NJStats) = _onStats;
    if (cb) dispatch_async(dispatch_get_main_queue(), ^{ cb(st); });
}

- (void)startSyntheticWithTrigger:(NSString*)trigger
                          onFrame:(void (^)(CGImageRef, NJStats))onFrame
                          onAlert:(void (^)(NSString*))onAlert {
    [self stop];
    _engine = std::make_unique<LiveEngine>(trigger ? trigger.UTF8String : "appears", onAlert);
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

- (void)stop {
    if (_running.exchange(false) && _loop.joinable()) _loop.join();
    _onStats = nil;
    _engine.reset();
}

- (void)dealloc { [self stop]; }

@end
