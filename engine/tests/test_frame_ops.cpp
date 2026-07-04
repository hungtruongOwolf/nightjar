#include "nightjar/frame_ops.h"

#include <cmath>
#include <vector>

#include "check.h"

using namespace nightjar;

namespace {

void test_expand_rect_margin_and_clamp() {
    // 20% margin on a 100×50 rect at (40,30): +20 x, +10 y each side.
    Rect r = expand_rect(Rect{40, 30, 100, 50}, 0.20f, 1000, 1000);
    CHECK_EQ(r.x, 20);
    CHECK_EQ(r.y, 20);
    CHECK_EQ(r.w, 140);
    CHECK_EQ(r.h, 70);

    // Clamped at the image edge — never negative, never past bounds.
    Rect c = expand_rect(Rect{0, 0, 20, 20}, 0.5f, 25, 25);
    CHECK_EQ(c.x, 0);
    CHECK_EQ(c.y, 0);
    CHECK(c.x + c.w <= 25);
    CHECK(c.y + c.h <= 25);
}

void test_letterbox_uniform_region_is_uniform() {
    // A uniform crop must stay uniform through resize (bilinear of equal
    // values), and the padded border must be the pad value.
    const int sw = 40, sh = 40;
    std::vector<uint8_t> src(size_t(sw) * sh, 123);
    Letterboxed lb = crop_and_letterbox(src.data(), sw, sh, sw, Rect{0, 0, 40, 40}, 448, 0);
    CHECK_EQ(lb.size, 448);
    // Square crop => fills the whole 448×448, no padding, all == 123.
    bool all_123 = true;
    for (uint8_t p : lb.pixels)
        if (p != 123) all_123 = false;
    CHECK(all_123);
}

void test_letterbox_aspect_pads_short_side() {
    // A wide crop (80×20) letterboxed to 100 => content 100×25, padded top/bottom.
    const int sw = 80, sh = 20;
    std::vector<uint8_t> src(size_t(sw) * sh, 200);
    Letterboxed lb = crop_and_letterbox(src.data(), sw, sh, sw, Rect{0, 0, 80, 20}, 100, 7);
    CHECK_EQ(lb.size, 100);
    // Top row must be pad (content is centered vertically, height ~25 < 100).
    CHECK_EQ(int(lb.pixels[0]), 7);
    // Center row must be content.
    CHECK_EQ(int(lb.pixels[size_t(50) * 100 + 50]), 200);
}

void test_letterbox_clamps_out_of_bounds_crop() {
    const int sw = 32, sh = 32;
    std::vector<uint8_t> src(size_t(sw) * sh, 90);
    // Crop partly outside the image; used_crop must be clamped inside.
    Letterboxed lb = crop_and_letterbox(src.data(), sw, sh, sw, Rect{-10, -10, 20, 20}, 64, 0);
    CHECK(lb.used_crop.x >= 0);
    CHECK(lb.used_crop.y >= 0);
    CHECK(lb.used_crop.x + lb.used_crop.w <= sw);
}

void test_variance_of_laplacian_flat_is_zero() {
    const int w = 20, h = 20;
    std::vector<uint8_t> flat(size_t(w) * h, 128);
    CHECK_EQ(variance_of_laplacian(flat.data(), w, h, w), 0.0);
}

void test_variance_of_laplacian_sharp_beats_blurred() {
    const int w = 32, h = 32;
    std::vector<uint8_t> sharp(size_t(w) * h, 0), blurred(size_t(w) * h, 0);
    // Sharp: hard vertical edge at x=16. Blurred: a gradient ramp.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            sharp[size_t(y) * w + x] = x < 16 ? 0 : 255;
            blurred[size_t(y) * w + x] = static_cast<uint8_t>(x * 255 / (w - 1));
        }
    }
    const double vs = variance_of_laplacian(sharp.data(), w, h, w);
    const double vb = variance_of_laplacian(blurred.data(), w, h, w);
    CHECK(vs > vb);
    CHECK(vs > 0.0);
}

}  // namespace

int main() {
    test_expand_rect_margin_and_clamp();
    test_letterbox_uniform_region_is_uniform();
    test_letterbox_aspect_pads_short_side();
    test_letterbox_clamps_out_of_bounds_crop();
    test_variance_of_laplacian_flat_is_zero();
    test_variance_of_laplacian_sharp_beats_blurred();
    return njtest::failures() == 0 ? 0 : 1;
}
