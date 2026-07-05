#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#include "nightjar/clip_encoder.h"
#include "nightjar/pgm.h"

namespace nightjar {

struct ClipConfig {
    std::string dir = "clips";   // where event clips are written
    int max_clips = 50;          // HARD CAP — oldest clips evicted beyond this (bounded storage)
    int pre_roll_frames = 30;    // frames of context kept before the event (~1s @ 30fps)
    int post_roll_frames = 60;   // frames recorded after the event fires (~2s)
    int queue_max = 240;         // command-queue cap; oldest frame commands dropped if exceeded
    // How clip frames are serialized. Default JPEG (~10x smaller than raw PGM);
    // the harness can pass pgm_encoder() for lossless, the iOS shell a hardware
    // HEVC encoder. Frames are already downscaled + cropped upstream.
    ClipEncoder encoder = jpeg_encoder(70);
};

// Bounded, rotating on-device clip storage — the answer to "camera memory is
// never enough". Never keeps continuous video or a growing database: a small
// pre-roll ring, and on an event a short clip (pre-roll + post-roll) written to
// disk, at most `max_clips` (oldest evicted).
//
// All clip state and disk I/O live on a single owner (a writer thread); the
// capture and VLM threads only enqueue commands. This keeps the capture fast
// path free of disk I/O (consistent with the pipeline's microsecond tick
// handler) — frame commands are best-effort (dropped if the queue is full),
// begin-event commands are never dropped.
class EventClipStore {
public:
    explicit EventClipStore(ClipConfig config);
    ~EventClipStore();

    // Capture thread: hand every (downscaled) frame to the store (cheap enqueue).
    void on_frame(const GrayImage& frame);

    // Any thread (typically the VLM thread on alert): start an event clip.
    // Returns the directory the clip will be written to (created synchronously).
    std::string begin_event(const std::string& event_id);

    // Block until the writer has drained the queue (tests / shutdown).
    void flush();

    size_t stored_clips() const { return stored_.load(); }

private:
    struct Cmd {
        enum Type { Frame, Begin } type = Frame;
        GrayImage frame;
        std::string dir;
    };
    void worker_loop();

    ClipConfig config_;
    std::thread worker_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::condition_variable drained_cv_;
    std::deque<Cmd> queue_;
    bool stop_ = false;
    bool processing_ = false;  // a command is popped and being written
    std::atomic<size_t> stored_{0};
    std::atomic<uint64_t> dropped_frames_{0};
};

}  // namespace nightjar
