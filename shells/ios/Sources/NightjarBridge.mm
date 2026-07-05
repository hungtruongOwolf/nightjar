#import "NightjarBridge.h"

#include <sys/sysctl.h>

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "nightjar/alert_sink.h"
#include "nightjar/file_replay_source.h"
#include "nightjar/pipeline.h"
#include "nightjar/predicate_vlm.h"
#include "nightjar/telemetry.h"
#include "nightjar/temporal_rule.h"
#include "nightjar/temporal_rule_engine.h"

using namespace nightjar;
namespace fs = std::filesystem;

namespace {

constexpr int W = 640, H = 480;

void write_pgm(const std::string& path, const std::vector<uint8_t>& px) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P5\n%d %d\n255\n", W, H);
    std::fwrite(px.data(), 1, px.size(), f);
    std::fclose(f);
}

std::vector<uint8_t> make_frame(bool with_person) {
    std::vector<uint8_t> px(size_t(W) * H, 50);
    if (with_person) {
        const int x0 = 280, y0 = 190, size = 110;
        for (int y = y0; y < y0 + size; ++y)
            for (int x = x0; x < x0 + size; ++x)
                px[size_t(y) * W + x] = ((x / 3 + y / 3) & 1) ? 0 : 255;
    }
    return px;
}

// A person enters and lingers, then leaves — enough to fire either an
// "appears" (rising edge) or a "loiter" (sustained dwell) rule.
std::string generate_clip() {
    fs::path dir = fs::path(NSTemporaryDirectory().UTF8String) / "nightjar_clip";
    fs::remove_all(dir);
    fs::create_directories(dir);
    int idx = 0;
    auto emit = [&](bool person, int count) {
        auto px = make_frame(person);
        for (int i = 0; i < count; ++i) {
            char name[64];
            std::snprintf(name, sizeof(name), "frame_%04d.pgm", idx++);
            write_pgm((dir / name).string(), px);
        }
    };
    emit(false, 30);  // 1s quiet
    emit(true, 90);   // 3s: a person lingers
    emit(false, 30);  // 1s quiet
    return dir.string();
}

TemporalRule rule_for(const std::string& trigger) {
    TemporalRule r;
    r.predicate = "person";
    r.zone_id = "any";
    r.time_window = TimeWindow{22 * 60, 6 * 60};  // night window
    r.cooldown_s = 120;
    r.actions = {Action{ActionType::Ntfy, "nightjar"}};
    if (trigger == "loiter") {
        r.id = "loiter-backyard";
        r.raw_text = "tell me if someone loiters in the backyard at night";
        r.trigger = Trigger::Sustained;
        r.dwell_s = 2;  // demo-scaled (production: 60s+)
    } else {
        r.id = "person-appears";
        r.raw_text = "notify me if a person appears in the backyard at night";
        r.trigger = Trigger::Appears;
    }
    return r;
}

bool has_feature(const char* name) {
    int v = 0;
    size_t sz = sizeof(v);
    return sysctlbyname(name, &v, &sz, nullptr, 0) == 0 && v != 0;
}

NSString* isa_line() {
    std::string s;
    s += has_feature("hw.optional.arm.FEAT_DotProd") ? "dotprod " : "";
    s += has_feature("hw.optional.arm.FEAT_I8MM") ? "i8mm " : "";
    s += has_feature("hw.optional.arm.FEAT_FP16") ? "fp16 " : "";
    s += has_feature("hw.optional.arm.FEAT_BF16") ? "bf16 " : "";
    if (s.empty()) s = "(none reported)";
    return [NSString stringWithUTF8String:s.c_str()];
}

}  // namespace

@implementation NightjarResult
@end

@implementation NightjarBridge

+ (NightjarResult*)runGuardDemoWithTrigger:(NSString*)trigger {
    const std::string trig = trigger ? trigger.UTF8String : "appears";

    TemporalRuleEngine rules;
    rules.set_rules({rule_for(trig)});

    // Deterministic Tier-2: person is present in any candidate frame. 60ms
    // simulated inference keeps the demo snappy while still exercising the
    // ConflatingSlot (bursts drop-old, counted honestly).
    ScriptedPredicateVlm vlm({{"person", true}}, 60);

    CapturingSink sink;
    Telemetry tel;

    PipelineConfig cfg;
    cfg.predicates = {{"person", "Is there a person in this image? Answer y or n."}};
    Pipeline pipe(cfg, &rules, &vlm, &sink, &tel);
    // Fixed time-of-day inside the night window; REAL unix seconds so the
    // loitering dwell timer actually elapses as the clip plays.
    pipe.set_clock([] { return Clock{23 * 60 + 42, static_cast<int64_t>(std::time(nullptr))}; });

    const std::string dir = generate_clip();
    FileReplaySource source(ReplayConfig{dir, 30.0, false});

    pipe.start();
    source.start([&](const FrameView& f) { pipe.on_frame(f); });
    source.wait();
    pipe.stop();

    Report rep = tel.make_report();
    auto seg = [&](Segment s) { return rep.segments[static_cast<size_t>(s)].p50; };
    auto cnt = [&](Counter c) { return rep.counters[static_cast<size_t>(c)]; };

    NightjarResult* out = [[NightjarResult alloc] init];
    out.framesProcessed = static_cast<int>(source.frames_delivered());
    out.vlmInferences = static_cast<int>(cnt(Counter::VlmInferences));
    out.conflationDrops = static_cast<int>(cnt(Counter::ConflationDrops));
    out.gateP50us = seg(Segment::GateCost) * 1000.0;   // Telemetry stores ms -> us
    out.e2eP50ms = seg(Segment::EndToEnd);             // already ms
    out.isa = isa_line();

    auto alerts = sink.alerts();
    out.alert = alerts.empty() ? @""
                               : [NSString stringWithUTF8String:alerts.front().one_liner.c_str()];
    return out;
}

@end
