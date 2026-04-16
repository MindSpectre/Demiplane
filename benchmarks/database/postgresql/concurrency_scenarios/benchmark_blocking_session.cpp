#include <cstddef>
#include <format>
#include <memory>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <postgres_session.hpp>

#include "bench_constants.hpp"
#include "bench_fixture.hpp"
#include "bench_scenarios.hpp"
#include "bench_workload.hpp"

namespace {

    using demiplane::db::postgres::BlockingSession;
    using demiplane::db::postgres::ConnectionConfig;
    using demiplane::db::postgres::PoolConfig;

    class BlockingBackend {
    public:
        explicit BlockingBackend(const bench::pg::Scenario& scenario)
            : session_{ConnectionConfig::testing(), make_pool_config(scenario)},
              work_guard_{boost::asio::make_work_guard(io_)} {
            io_threads_.reserve(scenario.executor_threads);
            for (std::size_t i = 0; i < scenario.executor_threads; ++i) {
                io_threads_.emplace_back([this] { io_.run(); });
            }
            query_ = bench::pg::query_for(scenario);
        }

        ~BlockingBackend() {
            if (running_) {
                wait_and_shutdown();
            }
        }

        BlockingBackend(const BlockingBackend&)            = delete;
        BlockingBackend& operator=(const BlockingBackend&) = delete;

        boost::asio::io_context& io() noexcept {
            return io_;
        }

        boost::asio::awaitable<bool> run_query(int id, boost::asio::any_io_executor e) {
            auto acquired = session_.try_with_async(e);
            if (!acquired.is_success()) {
                co_return false;
            }
            auto ae     = std::move(acquired).value();
            auto result = co_await ae.execute(std::string{query_}, id);
            benchmark::DoNotOptimize(result);
            co_return result.is_success();
        }

        void wait_and_shutdown() {
            if (!running_) {
                return;
            }
            running_ = false;
            work_guard_.reset();
            io_.stop();
            for (auto& t : io_threads_) {
                if (t.joinable()) {
                    t.join();
                }
            }
            session_.shutdown();
        }

    private:
        static PoolConfig make_pool_config(const bench::pg::Scenario& s) {
            return PoolConfig::Builder{}.capacity(s.connections).min_connections(s.connections).finalize();
        }

        BlockingSession session_;
        boost::asio::io_context io_{};
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
        std::vector<std::thread> io_threads_{};
        const char* query_ = bench::pg::BENCH_QUERY;
        bool running_      = true;
    };

    void RunScenario(::benchmark::State& state, std::size_t scenario_index) {
        const auto& scenario = bench::pg::SCENARIOS[scenario_index];
        BlockingBackend backend{scenario};
        bench::pg::run_async_workload(state, scenario, backend);
        backend.wait_and_shutdown();
    }

    int RegisterAll() {
        for (std::size_t i = 0; i < bench::pg::SCENARIOS.size(); ++i) {
            const auto name = std::format("BM_Blocking_{}", bench::pg::SCENARIOS[i].name);
            auto* b         = ::benchmark::RegisterBenchmark(name, [i](::benchmark::State& st) { RunScenario(st, i); });
            bench::pg::register_workers(b);
        }
        return 0;
    }

    const int kRegistered = RegisterAll();

}  // namespace

int main(int argc, char** argv) {
    if (auto* c = bench::pg::connect(); c != nullptr) {
        bench::pg::setup_table(c);
        PQfinish(c);
    }

    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();

    if (auto* c = bench::pg::connect(); c != nullptr) {
        bench::pg::teardown_table(c);
        PQfinish(c);
    }
    return 0;
}
