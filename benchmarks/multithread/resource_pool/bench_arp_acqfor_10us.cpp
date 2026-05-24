#include <chrono>
#include <cstddef>
#include <demiplane/multithread>
#include <string>

#include <benchmark/benchmark.h>

#include "common/bench_scenarios.hpp"
#include "common/coro_workload.hpp"
#include "common/mock_resource.hpp"
#include "common/pool_bench_main.hpp"

using namespace bench::pool;
using demiplane::multithread::AsyncResourcePool;

using PoolT                        = AsyncResourcePool<MockResource, 1024>;
constexpr auto ACQUIRE_FOR_TIMEOUT = std::chrono::microseconds{10};

namespace {

    auto make_pool(const Scenario& sc, const std::size_t workers) {
        return PoolT{free_pool_size(sc, workers), [](const std::size_t i) noexcept { return MockResource{i}; }};
    }

}  // namespace

int main(int argc, char** argv) {
    const bool pin = parse_pin_flag(argc, argv);
    print_calibration();
    env_check();

    for (const auto kind : ASYNC_FREE_REGION_SCENARIOS) {
        const Scenario sc      = scenario(kind);
        const std::string name = make_bench_name("ARP_AcqFor_10us", sc.name);
        auto* b                = ::benchmark::RegisterBenchmark(name, [pin, sc](::benchmark::State& st) {
            const auto workers = static_cast<std::size_t>(st.range(0));
            auto pool          = make_pool(sc, workers);
            run_coro_workload(st, sc, pool, ACQUIRE_FOR_TIMEOUT, pin);
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
