#include <demiplane/chrono>
#include <query_library.hpp>
#include <query_expressions.hpp>
#include <postgres_dialect.hpp>
#include <postgres_sync_executor.hpp>

#include <libpq-fe.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <format>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

    constexpr std::size_t WARMUP_ITERATIONS = 100;
    constexpr std::size_t BENCHMARK_ITERATIONS = 1000;

    enum class BenchmarkMode { Raw, Compiled, Both };

    struct TimingStats {
        std::chrono::nanoseconds total{0};
        std::chrono::nanoseconds min{std::chrono::nanoseconds::max()};
        std::chrono::nanoseconds max{0};
        double avg_ns{0.0};
        double avg_us{0.0};
        double ops_per_sec{0.0};

        void calculate(const std::vector<std::chrono::nanoseconds>& timings) {
            total = std::accumulate(timings.begin(), timings.end(), std::chrono::nanoseconds{0});
            min = *std::min_element(timings.begin(), timings.end());
            max = *std::max_element(timings.begin(), timings.end());
            avg_ns = static_cast<double>(total.count()) / static_cast<double>(timings.size());
            avg_us = avg_ns / 1000.0;
            ops_per_sec = 1e9 / avg_ns;
        }
    };


    PGconn* connect_to_database() {
        const char* host     = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
        const char* port     = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5433";
        const char* dbname   = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "test_db";
        const char* user     = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "test_user";
        const char* password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "test_password";

        const std::string conninfo = std::format(
            "host={} port={} dbname={} user={} password={}",
            host, port, dbname, user, password);

        PGconn* conn = PQconnectdb(conninfo.c_str());

        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Failed to connect to PostgreSQL: " << PQerrorMessage(conn) << "\n";
            std::cerr << "Set environment variables: POSTGRES_HOST, POSTGRES_PORT, POSTGRES_DB, POSTGRES_USER, POSTGRES_PASSWORD\n";
            PQfinish(conn);
            return nullptr;
        }

        return conn;
    }

    void setup_tables(demiplane::db::postgres::SyncExecutor& executor) {
        // Drop and recreate tables for clean state
        (void)executor.execute("DROP TABLE IF EXISTS bench_users CASCADE");

        auto result = executor.execute(R"(
            CREATE TABLE bench_users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                age INTEGER NOT NULL,
                active BOOLEAN NOT NULL DEFAULT true
            )
        )");

        if (!result.is_success()) {
            std::cerr << "Failed to create bench_users table\n";
            return;
        }

        // Insert some test data
        for (int i = 1; i <= 100; ++i) {
            auto insert_result = executor.execute(std::format(
                "INSERT INTO bench_users (id, name, age, active) VALUES ({}, 'User{}', {}, {})",
                i, i, 20 + (i % 50), (i % 2 == 0) ? "true" : "false"));
            if (!insert_result.is_success()) {
                std::cerr << "Failed to insert test data\n";
            }
        }
    }

    void print_usage(const char* prog) {
        std::cout << "Usage: " << prog << " [--raw | --compiled | --both]\n\n";
        std::cout << "  --raw       Run only raw string queries\n";
        std::cout << "  --compiled  Run only compiled queries\n";
        std::cout << "  --both      Run both (interleaved, default)\n\n";
        std::cout << "For accurate comparison:\n";
        std::cout << "  1. Run: " << prog << " --raw\n";
        std::cout << "  2. Restart PostgreSQL: docker restart <container>\n";
        std::cout << "  3. Run: " << prog << " --compiled\n";
        std::cout << "  4. Compare the results\n";
    }

    void print_header(BenchmarkMode mode) {
        std::cout << "\n";
        std::cout << "PostgreSQL Query Execution Benchmark\n";
        std::cout << "====================================\n\n";
        std::cout << "Mode: ";
        switch (mode) {
            case BenchmarkMode::Raw: std::cout << "RAW STRING ONLY\n"; break;
            case BenchmarkMode::Compiled: std::cout << "COMPILED ONLY\n"; break;
            case BenchmarkMode::Both: std::cout << "BOTH (interleaved)\n"; break;
        }
        std::cout << "\nConfiguration:\n";
        std::cout << "  Warmup iterations:    " << WARMUP_ITERATIONS << "\n";
        std::cout << "  Benchmark iterations: " << BENCHMARK_ITERATIONS << "\n\n";
    }

    void print_single_result(std::string_view name, const TimingStats& stats) {
        std::cout << name << "\n";
        std::cout << std::string(50, '-') << "\n";
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "  Avg: " << stats.avg_us << " us\n";
        std::cout << "  Min: " << (static_cast<double>(stats.min.count()) / 1000.0) << " us\n";
        std::cout << "  Max: " << (static_cast<double>(stats.max.count()) / 1000.0) << " us\n";
        std::cout << "  Ops/sec: " << std::setprecision(0) << stats.ops_per_sec << "\n\n";
    }

}  // namespace

