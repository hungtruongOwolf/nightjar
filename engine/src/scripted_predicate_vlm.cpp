#include "nightjar/predicate_vlm.h"

#include <chrono>
#include <thread>

namespace nightjar {

ScriptedPredicateVlm::ScriptedPredicateVlm(std::map<std::string, bool> answers, int sim_infer_ms)
    : fn_([answers](const CandidateFrame&, const Predicate& p) {
          auto it = answers.find(p.id);
          return it != answers.end() && it->second;
      }),
      sim_infer_ms_(sim_infer_ms) {}

ScriptedPredicateVlm::ScriptedPredicateVlm(
    std::function<bool(const CandidateFrame&, const Predicate&)> fn, int sim_infer_ms)
    : fn_(std::move(fn)), sim_infer_ms_(sim_infer_ms) {}

PredicateResult ScriptedPredicateVlm::evaluate(const CandidateFrame& candidate,
                                               const std::vector<Predicate>& predicates) {
    calls_.fetch_add(1);
    if (sim_infer_ms_ > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sim_infer_ms_));
    }
    PredicateResult r;
    for (const Predicate& p : predicates) r.answers[p.id] = fn_(candidate, p);
    if (sim_infer_ms_ > 0) {
        const float total = static_cast<float>(sim_infer_ms_);
        r.encode_ms = total * 0.15f;
        r.prefill_ms = total * 0.50f;
        r.decode_ms = total * 0.35f;
    }
    return r;
}

}  // namespace nightjar
