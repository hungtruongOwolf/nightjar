// nightjar_ablation, quantify the two-tier gate's compute (and hence energy)
// reduction: how many VLM inferences the gate + best-frame selector avoid
// versus running the VLM on every captured frame. VLM inferences are a faithful
// energy proxy (each costs ~the same), so this is the core perf-per-watt lever,
// measurable with no model and no sudo. Pass --j-per-infer <J> (from a real
// powermetrics measurement) to also print an energy estimate.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "nightjar/alert_sink.h"
#include "nightjar/file_replay_source.h"
#include "nightjar/pipeline.h"
#include "nightjar/predicate_vlm.h"
#include "nightjar/telemetry.h"
#include "nightjar/temporal_rule_engine.h"

using namespace nightjar;
namespace fs = std::filesystem;

namespace {
constexpr int W = 640, H = 480;
void write_pgm(const std::string& p, const std::vector<uint8_t>& px) {
    std::FILE* f = std::fopen(p.c_str(), "wb");
    std::fprintf(f, "P5\n%d %d\n255\n", W, H);
    std::fwrite(px.data(), 1, px.size(), f);
    std::fclose(f);
}
std::vector<uint8_t> frame(bool person) {
    std::vector<uint8_t> px(size_t(W) * H, 50);
    if (person)
        for (int y = 190; y < 300; ++y)
            for (int x = 280; x < 390; ++x) px[size_t(y) * W + x] = ((x / 3 + y / 3) & 1) ? 0 : 255;
    return px;
}
// A realistic guard clip: mostly quiet, with a couple of brief person events -
// the regime where the gate shines (~1-5% of frames are interesting).
std::string make_clip() {
    fs::path dir = fs::temp_directory_path() / "nightjar_ablation_clip";
    fs::remove_all(dir);
    fs::create_directories(dir);
    int idx = 0;
    auto emit = [&](bool p, int n) {
        auto px = frame(p);
        for (int i = 0; i < n; ++i) {
            char nm[64];
            std::snprintf(nm, sizeof(nm), "frame_%04d.pgm", idx++);
            write_pgm((dir / nm).string(), px);
        }
    };
    emit(false, 300);  // 10s quiet
    emit(true, 60);    // 2s person
    emit(false, 240);  // 8s quiet
    emit(true, 30);    // 1s person
    emit(false, 150);  // 5s quiet
    return dir.string();
}
}  // namespace

int main(int argc, char** argv) {
    double j_per_infer = 0.0;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--j-per-infer") == 0 && i + 1 < argc)
            j_per_infer = std::atof(argv[++i]);

    const std::string dir = make_clip();

    TemporalRule r;
    r.id = "loiter";
    r.predicate = "person";
    r.trigger = Trigger::Sustained;
    r.dwell_s = 2;
    r.time_window = TimeWindow{0, 0};
    TemporalRuleEngine rules;
    rules.set_rules({r});
    ScriptedPredicateVlm vlm({{"person", true}}, /*sim_infer_ms=*/0);
    CapturingSink sink;
    Telemetry tel;
    PipelineConfig cfg;
    cfg.predicates = {{"person", "q"}};
    Pipeline pipe(cfg, &rules, &vlm, &sink, &tel);
    pipe.set_clock([] { return Clock{23 * 60, static_cast<int64_t>(std::time(nullptr))}; });

    FileReplaySource src(ReplayConfig{dir, 120.0, false});  // fast replay (offline compute count)
    pipe.start();
    src.start([&](const FrameView& f) { pipe.on_frame(f); });
    src.wait();
    pipe.stop();

    const Report rep = tel.make_report();
    const int64_t frames = rep.counters[static_cast<size_t>(Counter::FramesCaptured)];
    const int64_t gated_infers = rep.counters[static_cast<size_t>(Counter::VlmInferences)];
    const int64_t baseline_infers = frames;  // VLM-every-frame baseline
    const double reduction = baseline_infers > 0
                                 ? 100.0 * (1.0 - double(gated_infers) / double(baseline_infers))
                                 : 0.0;

    std::printf("# Perf-per-watt: two-tier gate compute ablation\n\n");
    std::printf("clip frames (captured):        %lld\n", (long long)frames);
    std::printf("VLM inferences, every-frame:   %lld  (baseline)\n", (long long)baseline_infers);
    std::printf("VLM inferences, two-tier gate: %lld\n", (long long)gated_infers);
    std::printf("VLM compute avoided by gate:   %.1f%%\n", reduction);
    if (j_per_infer > 0) {
        std::printf("\nwith measured %.3f J/inference:\n", j_per_infer);
        std::printf("  every-frame energy: %.1f J\n", j_per_infer * baseline_infers);
        std::printf("  gated energy:       %.1f J\n", j_per_infer * gated_infers);
        std::printf("  energy saved:       %.1f J (%.1f%%)\n",
                    j_per_infer * (baseline_infers - gated_infers), reduction);
    } else {
        std::printf("\n(pass --j-per-infer <J> from a powermetrics run for the energy estimate)\n");
    }
    return 0;
}
