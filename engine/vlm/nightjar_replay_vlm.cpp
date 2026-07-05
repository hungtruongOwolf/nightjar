// nightjar_replay_vlm — the real on-device pipeline over a PGM clip: same
// Pipeline as make demo, but Tier 2 is the actual llama.cpp SmolVLM worker.
// This is what produces the real report.md for eval clips.
//   nightjar_replay_vlm <frames_dir> <model.gguf> <mmproj.gguf> [fps]

#include <cstdio>
#include <cstdlib>

#include <memory>
#include <ctime>

#include "mtmd_vlm_worker.h"
#include "nightjar/alert_sink.h"
#include "nightjar/file_replay_source.h"
#include "nightjar/pipeline.h"
#include "nightjar/temporal_rule_engine.h"
#include "nightjar/telemetry.h"
#ifdef NIGHTJAR_HAVE_NET
#include "ntfy_sink.h"
#endif

using namespace nightjar;

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
                     "usage: %s <frames_dir> <model.gguf> <mmproj.gguf> [fps] [ntfy_topic]\n",
                     argv[0]);
        return 2;
    }
    const std::string frames_dir = argv[1];
    const double fps = argc > 4 ? std::atof(argv[4]) : 30.0;
    const std::string ntfy_topic = argc > 5 ? argv[5] : "";

    // Loitering: a person sustained in view — a condition a detector can't express.
    TemporalRule r;
    r.id = "loiter-backyard";
    r.raw_text = "tell me if someone loiters in the backyard at night";
    r.predicate = "person";
    r.trigger = Trigger::Sustained;
    r.dwell_s = 2;  // demo-scaled
    r.zone_id = "any";
    r.time_window = TimeWindow{22 * 60, 6 * 60};
    r.cooldown_s = 120;
    r.actions = {Action{ActionType::Ntfy, "nightjar-demo"}};
    TemporalRuleEngine rules;
    rules.set_rules({r});

    MtmdConfig vcfg;
    vcfg.model_path = argv[2];
    vcfg.mmproj_path = argv[3];
    MtmdVlmWorker vlm(vcfg);
    if (!vlm.ok()) {
        std::fprintf(stderr, "VLM init failed: %s\n", vlm.error().c_str());
        return 1;
    }

    // Always capture; also push to ntfy if a topic was given and net is built in.
    CapturingSink sink;
    MultiSink fanout;
    fanout.add(std::shared_ptr<IAlertSink>(&sink, [](IAlertSink*) {}));  // non-owning
#ifdef NIGHTJAR_HAVE_NET
    if (!ntfy_topic.empty()) {
        NtfyConfig ncfg;
        ncfg.topic = ntfy_topic;
        fanout.add(std::make_shared<NtfySink>(ncfg));
        std::printf("pushing alerts to ntfy.sh/%s\n", ntfy_topic.c_str());
    }
#else
    if (!ntfy_topic.empty())
        std::printf("(built without NIGHTJAR_NET — ntfy topic ignored)\n");
#endif

    Telemetry tel;
    PipelineConfig cfg;
    cfg.predicates = {{"person", "Is there a person in this image? Answer y or n."}};
    Pipeline pipe(cfg, &rules, &vlm, &fanout, &tel);
    // Fixed night time-of-day, real unix seconds so the dwell timer elapses.
    pipe.set_clock([] { return Clock{23 * 60 + 42, static_cast<int64_t>(std::time(nullptr))}; });

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
