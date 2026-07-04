#include <cstdint>

#include "check.h"
#include "ntfy_sink.h"

using namespace nightjar;

namespace {

void test_png_encoding_has_signature() {
    const int n = 16;
    std::vector<uint8_t> gray(size_t(n) * n, 128);
    auto png = encode_gray_png(gray.data(), n);
    CHECK(png.size() > 8);
    // PNG magic: 89 50 4E 47 0D 0A 1A 0A
    CHECK_EQ(int(png[0]), 0x89);
    CHECK_EQ(int(png[1]), 'P');
    CHECK_EQ(int(png[2]), 'N');
    CHECK_EQ(int(png[3]), 'G');
}

void test_png_empty_input() {
    CHECK(encode_gray_png(nullptr, 0).empty());
}

void test_fields_normal_vs_degraded() {
    Alert normal;
    normal.subject = Subject::Vehicle;
    auto f = ntfy_fields_for(normal);
    CHECK(f.title.find("Vehicle") != std::string::npos);
    CHECK_EQ(f.priority, std::string("high"));

    Alert degraded;
    degraded.degraded = true;
    auto d = ntfy_fields_for(degraded);
    CHECK(d.title.find("motion-only") != std::string::npos);  // honest degraded label
    CHECK_EQ(d.priority, std::string("default"));
}

}  // namespace

int main() {
    test_png_encoding_has_signature();
    test_png_empty_input();
    test_fields_normal_vs_degraded();
    return njtest::failures() == 0 ? 0 : 1;
}
