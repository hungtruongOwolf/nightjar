#include "nightjar/differential_clip_codec.h"

#include <cstdio>
#include <vector>

#include "check.h"
#include "nightjar/clip_encoder.h"

using namespace nightjar;

namespace {

constexpr int W = 320, H = 240;  // multiples of 16

// A surveillance-like clip: static gradient background, a small textured blob
// that moves a bit each frame (only a few blocks change frame-to-frame).
std::vector<GrayImage> surveillance_clip(int n) {
    std::vector<GrayImage> frames;
    for (int f = 0; f < n; ++f) {
        GrayImage g;
        g.width = W;
        g.height = H;
        g.pixels.resize(size_t(W) * H);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) g.pixels[size_t(y) * W + x] = uint8_t((x + y) & 0xFF);
        // small 48x48 blob drifting right
        const int x0 = 20 + f * 4, y0 = 100;
        for (int y = y0; y < y0 + 48 && y < H; ++y)
            for (int x = x0; x < x0 + 48 && x < W; ++x)
                g.pixels[size_t(y) * W + x] = ((x / 2 + y / 2) & 1) ? 0 : 240;
        frames.push_back(std::move(g));
    }
    return frames;
}

void test_lossless_roundtrip() {
    auto frames = surveillance_clip(20);
    DifferentialClipCodec codec;
    auto enc = codec.encode(frames);
    auto dec = codec.decode(enc);
    CHECK_EQ(dec.size(), frames.size());
    bool exact = dec.size() == frames.size();
    for (size_t i = 0; i < dec.size() && exact; ++i)
        if (dec[i].pixels != frames[i].pixels) exact = false;
    CHECK(exact);  // differential deltas are lossless
}

void test_compression_vs_per_frame_jpeg() {
    auto frames = surveillance_clip(30);
    DifferentialClipCodec codec;
    auto enc = codec.encode(frames);

    // Differential: JPEG the keyframe + raw block deltas.
    size_t diff_total = encode_gray_jpeg(enc.keyframe, 70).size();
    for (const auto& d : enc.deltas) diff_total += d.size();

    // Baseline: every frame as JPEG.
    size_t jpeg_total = 0;
    for (const auto& f : frames) jpeg_total += encode_gray_jpeg(f, 70).size();

    CHECK(diff_total < jpeg_total);  // differential wins on a mostly-static clip
    std::fprintf(stderr,
                 "[diff_codec] %zu frames %dx%d: per-frame JPEG %zuB, differential %zuB (%.1fx)\n",
                 frames.size(), W, H, jpeg_total, diff_total,
                 double(jpeg_total) / double(diff_total));
}

void test_static_clip_deltas_tiny() {
    // Truly static clip: deltas should be near-empty (just the changed-count header).
    std::vector<GrayImage> frames;
    GrayImage g;
    g.width = W;
    g.height = H;
    g.pixels.assign(size_t(W) * H, 128);
    for (int i = 0; i < 10; ++i) frames.push_back(g);
    DifferentialClipCodec codec;
    auto enc = codec.encode(frames);
    for (const auto& d : enc.deltas) CHECK_EQ(d.size(), size_t(2));  // just num_changed=0
}

}  // namespace

int main() {
    test_lossless_roundtrip();
    test_compression_vs_per_frame_jpeg();
    test_static_clip_deltas_tiny();
    return njtest::failures() == 0 ? 0 : 1;
}
