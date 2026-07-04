#pragma once

// Minimal zero-dependency test helper. A test is a plain executable that
// returns non-zero on failure; CHECK/CHECK_EQ print the failing location and
// flip a global fail flag. Keeps the repo buildable offline (no GoogleTest
// fetch) — the whole engine is meant to `cmake && ctest` on a fresh clone.

#include <cstdio>
#include <cstdlib>

namespace njtest {
inline int& failures() {
    static int f = 0;
    return f;
}
}  // namespace njtest

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "CHECK failed: %s\n  at %s:%d\n", #cond,      \
                         __FILE__, __LINE__);                                  \
            ++njtest::failures();                                              \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        auto _va = (a);                                                        \
        auto _vb = (b);                                                        \
        if (!(_va == _vb)) {                                                   \
            std::fprintf(stderr, "CHECK_EQ failed: %s == %s\n  at %s:%d\n",    \
                         #a, #b, __FILE__, __LINE__);                          \
            ++njtest::failures();                                              \
        }                                                                      \
    } while (0)

#define TEST_MAIN()                                                            \
    int main() { return njtest::failures() == 0 ? 0 : 1; }
