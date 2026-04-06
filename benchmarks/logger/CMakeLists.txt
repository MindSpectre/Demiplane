##############################################################################
# Benchmark Scroll file logger (8-thread contention + 1-thread baseline)
##############################################################################
add_executable(${DMP_BENCHMARKS}.Scroll.FileBenchmark
        benchmark_harness.hpp
        scroll_file_benchmark.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Scroll.FileBenchmark
        PRIVATE
        Demiplane::Common::Scroll
        Demiplane::Common::Nexus
        Demiplane::Common::Math
)

##############################################################################
# Benchmark Abseil file logger (8-thread contention + 1-thread baseline)
##############################################################################
find_package(absl CONFIG REQUIRED)

add_executable(${DMP_BENCHMARKS}.Abseil.FileBenchmark
        benchmark_harness.hpp
        abseil_file_benchmark.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Abseil.FileBenchmark
        PRIVATE
        absl::log
        absl::log_sink
        absl::log_initialize
        absl::log_globals
        Demiplane::Common::Gears
)
