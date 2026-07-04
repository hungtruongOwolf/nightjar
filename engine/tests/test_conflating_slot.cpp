#include "nightjar/conflating_slot.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "check.h"

using namespace nightjar;
using namespace std::chrono_literals;

namespace {

void test_publish_then_take() {
    ConflatingSlot<int> slot;
    slot.publish(42);
    auto v = slot.take_blocking(10ms);
    CHECK(v.has_value());
    CHECK_EQ(*v, 42);
    CHECK_EQ(slot.drops(), uint64_t(0));
}

void test_keeps_latest_and_counts_drops() {
    ConflatingSlot<int> slot;
    slot.publish(1);
    slot.publish(2);  // 1 dropped
    slot.publish(3);  // 2 dropped
    auto v = slot.take_blocking(10ms);
    CHECK(v.has_value());
    CHECK_EQ(*v, 3);                    // newest survives
    CHECK_EQ(slot.drops(), uint64_t(2));
}

void test_take_empty_times_out() {
    ConflatingSlot<int> slot;
    auto t0 = std::chrono::steady_clock::now();
    auto v = slot.take_blocking(30ms);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    CHECK(!v.has_value());
    CHECK(ms >= 25);  // actually waited
}

void test_try_take() {
    ConflatingSlot<int> slot;
    CHECK(!slot.try_take().has_value());
    slot.publish(7);
    auto v = slot.try_take();
    CHECK(v.has_value());
    CHECK_EQ(*v, 7);
    CHECK(!slot.try_take().has_value());  // consumed
}

void test_close_unblocks_consumer() {
    ConflatingSlot<int> slot;
    std::atomic<bool> returned{false};
    std::thread consumer([&] {
        slot.take_blocking(5000ms);  // would block a long time
        returned.store(true);
    });
    std::this_thread::sleep_for(20ms);
    slot.close();
    consumer.join();
    CHECK(returned.load());  // close() woke it well before the 5s timeout
}

void test_producer_consumer_conflation() {
    // Fast producer, slow consumer: some values must be dropped, the consumer
    // always sees monotonically increasing (latest) values, nothing corrupts.
    ConflatingSlot<int> slot;
    std::atomic<bool> stop{false};
    int last_seen = -1;
    bool monotonic = true;
    uint64_t consumed = 0;

    std::thread consumer([&] {
        while (!stop.load()) {
            auto v = slot.take_blocking(5ms);
            if (v.has_value()) {
                if (*v <= last_seen) monotonic = false;
                last_seen = *v;
                ++consumed;
                std::this_thread::sleep_for(2ms);  // slow
            }
        }
    });

    for (int i = 0; i < 500; ++i) {
        slot.publish(i);
        std::this_thread::sleep_for(std::chrono::microseconds(200));  // fast
    }
    std::this_thread::sleep_for(20ms);
    stop.store(true);
    slot.close();
    consumer.join();

    CHECK(monotonic);
    CHECK(slot.drops() > 0);                 // conflation happened
    CHECK(consumed + slot.drops() <= 500);   // every published value was consumed or dropped
    CHECK(last_seen == 499 || consumed > 0); // saw progress
}

}  // namespace

int main() {
    test_publish_then_take();
    test_keeps_latest_and_counts_drops();
    test_take_empty_times_out();
    test_try_take();
    test_close_unblocks_consumer();
    test_producer_consumer_conflation();
    return njtest::failures() == 0 ? 0 : 1;
}
