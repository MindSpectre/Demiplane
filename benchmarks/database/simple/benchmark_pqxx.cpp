#include <latch>
#include <memory>
#include <pqxx/pqxx>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <gears_macros.hpp>

#include "bench_config.hpp"
#include "bench_constants.hpp"
#include "bench_fixture.hpp"
#include "bench_latency.hpp"
namespace {

    class PqxxFixture : public benchmark::Fixture {
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

    BENCHMARK_DEFINE_F(PqxxFixture, BM_Pqxx)(benchmark::State& state) {
        const auto concurrency      = static_cast<std::size_t>(state.range(0));
        const auto queries_per_task = bench::pg::QUERIES_PER_TASK;
        const auto connstr          = bench::pg::make_connection_string();

        // Create per-thread connections outside timing loop
        std::vector<std::unique_ptr<pqxx::connection>> connections;
        connections.reserve(concurrency);
        for (std::size_t i = 0; i < concurrency; ++i) {
            try {
                connections.push_back(std::make_unique<pqxx::connection>(connstr));
            } catch (const std::exception& e) {
                state.SkipWithError(e.what());
                return;
            }
        }

        boost::asio::thread_pool pool{concurrency};
        std::vector<bench::pg::LatencyCollector> collectors(concurrency);

        for (GEARS_UNUSED_VAR : state) {
            std::latch done{static_cast<std::ptrdiff_t>(concurrency)};

            for (std::size_t i = 0; i < concurrency; ++i) {
                boost::asio::post(pool, [&, i] {
                    thread_local std::mt19937 rng{std::random_device{}()};
                    std::uniform_int_distribution<int> dist{1, bench::pg::MAX_ID};

                    for (int q = 0; q < queries_per_task; ++q) {
                        const auto id = dist(rng);

                        bench::pg::timed_op(collectors[i], [&] {
                            pqxx::nontransaction txn{*connections[i]};
                            auto result = txn.query_value<int>(bench::pg::BENCH_QUERY, pqxx::params{id});
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
    }

    BENCHMARK_REGISTER_F(PqxxFixture, BM_Pqxx)->Apply(bench::pg::add_concurrency_args)->UseRealTime();

}  // namespace

BENCHMARK_MAIN();
