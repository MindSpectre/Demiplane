#pragma once

namespace bench::pg {

    static constexpr auto BENCH_QUERY = "SELECT id FROM bench WHERE id = $1";

    // Slow variant for concurrency benchmarks: adds ~10ms of server-side work
    // so queries actually hold a pool slot long enough for realistic contention.
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

}  // namespace bench::pg
