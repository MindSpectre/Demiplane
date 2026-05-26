#pragma once

#include <chrono>
#include <cstddef>
#include <string_view>

#include <benchmark/benchmark.h>

#include "async/coro_workload.hpp"
#include "common/bench_scenarios.hpp"
#include "common/pool_bench_main.hpp"

namespace bench::pool {

    /// Registration entry point for an AsyncResourcePool subject (ArpAcqFor*). No acquire
    /// strategy (every async acquire suspends); always the coroutine runner, over the async
    /// scenario subset (no AsioPost* analog).
    template <typename Pool, typename MakePool>
    int run_async_bench_main(int argc,
                             char** argv,
                             const std::string_view subject,
                             const std::chrono::nanoseconds timeout,
                             MakePool make_pool) {
        const bool pin = parse_pin_flag(argc, argv);
        print_calibration();
        env_check();
        for (const auto kind : ASYNC_FREE_REGION_SCENARIOS) {
            const Scenario sc = scenario(kind);
            auto* b           = ::benchmark::RegisterBenchmark(make_bench_name(subject, sc.name),
                                                     [pin, sc, timeout, make_pool](::benchmark::State& st) {
                                                         const auto workers = static_cast<std::size_t>(st.range(0));
                                                         auto pool          = make_pool(sc, workers);
                                                         run_coro_workload(st, sc, pool, timeout, pin);
                                                     });
            if (pin) {
                register_pinned_worker(b);
            } else {
                register_floating_workers(b);
            }
        }
        ::benchmark::Initialize(&argc, argv);
        ::benchmark::RunSpecifiedBenchmarks();
        ::benchmark::Shutdown();
        return 0;
    }

}  // namespace bench::pool
