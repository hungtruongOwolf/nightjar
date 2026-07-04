// Micro-benchmark for the Tier-1 gate. Reports per-step scalar-vs-NEON timing
// on a 640×480 Y-plane and the full MotionGate::evaluate() per-frame cost, to
// check the <0.5ms/frame budget (design doc §3.2) with a real measured number.
//
// Not a correctness test (that's test_gate_steps / test_motion_gate). Build and
// run: it prints a markdown table for the report.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include "nightjar/gate_steps.h"
#include "nightjar/motion_gate.h"

using namespace nightjar;
using clock_t_ = std::chrono::steady_clock;

namespace {

constexpr int W = 640, H = 480;
constexpr size_t N = size_t(W) * H;

std::vector<uint8_t> random_bytes(size_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::vector<uint8_t> v(n);
    for (auto& b : v) b = static_cast<uint8_t>(rng() & 0xFF);
    return v;
}

template <typename F>
double time_us(int iters, F&& fn) {
    // Warm up, then time `iters` runs; return microseconds per run.
    fn();
    auto t0 = clock_t_::now();
    for (int i = 0; i < iters; ++i) fn();
    auto t1 = clock_t_::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1e3 / iters;
}

}  // namespace

int main() {
    const int iters = 2000;
    auto a = random_bytes(N, 1);
    auto b = random_bytes(N, 2);
    std::vector<uint8_t> out(N), bin(N), bg8(N);
    std::vector<uint16_t> bg16(N);
    for (size_t i = 0; i < N; ++i) bg16[i] = static_cast<uint16_t>(a[i] << 8);
    std::vector<uint16_t> counts(size_t(W / 16) * (H / 16));

    std::printf("# Gate micro-bench (640x480, %d iters/run)\n\n", iters);
    std::printf("NEON available: %s\n\n", NIGHTJAR_HAS_NEON ? "yes" : "no");
    std::printf("| step | scalar us | neon us | speedup |\n|---|---:|---:|---:|\n");

    auto row = [&](const char* name, double s, double n) {
        std::printf("| %s | %.1f | %.1f | %.2fx |\n", name, s, n, n > 0 ? s / n : 0.0);
    };

#if NIGHTJAR_HAS_NEON
    row("abs_diff",
        time_us(iters, [&] { gate::abs_diff_scalar(a.data(), b.data(), out.data(), N); }),
        time_us(iters, [&] { gate::abs_diff_neon(a.data(), b.data(), out.data(), N); }));
    row("ema_update",
        time_us(iters, [&] { gate::ema_update_scalar(bg16.data(), a.data(), N, 6); }),
        time_us(iters, [&] { gate::ema_update_neon(bg16.data(), a.data(), N, 6); }));
    row("project_high_byte",
        time_us(iters, [&] { gate::project_high_byte_scalar(bg16.data(), bg8.data(), N); }),
        time_us(iters, [&] { gate::project_high_byte_neon(bg16.data(), bg8.data(), N); }));
    row("threshold",
        time_us(iters, [&] { gate::threshold_to_binary_scalar(a.data(), bin.data(), N, 20); }),
        time_us(iters, [&] { gate::threshold_to_binary_neon(a.data(), bin.data(), N, 20); }));
    row("block_counts",
        time_us(iters, [&] { gate::block_counts_scalar(bin.data(), W, H, 16, counts.data()); }),
        time_us(iters, [&] { gate::block_counts_neon(bin.data(), W, H, 16, counts.data()); }));
#endif

    // Full gate: seed once, then time evaluate() on a fresh blob frame.
    MotionGate gate(GateConfig{});
    FrameView f;
    f.y_plane = a.data();
    f.width = W;
    f.height = H;
    f.stride = W;
    gate.evaluate(f);  // seed
    double per_frame = time_us(iters, [&] {
        f.y_plane = b.data();
        f.seq++;
        gate.evaluate(f);
    });
    std::printf("\nFull MotionGate::evaluate(): **%.1f us/frame** (budget 500us)\n", per_frame);
    std::printf("verdict: %s\n", per_frame < 500.0 ? "PASS <0.5ms" : "OVER BUDGET");
    return 0;
}
