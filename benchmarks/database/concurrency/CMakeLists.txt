##############################################################################
# True Concurrency Benchmarks
# Many workers sharing a small connection pool — models real HTTP load
##############################################################################

# ─── Session Async (coroutines on io_context) ─────────────────────────
add_executable(${DMP_BENCHMARKS}.Database.Concurrency.SessionAsync
        benchmark_session_async.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Database.Concurrency.SessionAsync
        PRIVATE
        bench_pg_common
        Demiplane::Component::Database::PostgreSQL
)

# ─── pqxx (tasks on thread_pool) ─────────────────────────────────────
add_executable(${DMP_BENCHMARKS}.Database.Concurrency.Pqxx
        benchmark_pqxx.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Database.Concurrency.Pqxx
        PRIVATE
        bench_pg_common
        libpqxx::pqxx
)
