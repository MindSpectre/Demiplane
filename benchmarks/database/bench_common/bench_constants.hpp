#pragma once

#include <array>
#include <cstddef>

#include <benchmark/benchmark.h>

namespace bench::pg {

    static constexpr auto BENCH_QUERY = "SELECT id FROM bench WHERE id = $1";

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
