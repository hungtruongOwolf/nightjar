// compile_eval, reproducible accuracy eval for the decomposed rule compiler.
// Runs DecomposedRuleCompiler (real model) over a labeled set and reports
// subject + trigger accuracy. Run from the repo root (reads prompts/ + grammar/).
//   compile_eval <model.gguf> <eval.txt>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "llama_text_model.h"
#include "nightjar/decomposed_rule_compiler.h"

using namespace nightjar;

namespace {
std::string slurp(const std::string& p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
std::string trigger_word(Trigger t) {
    switch (t) {
        case Trigger::Appears: return "appears";
        case Trigger::Sustained: return "stays";
        case Trigger::Removed: return "disappears";
        case Trigger::LeftBehind: return "left_behind";
    }
    return "?";
}
std::string field(const std::string& line, int idx) {  // split on '|', trimmed
    std::stringstream ss(line);
    std::string part;
    for (int i = 0; i <= idx; ++i)
        if (!std::getline(ss, part, '|')) return "";
    size_t a = part.find_first_not_of(" \t");
    size_t b = part.find_last_not_of(" \t");
    return a == std::string::npos ? "" : part.substr(a, b - a + 1);
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <model.gguf> <eval.txt>   (run from repo root)\n", argv[0]);
        return 2;
    }
    LlamaTextModel model(argv[1]);
    if (!model.ok()) {
        std::fprintf(stderr, "%s\n", model.error().c_str());
        return 1;
    }
    CompilerAssets assets;
    assets.subject_prompt = slurp("prompts/compile_subject.txt");
    assets.subject_grammar = slurp("grammar/subject.gbnf");
    assets.event_prompt = slurp("prompts/compile_event.txt");
    assets.event_grammar = slurp("grammar/event.gbnf");
    assets.dwell_prompt = slurp("prompts/compile_dwell.txt");
    assets.dwell_grammar = slurp("grammar/dwell.gbnf");
    DecomposedRuleCompiler compiler(assets, model.as_infer());

    std::ifstream f(argv[2]);
    std::string line;
    int n = 0, subj_ok = 0, trig_ok = 0, both_ok = 0;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        const std::string english = field(line, 0);
        const std::string want_subj = field(line, 1);
        const std::string want_trig = field(line, 2);
        auto r = compiler.compile(english, "e" + std::to_string(n));
        const std::string got_subj = r ? r->predicate : "(fail)";
        const std::string got_trig = r ? trigger_word(r->trigger) : "(fail)";
        const bool s_ok = got_subj == want_subj;
        const bool t_ok = got_trig == want_trig;
        subj_ok += s_ok;
        trig_ok += t_ok;
        both_ok += (s_ok && t_ok);
        ++n;
        std::printf("[%s%s] subj %-8s(%s)  trig %-11s(%s)  %s\n", s_ok ? "S" : "-", t_ok ? "T" : "-",
                    got_subj.c_str(), want_subj.c_str(), got_trig.c_str(), want_trig.c_str(),
                    english.c_str());
    }
    std::printf("\nsubject: %d/%d   trigger: %d/%d   both: %d/%d\n", subj_ok, n, trig_ok, n, both_ok,
                n);
    return 0;
}
