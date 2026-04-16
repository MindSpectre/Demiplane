set(DMP_BENCHMARKS_POSTGRESQL ${DMP_BENCHMARKS}.Database.PostgreSQL)
##############################################################################
# PostgreSQL Client Overhead Benchmarks
# Requires vcpkg feature 'pg-benchmarks' (Google Benchmark + libpqxx)
##############################################################################
find_package(benchmark CONFIG QUIET)
find_package(libpqxx CONFIG QUIET)

if (benchmark_FOUND AND libpqxx_FOUND)

    # ─── Shared fixture library ──────────────────────────────────────────
    add_library(${DMP_BENCHMARKS_POSTGRESQL}.Common STATIC
            common/bench_fixture.cpp
    )
    target_include_directories(${DMP_BENCHMARKS_POSTGRESQL}.Common PUBLIC common)
    target_link_libraries(${DMP_BENCHMARKS_POSTGRESQL}.Common
            PUBLIC
            benchmark::benchmark
            benchmark::benchmark_main
            PostgreSQL::PostgreSQL
            Boost::headers
            Demiplane::Common::Gears
    )

    add_subdirectory(concurrency_scenarios)

else ()
    if (NOT benchmark_FOUND)
        message(STATUS "Google Benchmark not found — skipping PG overhead benchmarks (enable vcpkg feature 'pg-benchmarks')")
    endif ()
    if (NOT libpqxx_FOUND)
        message(STATUS "libpqxx not found — skipping PG overhead benchmarks (enable vcpkg feature 'pg-benchmarks')")
    endif ()
endif ()

##############################################################################
# Benchmark Raw String vs Compiled Query
##############################################################################
add_executable(${DMP_BENCHMARKS_POSTGRESQL}.RawVsCompiled
        raw_vs_compiled_benchmark.cpp
)
target_link_libraries(${DMP_BENCHMARKS_POSTGRESQL}.RawVsCompiled
        PRIVATE
        Demiplane::Common::Chrono
        Demiplane::Component::Database::PostgreSQL
)
##############################################################################
