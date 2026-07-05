#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "nightjar/pgm.h"  // GrayImage

namespace nightjar {

// Encoded bytes for one clip frame plus the file extension to use.
struct EncodedFrame {
    std::vector<uint8_t> bytes;
    std::string ext;  // "pgm" / "jpg" / "png"
};

// How a clip frame is serialized. Pluggable so the harness can keep lossless
// PGM while the device stores compressed frames (and the iOS shell can swap in
// hardware HEVC via VideoToolbox — the encoder is just this seam).
using ClipEncoder = std::function<EncodedFrame(const GrayImage&)>;

ClipEncoder pgm_encoder();                    // lossless, uncompressed (harness/debug)
ClipEncoder jpeg_encoder(int quality = 70);   // ~10x smaller — the default on-device

// Raw one-shot encoders (also used by the alert sink for the attached crop).
std::vector<uint8_t> encode_gray_jpeg(const GrayImage& img, int quality = 70);
std::vector<uint8_t> encode_gray_png(const uint8_t* pixels, int size);  // square grayscale

}  // namespace nightjar
