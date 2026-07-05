#include "nightjar/event_clip_store.h"

#include <filesystem>
#include <string>

#include "check.h"

using namespace nightjar;
namespace fs = std::filesystem;

namespace {

GrayImage tiny(uint8_t v) {
    GrayImage g;
    g.width = 4;
    g.height = 4;
    g.pixels.assign(16, v);
    return g;
}

size_t count_pgm(const std::string& dir) {
    size_t n = 0;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(dir, ec))
        if (e.path().extension() == ".pgm") ++n;
    return n;
}

std::string fresh_dir(const char* tag) {
    fs::path d = fs::temp_directory_path() / (std::string("nj_clips_") + tag);
    fs::remove_all(d);
    return d.string();
}

void test_clip_has_preroll_and_postroll() {
    ClipConfig cfg;
    cfg.dir = fresh_dir("roll");
    cfg.pre_roll_frames = 5;
    cfg.post_roll_frames = 8;
    cfg.encoder = pgm_encoder();
    EventClipStore store(cfg);

    for (int i = 0; i < 10; ++i) store.on_frame(tiny(uint8_t(i)));  // ring keeps last 5
    std::string clip = store.begin_event("e1");
    CHECK(!clip.empty());
    for (int i = 0; i < 8; ++i) store.on_frame(tiny(100 + i));  // post-roll
    store.flush();

    // pre_roll (5) + post_roll (8) = 13 frames.
    CHECK_EQ(count_pgm(clip), size_t(13));
    fs::remove_all(cfg.dir);
}

void test_cap_evicts_oldest() {
    ClipConfig cfg;
    cfg.dir = fresh_dir("cap");
    cfg.pre_roll_frames = 2;
    cfg.post_roll_frames = 0;
    cfg.max_clips = 3;  // bounded storage
    cfg.encoder = pgm_encoder();
    EventClipStore store(cfg);

    std::string first;
    for (int i = 0; i < 6; ++i) {  // 6 events, cap 3
        store.on_frame(tiny(1));
        store.on_frame(tiny(2));
        std::string c = store.begin_event("e" + std::to_string(i));
        if (i == 0) first = c;
    }
    store.flush();
    CHECK_EQ(store.stored_clips(), size_t(3));  // never exceeds the cap
    CHECK(!fs::exists(first));                    // oldest evicted from disk
    fs::remove_all(cfg.dir);
}

void test_preroll_ring_bounded() {
    ClipConfig cfg;
    cfg.dir = fresh_dir("ring");
    cfg.pre_roll_frames = 3;
    cfg.post_roll_frames = 0;
    cfg.encoder = pgm_encoder();
    EventClipStore store(cfg);
    for (int i = 0; i < 100; ++i) store.on_frame(tiny(uint8_t(i)));  // ring never grows past 3
    std::string clip = store.begin_event("e");
    store.flush();
    CHECK_EQ(count_pgm(clip), size_t(3));  // only the last 3 pre-roll frames
    fs::remove_all(cfg.dir);
}

}  // namespace

int main() {
    test_clip_has_preroll_and_postroll();
    test_cap_evicts_oldest();
    test_preroll_ring_bounded();
    return njtest::failures() == 0 ? 0 : 1;
}
