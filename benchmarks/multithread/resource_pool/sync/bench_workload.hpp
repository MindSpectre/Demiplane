#pragma once

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <demiplane/chrono>
#include <demiplane/multithread>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>
#include <pause.hpp>

#include "common/bench_latency.hpp"
#include "common/bench_scenarios.hpp"

namespace bench::pool {
    using TscClock = demiplane::chrono::TscClock;

    namespace detail {

        inline void busy_wait_for(const std::uint64_t cycles, std::atomic<bool>& stop) noexcept {
            const std::uint64_t end = TscClock::now() + cycles;
            while (TscClock::now() < end && !stop.load(std::memory_order_acquire)) {
                demiplane::multithread::pause_arc_agnostic();
            }
        }

        template <typename Pool>
        concept HasPinnedApi = requires(Pool& p) { p.pinned(std::size_t{0}); };

        template <typename Pool, typename Strategy>
        void run_acquire_frame(const Scenario& sc,
                               Strategy& strategy,
                               Pool& pool,
                               LatencyCollector& collector,
                               std::atomic<std::int64_t>& completed,
                               std::atomic<std::int64_t>& skipped,
                               const std::uint64_t frame_end_cycles,
                               std::atomic<bool>& stop) noexcept {
            switch (sc.kind) {
                case ScenarioKind::Steady:
                case ScenarioKind::TimeoutPressure: {
                    while (TscClock::now() < frame_end_cycles && !stop.load(std::memory_order_acquire)) {
                        const std::uint64_t t0 = TscClock::now();
                        auto lease             = strategy(pool);
                        const std::uint64_t t1 = TscClock::now();
                        if (lease.has_value()) {
                            collector.record(TscClock::to_duration(t1 - t0));
                            (*lease)->work_for(sc.work_duration);
                            completed.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            skipped.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    break;
                }
                case ScenarioKind::Burst:
                case ScenarioKind::HeavyBurst: {
                    const std::uint64_t idle_cycles = TscClock::to_cycles(sc.idle_between_bursts);
                    while (TscClock::now() < frame_end_cycles && !stop.load(std::memory_order_acquire)) {
                        for (int b = 0; b < sc.burst_size && TscClock::now() < frame_end_cycles; ++b) {
                            const std::uint64_t t0 = TscClock::now();
                            auto lease             = strategy(pool);
                            const std::uint64_t t1 = TscClock::now();
                            if (lease.has_value()) {
                                collector.record(TscClock::to_duration(t1 - t0));
                                (*lease)->work_for(sc.work_duration);
                                completed.fetch_add(1, std::memory_order_relaxed);
                            } else {
                                skipped.fetch_add(1, std::memory_order_relaxed);
                            }
                        }
                        if (TscClock::now() >= frame_end_cycles) {
                            break;
                        }
                        busy_wait_for(idle_cycles, stop);
                    }
                    break;
                }
                case ScenarioKind::PinnedZeroContention:
                case ScenarioKind::AsioPostSteady:
                case ScenarioKind::AsioPostBurst:
                    // Dispatched separately (run_pinned_frame / run_asio_workload);
                    // not entered through run_acquire_frame.
                    break;
            }
        }

        template <typename Pool>
        void run_pinned_frame(const Scenario& sc,
                              Pool& pool,
                              LatencyCollector& collector,
                              std::atomic<std::int64_t>& completed,
                              const std::uint64_t frame_end_cycles,
                              const std::size_t my_slot,
                              std::atomic<bool>& stop) noexcept
            requires HasPinnedApi<Pool>
        {
            constexpr int BATCH                           = 1024;
            std::atomic<typename Pool::value_type*>& cell = pool.pinned(my_slot);

            while (TscClock::now() < frame_end_cycles && !stop.load(std::memory_order_acquire)) {
                const std::uint64_t t0 = TscClock::now();
                for (int k = 0; k < BATCH; ++k) {
                    auto* r = cell.load(std::memory_order_acquire);
                    benchmark::DoNotOptimize(r);
                    if (r != nullptr) [[likely]] {
                        if (sc.work_duration.count() > 0) {
                            r->work_for(sc.work_duration);
                        }
                    }
                    demiplane::multithread::pause_arc_agnostic();
                }
                const std::uint64_t t1 = TscClock::now();
                collector.record(TscClock::to_duration((t1 - t0) / BATCH));
                completed.fetch_add(BATCH, std::memory_order_relaxed);
            }
        }

        /// Per-asio-thread latency slot, claimed lazily on the first task each
        /// thread runs. `slots` is sized to the asio worker count; threads
        /// that run no tasks leave their slot empty (merge_latency handles it).
        struct AsioCollectorRegistry {
            std::vector<LatencyCollector> slots;
            std::atomic<int> next_id{0};

            LatencyCollector& my() {
                thread_local int id = next_id.fetch_add(1, std::memory_order_relaxed);
                return slots[static_cast<std::size_t>(id)];
            }
        };

    }  // namespace detail

    /// Drives one benchmark measurement.
    ///
    /// Threads are pre-spawned ONCE; each Google-Benchmark iteration is a single
    /// barrier-synchronised "frame" of ~5 ms wall time. Reasons:
    ///   - jthread ctor/join is 30–80 µs on Linux; spawning per iteration would
    ///     swamp the timing at low worker counts.
    ///   - The barrier pair makes the per-iteration timing reflect actual work,
    ///     not setup.
    template <typename Pool, typename Strategy>
    void run_workload(benchmark::State& state, const Scenario& sc, Pool& pool, Strategy strategy, const bool pin) {
        using namespace std::chrono_literals;
        constexpr auto FRAME_BUDGET              = 5ms;
        constexpr std::size_t SAMPLES_PER_WORKER = 1u << 16;
        const auto workers                       = static_cast<std::size_t>(state.range(0));
        const std::uint64_t frame_budget_cycles  = TscClock::to_cycles(FRAME_BUDGET);

        std::vector<LatencyCollector> collectors(workers);
        for (auto& c : collectors) {
            c.reserve(SAMPLES_PER_WORKER);
        }

        std::atomic<std::int64_t> completed{0};
        std::atomic<std::int64_t> skipped{0};
        std::atomic<bool> stop{false};

        std::barrier go_gate{static_cast<std::ptrdiff_t>(workers + 1)};
        std::barrier done_gate{static_cast<std::ptrdiff_t>(workers + 1)};

        std::vector<std::jthread> ws;
        ws.reserve(workers);
        for (std::size_t i = 0; i < workers; ++i) {
            ws.emplace_back([&, i] {
                if (pin) {
                    demiplane::multithread::pin_current_thread_to_core(static_cast<int>(i % CORE_COUNT));
                }
                while (true) {
                    go_gate.arrive_and_wait();
                    if (stop.load(std::memory_order_acquire)) {
                        return;
                    }
                    const std::uint64_t frame_end = TscClock::now() + frame_budget_cycles;
                    if constexpr (detail::HasPinnedApi<Pool>) {
                        if (sc.kind == ScenarioKind::PinnedZeroContention) {
                            detail::run_pinned_frame(sc, pool, collectors[i], completed, frame_end, i, stop);
                            done_gate.arrive_and_wait();
                            continue;
                        }
                    }
                    detail::run_acquire_frame(sc, strategy, pool, collectors[i], completed, skipped, frame_end, stop);
                    done_gate.arrive_and_wait();
                }
            });
        }

        for (auto _ : state) {  // NOLINT(clang-diagnostic-unused-but-set-variable)
            (void)_;
            go_gate.arrive_and_wait();
            done_gate.arrive_and_wait();
        }

        stop.store(true, std::memory_order_release);
        go_gate.arrive_and_wait();  // wake workers waiting at gate
        ws.clear();                 // jthread dtors join

        state.SetItemsProcessed(completed.load(std::memory_order_relaxed));
        state.counters["skipped"]     = static_cast<double>(skipped.load(std::memory_order_relaxed));
        state.counters["rps_ceiling"] = rps_ceiling(sc, workers);
        merge_latency(state, collectors);
    }

    /// Runner for the AsioPost* scenarios.
    ///
    /// Architecture: 1 producer thread + N asio worker threads (N = state.range(0)).
    /// Producer posts tasks to the asio io_context as fast as it can (Steady)
    /// or in bursts of B with idle gaps I (Burst). Each task acquires from the
    /// pool, does work, releases. Latency is measured per-task on the asio
    /// worker (rdtsc around acquire only — release is the lease dtor at
    /// end-of-lambda and is excluded, matching the other scenarios).
    template <typename Pool, typename Strategy>
    void run_asio_workload(benchmark::State& state, const Scenario& sc, Pool& pool, Strategy strategy, const bool pin) {
        const auto n_workers = static_cast<std::size_t>(state.range(0));

        // ORDER MATTERS for clean teardown.
        // Asio worker threads can be mid-task (touching `registry.slots`)
        // when their threads_ dtor joins. So the backend must destruct
        // BEFORE the registry. Locals destruct in reverse construction
        // order ⇒ construct registry first, backend second.
        std::atomic<std::int64_t> completed{0};
        std::atomic<std::int64_t> skipped{0};
        std::atomic<bool> stop{false};

        detail::AsioCollectorRegistry registry{std::vector<LatencyCollector>(n_workers), 0};
        for (auto& c : registry.slots) {
            c.reserve(1u << 16);
        }

        demiplane::multithread::AsioBackend backend{n_workers, pin, static_cast<std::size_t>(CORE_COUNT)};

        auto task = [&] {
            auto& col              = registry.my();
            const std::uint64_t t0 = TscClock::now();
            auto lease             = strategy(pool);
            const std::uint64_t t1 = TscClock::now();
            if (lease.has_value()) {
                col.record(TscClock::to_duration(t1 - t0));
                (*lease)->work_for(sc.work_duration);
                completed.fetch_add(1, std::memory_order_relaxed);
            } else {
                skipped.fetch_add(1, std::memory_order_relaxed);
            }
        };

        std::jthread producer([&] {
            if (pin) {
                demiplane::multithread::pin_current_thread_to_core(0);
            }
            if (sc.kind == ScenarioKind::AsioPostSteady) {
                while (!stop.load(std::memory_order_acquire)) {
                    backend.post(task);
                }
            } else {  // AsioPostBurst
                const std::uint64_t idle_cycles = TscClock::to_cycles(sc.idle_between_bursts);
                while (!stop.load(std::memory_order_acquire)) {
                    for (int b = 0; b < sc.burst_size && !stop.load(std::memory_order_acquire); ++b) {
                        backend.post(task);
                    }
                    detail::busy_wait_for(idle_cycles, stop);
                }
            }
        });

        using namespace std::chrono_literals;
        for (auto _ : state) {  // NOLINT(clang-diagnostic-unused-but-set-variable)
            (void)_;
            std::this_thread::sleep_for(5ms);
        }

        stop.store(true, std::memory_order_release);
        producer = std::jthread{};
        // AsioBackend dtor drains the queue and joins its workers before
        // the pool's lifetime ends (pool is a caller-owned local).

        state.SetItemsProcessed(completed.load(std::memory_order_relaxed));
        state.counters["skipped"]     = static_cast<double>(skipped.load(std::memory_order_relaxed));
        state.counters["rps_ceiling"] = rps_ceiling(sc, n_workers);
        merge_latency(state, registry.slots);
    }

}  // namespace bench::pool
