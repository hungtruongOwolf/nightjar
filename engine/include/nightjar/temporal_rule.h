#pragma once

#include <map>
#include <string>
#include <vector>

#include "nightjar/rule.h"  // TimeWindow, Action

namespace nightjar {

// What kind of temporal condition fires the rule. These are the conditions a
// closed-vocabulary detector fundamentally cannot express — the differentiator.
enum class Trigger {
    Appears,     // predicate becomes true (rising edge) — the classic case
    Sustained,   // predicate stays true continuously for >= dwell_s (loitering)
    LeftBehind,  // object appears and STAYS present while the actor leaves
                 // (package dropped, then the person walks off) — "delivery"
    Removed,     // object that WAS present goes absent while a person is around
                 // (something is taken away) — "theft". The mirror of LeftBehind:
                 // place vs take are distinguished by the object's presence
                 // trajectory (appears+stays vs was-there+disappears), not one frame.
};

// A compiled temporal rule. Reuses TimeWindow/Action/cooldown from Rule; adds
// the trigger kind and its parameters. `predicate` is the primary predicate id;
// `actor_predicate` is the one that must leave for LeftBehind.
struct TemporalRule {
    std::string id;
    std::string raw_text;
    std::string predicate;              // e.g. "person", "package"
    std::string actor_predicate = "person";  // LeftBehind: who leaves
    Trigger trigger = Trigger::Appears;
    int dwell_s = 0;                    // Sustained: required continuous seconds
    std::string zone_id = "any";
    TimeWindow time_window;
    int cooldown_s = 120;
    std::vector<Action> actions;
};

// One frame's worth of VLM answers plus context, fed to the engine over time.
struct Observation {
    std::map<std::string, bool> predicates;  // predicate id -> present this frame
    std::string zone = "any";
    int minute_of_day = 0;
    int64_t unix_s = 0;

    bool has(const std::string& id) const {
        auto it = predicates.find(id);
        return it != predicates.end() && it->second;
    }
};

}  // namespace nightjar
