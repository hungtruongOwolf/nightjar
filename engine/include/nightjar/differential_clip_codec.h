#pragma once

#include <cstdint>
#include <vector>

#include "nightjar/pgm.h"  // GrayImage

namespace nightjar {

struct DiffConfig {
    int block = 16;  // block grid; only blocks that changed vs the previous frame are stored
};

// Inter-frame differential codec for evidence clips, the "surveillance scenes
// are ~95% static, so store only what moved" idea, reusing Nightjar's own
// block-change concept (the motion gate already thinks in 16×16 blocks). Frame 0
// is a keyframe; each later frame stores ONLY the blocks that differ from the
// previous frame (like a video P-frame). For a static scene with a small moving
// subject, per-frame cost collapses to a few blocks instead of a whole image.
//
// The block deltas are lossless (raw block bytes); the keyframe can be JPEG'd
// separately. Requires width/height multiples of `block`.
class DifferentialClipCodec {
public:
    struct Encoded {
        GrayImage keyframe;                     // frame 0, full
        std::vector<std::vector<uint8_t>> deltas;  // one changed-block record per later frame
        int width = 0, height = 0, block = 0;
    };

    explicit DifferentialClipCodec(DiffConfig cfg = {}) : cfg_(cfg) {}

    // Encode a clip into keyframe + per-frame block deltas.
    Encoded encode(const std::vector<GrayImage>& frames) const;

    // Reconstruct the full frame sequence (lossless w.r.t. the input frames).
    std::vector<GrayImage> decode(const Encoded& enc) const;

    // Streaming API (used by EventClipStore's writer): the delta bytes for one
    // frame given the previous frame; and applying a delta onto a previous frame.
    std::vector<uint8_t> encode_frame(const GrayImage& cur, const GrayImage& prev) const;
    void apply_delta(GrayImage& frame, const std::vector<uint8_t>& delta) const;

    // Total serialized bytes: keyframe (as raw) + all deltas. (For the JPEG
    // keyframe size, encode the keyframe with jpeg_encoder separately.)
    static size_t delta_bytes(const Encoded& enc);

private:
    DiffConfig cfg_;
};

}  // namespace nightjar
