#include <cstddef>
#include <demiplane/multithread>
#include <optional>

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
    auto strategy = [](PoolT& p) noexcept -> std::optional<demiplane::multithread::Lease<MockResource>> {
        return p.try_acquire();
    };
    return run_sync_bench_main<PoolT>(argc, argv, "RP_Try", FREE_REGION_SCENARIOS, make_pool, strategy);
}
