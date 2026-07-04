#include "nightjar/file_replay_source.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace nightjar {
namespace {

using clock = std::chrono::steady_clock;

uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch())
            .count());
}

// Collect *.pgm paths in the directory, sorted by filename so frame_0001,
// frame_0002, ... replay in order.
std::vector<std::string> list_pgm_frames(const std::string& dir) {
    std::vector<std::string> paths;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".pgm") {
            paths.push_back(entry.path().string());
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

}  // namespace

FileReplaySource::FileReplaySource(ReplayConfig config) : config_(std::move(config)) {
    for (const auto& path : list_pgm_frames(config_.frames_dir)) {
        if (auto img = read_pgm(path)) frames_.push_back(std::move(*img));
    }
}

FileReplaySource::~FileReplaySource() { stop(); }

void FileReplaySource::start(FrameCallback on_frame) {
    if (thread_.joinable() || frames_.empty()) return;
    stop_.store(false);
    thread_ = std::thread([this, cb = std::move(on_frame)]() mutable { pump(std::move(cb)); });
}

void FileReplaySource::stop() {
    stop_.store(true);
    if (thread_.joinable()) thread_.join();
}

void FileReplaySource::wait() {
    if (thread_.joinable()) thread_.join();
}

void FileReplaySource::pump(FrameCallback on_frame) {
    const auto period_ns = static_cast<uint64_t>(1e9 / config_.fps);
    const uint64_t t0 = now_ns();
    uint64_t seq = 0;

    do {
        for (size_t i = 0; i < frames_.size() && !stop_.load(); ++i) {
            const uint64_t scheduled = t0 + (seq * period_ns);
            const uint64_t now = now_ns();

            // Slot already gone (consumer ran long): drop this frame, as a real
            // camera + downstream ConflatingSlot would. Do not stretch time.
            if (now > scheduled + period_ns) {
                dropped_.fetch_add(1);
                ++seq;
                continue;
            }
            if (now < scheduled) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(scheduled - now));
            }

            const GrayImage& img = frames_[i];
            FrameView view;
            view.y_plane = img.pixels.data();
            view.width = img.width;
            view.height = img.height;
            view.stride = img.width;  // PGM is tightly packed
            view.ts_mono_ns = now_ns();
            view.seq = seq;
            on_frame(view);

            delivered_.fetch_add(1);
            ++seq;
        }
    } while (config_.loop && !stop_.load());
}

}  // namespace nightjar
