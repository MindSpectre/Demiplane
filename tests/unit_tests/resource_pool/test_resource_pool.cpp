#include <demiplane/multithread>

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using namespace demiplane::multithread;

namespace {
    // Builds a Lease<T> by hand against a caller-owned word + EventCount, mirroring
    // exactly how ResourcePool::try_acquire will construct one.
    struct LeaseFixture {
        int                        resource{42};
        std::atomic<std::uint64_t> word{0};                 // bit 3 starts CLEAR (leased)
        EventCount                 waiters{};
        static constexpr std::uint64_t bit = std::uint64_t{1} << 3;

        Lease<int> make_lease() { return Lease<int>{&resource, &word, bit, &waiters}; }
    };
}  // namespace

TEST(LeaseTest, AccessorsReachTheResource) {
    LeaseFixture fx;
    Lease<int> lease = fx.make_lease();
    EXPECT_EQ(*lease, 42);
    EXPECT_EQ(lease.get(), &fx.resource);
    *lease = 7;
    EXPECT_EQ(fx.resource, 7);
}

TEST(LeaseTest, DestructorSetsBitAndNotifies) {
    LeaseFixture fx;
    const std::uint32_t epoch_before = fx.waiters.get_epoch();
    {
        Lease<int> lease = fx.make_lease();
        EXPECT_EQ(fx.word.load(), 0u);  // still leased
    }
    EXPECT_EQ(fx.word.load(), LeaseFixture::bit);          // bit set on release
    EXPECT_EQ(fx.waiters.get_epoch(), epoch_before + 1u);  // exactly one notify
}

TEST(LeaseTest, MoveTransfersOwnershipAndNullsSource) {
    LeaseFixture fx;
    const std::uint32_t epoch_before = fx.waiters.get_epoch();
    {
        Lease<int> src = fx.make_lease();
        Lease<int> dst = std::move(src);
        // src is moved-from: its destructor must release nothing.
    }
    // Released exactly once by dst: the bit is set AND the epoch advanced by
    // exactly 1. The bit alone cannot catch a stray release by the moved-from
    // src (fetch_or is idempotent) — the epoch delta is what proves "exactly once".
    EXPECT_EQ(fx.word.load(), LeaseFixture::bit);
    EXPECT_EQ(fx.waiters.get_epoch(), epoch_before + 1u);
}

TEST(LeaseTest, MoveAssignReleasesTheOverwrittenLease) {
    std::atomic<std::uint64_t> word_a{0};
    std::atomic<std::uint64_t> word_b{0};
    EventCount waiters;
    int res_a{1};
    int res_b{2};
    constexpr std::uint64_t bit = std::uint64_t{1} << 0;

    Lease<int> a{&res_a, &word_a, bit, &waiters};
    Lease<int> b{&res_b, &word_b, bit, &waiters};
    a = std::move(b);                       // a's old slot must be released here
    EXPECT_EQ(word_a.load(), bit);          // old slot of `a` released
    EXPECT_EQ(word_b.load(), 0u);           // `b`'s slot still held by `a`
    EXPECT_EQ(*a, 2);
}

TEST(LeaseTest, DefaultConstructedLeaseReleasesNothing) {
    Lease<int> empty;  // must compile and be a harmless no-op on destruction
    SUCCEED();
}

namespace {
    // A probe resource that counts factory calls and live instances. Must be
    // move-constructible: the pool moves the factory's return value into its slot
    // via std::construct_at. factory_calls is bumped only by the size_t constructor
    // (an actual factory production); the move constructor bumps only `live`.
    struct Probe {
        static inline std::atomic<int>          live{0};
        static inline std::atomic<int>          factory_calls{0};
        static inline std::vector<std::size_t>  indices{};  // not thread-safe; single-thread tests only

        std::size_t index;

