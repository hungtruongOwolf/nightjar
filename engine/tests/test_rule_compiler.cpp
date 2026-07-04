#include "nightjar/rule_compiler.h"

#include <string>

#include "check.h"

using namespace nightjar;

namespace {

// ---- parse() (JSON -> Rule) ----

void test_parse_full_object() {
    auto r = RuleCompiler::parse(
        R"({"subject":"person","zone":"backyard","start":"22:00","end":"06:00","cooldown_s":300})",
        "rule-1", "notify me if a person enters the backyard after 10pm");
    CHECK(r.has_value());
    CHECK(r->subject == Subject::Person);
    CHECK_EQ(r->zone_id, std::string("backyard"));
    CHECK_EQ(r->time_window.start_min, 22 * 60);
    CHECK_EQ(r->time_window.end_min, 6 * 60);
    CHECK_EQ(r->cooldown_s, 300);
    CHECK_EQ(r->id, std::string("rule-1"));
    CHECK(r->time_window.contains(23 * 60));  // 11pm inside overnight window
}

void test_parse_all_subjects() {
    for (auto pair : {std::pair<const char*, Subject>{"vehicle", Subject::Vehicle},
                      {"animal", Subject::Animal},
                      {"package", Subject::Package}}) {
        std::string j = std::string("{\"subject\":\"") + pair.first +
                        "\",\"zone\":\"any\",\"start\":\"00:00\",\"end\":\"00:00\"}";
        auto r = RuleCompiler::parse(j, "x", "t");
        CHECK(r.has_value());
        CHECK(r->subject == pair.second);
    }
}

void test_parse_defaults_cooldown() {
    auto r = RuleCompiler::parse(
        R"({"subject":"person","zone":"any","start":"00:00","end":"00:00"})", "x", "t");
    CHECK(r.has_value());
    CHECK_EQ(r->cooldown_s, 120);  // default when absent
}

void test_parse_rejects_bad_subject_and_time() {
    CHECK(!RuleCompiler::parse(R"({"subject":"ufo","zone":"a","start":"00:00","end":"00:00"})", "x",
                               "t")
               .has_value());
    CHECK(!RuleCompiler::parse(R"({"subject":"person","zone":"a","start":"25:00","end":"00:00"})",
                               "x", "t")
               .has_value());
    CHECK(!RuleCompiler::parse("not json at all", "x", "t").has_value());
    CHECK(!RuleCompiler::parse(R"({"subject":"person"})", "x", "t").has_value());  // missing fields
}

// ---- compile() with an injected fake LLM ----

void test_compile_with_fake_infer() {
    // Fake model: maps known English to canned JSON.
    TextInferFn fake = [](const std::string& prompt, const std::string&) -> std::string {
        if (prompt.find("cat") != std::string::npos)
            return R"({"subject":"animal","zone":"garden","start":"00:00","end":"00:00","cooldown_s":120})";
        return R"({"subject":"person","zone":"backyard","start":"22:00","end":"06:00","cooldown_s":120})";
    };
    RuleCompiler compiler("Rule: {RULE_TEXT}", "grammar", fake);

    auto r1 = compiler.compile("notify me if a person enters the backyard after 10pm", "r1");
    CHECK(r1.has_value());
    CHECK(r1->subject == Subject::Person);
    CHECK_EQ(r1->zone_id, std::string("backyard"));

    auto r2 = compiler.compile("tell me if a cat gets in the garden", "r2");
    CHECK(r2.has_value());
    CHECK(r2->subject == Subject::Animal);
    CHECK_EQ(r2->zone_id, std::string("garden"));
    CHECK_EQ(r2->raw_text, std::string("tell me if a cat gets in the garden"));
}

void test_compile_prompt_gets_rule_text() {
    // Verify {RULE_TEXT} is substituted into the prompt handed to the model.
    std::string seen_prompt;
    TextInferFn spy = [&](const std::string& prompt, const std::string&) {
        seen_prompt = prompt;
        return R"({"subject":"person","zone":"any","start":"00:00","end":"00:00"})";
    };
    RuleCompiler compiler("PROMPT[{RULE_TEXT}]END", "g", spy);
    compiler.compile("watch the gate", "r");
    CHECK(seen_prompt == std::string("PROMPT[watch the gate]END"));
}

void test_compile_returns_nullopt_on_garbage_model_output() {
    RuleCompiler compiler("{RULE_TEXT}", "g",
                          [](const std::string&, const std::string&) { return "sorry I can't"; });
    CHECK(!compiler.compile("anything", "r").has_value());
}

}  // namespace

int main() {
    test_parse_full_object();
    test_parse_all_subjects();
    test_parse_defaults_cooldown();
    test_parse_rejects_bad_subject_and_time();
    test_compile_with_fake_infer();
    test_compile_prompt_gets_rule_text();
    test_compile_returns_nullopt_on_garbage_model_output();
    return njtest::failures() == 0 ? 0 : 1;
}
