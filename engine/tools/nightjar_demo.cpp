// nightjar_demo — replay a clip through the full engine and print the report.
//
// Default (--synthetic) generates a self-contained clip where a person lingers
// in view, and fires a LOITERING rule — a temporal condition a closed-vocab
// detector cannot express. Runs on any Mac, no external assets, no model
// (judge requirement G5); Tier 2 is the deterministic ScriptedPredicateVlm.
// The real mtmd worker is swapped in on device (see nightjar_replay_vlm).

#include <cstdint>
#include <cstdio>
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

void write_pgm(const std::string& path, const std::vector<uint8_t>& px) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
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

std::string generate_synthetic_clip() {
    fs::path dir = fs::temp_directory_path() / "nightjar_demo_clip";
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
    emit(true, 90);   // 3s: a person lingers in the zone  -> loitering
    emit(false, 30);  // 1s quiet
    return dir.string();
}

TemporalRule loitering_rule() {
    TemporalRule r;
    r.id = "loiter-backyard";
    r.raw_text = "tell me if someone loiters in the backyard at night";
    r.predicate = "person";
    r.trigger = Trigger::Sustained;
    r.dwell_s = 2;  // demo-scaled (production: 60s+)
    r.zone_id = "any";
    r.time_window = TimeWindow{22 * 60, 6 * 60};
    r.cooldown_s = 120;
    r.actions = {Action{ActionType::Ntfy, "nightjar-demo"}};
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    std::string frames_dir;
    double fps = 30.0;
    int sim_infer_ms = 200;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) frames_dir = argv[++i];
        else if (std::strcmp(argv[i], "--fps") == 0 && i + 1 < argc) fps = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--sim-infer-ms") == 0 && i + 1 < argc) sim_infer_ms = std::atoi(argv[++i]);
    }
    if (frames_dir.empty()) {
        std::printf("generating synthetic clip (person lingers -> loitering)...\n");
        frames_dir = generate_synthetic_clip();
    }

    TemporalRuleEngine rules;
    rules.set_rules({loitering_rule()});

    ScriptedPredicateVlm vlm({{"person", true}}, sim_infer_ms);

    CapturingSink sink;
    Telemetry tel;

    PipelineConfig cfg;
    cfg.predicates = {{"person", "Is there a person in this image? Answer y or n."}};
    Pipeline pipe(cfg, &rules, &vlm, &sink, &tel);
    // Fixed time-of-day (inside the night window) but REAL unix seconds so the
    // loitering dwell timer actually elapses as the clip plays.
    pipe.set_clock([] { return Clock{23 * 60 + 42, static_cast<int64_t>(std::time(nullptr))}; });

    FileReplaySource source(ReplayConfig{frames_dir, fps, false});
    std::printf("replaying %zu frames at %.0f fps through the full pipeline...\n\n",
                source.frames_loaded(), fps);

    pipe.start();
    source.start([&](const FrameView& f) { pipe.on_frame(f); });
    source.wait();
    pipe.stop();

    for (const Alert& a : sink.alerts())
        std::printf("ALERT [%s] %s\n", a.rule_id.c_str(), a.one_liner.c_str());
    std::printf("\nframes delivered=%llu dropped=%llu\n\n",
                static_cast<unsigned long long>(source.frames_delivered()),
                static_cast<unsigned long long>(source.frames_dropped()));
    std::printf("%s\n", tel.make_report().to_markdown().c_str());
    return sink.count() > 0 ? 0 : 1;
}