        explicit Probe(std::size_t i) : index{i} {
            live.fetch_add(1, std::memory_order_relaxed);
            factory_calls.fetch_add(1, std::memory_order_relaxed);
            indices.push_back(i);
        }
        Probe(Probe&& other) noexcept : index{other.index} {
            live.fetch_add(1, std::memory_order_relaxed);
        }
        Probe(const Probe&)            = delete;
        Probe& operator=(const Probe&) = delete;
        Probe& operator=(Probe&&)      = delete;
        ~Probe() { live.fetch_sub(1, std::memory_order_relaxed); }

        static void reset() {
            live.store(0);
            factory_calls.store(0);
            indices.clear();
        }
    };
}  // namespace

TEST(ResourcePoolConstruction, FreeOnlyPoolInvokesFactoryPerSlot) {
    Probe::reset();
    {
        ResourcePool<Probe, 32> pool{5, [](std::size_t i) { return Probe{i}; }};
        EXPECT_EQ(pool.capacity(), 5u);
        EXPECT_EQ(pool.pinned_count(), 0u);
        EXPECT_EQ(pool.free_count(), 5u);
        EXPECT_EQ(Probe::live.load(), 5);           // 5 slots alive (factory temps already gone)
        EXPECT_EQ(Probe::factory_calls.load(), 5);  // factory produced exactly 5
        const std::vector<std::size_t> expected{0, 1, 2, 3, 4};
        EXPECT_EQ(Probe::indices, expected);
    }
    EXPECT_EQ(Probe::live.load(), 0);  // destructor destroyed every slot
}

TEST(ResourcePoolConstruction, PartitionedPoolCountsAndIndices) {
    Probe::reset();
    {
        ResourcePool<Probe, 32> pool{3, 7, [](std::size_t i) { return Probe{i}; }};
        EXPECT_EQ(pool.capacity(), 10u);
        EXPECT_EQ(pool.pinned_count(), 3u);
        EXPECT_EQ(pool.free_count(), 7u);
        EXPECT_EQ(Probe::live.load(), 10);
        EXPECT_EQ(Probe::factory_calls.load(), 10);
    }
    EXPECT_EQ(Probe::live.load(), 0);
}

TEST(ResourcePoolConstruction, NullaryFactoryIsAccepted) {
    int calls = 0;
    ResourcePool<int, 8> pool{4, [&calls] { ++calls; return 99; }};
    EXPECT_EQ(calls, 4);
    EXPECT_EQ(pool.free_count(), 4u);
}

TEST(ResourcePoolConstruction, ThrowsWhenPartitionsExceedMaxSize) {
    EXPECT_THROW((ResourcePool<int, 8>{5, 5, [](std::size_t) { return 0; }}),
                 std::invalid_argument);
}

TEST(ResourcePoolConstruction, FactoryThrowMidConstructionLeaksNothing) {
    Probe::reset();
    auto make = [](std::size_t i) {
        if (i == 4) {
            throw std::runtime_error{"factory failure on slot 4"};
        }
        return Probe{i};
    };
    EXPECT_THROW((ResourcePool<Probe, 16>{10, make}), std::runtime_error);
    EXPECT_EQ(Probe::live.load(), 0);              // the 4 built slots were destroyed on unwind
    EXPECT_EQ(Probe::factory_calls.load(), 4);     // factory ran for slots 0..3, threw on slot 4
}

TEST(ResourcePoolConstruction, PartitionsSummingToMaxSizeDoNotThrow) {
    // n_pinned + n_free == MaxSize is the inclusive boundary — must NOT throw
    // (the ctor guard is `> MaxSize`, strict).
    ResourcePool<int, 8> pool{3, 5, [](std::size_t i) { return static_cast<int>(i); }};
    EXPECT_EQ(pool.capacity(), 8u);
    EXPECT_EQ(pool.pinned_count(), 3u);
    EXPECT_EQ(pool.free_count(), 5u);
}

