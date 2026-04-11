##############################################################################
# PostgreSQL Client Overhead Benchmarks
# Requires vcpkg feature 'pg-benchmarks' (Google Benchmark + libpqxx)
##############################################################################
find_package(benchmark CONFIG QUIET)
find_package(libpqxx CONFIG QUIET)

if (benchmark_FOUND AND libpqxx_FOUND)

    # ─── Shared fixture library ──────────────────────────────────────────
    add_library(bench_pg_common STATIC
            bench_common/bench_fixture.cpp
    )
    target_include_directories(bench_pg_common PUBLIC bench_common)
    target_link_libraries(bench_pg_common
            PUBLIC
            benchmark::benchmark
            benchmark::benchmark_main
            PostgreSQL::PostgreSQL
            Boost::headers
            Demiplane::Common::Gears
    )

    add_subdirectory(simple)
    add_subdirectory(concurrency)

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
add_executable(${DMP_BENCHMARKS}.Database.RawVsCompiled
        raw_vs_compiled_benchmark.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Database.RawVsCompiled
        PRIVATE
        Demiplane::Common::Chrono
        Demiplane::Component::Database::PostgreSQL
)
##############################################################################
