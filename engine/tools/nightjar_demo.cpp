// nightjar_demo — replay a clip through the full engine and print the report.
//
// Default (--synthetic) generates a self-contained clip (a textured intruder
// blob appears in the zone for ~1s), so `make demo` runs on any Mac with no
// external assets or model (judge requirement G5). --frames <dir> replays a
// real PGM sequence instead. The VLM stage is the deterministic ScriptedVlmWorker
// (reports "person"); the real mtmd worker is swapped in on device.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "nightjar/alert_sink.h"
#include "nightjar/file_replay_source.h"
#include "nightjar/pipeline.h"
#include "nightjar/rule_engine.h"
#include "nightjar/telemetry.h"
#include "nightjar/vlm_worker.h"

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

// Background: flat gray. Optionally stamp a textured (checkerboard) blob so the
// gate sees a big change and the selector sees high sharpness.
std::vector<uint8_t> make_frame(bool with_blob) {
    std::vector<uint8_t> px(size_t(W) * H, 50);
    if (with_blob) {
        const int x0 = 280, y0 = 200, size = 96;
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
    auto emit = [&](bool blob, int count) {
        auto px = make_frame(blob);
        for (int i = 0; i < count; ++i) {
            char name[64];
            std::snprintf(name, sizeof(name), "frame_%04d.pgm", idx++);
            write_pgm((dir / name).string(), px);
        }
    };
    emit(false, 30);  // 1s quiet
    emit(true, 30);   // 1s intruder in zone
    emit(false, 30);  // 1s quiet
    return dir.string();
}

Rule backyard_night_rule() {
    Rule r;
    r.id = "backyard-night";
    r.raw_text = "notify me if a person enters the backyard after 10pm";
    r.subject = Subject::Person;
    r.zone_id = "any";  // single-zone demo
    r.time_window = TimeWindow{22 * 60, 6 * 60};  // 22:00-06:00
    r.cooldown_s = 120;
    r.actions = {Action{ActionType::Ntfy, "nightjar-demo"}};
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    std::string frames_dir;
    double fps = 30.0;
    int sim_infer_ms = 400;  // stand-in for the real VLM's ~sub-second inference

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            fps = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--sim-infer-ms") == 0 && i + 1 < argc) {
            sim_infer_ms = std::atoi(argv[++i]);
        }
    }
    if (frames_dir.empty()) {
        std::printf("generating synthetic clip (use --frames <dir> for a real one)...\n");
        frames_dir = generate_synthetic_clip();
    }

    RuleEngine rules;
    rules.set_rules({backyard_night_rule()});

    Facts person;
    person.person = true;
    ScriptedVlmWorker vlm(person, sim_infer_ms);

    CapturingSink sink;
    Telemetry tel;

    Pipeline pipe(PipelineConfig{}, &rules, &vlm, &sink, &tel);
    pipe.set_clock([] { return Clock{23 * 60 + 42, 1'700'000'000}; });  // 23:42, within window

    FileReplaySource source(ReplayConfig{frames_dir, fps, /*loop=*/false});
    std::printf("replaying %zu frames at %.0f fps through the full pipeline...\n\n",
                source.frames_loaded(), fps);

    pipe.start();
    source.start([&](const FrameView& f) { pipe.on_frame(f); });
    source.wait();
    pipe.stop();

    for (const Alert& a : sink.alerts()) {
        std::printf("ALERT [%s] %s\n", a.rule_id.c_str(), a.one_liner.c_str());
    }
    std::printf("\nframes delivered=%llu dropped=%llu\n\n",
                static_cast<unsigned long long>(source.frames_delivered()),
                static_cast<unsigned long long>(source.frames_dropped()));
    std::printf("%s\n", tel.make_report().to_markdown().c_str());
    return sink.count() > 0 ? 0 : 1;
}
