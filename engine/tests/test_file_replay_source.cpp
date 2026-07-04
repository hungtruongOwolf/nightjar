#include "nightjar/file_replay_source.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "check.h"

using namespace nightjar;
namespace fs = std::filesystem;

namespace {

// Write `count` 1x1 PGM frames into a fresh temp dir; frame i has pixel value i
// so delivery order can be verified. Returns the dir path.
std::string make_frames(const char* tag, int count) {
    fs::path dir = fs::temp_directory_path() / (std::string("nj_replay_") + tag);
    fs::remove_all(dir);
    fs::create_directories(dir);
    for (int i = 0; i < count; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "frame_%04d.pgm", i);
        std::FILE* f = std::fopen((dir / name).string().c_str(), "wb");
        std::fprintf(f, "P5\n1 1\n255\n");
        unsigned char v = static_cast<unsigned char>(i);
        std::fwrite(&v, 1, 1, f);
        std::fclose(f);
    }
    return dir.string();
}

void test_delivers_all_in_order_fast_consumer() {
    std::string dir = make_frames("order", 6);
    FileReplaySource src({dir, /*fps=*/200.0, /*loop=*/false});
    CHECK_EQ(src.frames_loaded(), size_t(6));

    std::vector<int> got;
    src.start([&](const FrameView& v) { got.push_back(int(v.y_plane[0])); });
    src.wait();

    CHECK_EQ(src.frames_delivered(), uint64_t(6));
    CHECK_EQ(src.frames_dropped(), uint64_t(0));
    CHECK_EQ(got.size(), size_t(6));
    for (int i = 0; i < 6; ++i) CHECK_EQ(got[size_t(i)], i);
    fs::remove_all(dir);
}

void test_respects_wall_clock_schedule() {
    std::string dir = make_frames("clock", 5);
    FileReplaySource src({dir, /*fps=*/50.0, /*loop=*/false});  // 20ms period

    auto t0 = std::chrono::steady_clock::now();
    src.start([](const FrameView&) {});
    src.wait();
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0)
            .count();

    // 5 frames at 20ms spacing => ~80ms. Loose bounds to stay non-flaky on CI.
    CHECK(elapsed_ms >= 60);
    CHECK(elapsed_ms < 300);
    CHECK_EQ(src.frames_delivered(), uint64_t(5));
    fs::remove_all(dir);
}

void test_drops_frames_when_consumer_is_slow() {
    std::string dir = make_frames("slow", 20);
    FileReplaySource src({dir, /*fps=*/1000.0, /*loop=*/false});  // 1ms period

    // Each callback takes 20ms — far longer than the 1ms slot, so most slots
    // pass and their frames must be dropped, not queued (anti-coordinated-omission).
    src.start([](const FrameView&) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); });
    src.wait();

    CHECK(src.frames_dropped() > 0);
    CHECK(src.frames_delivered() >= 1);
    // Invariant for a non-looping run: every loaded frame is either delivered or dropped.
    CHECK_EQ(src.frames_delivered() + src.frames_dropped(), uint64_t(src.frames_loaded()));
    fs::remove_all(dir);
}

void test_empty_dir_delivers_nothing() {
    std::string dir = make_frames("empty", 0);
    FileReplaySource src({dir, 30.0, false});
    src.start([](const FrameView&) {});
    src.wait();
    CHECK_EQ(src.frames_delivered(), uint64_t(0));
    fs::remove_all(dir);
}

}  // namespace

int main() {
    test_delivers_all_in_order_fast_consumer();
    test_respects_wall_clock_schedule();
    test_drops_frames_when_consumer_is_slow();
    test_empty_dir_delivers_nothing();
    return njtest::failures() == 0 ? 0 : 1;
}
