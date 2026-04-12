#include <latch>
#include <memory>
#include <mutex>
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

    // Connections available in the pool — same as the async benchmark
    constexpr std::size_t CONNECTIONS = 8;

    constexpr std::size_t THREADS = 4;

    // Queries per "request" — same as the async benchmark
    constexpr int QUERIES_PER_REQUEST = 4;

    inline void add_worker_args(::benchmark::Benchmark* b) {
        for (const auto workers : {8, 16, 32, 64, 128, 256, 512, 1024}) {
            b->Arg(workers);
        }
    }

    class PqxxFixture : public benchmark::Fixture {
    public:
        void SetUp(const benchmark::State& /*state*/) override {
            auto* conn = bench::pg::connect();
            if (conn != nullptr) {
                bench::pg::setup_table(conn);
                PQfinish(conn);
            }

            const auto connstr = bench::pg::make_connection_string();
            connections_.reserve(CONNECTIONS);
            for (std::size_t i = 0; i < CONNECTIONS; ++i) {
                connections_.push_back(std::make_unique<pqxx::connection>(connstr));
            }
        }

        void TearDown(const benchmark::State& /*state*/) override {
            connections_.clear();

            auto* conn = bench::pg::connect();
            if (conn != nullptr) {
                bench::pg::teardown_table(conn);
                PQfinish(conn);
            }
        }

        std::vector<std::unique_ptr<pqxx::connection>> connections_;
        std::vector<std::mutex> conn_mutexes_{CONNECTIONS};
    };

    BENCHMARK_DEFINE_F(PqxxFixture, BM_Pqxx)(benchmark::State& state) {
        const auto workers          = static_cast<std::size_t>(state.range(0));
        const auto queries_per_task = QUERIES_PER_REQUEST;

        std::vector<bench::pg::LatencyCollector> collectors(workers);

        // Thread pool with kThreads workers (< kConnections).
        // Synchronous pqxx can only drive kThreads queries at a time regardless
        // of how many connections exist — threads are the bottleneck.
        boost::asio::thread_pool pool{THREADS};

        for (GEARS_UNUSED_VAR : state) {
            for (auto& c : collectors) {
                c.clear();
            }
            std::latch done{static_cast<std::ptrdiff_t>(workers)};

            for (std::size_t i = 0; i < workers; ++i) {
                boost::asio::post(pool, [&, i] {
                    thread_local std::mt19937 rng{std::random_device{}()};
                    std::uniform_int_distribution<int> dist{1, bench::pg::MAX_ID};

                    for (int q = 0; q < queries_per_task; ++q) {
                        const auto id       = dist(rng);
                        const auto conn_idx = i % CONNECTIONS;

                        bench::pg::timed_op(collectors[i], [&] {
                            std::lock_guard lock{conn_mutexes_[conn_idx]};
                            pqxx::nontransaction txn{*connections_[conn_idx]};
                            auto result = txn.query_value<int>(bench::pg::SLOW_BENCH_QUERY, pqxx::params{id});
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
        state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * static_cast<std::int64_t>(workers) *
                                static_cast<std::int64_t>(queries_per_task));
        bench::pg::merge_latency(state, collectors);
    }

    BENCHMARK_REGISTER_F(PqxxFixture, BM_Pqxx)->Apply(add_worker_args)->UseRealTime();

}  // namespace

int main(int argc, char** argv) {
    benchmark::MaybeReenterWithoutASLR(argc, argv);
    char arg0_default[] = "benchmark";
    char* args_default  = reinterpret_cast<char*>(arg0_default);
    if (!argv) {
        argc = 1;
        argv = &args_default;
    }
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv))
        return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0; } int main(int, char**);
