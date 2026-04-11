#include <atomic>
#include <latch>
#include <random>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <libpq-fe.h>
#include <postgres_async_executor.hpp>

#include "bench_constants.hpp"
#include "bench_fixture.hpp"
#include "bench_latency.hpp"

namespace {

    using demiplane::db::postgres::AsyncExecutor;

    class AsyncExecutorFixture : public benchmark::Fixture {
    public:
        void SetUp(const benchmark::State& /*state*/) override {
            auto* conn = bench::pg::connect();
            if (conn != nullptr) {
                bench::pg::setup_table(conn);
                PQfinish(conn);
            }
        }

        void TearDown(const benchmark::State& /*state*/) override {
            auto* conn = bench::pg::connect();
            if (conn != nullptr) {
                bench::pg::teardown_table(conn);
                PQfinish(conn);
            }
        }
    };

    BENCHMARK_DEFINE_F(AsyncExecutorFixture, BM_AsyncExecutor)(benchmark::State& state) {
        const auto concurrency      = static_cast<std::size_t>(state.range(0));
        const auto queries_per_task = bench::pg::QUERIES_PER_TASK;

        // Create connections and set non-blocking
        std::vector<PGconn*> connections(concurrency);
        for (std::size_t i = 0; i < concurrency; ++i) {
            connections[i] = bench::pg::connect();
            if (connections[i] == nullptr) {
                state.SkipWithError("Failed to connect to PostgreSQL");
                for (std::size_t j = 0; j < i; ++j) {
                    PQfinish(connections[j]);
                }
                return;
            }
        }

        std::vector<bench::pg::LatencyCollector> collectors(concurrency);

        // Create io_context and thread pool to drive it
        boost::asio::io_context io;
        auto work_guard = boost::asio::make_work_guard(io);

        std::vector<std::thread> io_threads;
        io_threads.reserve(concurrency);
        for (std::size_t i = 0; i < concurrency; ++i) {
            io_threads.emplace_back([&io] { io.run(); });
        }

        // Create executors (one per connection)
        std::vector<AsyncExecutor> executors;
        executors.reserve(concurrency);
        for (std::size_t i = 0; i < concurrency; ++i) {
            executors.emplace_back(connections[i], io.get_executor());
            if (!executors.back().valid()) {
                state.SkipWithError("Failed to create async executor");
                work_guard.reset();
                for (auto& t : io_threads) {
                    t.join();
                }
                for (auto* conn : connections) {
                    if (conn != nullptr) {
                        PQfinish(conn);
                    }
                }
                return;
            }
        }

        for (GEARS_UNUSED_VAR : state) {
            std::latch done{static_cast<std::ptrdiff_t>(concurrency)};

            for (std::size_t i = 0; i < concurrency; ++i) {
                boost::asio::co_spawn(
                    io,
                    [&, i]() -> boost::asio::awaitable<void> {
                        thread_local std::mt19937 rng{std::random_device{}()};
                        std::uniform_int_distribution<int> dist{1, bench::pg::MAX_ID};

                        for (int q = 0; q < queries_per_task; ++q) {
                            const auto id = dist(rng);

                            const auto start   = std::chrono::steady_clock::now();
                            auto result        = co_await executors[i].execute(std::string{bench::pg::BENCH_QUERY}, id);
                            const auto elapsed = std::chrono::steady_clock::now() - start;

                            benchmark::DoNotOptimize(result);
                            collectors[i].record(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
                        }

                        done.count_down();
                    },
                    boost::asio::detached);
            }

            done.wait();
        }

        // Shutdown
        work_guard.reset();
        io.stop();
        for (auto& t : io_threads) {
            t.join();
        }

        // Report metrics
        state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * static_cast<std::int64_t>(concurrency) *
                                static_cast<std::int64_t>(queries_per_task));
        bench::pg::merge_latency(state, collectors);

        // Cleanup: executors first, then connections
        executors.clear();
        for (auto* conn : connections) {
            PQfinish(conn);
        }
    }

    BENCHMARK_REGISTER_F(AsyncExecutorFixture, BM_AsyncExecutor)->Apply(bench::pg::add_concurrency_args)->UseRealTime();

}  // namespace

BENCHMARK_MAIN();
