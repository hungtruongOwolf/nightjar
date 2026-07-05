#include "nightjar/clip_encoder.h"

#include <cstdio>

#include "check.h"

using namespace nightjar;

namespace {

// A realistic-ish 320x240 grayscale frame: smooth gradient background + a
// textured foreground block (like a person crop), not a trivial flat image.
GrayImage sample_frame() {
    GrayImage g;
    g.width = 320;
    g.height = 240;
    g.pixels.resize(size_t(g.width) * g.height);
    for (int y = 0; y < g.height; ++y)
        for (int x = 0; x < g.width; ++x) {
            uint8_t v = static_cast<uint8_t>((x + y) & 0xFF);          // gradient
            if (x > 120 && x < 200 && y > 80 && y < 160)
                v = ((x / 2 + y / 2) & 1) ? 20 : 200;                  // textured blob
            g.pixels[size_t(y) * g.width + x] = v;
        }
    return g;
}

void test_jpeg_smaller_than_pgm() {
    GrayImage f = sample_frame();
    auto pgm = pgm_encoder()(f);
    auto jpg = jpeg_encoder(70)(f);
    CHECK(!pgm.bytes.empty());
    CHECK(!jpg.bytes.empty());
    CHECK_EQ(pgm.ext, std::string("pgm"));
    CHECK_EQ(jpg.ext, std::string("jpg"));
    // JPEG must be clearly smaller (compression is the whole point).
    CHECK(jpg.bytes.size() * 3 < pgm.bytes.size());
    std::fprintf(stderr, "[clip_encoder] 320x240 gray: PGM %zuB, JPEG(q70) %zuB (%.1fx smaller)\n",
                 pgm.bytes.size(), jpg.bytes.size(),
                 double(pgm.bytes.size()) / double(jpg.bytes.size()));
}

void test_jpeg_has_soi_marker() {
    auto jpg = jpeg_encoder(70)(sample_frame());
    CHECK(jpg.bytes.size() > 2);
    CHECK_EQ(int(jpg.bytes[0]), 0xFF);  // JPEG SOI
    CHECK_EQ(int(jpg.bytes[1]), 0xD8);
}

void test_pgm_roundtrips_size() {
    GrayImage f = sample_frame();
    auto pgm = pgm_encoder()(f);
    // header + width*height pixel bytes
    CHECK(pgm.bytes.size() > size_t(f.width) * f.height);
}

}  // namespace

int main() {
    test_jpeg_smaller_than_pgm();
    test_jpeg_has_soi_marker();
    test_pgm_roundtrips_size();
    return njtest::failures() == 0 ? 0 : 1;
}
