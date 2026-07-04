#include "nightjar/motion_gate.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "nightjar/gate_steps.h"

namespace nightjar {
namespace {

// Compact a possibly-strided Y-plane into a tightly packed buffer so the NEON
// steps see stride == width. A no-op copy when the frame is already packed.
void pack_frame(const FrameView& f, uint8_t* dst) {
    if (f.stride == f.width) {
        std::memcpy(dst, f.y_plane, static_cast<size_t>(f.width) * f.height);
        return;
    }
    for (int y = 0; y < f.height; ++y) {
        std::memcpy(dst + static_cast<size_t>(y) * f.width, f.y_plane + static_cast<size_t>(y) * f.stride,
                    f.width);
    }
}

}  // namespace

MotionGate::MotionGate(GateConfig config) : config_(config) {}

void MotionGate::set_zone_mask(BlockBitmap mask) { zone_mask_ = std::move(mask); }

bool MotionGate::in_zone(int grid_index) const {
    if (zone_mask_.empty()) return true;
    return zone_mask_[static_cast<size_t>(grid_index)] != 0;
}

void MotionGate::ensure_allocated(int w, int h) {
    if (w == width_ && h == height_) return;
    width_ = w;
    height_ = h;
    grid_w_ = w / config_.block;
    grid_h_ = h / config_.block;
    const size_t n = static_cast<size_t>(w) * h;
    const size_t g = static_cast<size_t>(grid_w_) * grid_h_;
    packed_.assign(n, 0);
    bg8_.assign(n, 0);
    bg16_.assign(n, 0);
    diff_.assign(n, 0);
    bin_.assign(n, 0);
    counts_.assign(g, 0);
    active_.assign(g, 0);
    visited_.assign(g, 0);
    seeded_ = false;
}

GateResult MotionGate::evaluate(const FrameView& frame) {
    ensure_allocated(frame.width, frame.height);
    pack_frame(frame, packed_.data());
    const size_t n = static_cast<size_t>(width_) * height_;

    // First frame seeds the background; there is nothing to diff against yet.
    if (!seeded_) {
        for (size_t i = 0; i < n; ++i) bg16_[i] = static_cast<uint16_t>(packed_[i] << 8);
        gate::project_high_byte(bg16_.data(), bg8_.data(), n);
        seeded_ = true;
        return GateResult{};
    }

    // Steps 1–3 (NEON): diff against background, advance background, project
    // the byte view for next frame, threshold, and reduce to per-block counts.
    gate::abs_diff(packed_.data(), bg8_.data(), diff_.data(), n);
    gate::ema_update(bg16_.data(), packed_.data(), n, config_.ema_alpha_shift);
    gate::project_high_byte(bg16_.data(), bg8_.data(), n);
    gate::threshold_to_binary(diff_.data(), bin_.data(), n, config_.diff_threshold);
    gate::block_counts(bin_.data(), width_, height_, config_.block, counts_.data());

    // Mark active blocks (changed enough AND inside the zone).
    const int g = grid_w_ * grid_h_;
    int active_count = 0, zone_blocks = 0;
    for (int i = 0; i < g; ++i) {
        const bool zoned = in_zone(i);
        zone_blocks += zoned ? 1 : 0;
        const bool active = zoned && counts_[static_cast<size_t>(i)] >= config_.block_fg_pixels;
        active_[static_cast<size_t>(i)] = active ? 1 : 0;
        active_count += active ? 1 : 0;
    }

    GateResult result;
    result.fg_ratio = zone_blocks > 0 ? static_cast<float>(active_count) / zone_blocks : 0.0f;

    // Step 4 — global-illumination suppressor: a change spread across most of
    // the frame is a lighting shift, not an intruder. Suppress it.
    if (zone_blocks > 0 && result.fg_ratio > config_.global_change_ratio) {
        result.suppressed_global = true;
        return result;
    }

    // Step 5 — largest connected active blob (4-connectivity flood fill on the
    // small grid; scalar by design). Motion iff it clears min_blocks_connected.
    std::fill(visited_.begin(), visited_.end(), 0);
    std::vector<int> stack;
    int best_size = 0;
    Rect best_bbox;
    for (int start = 0; start < g; ++start) {
        if (!active_[static_cast<size_t>(start)] || visited_[static_cast<size_t>(start)]) continue;
        stack.clear();
        stack.push_back(start);
        visited_[static_cast<size_t>(start)] = 1;
        int size = 0, min_x = grid_w_, min_y = grid_h_, max_x = -1, max_y = -1;
        while (!stack.empty()) {
            const int idx = stack.back();
            stack.pop_back();
            const int cx = idx % grid_w_;
            const int cy = idx / grid_w_;
            ++size;
            min_x = std::min(min_x, cx);
            min_y = std::min(min_y, cy);
            max_x = std::max(max_x, cx);
            max_y = std::max(max_y, cy);
            const int neighbours[4][2] = {{cx - 1, cy}, {cx + 1, cy}, {cx, cy - 1}, {cx, cy + 1}};
            for (const auto& nb : neighbours) {
                const int nx = nb[0], ny = nb[1];
                if (nx < 0 || ny < 0 || nx >= grid_w_ || ny >= grid_h_) continue;
                const int nidx = ny * grid_w_ + nx;
                if (active_[static_cast<size_t>(nidx)] && !visited_[static_cast<size_t>(nidx)]) {
                    visited_[static_cast<size_t>(nidx)] = 1;
                    stack.push_back(nidx);
                }
            }
        }
        if (size > best_size) {
            best_size = size;
            best_bbox = Rect{min_x * config_.block, min_y * config_.block,
                             (max_x - min_x + 1) * config_.block, (max_y - min_y + 1) * config_.block};
        }
    }

    result.blob_area_blocks = static_cast<uint16_t>(best_size);
    result.blob_bbox = best_bbox;
    result.motion = best_size >= config_.min_blocks_connected;
    return result;
}

}  // namespace nightjar