TEST(ResourcePoolConstruction, PurePinnedPoolHasEmptyFreeRegion) {
    // n_free == 0: a pinned-only pool. The free-bit-init loop must run zero times
    // and construction must still succeed and destroy every slot.
    Probe::reset();
    {
        ResourcePool<Probe, 16> pool{4, 0, [](std::size_t i) { return Probe{i}; }};
        EXPECT_EQ(pool.capacity(), 4u);
        EXPECT_EQ(pool.pinned_count(), 4u);
        EXPECT_EQ(pool.free_count(), 0u);
        EXPECT_EQ(Probe::live.load(), 4);
        EXPECT_EQ(Probe::factory_calls.load(), 4);
    }
    EXPECT_EQ(Probe::live.load(), 0);
}

TEST(ResourcePoolTryAcquire, HandsOutDistinctSlotsThenEmpty) {
    ResourcePool<int, 64> pool{4, [](std::size_t i) { return static_cast<int>(i) + 100; }};

    std::optional<Lease<int>> a = pool.try_acquire();
    std::optional<Lease<int>> b = pool.try_acquire();
    std::optional<Lease<int>> c = pool.try_acquire();
    std::optional<Lease<int>> d = pool.try_acquire();
    ASSERT_TRUE(a && b && c && d);

    // Four distinct resources handed out.
    std::array<int*, 4> ptrs{a->get(), b->get(), c->get(), d->get()};
    std::ranges::sort(ptrs);
    EXPECT_EQ(std::ranges::adjacent_find(ptrs), ptrs.end());

    EXPECT_FALSE(pool.try_acquire());  // pool exhausted
}

TEST(ResourcePoolTryAcquire, DroppingALeaseReturnsTheSlot) {
    ResourcePool<int, 64> pool{1, [](std::size_t) { return 5; }};

    {
        std::optional<Lease<int>> only = pool.try_acquire();
        ASSERT_TRUE(only);
        EXPECT_FALSE(pool.try_acquire());  // the single slot is taken
    }                                      // `only` released here
    EXPECT_TRUE(pool.try_acquire());       // slot is back in circulation
}

TEST(ResourcePoolTryAcquire, SpansMultipleBitsetWords) {
    // 130 free slots => 3 bitset words; every slot must be obtainable exactly once.
    ResourcePool<int, 130> pool{130, [](std::size_t i) { return static_cast<int>(i); }};

    std::vector<Lease<int>> held;
    held.reserve(130);
    for (std::size_t i = 0; i < 130; ++i) {
        std::optional<Lease<int>> lease = pool.try_acquire();
        ASSERT_TRUE(lease) << "failed to acquire slot " << i;
        held.push_back(std::move(*lease));
    }
    EXPECT_FALSE(pool.try_acquire());
}

TEST(ResourcePoolTryAcquire, EmptyFreeRegionAlwaysReturnsNullopt) {
    ResourcePool<int, 8> pool{2, 0, [](std::size_t) { return 1; }};  // pinned only
    EXPECT_FALSE(pool.try_acquire());
}

using namespace std::chrono_literals;

TEST(ResourcePoolAcquireFor, FastPathReturnsImmediately) {
    ResourcePool<int, 16> pool{4, [](std::size_t) { return 1; }};
    const auto t0 = std::chrono::steady_clock::now();
    std::optional<Lease<int>> lease = pool.acquire_for(1s);
    EXPECT_TRUE(lease);
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 100ms);  // never actually waited
}

TEST(ResourcePoolAcquireFor, TimesOutOnSaturatedPool) {
    ResourcePool<int, 8> pool{2, [](std::size_t) { return 1; }};
    std::optional<Lease<int>> a = pool.try_acquire();
    std::optional<Lease<int>> b = pool.try_acquire();
    ASSERT_TRUE(a && b);  // pool is now saturated

    const auto t0 = std::chrono::steady_clock::now();
    std::optional<Lease<int>> timed_out = pool.acquire_for(50ms);
    const auto elapsed = std::chrono::steady_clock::now() - t0;

    EXPECT_FALSE(timed_out);
    EXPECT_GE(elapsed, 40ms);
    EXPECT_LT(elapsed, 2s);
}

