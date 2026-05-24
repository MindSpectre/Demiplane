#include <chrono>
#include <cstddef>
#include <optional>

#include <benchmark/benchmark.h>
#include <demiplane/multithread>

#include "common/bench_scenarios.hpp"
#include "common/bench_workload.hpp"
#include "common/mock_resource.hpp"
#include "common/pool_bench_main.hpp"

using namespace bench::pool;
using demiplane::multithread::ResourcePool;

using PoolT = ResourcePool<MockResource, 1024>;

namespace {

    auto make_pool(const Scenario& sc, const std::size_t workers) {
        return PoolT{free_pool_size(sc, workers),
                     [](const std::size_t i) noexcept { return MockResource{i}; }};
    }

    auto strategy = [](PoolT& p) noexcept -> std::optional<demiplane::multithread::Lease<MockResource>> {
        return p.try_acquire();
    };

}  // namespace

int main(int argc, char** argv) {
    const bool pin = parse_pin_flag(argc, argv);
    print_calibration();
    env_check();

    for (const auto kind : FREE_REGION_SCENARIOS) {
        const Scenario    sc   = scenario(kind);
        const std::string name = make_bench_name("RP_Try", sc.name);
        auto* b                = ::benchmark::RegisterBenchmark(
            name,
            [pin, sc](::benchmark::State& st) {
                const auto workers = static_cast<std::size_t>(st.range(0));
                auto       pool    = make_pool(sc, workers);
                if (sc.kind == ScenarioKind::AsioPostSteady
                    || sc.kind == ScenarioKind::AsioPostBurst) {
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

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
