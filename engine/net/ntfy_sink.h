#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "nightjar/alert_sink.h"

namespace nightjar {

struct NtfyConfig {
    std::string server = "https://ntfy.sh";  // hosted default; self-host by changing this
    std::string topic;                       // e.g. "nightjar-demo-x7k2"
    int timeout_s = 8;
    int retries = 1;  // design §5.6: retry once
};

// Alert sink that pushes to an ntfy pub-sub topic (hosted or self-hosted).
// Anyone who subscribes to the topic in the ntfy app gets the push — no
// account, works for many users. The alert crop is PNG-encoded and attached;
// it is the only thing that leaves the device (ethics §8). The on-device iOS
// shell implements IAlertSink with URLSession; this C++ sink drives the Mac
// replay/companion path via libcurl.
class NtfySink : public IAlertSink {
public:
    explicit NtfySink(NtfyConfig config);
    void send(const Alert& alert) override;

    // Result of the last send (for the harness / tools to report).
    int last_status() const { return last_status_; }

private:
    NtfyConfig config_;
    int last_status_ = 0;
};

// Encode a square grayscale Letterboxed image to PNG bytes (stb). Exposed for
// testing. Returns empty on failure or empty input.
std::vector<uint8_t> encode_gray_png(const uint8_t* pixels, int size);

// Priority/tags string for an alert (pure, testable): degraded alerts are
// flagged honestly.
struct NtfyFields {
    std::string title;
    std::string tags;
    std::string priority;
};
NtfyFields ntfy_fields_for(const Alert& alert);

}  // namespace nightjar
