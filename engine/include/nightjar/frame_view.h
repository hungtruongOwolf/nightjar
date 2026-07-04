#pragma once

#include <cstdint>

namespace nightjar {

// A zero-copy, non-owning view of one captured luma (Y) plane.
//
// The capture source owns the pixels; a FrameView is only valid for the
// duration of the on_frame callback unless the consumer retains the
// underlying buffer via native_handle. Stride may exceed width (the ISP
// often pads rows), so every pixel walk must use `stride`, never `width`,
// to advance between rows.
struct FrameView {
    const uint8_t* y_plane = nullptr;  // luma, row-major, `stride` bytes/row
    int width = 0;
    int height = 0;
    int stride = 0;                    // bytes per row; may be > width
    uint64_t ts_mono_ns = 0;           // capture time (t0), monotonic clock
    uint64_t seq = 0;                  // monotonically increasing frame index
    void* native_handle = nullptr;     // platform buffer (e.g. CVPixelBufferRef); retain to keep pixels alive
};

}  // namespace nightjar
