#include "nightjar/best_frame_selector.h"

#include <vector>

#include "check.h"

using namespace nightjar;

namespace {

constexpr int W = 128, H = 128;
constexpr uint64_t ms = 1'000'000;

// A frame with a high-contrast checkerboard inside the blob region so the crop
// has real sharpness; the rest is flat.
std::vector<uint8_t> textured_frame() {
    std::vector<uint8_t> buf(size_t(W) * H, 30);
    for (int y = 16; y < 80; ++y)
        for (int x = 16; x < 80; ++x)
            buf[size_t(y) * W + x] = ((x / 2 + y / 2) & 1) ? 0 : 255;
    return buf;
}

FrameView view_of(const std::vector<uint8_t>& buf, uint64_t seq) {
    FrameView f;
    f.y_plane = buf.data();
    f.width = W;
    f.height = H;
    f.stride = W;
    f.seq = seq;
    return f;
}

GateResult motion(uint16_t blocks, Rect bbox) {
    GateResult g;
    g.motion = true;
    g.blob_area_blocks = blocks;
    g.blob_bbox = bbox;
    return g;
}

void test_no_motion_no_candidate() {
    BestFrameSelector sel(BestFrameConfig{});
    auto f = textured_frame();
    GateResult still;  // motion=false
    CHECK(!sel.offer(view_of(f, 0), still, 0).has_value());
}

void test_early_exit_on_big_sharp_blob() {
    BestFrameConfig cfg;
    cfg.early_exit_min_blob_blocks = 3;
    cfg.early_exit_min_sharpness = 1.0;  // the textured crop easily clears this
    BestFrameSelector sel(cfg);

    auto f = textured_frame();
    auto c = sel.offer(view_of(f, 5), motion(4, Rect{16, 16, 64, 64}), 100 * ms);
    CHECK(c.has_value());
    CHECK(c->early_exit);
    CHECK_EQ(c->image.size, 448);
    CHECK_EQ(c->source_bbox.w, 64);
    CHECK_EQ(c->seq, uint64_t(5));
    CHECK(c->sharpness > 1.0);
}

void test_window_ceiling_publishes_when_no_early_exit() {
    BestFrameConfig cfg;
    cfg.window_ms = 500;
    cfg.early_exit_min_sharpness = 1e12;  // impossible => never early-exit
    BestFrameSelector sel(cfg);
    auto f = textured_frame();

    // Within the window: still collecting.
    CHECK(!sel.offer(view_of(f, 1), motion(4, Rect{16, 16, 64, 64}), 0).has_value());
    CHECK(!sel.offer(view_of(f, 2), motion(4, Rect{16, 16, 64, 64}), 200 * ms).has_value());
    // At the ceiling: publish (not an early-exit).
    auto c = sel.offer(view_of(f, 3), motion(4, Rect{16, 16, 64, 64}), 500 * ms);
    CHECK(c.has_value());
    CHECK(!c->early_exit);
}

void test_tracks_largest_blob() {
    BestFrameConfig cfg;
    cfg.early_exit_min_sharpness = 1e12;  // force window-close path
    cfg.window_ms = 100;
    BestFrameSelector sel(cfg);
    auto f = textured_frame();

    sel.offer(view_of(f, 1), motion(3, Rect{16, 16, 32, 32}), 0);
    sel.offer(view_of(f, 2), motion(9, Rect{16, 16, 64, 64}), 20 * ms);  // biggest
    sel.offer(view_of(f, 3), motion(5, Rect{16, 16, 48, 48}), 40 * ms);
    auto c = sel.offer(view_of(f, 4), motion(4, Rect{16, 16, 40, 40}), 100 * ms);
    CHECK(c.has_value());
    CHECK_EQ(c->seq, uint64_t(2));           // the largest-blob frame
    CHECK_EQ(c->source_bbox.w, 64);
}

void test_motion_quiet_closes_window() {
    BestFrameConfig cfg;
    cfg.early_exit_min_sharpness = 1e12;  // no early-exit
    BestFrameSelector sel(cfg);
    auto f = textured_frame();

    CHECK(!sel.offer(view_of(f, 1), motion(4, Rect{16, 16, 64, 64}), 0).has_value());
    // Motion stops before the ceiling: publish the best captured so far.
    GateResult still;
    auto c = sel.offer(view_of(f, 2), still, 120 * ms);
    CHECK(c.has_value());
    CHECK(!c->early_exit);
    CHECK_EQ(c->seq, uint64_t(1));
}

void test_resets_between_events() {
    BestFrameConfig cfg;
    cfg.early_exit_min_blob_blocks = 3;
    cfg.early_exit_min_sharpness = 1.0;
    BestFrameSelector sel(cfg);
    auto f = textured_frame();

    auto c1 = sel.offer(view_of(f, 1), motion(4, Rect{16, 16, 64, 64}), 0);
    CHECK(c1.has_value());
    // A fresh event later should produce another candidate (state reset).
    auto c2 = sel.offer(view_of(f, 9), motion(4, Rect{16, 16, 64, 64}), 5000 * ms);
    CHECK(c2.has_value());
    CHECK_EQ(c2->seq, uint64_t(9));
}

}  // namespace

int main() {
    test_no_motion_no_candidate();
    test_early_exit_on_big_sharp_blob();
    test_window_ceiling_publishes_when_no_early_exit();
    test_tracks_largest_blob();
    test_motion_quiet_closes_window();
    test_resets_between_events();
    return njtest::failures() == 0 ? 0 : 1;
}
