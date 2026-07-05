#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

#include "nightjar/best_frame_selector.h"  // CandidateFrame
#include "nightjar/facts.h"

namespace nightjar {

// The Tier-2 vision-language stage. The real implementation wraps llama.cpp
// mtmd and links the model; the engine core depends only on this interface so
// the whole pipeline can be assembled and tested deterministically without the
// model (and so `make demo` is reproducible on any Mac, judge requirement G5).
class IVlmWorker {
public:
    virtual ~IVlmWorker() = default;
    virtual Facts infer(const CandidateFrame& candidate) = 0;
};

// Deterministic stand-in for tests and the reproducible replay demo. Returns
// scripted Facts and, optionally, simulates inference cost so the pipeline's
// conflation/queue-wait behaviour can be exercised honestly under load.
class ScriptedVlmWorker : public IVlmWorker {
public:
    // Always return `fixed`. sim_infer_ms > 0 sleeps that long and fills the
    // encode/prefill/decode split (clearly simulated, not a real measurement).
    explicit ScriptedVlmWorker(Facts fixed, int sim_infer_ms = 0);

    // Full control: derive Facts from each candidate (e.g. by seq).
    explicit ScriptedVlmWorker(std::function<Facts(const CandidateFrame&)> fn,
                               int sim_infer_ms = 0);

    Facts infer(const CandidateFrame& candidate) override;
    uint64_t calls() const { return calls_.load(); }

private:
    std::function<Facts(const CandidateFrame&)> fn_;
    int sim_infer_ms_ = 0;
    std::atomic<uint64_t> calls_{0};
};

}  // namespace nightjar
