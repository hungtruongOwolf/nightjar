#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "nightjar/facts.h"
#include "nightjar/frame_ops.h"  // Letterboxed

namespace nightjar {

// One alert to deliver: which rule fired, a human one-liner, the timestamp, and
// the crop the user configured to receive. The image is the only thing that
// ever leaves the device (ethics constraint §8).
struct Alert {
    std::string rule_id;
    Subject subject = Subject::Person;
    std::string one_liner;   // e.g. "person in backyard at 23:42"
    int64_t unix_s = 0;
    Letterboxed image;       // the alert crop (may be empty in tests)
    bool degraded = false;   // true when running motion-only (VLM paused) — announced honestly
};

// Where alerts go. Concrete sinks (ntfy, Telegram, webhook) live in the
// platform shell and do the HTTP; the engine only depends on this interface.
class IAlertSink {
public:
    virtual ~IAlertSink() = default;
    virtual void send(const Alert& alert) = 0;
};

// Fan-out to several sinks (design v0.3 #4: dual-post ntfy + Telegram on every
// alert, so an intermittent ntfy iOS delivery never silently loses the alert).
class MultiSink : public IAlertSink {
public:
    void add(std::shared_ptr<IAlertSink> sink);
    void send(const Alert& alert) override;

private:
    std::vector<std::shared_ptr<IAlertSink>> sinks_;
};

// Test/harness sink: records every alert it receives.
class CapturingSink : public IAlertSink {
public:
    void send(const Alert& alert) override;
    std::vector<Alert> alerts() const;
    size_t count() const;

private:
    mutable std::mutex mu_;
    std::vector<Alert> alerts_;
};

}  // namespace nightjar
