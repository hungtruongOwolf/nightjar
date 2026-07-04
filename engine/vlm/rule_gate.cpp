// rule_gate — the week-3 RuleCompiler quality gate (design doc §5.5). Compiles
// all of tests/rules_20.txt with the real local model and checks each result's
// subject against tests/rules_20_expected.txt. Pass = >= 16/20 subjects correct;
// a failure activates C6 (load a stronger model like Qwen2.5-1.5B at setup).
//
//   rule_gate <model.gguf> <prompt.txt> <grammar.gbnf> <rules.txt> <expected.txt>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "llama_text_model.h"
#include "nightjar/rule_compiler.h"

using namespace nightjar;

namespace {

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Non-blank, non-# lines, trimmed.
std::vector<std::string> read_lines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        size_t a = line.find_first_not_of(" \t\r");
        if (a == std::string::npos) continue;
        if (line[a] == '#') continue;
        size_t b = line.find_last_not_of(" \t\r");
        lines.push_back(line.substr(a, b - a + 1));
    }
    return lines;
}

const char* subject_word(Subject s) {
    switch (s) {
        case Subject::Person: return "person";
        case Subject::Vehicle: return "vehicle";
        case Subject::Animal: return "animal";
        case Subject::Package: return "package";
    }
    return "?";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr,
                     "usage: %s <model.gguf> <prompt.txt> <grammar.gbnf> <rules.txt> <expected.txt>\n",
                     argv[0]);
        return 2;
    }
    LlamaTextModel model(argv[1]);
    if (!model.ok()) {
        std::fprintf(stderr, "model init failed: %s\n", model.error().c_str());
        return 1;
    }

    RuleCompiler compiler(slurp(argv[2]), slurp(argv[3]), model.as_infer());
    const auto rules = read_lines(argv[4]);
    const auto expected = read_lines(argv[5]);
    if (rules.size() != expected.size()) {
        std::fprintf(stderr, "rules (%zu) and expected (%zu) count mismatch\n", rules.size(),
                     expected.size());
        return 2;
    }

    int correct = 0;
    for (size_t i = 0; i < rules.size(); ++i) {
        auto rule = compiler.compile(rules[i], "gate-" + std::to_string(i));
        const std::string got = rule ? subject_word(rule->subject) : "(parse-fail)";
        const bool ok = rule && got == expected[i];
        correct += ok ? 1 : 0;
        std::printf("[%s] want=%-8s got=%-12s %s\n", ok ? "OK" : "XX", expected[i].c_str(),
                    got.c_str(), rules[i].c_str());
    }

    const int n = static_cast<int>(rules.size());
    std::printf("\nSUBJECT accuracy: %d/%d  (gate: >=16/20)\n", correct, n);
    std::printf("verdict: %s\n", correct >= 16 ? "PASS" : "FAIL -> activate C6 (stronger compiler model)");
    return correct >= 16 ? 0 : 1;
}
