#include <chrono>
#include <cstddef>
#include <demiplane/multithread>

#include "async/async_bench_main.hpp"
#include "common/bench_scenarios.hpp"
#include "common/mock_resource.hpp"

using namespace bench::pool;
using demiplane::multithread::AsyncResourcePool;

using PoolT = AsyncResourcePool<MockResource, 1024>;

int main(int argc, char** argv) {
    auto make_pool = [](const Scenario& sc, const std::size_t workers) {
        return PoolT{free_pool_size(sc, workers), [](const std::size_t i) noexcept { return MockResource{i}; }};
    };
    return run_async_bench_main<PoolT>(argc, argv, "ARP_AcqFor_1us", std::chrono::microseconds{1}, make_pool);
}
