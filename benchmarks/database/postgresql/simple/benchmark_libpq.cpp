#include <cstring>
#include <latch>
#include <random>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <gears_macros.hpp>
#include <libpq-fe.h>

#include "bench_constants.hpp"
#include "bench_fixture.hpp"
#include "bench_latency.hpp"
namespace {

    class LibPQFixture : public benchmark::Fixture {
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

    BENCHMARK_DEFINE_F(LibPQFixture, BM_LibPQ)(benchmark::State& state) {
        const auto concurrency      = static_cast<std::size_t>(state.range(0));
        const auto queries_per_task = bench::pg::QUERIES_PER_TASK;

        // Create per-thread connections outside timing loop
        std::vector<PGconn*> connections(concurrency);
        for (auto& conn : connections) {
            conn = bench::pg::connect();
            if (conn == nullptr) {
                state.SkipWithError("Failed to connect to PostgreSQL");
                return;
            }
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
                        const auto id     = dist(rng);
                        const auto id_str = std::to_string(id);

                        const char* values[] = {id_str.c_str()};
                        const int lengths[]  = {static_cast<int>(id_str.size())};
                        const int formats[]  = {0};

                        bench::pg::timed_op(collectors[i], [&] {
                            PGresult* res = PQexecParams(
                                connections[i], bench::pg::BENCH_QUERY, 1, nullptr, values, lengths, formats, 0);
                            PQclear(res);
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

        // Cleanup
        for (auto* conn : connections) {
            PQfinish(conn);
        }
    }

    BENCHMARK_REGISTER_F(LibPQFixture, BM_LibPQ)->Apply(bench::pg::add_concurrency_args)->UseRealTime();

}  // namespace

BENCHMARK_MAIN();