// Benchmark definitions - each returns TimingStats for the specified mode
struct BenchmarkDef {
    std::string_view name;
    std::string_view raw_sql;
};

std::vector<BenchmarkDef> get_benchmarks() {
    return {
        {"SELECT by ID", "SELECT id, name, age FROM bench_users WHERE id = 1"},
        {"SELECT with range", "SELECT id, name FROM bench_users WHERE age > 30"},
        {"COUNT(*) aggregate", "SELECT COUNT(*) FROM bench_users WHERE active = true"},
        {"UPDATE single row", "UPDATE bench_users SET age = 25 WHERE id = 1"},
        {"SELECT ORDER BY LIMIT", "SELECT id, name, age FROM bench_users ORDER BY age DESC LIMIT 10"},
        {"GROUP BY with COUNT", "SELECT active, COUNT(*) FROM bench_users GROUP BY active"},
    };
}

void run_raw_benchmarks(demiplane::db::postgres::SyncExecutor& executor) {
    auto benchmarks = get_benchmarks();

    std::cout << "Running RAW STRING benchmarks...\n\n";

    double total_avg = 0.0;

    for (const auto& bench : benchmarks) {
        std::vector<std::chrono::nanoseconds> timings;
        timings.reserve(BENCHMARK_ITERATIONS);

        // Warmup
        for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
            (void)executor.execute(std::string(bench.raw_sql));
        }

        // Benchmark
        for (std::size_t i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            auto elapsed = demiplane::chrono::Stopwatch<std::chrono::nanoseconds>::measure([&] {
                (void)executor.execute(std::string(bench.raw_sql));
            });
            timings.push_back(elapsed);
        }

        TimingStats stats;
        stats.calculate(timings);
        print_single_result(bench.name, stats);
        total_avg += stats.avg_us;
    }

    std::cout << std::string(50, '=') << "\n";
    std::cout << "TOTAL AVERAGE: " << std::fixed << std::setprecision(1)
              << (total_avg / static_cast<double>(benchmarks.size())) << " us\n";
    std::cout << std::string(50, '=') << "\n";
}

