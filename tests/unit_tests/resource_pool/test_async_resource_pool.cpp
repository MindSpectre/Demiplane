#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <demiplane/multithread>
#include <optional>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <variant>
#include <vector>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/this_coro.hpp>
#include <gtest/gtest.h>

// Reaching this TU proves async_resource_pool.hpp is included by the umbrella
// header and compiles cleanly alongside resource_pool.hpp (ODR check).
TEST(AsyncResourcePoolSmoke, HeaderIncludesCleanly) {
    SUCCEED();
}

// ===========================================================================
// detail::WaiterList — frame-local intrusive FIFO registry
// ===========================================================================

using demiplane::multithread::detail::WaiterList;
using demiplane::multithread::detail::WaiterNode;
using demiplane::multithread::detail::WaitOutcome;
using demiplane::multithread::detail::WaitState;

namespace {
    // Installs a recording completion handler into `node`, as initiate_park will:
    // captures the outcome the node is completed with. Runs on `ioc`.
    void arm(WaiterList& list, WaiterNode& node, boost::asio::io_context& ioc, std::optional<WaitOutcome>& sink) {
        node.state.store(WaitState::parked, std::memory_order_relaxed);
        node.exec    = ioc.get_executor();
        node.handler = [&sink](WaitOutcome o) { sink = o; };
        list.link(node);
    }
}  // namespace

TEST(WaiterListTest, WakeOneCompletesExactlyOneInFifoOrder) {
    boost::asio::io_context ioc;
    WaiterList list;
    WaiterNode n0, n1, n2;
    std::optional<WaitOutcome> r0, r1, r2;
    arm(list, n0, ioc, r0);
    arm(list, n1, ioc, r1);
    arm(list, n2, ioc, r2);

    list.wake_one();  // must wake n0 (FIFO head)
    ioc.run();
    EXPECT_EQ(r0, WaitOutcome::woken);
    EXPECT_FALSE(r1.has_value());
    EXPECT_FALSE(r2.has_value());

    ioc.restart();
    list.wake_one();  // now n1
    ioc.run();
    EXPECT_EQ(r1, WaitOutcome::woken);
    EXPECT_FALSE(r2.has_value());
}

TEST(WaiterListTest, WakeOneOnEmptyListIsHarmless) {
    WaiterList list;
    list.wake_one();  // no nodes — must not crash
    SUCCEED();
}

TEST(WaiterListTest, CompleteArbitratesThreeArmsExactlyOnce) {
    boost::asio::io_context ioc;
    WaiterList list;
    WaiterNode node;
    std::optional<WaitOutcome> r;
    arm(list, node, ioc, r);

    // First arm to win the CAS drives the completion; the rest no-op.
    EXPECT_TRUE(list.complete(node, WaitState::cancelled));
    EXPECT_FALSE(list.complete(node, WaitState::notified));  // lost
    list.wake_one();                                         // also a no-op for this node
    ioc.run();
    EXPECT_EQ(r, WaitOutcome::cancelled);
}

TEST(WaiterListTest, DrainAllCompletesEveryParkedWaiterWithShutDown) {
    boost::asio::io_context ioc;
    WaiterList list;
    WaiterNode n0, n1;
    std::optional<WaitOutcome> r0, r1;
    arm(list, n0, ioc, r0);
    arm(list, n1, ioc, r1);

    list.drain_all(WaitState::shut_down);
    ioc.run();
    EXPECT_EQ(r0, WaitOutcome::shut_down);
    EXPECT_EQ(r1, WaitOutcome::shut_down);
    EXPECT_TRUE(list.empty());
}

// ===========================================================================
// AsyncLease<T> — RAII handle that wakes the WaiterList on release
// ===========================================================================

using demiplane::multithread::AsyncLease;

namespace {
    struct AsyncLeaseFixture {
        int resource{42};
        std::atomic<std::uint64_t> word{0};  // bit 3 starts CLEAR (leased)
        WaiterList waiters{};
        static constexpr std::uint64_t bit = std::uint64_t{1} << 3;

        AsyncLease<int> make_lease() {
            return AsyncLease<int>{&resource, &word, bit, &waiters};
        }
    };
}  // namespace

TEST(AsyncLeaseTest, AccessorsReachTheResource) {
    AsyncLeaseFixture fx;
    AsyncLease<int> lease = fx.make_lease();
    EXPECT_EQ(*lease, 42);
    EXPECT_EQ(lease.get(), &fx.resource);
    *lease = 7;
    EXPECT_EQ(fx.resource, 7);
}

