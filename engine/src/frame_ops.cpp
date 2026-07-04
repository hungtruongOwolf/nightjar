#include "nightjar/frame_ops.h"

#include <algorithm>
#include <cmath>

namespace nightjar {

Rect expand_rect(Rect r, float margin, int max_w, int max_h) {
    const int dx = static_cast<int>(std::lround(r.w * margin));
    const int dy = static_cast<int>(std::lround(r.h * margin));
    int x0 = r.x - dx;
    int y0 = r.y - dy;
    int x1 = r.x + r.w + dx;
    int y1 = r.y + r.h + dy;
    x0 = std::max(0, x0);
    y0 = std::max(0, y0);
    x1 = std::min(max_w, x1);
    y1 = std::min(max_h, y1);
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    return Rect{x0, y0, x1 - x0, y1 - y0};
}

namespace {

// Bilinear sample from a grayscale plane at fractional (fx, fy).
uint8_t sample_bilinear(const uint8_t* src, int sw, int sh, int sstride, double fx, double fy) {
    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (fx > sw - 1) fx = sw - 1;
    if (fy > sh - 1) fy = sh - 1;
    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, sw - 1);
    const int y1 = std::min(y0 + 1, sh - 1);
    const double ax = fx - x0;
    const double ay = fy - y0;
    const double p00 = src[static_cast<size_t>(y0) * sstride + x0];
    const double p10 = src[static_cast<size_t>(y0) * sstride + x1];
    const double p01 = src[static_cast<size_t>(y1) * sstride + x0];
    const double p11 = src[static_cast<size_t>(y1) * sstride + x1];
    const double top = p00 + (p10 - p00) * ax;
    const double bot = p01 + (p11 - p01) * ax;
    return static_cast<uint8_t>(std::lround(top + (bot - top) * ay));
}

Rect clamp_rect(Rect r, int max_w, int max_h) {
    int x0 = std::max(0, r.x);
    int y0 = std::max(0, r.y);
    int x1 = std::min(max_w, r.x + r.w);
    int y1 = std::min(max_h, r.y + r.h);
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    return Rect{x0, y0, x1 - x0, y1 - y0};
}

}  // namespace

Letterboxed crop_and_letterbox(const uint8_t* src, int sw, int sh, int sstride, Rect crop,
                               int size, uint8_t pad) {
    Letterboxed out;
    out.size = size;
    out.pixels.assign(static_cast<size_t>(size) * size, pad);

    const Rect c = clamp_rect(crop, sw, sh);
    out.used_crop = c;
    if (c.w <= 0 || c.h <= 0) return out;

    // Scale so the long side maps to `size`, preserving aspect.
    const double scale = static_cast<double>(size) / std::max(c.w, c.h);
    const int dst_w = std::max(1, static_cast<int>(std::lround(c.w * scale)));
    const int dst_h = std::max(1, static_cast<int>(std::lround(c.h * scale)));
    const int off_x = (size - dst_w) / 2;  // center the content
    const int off_y = (size - dst_h) / 2;

    for (int dy = 0; dy < dst_h; ++dy) {
        // Map destination pixel centers back into the crop region.
        const double fy = c.y + (dy + 0.5) * c.h / dst_h - 0.5;
        uint8_t* drow = out.pixels.data() + static_cast<size_t>(off_y + dy) * size + off_x;
        for (int dx = 0; dx < dst_w; ++dx) {
            const double fx = c.x + (dx + 0.5) * c.w / dst_w - 0.5;
            drow[dx] = sample_bilinear(src, sw, sh, sstride, fx, fy);
        }
    }
    return out;
}

double variance_of_laplacian(const uint8_t* gray, int w, int h, int stride) {
    if (w < 3 || h < 3) return 0.0;
    double sum = 0.0, sum_sq = 0.0;
    long n = 0;
    for (int y = 1; y < h - 1; ++y) {
        const uint8_t* row = gray + static_cast<size_t>(y) * stride;
        const uint8_t* up = gray + static_cast<size_t>(y - 1) * stride;
        const uint8_t* down = gray + static_cast<size_t>(y + 1) * stride;
        for (int x = 1; x < w - 1; ++x) {
            const int lap = 4 * row[x] - up[x] - down[x] - row[x - 1] - row[x + 1];
            sum += lap;
            sum_sq += static_cast<double>(lap) * lap;
            ++n;
        }
    }
    if (n == 0) return 0.0;
    const double mean = sum / n;
    return sum_sq / n - mean * mean;
}

}  // namespace nightjar
