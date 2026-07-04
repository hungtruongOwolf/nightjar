#include "nightjar/gate_steps.h"

#include <cstdint>
#include <random>
#include <vector>

#include "check.h"

using namespace nightjar::gate;

namespace {

// Deterministic pseudo-random byte buffer.
std::vector<uint8_t> random_bytes(size_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> d(0, 255);
    std::vector<uint8_t> v(n);
    for (auto& b : v) b = static_cast<uint8_t>(d(rng));
    return v;
}

// The whole point of the module: scalar and NEON must agree bit-for-bit.
// (When built on non-ARM these tests are trivially skipped.)
#if NIGHTJAR_HAS_NEON

void test_abs_diff_parity() {
    // Include a non-multiple-of-16 size to exercise the scalar tail.
    for (size_t n : {size_t(64), size_t(1000), size_t(640 * 480 + 7)}) {
        auto a = random_bytes(n, 1);
        auto b = random_bytes(n, 2);
        std::vector<uint8_t> s(n), v(n);
        abs_diff_scalar(a.data(), b.data(), s.data(), n);
        abs_diff_neon(a.data(), b.data(), v.data(), n);
        CHECK(s == v);
    }
}

void test_ema_update_parity() {
    for (int shift : {1, 3, 6, 8}) {
        const size_t n = 1000 + 7;
        auto frame = random_bytes(n, 10 + shift);
        // Seed two identical backgrounds, advance both, compare.
        std::vector<uint16_t> s(n), v(n);
        for (size_t i = 0; i < n; ++i) s[i] = v[i] = static_cast<uint16_t>((i * 37) & 0xFFFF);
        ema_update_scalar(s.data(), frame.data(), n, shift);
        ema_update_neon(v.data(), frame.data(), n, shift);
        CHECK(s == v);
    }
}

void test_ema_converges() {
    // A background fed a constant frame must converge toward that value.
    const size_t n = 16;
    std::vector<uint16_t> bg(n, 0);
    std::vector<uint8_t> frame(n, 200);
    for (int step = 0; step < 500; ++step) ema_update_scalar(bg.data(), frame.data(), n, 6);
    std::vector<uint8_t> bg8(n);
    project_high_byte_scalar(bg.data(), bg8.data(), n);
    CHECK(bg8[0] >= 199 && bg8[0] <= 200);  // reached the target (fixed-point precision holds)
}

void test_project_high_byte_parity() {
    const size_t n = 500 + 3;
    std::vector<uint16_t> bg(n);
    std::mt19937 rng(77);
    for (auto& x : bg) x = static_cast<uint16_t>(rng() & 0xFFFF);
    std::vector<uint8_t> s(n), v(n);
    project_high_byte_scalar(bg.data(), s.data(), n);
    project_high_byte_neon(bg.data(), v.data(), n);
    CHECK(s == v);
}

void test_threshold_parity() {
    for (uint8_t thr : {uint8_t(1), uint8_t(20), uint8_t(128), uint8_t(255)}) {
        const size_t n = 800 + 5;
        auto diff = random_bytes(n, 100 + thr);
        std::vector<uint8_t> s(n), v(n);
        threshold_to_binary_scalar(diff.data(), s.data(), n, thr);
        threshold_to_binary_neon(diff.data(), v.data(), n, thr);
        CHECK(s == v);
    }
}

void test_block_counts_parity() {
    const int w = 64, h = 48, block = 16;  // 4×3 grid
    auto bin_bytes = random_bytes(size_t(w) * h, 55);
    for (auto& b : bin_bytes) b = b & 1;  // 0/1 mask
    const int g = (w / block) * (h / block);
    std::vector<uint16_t> s(g), v(g);
    block_counts_scalar(bin_bytes.data(), w, h, block, s.data());
    block_counts_neon(bin_bytes.data(), w, h, block, v.data());
    CHECK(s == v);
}

void test_block_counts_known_values() {
    // All-ones 16×16 single block => count 256; all-zeros => 0.
    const int w = 32, h = 16, block = 16;  // 2×1 grid
    std::vector<uint8_t> bin(size_t(w) * h, 0);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) bin[size_t(y) * w + x] = 1;  // left block all ones
    std::vector<uint16_t> counts(2);
    block_counts_scalar(bin.data(), w, h, block, counts.data());
    CHECK_EQ(counts[0], uint16_t(256));
    CHECK_EQ(counts[1], uint16_t(0));
}

#endif  // NIGHTJAR_HAS_NEON

}  // namespace

int main() {
#if NIGHTJAR_HAS_NEON
    test_abs_diff_parity();
    test_ema_update_parity();
    test_ema_converges();
    test_project_high_byte_parity();
    test_threshold_parity();
    test_block_counts_parity();
    test_block_counts_known_values();
#endif
    return njtest::failures() == 0 ? 0 : 1;
}
