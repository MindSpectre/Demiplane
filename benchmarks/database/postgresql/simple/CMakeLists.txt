##############################################################################
# Simple Threaded Benchmarks
# Overhead comparison: each client runs N threads, each thread hammers queries
##############################################################################

# ─── 1. libpq (control group) ────────────────────────────────────────
add_executable(${DMP_BENCHMARKS_POSTGRESQL}.Simple.LibPQ
        benchmark_libpq.cpp
)
target_link_libraries(${DMP_BENCHMARKS_POSTGRESQL}.Simple.LibPQ
        PRIVATE
        ${DMP_BENCHMARKS_POSTGRESQL}.Common
)

# ─── 2. pqxx ─────────────────────────────────────────────────────────
add_executable(${DMP_BENCHMARKS_POSTGRESQL}.Simple.Pqxx
        benchmark_pqxx.cpp
)
target_link_libraries(${DMP_BENCHMARKS_POSTGRESQL}.Simple.Pqxx
        PRIVATE
        ${DMP_BENCHMARKS_POSTGRESQL}.Common
        libpqxx::pqxx
)

# ─── 3. SyncExecutor ─────────────────────────────────────────────────
add_executable(${DMP_BENCHMARKS_POSTGRESQL}.Simple.SyncExecutor
        benchmark_sync_executor.cpp
)
target_link_libraries(${DMP_BENCHMARKS_POSTGRESQL}.Simple.SyncExecutor
        PRIVATE
        ${DMP_BENCHMARKS_POSTGRESQL}.Common
        Demiplane::Component::Database::PostgreSQL
)

# ─── 4. AsyncExecutor ────────────────────────────────────────────────
add_executable(${DMP_BENCHMARKS_POSTGRESQL}.Simple.AsyncExecutor
        benchmark_async_executor.cpp
)
target_link_libraries(${DMP_BENCHMARKS_POSTGRESQL}.Simple.AsyncExecutor
        PRIVATE
        ${DMP_BENCHMARKS_POSTGRESQL}.Common
        Demiplane::Component::Database::PostgreSQL
)

# ─── 5. Session (sync) ───────────────────────────────────────────────
add_executable(${DMP_BENCHMARKS_POSTGRESQL}.Simple.SessionSync
        benchmark_session_sync.cpp
)
target_link_libraries(${DMP_BENCHMARKS_POSTGRESQL}.Simple.SessionSync
        PRIVATE
        ${DMP_BENCHMARKS_POSTGRESQL}.Common
        Demiplane::Component::Database::PostgreSQL
)

# ─── 6. Session (async) ──────────────────────────────────────────────
add_executable(${DMP_BENCHMARKS_POSTGRESQL}.Simple.SessionAsync
        benchmark_session_async.cpp
)
target_link_libraries(${DMP_BENCHMARKS_POSTGRESQL}.Simple.SessionAsync
        PRIVATE
        ${DMP_BENCHMARKS_POSTGRESQL}.Common
        Demiplane::Component::Database::PostgreSQL
)
