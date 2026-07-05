#include "nightjar/rule_engine.h"

namespace nightjar {

void RuleEngine::set_rules(std::vector<Rule> rules) {
    rules_ = std::move(rules);
    last_fired_unix_s_.clear();
}

std::vector<AlertDecision> RuleEngine::match(const Facts& facts, const std::string& zone_hit,
                                             Clock now) {
    std::vector<AlertDecision> decisions;
    for (const Rule& rule : rules_) {
        if (!facts.has(rule.subject)) continue;
        if (rule.zone_id != "any" && rule.zone_id != zone_hit) continue;
        if (!rule.time_window.contains(now.minute_of_day)) continue;

        // Per-rule cooldown: suppress repeats within cooldown_s of the last fire.
        auto it = last_fired_unix_s_.find(rule.id);
        if (it != last_fired_unix_s_.end() && now.unix_s - it->second < rule.cooldown_s) {
            continue;
        }
        last_fired_unix_s_[rule.id] = now.unix_s;

        decisions.push_back(AlertDecision{rule.id, rule.subject, rule.actions, now.unix_s, {}});
    }
    return decisions;
}

}  // namespace nightjar
