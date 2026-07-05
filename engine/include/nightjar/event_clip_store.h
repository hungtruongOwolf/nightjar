#pragma once

#include <cstdint>
#include <deque>
#include <string>

#include "nightjar/pgm.h"

namespace nightjar {

struct ClipConfig {
    std::string dir = "clips";   // where event clips are written
    int max_clips = 50;          // HARD CAP — oldest clips evicted beyond this (bounded storage)
    int pre_roll_frames = 30;    // frames of context kept before the event (~1s @ 30fps)
    int post_roll_frames = 60;   // frames recorded after the event fires (~2s)
    int sample_every = 1;        // keep 1 in N frames (downsample the clip to save space)
};

// Bounded, rotating on-device clip storage — the answer to "camera memory is
// never enough". We never keep continuous video or a growing database: a small
// pre-roll ring lives in RAM, and when an event fires a short clip (pre-roll +
// post-roll) is written to disk. The store keeps at most `max_clips`; the oldest
// is evicted when a new one would exceed the cap. All on-device; a clip is
// evidence (a span of frames, not a single image) and the user can delete it.
class EventClipStore {
public:
    explicit EventClipStore(ClipConfig config);

    // Feed every (downscaled) frame. Maintains the pre-roll ring, and if a clip
    // is being recorded, appends this frame until post_roll is satisfied.
    void on_frame(const GrayImage& frame);

    // Start an event clip: writes the pre-roll immediately, then records the next
    // post_roll frames. Returns the clip directory (empty on failure). Enforces
    // the max_clips cap by evicting the oldest clip(s).
    std::string begin_event(const std::string& event_id);

    bool recording() const { return remaining_post_ > 0; }
    size_t stored_clips() const;

private:
    void write_frame_to_current(const GrayImage& f);
    void evict_to_cap();

    ClipConfig config_;
    std::deque<GrayImage> preroll_;  // ring of recent frames (<= pre_roll_frames)
    std::string current_dir_;
    int current_idx_ = 0;
    int remaining_post_ = 0;
    int frame_counter_ = 0;  // for sample_every
    std::deque<std::string> clip_dirs_;  // oldest -> newest, for eviction
};

}  // namespace nightjar
