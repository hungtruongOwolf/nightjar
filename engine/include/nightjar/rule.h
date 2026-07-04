#pragma once

#include <string>
#include <vector>

#include "nightjar/facts.h"

namespace nightjar {

// v1 has one event only (NG9): the subject appears in the zone.
enum class Event { Appears };

// Minutes-since-midnight interval. Overnight windows (start > end) are allowed
// and wrap past midnight. start == end means "always active" (24h).
struct TimeWindow {
    int start_min = 0;
    int end_min = 0;
    bool contains(int minute_of_day) const;
};

enum class ActionType { Ntfy, Telegram, Webhook };

struct Action {
    ActionType type = ActionType::Ntfy;
    std::string target;  // ntfy topic / telegram chat id / webhook URL
};

// A compiled rule (design doc §5.5). Produced once by the RuleCompiler from
// English, confirmed by the user, then matched deterministically at runtime.
struct Rule {
    std::string id;
    std::string raw_text;   // the original English, for the confirmation screen
    Subject subject = Subject::Person;
    std::string zone_id = "any";  // "any" matches every zone
    TimeWindow time_window;
    Event event = Event::Appears;
    int cooldown_s = 120;
    std::vector<Action> actions;
};

// Parse "HH:MM" (24h) to minutes since midnight; -1 if malformed.
int parse_hhmm(const std::string& s);

}  // namespace nightjar
