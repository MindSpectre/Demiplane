#pragma once

#include <cstddef>
#include <span>
#include <string_view>

#include <benchmark/benchmark.h>

#include "common/bench_scenarios.hpp"
#include "common/pool_bench_main.hpp"
#include "sync/bench_workload.hpp"

namespace bench::pool {

    /// Registration entry point for a synchronous-pool subject (Try, AcqFor*, Pinned).
    /// Each subject's main() supplies its pool type, factory, acquire strategy, bench-name
    /// subject, and scenario set; the shared boilerplate (pin flag, calibration print, env
    /// check, per-scenario registration, worker-count args, run) lives here. `AsioPost*`
    /// scenarios dispatch to the producer/asio runner; the rest to run_workload.
    template <typename Pool, typename MakePool, typename Strategy>
    int run_sync_bench_main(int argc,
                            char** argv,
                            const std::string_view subject,
                            const std::span<const ScenarioKind> scenarios,
                            MakePool make_pool,
                            Strategy strategy) {
        const bool pin = parse_pin_flag(argc, argv);
        print_calibration();
        env_check();
        for (const auto kind : scenarios) {
            const Scenario sc = scenario(kind);
            auto* b           = ::benchmark::RegisterBenchmark(
                make_bench_name(subject, sc.name), [pin, sc, make_pool, strategy](::benchmark::State& st) {
                    const auto workers = static_cast<std::size_t>(st.range(0));
                    auto pool          = make_pool(sc, workers);
                    if (sc.kind == ScenarioKind::AsioPostSteady || sc.kind == ScenarioKind::AsioPostBurst) {
                        run_asio_workload(st, sc, pool, strategy, pin);
                    } else {
                        run_workload(st, sc, pool, strategy, pin);
                    }
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
