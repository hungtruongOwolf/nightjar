// raw_compile_probe — print the model's raw JSON for a set of rules, to inspect
// the decomposed-fields compilation method (classify fields, assemble in code).
//   raw_compile_probe <model.gguf> <prompt.txt> <grammar.gbnf>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "llama_text_model.h"

using namespace nightjar;

namespace {
std::string slurp(const std::string& p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
std::string sub(std::string s, const std::string& r) {
    const size_t pos = s.find("{RULE_TEXT}");
    if (pos != std::string::npos) s.replace(pos, 11, r);
    return s;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <model.gguf> <prompt.txt> <grammar.gbnf>\n", argv[0]);
        return 2;
    }
    LlamaTextModel model(argv[1]);
    if (!model.ok()) return 1;
    const std::string tpl = slurp(argv[2]);
    const std::string grammar = slurp(argv[3]);

    const std::vector<std::string> cases = {
        "notify me if a person enters the backyard after 10pm",
        "tell me if someone loiters near my car for more than a minute",
        "alert me if my package is stolen from the porch",
        "let me know if a delivery is left at the door and the courier walks away",
        "a car pulls into the driveway",
        "someone waits by the gate and then a car shows up",
        "warn me if an animal hangs around the garden for a few minutes",
        "tell me if a person takes my bike",
    };
    for (const std::string& c : cases) {
        std::string j = model.generate(sub(tpl, c), grammar, 96);
        const size_t nl = j.find('\n');
        if (nl != std::string::npos) j = j.substr(0, nl);
        std::printf("%-58s\n  %s\n", c.c_str(), j.c_str());
    }
    return 0;
}
