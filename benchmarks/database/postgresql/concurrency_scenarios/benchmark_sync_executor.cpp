#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <string>

#include <benchmark/benchmark.h>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <libpq-fe.h>
#include <postgres_sync_executor.hpp>

#include "bench_config.hpp"
#include "bench_constants.hpp"
#include "bench_fixture.hpp"
#include "bench_scenarios.hpp"
#include "bench_workload.hpp"

namespace {

    using demiplane::db::postgres::SyncExecutor;

    // thread_local owns a raw PGconn* and a SyncExecutor wrapping it.
    // PGconn lifetime is longer than SyncExecutor's; destruction order tears
    // down SyncExecutor first.
    struct ThreadExecutor {
        PGconn* conn = nullptr;
        std::unique_ptr<SyncExecutor> sync_exec;

        SyncExecutor* get() {
            if (sync_exec) {
                return sync_exec.get();
            }
            conn = bench::pg::connect();
            if (conn == nullptr) {
                return nullptr;
            }
            sync_exec = std::make_unique<SyncExecutor>(conn);
            return sync_exec.get();
        }

        ~ThreadExecutor() {
            sync_exec.reset();
            if (conn != nullptr) {
                PQfinish(conn);
            }
        }
    };

    SyncExecutor* thread_sync_executor() {
        thread_local ThreadExecutor te;
        return te.get();
    }

    class SyncExecutorBackend {
    public:
        explicit SyncExecutorBackend(const bench::pg::Scenario& scenario)
            : pool_{bench::pg::control_pool_size(scenario)},
              query_{bench::pg::query_for(scenario)} {
        }

        ~SyncExecutorBackend() {
            pool_.join();
        }

        SyncExecutorBackend(const SyncExecutorBackend&)            = delete;
        SyncExecutorBackend& operator=(const SyncExecutorBackend&) = delete;

        void post_task(std::function<void()> f) {
            boost::asio::post(pool_, std::move(f));
        }

        bool run_query(int id) {
            auto* se = thread_sync_executor();
            if (se == nullptr) {
                return false;
            }
            auto result = se->execute(std::string{query_}, id);
            benchmark::DoNotOptimize(result);
            return result.is_success();
        }

    private:
        boost::asio::thread_pool pool_;
        const char* query_;
    };

    void RunScenario(::benchmark::State& state, std::size_t scenario_index) {
        const auto& scenario = bench::pg::SCENARIOS[scenario_index];
        SyncExecutorBackend backend{scenario};
        bench::pg::run_sync_workload(state, scenario, backend);
    }

    int RegisterAll() {
        for (std::size_t i = 0; i < bench::pg::SCENARIOS.size(); ++i) {
            const auto name = std::format("BM_SyncExecutor_{}", bench::pg::SCENARIOS[i].name);
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
