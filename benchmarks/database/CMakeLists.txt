##############################################################################
# Benchmark Raw String vs Compiled Query
##############################################################################
add_executable(${DMP_BENCHMARKS}.Database.RawVsCompiled
        raw_vs_compiled_benchmark.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Database.RawVsCompiled
        PRIVATE
        Demiplane::Common::Chrono
        Demiplane::Tests::Shared::Database
)
##############################################################################
