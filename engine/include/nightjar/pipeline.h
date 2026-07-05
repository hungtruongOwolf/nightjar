#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "nightjar/alert_sink.h"
#include "nightjar/best_frame_selector.h"
#include "nightjar/conflating_slot.h"
#include "nightjar/frame_view.h"
#include "nightjar/motion_gate.h"
#include "nightjar/predicate.h"
#include "nightjar/predicate_vlm.h"
#include "nightjar/rule_engine.h"  // Clock, AlertDecision
#include "nightjar/telemetry.h"
#include "nightjar/temporal_rule_engine.h"

namespace nightjar {

struct PipelineConfig {
    GateConfig gate;
    BestFrameConfig best_frame;
    std::string default_zone = "any";       // zone reported for motion (single zone in v1)
    std::vector<Predicate> predicates;       // union of the active rules' predicates to evaluate
};

// Wires the whole engine (design doc §4.1). The capture callback runs the cheap
// gate + best-frame inline; the VLM runs on its own thread, fed via a
// ConflatingSlot so candidate bursts drop-old rather than queue. Each candidate
// becomes a set of per-frame predicate answers (IPredicateVlm), packaged as an
// Observation and fed to the TemporalRuleEngine — so the pipeline fires on
// conditions a detector can't express (loitering, left-behind), not just
// object presence. Telemetry stamps every stage. Deps are interfaces, so the
// same pipeline runs the reproducible demo and the on-device app.
class Pipeline {
public:
    Pipeline(PipelineConfig config, TemporalRuleEngine* rules, IPredicateVlm* vlm, IAlertSink* sink,
             Telemetry* telemetry);
    ~Pipeline();

    void set_clock(std::function<Clock()> clock_fn);

    void start();
    void stop();

    void on_frame(const FrameView& frame);

private:
    void vlm_loop();
    void process_candidate(const CandidateFrame& candidate);

    PipelineConfig config_;
    TemporalRuleEngine* rules_;
    IPredicateVlm* vlm_;
    IAlertSink* sink_;
    Telemetry* tel_;
    std::function<Clock()> clock_fn_;

    MotionGate gate_;
    BestFrameSelector selector_;
    ConflatingSlot<CandidateFrame> slot_;

    std::thread vlm_thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> next_event_id_{1'000'000'000ull};
};

}  // namespace nightjar
