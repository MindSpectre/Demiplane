#include <latch>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <gears_macros.hpp>
#include <libpq-fe.h>
#include <postgres_sync_executor.hpp>

#include "bench_constants.hpp"
#include "bench_fixture.hpp"
#include "bench_latency.hpp"

namespace {

    using demiplane::db::postgres::SyncExecutor;

    class SyncExecutorFixture : public benchmark::Fixture {
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

    BENCHMARK_DEFINE_F(SyncExecutorFixture, BM_SyncExecutor)(benchmark::State& state) {
        const auto concurrency      = static_cast<std::size_t>(state.range(0));
        const auto queries_per_task = bench::pg::QUERIES_PER_TASK;

        // Create per-thread connections and executors outside timing loop
        std::vector<PGconn*> connections(concurrency);
        std::vector<SyncExecutor> executors;
        executors.reserve(concurrency);

        for (std::size_t i = 0; i < concurrency; ++i) {
            connections[i] = bench::pg::connect();
            if (connections[i] == nullptr) {
                state.SkipWithError("Failed to connect to PostgreSQL");
                // Cleanup already-created connections
                for (std::size_t j = 0; j < i; ++j) {
                    PQfinish(connections[j]);
                }
                return;
            }
            executors.emplace_back(connections[i]);
        }

        boost::asio::thread_pool pool{concurrency};
        std::vector<bench::pg::LatencyCollector> collectors(concurrency);

        for (GEARS_UNUSED_VAR : state) {
            for (auto& c : collectors) {
                c.clear();
            }
            std::latch done{static_cast<std::ptrdiff_t>(concurrency)};

            for (std::size_t i = 0; i < concurrency; ++i) {
                boost::asio::post(pool, [&, i] {
                    thread_local std::mt19937 rng{std::random_device{}()};
                    std::uniform_int_distribution<int> dist{1, bench::pg::MAX_ID};

                    for (int q = 0; q < queries_per_task; ++q) {
                        const auto id = dist(rng);

                        bench::pg::timed_op(collectors[i], [&] {
                            auto result = executors[i].execute(bench::pg::BENCH_QUERY, id);
                            benchmark::DoNotOptimize(result);
                        });
                    }

                    done.count_down();
                });
            }

            done.wait();
        }

        pool.join();

        // Report metrics
        state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * static_cast<std::int64_t>(concurrency) *
                                static_cast<std::int64_t>(queries_per_task));
        bench::pg::merge_latency(state, collectors);

        // Cleanup: clear executors first (they don't own the connections), then finish connections
        executors.clear();
        for (auto* conn : connections) {
            PQfinish(conn);
        }
    }

    BENCHMARK_REGISTER_F(SyncExecutorFixture, BM_SyncExecutor)->Apply(bench::pg::add_concurrency_args)->UseRealTime();

}  // namespace

BENCHMARK_MAIN();
