#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include "nightjar/alert_sink.h"
#include "nightjar/best_frame_selector.h"
#include "nightjar/conflating_slot.h"
#include "nightjar/frame_view.h"
#include "nightjar/motion_gate.h"
#include "nightjar/rule_engine.h"
#include "nightjar/telemetry.h"
#include "nightjar/vlm_worker.h"

namespace nightjar {

struct PipelineConfig {
    GateConfig gate;
    BestFrameConfig best_frame;
    std::string default_zone = "any";  // zone reported for motion (single zone in v1)
};

// Wires the whole engine together (design doc §4.1). The capture callback runs
// the cheap gate + best-frame selection inline (the ".utility" path); the
// expensive VLM runs on its own thread, fed through a ConflatingSlot so a burst
// of candidates never queues — the newest wins and drops are counted. Telemetry
// stamps every stage. Dependencies are injected as interfaces so the same
// pipeline runs the reproducible replay demo and the on-device app.
class Pipeline {
public:
    Pipeline(PipelineConfig config, RuleEngine* rules, IVlmWorker* vlm, IAlertSink* sink,
             Telemetry* telemetry);
    ~Pipeline();

    // Override the wall clock (for deterministic replays / rule-time demos).
    void set_clock(std::function<Clock()> clock_fn);

    void start();  // launch the VLM consumer thread
    void stop();   // stop it and fold conflation drops into telemetry

    // Capture callback: feed one frame. Runs gate + best-frame inline.
    void on_frame(const FrameView& frame);

private:
    void vlm_loop();
    void process_candidate(const CandidateFrame& candidate);

    PipelineConfig config_;
    RuleEngine* rules_;
    IVlmWorker* vlm_;
    IAlertSink* sink_;
    Telemetry* tel_;
    std::function<Clock()> clock_fn_;

    MotionGate gate_;
    BestFrameSelector selector_;
    ConflatingSlot<CandidateFrame> slot_;

    std::thread vlm_thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> next_event_id_{1'000'000'000ull};  // disjoint from frame seqs
};

}  // namespace nightjar
