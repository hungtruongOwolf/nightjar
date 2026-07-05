#include "nightjar/predicate_debouncer.h"

namespace nightjar {

std::map<std::string, bool> PredicateDebouncer::update(const std::map<std::string, bool>& raw) {
    std::map<std::string, bool> out;
    for (const auto& kv : raw) {
        S& s = st_[kv.first];
        const bool observed = kv.second;
        if (observed == s.state) {
            s.against = 0;  // agrees with current state
        } else {
            ++s.against;
            const int threshold = observed ? config_.on_streak : config_.off_streak;
            if (s.against >= threshold) {
                s.state = observed;  // enough consecutive disagreement -> flip
                s.against = 0;
            }
        }
        out[kv.first] = s.state;
    }
    return out;
}

bool PredicateDebouncer::state(const std::string& id) const {
    auto it = st_.find(id);
    return it != st_.end() && it->second.state;
}

}  // namespace nightjar
