#include "nightjar/differential_clip_codec.h"

#include <cstring>

namespace nightjar {
namespace {

// Does block (bx,by) differ at all between two frames? (lossless: any pixel diff)
bool block_changed(const GrayImage& a, const GrayImage& b, int bx, int by, int block) {
    const int w = a.width;
    for (int y = 0; y < block; ++y) {
        const size_t row = static_cast<size_t>(by * block + y) * w + bx * block;
        if (std::memcmp(&a.pixels[row], &b.pixels[row], block) != 0) return true;
    }
    return false;
}

void put_u16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(static_cast<uint8_t>(x & 0xFF));
    v.push_back(static_cast<uint8_t>(x >> 8));
}
uint16_t get_u16(const std::vector<uint8_t>& v, size_t& i) {
    const uint16_t x = static_cast<uint16_t>(v[i] | (v[i + 1] << 8));
    i += 2;
    return x;
}

}  // namespace

DifferentialClipCodec::Encoded DifferentialClipCodec::encode(
    const std::vector<GrayImage>& frames) const {
    Encoded enc;
    if (frames.empty()) return enc;
    const int block = cfg_.block;
    enc.keyframe = frames[0];
    enc.width = frames[0].width;
    enc.height = frames[0].height;
    enc.block = block;
    const int gw = enc.width / block;
    const int gh = enc.height / block;

    for (size_t f = 1; f < frames.size(); ++f) {
        const GrayImage& cur = frames[f];
        const GrayImage& prev = frames[f - 1];
        std::vector<uint8_t> delta;
        std::vector<uint8_t> body;
        uint16_t changed = 0;
        for (int by = 0; by < gh; ++by) {
            for (int bx = 0; bx < gw; ++bx) {
                if (!block_changed(cur, prev, bx, by, block)) continue;
                ++changed;
                put_u16(body, static_cast<uint16_t>(by * gw + bx));  // block index
                for (int y = 0; y < block; ++y) {
                    const size_t row = static_cast<size_t>(by * block + y) * enc.width + bx * block;
                    body.insert(body.end(), &cur.pixels[row], &cur.pixels[row] + block);
                }
            }
        }
        put_u16(delta, changed);
        delta.insert(delta.end(), body.begin(), body.end());
        enc.deltas.push_back(std::move(delta));
    }
    return enc;
}

std::vector<GrayImage> DifferentialClipCodec::decode(const Encoded& enc) const {
    std::vector<GrayImage> out;
    if (enc.keyframe.pixels.empty()) return out;
    const int block = enc.block;
    const int gw = enc.width / block;

    GrayImage cur = enc.keyframe;
    out.push_back(cur);
    for (const auto& delta : enc.deltas) {
        size_t i = 0;
        const uint16_t changed = get_u16(delta, i);
        for (uint16_t c = 0; c < changed; ++c) {
            const uint16_t idx = get_u16(delta, i);
            const int bx = idx % gw;
            const int by = idx / gw;
            for (int y = 0; y < block; ++y) {
                const size_t row = static_cast<size_t>(by * block + y) * enc.width + bx * block;
                std::memcpy(&cur.pixels[row], &delta[i], block);
                i += block;
            }
        }
        out.push_back(cur);
    }
    return out;
}

size_t DifferentialClipCodec::delta_bytes(const Encoded& enc) {
    size_t n = enc.keyframe.pixels.size();  // keyframe raw (JPEG separately)
    for (const auto& d : enc.deltas) n += d.size();
    return n;
}

}  // namespace nightjar
