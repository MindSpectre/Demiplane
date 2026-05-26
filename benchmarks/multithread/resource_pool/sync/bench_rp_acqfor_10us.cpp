#include <chrono>
#include <cstddef>
#include <demiplane/multithread>

#include "common/bench_scenarios.hpp"
#include "common/mock_resource.hpp"
#include "sync/sync_bench_main.hpp"

using namespace bench::pool;
using demiplane::multithread::ResourcePool;

using PoolT = ResourcePool<MockResource, 1024>;

int main(int argc, char** argv) {
    auto make_pool = [](const Scenario& sc, const std::size_t workers) {
        return PoolT{free_pool_size(sc, workers), [](const std::size_t i) noexcept { return MockResource{i}; }};
    };
    auto strategy = [](PoolT& p) noexcept { return p.acquire_for(std::chrono::microseconds{10}); };
    return run_sync_bench_main<PoolT>(argc, argv, "RP_AcqFor_10us", FREE_REGION_SCENARIOS, make_pool, strategy);
}
