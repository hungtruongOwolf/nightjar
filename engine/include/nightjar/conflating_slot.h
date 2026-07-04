#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace nightjar {

// A single-slot, keep-latest hand-off between one producer and one consumer
// (design doc §5.3). The gate produces candidate frames faster than the VLM
// can consume them; rather than queue and fall behind, the slot keeps only the
// newest value and counts how many were dropped — this is what makes the
// end-to-end latency honest under load (a stale candidate is never processed).
template <typename T>
class ConflatingSlot {
public:
    // Overwrite the slot with the newest value. If a previous value had not
    // been taken yet, it is dropped (and counted).
    void publish(T value) {
        std::lock_guard<std::mutex> lock(mu_);
        if (slot_.has_value()) ++drops_;
        slot_ = std::move(value);
        cv_.notify_one();
    }

    // Wait up to `timeout` for a value. Returns nullopt on timeout or if the
    // slot was closed while empty.
    std::optional<T> take_blocking(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait_for(lock, timeout, [&] { return slot_.has_value() || closed_; });
        if (!slot_.has_value()) return std::nullopt;
        std::optional<T> out = std::move(slot_);
        slot_.reset();
        return out;
    }

    // Non-blocking take.
    std::optional<T> try_take() {
        std::lock_guard<std::mutex> lock(mu_);
        if (!slot_.has_value()) return std::nullopt;
        std::optional<T> out = std::move(slot_);
        slot_.reset();
        return out;
    }

    // Wake any blocked consumer so it can exit (shutdown).
    void close() {
        std::lock_guard<std::mutex> lock(mu_);
        closed_ = true;
        cv_.notify_all();
    }

    uint64_t drops() const {
        std::lock_guard<std::mutex> lock(mu_);
        return drops_;
    }

private:
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::optional<T> slot_;
    uint64_t drops_ = 0;
    bool closed_ = false;
};

}  // namespace nightjar
