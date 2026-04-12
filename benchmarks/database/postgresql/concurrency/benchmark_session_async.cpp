
#include <atomic>
#include <latch>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <gears_macros.hpp>
#include <postgres_session.hpp>

#include "bench_config.hpp"
#include "bench_constants.hpp"
#include "bench_fixture.hpp"
#include "bench_latency.hpp"

namespace {

    using demiplane::db::postgres::ConnectionConfig;
    using demiplane::db::postgres::PoolConfig;
    using demiplane::db::postgres::Session;
    using demiplane::db::postgres::SslMode;

    // Connections available in the pool — saturation ceiling on concurrent queries
    constexpr std::size_t CONNECTIONS = 8;

    // I/O threads driving the coroutine scheduler — intentionally scarce
    // (fewer threads than connections) so async actually gets a chance to multiplex:
    // while one coroutine awaits network I/O, its thread picks up another that has
    // a free connection. With threads == connections, async provides no benefit
    // over a straight mutex pool.
    constexpr std::size_t THREADS = 4;

    // Queries per "request" — models a typical HTTP handler doing 3-4 DB calls
    constexpr int QUERIES_PER_REQUEST = 4;


    PoolConfig make_pool_config() {
        return PoolConfig::Builder{}.capacity(CONNECTIONS).min_connections(CONNECTIONS).finalize();
    }

    inline void add_worker_args(::benchmark::Benchmark* b) {
        for (const auto workers : {8, 16, 32, 64, 128, 256, 512, 1024}) {
            b->Arg(workers);
        }
    }

    // =========================================================================
    // Async: N coroutines on io_context, sharing 8 connections
    // While coroutine A waits for DB response, thread picks up coroutine B
    // =========================================================================

    class AsyncFixture : public benchmark::Fixture {
    public:
        void SetUp(const benchmark::State& /*state*/) override {
            auto* conn = bench::pg::connect();
            if (conn != nullptr) {
                bench::pg::setup_table(conn);
                PQfinish(conn);
            }

            session_ = std::make_unique<Session>(ConnectionConfig::testing(), make_pool_config());
        }

        void TearDown(const benchmark::State& /*state*/) override {
            if (session_) {
                session_->shutdown();
                session_.reset();
            }

            auto* conn = bench::pg::connect();
            if (conn != nullptr) {
                bench::pg::teardown_table(conn);
                PQfinish(conn);
            }
        }

        std::unique_ptr<Session> session_;
    };

    BENCHMARK_DEFINE_F(AsyncFixture, BM_SessionAsync)(benchmark::State& state) {
        if (!session_) {
            state.SkipWithError("Failed to create session");
            return;
        }

        const auto workers          = static_cast<std::size_t>(state.range(0));
        const auto queries_per_task = QUERIES_PER_REQUEST;

        std::vector<bench::pg::LatencyCollector> collectors(workers);
        std::atomic<std::int64_t> completed_ops{0};
        std::atomic<std::int64_t> skipped_ops{0};

        // io_context with kThreads (< kConnections) so async actually multiplexes
        boost::asio::io_context io;
        auto work_guard = boost::asio::make_work_guard(io);

        std::vector<std::thread> io_threads;
        io_threads.reserve(THREADS);
        for (std::size_t i = 0; i < THREADS; ++i) {
            io_threads.emplace_back([&io] { io.run(); });
        }

        for (GEARS_UNUSED_VAR : state) {
            for (auto& c : collectors) {
                c.clear();
            }
            std::latch done{static_cast<std::ptrdiff_t>(workers)};

            for (std::size_t i = 0; i < workers; ++i) {
                boost::asio::co_spawn(
                    io,
                    [&, i]() -> boost::asio::awaitable<void> {
                        thread_local std::mt19937 rng{std::random_device{}()};
                        std::uniform_int_distribution<int> dist{1, bench::pg::MAX_ID};

                        for (int q = 0; q < queries_per_task; ++q) {
                            const auto id = dist(rng);

                            const auto start = std::chrono::steady_clock::now();
                            auto exec_result = session_->with_async(co_await boost::asio::this_coro::executor);

                            if (!exec_result.is_success()) {
                                // Pool exhausted beyond timeout — skip this query entirely.
                                // Not counted as an operation; not recorded in latency samples.
                                skipped_ops.fetch_add(1, std::memory_order_relaxed);
                                continue;
                            }

                            auto exec          = std::move(exec_result).value();
                            auto result        = co_await exec.execute(std::string{bench::pg::SLOW_BENCH_QUERY}, id);
                            const auto elapsed = std::chrono::steady_clock::now() - start;

                            benchmark::DoNotOptimize(result);
                            collectors[i].record(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
                            completed_ops.fetch_add(1, std::memory_order_relaxed);
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

        // Report only ops that actually ran a query
        state.SetItemsProcessed(completed_ops.load(std::memory_order_relaxed));
        state.counters["skipped"] = static_cast<double>(skipped_ops.load(std::memory_order_relaxed));
        bench::pg::merge_latency(state, collectors);
    }

    BENCHMARK_REGISTER_F(AsyncFixture, BM_SessionAsync)->Apply(add_worker_args)->UseRealTime();

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
    return 0;
}
int main(int, char**);
