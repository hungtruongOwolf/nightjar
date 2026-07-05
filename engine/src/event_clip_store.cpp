#include "nightjar/event_clip_store.h"

#include <cstdio>
#include <filesystem>

namespace nightjar {
namespace fs = std::filesystem;

EventClipStore::EventClipStore(ClipConfig config) : config_(std::move(config)) {
    std::error_code ec;
    fs::create_directories(config_.dir, ec);
    worker_ = std::thread([this] { worker_loop(); });
}

EventClipStore::~EventClipStore() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void EventClipStore::on_frame(const GrayImage& frame) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        // Best-effort: if the writer is behind, drop the oldest queued FRAME so
        // the capture thread never blocks (the ring tolerates gaps).
        if (static_cast<int>(queue_.size()) >= config_.queue_max) {
            for (auto it = queue_.begin(); it != queue_.end(); ++it) {
                if (it->type == Cmd::Frame) {
                    queue_.erase(it);
                    dropped_frames_.fetch_add(1);
                    break;
                }
            }
        }
        Cmd c;
        c.type = Cmd::Frame;
        c.frame = frame;  // copy (RAM, cheap); disk I/O happens on the worker
        queue_.push_back(std::move(c));
    }
    cv_.notify_one();
}

std::string EventClipStore::begin_event(const std::string& event_id) {
    const std::string dir = (fs::path(config_.dir) / ("event_" + event_id)).string();
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) return "";
    {
        std::lock_guard<std::mutex> lock(mu_);
        Cmd c;
        c.type = Cmd::Begin;
        c.dir = dir;
        queue_.push_back(std::move(c));  // never dropped
    }
    cv_.notify_one();
    return dir;
}

void EventClipStore::flush() {
    std::unique_lock<std::mutex> lock(mu_);
    drained_cv_.wait(lock, [this] { return queue_.empty() && !processing_; });
}

void EventClipStore::worker_loop() {
    // All clip state is owned here — no cross-thread sharing beyond the queue.
    std::deque<GrayImage> preroll;
    std::deque<std::string> clip_dirs;
    std::string cur_dir;
    int cur_idx = 0;
    int remaining_post = 0;
    DifferentialClipCodec codec;   // for differential mode
    GrayImage prev_frame;          // previous written frame (differential)
    bool have_prev = false;

    auto write_bytes = [&](const std::string& name, const uint8_t* data, size_t n) {
        std::FILE* fp = std::fopen((fs::path(cur_dir) / name).string().c_str(), "wb");
        if (fp) {
            std::fwrite(data, 1, n, fp);
            std::fclose(fp);
        }
    };

    auto write_frame = [&](const GrayImage& f) {
        char name[64];
        if (config_.differential && have_prev) {
            // Inter-frame delta: only the blocks that changed vs the previous frame.
            const std::vector<uint8_t> delta = codec.encode_frame(f, prev_frame);
            std::snprintf(name, sizeof(name), "frame_%04d.delta", cur_idx++);
            write_bytes(name, delta.data(), delta.size());
            prev_frame = f;
        } else {
            // Keyframe (or non-differential): full frame via the encoder.
            const EncodedFrame ef = config_.encoder(f);
            std::snprintf(name, sizeof(name), "frame_%04d.%s", cur_idx++, ef.ext.c_str());
            write_bytes(name, ef.bytes.data(), ef.bytes.size());
            if (config_.differential) {
                prev_frame = f;
                have_prev = true;
            }
        }
    };

    for (;;) {
        Cmd cmd;
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [this] { return !queue_.empty() || stop_; });
            if (stop_ && queue_.empty()) break;
            cmd = std::move(queue_.front());
            queue_.pop_front();
            processing_ = true;  // not "drained" until the write below finishes
        }

        if (cmd.type == Cmd::Frame) {
            preroll.push_back(cmd.frame);
            while (static_cast<int>(preroll.size()) > config_.pre_roll_frames) preroll.pop_front();
            if (remaining_post > 0) {
                write_frame(cmd.frame);
                --remaining_post;
            }
        } else {  // Begin
            cur_dir = cmd.dir;
            cur_idx = 0;
            have_prev = false;  // each clip starts with a fresh keyframe
            for (const GrayImage& f : preroll) write_frame(f);  // dump the pre-roll context
            remaining_post = config_.post_roll_frames;

            clip_dirs.push_back(cur_dir);
            while (static_cast<int>(clip_dirs.size()) > config_.max_clips) {
                std::error_code ec;
                fs::remove_all(clip_dirs.front(), ec);  // evict oldest
                clip_dirs.pop_front();
            }
            stored_.store(clip_dirs.size());
        }

        {
            std::lock_guard<std::mutex> lock(mu_);
            processing_ = false;
            if (queue_.empty()) drained_cv_.notify_all();
        }
    }
}

}  // namespace nightjar
