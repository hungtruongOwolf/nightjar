// temporal_compile_probe — compile a handful of temporal English rules with the
// real model and print subject/trigger/dwell, to verify the model picks the
// right trigger (appears / loiter / left_behind).
//   temporal_compile_probe <model.gguf> <prompt.txt> <grammar.gbnf>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "llama_text_model.h"
#include "nightjar/rule_compiler.h"

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
        case Trigger::Sustained: return "loiter";
        case Trigger::LeftBehind: return "left_behind";
    }
    return "?";
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <model.gguf> <prompt.txt> <grammar.gbnf>\n", argv[0]);
        return 2;
    }
    LlamaTextModel model(argv[1]);
    if (!model.ok()) {
        std::fprintf(stderr, "%s\n", model.error().c_str());
        return 1;
    }
    RuleCompiler c(slurp(argv[2]), slurp(argv[3]), model.as_infer());

    struct Case {
        const char* text;
        const char* want;  // expected trigger
    };
    const std::vector<Case> cases = {
        {"notify me if a person enters the backyard after 10pm", "appears"},
        {"tell me if someone loiters near my car for more than a minute", "loiter"},
        {"alert me if a package is left at the door and the person walks away", "left_behind"},
        {"let me know if anyone hangs around the shed at night", "loiter"},
        {"a car pulls into the driveway", "appears"},
        {"someone waiting by the gate for a few minutes", "loiter"},
        {"a delivery is dropped off and the courier leaves", "left_behind"},
        {"warn me when an animal gets into the garden", "appears"},
    };

    int ok = 0;
    for (const auto& cse : cases) {
        auto r = c.compile_temporal(cse.text, "probe");
        const std::string got = r ? trig(r->trigger) : "(fail)";
        const bool hit = (got == cse.want);
        ok += hit;
        std::printf("[%s] want=%-11s got=%-11s subj=%-8s dwell=%d  %s\n", hit ? "OK" : "XX",
                    cse.want, got.c_str(), r ? r->predicate.c_str() : "?", r ? r->dwell_s : -1,
                    cse.text);
    }
    std::printf("\ntrigger accuracy: %d/%zu\n", ok, cases.size());
    return 0;
}
