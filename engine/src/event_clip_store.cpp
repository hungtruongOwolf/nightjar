#include "nightjar/event_clip_store.h"

#include <cstdio>
#include <filesystem>

namespace nightjar {
namespace fs = std::filesystem;

EventClipStore::EventClipStore(ClipConfig config) : config_(std::move(config)) {
    std::error_code ec;
    fs::create_directories(config_.dir, ec);
}

void EventClipStore::write_frame_to_current(const GrayImage& f) {
    char name[64];
    std::snprintf(name, sizeof(name), "frame_%04d.pgm", current_idx_++);
    write_pgm((fs::path(current_dir_) / name).string(), f);
}

void EventClipStore::on_frame(const GrayImage& frame) {
    if (config_.sample_every > 1 && (frame_counter_++ % config_.sample_every) != 0 &&
        !recording()) {
        return;  // downsample the pre-roll ring; always keep frames while recording
    }

    // Maintain the pre-roll ring.
    preroll_.push_back(frame);
    while (static_cast<int>(preroll_.size()) > config_.pre_roll_frames) preroll_.pop_front();

    // If recording, append to the current clip.
    if (recording()) {
        write_frame_to_current(frame);
        --remaining_post_;
    }
}

std::string EventClipStore::begin_event(const std::string& event_id) {
    current_dir_ = (fs::path(config_.dir) / ("event_" + event_id)).string();
    std::error_code ec;
    fs::create_directories(current_dir_, ec);
    if (ec) return "";
    current_idx_ = 0;

    // Dump the pre-roll (context BEFORE the event — evidence is a span, not a frame).
    for (const GrayImage& f : preroll_) write_frame_to_current(f);
    remaining_post_ = config_.post_roll_frames;

    clip_dirs_.push_back(current_dir_);
    evict_to_cap();
    return current_dir_;
}

void EventClipStore::evict_to_cap() {
    while (static_cast<int>(clip_dirs_.size()) > config_.max_clips) {
        std::error_code ec;
        fs::remove_all(clip_dirs_.front(), ec);  // delete oldest clip
        clip_dirs_.pop_front();
    }
}

size_t EventClipStore::stored_clips() const { return clip_dirs_.size(); }

}  // namespace nightjar
