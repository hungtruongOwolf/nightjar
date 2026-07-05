#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "nightjar/rule_engine.h"  // AlertDecision
#include "nightjar/temporal_rule.h"

namespace nightjar {

// Evaluates temporal rules over a stream of per-frame Observations — plain
// deterministic code, NO AI (design principle: no AI at the runtime decision
// point). The VLM answers per-frame yes/no predicates; this engine integrates
// them over time into the conditions a detector can't do: rising-edge appears,
// sustained presence (loitering), and object-left-behind sequences. Fed one
// Observation per candidate frame; returns the rules that fire at that frame.
class TemporalRuleEngine {
public:
    void set_rules(std::vector<TemporalRule> rules);
    const std::vector<TemporalRule>& rules() const { return rules_; }

    std::vector<AlertDecision> observe(const Observation& obs);

private:
    struct State {
        bool prev_present = false;                                // for rising-edge
        int64_t true_since_s = -1;                                // Sustained: when it went true
        bool actor_prev_present = false;                          // LeftBehind
        bool object_seen = false;                                 // LeftBehind
        int64_t last_fired_s = std::numeric_limits<int64_t>::min();
    };

    bool passes_gates(const TemporalRule& rule, const Observation& obs, State& state);

    std::vector<TemporalRule> rules_;
    std::unordered_map<std::string, State> state_;
};

}  // namespace nightjar
