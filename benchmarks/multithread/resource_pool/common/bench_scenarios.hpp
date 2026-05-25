#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <string_view>

#include <benchmark/benchmark.h>

namespace bench::pool {

    enum class ScenarioKind {
        Steady,
        Burst,
        TimeoutPressure,
        AsioPostSteady,
        AsioPostBurst,
        PinnedZeroContention,
        HeavyBurst,
    };

    struct Scenario {
        std::string_view name;
        ScenarioKind kind;
        std::chrono::nanoseconds work_duration;
        int burst_size;                                // Burst, AsioPostBurst
        std::chrono::nanoseconds idle_between_bursts;  // Burst, AsioPostBurst
    };

    // Work durations are sized to "one small websocket frame write" (~500 ns
    // on Linux for serialize + small send()). TimeoutPressure holds slightly
    // longer (2 frames) to amplify contention against the short timeouts.
    inline constexpr std::array<Scenario, 7> ALL_SCENARIOS = {
        {
         {"Steady", ScenarioKind::Steady, std::chrono::nanoseconds{500}, 0, std::chrono::nanoseconds{0}},
         {"Burst", ScenarioKind::Burst, std::chrono::nanoseconds{500}, 8, std::chrono::microseconds{10}},
         {"TimeoutPressure",
             ScenarioKind::TimeoutPressure,
             std::chrono::microseconds{1},
             0,
             std::chrono::nanoseconds{0}},
         // AsioPost*: 1 producer thread + state.range(0) asio workers. Producer
            // posts tasks; each task acquires from the pool, works for W, releases.
            // Steady = post 1-by-1, no pauses. Burst = B back-to-back posts, then
            // busy-wait I, repeat.
            {"AsioPostSteady",
             ScenarioKind::AsioPostSteady,
             std::chrono::nanoseconds{500},
             0,
             std::chrono::nanoseconds{0}},
         {"AsioPostBurst",
             ScenarioKind::AsioPostBurst,
             std::chrono::nanoseconds{500},
             8,
             std::chrono::microseconds{10}},
         // W=0: pinned-cell load floor check. Pool.md claims 1–2 ns for the
            // pinned path; with W=0 the runner skips work_for and measures only
            // the amortized cell.load(acquire) cost, isolating that floor. This
            // scenario is the *reference baseline* — plot script overlays its
            // p50 as a horizontal line on the other scenarios' latency plots.
            {"PinnedZeroContention",
             ScenarioKind::PinnedZeroContention,
             std::chrono::nanoseconds{0},
             0,
             std::chrono::nanoseconds{0}},
         // HeavyBurst: SPMC-drain spike. Each worker rips through burst_size=256
            // records back-to-back (a freelock queue dumped ~all at once), then a
            // long idle cooldown, repeat. The held work is W=10µs — for the async
            // pool this is held ACROSS a co_await (≈ an I/O round-trip), so far more
            // than the io-thread count hold slots at once and a P=128 pool genuinely
            // saturates and parks; for the sync pool the 256 threads hold across the
            // synchronous work_for and exhaust it the same way. This is the scenario
            // that exercises the park/wake path under a heavy transient.
            {"HeavyBurst",
             ScenarioKind::HeavyBurst,
             std::chrono::microseconds{10},
             256,
             std::chrono::microseconds{500}},
         }
    };

    [[nodiscard]] constexpr const Scenario& scenario(const ScenarioKind k) noexcept {
        for (const auto& s : ALL_SCENARIOS) {
            if (s.kind == k) {
                return s;
            }
        }
        return ALL_SCENARIOS[0];  // unreachable in practice
    }

    /// Sub-views matching what each binary registers.
    inline constexpr std::array<ScenarioKind, 6> FREE_REGION_SCENARIOS = {
        ScenarioKind::Steady,
        ScenarioKind::Burst,
        ScenarioKind::TimeoutPressure,
        ScenarioKind::AsioPostSteady,
        ScenarioKind::AsioPostBurst,
        ScenarioKind::HeavyBurst,
    };


    inline constexpr std::array<int, 6> WORKER_COUNTS_FLOATING = {8, 16, 32, 64, 128, 256};
    inline constexpr int WORKER_COUNT_PINNED                   = 10;
    inline constexpr int CORE_COUNT                            = 10;   // taskset -c 0-9
    inline constexpr std::size_t FREE_POOL_SIZE                = 128;  // fixed for all free scenarios

    // Pool is fixed at FREE_POOL_SIZE for the free-region scenarios. Contention
    // is varied entirely by worker count: workers < pool = sparse, workers ==
    // pool = saturated, workers > pool = oversubscribed (Burst peak).
    [[nodiscard]] constexpr std::size_t free_pool_size(const Scenario& sc,
                                                       [[maybe_unused]] const std::size_t workers) noexcept {
        switch (sc.kind) {
            case ScenarioKind::Steady:
            case ScenarioKind::Burst:
            case ScenarioKind::TimeoutPressure:
            case ScenarioKind::AsioPostSteady:
            case ScenarioKind::AsioPostBurst:
            case ScenarioKind::HeavyBurst:
                return FREE_POOL_SIZE;
            case ScenarioKind::PinnedZeroContention:
                return 0;
        }
        return 1;
    }

    [[nodiscard]] constexpr std::size_t pinned_pool_size(const Scenario& sc, const std::size_t workers) noexcept {
        return sc.kind == ScenarioKind::PinnedZeroContention ? workers : 0;
    }

    [[nodiscard]] constexpr double rps_ceiling(const Scenario& sc, const std::size_t workers) noexcept {
        const auto w_ns = static_cast<double>(sc.work_duration.count());
        if (w_ns <= 0.0) {
            return 0.0;
        }
        const double inv_w_s = 1e9 / w_ns;
        switch (sc.kind) {
            case ScenarioKind::Steady:
            case ScenarioKind::Burst:
            case ScenarioKind::TimeoutPressure:
            case ScenarioKind::HeavyBurst:
                return static_cast<double>(free_pool_size(sc, workers)) * inv_w_s;
            case ScenarioKind::AsioPostSteady:
            case ScenarioKind::AsioPostBurst:
                // The actual cap on these scenarios is a single producer's
                // `post()` throughput (~1 µs of asio-internal queue insert per
                // task ⇒ ~1 M/s, not the pool's P/W). Report 0 to suppress
                // the misleading "P/W ceiling" reference line — the bars
                // themselves show the producer-bound rate.
                return 0.0;
            case ScenarioKind::PinnedZeroContention:
                return static_cast<double>(workers) * inv_w_s;
        }
        return 0.0;
    }

    inline void register_floating_workers(::benchmark::Benchmark* b) {
        for (const auto w : WORKER_COUNTS_FLOATING) {
            b->Arg(w);
        }
        b->UseRealTime();
    }

    inline void register_pinned_worker(::benchmark::Benchmark* b) {
        b->Arg(WORKER_COUNT_PINNED);
        b->UseRealTime();
    }

}  // namespace bench::pool
