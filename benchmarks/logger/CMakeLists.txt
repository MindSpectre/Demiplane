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
        Demiplane::Common::Ink
)

##############################################################################
# Benchmark Abseil file logger (8-thread contention + 1-thread baseline)
# Only built when the "abseil-benchmarks" vcpkg feature is enabled.
##############################################################################
find_package(absl CONFIG QUIET)

if (absl_FOUND)
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
            Demiplane::Common::Ink
    )
else ()
    message(STATUS "Abseil not found — skipping Abseil benchmarks (enable vcpkg feature 'abseil-benchmarks')")
endif ()