TEST(AsyncLeaseTest, DestructorSetsBitAndWakesOneWaiter) {
    boost::asio::io_context ioc;
    AsyncLeaseFixture fx;
    WaiterNode node;
    std::optional<WaitOutcome> woke;
    node.state.store(WaitState::parked, std::memory_order_relaxed);
    node.exec    = ioc.get_executor();
    node.handler = [&woke](WaitOutcome o) { woke = o; };
    fx.waiters.link(node);

    {
        AsyncLease<int> lease = fx.make_lease();
        EXPECT_EQ(fx.word.load(), 0u);  // still leased
    }  // release here: bit set + wake_one
    EXPECT_EQ(fx.word.load(), AsyncLeaseFixture::bit);

    ioc.run();
    EXPECT_EQ(woke, WaitOutcome::woken);  // the parked node was woken exactly once
}

TEST(AsyncLeaseTest, MovedFromLeaseReleasesNothing) {
    AsyncLeaseFixture fx;
    {
        AsyncLease<int> src = fx.make_lease();
        AsyncLease<int> dst = std::move(src);  // src now releases nothing
    }
    EXPECT_EQ(fx.word.load(), AsyncLeaseFixture::bit);  // released exactly once by dst
}

TEST(AsyncLeaseTest, DefaultConstructedReleasesNothing) {
    AsyncLease<int> empty;
    SUCCEED();
}

// ===========================================================================
// AsyncResourcePool — construction, destruction, accessors
// ===========================================================================

using demiplane::multithread::AsyncResourcePool;

namespace {
    struct Probe {
        static inline std::atomic<int> live{0};
        static inline std::atomic<int> factory_calls{0};
        static inline std::vector<std::size_t> indices{};

        std::size_t index;
        explicit Probe(std::size_t i)
            : index{i} {
            live.fetch_add(1, std::memory_order_relaxed);
            factory_calls.fetch_add(1, std::memory_order_relaxed);
            indices.push_back(i);
        }
        Probe(Probe&& o) noexcept
            : index{o.index} {
            live.fetch_add(1, std::memory_order_relaxed);
        }
        Probe(const Probe&)            = delete;
        Probe& operator=(const Probe&) = delete;
        Probe& operator=(Probe&&)      = delete;
        ~Probe() {
            live.fetch_sub(1, std::memory_order_relaxed);
        }
        static void reset() {
            live.store(0);
            factory_calls.store(0);
            indices.clear();
        }
    };
}  // namespace

TEST(AsyncResourcePoolConstruction, FreeOnlyPoolInvokesFactoryPerSlot) {
    Probe::reset();
    {
        AsyncResourcePool<Probe, 32> pool{5, [](std::size_t i) { return Probe{i}; }};
        EXPECT_EQ(pool.capacity(), 5u);
        EXPECT_EQ(pool.pinned_count(), 0u);
        EXPECT_EQ(pool.free_count(), 5u);
        EXPECT_EQ(Probe::live.load(), 5);
        EXPECT_EQ(Probe::factory_calls.load(), 5);
        const std::vector<std::size_t> expected{0, 1, 2, 3, 4};
        EXPECT_EQ(Probe::indices, expected);
    }
    EXPECT_EQ(Probe::live.load(), 0);
}

TEST(AsyncResourcePoolConstruction, ThrowsWhenPartitionsExceedMaxSize) {
    EXPECT_THROW((AsyncResourcePool<int, 8>{5, 5, [](std::size_t) { return 0; }}), std::invalid_argument);
}

TEST(AsyncResourcePoolConstruction, FactoryThrowMidConstructionLeaksNothing) {
    Probe::reset();
    auto make = [](std::size_t i) {
        if (i == 4) {
            throw std::runtime_error{"boom"};
        }
        return Probe{i};
    };
    EXPECT_THROW((AsyncResourcePool<Probe, 16>{10, make}), std::runtime_error);
    EXPECT_EQ(Probe::live.load(), 0);
    EXPECT_EQ(Probe::factory_calls.load(), 4);
}

TEST(AsyncResourcePoolConstruction, NullaryFactoryIsAccepted) {
    int calls = 0;
    AsyncResourcePool<int, 8> pool{4, [&calls] {
                                       ++calls;
                                       return 99;
                                   }};
    EXPECT_EQ(calls, 4);
    EXPECT_EQ(pool.free_count(), 4u);
}

