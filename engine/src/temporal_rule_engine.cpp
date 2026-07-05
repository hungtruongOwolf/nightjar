#include "nightjar/temporal_rule_engine.h"

#include <limits>

namespace nightjar {
namespace {

Subject subject_of(const std::string& predicate) {
    if (predicate == "vehicle") return Subject::Vehicle;
    if (predicate == "animal") return Subject::Animal;
    if (predicate == "package") return Subject::Package;
    return Subject::Person;  // default / attribute predicates alert as person context
}

}  // namespace

void TemporalRuleEngine::set_rules(std::vector<TemporalRule> rules) {
    rules_ = std::move(rules);
    state_.clear();
}

// Zone + time-window + cooldown, shared by all trigger kinds. On success it
// records the fire time so the cooldown applies.
bool TemporalRuleEngine::passes_gates(const TemporalRule& rule, const Observation& obs,
                                      State& state) {
    if (rule.zone_id != "any" && rule.zone_id != obs.zone) return false;
    if (!rule.time_window.contains(obs.minute_of_day)) return false;
    // Cooldown (skip the check if never fired — avoids INT64_MIN underflow).
    const bool ever_fired = state.last_fired_s != std::numeric_limits<int64_t>::min();
    if (ever_fired && obs.unix_s - state.last_fired_s < rule.cooldown_s) return false;
    state.last_fired_s = obs.unix_s;
    return true;
}

std::vector<AlertDecision> TemporalRuleEngine::observe(const Observation& obs) {
    std::vector<AlertDecision> decisions;

    for (const TemporalRule& rule : rules_) {
        State& s = state_[rule.id];
        const bool present = obs.has(rule.predicate);
        bool fired = false;

        switch (rule.trigger) {
            case Trigger::Appears:
                fired = present && !s.prev_present;  // rising edge only
                break;

            case Trigger::Sustained: {
                if (present) {
                    if (s.true_since_s < 0) s.true_since_s = obs.unix_s;
                    fired = (obs.unix_s - s.true_since_s) >= rule.dwell_s;
                } else {
                    s.true_since_s = -1;  // presence broken, restart the clock
                }
                break;
            }

            case Trigger::LeftBehind: {
                if (present) s.object_seen = true;  // object (e.g. package) is here
                const bool actor = obs.has(rule.actor_predicate);
                const bool actor_left = s.actor_prev_present && !actor;
                // Object still present AND the actor just left the scene.
                fired = s.object_seen && present && actor_left;
                s.actor_prev_present = actor;
                break;
            }
        }

        s.prev_present = present;

        if (fired && passes_gates(rule, obs, s)) {
            decisions.push_back(AlertDecision{rule.id, subject_of(rule.predicate), rule.actions,
                                              obs.unix_s});
        }
    }
    return decisions;
}

}  // namespace nightjar
