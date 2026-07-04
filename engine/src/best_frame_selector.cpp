#include "nightjar/best_frame_selector.h"

namespace nightjar {

BestFrameSelector::BestFrameSelector(BestFrameConfig config) : config_(config) {}

void BestFrameSelector::update_best(const FrameView& frame, const GateResult& gate) {
    const Rect crop = expand_rect(gate.blob_bbox, config_.crop_margin, frame.width, frame.height);

    // Sharpness of the crop region (in-place on the borrowed plane, stride-aware).
    const uint8_t* crop_origin =
        frame.y_plane + static_cast<size_t>(crop.y) * frame.stride + crop.x;
    const double sharpness =
        variance_of_laplacian(crop_origin, crop.w, crop.h, frame.stride);

    best_.image = crop_and_letterbox(frame.y_plane, frame.width, frame.height, frame.stride, crop,
                                     config_.letterbox_size);
    best_.source_bbox = gate.blob_bbox;
    best_.seq = frame.seq;
    best_.ts_mono_ns = frame.ts_mono_ns;  // capture time (t0), for end-to-end latency
    best_.sharpness = sharpness;
    best_area_ = gate.blob_area_blocks;
    have_best_ = true;
}

CandidateFrame BestFrameSelector::emit(bool early_exit) {
    in_window_ = false;
    have_best_ = false;
    best_area_ = 0;
    best_.early_exit = early_exit;
    return std::move(best_);
}

std::optional<CandidateFrame> BestFrameSelector::offer(const FrameView& frame,
                                                       const GateResult& gate, uint64_t now_ns) {
    if (!gate.motion) {
        // Motion went quiet: close the window on whatever best we captured.
        if (in_window_ && have_best_) return emit(/*early_exit=*/false);
        return std::nullopt;
    }

    if (!in_window_) {
        in_window_ = true;
        window_start_ns_ = now_ns;
        best_area_ = 0;
        have_best_ = false;
    }

    // Track the largest-blob frame in the window.
    if (!have_best_ || gate.blob_area_blocks > best_area_) {
        update_best(frame, gate);
    }

    // Early-exit: a big, sharp blob is worth sending now rather than waiting out
    // the window (saves p50 latency).
    if (best_area_ >= config_.early_exit_min_blob_blocks &&
        best_.sharpness >= config_.early_exit_min_sharpness) {
        return emit(/*early_exit=*/true);
    }

    // Ceiling: publish the best so far.
    const uint64_t window_ns = static_cast<uint64_t>(config_.window_ms) * 1'000'000ull;
    if (now_ns - window_start_ns_ >= window_ns) {
        return emit(/*early_exit=*/false);
    }

    return std::nullopt;  // still collecting
}

}  // namespace nightjar
