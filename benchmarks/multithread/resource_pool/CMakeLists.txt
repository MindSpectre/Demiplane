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
add_library(${DMP_BENCHMARKS_RESOURCEPOOL}.Common INTERFACE)
target_sources(${DMP_BENCHMARKS_RESOURCEPOOL}.Common
        INTERFACE
        common/asio_backend.hpp
        common/bench_latency.hpp
        common/bench_scenarios.hpp
        common/bench_workload.hpp
        common/mock_resource.hpp
        common/mutex_queue_pool.hpp
        common/pin_to_core.hpp
        common/pool_bench_main.hpp
        common/rdtsc_clock.hpp
)
target_include_directories(${DMP_BENCHMARKS_RESOURCEPOOL}.Common
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_BENCHMARKS_RESOURCEPOOL}.Common
        INTERFACE
        benchmark::benchmark
        Demiplane::Common::Multithread
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

add_pool_bench(Try bench_rp_try.cpp)
add_pool_bench(AcqFor1us bench_rp_acqfor_1us.cpp)
add_pool_bench(AcqFor2us bench_rp_acqfor_2us.cpp)
add_pool_bench(AcqFor10us bench_rp_acqfor_10us.cpp)
add_pool_bench(Pinned bench_rp_pinned.cpp)
add_pool_bench(MutexBase bench_mutex_base.cpp)

# AsyncResourcePool — coroutine acquire_for, comparable to the sync AcqFor* subjects.
add_pool_bench(ArpAcqFor1us bench_arp_acqfor_1us.cpp)
add_pool_bench(ArpAcqFor2us bench_arp_acqfor_2us.cpp)
add_pool_bench(ArpAcqFor10us bench_arp_acqfor_10us.cpp)

# Flagship: end-to-end burst latency, sync vs async, disruptor-fed.
add_pool_bench(FlagshipBurst bench_flagship_burst.cpp)
##############################################################################
