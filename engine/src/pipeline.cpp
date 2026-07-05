#include "nightjar/pipeline.h"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace nightjar {
namespace {

using steady = std::chrono::steady_clock;

uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(steady::now().time_since_epoch())
            .count());
}

Clock real_clock() {
    const std::time_t t = std::time(nullptr);
    std::tm lt{};
    localtime_r(&t, &lt);
    return Clock{lt.tm_hour * 60 + lt.tm_min, static_cast<int64_t>(t)};
}

const char* subject_word(Subject s) {
    switch (s) {
        case Subject::Person: return "person";
        case Subject::Vehicle: return "vehicle";
        case Subject::Animal: return "animal";
        case Subject::Package: return "package";
    }
    return "activity";
}

std::string one_liner(const AlertDecision& d, const std::string& zone, int minute_of_day) {
    char buf[192];
    // Prefer the rule's own English phrase; fall back to a generic line.
    if (!d.label.empty()) {
        std::snprintf(buf, sizeof(buf), "%s (%s, %02d:%02d)", d.label.c_str(), zone.c_str(),
                      minute_of_day / 60, minute_of_day % 60);
    } else {
        std::snprintf(buf, sizeof(buf), "%s in %s at %02d:%02d", subject_word(d.subject),
                      zone.c_str(), minute_of_day / 60, minute_of_day % 60);
    }
    return buf;
}

}  // namespace

Pipeline::Pipeline(PipelineConfig config, TemporalRuleEngine* rules, IPredicateVlm* vlm,
                   IAlertSink* sink, Telemetry* telemetry)
    : config_(std::move(config)),
      rules_(rules),
      vlm_(vlm),
      sink_(sink),
      tel_(telemetry),
      clock_fn_(real_clock),
      gate_(config_.gate),
      selector_(config_.best_frame) {}

Pipeline::~Pipeline() { stop(); }

void Pipeline::set_clock(std::function<Clock()> clock_fn) { clock_fn_ = std::move(clock_fn); }

void Pipeline::start() {
    if (running_.exchange(true)) return;
    vlm_thread_ = std::thread([this] { vlm_loop(); });
}

void Pipeline::stop() {
    if (!running_.exchange(false)) return;
    slot_.close();
    if (vlm_thread_.joinable()) vlm_thread_.join();
    tel_->counter(Counter::ConflationDrops, static_cast<int64_t>(slot_.drops()));
}

void Pipeline::on_frame(const FrameView& frame) {
    const uint64_t seq = frame.seq;
    tel_->stamp(Stage::Capture, seq, frame.ts_mono_ns);
    tel_->counter(Counter::FramesCaptured);

    const GateResult gate = gate_.evaluate(frame);
    tel_->stamp(Stage::GateVerdict, seq, now_ns());
    if (gate.suppressed_global) tel_->counter(Counter::SuppressedGlobal);
    if (gate.motion) tel_->counter(Counter::FramesGated);
    tel_->finalize(seq);  // records gate_cost

    auto candidate = selector_.offer(frame, gate, now_ns());
    if (!candidate) return;

    const uint64_t ev = next_event_id_.fetch_add(1);
    candidate->event_id = ev;
    tel_->stamp(Stage::Capture, ev, candidate->ts_mono_ns);
    tel_->stamp(Stage::CandidatePublish, ev, now_ns());
    slot_.publish(std::move(*candidate));
}

void Pipeline::vlm_loop() {
    using namespace std::chrono_literals;
    while (running_.load()) {
        auto candidate = slot_.take_blocking(100ms);
        if (!candidate) continue;
        process_candidate(*candidate);
    }
    while (auto candidate = slot_.try_take()) process_candidate(*candidate);
}

void Pipeline::process_candidate(const CandidateFrame& candidate) {
    const uint64_t ev = candidate.event_id;
    tel_->counter(Counter::VlmInferences);

    // Letterbox the raw crop HERE, on the VLM thread — off the capture fast path.
    CandidateFrame c = candidate;
    c.image = crop_and_letterbox(c.crop.data(), c.crop_w, c.crop_h, c.crop_w,
                                 Rect{0, 0, c.crop_w, c.crop_h}, config_.best_frame.letterbox_size);

    const uint64_t t_start = now_ns();
    tel_->stamp(Stage::VlmStart, ev, t_start);
    const PredicateResult facts = vlm_->evaluate(c, config_.predicates);
    const uint64_t t_decode = now_ns();

    const uint64_t enc_ns = t_start + static_cast<uint64_t>(facts.encode_ms * 1e6);
    const uint64_t pre_ns = enc_ns + static_cast<uint64_t>(facts.prefill_ms * 1e6);
    tel_->stamp(Stage::EncodeDone, ev, enc_ns);
    tel_->stamp(Stage::PrefillDone, ev, pre_ns);
    tel_->stamp(Stage::DecodeDone, ev, t_decode);

    const Clock now = clock_fn_();
    Observation obs;
    obs.predicates = facts.answers;
    obs.zone = config_.default_zone;
    obs.minute_of_day = now.minute_of_day;
    obs.unix_s = now.unix_s;

    const auto decisions = rules_->observe(obs);
    tel_->stamp(Stage::RuleMatch, ev, now_ns());

    for (const AlertDecision& d : decisions) {
        Alert alert;
        alert.rule_id = d.rule_id;
        alert.subject = d.subject;
        alert.one_liner = one_liner(d, config_.default_zone, now.minute_of_day);
        alert.unix_s = now.unix_s;
        alert.image = c.image;
        sink_->send(alert);
        tel_->counter(Counter::AlertsFired);
    }

    tel_->stamp(Stage::PostDone, ev, now_ns());
    tel_->finalize(ev);
}

}  // namespace nightjar
