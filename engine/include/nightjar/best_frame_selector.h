#pragma once

#include <cstdint>
#include <optional>

#include "nightjar/frame_ops.h"
#include "nightjar/frame_view.h"
#include "nightjar/motion_gate.h"

namespace nightjar {

struct BestFrameConfig {
    int window_ms = 500;                    // hard ceiling on the selection window
    int letterbox_size = 448;               // VLM input side
    float crop_margin = 0.20f;              // context added around the motion blob
    int early_exit_min_blob_blocks = 6;     // early-exit needs a blob at least this big
    double early_exit_min_sharpness = 50.0; // ...and this sharp [tune in KT2 with clips]
};

// The candidate frame handed to the VLM.
struct CandidateFrame {
    Letterboxed image;        // letterbox_size × letterbox_size grayscale
    Rect source_bbox;         // motion bbox in original frame coordinates
    uint64_t seq = 0;         // source frame seq (best frame chosen)
    uint64_t ts_mono_ns = 0;  // capture time (t0) of the chosen frame
    uint64_t event_id = 0;    // assigned by the pipeline for telemetry correlation
    double sharpness = 0.0;
    bool early_exit = false;  // published early (good enough) vs at window close
};

// Picks the frame to send to the VLM once the gate reports motion (design doc
// §5.3). Opens a window on motion onset, tracks the largest-blob frame, and
// publishes when either the frame is clearly good enough (early-exit on blob
// size + sharpness — saves p50 latency) or the window closes (ceiling reached,
// or motion goes quiet). Copies the chosen frame's pixels immediately, since a
// FrameView is only borrowed.
class BestFrameSelector {
public:
    explicit BestFrameSelector(BestFrameConfig config);

    // Feed one frame with its gate verdict and the current monotonic time.
    // Returns a candidate when one is ready to publish, else nullopt.
    std::optional<CandidateFrame> offer(const FrameView& frame, const GateResult& gate,
                                        uint64_t now_ns);

private:
    void update_best(const FrameView& frame, const GateResult& gate);
    CandidateFrame emit(bool early_exit);

    BestFrameConfig config_;
    bool in_window_ = false;
    uint64_t window_start_ns_ = 0;

    // Pending best (rebuilt whenever a larger blob arrives).
    CandidateFrame best_;
    uint16_t best_area_ = 0;
    bool have_best_ = false;
};

}  // namespace nightjar
