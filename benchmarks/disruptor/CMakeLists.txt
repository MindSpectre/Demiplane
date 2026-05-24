##############################################################################
# Benchmark Disruptor
##############################################################################
add_executable(${DMP_BENCHMARKS}.Multithread.Disruptor
        disruptor_speed_test.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Multithread.Disruptor
        PRIVATE
        Demiplane::Common::Multithread
        Demiplane::Common::Ink
        Threads::Threads
)
# Reuse the header-only rdtsc_clock.hpp / pin_to_core.hpp from the resource_pool benchmarks.
target_include_directories(${DMP_BENCHMARKS}.Multithread.Disruptor
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../multithread/resource_pool/common
)
add_compile_options(${DMP_BENCHMARK}.Multithread.Disruptor
    -O3
)
##############################################################################