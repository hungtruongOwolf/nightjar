#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nightjar {

// Pipeline timestamps for one frame's journey (design doc §4.4, §5.6).
// Stamped as the frame flows through; not every frame reaches every stage
// (most are discarded at the gate and never start the VLM).
enum class Stage : int {
    Capture = 0,       // t0  — frame arrives from capture
    GateVerdict,       // t1  — motion gate decided
    CandidatePublish,  // t2  — best-frame window closed, candidate published
    VlmStart,          // t3  — VLM inference began
    EncodeDone,        // t3a — vision encode finished
    PrefillDone,       // t3b — prompt prefill finished
    DecodeDone,        // t4  — token decode finished
    RuleMatch,         // t5  — rule engine decided
    PostDone,          // t6  — alert POST completed
    Count
};

// Monotonic counters over a run.
enum class Counter : int {
    FramesCaptured = 0,
    FramesGated,        // passed the motion gate
    SuppressedGlobal,   // global-illumination suppressor fired
    ConflationDrops,    // frames dropped by the ConflatingSlot
    VlmInferences,
    AlertsFired,
    Count
};

// A derived per-stage duration we report. Segments with a missing endpoint
// (e.g. Encode for a frame that never reached the VLM) are simply not recorded.
enum class Segment : int {
    GateCost = 0,   // GateVerdict - Capture      (Tier 1, per frame)
    BestFrameWindow,// CandidatePublish - GateVerdict
    Encode,         // EncodeDone - VlmStart
    Prefill,        // PrefillDone - EncodeDone
    Decode,         // DecodeDone - PrefillDone
    VlmTotal,       // DecodeDone - VlmStart
    RuleMatch,      // RuleMatch - DecodeDone
    Post,           // PostDone - RuleMatch
    EndToEnd,       // PostDone - Capture
    Count
};

struct Distribution {
    size_t count = 0;
    double p50 = 0, p90 = 0, p99 = 0, min = 0, max = 0, mean = 0;
};

struct Report {
    std::array<Distribution, static_cast<size_t>(Segment::Count)> segments;
    std::array<int64_t, static_cast<size_t>(Counter::Count)> counters{};
    std::optional<int64_t> min_available_bytes;  // worst-case jetsam headroom seen
    std::string to_markdown() const;
};

// Collects stamps/counters/memory samples across threads and produces the
// self-generating report. Thread-safe: the gate runs on one QoS queue, the
// VLM on another, so stamps arrive concurrently.
class Telemetry {
public:
    // Record the time a frame reached a stage. ts_ns is a monotonic clock.
    void stamp(Stage stage, uint64_t seq, uint64_t ts_ns);

    // Compute all available segment durations for this frame and fold them
    // into the distributions, then release the frame's stamps. Call once a
    // frame's journey ends (discarded, or alert posted).
    void finalize(uint64_t seq);

    void counter(Counter c, int64_t delta = 1);

    // Platform passes the available-memory reading (os_proc_available_memory
    // on Apple); the portable core never calls a platform API itself.
    void sample_memory(int64_t available_bytes);

    Report make_report() const;

    // Number of frames still in flight (stamped but not finalized) — lets the
    // harness assert it isn't leaking stamp state.
    size_t in_flight() const;

private:
    struct StampSet {
        std::array<std::optional<uint64_t>, static_cast<size_t>(Stage::Count)> ts;
    };

    mutable std::mutex mu_;
    std::unordered_map<uint64_t, StampSet> in_flight_;
    std::array<std::vector<double>, static_cast<size_t>(Segment::Count)> samples_;
    std::array<int64_t, static_cast<size_t>(Counter::Count)> counters_{};
    std::optional<int64_t> min_available_bytes_;
};

}  // namespace nightjar
