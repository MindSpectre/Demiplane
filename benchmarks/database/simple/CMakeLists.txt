##############################################################################
# Simple Threaded Benchmarks
# Overhead comparison: each client runs N threads, each thread hammers queries
##############################################################################

# ─── 1. libpq (control group) ────────────────────────────────────────
add_executable(${DMP_BENCHMARKS}.Database.Simple.LibPQ
        benchmark_libpq.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Database.Simple.LibPQ
        PRIVATE
        bench_pg_common
)

# ─── 2. pqxx ─────────────────────────────────────────────────────────
add_executable(${DMP_BENCHMARKS}.Database.Simple.Pqxx
        benchmark_pqxx.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Database.Simple.Pqxx
        PRIVATE
        bench_pg_common
        libpqxx::pqxx
)

# ─── 3. SyncExecutor ─────────────────────────────────────────────────
add_executable(${DMP_BENCHMARKS}.Database.Simple.SyncExecutor
        benchmark_sync_executor.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Database.Simple.SyncExecutor
        PRIVATE
        bench_pg_common
        Demiplane::Component::Database::PostgreSQL
)

# ─── 4. AsyncExecutor ────────────────────────────────────────────────
add_executable(${DMP_BENCHMARKS}.Database.Simple.AsyncExecutor
        benchmark_async_executor.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Database.Simple.AsyncExecutor
        PRIVATE
        bench_pg_common
        Demiplane::Component::Database::PostgreSQL
)

# ─── 5. Session (sync) ───────────────────────────────────────────────
add_executable(${DMP_BENCHMARKS}.Database.Simple.SessionSync
        benchmark_session_sync.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Database.Simple.SessionSync
        PRIVATE
        bench_pg_common
        Demiplane::Component::Database::PostgreSQL
)

# ─── 6. Session (async) ──────────────────────────────────────────────
add_executable(${DMP_BENCHMARKS}.Database.Simple.SessionAsync
        benchmark_session_async.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Database.Simple.SessionAsync
        PRIVATE
        bench_pg_common
        Demiplane::Component::Database::PostgreSQL
)
