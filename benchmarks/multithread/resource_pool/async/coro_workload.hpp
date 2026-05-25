#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <demiplane/chrono>
#include <demiplane/multithread>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "common/bench_latency.hpp"
#include "common/bench_scenarios.hpp"

namespace bench::pool {
    using TscClock = demiplane::chrono::TscClock;

    /// Free-region scenarios that map to a continuous coroutine workload. The two
    /// AsioPost* scenarios from the sync suite are sync-on-asio specific and have no
    /// async-pool analog (every async acquire already suspends), so they are omitted.
    inline constexpr std::array ASYNC_FREE_REGION_SCENARIOS = {
        ScenarioKind::Steady,
        ScenarioKind::Burst,
        ScenarioKind::TimeoutPressure,
        ScenarioKind::HeavyBurst,
    };

    /// Async analog of run_workload, for AsyncResourcePool.
    ///
    /// `state.range(0)` = number of concurrent coroutine acquirers (W). They are
    /// co_spawn'd onto a FIXED pool of CORE_COUNT io_context threads (so at high W the
    /// pool multiplexes many waiters onto few threads — the async pool's reason to
    /// exist). Each coroutine loops: rdtsc -> co_await async_acquire_for(timeout) ->
    /// rdtsc -> record latency -> work_for -> release (lease dtor). Latency is measured
    /// around the await (including any suspension), mirroring the sync runner's rdtsc
    /// pair around acquire. Each Google-Benchmark iteration is a 5 ms wall-clock window
    /// while the coroutines run continuously (same shape as run_asio_workload).
    template <typename Pool>
    void run_coro_workload(benchmark::State& state,
                           const Scenario& sc,
                           Pool& pool,
                           const std::chrono::nanoseconds timeout,
                           const bool pin) {
        using namespace std::chrono_literals;
        const auto n_coros = static_cast<std::size_t>(state.range(0));

        std::vector<LatencyCollector> collectors(n_coros);
        for (auto& c : collectors) {
            c.reserve(1u << 16);
        }

        std::atomic<std::int64_t> completed{0};
        std::atomic<std::int64_t> skipped{0};
        std::atomic<bool> stop{false};
        std::atomic<std::size_t> live{n_coros};

        // Sharded io pool: CORE_COUNT single-threaded io_contexts. Declared AFTER
        // collectors so it destructs FIRST (joins its threads) while the collectors and
        // pool are still alive.
        demiplane::multithread::ShardedAsioBackend backend{
            static_cast<std::size_t>(CORE_COUNT), pin, static_cast<std::size_t>(CORE_COUNT)};

        const bool is_burst   = sc.kind == ScenarioKind::Burst || sc.kind == ScenarioKind::HeavyBurst;
        // HeavyBurst holds the slot across an async wait (≈ a real I/O round-trip), so
        // many more than CORE_COUNT coroutines hold at once and the pool saturates/parks.
        // The other scenarios hold across a synchronous spin (fast-path dominated).
        const bool async_hold = sc.kind == ScenarioKind::HeavyBurst;

        for (std::size_t i = 0; i < n_coros; ++i) {
            // Pin each coroutine to one shard's single-threaded io_context. No strand
            // needed: a single-threaded context never runs two of this coroutine's
            // completions concurrently, which is what async_acquire_for's timeout
            // composition requires.
            boost::asio::co_spawn(
                backend.executor(i),
                [&, i]() -> boost::asio::awaitable<void> {
                    const auto exec       = co_await boost::asio::this_coro::executor;
                    LatencyCollector& col = collectors[i];
                    boost::asio::steady_timer idle_timer{exec};
                    boost::asio::steady_timer hold_timer{exec};

                    while (!stop.load(std::memory_order_acquire)) {
                        const int batch = is_burst ? sc.burst_size : 1;
                        for (int b = 0; b < batch && !stop.load(std::memory_order_acquire); ++b) {
                            const std::uint64_t t0 = TscClock::now();
                            auto lease             = co_await pool.async_acquire_for(exec, timeout);
                            const std::uint64_t t1 = TscClock::now();
                            if (lease.has_value()) {
                                col.record(TscClock::to_duration(t1 - t0));
                                if (async_hold) {
                                    // Hold the slot across an async wait (suspended).
                                    hold_timer.expires_after(sc.work_duration);
                                    co_await hold_timer.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));
                                } else {
                                    (*lease)->work_for(sc.work_duration);
                                }
                                completed.fetch_add(1, std::memory_order_relaxed);
                            } else {
                                skipped.fetch_add(1, std::memory_order_relaxed);
                            }
                        }
                        if (is_burst && !stop.load(std::memory_order_acquire)) {
                            idle_timer.expires_after(sc.idle_between_bursts);
                            co_await idle_timer.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));
                        }
                    }
                    live.fetch_sub(1, std::memory_order_acq_rel);
                    co_return;
                },
                boost::asio::detached);
        }

        for (auto _ : state) {  // NOLINT(clang-diagnostic-unused-but-set-variable)
            (void)_;
            std::this_thread::sleep_for(5ms);
        }

        // Signal stop, then wait for every coroutine to observe it and exit. Parked
        // coroutines unblock within `timeout`, so this drains promptly. Only after the
        // list is empty do we let `backend` destruct (join its io threads), guaranteeing
        // no in-flight coroutine still references `pool` / `collectors`.
        stop.store(true, std::memory_order_release);
        while (live.load(std::memory_order_acquire) != 0) {
            std::this_thread::sleep_for(1ms);
        }

        state.SetItemsProcessed(completed.load(std::memory_order_relaxed));
        state.counters["skipped"]     = static_cast<double>(skipped.load(std::memory_order_relaxed));
        state.counters["rps_ceiling"] = rps_ceiling(sc, n_coros);
        merge_latency(state, collectors);
    }

}  // namespace bench::pool
