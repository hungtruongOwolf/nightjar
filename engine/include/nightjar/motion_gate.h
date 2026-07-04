#pragma once

#include <cstdint>
#include <vector>

#include "nightjar/frame_view.h"

namespace nightjar {

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};

struct GateConfig {
    int block = 16;                    // block grid cell size (16 -> 40×30 on 640×480)
    uint8_t diff_threshold = 20;       // per-pixel foreground threshold [v1 fixed; adaptive
                                       // noise_mean + k·std is a KT2 tuning follow-up]
    int block_fg_pixels = 40;          // a block is "active" if >= this many of its 256 px changed
    int min_blocks_connected = 3;      // motion requires a connected blob at least this big
    float global_change_ratio = 0.60f;// > this fraction of in-zone blocks active => illumination
                                       // shift (lights on / cloud), suppressed, not motion
    int ema_alpha_shift = 6;           // background EMA rate; larger = slower (alpha = 2^-shift)
};

struct GateResult {
    bool motion = false;
    uint16_t blob_area_blocks = 0;     // size of the largest connected active blob
    Rect blob_bbox;                    // its bounding box in pixel coordinates
    float fg_ratio = 0.0f;             // fraction of in-zone blocks that were active
    bool suppressed_global = false;    // the global-illumination suppressor fired
};

// 40×30 (or w/block × h/block) bitmap; 1 = block is inside the watched zone.
using BlockBitmap = std::vector<uint8_t>;

// Tier-1 motion gate (design doc §5.2). Cheap per-frame filter that gates the
// expensive VLM: tuned never-to-miss, letting Tier 2 clean up false positives.
// Allocation-free after the first frame — all buffers are sized on first use
// and reused (hot path, CLAUDE.md §4).
class MotionGate {
public:
    explicit MotionGate(GateConfig config);

    // Evaluate one frame. Budget <0.5ms on a 640×480 Y-plane. The first frame
    // only seeds the background and always returns motion=false.
    GateResult evaluate(const FrameView& frame);

    // Restrict motion to a zone. Bitmap is w/block × h/block, row-major, 1=in.
    // Empty (default) means the whole frame is watched.
    void set_zone_mask(BlockBitmap mask);

private:
    void ensure_allocated(int w, int h);
    bool in_zone(int grid_index) const;

    GateConfig config_;
    BlockBitmap zone_mask_;  // empty => everything in zone

    int width_ = 0, height_ = 0, grid_w_ = 0, grid_h_ = 0;
    bool seeded_ = false;

    // Pre-allocated working buffers (sized on first frame).
    std::vector<uint8_t> packed_;   // frame compacted to stride==width
    std::vector<uint8_t> bg8_;      // background byte view (for abs-diff)
    std::vector<uint16_t> bg16_;    // background Q8.8 (source of truth)
    std::vector<uint8_t> diff_;     // |frame - bg8|
    std::vector<uint8_t> bin_;      // thresholded 0/1
    std::vector<uint16_t> counts_;  // per-block foreground pixel counts
    std::vector<uint8_t> active_;   // per-block active flag (post zone+threshold)
    std::vector<int> visited_;      // flood-fill scratch
};

}  // namespace nightjar
