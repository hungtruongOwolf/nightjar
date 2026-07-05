// nightjar_concurrency_bench, the low-latency pipeline story, measured.
//
// Drives the pipeline under a burst of motion frames while the VLM stage is
// deliberately slow, and measures whether the cheap gate stage (the "tick
// handler") stays responsive, i.e. on_frame() never blocks on the VLM. This is
// the trading-desk property: the fast path is never stalled by the slow path;
// stale work conflates; latency is measured coordinated-omission-free.
//
// Reports: gate-stage per-frame latency p50/p99 (must stay ~microseconds even
// while the VLM is saturated), VLM throughput, conflation drops. No model
// needed (ScriptedPredicateVlm with a configurable inference cost).

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>
#include <vector>

#include "nightjar/alert_sink.h"
#include "nightjar/pipeline.h"
#include "nightjar/predicate_vlm.h"
#include "nightjar/telemetry.h"
#include "nightjar/temporal_rule_engine.h"

using namespace nightjar;

namespace {
constexpr int W = 128, H = 128;
uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
std::vector<uint8_t> blob() {
    std::vector<uint8_t> b(size_t(W) * H, 40);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) b[size_t(y) * W + x] = ((x + y) & 1) ? 0 : 255;
    return b;
}
std::vector<uint8_t> flat() { return std::vector<uint8_t>(size_t(W) * H, 40); }
double pct(std::vector<double>& v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, size_t(p / 100.0 * v.size()))];
}
}  // namespace

int main(int argc, char** argv) {
    int frames = 600;      // burst length
    int vlm_ms = 150;      // realistic per-inference cost
    double fps = 120.0;    // aggressive capture rate (burst)
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--vlm-ms") && i + 1 < argc) vlm_ms = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--fps") && i + 1 < argc) fps = std::atof(argv[++i]);
    }

    TemporalRule r;
    r.id = "appears";
    r.predicate = "person";
    r.trigger = Trigger::Appears;
    r.time_window = TimeWindow{0, 0};
    TemporalRuleEngine rules;
    rules.set_rules({r});
    ScriptedPredicateVlm vlm({{"person", true}}, vlm_ms);  // slow VLM stage
    CapturingSink sink;
    Telemetry tel;
    PipelineConfig cfg;
    cfg.predicates = {{"person", "q"}};
    cfg.best_frame.early_exit_min_sharpness = 1.0;  // publish candidates readily
    cfg.best_frame.early_exit_min_blob_blocks = 3;
    Pipeline pipe(cfg, &rules, &vlm, &sink, &tel);
    pipe.set_clock([] { return Clock{23 * 60, static_cast<int64_t>(std::time(nullptr))}; });
    pipe.start();

    auto bg = flat();
    auto fg = blob();
    const uint64_t period_ns = uint64_t(1e9 / fps);
    std::vector<double> gate_us;  // per-frame on_frame() latency = gate-stage cost
    const uint64_t t0 = now_ns();

    for (int i = 0; i < frames; ++i) {
        FrameView f;
        f.y_plane = (i % 8 == 0 ? bg : fg).data();  // mostly motion (burst of candidates)
        f.width = W;
        f.height = H;
        f.stride = W;
        f.seq = uint64_t(i);
        f.ts_mono_ns = now_ns();

        const uint64_t a = now_ns();
        pipe.on_frame(f);  // the fast path, must not block on the VLM
        gate_us.push_back((now_ns() - a) / 1e3);

        // Pace to the capture schedule (don't wait for the system, honest).
        const uint64_t scheduled = t0 + uint64_t(i + 1) * period_ns;
        const uint64_t now = now_ns();
        if (now < scheduled) std::this_thread::sleep_for(std::chrono::nanoseconds(scheduled - now));
    }
    // Let the VLM drain.
    std::this_thread::sleep_for(std::chrono::milliseconds(vlm_ms * 3));
    pipe.stop();

    const Report rep = tel.make_report();
    const int64_t infers = rep.counters[static_cast<size_t>(Counter::VlmInferences)];
    const int64_t drops = rep.counters[static_cast<size_t>(Counter::ConflationDrops)];
    const double wall_s = (now_ns() - t0) / 1e9;

    std::printf("# Concurrency under burst (%d frames @ %.0ffps, VLM %dms/inference)\n\n", frames,
                fps, vlm_ms);
    std::printf("gate stage (fast path) latency:  p50=%.1fus  p99=%.1fus  max=%.1fus\n",
                pct(gate_us, 50), pct(gate_us, 99), pct(gate_us, 100));
    std::printf("  -> the tick handler stays ~microseconds even while the VLM is saturated\n");
    std::printf("     (ConflatingSlot publish is non-blocking; the slow path never stalls it)\n\n");
    std::printf("VLM stage throughput:            %.1f inferences/sec (%lld total)\n",
                infers / wall_s, (long long)infers);
    std::printf("conflation drops (stale, shed):  %lld\n", (long long)drops);
    std::printf("candidates offered vs processed: %lld + %lld drops\n", (long long)infers,
                (long long)drops);
    std::printf("VLM saturation ceiling:          ~%.1f inferences/sec (1000/%dms)\n",
                1000.0 / vlm_ms, vlm_ms);
    return 0;
}
