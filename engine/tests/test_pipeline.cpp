#include "nightjar/pipeline.h"

#include <chrono>
#include <filesystem>
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

TemporalRule person_appears() {
    TemporalRule r;
    r.id = "r1";
    r.raw_text = "notify me if a person appears";
    r.predicate = "person";
    r.trigger = Trigger::Appears;
    r.zone_id = "any";
    r.time_window = TimeWindow{0, 0};
    r.cooldown_s = 120;
    r.actions = {Action{ActionType::Ntfy, "topic"}};
    return r;
}

PipelineConfig test_config() {
    PipelineConfig cfg;
    cfg.best_frame.early_exit_min_blob_blocks = 3;
    cfg.best_frame.early_exit_min_sharpness = 1.0;
    cfg.predicates = {{"person", "Is there a person? Answer y or n."}};
    return cfg;
}

void test_end_to_end_person_alert() {
    TemporalRuleEngine rules;
    rules.set_rules({person_appears()});
    ScriptedPredicateVlm vlm({{"person", true}}, /*sim_infer_ms=*/10);
    CapturingSink sink;
    Telemetry tel;

    Pipeline pipe(test_config(), &rules, &vlm, &sink, &tel);
    pipe.set_clock([] { return Clock{23 * 60, 1000}; });
    pipe.start();

    auto bg = flat(40);
    pipe.on_frame(view_of(bg, 0));  // seed
    pipe.on_frame(view_of(bg, 1));  // static
    auto blob = blob_frame();
    pipe.on_frame(view_of(blob, 2));  // motion -> candidate -> person -> appears -> alert

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    pipe.stop();

    CHECK(sink.count() >= 1);
    if (sink.count() >= 1) {
        CHECK_EQ(sink.alerts()[0].rule_id, std::string("r1"));
        CHECK(sink.alerts()[0].one_liner.find("person appears") != std::string::npos);
    }
    Report r = tel.make_report();
    CHECK_EQ(r.counters[static_cast<size_t>(Counter::FramesCaptured)], int64_t(3));
    CHECK(r.counters[static_cast<size_t>(Counter::VlmInferences)] >= 1);
    CHECK(r.counters[static_cast<size_t>(Counter::AlertsFired)] >= 1);
    CHECK_EQ(r.segments[static_cast<size_t>(Segment::GateCost)].count, size_t(3));
    CHECK(r.segments[static_cast<size_t>(Segment::EndToEnd)].count >= 1);
}

void test_no_person_no_alert() {
    TemporalRuleEngine rules;
    rules.set_rules({person_appears()});
    ScriptedPredicateVlm vlm({{"person", false}}, 5);  // VLM sees no person
    CapturingSink sink;
    Telemetry tel;

    Pipeline pipe(test_config(), &rules, &vlm, &sink, &tel);
    pipe.set_clock([] { return Clock{23 * 60, 1000}; });
    pipe.start();
    auto bg = flat(40);
    pipe.on_frame(view_of(bg, 0));
    auto blob = blob_frame();
    pipe.on_frame(view_of(blob, 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    pipe.stop();

    CHECK_EQ(sink.count(), size_t(0));
    CHECK(tel.make_report().counters[static_cast<size_t>(Counter::VlmInferences)] >= 1);
}

void test_loitering_needs_dwell() {
    // A sustained/loitering rule must NOT fire on first sight; the pipeline uses
    // real wall-clock, so with a fixed clock (dwell never elapses) it stays silent.
    TemporalRule loiter = person_appears();
    loiter.trigger = Trigger::Sustained;
    loiter.dwell_s = 3600;  // 1h, will never elapse in the test
    loiter.raw_text = "someone loitering";
    TemporalRuleEngine rules;
    rules.set_rules({loiter});
    ScriptedPredicateVlm vlm({{"person", true}}, 5);
    CapturingSink sink;
    Telemetry tel;

    Pipeline pipe(test_config(), &rules, &vlm, &sink, &tel);
    pipe.set_clock([] { return Clock{23 * 60, 5000}; });  // fixed time
    pipe.start();
    auto bg = flat(40);
    pipe.on_frame(view_of(bg, 0));
    auto blob = blob_frame();
    for (uint64_t i = 1; i <= 4; ++i) {
        pipe.on_frame(view_of(blob, i));
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    pipe.stop();
    CHECK_EQ(sink.count(), size_t(0));  // dwell never reached -> no loitering alert
}

void test_event_captures_clip_and_detail() {
    namespace fs = std::filesystem;
    fs::path clipdir = fs::temp_directory_path() / "nj_pipe_clips";
    fs::remove_all(clipdir);

    TemporalRuleEngine rules;
    rules.set_rules({person_appears()});
    ScriptedPredicateVlm vlm({{"person", true}}, 10);
    CapturingSink sink;
    Telemetry tel;
    ClipConfig ccfg;
    ccfg.dir = clipdir.string();
    ccfg.pre_roll_frames = 3;
    ccfg.post_roll_frames = 4;
    ccfg.encoder = pgm_encoder();
    EventClipStore clips(ccfg);

    Pipeline pipe(test_config(), &rules, &vlm, &sink, &tel, &clips);
    pipe.set_clock([] { return Clock{23 * 60, 1000}; });
    pipe.start();

    auto bg = flat(40);
    auto blob = blob_frame();
    pipe.on_frame(view_of(bg, 0));
    pipe.on_frame(view_of(bg, 1));
    pipe.on_frame(view_of(blob, 2));  // event
    for (uint64_t i = 3; i < 10; ++i) pipe.on_frame(view_of(blob, i));  // post-roll frames
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    pipe.stop();
    clips.flush();

    CHECK(sink.count() >= 1);
    if (sink.count() >= 1)
        CHECK(sink.alerts()[0].one_liner.find("appeared") != std::string::npos);  // fact detail
    CHECK(clips.stored_clips() >= 1);  // an evidence clip was captured
    // The clip directory has frames (a span, not one image).
    bool has_frames = false;
    std::error_code ec;
    for (auto& e : fs::recursive_directory_iterator(clipdir, ec))
        if (e.path().extension() == ".pgm") has_frames = true;
    CHECK(has_frames);
    fs::remove_all(clipdir);
}

}  // namespace

int main() {
    test_end_to_end_person_alert();
    test_no_person_no_alert();
    test_loitering_needs_dwell();
    test_event_captures_clip_and_detail();
    return njtest::failures() == 0 ? 0 : 1;
}