TEST(ResourcePoolAcquireFor, SubSpinBudgetTimeoutStillReturns) {
    ResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    std::optional<Lease<int>> a = pool.try_acquire();
    ASSERT_TRUE(a);
    // 100ns < SPIN_BUDGET (1us): degrades to spin-only, must still return nullopt fast.
    EXPECT_FALSE(pool.acquire_for(100ns));
}

TEST(ResourcePoolAcquireFor, WakesWhenAnotherThreadReleases) {
    ResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    std::optional<Lease<int>> holder = pool.try_acquire();
    ASSERT_TRUE(holder);

    std::atomic<bool> acquired{false};
    std::thread waiter{[&] {
        std::optional<Lease<int>> lease = pool.acquire_for(5s);
        acquired.store(lease.has_value(), std::memory_order_release);
    }};

    std::this_thread::sleep_for(100ms);  // let `waiter` reach the park phase
    holder.reset();                      // release the only slot
    waiter.join();

    EXPECT_TRUE(acquired.load(std::memory_order_acquire));
}

TEST(ResourcePoolPinned, CellsPublishTheirStorageSlots) {
    ResourcePool<int, 16> pool{3, 0, [](std::size_t i) { return static_cast<int>(i) + 10; }};
    ASSERT_EQ(pool.pinned_count(), 3u);

    std::atomic<int*>& cell0 = pool.pinned(0);
    std::atomic<int*>& cell2 = pool.pinned(2);
    ASSERT_NE(cell0.load(), nullptr);
    ASSERT_NE(cell2.load(), nullptr);
    EXPECT_EQ(*cell0.load(), 10);
    EXPECT_EQ(*cell2.load(), 12);
    EXPECT_NE(cell0.load(), cell2.load());
}

TEST(ResourcePoolPinned, AccessorReturnsAStableReference) {
    ResourcePool<int, 16> pool{2, 0, [](std::size_t i) { return static_cast<int>(i); }};
    EXPECT_EQ(&pool.pinned(1), &pool.pinned(1));  // same cell every call
}

TEST(ResourcePoolPinned, CooperativePointerSwapRepairCycle) {
    ResourcePool<int, 16> pool{1, 0, [](std::size_t) { return 7; }};
    std::atomic<int*>& cell = pool.pinned(0);

    // Repairer takes exclusive ownership; the owner would see nullptr meanwhile.
    int* p = cell.exchange(nullptr, std::memory_order_acq_rel);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(cell.load(std::memory_order_acquire), nullptr);

    *p = 99;  // "reconstruct in place"
    cell.store(p, std::memory_order_release);

    EXPECT_EQ(cell.load(std::memory_order_acquire), p);
    EXPECT_EQ(*cell.load(), 99);
}

TEST(ResourcePoolRepair, ClaimSucceedsOnFreeSlotAndBlocksAcquire) {
    // 70 free slots => 2 bitset words; claim a specific slot in the second word.
    ResourcePool<int, 70> pool{70, [](std::size_t i) { return static_cast<int>(i); }};

    EXPECT_TRUE(pool.try_claim_free_for_repair(65));
    EXPECT_FALSE(pool.try_claim_free_for_repair(65));  // already claimed (down)

    // Drain the other 69 slots; none of them can be slot 65.
    std::vector<Lease<int>> held;
    held.reserve(69);
    for (int i = 0; i < 69; ++i) {
        std::optional<Lease<int>> lease = pool.try_acquire();
        ASSERT_TRUE(lease);
        held.push_back(std::move(*lease));
    }
    EXPECT_FALSE(pool.try_acquire());  // slot 65 is under repair, not acquirable

    pool.mark_healthy_free(65);
    EXPECT_TRUE(pool.try_acquire());   // slot 65 back in circulation
}

