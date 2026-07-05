// nightjar_flow — the full product flow in one run: type an English rule, the
// compiler model (Qwen) turns it into a TemporalRule via decomposition, is then
// freed, and the runtime (SmolVLM + gate + temporal engine) guards a clip.
// Mirrors the on-device setup->runtime split (compile once, unload, then run).
//   nightjar_flow <compiler.gguf> <vlm.gguf> <mmproj.gguf> <frames_dir> "<english rule>"
// Run from the repo root (reads prompts/ + grammar/).

#include <cstdio>
#include <ctime>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "llama_text_model.h"
#include "mtmd_vlm_worker.h"
#include "nightjar/alert_sink.h"
#include "nightjar/decomposed_rule_compiler.h"
#include "nightjar/file_replay_source.h"
#include "nightjar/pipeline.h"
#include "nightjar/telemetry.h"
#include "nightjar/temporal_rule_engine.h"

using namespace nightjar;

namespace {
std::string slurp(const std::string& p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
const char* trig(Trigger t) {
    switch (t) {
        case Trigger::Appears: return "appears";
        case Trigger::Sustained: return "loiter/stays";
        case Trigger::Removed: return "taken/removed";
        case Trigger::LeftBehind: return "left_behind";
    }
    return "?";
}

// Load the compiler model, compile the English rule, free the model (RAM back).
std::optional<TemporalRule> compile_rule(const std::string& compiler_model,
                                         const std::string& english) {
    LlamaTextModel model(compiler_model);
    if (!model.ok()) {
        std::fprintf(stderr, "compiler init: %s\n", model.error().c_str());
        return std::nullopt;
    }
    CompilerAssets a;
    a.subject_prompt = slurp("prompts/compile_subject.txt");
    a.subject_grammar = slurp("grammar/subject.gbnf");
    a.event_prompt = slurp("prompts/compile_event.txt");
    a.event_grammar = slurp("grammar/event.gbnf");
    a.dwell_prompt = slurp("prompts/compile_dwell.txt");
    a.dwell_grammar = slurp("grammar/dwell.gbnf");
    DecomposedRuleCompiler compiler(a, model.as_infer());
    return compiler.compile(english, "user-rule");
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr,
                     "usage: %s <compiler.gguf> <vlm.gguf> <mmproj.gguf> <frames> \"<rule>\"\n",
                     argv[0]);
        return 2;
    }
    const std::string english = argv[5];

    // --- SETUP: compile English -> TemporalRule with the compiler model, then free it ---
    std::printf("compiling rule: \"%s\"\n", english.c_str());
    std::optional<TemporalRule> rule;
    {
        rule = compile_rule(argv[1], english);
    }  // compiler model freed here
    if (!rule) {
        std::fprintf(stderr, "compile failed\n");
        return 1;
    }
    std::printf("  -> subject=%s  trigger=%s  dwell=%ds\n\n", rule->predicate.c_str(),
                trig(rule->trigger), rule->dwell_s);

    // --- RUNTIME: guard the clip with the compiled rule ---
    TemporalRuleEngine rules;
    rules.set_rules({*rule});

    MtmdConfig vcfg;
    vcfg.model_path = argv[2];
    vcfg.mmproj_path = argv[3];
    vcfg.reuse_image_kv = true;  // optimization 1
    MtmdVlmWorker vlm(vcfg);
    if (!vlm.ok()) {
        std::fprintf(stderr, "vlm init: %s\n", vlm.error().c_str());
        return 1;
    }

    CapturingSink sink;
    Telemetry tel;
    PipelineConfig cfg;
    cfg.predicates = {{rule->predicate,
                       "Is there a " + rule->predicate + " in this image? Answer y or n."}};
    Pipeline pipe(cfg, &rules, &vlm, &sink, &tel);
    pipe.set_clock([] { return Clock{23 * 60 + 42, static_cast<int64_t>(std::time(nullptr))}; });

    FileReplaySource src(ReplayConfig{argv[4], 30.0, false});
    std::printf("guarding %zu frames with the compiled rule...\n\n", src.frames_loaded());
    pipe.start();
    src.start([&](const FrameView& f) { pipe.on_frame(f); });
    src.wait();
    pipe.stop();

    for (const Alert& a : sink.alerts()) std::printf("ALERT: %s\n", a.one_liner.c_str());
    if (sink.count() == 0) std::printf("(no alert)\n");
    return 0;
}
