#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "nightjar/best_frame_selector.h"  // CandidateFrame
#include "nightjar/predicate.h"

namespace nightjar {

// The VLM's answers to a set of per-frame predicates, plus the mandatory
// encode/prefill/decode timing split (summed across the questions asked).
struct PredicateResult {
    std::map<std::string, bool> answers;  // predicate id -> present
    float encode_ms = 0.0f;
    float prefill_ms = 0.0f;
    float decode_ms = 0.0f;

    bool get(const std::string& id) const {
        auto it = answers.find(id);
        return it != answers.end() && it->second;
    }
};

// Tier-2 boundary for the predicate/temporal pipeline: answer a set of y/n
// predicates about one candidate frame. The real mtmd worker implements this;
// the scripted one keeps the pipeline testable and the demo reproducible.
class IPredicateVlm {
public:
    virtual ~IPredicateVlm() = default;
    virtual PredicateResult evaluate(const CandidateFrame& candidate,
                                     const std::vector<Predicate>& predicates) = 0;
};

// Deterministic stand-in. Answers from a fixed map (missing => false) or a
// callback; optional simulated latency exercises conflation honestly.
class ScriptedPredicateVlm : public IPredicateVlm {
public:
    explicit ScriptedPredicateVlm(std::map<std::string, bool> answers, int sim_infer_ms = 0);
    explicit ScriptedPredicateVlm(
        std::function<bool(const CandidateFrame&, const Predicate&)> fn, int sim_infer_ms = 0);

    PredicateResult evaluate(const CandidateFrame& candidate,
                             const std::vector<Predicate>& predicates) override;
    uint64_t calls() const { return calls_.load(); }

private:
    std::function<bool(const CandidateFrame&, const Predicate&)> fn_;
    int sim_infer_ms_ = 0;
    std::atomic<uint64_t> calls_{0};
};

}  // namespace nightjar
