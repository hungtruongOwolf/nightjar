#include "nightjar/clip_encoder.h"

// The single translation unit that defines the stb implementation (kept here in
// the core so the alert sink and clip store share one copy — no duplicate
// symbols). Silence the vendored header's own warnings.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#pragma clang diagnostic pop

#include <cstdio>

namespace nightjar {
namespace {
void sink_bytes(void* ctx, void* data, int len) {
    auto* out = static_cast<std::vector<uint8_t>*>(ctx);
    const auto* b = static_cast<const uint8_t*>(data);
    out->insert(out->end(), b, b + len);
}
}  // namespace

std::vector<uint8_t> encode_gray_jpeg(const GrayImage& img, int quality) {
    std::vector<uint8_t> out;
    if (img.width <= 0 || img.height <= 0) return out;
    stbi_write_jpg_to_func(sink_bytes, &out, img.width, img.height, /*comp=*/1, img.pixels.data(),
                           quality);
    return out;
}

std::vector<uint8_t> encode_gray_png(const uint8_t* pixels, int size) {
    std::vector<uint8_t> out;
    if (!pixels || size <= 0) return out;
    stbi_write_png_to_func(sink_bytes, &out, size, size, /*comp=*/1, pixels, /*stride=*/size);
    return out;
}

ClipEncoder pgm_encoder() {
    return [](const GrayImage& img) {
        EncodedFrame f;
        f.ext = "pgm";
        char hdr[32];
        const int n = std::snprintf(hdr, sizeof(hdr), "P5\n%d %d\n255\n", img.width, img.height);
        f.bytes.insert(f.bytes.end(), hdr, hdr + n);
        f.bytes.insert(f.bytes.end(), img.pixels.begin(), img.pixels.end());
        return f;
    };
}

ClipEncoder jpeg_encoder(int quality) {
    return [quality](const GrayImage& img) {
        return EncodedFrame{encode_gray_jpeg(img, quality), "jpg"};
    };
}

}  // namespace nightjar