void run_compiled_benchmarks(
    demiplane::db::postgres::SyncExecutor& executor,
    demiplane::test::QueryLibrary& library) {

    using namespace demiplane::db;

    const auto& s = library.schemas();

    std::cout << "Running COMPILED benchmarks...\n\n";

    std::vector<std::pair<std::string_view, std::function<CompiledQuery()>>> compiled_queries = {
        {"SELECT by ID", [&] {
            auto query = select(s.users().id, s.users().name, s.users().age)
                             .from("bench_users")
                             .where(s.users().id == 1);
            return library.compiler().compile(query);
        }},
        {"SELECT with range", [&] {
            auto query = select(s.users().id, s.users().name)
                             .from("bench_users")
                             .where(s.users().age > 30);
            return library.compiler().compile(query);
        }},
        {"COUNT(*) aggregate", [&] {
            auto query = select(count(s.users().id))
                             .from("bench_users")
                             .where(s.users().active == true);
            return library.compiler().compile(query);
        }},
        {"UPDATE single row", [&] {
            auto query = update("bench_users")
                             .set("age", 25)
                             .where(s.users().id == 1);
            return library.compiler().compile(query);
        }},
        {"SELECT ORDER BY LIMIT", [&] {
            auto query = select(s.users().id, s.users().name, s.users().age)
                             .from("bench_users")
                             .order_by(desc(s.users().age))
                             .limit(10);
            return library.compiler().compile(query);
        }},
        {"GROUP BY with COUNT", [&] {
            auto query = select(s.users().active, count(s.users().id))
                             .from("bench_users")
                             .group_by(s.users().active);
            return library.compiler().compile(query);
        }},
    };

    double total_avg = 0.0;

    for (const auto& [name, make_query] : compiled_queries) {
        std::vector<std::chrono::nanoseconds> timings;
        timings.reserve(BENCHMARK_ITERATIONS);

        // Warmup
        for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
            auto compiled = make_query();
            (void)executor.execute(compiled);
        }

        // Benchmark
        for (std::size_t i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            auto elapsed = demiplane::chrono::Stopwatch<std::chrono::nanoseconds>::measure([&] {
                auto compiled = make_query();
                (void)executor.execute(compiled);
            });
            timings.push_back(elapsed);
        }

        TimingStats stats;
        stats.calculate(timings);
        print_single_result(name, stats);
        total_avg += stats.avg_us;
    }

    std::cout << std::string(50, '=') << "\n";
    std::cout << "TOTAL AVERAGE: " << std::fixed << std::setprecision(1)
              << (total_avg / static_cast<double>(compiled_queries.size())) << " us\n";
    std::cout << std::string(50, '=') << "\n";
}

int main(int argc, char* argv[]) {
    using namespace demiplane::db;
    using namespace demiplane::db::postgres;
    using namespace demiplane::test;

    BenchmarkMode mode = BenchmarkMode::Both;

    // Parse command line
    if (argc > 1) {
        if (std::strcmp(argv[1], "--raw") == 0) {
            mode = BenchmarkMode::Raw;
        } else if (std::strcmp(argv[1], "--compiled") == 0) {
            mode = BenchmarkMode::Compiled;
        } else if (std::strcmp(argv[1], "--both") == 0) {
            mode = BenchmarkMode::Both;
        } else if (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[1] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    print_header(mode);

    // Connect to database
    PGconn* conn = connect_to_database();
    if (!conn) {
        return 1;
    }

    std::cout << "Connected to PostgreSQL successfully.\n";

    // Create executor and query library
    SyncExecutor executor(conn);
    QueryLibrary library{std::make_unique<Dialect>()};

    // Setup test tables
    std::cout << "Setting up test tables...\n";
    setup_tables(executor);
    std::cout << "Test tables ready.\n\n";

    switch (mode) {
        case BenchmarkMode::Raw:
            run_raw_benchmarks(executor);
            break;

        case BenchmarkMode::Compiled:
            run_compiled_benchmarks(executor, library);
            break;

        case BenchmarkMode::Both:
            std::cout << "=== RAW STRING ===\n";
            run_raw_benchmarks(executor);
            std::cout << "\n=== COMPILED ===\n";
            run_compiled_benchmarks(executor, library);
            std::cout << "\nNOTE: For accurate comparison, run --raw and --compiled separately\n";
            std::cout << "with a PostgreSQL restart between runs.\n";
            break;
    }

    // Cleanup
    (void)executor.execute("DROP TABLE IF EXISTS bench_users CASCADE");
    PQfinish(conn);

    std::cout << "\nBenchmark completed.\n";

    return 0;
}
