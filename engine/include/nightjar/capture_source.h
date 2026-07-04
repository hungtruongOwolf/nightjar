#pragma once

#include <functional>

#include "nightjar/frame_view.h"

namespace nightjar {

// Pluggable frame source: AVFoundation on device, FileReplay for the test
// harness, RTSP in the future. The engine core depends only on this
// interface, never on a concrete capture backend.
//
// Contract: the on_frame callback must do nothing heavy — it runs on the
// source's delivery thread and any time spent in it delays (or drops) the
// next frame. Real work belongs downstream, behind a ConflatingSlot.
class ICaptureSource {
public:
    using FrameCallback = std::function<void(const FrameView&)>;

    virtual ~ICaptureSource() = default;

    // Begin delivering frames to `on_frame`. Non-blocking; delivery happens
    // on the source's own thread until stop() is called.
    virtual void start(FrameCallback on_frame) = 0;

    // Stop delivery and join the delivery thread. Safe to call more than once.
    virtual void stop() = 0;
};

}  // namespace nightjar
