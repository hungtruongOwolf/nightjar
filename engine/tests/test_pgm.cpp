#include "nightjar/pgm.h"

#include <cstdio>
#include <filesystem>
#include <string>

#include "check.h"

using namespace nightjar;

namespace {

std::string tmp_path(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

void write_bytes(const std::string& path, const char* data, size_t n) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    std::fwrite(data, 1, n, f);
    std::fclose(f);
}

void test_valid_with_comment() {
    // 2x2 image, a comment line in the header, pixel values 0,64,128,255.
    const char data[] = "P5\n# a comment\n2 2\n255\n";
    std::string path = tmp_path("nj_test_valid.pgm");
    std::string full(data, sizeof(data) - 1);
    full.push_back(0);    // (0,0)
    full.push_back(64);   // (1,0)
    full.push_back(char(128));  // (0,1)
    full.push_back(char(255));  // (1,1)
    write_bytes(path, full.data(), full.size());

    auto img = read_pgm(path);
    CHECK(img.has_value());
    CHECK_EQ(img->width, 2);
    CHECK_EQ(img->height, 2);
    CHECK_EQ(img->pixels.size(), size_t(4));
    CHECK_EQ(int(img->pixels[0]), 0);
    CHECK_EQ(int(img->pixels[1]), 64);
    CHECK_EQ(int(img->pixels[2]), 128);
    CHECK_EQ(int(img->pixels[3]), 255);
    std::filesystem::remove(path);
}

void test_rejects_wrong_magic() {
    const char data[] = "P2\n2 2\n255\n";
    std::string path = tmp_path("nj_test_p2.pgm");
    write_bytes(path, data, sizeof(data) - 1);
    CHECK(!read_pgm(path).has_value());
    std::filesystem::remove(path);
}

void test_rejects_truncated_pixels() {
    // Header claims 2x2 (4 bytes) but only 2 pixel bytes follow.
    std::string path = tmp_path("nj_test_trunc.pgm");
    std::string full = "P5\n2 2\n255\n";
    full.push_back(10);
    full.push_back(20);
    write_bytes(path, full.data(), full.size());
    CHECK(!read_pgm(path).has_value());
    std::filesystem::remove(path);
}

void test_missing_file() { CHECK(!read_pgm("/no/such/nightjar_file.pgm").has_value()); }

}  // namespace

int main() {
    test_valid_with_comment();
    test_rejects_wrong_magic();
    test_rejects_truncated_pixels();
    test_missing_file();
    return njtest::failures() == 0 ? 0 : 1;
}
