// ntfy_probe, send one test alert (text + a generated image) to an ntfy topic
// and print the HTTP status. Subscribe to the topic in the ntfy app to see it.
//   ntfy_probe <topic> [server]   (server default https://ntfy.sh)

#include <cmath>
#include <cstdio>
#include <vector>

#include "nightjar/alert_sink.h"
#include "ntfy_sink.h"

using namespace nightjar;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <topic> [server]\n", argv[0]);
        return 2;
    }
    NtfyConfig cfg;
    cfg.topic = argv[1];
    if (argc > 2) cfg.server = argv[2];
    NtfySink sink(cfg);

    // A recognizable test image (diagonal gradient with a bright square).
    const int n = 224;
    Alert alert;
    alert.subject = Subject::Person;
    alert.one_liner = "test alert from nightjar at 23:42 (ntfy probe)";
    alert.image.size = n;
    alert.image.pixels.assign(size_t(n) * n, 0);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            alert.image.pixels[size_t(y) * n + x] =
                static_cast<uint8_t>((x + y) * 255 / (2 * n));

    sink.send(alert);
    std::printf("ntfy POST -> %s/%s : HTTP %d\n", cfg.server.c_str(), cfg.topic.c_str(),
                sink.last_status());
    return (sink.last_status() >= 200 && sink.last_status() < 300) ? 0 : 1;
}