// ===========================================================================
// AsyncResourcePool — try_acquire, pinned, repair
// ===========================================================================

TEST(AsyncResourcePoolTryAcquire, HandsOutDistinctSlotsThenEmpty) {
    AsyncResourcePool<int, 64> pool{4, [](std::size_t i) { return static_cast<int>(i) + 100; }};
    auto a = pool.try_acquire();
    auto b = pool.try_acquire();
    auto c = pool.try_acquire();
    auto d = pool.try_acquire();
    ASSERT_TRUE(a && b && c && d);
    std::array<int*, 4> ptrs{a->get(), b->get(), c->get(), d->get()};
    std::ranges::sort(ptrs);
    EXPECT_EQ(std::ranges::adjacent_find(ptrs), ptrs.end());
    EXPECT_FALSE(pool.try_acquire());
}

TEST(AsyncResourcePoolTryAcquire, DroppingALeaseReturnsTheSlot) {
    AsyncResourcePool<int, 64> pool{1, [](std::size_t) { return 5; }};
    {
        auto only = pool.try_acquire();
        ASSERT_TRUE(only);
        EXPECT_FALSE(pool.try_acquire());
    }
    EXPECT_TRUE(pool.try_acquire());
}

TEST(AsyncResourcePoolTryAcquire, SpansMultipleBitsetWords) {
    AsyncResourcePool<int, 130> pool{130, [](std::size_t i) { return static_cast<int>(i); }};
    std::vector<AsyncLease<int>> held;
    held.reserve(130);
    for (std::size_t i = 0; i < 130; ++i) {
        auto lease = pool.try_acquire();
        ASSERT_TRUE(lease) << "failed at slot " << i;
        held.push_back(std::move(*lease));
    }
    EXPECT_FALSE(pool.try_acquire());
}

TEST(AsyncResourcePoolPinned, CellsPublishStorageSlots) {
    AsyncResourcePool<int, 16> pool{3, 0, [](std::size_t i) { return static_cast<int>(i) + 10; }};
    ASSERT_EQ(pool.pinned_count(), 3u);
    EXPECT_EQ(*pool.pinned(0).load(), 10);
    EXPECT_EQ(*pool.pinned(2).load(), 12);
    EXPECT_NE(pool.pinned(0).load(), pool.pinned(2).load());
}

TEST(AsyncResourcePoolRepair, ClaimSucceedsOnFreeSlotThenMarkHealthyRestores) {
    AsyncResourcePool<int, 70> pool{70, [](std::size_t i) { return static_cast<int>(i); }};
    EXPECT_TRUE(pool.try_claim_free_for_repair(65));
    EXPECT_FALSE(pool.try_claim_free_for_repair(65));
    std::vector<AsyncLease<int>> held;
    for (int i = 0; i < 69; ++i) {
        auto lease = pool.try_acquire();
        ASSERT_TRUE(lease);
        held.push_back(std::move(*lease));
    }
    EXPECT_FALSE(pool.try_acquire());  // slot 65 under repair
    pool.mark_healthy_free(65);
    EXPECT_TRUE(pool.try_acquire());
}

TEST(AsyncResourcePoolRepair, ClaimFailsOnLeasedSlot) {
    AsyncResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    auto lease = pool.try_acquire();
    ASSERT_TRUE(lease);
    EXPECT_FALSE(pool.try_claim_free_for_repair(0));
    lease.reset();
    EXPECT_TRUE(pool.try_claim_free_for_repair(0));
}

// ===========================================================================
// AsyncResourcePool — async_acquire_for fast path + spin
// ===========================================================================

using namespace std::chrono_literals;

TEST(AsyncResourcePoolAcquireFor, FastPathReturnsLeaseWithoutSuspending) {
    boost::asio::io_context ioc;
    AsyncResourcePool<int, 16> pool{4, [](std::size_t) { return 1; }};
    std::optional<bool> got;

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto lease = co_await pool.async_acquire_for(ioc.get_executor(), 1s);
            got        = lease.has_value();
            co_return;
        },
        boost::asio::detached);

    ioc.run();
    ASSERT_TRUE(got.has_value());
    EXPECT_TRUE(*got);
}

TEST(AsyncResourcePoolAcquireFor, SubSpinBudgetTimeoutReturnsNullopt) {
    boost::asio::io_context ioc;
    AsyncResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    auto held = pool.try_acquire();  // saturate
    ASSERT_TRUE(held);
    std::optional<bool> got;

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto lease = co_await pool.async_acquire_for(ioc.get_executor(), 100ns);  // < 500ns spin
            got        = lease.has_value();
            co_return;
        },
        boost::asio::detached);

    ioc.run();
    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(*got);  // degrades to spin-only, returns nullopt
}

