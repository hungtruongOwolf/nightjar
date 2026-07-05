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

constexpr int W = 384, H = 288;
using steady = std::chrono::steady_clock;

uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(steady::now().time_since_epoch())
        .count();
}

// Cheap per-pixel hash for stable film-grain noise (below the gate threshold, so
// it reads as a live sensor without tripping motion).
inline int grain(int x, int y, int f) {
    uint32_t h = uint32_t(x) * 73856093u ^ uint32_t(y) * 19349663u ^ uint32_t(f) * 83492791u;
    return int(h % 7u) - 3;  // [-3, +3]
}

void fill_disc(std::vector<uint8_t>& px, int cx, int cy, int r, uint8_t v) {
    for (int y = cy - r; y <= cy + r; ++y) {
        if (y < 0 || y >= H) continue;
        for (int x = cx - r; x <= cx + r; ++x) {
            if (x < 0 || x >= W) continue;
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) px[y * W + x] = v;
        }
    }
}

void fill_rect(std::vector<uint8_t>& px, int x0, int y0, int w, int h, uint8_t v) {
    for (int y = y0; y < y0 + h; ++y) {
        if (y < 0 || y >= H) continue;
        for (int x = x0; x < x0 + w; ++x) {
            if (x < 0 || x >= W) continue;
            px[y * W + x] = v;
        }
    }
}

// Render one frame of the night scene at loop-time t (seconds). A person enters
// from the right, lingers near the centre (with a small sway so a standing
// figure still registers as live motion), then leaves. Returns the person's
// horizontal centre in [0,1] or -1 when absent.
void render_scene(std::vector<uint8_t>& px, double t, int frame) {
    // ground gradient + horizon + fence posts (static backdrop)
    for (int y = 0; y < H; ++y) {
        uint8_t base = 26;
        if (y > H / 2) base = uint8_t(26 + (y - H / 2) * 26 / (H / 2));
        for (int x = 0; x < W; ++x) px[y * W + x] = base;
    }
    fill_rect(px, 0, H / 2, W, 2, 60);                       // horizon
    for (int x = 20; x < W; x += 64) fill_rect(px, x, H / 2 - 34, 5, 34, 66);  // posts

    // person
    double cx = -1;
    if (t >= 2.0 && t <= 12.0) {
        double nx;  // normalized x path, right -> centre -> left
        if (t < 5.0)
            nx = 0.85 - (t - 2.0) / 3.0 * 0.35;   // enter
        else if (t < 10.0)
            nx = 0.50;                            // linger
        else
            nx = 0.50 - (t - 10.0) / 2.0 * 0.35;  // leave
        nx += std::sin(t * 2.3) * 0.006;          // sway keeps a standing figure "moving"
        cx = nx;
        int px_cx = int(nx * W);
        int feet = int(H * 0.88);
        int head_r = 15, top = feet - 118;
        fill_disc(px, px_cx, top + head_r, head_r, 128);              // head
        fill_rect(px, px_cx - 19, top + head_r * 2, 38, 62, 120);     // torso
        fill_rect(px, px_cx - 15, top + head_r * 2 + 62, 12, 40, 118);  // leg
        fill_rect(px, px_cx + 3, top + head_r * 2 + 62, 12, 40, 118);   // leg
    }

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int v = px[y * W + x] + grain(x, y, frame);
            px[y * W + x] = uint8_t(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
    (void)cx;
}

// Alert sink that forwards to a UI callback on the main queue.
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
    r.time_window = TimeWindow{22 * 60, 6 * 60};
    r.cooldown_s = 10;  // demo cadence: re-arms each scene loop
    r.actions = {Action{ActionType::Ntfy, "nightjar"}};
    if (trigger == "loiter") {
        r.id = "loiter";
        r.raw_text = "tell me if someone loiters in the backyard at night";
        r.trigger = Trigger::Sustained;
        r.dwell_s = 3;
    } else {
        r.id = "appears";
        r.raw_text = "notify me if a person appears in the backyard at night";
        r.trigger = Trigger::Appears;
    }
    return r;
}

