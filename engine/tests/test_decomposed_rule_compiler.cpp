#include "nightjar/decomposed_rule_compiler.h"

#include <string>

#include "check.h"

using namespace nightjar;

namespace {

// A scripted model: routes each focused question by inspecting the grammar
// (subject grammar mentions "person"; event grammar mentions "appears"; dwell
// grammar is digits) and returns a programmed answer. Verifies the compiler's
// decomposition + deterministic assembly, no model needed.
CompilerAssets test_assets() {
    CompilerAssets a;
    a.subject_grammar = "person vehicle animal package";
    a.event_grammar = "appears stays disappears";
    a.dwell_grammar = "30 60 180 600";
    a.subject_prompt = a.event_prompt = a.dwell_prompt = "{RULE_TEXT}";
    return a;
}

DecomposedRuleCompiler make(const std::string& subj, const std::string& ev,
                            const std::string& dwell = "60") {
    TextInferFn infer = [subj, ev, dwell](const std::string&, const std::string& grammar) {
        if (grammar.find("appears") != std::string::npos) return ev;
        if (grammar.find("person") != std::string::npos) return subj;
        return dwell;  // dwell grammar
    };
    return DecomposedRuleCompiler(test_assets(), infer);
}

void test_appears() {
    auto r = make("vehicle", "appears").compile("a car pulls in", "r");
    CHECK(r.has_value());
    CHECK(r->trigger == Trigger::Appears);
    CHECK(r->predicate == std::string("vehicle"));
}

void test_loiter_with_dwell() {
    auto r = make("person", "stays", "180").compile("someone hangs around a few minutes", "r");
    CHECK(r.has_value());
    CHECK(r->trigger == Trigger::Sustained);
    CHECK_EQ(r->dwell_s, 180);
}

void test_theft() {
    auto r = make("vehicle", "disappears").compile("my bike is stolen", "r");
    CHECK(r.has_value());
    CHECK(r->trigger == Trigger::Removed);
    CHECK(r->predicate == std::string("vehicle"));
}

void test_answers_are_trimmed() {
    // Model returns leading whitespace/newline (common) — must still parse.
    TextInferFn infer = [](const std::string&, const std::string& g) -> std::string {
        if (g.find("appears") != std::string::npos) return " stays\n";
        if (g.find("person") != std::string::npos) return "\n person ";
        return "  60 ";
    };
    DecomposedRuleCompiler c(test_assets(), infer);
    auto r = c.compile("someone loiters", "r");
    CHECK(r.has_value());
    CHECK(r->trigger == Trigger::Sustained);
    CHECK(r->predicate == std::string("person"));
}

void test_bad_subject_rejected() {
    auto r = make("spaceship", "appears").compile("x", "r");
    CHECK(!r.has_value());
}

}  // namespace

int main() {
    test_appears();
    test_loiter_with_dwell();
    test_theft();
    test_answers_are_trimmed();
    test_bad_subject_rejected();
    return njtest::failures() == 0 ? 0 : 1;
}
