#include "nightjar/motion_gate.h"

#include <cstdint>
#include <vector>

#include "check.h"

using namespace nightjar;

namespace {

constexpr int W = 64, H = 64;  // 4×4 block grid at block=16

FrameView view_of(const std::vector<uint8_t>& buf, uint64_t seq) {
    FrameView f;
    f.y_plane = buf.data();
    f.width = W;
    f.height = H;
    f.stride = W;
    f.seq = seq;
    return f;
}

std::vector<uint8_t> uniform(uint8_t v) { return std::vector<uint8_t>(size_t(W) * H, v); }

// Fill a pixel rectangle [x0,x1) × [y0,y1) with value v.
void fill_rect(std::vector<uint8_t>& buf, int x0, int y0, int x1, int y1, uint8_t v) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) buf[size_t(y) * W + x] = v;
}

GateConfig test_config() {
    GateConfig c;
    c.block = 16;
    c.diff_threshold = 20;
    c.block_fg_pixels = 40;
    c.min_blocks_connected = 3;
    c.global_change_ratio = 0.60f;
    c.ema_alpha_shift = 6;
    return c;
}

void test_first_frame_seeds_no_motion() {
    MotionGate gate(test_config());
    auto f = uniform(50);
    GateResult r = gate.evaluate(view_of(f, 0));
    CHECK(!r.motion);
    CHECK(!r.suppressed_global);
}

void test_static_scene_no_motion() {
    MotionGate gate(test_config());
    auto f = uniform(50);
    gate.evaluate(view_of(f, 0));  // seed
    for (int i = 1; i < 5; ++i) {
        GateResult r = gate.evaluate(view_of(f, uint64_t(i)));
        CHECK(!r.motion);
        CHECK_EQ(r.blob_area_blocks, uint16_t(0));
    }
}

void test_moving_blob_triggers_motion() {
    MotionGate gate(test_config());
    auto bg = uniform(50);
    gate.evaluate(view_of(bg, 0));  // seed at 50

    auto blob = uniform(50);
    fill_rect(blob, 0, 0, 32, 32, 240);  // 2×2 blocks changed => 4 blocks
    GateResult r = gate.evaluate(view_of(blob, 1));

    CHECK(r.motion);
    CHECK(!r.suppressed_global);
    CHECK(r.blob_area_blocks >= 3);
    CHECK_EQ(r.blob_bbox.x, 0);
    CHECK_EQ(r.blob_bbox.y, 0);
    CHECK_EQ(r.blob_bbox.w, 32);
    CHECK_EQ(r.blob_bbox.h, 32);
}

void test_small_change_below_threshold_no_motion() {
    MotionGate gate(test_config());
    auto bg = uniform(50);
    gate.evaluate(view_of(bg, 0));

    auto tiny = uniform(50);
    fill_rect(tiny, 0, 0, 16, 16, 240);  // only ONE block (< min_blocks_connected=3)
    GateResult r = gate.evaluate(view_of(tiny, 1));
    CHECK(!r.motion);
    CHECK_EQ(r.blob_area_blocks, uint16_t(1));
}

void test_global_illumination_suppressed() {
    MotionGate gate(test_config());
    auto dark = uniform(30);
    gate.evaluate(view_of(dark, 0));  // seed dark

    auto bright = uniform(200);  // whole frame changes => lighting shift
    GateResult r = gate.evaluate(view_of(bright, 1));
    CHECK(r.suppressed_global);
    CHECK(!r.motion);
    CHECK(r.fg_ratio > 0.60f);
}

void test_zone_mask_excludes_out_of_zone_motion() {
    // Zone = only the bottom-right block region; a blob in the top-left is ignored.
    MotionGate gate(test_config());
    const int gw = W / 16, gh = H / 16;  // 4×4
    BlockBitmap mask(size_t(gw) * gh, 0);
    for (int by = 2; by < gh; ++by)
        for (int bx = 2; bx < gw; ++bx) mask[size_t(by) * gw + bx] = 1;  // bottom-right 2×2
    gate.set_zone_mask(mask);

    auto bg = uniform(50);
    gate.evaluate(view_of(bg, 0));

    auto blob = uniform(50);
    fill_rect(blob, 0, 0, 32, 32, 240);  // top-left blob — outside the zone
    GateResult r = gate.evaluate(view_of(blob, 1));
    CHECK(!r.motion);
    CHECK_EQ(r.blob_area_blocks, uint16_t(0));
}

void test_strided_frame_handled() {
    // A frame whose stride exceeds width must still gate correctly.
    MotionGate gate(test_config());
    const int stride = W + 13;
    std::vector<uint8_t> bg(size_t(stride) * H, 50);
    FrameView f;
    f.y_plane = bg.data();
    f.width = W;
    f.height = H;
    f.stride = stride;
    gate.evaluate(f);  // seed

    std::vector<uint8_t> blobbuf(size_t(stride) * H, 50);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) blobbuf[size_t(y) * stride + x] = 240;
    f.y_plane = blobbuf.data();
    f.seq = 1;
    GateResult r = gate.evaluate(f);
    CHECK(r.motion);
    CHECK_EQ(r.blob_bbox.w, 32);
}

}  // namespace

int main() {
    test_first_frame_seeds_no_motion();
    test_static_scene_no_motion();
    test_moving_blob_triggers_motion();
    test_small_change_below_threshold_no_motion();
    test_global_illumination_suppressed();
    test_zone_mask_excludes_out_of_zone_motion();
    test_strided_frame_handled();
    return njtest::failures() == 0 ? 0 : 1;
}
