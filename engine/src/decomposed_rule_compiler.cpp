#include "nightjar/decomposed_rule_compiler.h"

#include <algorithm>
#include <cctype>

namespace nightjar {
namespace {

std::string trim(std::string s) {
    auto notspace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    return s;
}

std::string replace_rule(std::string tpl, const std::string& rule) {
    const size_t pos = tpl.find("{RULE_TEXT}");
    if (pos != std::string::npos) tpl.replace(pos, 11, rule);
    return tpl;
}

std::optional<Subject> to_subject(const std::string& s) {
    if (s == "person") return Subject::Person;
    if (s == "vehicle") return Subject::Vehicle;
    if (s == "animal") return Subject::Animal;
    if (s == "package") return Subject::Package;
    return std::nullopt;
}

const char* predicate_id(Subject s) {
    switch (s) {
        case Subject::Person: return "person";
        case Subject::Vehicle: return "vehicle";
        case Subject::Animal: return "animal";
        case Subject::Package: return "package";
    }
    return "person";
}

}  // namespace

DecomposedRuleCompiler::DecomposedRuleCompiler(CompilerAssets assets, TextInferFn infer)
    : assets_(std::move(assets)), infer_(std::move(infer)) {}

std::string DecomposedRuleCompiler::ask(const std::string& prompt_tpl, const std::string& grammar,
                                        const std::string& rule) const {
    return trim(infer_(replace_rule(prompt_tpl, rule), grammar));
}

std::optional<TemporalRule> DecomposedRuleCompiler::compile(const std::string& english,
                                                            const std::string& id) const {
    // Focused classification #1: subject.
    const auto subject = to_subject(ask(assets_.subject_prompt, assets_.subject_grammar, english));
    if (!subject) return std::nullopt;

    // Focused classification #2: event.
    const std::string event = ask(assets_.event_prompt, assets_.event_grammar, english);

    TemporalRule rule;
    rule.id = id;
    rule.raw_text = english;
    rule.predicate = predicate_id(*subject);
    rule.actor_predicate = "person";
    rule.zone_id = "any";
    rule.time_window = TimeWindow{0, 0};  // defaults; the confirmation screen refines
    rule.cooldown_s = 120;

    if (event == "appears") {
        rule.trigger = Trigger::Appears;
    } else if (event == "disappears") {
        rule.trigger = Trigger::Removed;  // taken / stolen
    } else if (event == "stays") {
        rule.trigger = Trigger::Sustained;
        // Focused classification #3: dwell (only when it matters).
        const std::string dwell = ask(assets_.dwell_prompt, assets_.dwell_grammar, english);
        try {
            const int v = std::stoi(dwell);
            rule.dwell_s = v > 0 ? v : 60;
        } catch (...) {
            rule.dwell_s = 60;
        }
    } else {
        return std::nullopt;  // grammar guarantees one of the three; guard anyway
    }
    return rule;
}

}  // namespace nightjar