// ===========================================================================
// AsyncResourcePool — parking (bounded + unbounded), wake-on-release, timeout
// ===========================================================================

// Insurance: confirm the operator|| result shape the park loop relies on. If this
// fails, adjust the variant indexing in async_acquire_for to match the real type.
namespace {
    using park_t  = boost::asio::awaitable<WaitOutcome>;
    using timer_t = boost::asio::awaitable<std::tuple<boost::system::error_code>>;
    using namespace boost::asio::experimental::awaitable_operators;
    using or_result_t = decltype(std::declval<park_t>() || std::declval<timer_t>());
    static_assert(
        std::is_same_v<or_result_t,
                       boost::asio::awaitable<std::variant<WaitOutcome, std::tuple<boost::system::error_code>>>>,
        "Adjust the variant indexing in async_acquire_for to match operator|| result");
}  // namespace

TEST(AsyncResourcePoolPark, BoundedAcquireWokenByRelease) {
    boost::asio::io_context ioc;
    AsyncResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    auto held = pool.try_acquire();  // saturate
    ASSERT_TRUE(held);
    std::optional<bool> got;

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto lease = co_await pool.async_acquire_for(ioc.get_executor(), 5s);
            got        = lease.has_value();
            co_return;
        },
        boost::asio::detached);

    std::thread runner{[&] { ioc.run(); }};
    std::this_thread::sleep_for(100ms);  // let the coroutine reach the park
    held.reset();                        // release => wake_one
    runner.join();

    ASSERT_TRUE(got.has_value());
    EXPECT_TRUE(*got);
}

TEST(AsyncResourcePoolPark, BoundedAcquireTimesOut) {
    boost::asio::io_context ioc;
    AsyncResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    auto held = pool.try_acquire();
    ASSERT_TRUE(held);
    std::optional<bool> got;

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            const auto t0 = std::chrono::steady_clock::now();
            auto lease    = co_await pool.async_acquire_for(ioc.get_executor(), 200ms);
            EXPECT_GE(std::chrono::steady_clock::now() - t0, 150ms);
            got = lease.has_value();
            co_return;
        },
        boost::asio::detached);

    ioc.run();
    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(*got);  // timed out
}

TEST(AsyncResourcePoolPark, UnboundedAcquireWokenByRelease) {
    boost::asio::io_context ioc;
    AsyncResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    auto held = pool.try_acquire();
    ASSERT_TRUE(held);
    std::optional<bool> got;

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto lease = co_await pool.async_acquire(ioc.get_executor());
            got        = lease.has_value();
            co_return;
        },
        boost::asio::detached);

    std::thread runner{[&] { ioc.run(); }};
    std::this_thread::sleep_for(100ms);
    held.reset();
    runner.join();
    ASSERT_TRUE(got.has_value());
    EXPECT_TRUE(*got);
}

// ===========================================================================
// AsyncResourcePool — cancellation
// ===========================================================================

TEST(AsyncResourcePoolCancel, ParkedUnboundedAcquireCancelsToNullopt) {
    boost::asio::io_context ioc;
    AsyncResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    auto held = pool.try_acquire();  // saturate so the acquire parks
    ASSERT_TRUE(held);

    boost::asio::cancellation_signal sig;
    std::optional<bool> got;

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto lease = co_await pool.async_acquire(ioc.get_executor());
            got        = lease.has_value();
            co_return;
        },
        boost::asio::bind_cancellation_slot(sig.slot(), boost::asio::detached));

    ioc.poll();                                     // run until the coroutine parks
    sig.emit(boost::asio::cancellation_type::all);  // cancel it
    ioc.run();

    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(*got);                            // cancellation resolves to nullopt
    EXPECT_FALSE(pool.try_acquire().has_value());  // `held` still owns the only slot
}

// ===========================================================================
// AsyncResourcePool — shutdown
// ===========================================================================

TEST(AsyncResourcePoolShutdown, DrainsParkedWaitersToNullopt) {
    boost::asio::io_context ioc;
    AsyncResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    auto held = pool.try_acquire();
    ASSERT_TRUE(held);
    std::optional<bool> got;

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            auto lease = co_await pool.async_acquire(ioc.get_executor());
            got        = lease.has_value();
            co_return;
        },
        boost::asio::detached);

    std::thread runner{[&] { ioc.run(); }};
    std::this_thread::sleep_for(100ms);  // let it park
    pool.shutdown();                     // drain
    runner.join();

    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(*got);
}

