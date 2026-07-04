#include "nightjar/vlm_worker.h"

#include <chrono>

#include "check.h"

using namespace nightjar;

namespace {

CandidateFrame candidate(uint64_t seq) {
    CandidateFrame c;
    c.seq = seq;
    return c;
}

void test_fixed_facts() {
    Facts p;
    p.person = true;
    ScriptedVlmWorker worker(p);
    Facts got = worker.infer(candidate(1));
    CHECK(got.person);
    CHECK(!got.vehicle);
    CHECK_EQ(worker.calls(), uint64_t(1));
}

void test_functional_facts_by_seq() {
    // Odd seqs = person, even = vehicle.
    ScriptedVlmWorker worker([](const CandidateFrame& c) {
        Facts f;
        (c.seq % 2 ? f.person : f.vehicle) = true;
        return f;
    });
    CHECK(worker.infer(candidate(1)).person);
    CHECK(worker.infer(candidate(2)).vehicle);
    CHECK_EQ(worker.calls(), uint64_t(2));
}

void test_simulated_latency_and_split() {
    Facts p;
    p.person = true;
    ScriptedVlmWorker worker(p, /*sim_infer_ms=*/40);
    auto t0 = std::chrono::steady_clock::now();
    Facts got = worker.infer(candidate(1));
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    CHECK(ms >= 35);  // actually slept
    // Split fields populated and sum to ~total.
    CHECK(got.encode_ms > 0 && got.prefill_ms > 0 && got.decode_ms > 0);
    const float sum = got.encode_ms + got.prefill_ms + got.decode_ms;
    CHECK(sum > 39.0f && sum < 41.0f);
}

}  // namespace

int main() {
    test_fixed_facts();
    test_functional_facts_by_seq();
    test_simulated_latency_and_split();
    return njtest::failures() == 0 ? 0 : 1;
}
