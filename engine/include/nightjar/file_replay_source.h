#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "nightjar/capture_source.h"
#include "nightjar/pgm.h"

namespace nightjar {

struct ReplayConfig {
    std::string frames_dir;   // directory of *.pgm frames, delivered in name order
    double fps = 30.0;        // wall-clock delivery rate
    bool loop = false;        // restart from frame 0 after the last frame
};

// Replays a PGM frame sequence on the clip's wall-clock schedule, exactly as
// a real camera would. Critically (anti-coordinated-omission, CLAUDE.md §5):
// the pump never waits for a slow consumer. If a frame's scheduled slot has
// already passed because on_frame ran long, that frame is DROPPED and counted
// — the clip plays in real time and a busy system loses frames rather than
// stretching the timeline. Frames are pre-loaded into memory before the clock
// starts so disk I/O never pollutes the timing.
class FileReplaySource : public ICaptureSource {
public:
    explicit FileReplaySource(ReplayConfig config);
    ~FileReplaySource() override;

    void start(FrameCallback on_frame) override;
    void stop() override;

    // Block until a finite (loop=false) clip finishes replaying on its own.
    // This is the harness's "play the whole clip, then score it" call, as
    // opposed to stop(), which aborts mid-clip. No-op if not running.
    void wait();

    // Telemetry, valid after stop() (or any time during a run).
    uint64_t frames_delivered() const { return delivered_.load(); }
    uint64_t frames_dropped() const { return dropped_.load(); }
    size_t frames_loaded() const { return frames_.size(); }

private:
    void pump(FrameCallback on_frame);

    ReplayConfig config_;
    std::vector<GrayImage> frames_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<uint64_t> delivered_{0};
    std::atomic<uint64_t> dropped_{0};
};

}  // namespace nightjar
