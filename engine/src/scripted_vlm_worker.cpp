#include "nightjar/vlm_worker.h"

#include <chrono>
#include <thread>

namespace nightjar {

ScriptedVlmWorker::ScriptedVlmWorker(Facts fixed, int sim_infer_ms)
    : fn_([fixed](const CandidateFrame&) { return fixed; }), sim_infer_ms_(sim_infer_ms) {}

ScriptedVlmWorker::ScriptedVlmWorker(std::function<Facts(const CandidateFrame&)> fn, int sim_infer_ms)
    : fn_(std::move(fn)), sim_infer_ms_(sim_infer_ms) {}

Facts ScriptedVlmWorker::infer(const CandidateFrame& candidate) {
    calls_.fetch_add(1);
    if (sim_infer_ms_ > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sim_infer_ms_));
    }
    Facts f = fn_(candidate);
    if (sim_infer_ms_ > 0) {
        // Plausible simulated split (encode/prefill/decode ~ 15/50/35%). Clearly
        // synthetic — real numbers come from the mtmd-backed worker.
        const float total = static_cast<float>(sim_infer_ms_);
        f.encode_ms = total * 0.15f;
        f.prefill_ms = total * 0.50f;
        f.decode_ms = total * 0.35f;
    }
    return f;
}

}  // namespace nightjar
