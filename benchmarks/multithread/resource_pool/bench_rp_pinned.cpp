#include <chrono>
#include <cstddef>
#include <demiplane/multithread>
#include <optional>

#include <benchmark/benchmark.h>

#include "common/bench_scenarios.hpp"
#include "common/bench_workload.hpp"
#include "common/mock_resource.hpp"
#include "common/pool_bench_main.hpp"

using namespace bench::pool;
using demiplane::multithread::ResourcePool;

using PoolT = ResourcePool<MockResource, 1024>;

static_assert(WORKER_COUNTS_FLOATING.back() <= 1024, "MaxSize=1024 must cover the largest floating worker count");

namespace {

    auto make_pool(const Scenario& sc, const std::size_t workers) {
        return PoolT{pinned_pool_size(sc, workers),
                     free_pool_size(sc, workers),
                     std::chrono::nanoseconds{400},
                     [](const std::size_t i) noexcept { return MockResource{i}; }};
    }

    // Strategy is unused by the pinned-zero-contention frame (the runner takes
    // the pinned-API code path), but the workload-runner template requires one.
    auto strategy = [](PoolT& p) noexcept { return p.try_acquire(); };

}  // namespace

int main(int argc, char** argv) {
    const bool pin = parse_pin_flag(argc, argv);
    print_calibration();
    env_check();

    constexpr Scenario sc  = scenario(ScenarioKind::PinnedZeroContention);
    const std::string name = make_bench_name("RP_Pinned", sc.name);
    auto* b                = ::benchmark::RegisterBenchmark(name, [pin, sc](::benchmark::State& st) {
        const auto workers = static_cast<std::size_t>(st.range(0));
        auto pool          = make_pool(sc, workers);
        run_workload(st, sc, pool, strategy, pin);
    });
    if (pin) {
        register_pinned_worker(b);
    } else {
        register_floating_workers(b);
    }

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
