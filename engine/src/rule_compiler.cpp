#include "nightjar/rule_compiler.h"

#include <optional>

#include "nightjar/json_object.h"

namespace nightjar {
namespace {

std::optional<Subject> parse_subject(const std::string& s) {
    if (s == "person") return Subject::Person;
    if (s == "vehicle") return Subject::Vehicle;
    if (s == "animal") return Subject::Animal;
    if (s == "package") return Subject::Package;
    return std::nullopt;
}

std::string replace_first(std::string s, const std::string& from, const std::string& to) {
    const size_t pos = s.find(from);
    if (pos != std::string::npos) s.replace(pos, from.size(), to);
    return s;
}

}  // namespace

RuleCompiler::RuleCompiler(std::string prompt_template, std::string grammar, TextInferFn infer)
    : prompt_template_(std::move(prompt_template)),
      grammar_(std::move(grammar)),
      infer_(std::move(infer)) {}

std::optional<Rule> RuleCompiler::parse(const std::string& json, const std::string& id,
                                        const std::string& raw_text) {
    auto obj = parse_flat_json_object(json);
    if (!obj) return std::nullopt;

    auto get = [&](const char* k) -> const std::string* {
        auto it = obj->find(k);
        return it == obj->end() ? nullptr : &it->second;
    };

    const std::string* subj = get("subject");
    const std::string* zone = get("zone");
    const std::string* start = get("start");
    const std::string* end = get("end");
    if (!subj || !zone || !start || !end) return std::nullopt;

    auto subject = parse_subject(*subj);
    if (!subject) return std::nullopt;

    const int start_min = parse_hhmm(*start);
    const int end_min = parse_hhmm(*end);
    if (start_min < 0 || end_min < 0) return std::nullopt;

    Rule rule;
    rule.id = id;
    rule.raw_text = raw_text;
    rule.subject = *subject;
    rule.zone_id = zone->empty() ? "any" : *zone;
    rule.time_window = TimeWindow{start_min, end_min};
    rule.event = Event::Appears;

    if (const std::string* cd = get("cooldown_s")) {
        try {
            const int v = std::stoi(*cd);
            if (v >= 0) rule.cooldown_s = v;
        } catch (...) {
            // leave default cooldown; the value was malformed
        }
    }
    return rule;
}

std::optional<Rule> RuleCompiler::compile(const std::string& english, const std::string& id) const {
    const std::string prompt = replace_first(prompt_template_, "{RULE_TEXT}", english);
    const std::string json = infer_(prompt, grammar_);
    return parse(json, id, english);
}

std::optional<TemporalRule> RuleCompiler::parse_temporal(const std::string& json,
                                                         const std::string& id,
                                                         const std::string& raw_text) {
    auto obj = parse_flat_json_object(json);
    if (!obj) return std::nullopt;
    auto get = [&](const char* k) -> const std::string* {
        auto it = obj->find(k);
        return it == obj->end() ? nullptr : &it->second;
    };

    const std::string* subj = get("subject");
    const std::string* zone = get("zone");
    const std::string* trig = get("trigger");
    const std::string* start = get("start");
    const std::string* end = get("end");
    if (!subj || !zone || !trig || !start || !end) return std::nullopt;
    if (!parse_subject(*subj)) return std::nullopt;

    const int start_min = parse_hhmm(*start);
    const int end_min = parse_hhmm(*end);
    if (start_min < 0 || end_min < 0) return std::nullopt;

    TemporalRule rule;
    rule.id = id;
    rule.raw_text = raw_text;
    rule.zone_id = zone->empty() ? "any" : *zone;
    rule.time_window = TimeWindow{start_min, end_min};

    if (*trig == "appears") {
        rule.trigger = Trigger::Appears;
        rule.predicate = *subj;
    } else if (*trig == "loiter") {
        rule.trigger = Trigger::Sustained;
        rule.predicate = *subj;
        if (const std::string* d = get("dwell_s")) {
            try {
                const int v = std::stoi(*d);
                rule.dwell_s = v > 0 ? v : 60;
            } catch (...) {
                rule.dwell_s = 60;
            }
        } else {
            rule.dwell_s = 60;
        }
    } else if (*trig == "left_behind") {
        rule.trigger = Trigger::LeftBehind;
        rule.predicate = *subj;      // the object left behind (e.g. package)
        rule.actor_predicate = "person";
    } else {
        return std::nullopt;
    }

    if (const std::string* cd = get("cooldown_s")) {
        try {
            const int v = std::stoi(*cd);
            if (v >= 0) rule.cooldown_s = v;
        } catch (...) {
        }
    }
    return rule;
}

std::optional<TemporalRule> RuleCompiler::compile_temporal(const std::string& english,
                                                           const std::string& id) const {
    const std::string prompt = replace_first(prompt_template_, "{RULE_TEXT}", english);
    const std::string json = infer_(prompt, grammar_);
    return parse_temporal(json, id, english);
}

}  // namespace nightjar
