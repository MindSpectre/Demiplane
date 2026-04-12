##############################################################################
# True Concurrency Benchmarks
# Many workers sharing a small connection pool — models real HTTP load
##############################################################################

# ─── Session Async (coroutines on io_context) ─────────────────────────
add_executable(${DMP_BENCHMARKS_POSTGRESQL}.Concurrency.SessionAsync
        benchmark_session_async.cpp
)
target_link_libraries(${DMP_BENCHMARKS_POSTGRESQL}.Concurrency.SessionAsync
        PRIVATE
        ${DMP_BENCHMARKS_POSTGRESQL}.Common
        Demiplane::Component::Database::PostgreSQL
)

# ─── pqxx (tasks on thread_pool) ─────────────────────────────────────
add_executable(${DMP_BENCHMARKS_POSTGRESQL}.Concurrency.Pqxx
        benchmark_pqxx.cpp
)
target_link_libraries(${DMP_BENCHMARKS_POSTGRESQL}.Concurrency.Pqxx
        PRIVATE
        ${DMP_BENCHMARKS_POSTGRESQL}.Common
        libpqxx::pqxx
)
