#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <pqxx/pqxx>

#include <benchmark/benchmark.h>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include "bench_config.hpp"
#include "bench_constants.hpp"
#include "bench_fixture.hpp"
#include "bench_scenarios.hpp"
#include "bench_workload.hpp"

namespace {

    struct ThreadConn {
        std::unique_ptr<pqxx::connection> conn;

        pqxx::connection* get() {
            if (!conn) {
                try {
                    conn = std::make_unique<pqxx::connection>(bench::pg::make_connection_string());
                } catch (const std::exception&) {
                    conn.reset();
                }
            }
            return conn.get();
        }
    };

    pqxx::connection* thread_conn() {
        thread_local ThreadConn tc;
        return tc.get();
    }

    class PqxxBackend {
    public:
        explicit PqxxBackend(const bench::pg::Scenario& scenario)
            : pool_{bench::pg::control_pool_size(scenario)},
              query_{bench::pg::query_for(scenario)} {
        }

        ~PqxxBackend() {
            pool_.join();
        }

        PqxxBackend(const PqxxBackend&)            = delete;
        PqxxBackend& operator=(const PqxxBackend&) = delete;

        void post_task(std::function<void()> f) {
            boost::asio::post(pool_, std::move(f));
        }

        bool run_query(int id) {
            auto* c = thread_conn();
            if (c == nullptr) {
                return false;
            }
            try {
                pqxx::nontransaction txn{*c};
                auto result = txn.query_value<int>(query_, pqxx::params{id});
                benchmark::DoNotOptimize(result);
                return true;
            } catch (const std::exception&) {
                return false;
            }
        }

    private:
        boost::asio::thread_pool pool_;
        const char* query_;
    };

    void RunScenario(::benchmark::State& state, std::size_t scenario_index) {
        const auto& scenario = bench::pg::SCENARIOS[scenario_index];
        PqxxBackend backend{scenario};
        bench::pg::run_sync_workload(state, scenario, backend);
    }

    int RegisterAll() {
        for (std::size_t i = 0; i < bench::pg::SCENARIOS.size(); ++i) {
            const auto name = std::format("BM_Pqxx_{}", bench::pg::SCENARIOS[i].name);
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
