#include "nightjar/alert_sink.h"

namespace nightjar {

void MultiSink::add(std::shared_ptr<IAlertSink> sink) {
    if (sink) sinks_.push_back(std::move(sink));
}

void MultiSink::send(const Alert& alert) {
    // One sink failing (or blocking) must not stop the others; concrete sinks
    // own their own retry/timeout. Here we simply fan out.
    for (auto& sink : sinks_) sink->send(alert);
}

void CapturingSink::send(const Alert& alert) {
    std::lock_guard<std::mutex> lock(mu_);
    alerts_.push_back(alert);
}

std::vector<Alert> CapturingSink::alerts() const {
    std::lock_guard<std::mutex> lock(mu_);
    return alerts_;
}

size_t CapturingSink::count() const {
    std::lock_guard<std::mutex> lock(mu_);
    return alerts_.size();
}

}  // namespace nightjar
