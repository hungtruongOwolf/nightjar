#pragma once

#include <cstddef>
#include <cstdint>

// The per-pixel steps of the Tier-1 motion gate. Each has a scalar reference
// and a NEON implementation that must be bit-identical (parity is unit-tested,
// CLAUDE.md §4). The dispatch functions pick NEON when available at runtime.
//
// Fixed-point integer arithmetic throughout — no float — so scalar and NEON
// produce exactly the same bytes (float FMA/ordering could diverge).

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define NIGHTJAR_HAS_NEON 1
#else
#define NIGHTJAR_HAS_NEON 0
#endif

namespace nightjar::gate {

// Step 1 — absolute difference of frame vs background byte view (vabdq_u8).
void abs_diff_scalar(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t n);

// Step 2 — EMA background update in Q8.8 fixed point.
// Uses the identity bg' = bg - (bg>>shift) + (frame << (8-shift)), which keeps
// every term in uint16 range (no overflow) and needs sub-integer precision so
// a slow-moving background actually converges. shift must be in [1, 8].
void ema_update_scalar(uint16_t* bg16, const uint8_t* frame, size_t n, int shift);

// Step 2b — project the Q8.8 background down to a byte view for the next
// frame's abs-diff (bg8 = bg16 >> 8).
void project_high_byte_scalar(const uint16_t* bg16, uint8_t* bg8, size_t n);

// Step 3a — threshold the diff to 0/1 bytes (diff >= thr ? 1 : 0).
void threshold_to_binary_scalar(const uint8_t* diff, uint8_t* out, size_t n, uint8_t thr);

// Step 3b — sum the 0/1 mask over each block×block cell of a w×h image into a
// row-major grid of counts (grid is w/block by h/block). Requires w and h to
// be multiples of block.
void block_counts_scalar(const uint8_t* bin, int w, int h, int block, uint16_t* counts);

#if NIGHTJAR_HAS_NEON
void abs_diff_neon(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t n);
void ema_update_neon(uint16_t* bg16, const uint8_t* frame, size_t n, int shift);
void project_high_byte_neon(const uint16_t* bg16, uint8_t* bg8, size_t n);
void threshold_to_binary_neon(const uint8_t* diff, uint8_t* out, size_t n, uint8_t thr);
void block_counts_neon(const uint8_t* bin, int w, int h, int block, uint16_t* counts);
#endif

// Dispatch: NEON where available, scalar otherwise. The engine calls these.
void abs_diff(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t n);
void ema_update(uint16_t* bg16, const uint8_t* frame, size_t n, int shift);
void project_high_byte(const uint16_t* bg16, uint8_t* bg8, size_t n);
void threshold_to_binary(const uint8_t* diff, uint8_t* out, size_t n, uint8_t thr);
void block_counts(const uint8_t* bin, int w, int h, int block, uint16_t* counts);

}  // namespace nightjar::gate
