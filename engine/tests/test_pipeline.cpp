#include "nightjar/pipeline.h"

#include <chrono>
#include <thread>
#include <vector>

#include "check.h"

using namespace nightjar;

namespace {

constexpr int W = 128, H = 128;

uint64_t now_ns() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());
}

std::vector<uint8_t> flat(uint8_t v) { return std::vector<uint8_t>(size_t(W) * H, v); }

// A frame with a sharp, high-contrast blob so both the gate (big change) and
// the selector (high sharpness -> early-exit) trigger.
std::vector<uint8_t> blob_frame() {
    auto buf = flat(40);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) buf[size_t(y) * W + x] = ((x + y) & 1) ? 0 : 255;
    return buf;
}

FrameView view_of(const std::vector<uint8_t>& buf, uint64_t seq) {
    FrameView f;
    f.y_plane = buf.data();
    f.width = W;
    f.height = H;
    f.stride = W;
    f.seq = seq;
    f.ts_mono_ns = now_ns();
    return f;
}

Rule person_rule() {
    Rule r;
    r.id = "r1";
    r.subject = Subject::Person;
    r.zone_id = "any";
    r.time_window = TimeWindow{0, 0};  // always
    r.cooldown_s = 120;
    r.actions = {Action{ActionType::Ntfy, "topic"}};
    return r;
}

PipelineConfig test_pipeline_config() {
    PipelineConfig cfg;
    cfg.best_frame.early_exit_min_blob_blocks = 3;
    cfg.best_frame.early_exit_min_sharpness = 1.0;  // textured blob clears this
    return cfg;
}

void test_end_to_end_person_alert() {
    RuleEngine rules;
    rules.set_rules({person_rule()});
    Facts p;
    p.person = true;
    ScriptedVlmWorker vlm(p, /*sim_infer_ms=*/10);
    CapturingSink sink;
    Telemetry tel;

    Pipeline pipe(test_pipeline_config(), &rules, &vlm, &sink, &tel);
    pipe.set_clock([] { return Clock{23 * 60, 1000}; });  // 23:00, fixed
    pipe.start();

    auto bg = flat(40);
    pipe.on_frame(view_of(bg, 0));  // seed (no motion)
    pipe.on_frame(view_of(bg, 1));  // static (no motion)

    auto blob = blob_frame();
    pipe.on_frame(view_of(blob, 2));  // motion -> candidate -> VLM -> person -> alert

    // Let the VLM thread process.
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    pipe.stop();

    CHECK(sink.count() >= 1);
    if (sink.count() >= 1) {
        CHECK_EQ(sink.alerts()[0].rule_id, std::string("r1"));
        CHECK(sink.alerts()[0].one_liner.find("person") != std::string::npos);
        CHECK(sink.alerts()[0].one_liner.find("23:00") != std::string::npos);
    }

    Report r = tel.make_report();
    CHECK_EQ(r.counters[static_cast<size_t>(Counter::FramesCaptured)], int64_t(3));
    CHECK(r.counters[static_cast<size_t>(Counter::VlmInferences)] >= 1);
    CHECK(r.counters[static_cast<size_t>(Counter::AlertsFired)] >= 1);
    CHECK_EQ(r.segments[static_cast<size_t>(Segment::GateCost)].count, size_t(3));  // every frame
    CHECK(r.segments[static_cast<size_t>(Segment::VlmTotal)].count >= 1);
    CHECK(r.segments[static_cast<size_t>(Segment::EndToEnd)].count >= 1);
}

void test_no_person_no_alert() {
    RuleEngine rules;
    rules.set_rules({person_rule()});
    Facts animal;  // VLM sees an animal, not a person
    animal.animal = true;
    ScriptedVlmWorker vlm(animal, 5);
    CapturingSink sink;
    Telemetry tel;

    Pipeline pipe(test_pipeline_config(), &rules, &vlm, &sink, &tel);
    pipe.set_clock([] { return Clock{23 * 60, 1000}; });
    pipe.start();

    auto bg = flat(40);
    pipe.on_frame(view_of(bg, 0));
    auto blob = blob_frame();
    pipe.on_frame(view_of(blob, 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    pipe.stop();

    CHECK_EQ(sink.count(), size_t(0));  // animal doesn't match the person rule
    CHECK(tel.make_report().counters[static_cast<size_t>(Counter::VlmInferences)] >= 1);
}

void test_cooldown_limits_repeat_alerts() {
    RuleEngine rules;
    rules.set_rules({person_rule()});  // cooldown 120s
    Facts p;
    p.person = true;
    ScriptedVlmWorker vlm(p, 5);
    CapturingSink sink;
    Telemetry tel;

    Pipeline pipe(test_pipeline_config(), &rules, &vlm, &sink, &tel);
    pipe.set_clock([] { return Clock{23 * 60, 1000}; });  // fixed time => within cooldown
    pipe.start();

    auto bg = flat(40);
    pipe.on_frame(view_of(bg, 0));
    // Several separated motion events at the same clock second.
    for (uint64_t i = 1; i <= 6; ++i) {
        auto blob = blob_frame();
        pipe.on_frame(view_of(blob, i));
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        pipe.on_frame(view_of(bg, i + 100));  // quiet between events
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    pipe.stop();

    CHECK_EQ(sink.count(), size_t(1));  // cooldown collapses repeats to one
}

}  // namespace

int main() {
    test_end_to_end_person_alert();
    test_no_person_no_alert();
    test_cooldown_limits_repeat_alerts();
    return njtest::failures() == 0 ? 0 : 1;
}
