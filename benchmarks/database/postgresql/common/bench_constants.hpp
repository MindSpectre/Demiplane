#pragma once

#include <array>
#include <cstddef>

#include <benchmark/benchmark.h>

namespace bench::pg {

    static constexpr auto BENCH_QUERY = "SELECT id FROM bench WHERE id = $1";

    // Slow variant for concurrency benchmarks: adds ~10ms of server-side work
    // so queries actually hold a pool slot long enough for realistic contention.
    // Without this, queries return in ~150μs and workers overflow the dispatch
    // loop faster than the pool can service them.
    //
    // Note: pg_sleep must be in FROM (not EXISTS / scalar subquery) or the
    // planner hoists it out as a constant. Using it as a cross-join with a
    // volatile function forces per-execution evaluation.
    static constexpr auto SLOW_BENCH_QUERY = "SELECT b.id FROM bench b, pg_sleep(0.01) s WHERE b.id = $1";

    static constexpr auto CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS bench (
            id INTEGER PRIMARY KEY
        )
    )";

    static constexpr auto INSERT_DATA =
        "INSERT INTO bench (id) SELECT g FROM generate_series(1, 1000) AS g ON CONFLICT DO NOTHING";

    static constexpr auto ANALYZE_BENCH = "ANALYZE bench";

    static constexpr auto DROP_TABLE = "DROP TABLE IF EXISTS bench CASCADE";

    static constexpr int MAX_ID = 1000;

    static constexpr std::array CONCURRENCY_LEVELS = {1, 2, 4, 8, 16, 32};

    static constexpr int QUERIES_PER_TASK = 100;

    inline void add_concurrency_args(::benchmark::Benchmark* b) {
        for (const auto level : CONCURRENCY_LEVELS) {
            b->Arg(level);
        }
    }

}  // namespace bench::pg
