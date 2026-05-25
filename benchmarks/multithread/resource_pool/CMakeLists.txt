set(DMP_BENCHMARKS_RESOURCEPOOL ${DMP_BENCHMARKS_MULTITHREAD}.ResourcePool)
##############################################################################
# ResourcePool concurrency benchmarks
# One binary per subject; each defines its own main() (no benchmark_main link).
# Run with --pin=0 (floating workers) or --pin=1 (each worker pinned to one
# core; worker_count fixed to 10).
##############################################################################
find_package(benchmark CONFIG QUIET)

if (NOT benchmark_FOUND)
    message(STATUS "Google Benchmark not found — skipping ResourcePool benchmarks")
    return()
endif ()

# ─── Shared header-only common (target_sources keeps the headers visible to IDEs)
add_library(${DMP_BENCHMARKS_RESOURCEPOOL}.Common INTERFACE
        common/bench_latency.hpp
        common/pool_bench_main.hpp
        common/bench_scenarios.hpp
        common/mock_resource.hpp

        flagship/ws_harness.hpp
        flagship/flagship_worker.hpp
        flagship/flagship_utils.hpp
        flagship/flagship_sync.hpp
        flagship/flagship_producer.hpp
        flagship/flagship_config.hpp
        flagship/flagship_async.hpp
        flagship/dispatch/pull_dedicated.hpp
        flagship/dispatch/pull.hpp
        flagship/dispatch/channel.hpp

        sync/bench_workload.hpp
        sync/sync_bench_main.hpp

        async/async_bench_main.hpp
        async/coro_workload.hpp
)
target_include_directories(${DMP_BENCHMARKS_RESOURCEPOOL}.Common
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_BENCHMARKS_RESOURCEPOOL}.Common
        INTERFACE
        benchmark::benchmark
        Demiplane::Common::Multithread
        Demiplane::Common::Chrono
        Demiplane::Common::Ink
        Boost::headers
        Boost::system
        Threads::Threads
)

# ─── Per-subject executables ────────────────────────────────────────────────
function(add_pool_bench name source)
    add_executable(${DMP_BENCHMARKS_RESOURCEPOOL}.${name} ${source})
    target_link_libraries(${DMP_BENCHMARKS_RESOURCEPOOL}.${name}
            PRIVATE
            ${DMP_BENCHMARKS_RESOURCEPOOL}.Common
    )
endfunction()

add_pool_bench(Try sync/bench_rp_try.cpp)
add_pool_bench(AcqFor1us sync/bench_rp_acqfor_1us.cpp)
add_pool_bench(AcqFor2us sync/bench_rp_acqfor_2us.cpp)
add_pool_bench(AcqFor10us sync/bench_rp_acqfor_10us.cpp)
add_pool_bench(Pinned sync/bench_rp_pinned.cpp)

# AsyncResourcePool — coroutine acquire_for, comparable to the sync AcqFor* subjects.
add_pool_bench(ArpAcqFor1us async/bench_arp_acqfor_1us.cpp)
add_pool_bench(ArpAcqFor2us async/bench_arp_acqfor_2us.cpp)
add_pool_bench(ArpAcqFor10us async/bench_arp_acqfor_10us.cpp)

# Flagship: end-to-end burst latency, sync vs async, disruptor-fed.
add_pool_bench(FlagshipBurst flagship/flagship_bench.cpp)
##############################################################################
