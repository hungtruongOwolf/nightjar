#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nightjar {

// A decoded 8-bit grayscale image, exactly one luma plane, tightly packed
// (stride == width). PGM is used as the harness frame format because it maps
// 1:1 onto a Y-plane and needs no video-decode dependency; any clip or image
// is converted to a PGM sequence offline (ffmpeg/sips).
struct GrayImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;  // size == width * height
};

// Parse a binary (P5) PGM file. Returns nullopt on any malformed input.
// Supports the standard header quirks: '#' comment lines and arbitrary
// whitespace between fields. Only maxval <= 255 (8-bit) is accepted.
std::optional<GrayImage> read_pgm(const std::string& path);

// Write a GrayImage as a binary (P5) PGM. Returns false on I/O error.
bool write_pgm(const std::string& path, const GrayImage& img);

}  // namespace nightjar
