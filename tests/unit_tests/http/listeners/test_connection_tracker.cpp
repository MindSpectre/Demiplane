#include <atomic>
#include <chrono>
#include <memory>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <gtest/gtest.h>

#include <connection_tracker.hpp>

using namespace demiplane::http;
using namespace std::chrono_literals;

namespace {
    // Minimal connection model: owns a strand + a cancel flag. cancel() is what
    // ConnectionTracker requires structurally (D2) — not part of the Connection
    // concept.
    struct FakeConn {
        boost::asio::strand<boost::asio::io_context::executor_type> strand;
        std::atomic<bool> cancelled{false};
        explicit FakeConn(boost::asio::io_context& ioc) : strand{boost::asio::make_strand(ioc)} {}
        void cancel() noexcept { cancelled.store(true, std::memory_order_release); }
    };
}  // namespace

TEST(ConnectionTrackerTest, HandleRaiiCountsInFlight) {
    boost::asio::io_context ioc;
    ConnectionTracker tracker;
    EXPECT_EQ(tracker.in_flight(), 0u);
    {
        auto c1 = std::make_shared<FakeConn>(ioc);
        auto c2 = std::make_shared<FakeConn>(ioc);
        auto h1 = tracker.register_connection(c1, c1->strand);
        auto h2 = tracker.register_connection(c2, c2->strand);
        EXPECT_EQ(tracker.in_flight(), 2u);
    }  // both Handles destroyed → deregister
    EXPECT_EQ(tracker.in_flight(), 0u);
}

TEST(ConnectionTrackerTest, DrainReturnsImmediatelyWhenEmpty) {
    boost::asio::io_context ioc;
    ConnectionTracker tracker;
    auto fut = boost::asio::co_spawn(
        ioc,
        tracker.drain_until(ioc.get_executor(), std::chrono::steady_clock::now() + 10s),
        boost::asio::use_future);
    ioc.run();
    fut.get();  // does not block for 10s; rethrows on error
    // TODO: assert immediacy — wrap ioc.run() with EXPECT_LT(elapsed, 1s); SUCCEED() alone passes even if drain slept the full deadline.
    SUCCEED();
}

TEST(ConnectionTrackerTest, ForceCancelsSurvivorsAtDeadline) {
    boost::asio::io_context ioc;
    ConnectionTracker tracker;

    auto c1 = std::make_shared<FakeConn>(ioc);
    auto c2 = std::make_shared<FakeConn>(ioc);
    auto h1 = std::make_shared<ConnectionTracker::Handle>(
        tracker.register_connection(c1, c1->strand));
    auto h2 = std::make_shared<ConnectionTracker::Handle>(
        tracker.register_connection(c2, c2->strand));

    // They never finish on their own → drain hits the deadline and force-cancels.
    auto fut = boost::asio::co_spawn(
        ioc,
        tracker.drain_until(ioc.get_executor(), std::chrono::steady_clock::now() + 100ms),
        boost::asio::use_future);
    ioc.run();
    fut.get();

    EXPECT_TRUE(c1->cancelled.load());
    EXPECT_TRUE(c2->cancelled.load());
}

TEST(ConnectionTrackerTest, DeadConnectionIsSkippedNotUseAfterFree) {
    boost::asio::io_context ioc;
    ConnectionTracker tracker;

    auto c1 = std::make_shared<FakeConn>(ioc);
    // Register but then drop the connection while the Handle still exists — the
    // weak_ptr in the tracker must lock() to null and skip (D2 / spike S1).
    auto h1 = std::make_shared<ConnectionTracker::Handle>(
        tracker.register_connection(c1, c1->strand));
    c1.reset();

    // TODO: add EXPECT_EQ(tracker.in_flight(), 1u) to document the invariant (the skip is currently proven only by ASan).
    auto fut = boost::asio::co_spawn(
        ioc,
        tracker.drain_until(ioc.get_executor(), std::chrono::steady_clock::now() + 50ms),
        boost::asio::use_future);
    ioc.run();
    fut.get();  // no crash, no UAF
    SUCCEED();
}
