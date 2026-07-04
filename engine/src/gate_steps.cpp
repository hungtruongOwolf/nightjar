#include "nightjar/gate_steps.h"

#if NIGHTJAR_HAS_NEON
#include <arm_neon.h>
#endif

namespace nightjar::gate {

// ---------------------------------------------------------------- scalar refs

void abs_diff_scalar(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        out[i] = a[i] > b[i] ? static_cast<uint8_t>(a[i] - b[i])
                             : static_cast<uint8_t>(b[i] - a[i]);
    }
}

void ema_update_scalar(uint16_t* bg16, const uint8_t* frame, size_t n, int shift) {
    for (size_t i = 0; i < n; ++i) {
        const uint16_t decay = static_cast<uint16_t>(bg16[i] >> shift);
        const uint16_t inject = static_cast<uint16_t>(static_cast<uint16_t>(frame[i]) << (8 - shift));
        bg16[i] = static_cast<uint16_t>(bg16[i] - decay + inject);
    }
}

void project_high_byte_scalar(const uint16_t* bg16, uint8_t* bg8, size_t n) {
    for (size_t i = 0; i < n; ++i) bg8[i] = static_cast<uint8_t>(bg16[i] >> 8);
}

void threshold_to_binary_scalar(const uint8_t* diff, uint8_t* out, size_t n, uint8_t thr) {
    for (size_t i = 0; i < n; ++i) out[i] = diff[i] >= thr ? 1 : 0;
}

void block_counts_scalar(const uint8_t* bin, int w, int h, int block, uint16_t* counts) {
    const int gw = w / block;
    const int gh = h / block;
    for (int by = 0; by < gh; ++by) {
        for (int bx = 0; bx < gw; ++bx) {
            uint16_t sum = 0;
            for (int y = 0; y < block; ++y) {
                const uint8_t* row = bin + (by * block + y) * w + bx * block;
                for (int x = 0; x < block; ++x) sum += row[x];
            }
            counts[by * gw + bx] = sum;
        }
    }
}

// ------------------------------------------------------------------ NEON impls

#if NIGHTJAR_HAS_NEON

void abs_diff_neon(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t n) {
    size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        vst1q_u8(out + i, vabdq_u8(vld1q_u8(a + i), vld1q_u8(b + i)));
    }
    for (; i < n; ++i) {
        out[i] = a[i] > b[i] ? static_cast<uint8_t>(a[i] - b[i])
                             : static_cast<uint8_t>(b[i] - a[i]);
    }
}

void ema_update_neon(uint16_t* bg16, const uint8_t* frame, size_t n, int shift) {
    // Runtime shifts via vshlq (negative count = right shift).
    const int16x8_t rshift = vdupq_n_s16(static_cast<int16_t>(-shift));
    const int16x8_t lshift = vdupq_n_s16(static_cast<int16_t>(8 - shift));
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        const uint16x8_t bg = vld1q_u16(bg16 + i);
        const uint16x8_t fr = vmovl_u8(vld1_u8(frame + i));
        const uint16x8_t decay = vshlq_u16(bg, rshift);
        const uint16x8_t inject = vshlq_u16(fr, lshift);
        vst1q_u16(bg16 + i, vaddq_u16(vsubq_u16(bg, decay), inject));
    }
    for (; i < n; ++i) {
        const uint16_t decay = static_cast<uint16_t>(bg16[i] >> shift);
        const uint16_t inject = static_cast<uint16_t>(static_cast<uint16_t>(frame[i]) << (8 - shift));
        bg16[i] = static_cast<uint16_t>(bg16[i] - decay + inject);
    }
}

void project_high_byte_neon(const uint16_t* bg16, uint8_t* bg8, size_t n) {
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        vst1_u8(bg8 + i, vshrn_n_u16(vld1q_u16(bg16 + i), 8));
    }
    for (; i < n; ++i) bg8[i] = static_cast<uint8_t>(bg16[i] >> 8);
}

void threshold_to_binary_neon(const uint8_t* diff, uint8_t* out, size_t n, uint8_t thr) {
    const uint8x16_t vthr = vdupq_n_u8(thr);
    size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        const uint8x16_t mask = vcgeq_u8(vld1q_u8(diff + i), vthr);  // 0xFF / 0x00
        vst1q_u8(out + i, vshrq_n_u8(mask, 7));                      // -> 1 / 0
    }
    for (; i < n; ++i) out[i] = diff[i] >= thr ? 1 : 0;
}

void block_counts_neon(const uint8_t* bin, int w, int h, int block, uint16_t* counts) {
    // NEON fast path assumes 16-wide blocks (the design's 16×16 grid cell);
    // one vld1q_u8 covers a block row and vaddvq_u8 reduces it.
    if (block != 16 || (w % 16) != 0) {
        block_counts_scalar(bin, w, h, block, counts);
        return;
    }
    const int gw = w / block;
    const int gh = h / block;
    for (int by = 0; by < gh; ++by) {
        for (int bx = 0; bx < gw; ++bx) {
            uint16_t sum = 0;
            for (int y = 0; y < block; ++y) {
                const uint8_t* row = bin + (by * block + y) * w + bx * block;
                sum += vaddvq_u8(vld1q_u8(row));  // each byte is 0/1, row max 16
            }
            counts[by * gw + bx] = sum;
        }
    }
}

#endif  // NIGHTJAR_HAS_NEON

// -------------------------------------------------------------------- dispatch

void abs_diff(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t n) {
#if NIGHTJAR_HAS_NEON
    abs_diff_neon(a, b, out, n);
#else
    abs_diff_scalar(a, b, out, n);
#endif
}

void ema_update(uint16_t* bg16, const uint8_t* frame, size_t n, int shift) {
#if NIGHTJAR_HAS_NEON
    ema_update_neon(bg16, frame, n, shift);
#else
    ema_update_scalar(bg16, frame, n, shift);
#endif
}

void project_high_byte(const uint16_t* bg16, uint8_t* bg8, size_t n) {
#if NIGHTJAR_HAS_NEON
    project_high_byte_neon(bg16, bg8, n);
#else
    project_high_byte_scalar(bg16, bg8, n);
#endif
}

void threshold_to_binary(const uint8_t* diff, uint8_t* out, size_t n, uint8_t thr) {
#if NIGHTJAR_HAS_NEON
    threshold_to_binary_neon(diff, out, n, thr);
#else
    threshold_to_binary_scalar(diff, out, n, thr);
#endif
}

void block_counts(const uint8_t* bin, int w, int h, int block, uint16_t* counts) {
#if NIGHTJAR_HAS_NEON
    block_counts_neon(bin, w, h, block, counts);
#else
    block_counts_scalar(bin, w, h, block, counts);
#endif
}

}  // namespace nightjar::gate