TEST(AsyncResourcePoolShutdown, NewParkingAcquireAfterShutdownReturnsNullopt) {
    boost::asio::io_context ioc;
    AsyncResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    pool.shutdown();
    std::optional<bool> got;

    boost::asio::co_spawn(
        ioc,
        [&]() -> boost::asio::awaitable<void> {
            // A slot is free, but shutdown makes a *parking* acquire bail. Saturate
            // first to force the park branch rather than the fast path.
            auto saturate = pool.try_acquire();
            auto lease    = co_await pool.async_acquire(ioc.get_executor());
            got           = lease.has_value();
            co_return;
        },
        boost::asio::detached);

    ioc.run();
    ASSERT_TRUE(got.has_value());
    EXPECT_FALSE(*got);
}

// ===========================================================================
// AsyncResourcePool — concurrency stress (many coroutines, multi-threaded ioc)
// ===========================================================================

// Core safety property: a slot is never handed to two coroutines at once.
TEST(AsyncResourcePoolStress, NoDoubleAllocation) {
    constexpr std::size_t pool_size = 8;
    constexpr int coros             = 32;
    constexpr int iterations        = 5000;
    constexpr int n_threads         = 4;

    AsyncResourcePool<std::size_t, pool_size> pool{pool_size, [](std::size_t i) { return i; }};
    std::array<std::atomic<int>, pool_size> occupancy{};
    std::atomic<bool> race_detected{false};
    std::atomic<int> done{0};

    boost::asio::io_context ioc;
    auto guard = boost::asio::make_work_guard(ioc);

    for (int c = 0; c < coros; ++c) {
        boost::asio::co_spawn(
            ioc,
            [&]() -> boost::asio::awaitable<void> {
                for (int i = 0; i < iterations; ++i) {
                    auto lease = co_await pool.async_acquire_for(co_await boost::asio::this_coro::executor, 50ms);
                    if (!lease) {
                        continue;
                    }
                    const std::size_t slot = **lease;
                    if (occupancy[slot].fetch_add(1, std::memory_order_acq_rel) != 0) {
                        race_detected.store(true, std::memory_order_relaxed);
                    }
                    occupancy[slot].fetch_sub(1, std::memory_order_acq_rel);
                }
                if (done.fetch_add(1, std::memory_order_acq_rel) + 1 == coros) {
                    guard.reset();  // let ioc.run() return once all coroutines finish
                }
                co_return;
            },
            boost::asio::detached);
    }

    std::vector<std::thread> runners;
    runners.reserve(n_threads);
    for (int t = 0; t < n_threads; ++t) {
        runners.emplace_back([&] { ioc.run(); });
    }
    for (auto& r : runners) {
        r.join();
    }
    EXPECT_FALSE(race_detected.load());
}

// After every coroutine finishes, all free bits are set again.
TEST(AsyncResourcePoolStress, BitsConservedAfterContention) {
    constexpr std::size_t pool_size = 6;
    constexpr int coros             = 24;
    constexpr int n_threads         = 4;

    AsyncResourcePool<int, pool_size> pool{pool_size, [](std::size_t) { return 0; }};
    std::atomic<int> done{0};
    boost::asio::io_context ioc;
    auto guard = boost::asio::make_work_guard(ioc);

    for (int c = 0; c < coros; ++c) {
        boost::asio::co_spawn(
            ioc,
            [&]() -> boost::asio::awaitable<void> {
                for (int i = 0; i < 300; ++i) {
                    if (auto lease = co_await pool.async_acquire_for(co_await boost::asio::this_coro::executor, 20ms)) {
                        // hold briefly; released at scope end
                    }
                }
                if (done.fetch_add(1, std::memory_order_acq_rel) + 1 == coros) {
                    guard.reset();
                }
                co_return;
            },
            boost::asio::detached);
    }
    std::vector<std::thread> runners;
    runners.reserve(n_threads);
    for (int t = 0; t < n_threads; ++t) {
        runners.emplace_back([&] { ioc.run(); });
    }
    for (auto& r : runners) {
        r.join();
    }

    std::vector<AsyncLease<int>> held;
    held.reserve(pool_size);
    for (std::size_t i = 0; i < pool_size; ++i) {
        auto lease = pool.try_acquire();
        ASSERT_TRUE(lease) << "slot " << i << " was lost";
        held.push_back(std::move(*lease));
    }
    EXPECT_FALSE(pool.try_acquire());
}

