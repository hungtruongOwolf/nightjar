// nightjar_replay_vlm — the real on-device pipeline over a PGM clip: same
// Pipeline as make demo, but Tier 2 is the actual llama.cpp SmolVLM worker.
// This is what produces the real report.md for eval clips.
//   nightjar_replay_vlm <frames_dir> <model.gguf> <mmproj.gguf> [fps]

#include <cstdio>
#include <cstdlib>

#include "mtmd_vlm_worker.h"
#include "nightjar/alert_sink.h"
#include "nightjar/file_replay_source.h"
#include "nightjar/pipeline.h"
#include "nightjar/rule_engine.h"
#include "nightjar/telemetry.h"

using namespace nightjar;

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <frames_dir> <model.gguf> <mmproj.gguf> [fps]\n", argv[0]);
        return 2;
    }
    const std::string frames_dir = argv[1];
    const double fps = argc > 4 ? std::atof(argv[4]) : 30.0;

    Rule r;
    r.id = "backyard-night";
    r.subject = Subject::Person;
    r.zone_id = "any";
    r.time_window = TimeWindow{22 * 60, 6 * 60};
    r.cooldown_s = 120;
    r.actions = {Action{ActionType::Ntfy, "nightjar-demo"}};
    RuleEngine rules;
    rules.set_rules({r});

    // Only ask about the subjects the active rules need (here: person).
    MtmdConfig vcfg;
    vcfg.model_path = argv[2];
    vcfg.mmproj_path = argv[3];
    vcfg.subjects = {Subject::Person};
    MtmdVlmWorker vlm(vcfg);
    if (!vlm.ok()) {
        std::fprintf(stderr, "VLM init failed: %s\n", vlm.error().c_str());
        return 1;
    }

    CapturingSink sink;
    Telemetry tel;
    Pipeline pipe(PipelineConfig{}, &rules, &vlm, &sink, &tel);
    pipe.set_clock([] { return Clock{23 * 60 + 42, 1'700'000'000}; });

    FileReplaySource source(ReplayConfig{frames_dir, fps, false});
    std::printf("replaying %zu frames at %.0f fps with the real SmolVLM worker...\n\n",
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
    return 0;
}
