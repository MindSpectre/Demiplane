#include <latch>
#include <memory>
#include <random>

#include <benchmark/benchmark.h>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
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

    ConnectionConfig make_connection_config() {
        const char* host     = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
        const char* port     = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5433";
        const char* dbname   = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "test_db";
        const char* user     = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "test_user";
        const char* password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "test_password";

        return ConnectionConfig::Builder{}
            .host(host)
            .port(std::string{port})
            .dbname(dbname)
            .user(user)
            .password(password)
            .ssl_mode(SslMode::DISABLE)
            .finalize();
    }

    PoolConfig make_pool_config(std::size_t capacity) {
        return PoolConfig::Builder{}.capacity(capacity).min_connections(capacity).finalize();
    }

    class SessionFixture : public benchmark::Fixture {
    public:
        void SetUp(const benchmark::State& state) override {
            auto* conn = bench::pg::connect();
            if (conn != nullptr) {
                bench::pg::setup_table(conn);
                PQfinish(conn);
            }

            const auto concurrency = static_cast<std::size_t>(state.range(0));
            session_               = std::make_unique<Session>(make_connection_config(), make_pool_config(concurrency));
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

    BENCHMARK_DEFINE_F(SessionFixture, BM_Session)(benchmark::State& state) {
        if (!session_) {
            state.SkipWithError("Failed to create session");
            return;
        }

        const auto concurrency      = static_cast<std::size_t>(state.range(0));
        const auto queries_per_task = bench::pg::QUERIES_PER_TASK;

        boost::asio::thread_pool pool{concurrency};
        std::vector<bench::pg::LatencyCollector> collectors(concurrency);

        for (auto _ : state) {
            std::latch done{static_cast<std::ptrdiff_t>(concurrency)};

            for (std::size_t i = 0; i < concurrency; ++i) {
                boost::asio::post(pool, [&, i] {
                    thread_local std::mt19937 rng{std::random_device{}()};
                    std::uniform_int_distribution<int> dist{1, bench::pg::MAX_ID};

                    for (int q = 0; q < queries_per_task; ++q) {
                        const auto id = dist(rng);

                        bench::pg::timed_op(collectors[i], [&] {
                            auto exec   = session_->with_sync();
                            auto result = exec.execute(bench::pg::BENCH_QUERY, id);
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

    BENCHMARK_REGISTER_F(SessionFixture, BM_Session)->Apply(bench::pg::add_concurrency_args)->UseRealTime();

}  // namespace

BENCHMARK_MAIN();