// Hammer the timeout -> cancel path under full saturation: every acquire parks,
// times out via the || timer (which cancels async_park), runs complete(cancelled),
// and destroys its frame-local node. Stresses the node/cancellation-state lifetime
// and the cancel-vs-complete race across threads. ASan guards the frame-node
// unlink-before-destroy contract here.
TEST(AsyncResourcePoolStress, TimeoutCancelChurnUnderSaturation) {
    constexpr std::size_t pool_size = 4;
    constexpr int coros             = 24;
    constexpr int iterations        = 200;
    constexpr int n_threads         = 4;

    AsyncResourcePool<int, pool_size> pool{pool_size, [](std::size_t) { return 0; }};

    // Hold every slot for the whole test => every async_acquire_for must time out.
    std::vector<AsyncLease<int>> held;
    held.reserve(pool_size);
    for (std::size_t i = 0; i < pool_size; ++i) {
        auto l = pool.try_acquire();
        ASSERT_TRUE(l);
        held.push_back(std::move(*l));
    }

    std::atomic<int> timeouts{0};
    std::atomic<int> done{0};
    boost::asio::io_context ioc;
    auto guard = boost::asio::make_work_guard(ioc);

    for (int c = 0; c < coros; ++c) {
        boost::asio::co_spawn(
            ioc,
            [&]() -> boost::asio::awaitable<void> {
                for (int i = 0; i < iterations; ++i) {
                    auto lease = co_await pool.async_acquire_for(co_await boost::asio::this_coro::executor, 1ms);
                    if (!lease) {
                        timeouts.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                if (done.fetch_add(1, std::memory_order_acq_rel) + 1 == coros) {
                    guard.reset();
                }
                co_return;
            },
            boost::asio::detached);
    }
    std::vector<std::thread> runners;
    runners.reserve(n_threads);
    for (int t = 0; t < n_threads; ++t) {
        runners.emplace_back([&] { ioc.run(); });
    }
    for (auto& r : runners) {
        r.join();
    }
    EXPECT_EQ(timeouts.load(), coros * iterations);  // pool fully held => all timed out
    held.clear();
}

// Reproduces the HeavyBurst benchmark crash: slots held ACROSS an async wait
// (co_await timer) so the pool saturates, with a bounded acquire whose timeout is
// SHORTER than the hold — so acquires constantly park and time out, churning the
// ||-based acquire_for timeout path under a small, oversubscribed pool. Intended to
// fault under ASan/TSan if the bounded park/timeout path has a lifetime/race bug.
TEST(AsyncResourcePoolStress, HeldAcrossAsyncWorkBoundedTimeoutChurn) {
    constexpr std::size_t pool_size = 8;
    constexpr int coros             = 64;  // >> pool: heavy oversubscription
    constexpr int iterations        = 400;
    constexpr int n_threads         = 4;

    AsyncResourcePool<int, pool_size> pool{pool_size, [](std::size_t) { return 0; }};
    std::atomic<int> done{0};
    boost::asio::io_context ioc;
    auto guard = boost::asio::make_work_guard(ioc);

    for (int c = 0; c < coros; ++c) {
        boost::asio::co_spawn(
            boost::asio::make_strand(ioc),  // each coroutine on its own strand
            [&]() -> boost::asio::awaitable<void> {
                const auto exec = co_await boost::asio::this_coro::executor;
                for (int i = 0; i < iterations; ++i) {
                    auto lease = co_await pool.async_acquire_for(exec, 40us);  // < hold below
                    if (!lease) {
                        continue;  // timed out under contention — re-try
                    }
                    boost::asio::steady_timer t{exec};
                    t.expires_after(80us);  // hold the slot across an async wait
                    co_await t.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));
                    // lease released here
                }
                if (done.fetch_add(1, std::memory_order_acq_rel) + 1 == coros) {
                    guard.reset();
                }
                co_return;
            },
            boost::asio::detached);
    }

    std::vector<std::thread> runners;
    runners.reserve(n_threads);
    for (int t = 0; t < n_threads; ++t) {
        runners.emplace_back([&] { ioc.run(); });
    }
    for (auto& r : runners) {
        r.join();
    }
    SUCCEED();  // reaching here without a crash/sanitizer fault is the test
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
