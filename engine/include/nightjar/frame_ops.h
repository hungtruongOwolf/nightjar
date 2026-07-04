#pragma once

#include <cstdint>
#include <vector>

#include "nightjar/motion_gate.h"  // Rect

namespace nightjar {

// A square grayscale image ready for the VLM: the motion crop scaled to fit
// (aspect preserved) and padded to `size`×`size`.
struct Letterboxed {
    std::vector<uint8_t> pixels;  // size*size, row-major
    int size = 0;
    Rect used_crop;  // the crop region actually used (after clamping to bounds)
};

// Grow a rectangle by `margin` fraction on every side, then clamp to
// [0,max_w)×[0,max_h). Used to add context around the motion blob before crop.
Rect expand_rect(Rect r, float margin, int max_w, int max_h);

// Crop `crop` (clamped to the source) from a grayscale plane and letterbox it
// into a size×size buffer: scale so the long side == size (bilinear), center
// the result, pad the remainder with `pad`. This is the VLM's input frame.
Letterboxed crop_and_letterbox(const uint8_t* src, int sw, int sh, int sstride, Rect crop,
                               int size, uint8_t pad = 0);

// Variance of the 3×3 Laplacian response — the standard focus/sharpness score.
// Higher = sharper. A flat image scores 0. Used for best-frame early-exit so we
// don't hand the VLM a motion-blurred frame.
double variance_of_laplacian(const uint8_t* gray, int w, int h, int stride);

}  // namespace nightjar