TEST(ResourcePoolRepair, ClaimFailsOnALeasedSlot) {
    ResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    std::optional<Lease<int>> lease = pool.try_acquire();  // takes slot 0
    ASSERT_TRUE(lease);
    EXPECT_FALSE(pool.try_claim_free_for_repair(0));       // leaseholder has it

    lease.reset();
    EXPECT_TRUE(pool.try_claim_free_for_repair(0));        // now free -> claimable
}

TEST(ResourcePoolRepair, MarkHealthyFreeWakesAParkedWaiter) {
    ResourcePool<int, 8> pool{1, [](std::size_t) { return 1; }};
    ASSERT_TRUE(pool.try_claim_free_for_repair(0));  // slot 0 down; pool has nothing free

    std::atomic<bool> acquired{false};
    std::thread waiter{[&] {
        std::optional<Lease<int>> lease = pool.acquire_for(std::chrono::seconds{5});
        acquired.store(lease.has_value(), std::memory_order_release);
    }};

    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    pool.mark_healthy_free(0);
    waiter.join();

    EXPECT_TRUE(acquired.load(std::memory_order_acquire));
}

// The core safety property: a slot is never handed to two leaseholders at once.
TEST(ResourcePoolStress, NoDoubleAllocation) {
    constexpr std::size_t pool_size  = 32;
    constexpr int         threads    = 8;
    constexpr int         iterations = 5'000;

    ResourcePool<std::size_t, pool_size> pool{
        pool_size, [](std::size_t i) { return i; }};
    std::array<std::atomic<int>, pool_size> occupancy{};

    std::barrier sync{threads};
    std::atomic<bool> race_detected{false};

    std::vector<std::thread> workers;
    workers.reserve(threads);
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&] {
            sync.arrive_and_wait();
            for (int i = 0; i < iterations; ++i) {
                std::optional<Lease<std::size_t>> lease = pool.acquire_for(std::chrono::milliseconds{100});
                if (!lease) {
                    continue;  // legitimate timeout under contention
                }
                const std::size_t slot = *(*lease);  // factory stored the slot index
                if (occupancy[slot].fetch_add(1, std::memory_order_acq_rel) != 0) {
                    race_detected.store(true, std::memory_order_relaxed);
                }
                occupancy[slot].fetch_sub(1, std::memory_order_acq_rel);
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    EXPECT_FALSE(race_detected.load());
}

// After all leases are released, every free bit is set again.
TEST(ResourcePoolStress, BitsConservedAfterContention) {
    constexpr std::size_t pool_size = 24;
    constexpr int         threads   = 6;

    ResourcePool<int, pool_size> pool{pool_size, [](std::size_t) { return 0; }};
    std::barrier sync{threads};

    std::vector<std::thread> workers;
    workers.reserve(threads);
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&] {
            sync.arrive_and_wait();
            for (int i = 0; i < 5'000; ++i) {
                if (std::optional<Lease<int>> lease = pool.try_acquire()) {
                    // hold briefly, then release at end of scope
                }
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    // All slots free again => the whole pool is acquirable.
    std::vector<Lease<int>> held;
    held.reserve(pool_size);
    for (std::size_t i = 0; i < pool_size; ++i) {
        std::optional<Lease<int>> lease = pool.try_acquire();
        ASSERT_TRUE(lease) << "slot " << i << " was lost";
        held.push_back(std::move(*lease));
    }
    EXPECT_FALSE(pool.try_acquire());
}

// acquire_for under heavy oversubscription: no deadlock, no torn slot, clean nullopt-or-lease.
TEST(ResourcePoolStress, AcquireForUnderOversubscription) {
    constexpr std::size_t pool_size = 4;
    constexpr int         threads   = 16;

    ResourcePool<std::size_t, pool_size> pool{
        pool_size, [](std::size_t i) { return i; }};
    std::array<std::atomic<int>, pool_size> occupancy{};
    std::barrier sync{threads};
    std::atomic<bool> race_detected{false};

    std::vector<std::thread> workers;
    workers.reserve(threads);
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&] {
            sync.arrive_and_wait();
            for (int i = 0; i < 1'000; ++i) {
                std::optional<Lease<std::size_t>> lease = pool.acquire_for(std::chrono::milliseconds{1});
                if (!lease) {
                    continue;
                }
                const std::size_t slot = *(*lease);
                if (occupancy[slot].fetch_add(1, std::memory_order_acq_rel) != 0) {
                    race_detected.store(true, std::memory_order_relaxed);
                }
                std::this_thread::yield();
                occupancy[slot].fetch_sub(1, std::memory_order_acq_rel);
            }
        });
    }
    for (auto& w : workers) {
        w.join();  // must terminate — no deadlock
    }
    EXPECT_FALSE(race_detected.load());
}

