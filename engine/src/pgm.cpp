#include "nightjar/pgm.h"

#include <cctype>
#include <cstdio>
#include <memory>

namespace nightjar {
namespace {

// Read the next whitespace-separated integer token from a PGM header,
// skipping '#' comment lines. Returns false on EOF or non-numeric input.
bool read_header_int(std::FILE* f, int& out) {
    int c = std::getc(f);
    for (;;) {
        while (c != EOF && std::isspace(c)) c = std::getc(f);
        if (c == '#') {  // comment runs to end of line
            while (c != EOF && c != '\n') c = std::getc(f);
            continue;
        }
        break;
    }
    if (c == EOF || !std::isdigit(c)) return false;

    int value = 0;
    while (c != EOF && std::isdigit(c)) {
        value = value * 10 + (c - '0');
        c = std::getc(f);
    }
    out = value;
    return true;
}

}  // namespace

std::optional<GrayImage> read_pgm(const std::string& path) {
    std::unique_ptr<std::FILE, decltype(&std::fclose)> f(std::fopen(path.c_str(), "rb"),
                                                         &std::fclose);
    if (!f) return std::nullopt;

    char magic[2] = {0, 0};
    if (std::fread(magic, 1, 2, f.get()) != 2) return std::nullopt;
    if (magic[0] != 'P' || magic[1] != '5') return std::nullopt;

    int width = 0, height = 0, maxval = 0;
    if (!read_header_int(f.get(), width) || !read_header_int(f.get(), height) ||
        !read_header_int(f.get(), maxval)) {
        return std::nullopt;
    }
    if (width <= 0 || height <= 0 || maxval <= 0 || maxval > 255) return std::nullopt;
    // A single whitespace byte separates the header from the pixel data.
    // read_header_int already consumed it while scanning past maxval.

    GrayImage img;
    img.width = width;
    img.height = height;
    const size_t n = static_cast<size_t>(width) * static_cast<size_t>(height);
    img.pixels.resize(n);
    if (std::fread(img.pixels.data(), 1, n, f.get()) != n) return std::nullopt;
    return img;
}

}  // namespace nightjar
