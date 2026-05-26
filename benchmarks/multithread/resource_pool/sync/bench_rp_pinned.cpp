#include <array>
#include <chrono>
#include <cstddef>
#include <demiplane/multithread>

#include "common/bench_scenarios.hpp"
#include "common/mock_resource.hpp"
#include "sync/sync_bench_main.hpp"

using namespace bench::pool;
using demiplane::multithread::ResourcePool;

using PoolT = ResourcePool<MockResource, 1024>;

static_assert(WORKER_COUNTS_FLOATING.back() <= 1024, "MaxSize=1024 must cover the largest floating worker count");

int main(int argc, char** argv) {
    auto make_pool = [](const Scenario& sc, const std::size_t workers) {
        return PoolT{pinned_pool_size(sc, workers),
                     free_pool_size(sc, workers),
                     std::chrono::nanoseconds{400},
                     [](const std::size_t i) noexcept { return MockResource{i}; }};
    };
    // Unused on the pinned code path (run_pinned_frame takes the pinned-API branch),
    // but the sync helper's workload-runner template requires a strategy argument.
    auto strategy = [](PoolT& p) noexcept { return p.try_acquire(); };
    static constexpr std::array pinned_scenarios{ScenarioKind::PinnedZeroContention};
    return run_sync_bench_main<PoolT>(argc, argv, "RP_Pinned", pinned_scenarios, make_pool, strategy);
}
