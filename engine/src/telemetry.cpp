#include "nightjar/telemetry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace nightjar {
namespace {

// Which two stamps bound each reported segment.
struct SegmentDef {
    Segment segment;
    Stage from;
    Stage to;
};

constexpr SegmentDef kSegmentDefs[] = {
    {Segment::GateCost, Stage::Capture, Stage::GateVerdict},
    {Segment::BestFrameWindow, Stage::GateVerdict, Stage::CandidatePublish},
    {Segment::Encode, Stage::VlmStart, Stage::EncodeDone},
    {Segment::Prefill, Stage::EncodeDone, Stage::PrefillDone},
    {Segment::Decode, Stage::PrefillDone, Stage::DecodeDone},
    {Segment::VlmTotal, Stage::VlmStart, Stage::DecodeDone},
    {Segment::RuleMatch, Stage::DecodeDone, Stage::RuleMatch},
    {Segment::Post, Stage::RuleMatch, Stage::PostDone},
    {Segment::EndToEnd, Stage::Capture, Stage::PostDone},
};

const char* segment_name(Segment s) {
    switch (s) {
        case Segment::GateCost: return "gate_cost";
        case Segment::BestFrameWindow: return "best_frame_window";
        case Segment::Encode: return "encode";
        case Segment::Prefill: return "prefill";
        case Segment::Decode: return "decode";
        case Segment::VlmTotal: return "vlm_total";
        case Segment::RuleMatch: return "rule_match";
        case Segment::Post: return "post";
        case Segment::EndToEnd: return "end_to_end";
        case Segment::Count: break;
    }
    return "?";
}

const char* counter_name(Counter c) {
    switch (c) {
        case Counter::FramesCaptured: return "frames_captured";
        case Counter::FramesGated: return "frames_gated";
        case Counter::SuppressedGlobal: return "suppressed_global";
        case Counter::ConflationDrops: return "conflation_drops";
        case Counter::VlmInferences: return "vlm_inferences";
        case Counter::AlertsFired: return "alerts_fired";
        case Counter::Count: break;
    }
    return "?";
}

// Nearest-rank percentile on an already-sorted vector. p in [0,100].
double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    size_t idx = static_cast<size_t>(std::ceil(p / 100.0 * sorted.size()));
    if (idx > 0) --idx;
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    return sorted[idx];
}

Distribution summarize(std::vector<double> values) {
    Distribution d;
    d.count = values.size();
    if (values.empty()) return d;
    std::sort(values.begin(), values.end());
    d.min = values.front();
    d.max = values.back();
    double sum = 0;
    for (double v : values) sum += v;
    d.mean = sum / values.size();
    d.p50 = percentile(values, 50);
    d.p90 = percentile(values, 90);
    d.p99 = percentile(values, 99);
    return d;
}

}  // namespace

void Telemetry::stamp(Stage stage, uint64_t seq, uint64_t ts_ns) {
    std::lock_guard<std::mutex> lock(mu_);
    in_flight_[seq].ts[static_cast<size_t>(stage)] = ts_ns;
}

void Telemetry::finalize(uint64_t seq) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = in_flight_.find(seq);
    if (it == in_flight_.end()) return;
    const StampSet& s = it->second;

    for (const auto& def : kSegmentDefs) {
        const auto& a = s.ts[static_cast<size_t>(def.from)];
        const auto& b = s.ts[static_cast<size_t>(def.to)];
        if (!a || !b || *b < *a) continue;  // missing endpoint or clock skew
        const double ms = static_cast<double>(*b - *a) / 1e6;
        samples_[static_cast<size_t>(def.segment)].push_back(ms);
    }
    in_flight_.erase(it);
}

void Telemetry::counter(Counter c, int64_t delta) {
    std::lock_guard<std::mutex> lock(mu_);
    counters_[static_cast<size_t>(c)] += delta;
}

void Telemetry::sample_memory(int64_t available_bytes) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!min_available_bytes_ || available_bytes < *min_available_bytes_) {
        min_available_bytes_ = available_bytes;
    }
}

Report Telemetry::make_report() const {
    std::lock_guard<std::mutex> lock(mu_);
    Report r;
    for (size_t i = 0; i < samples_.size(); ++i) r.segments[i] = summarize(samples_[i]);
    r.counters = counters_;
    r.min_available_bytes = min_available_bytes_;
    return r;
}

size_t Telemetry::in_flight() const {
    std::lock_guard<std::mutex> lock(mu_);
    return in_flight_.size();
}

std::string Report::to_markdown() const {
    std::string out = "# Nightjar run report\n\n## Latency per stage (ms)\n\n";
    out += "| stage | count | p50 | p90 | p99 | min | max | mean |\n";
    out += "|---|---:|---:|---:|---:|---:|---:|---:|\n";
    char line[256];
    for (int i = 0; i < static_cast<int>(Segment::Count); ++i) {
        const Distribution& d = segments[i];
        std::snprintf(line, sizeof(line),
                      "| %s | %zu | %.1f | %.1f | %.1f | %.1f | %.1f | %.1f |\n",
                      segment_name(static_cast<Segment>(i)), d.count, d.p50, d.p90, d.p99,
                      d.min, d.max, d.mean);
        out += line;
    }

    out += "\n## Counters\n\n| counter | value |\n|---|---:|\n";
    for (int i = 0; i < static_cast<int>(Counter::Count); ++i) {
        std::snprintf(line, sizeof(line), "| %s | %lld |\n", counter_name(static_cast<Counter>(i)),
                      static_cast<long long>(counters[i]));
        out += line;
    }

    if (min_available_bytes) {
        std::snprintf(line, sizeof(line),
                      "\n## Memory\n\nmin available (jetsam headroom): %.1f MB\n",
                      static_cast<double>(*min_available_bytes) / (1024.0 * 1024.0));
        out += line;
    }
    return out;
}

}  // namespace nightjar
