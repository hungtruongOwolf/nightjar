#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "nightjar/facts.h"
#include "nightjar/rule.h"

namespace nightjar {

// Wall-clock context for a match: time-of-day for the window test, unix seconds
// for cooldown accounting.
struct Clock {
    int minute_of_day = 0;
    int64_t unix_s = 0;
};

// One rule fired: what to send and when.
struct AlertDecision {
    std::string rule_id;
    Subject subject = Subject::Person;
    std::vector<Action> actions;
    int64_t unix_s = 0;
    std::string label;   // human phrase for the alert (e.g. the rule's English); optional
    std::string detail;  // temporal fact for evidence (e.g. "present 92s", "left behind"); optional
};

// The runtime decision point (design doc §5.5). Plain deterministic code — NO
// AI here, microsecond-level. Given the VLM's Facts, the zone the motion was
// in, and the clock, it returns every rule that should fire (respecting each
// rule's per-rule cooldown). N active rules resolve in one pass.
class RuleEngine {
public:
    void set_rules(std::vector<Rule> rules);
    const std::vector<Rule>& rules() const { return rules_; }

    std::vector<AlertDecision> match(const Facts& facts, const std::string& zone_hit, Clock now);

private:
    std::vector<Rule> rules_;
    std::unordered_map<std::string, int64_t> last_fired_unix_s_;
};

}  // namespace nightjar
