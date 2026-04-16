#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

#include <benchmark/benchmark.h>

#include "bench_constants.hpp"

namespace bench::pg {

    struct Scenario {
        std::string_view name;
        int queries_per_fireshot;      // Q
        bool slow_query;               // D: false=instant, true=10ms
        std::size_t executor_threads;  // E
        std::size_t connections;       // C
    };

    inline constexpr std::array<Scenario, 5> SCENARIOS = {
        Scenario{"OverheadBaseline", 1, false, 8,  8 },
        Scenario{"SaturatedSteady",  1, true,  8,  8 },
        Scenario{"BurstFireshot",    4, true,  8,  8 },
        Scenario{"Oversubscribed",   1, true,  4,  16},
        Scenario{"UnderProvisioned", 1, true,  16, 4 },
    };

    inline constexpr std::array<int, 7> WORKER_COUNTS = {8, 16, 32, 64, 128, 256, 512};

    inline void register_workers(::benchmark::Benchmark* b) {
        for (const auto w : WORKER_COUNTS) {
            b->Arg(w);
        }
        b->UseRealTime();
    }

    // Theoretical peak throughput for a scenario. Returns 0.0 for instant
    // queries (no meaningful ceiling).
    [[nodiscard]] constexpr double rps_ceiling(const Scenario& s) noexcept {
        return s.slow_query ? 100.0 * static_cast<double>(s.connections) : 0.0;
    }

    // Effective pool size for control-group subjects that bind one
    // connection per executor thread.
    [[nodiscard]] constexpr std::size_t control_pool_size(const Scenario& s) noexcept {
        return std::min(s.executor_threads, s.connections);
    }

    [[nodiscard]] constexpr const char* query_for(const Scenario& s) noexcept {
        return s.slow_query ? SLOW_BENCH_QUERY : BENCH_QUERY;
    }

}  // namespace bench::pg
