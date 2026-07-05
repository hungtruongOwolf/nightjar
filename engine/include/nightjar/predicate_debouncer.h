#pragma once

#include <map>
#include <string>
#include <unordered_map>

namespace nightjar {

struct DebounceConfig {
    int on_streak = 1;   // consecutive raw-true frames before a predicate is "on"
                         // (1 = detect immediately, never-miss; Tier 2 cleans up)
    int off_streak = 2;  // consecutive raw-false frames before it goes "off"
                         // (asymmetric: slower to clear, so one bad frame doesn't
                         //  drop an object — matters for the place-vs-take signal)
};

// The per-frame VLM is a noisy sensor: one flaky frame can flip a fact. The
// debouncer applies hysteresis so a predicate only changes state after enough
// consecutive agreeing frames — a single blip is ignored. This is what makes
// the temporal state machine (and its place-vs-take distinction) trustworthy
// enough for a live demo. Deterministic; per-predicate state kept internally.
class PredicateDebouncer {
public:
    explicit PredicateDebouncer(DebounceConfig config) : config_(config) {}

    // Feed one frame's raw predicate answers; returns the debounced stable states.
    std::map<std::string, bool> update(const std::map<std::string, bool>& raw);

    // Current debounced value of a predicate (false if never seen).
    bool state(const std::string& id) const;

private:
    struct S {
        bool state = false;
        int against = 0;  // consecutive raw readings disagreeing with `state`
    };
    DebounceConfig config_;
    std::unordered_map<std::string, S> st_;
};

}  // namespace nightjar