CGImageRef make_gray_image(const std::vector<uint8_t>& px) {
    CFDataRef data = CFDataCreate(nullptr, px.data(), px.size());
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceGray();
    CGImageRef img = CGImageCreate(W, H, 8, 8, W, cs, kCGImageAlphaNone, provider, nullptr, false,
                                   kCGRenderingIntentDefault);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(provider);
    CFRelease(data);
    return img;  // caller releases
}

}  // namespace

@implementation NightjarEngine {
    std::atomic<bool> _running;
    std::thread _loop;
}

- (void)startWithTrigger:(NSString*)trigger
                 onFrame:(void (^)(CGImageRef, NJStats))onFrame
                 onAlert:(void (^)(NSString*))onAlert {
    [self stop];
    const std::string trig = trigger ? trigger.UTF8String : "appears";
    _running = true;
    std::atomic<bool>* running = &_running;

    _loop = std::thread([running, trig, onFrame, onAlert] {
        // One engine, wired once, then fed frames in real time.
        auto rules = std::make_shared<TemporalRuleEngine>();
        rules->set_rules({rule_for(trig)});
        ScriptedPredicateVlm vlm({{"person", true}}, 120);
        BlockSink sink(onAlert);
        Telemetry tel;
        PipelineConfig cfg;
        cfg.predicates = {{"person", "Is there a person in this image? Answer y or n."}};
        Pipeline pipe(cfg, rules.get(), &vlm, &sink, &tel);
        pipe.set_clock([] { return Clock{23 * 60 + 42, (int64_t)std::time(nullptr)}; });
        pipe.start();

        MotionGate overlay_gate(GateConfig{});  // independent gate, for the live motion box
        std::vector<uint8_t> px(size_t(W) * H);

        const auto t0 = steady::now();
        int frame = 0;
        while (running->load()) {
            const double loop_t = std::fmod(
                std::chrono::duration<double>(steady::now() - t0).count(), 14.0);
            render_scene(px, loop_t, frame);

            FrameView fv;
            fv.y_plane = px.data();
            fv.width = W;
            fv.height = H;
            fv.stride = W;
            fv.seq = frame;
            fv.ts_mono_ns = now_ns();

            GateResult g = overlay_gate.evaluate(fv);
            pipe.on_frame(fv);

            Report rep = tel.make_report();
            auto seg = [&](Segment s) { return rep.segments[(size_t)s].p50; };
            auto cnt = [&](Counter c) { return rep.counters[(size_t)c]; };
            int captured = (int)cnt(Counter::FramesCaptured);
            int gated = (int)cnt(Counter::FramesGated);

            NJStats st{};
            st.framesProcessed = captured;
            st.framesSkippedPct = captured > 0 ? (int)std::lround(100.0 * (captured - gated) / captured) : 0;
            st.vlmChecks = (int)cnt(Counter::VlmInferences);
            st.conflationDrops = (int)cnt(Counter::ConflationDrops);
            st.eventToAlertMs = seg(Segment::EndToEnd);
            st.gateMs = seg(Segment::GateCost);
            st.motion = g.motion;
            st.motionRect = g.motion ? CGRectMake((double)g.blob_bbox.x / W, (double)g.blob_bbox.y / H,
                                                  (double)g.blob_bbox.w / W, (double)g.blob_bbox.h / H)
                                     : CGRectZero;

            CGImageRef img = make_gray_image(px);  // self-contained (owns a copy of the pixels)
            dispatch_async(dispatch_get_main_queue(), ^{
                onFrame(img, st);      // Swift retains it into @Published as needed
                CGImageRelease(img);   // drop our creating +1
            });

            ++frame;
            std::this_thread::sleep_for(std::chrono::milliseconds(33));  // ~30 fps
        }
        pipe.stop();
    });
}

- (void)stop {
    if (_running.exchange(false) && _loop.joinable()) _loop.join();
}

- (void)dealloc {
    [self stop];
}

@end