// A repair thread cycling one slot must not break the no-double-allocation invariant.
TEST(ResourcePoolStress, RepairRacesAcquirers) {
    constexpr std::size_t pool_size = 16;
    constexpr int         acquirers = 6;

    ResourcePool<std::size_t, pool_size> pool{
        pool_size, [](std::size_t i) { return i; }};
    std::array<std::atomic<int>, pool_size> occupancy{};
    std::atomic<bool> stop{false};
    std::atomic<bool> race_detected{false};

    std::vector<std::thread> workers;
    workers.reserve(acquirers + 1);
    for (int t = 0; t < acquirers; ++t) {
        workers.emplace_back([&] {
            while (!stop.load(std::memory_order_acquire)) {
                std::optional<Lease<std::size_t>> lease = pool.acquire_for(std::chrono::milliseconds{5});
                if (!lease) {
                    continue;
                }
                const std::size_t slot = *(*lease);
                if (occupancy[slot].fetch_add(1, std::memory_order_acq_rel) != 0) {
                    race_detected.store(true, std::memory_order_relaxed);
                }
                occupancy[slot].fetch_sub(1, std::memory_order_acq_rel);
            }
        });
    }
    // Repair thread: continuously claims slot 7, "repairs" it, returns it.
    workers.emplace_back([&] {
        for (int i = 0; i < 20'000; ++i) {
            while (!pool.try_claim_free_for_repair(7)) {
                std::this_thread::yield();
            }
            pool.mark_healthy_free(7);
        }
        stop.store(true, std::memory_order_release);
    });
    for (auto& w : workers) {
        w.join();
    }
    EXPECT_FALSE(race_detected.load());
}

// A pinned owner loops load(acquire) while a repairer swaps the cell's pointer; the
// owner must only ever observe the real slot pointer or nullptr, never a torn value.
// (Concurrent reconstruction of *p is the caller's cooperative responsibility and is
// covered single-threaded in Task 8 — this test exercises the atomic cell mechanic.)
TEST(ResourcePoolStress, PinnedCellSwapIsAtomic) {
    ResourcePool<std::size_t, 8> pool{1, 0, [](std::size_t) { return std::size_t{777}; }};
    std::atomic<std::size_t*>& cell = pool.pinned(0);
    std::size_t* const real = cell.load(std::memory_order_acquire);
    ASSERT_NE(real, nullptr);

    std::atomic<bool> stop{false};
    std::atomic<bool> torn{false};

    std::thread owner{[&] {
        while (!stop.load(std::memory_order_acquire)) {
            std::size_t* p = cell.load(std::memory_order_acquire);
            if (p != nullptr && p != real) {
                torn.store(true, std::memory_order_relaxed);
            }
        }
    }};

    std::thread repairer{[&] {
        for (int i = 0; i < 100'000; ++i) {
            std::size_t* p = cell.exchange(nullptr, std::memory_order_acq_rel);
            cell.store(p, std::memory_order_release);
        }
        stop.store(true, std::memory_order_release);
    }};

    owner.join();
    repairer.join();
    EXPECT_FALSE(torn.load());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
