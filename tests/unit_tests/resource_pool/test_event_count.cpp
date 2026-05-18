#include <demiplane/multithread>

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

using namespace demiplane::multithread;
using namespace std::chrono_literals;

// prepare_wait registers a waiter; cancel_wait removes it.
TEST(EventCountTest, WaiterCountTracksPrepareAndCancel) {
    EventCount ec;
    EXPECT_EQ(ec.get_waiter_count(), 0u);

    const std::uint32_t key = ec.prepare_wait();
    EXPECT_EQ(ec.get_waiter_count(), 1u);
    EXPECT_EQ(key, ec.get_epoch());

    ec.cancel_wait();
    EXPECT_EQ(ec.get_waiter_count(), 0u);
}

// notify_one advances the epoch.
TEST(EventCountTest, NotifyAdvancesEpoch) {
    EventCount ec;
    const std::uint32_t e0 = ec.get_epoch();
    ec.notify_one();
    EXPECT_EQ(ec.get_epoch(), e0 + 1u);
    ec.notify_all();
    EXPECT_EQ(ec.get_epoch(), e0 + 2u);
}

// wait_until blocks until roughly the deadline when no notify arrives.
TEST(EventCountTest, WaitUntilTimesOut) {
    EventCount ec;
    const std::uint32_t key = ec.prepare_wait();

    const auto t0 = std::chrono::steady_clock::now();
    ec.wait_until(key, t0 + 50ms);
    const auto elapsed = std::chrono::steady_clock::now() - t0;

    EXPECT_GE(elapsed, 40ms);
    EXPECT_LT(elapsed, 2s);
}

// A notify wakes a parked waiter promptly (not via the 5s timeout).
TEST(EventCountTest, NotifyWakesParkedWaiter) {
    EventCount ec;
    std::atomic<bool> woke{false};

    std::thread waiter{[&] {
        // Each iteration: exactly one prepare_wait paired with exactly one of
        // {cancel_wait, wait_until}. A spurious wake just loops to re-register.
        for (;;) {
            const std::uint32_t key = ec.prepare_wait();
            if (ec.get_epoch() != key) {  // notify already landed
                ec.cancel_wait();
                break;
            }
            ec.wait_until(key, std::chrono::steady_clock::now() + 5s);  // consumes the registration
            if (ec.get_epoch() != key) {  // real notify
                break;
            }
            // spurious wake — loop; the next iteration does a fresh prepare_wait
        }
        woke.store(true, std::memory_order_release);
    }};

    std::this_thread::sleep_for(50ms);
    const auto t0 = std::chrono::steady_clock::now();
    ec.notify_one();
    waiter.join();

    EXPECT_TRUE(woke.load(std::memory_order_acquire));
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 2s);
    EXPECT_EQ(ec.get_waiter_count(), 0u);  // every prepare_wait was balanced
}

// The core guarantee: with the prepare_wait -> re-check -> wait_until protocol,
// a notify that races the waiter is never lost.
TEST(EventCountTest, LostWakeupRace) {
    EventCount ec;
    constexpr int iterations = 20'000;

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::atomic<bool> ready{false};
        std::thread releaser{[&] {
            ready.store(true, std::memory_order_release);
            ec.notify_one();
        }};
        // waiter side (this thread):
        while (!ready.load(std::memory_order_acquire)) {
            const std::uint32_t key = ec.prepare_wait();
            if (ready.load(std::memory_order_acquire)) {
                ec.cancel_wait();
                break;
            }
            ec.wait_until(key, std::chrono::steady_clock::now() + 5s);
        }
        releaser.join();
    }
    // A single lost wakeup would stall ~5s; 20k clean iterations finish well under this.
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 60s);
    EXPECT_EQ(ec.get_waiter_count(), 0u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
