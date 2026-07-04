#include "nightjar/telemetry.h"

#include <string>
#include <thread>
#include <vector>

#include "check.h"

using namespace nightjar;

namespace {

constexpr uint64_t ms = 1'000'000;  // ns per ms

// A full-journey frame: stamp every stage, finalize, check the derived
// segment durations come out as the stamp differences.
void test_segments_from_stamps() {
    Telemetry t;
    uint64_t seq = 1;
    t.stamp(Stage::Capture, seq, 0);
    t.stamp(Stage::GateVerdict, seq, 1 * ms);       // gate_cost = 1ms
    t.stamp(Stage::CandidatePublish, seq, 201 * ms);// best_frame_window = 200ms
    t.stamp(Stage::VlmStart, seq, 210 * ms);
    t.stamp(Stage::EncodeDone, seq, 310 * ms);      // encode = 100ms
    t.stamp(Stage::PrefillDone, seq, 610 * ms);     // prefill = 300ms
    t.stamp(Stage::DecodeDone, seq, 1110 * ms);     // decode = 500ms; vlm_total = 900ms
    t.stamp(Stage::RuleMatch, seq, 1111 * ms);      // rule_match = 1ms
    t.stamp(Stage::PostDone, seq, 1411 * ms);       // post = 300ms; end_to_end = 1411ms
    t.finalize(seq);

    Report r = t.make_report();
    auto seg = [&](Segment s) { return r.segments[static_cast<size_t>(s)]; };
    CHECK_EQ(seg(Segment::GateCost).p50, 1.0);
    CHECK_EQ(seg(Segment::BestFrameWindow).p50, 200.0);
    CHECK_EQ(seg(Segment::Encode).p50, 100.0);
    CHECK_EQ(seg(Segment::Prefill).p50, 300.0);
    CHECK_EQ(seg(Segment::Decode).p50, 500.0);
    CHECK_EQ(seg(Segment::VlmTotal).p50, 900.0);
    CHECK_EQ(seg(Segment::Post).p50, 300.0);
    CHECK_EQ(seg(Segment::EndToEnd).p50, 1411.0);
    CHECK_EQ(t.in_flight(), size_t(0));  // finalize released the stamps
}

// A gate-discarded frame reaches only Capture+GateVerdict: gate_cost is
// recorded, downstream segments are not.
void test_partial_journey_only_records_available_segments() {
    Telemetry t;
    t.stamp(Stage::Capture, 7, 0);
    t.stamp(Stage::GateVerdict, 7, 2 * ms);
    t.finalize(7);

    Report r = t.make_report();
    CHECK_EQ(r.segments[static_cast<size_t>(Segment::GateCost)].count, size_t(1));
    CHECK_EQ(r.segments[static_cast<size_t>(Segment::Encode)].count, size_t(0));
    CHECK_EQ(r.segments[static_cast<size_t>(Segment::EndToEnd)].count, size_t(0));
}

// Percentiles over a known 1..100ms set: p50=50, p90=90, p99=99 (nearest-rank).
void test_percentiles() {
    Telemetry t;
    for (int i = 1; i <= 100; ++i) {
        uint64_t seq = 1000 + i;
        t.stamp(Stage::VlmStart, seq, 0);
        t.stamp(Stage::EncodeDone, seq, uint64_t(i) * ms);
        t.finalize(seq);
    }
    Report r = t.make_report();
    const Distribution& d = r.segments[static_cast<size_t>(Segment::Encode)];
    CHECK_EQ(d.count, size_t(100));
    CHECK_EQ(d.min, 1.0);
    CHECK_EQ(d.max, 100.0);
    CHECK_EQ(d.p50, 50.0);
    CHECK_EQ(d.p90, 90.0);
    CHECK_EQ(d.p99, 99.0);
}

void test_counters_and_memory() {
    Telemetry t;
    t.counter(Counter::FramesCaptured, 300);
    t.counter(Counter::FramesGated);         // +1 default
    t.counter(Counter::FramesGated);         // +1
    t.counter(Counter::AlertsFired, 5);
    t.sample_memory(900 * 1024 * 1024);
    t.sample_memory(700 * 1024 * 1024);      // lower — becomes the min
    t.sample_memory(800 * 1024 * 1024);

    Report r = t.make_report();
    CHECK_EQ(r.counters[static_cast<size_t>(Counter::FramesCaptured)], int64_t(300));
    CHECK_EQ(r.counters[static_cast<size_t>(Counter::FramesGated)], int64_t(2));
    CHECK_EQ(r.counters[static_cast<size_t>(Counter::AlertsFired)], int64_t(5));
    CHECK(r.min_available_bytes.has_value());
    CHECK_EQ(*r.min_available_bytes, int64_t(700 * 1024 * 1024));
}

void test_report_markdown_contains_key_rows() {
    Telemetry t;
    t.stamp(Stage::VlmStart, 1, 0);
    t.stamp(Stage::EncodeDone, 1, 100 * ms);
    t.finalize(1);
    t.counter(Counter::AlertsFired, 3);
    std::string md = t.make_report().to_markdown();
    CHECK(md.find("encode") != std::string::npos);
    CHECK(md.find("alerts_fired") != std::string::npos);
    CHECK(md.find("Latency per stage") != std::string::npos);
}

// Concurrent stamping from many threads must not corrupt state (mutex).
void test_thread_safe_stamping() {
    Telemetry t;
    const int threads = 8, per = 200;
    std::vector<std::thread> pool;
    for (int th = 0; th < threads; ++th) {
        pool.emplace_back([&, th]() {
            for (int i = 0; i < per; ++i) {
                uint64_t seq = uint64_t(th) * 100000 + i;
                t.stamp(Stage::VlmStart, seq, 0);
                t.stamp(Stage::EncodeDone, seq, 5 * ms);
                t.finalize(seq);
                t.counter(Counter::VlmInferences);
            }
        });
    }
    for (auto& p : pool) p.join();

    Report r = t.make_report();
    CHECK_EQ(r.counters[static_cast<size_t>(Counter::VlmInferences)], int64_t(threads * per));
    CHECK_EQ(r.segments[static_cast<size_t>(Segment::Encode)].count, size_t(threads * per));
    CHECK_EQ(t.in_flight(), size_t(0));
}

}  // namespace

int main() {
    test_segments_from_stamps();
    test_partial_journey_only_records_available_segments();
    test_percentiles();
    test_counters_and_memory();
    test_report_markdown_contains_key_rows();
    test_thread_safe_stamping();
    return njtest::failures() == 0 ? 0 : 1;
}
