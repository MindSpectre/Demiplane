#include <cstddef>
#include <cstring>
#include <format>
#include <functional>
#include <string>

#include <benchmark/benchmark.h>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <libpq-fe.h>

#include "bench_config.hpp"
#include "bench_constants.hpp"
#include "bench_fixture.hpp"
#include "bench_scenarios.hpp"
#include "bench_workload.hpp"

namespace {

    // Lazy per-thread libpq connection. Initialized on first use inside a
    // thread_pool worker, destroyed at thread exit via thread_local destructor.
    struct ThreadConn {
        PGconn* conn = nullptr;

        PGconn* get() {
            if (conn == nullptr) {
                const auto connstr = bench::pg::make_connection_string();
                conn               = PQconnectdb(connstr.c_str());
                if (PQstatus(conn) != CONNECTION_OK) {
                    PQfinish(conn);
                    conn = nullptr;
                }
            }
            return conn;
        }

        ~ThreadConn() {
            if (conn != nullptr) {
                PQfinish(conn);
            }
        }
    };

    PGconn* thread_conn() {
        thread_local ThreadConn tc;
        return tc.get();
    }

    class LibpqBackend {
    public:
        explicit LibpqBackend(const bench::pg::Scenario& scenario)
            : pool_{bench::pg::control_pool_size(scenario)},
              query_{bench::pg::query_for(scenario)} {
        }

        ~LibpqBackend() {
            pool_.join();
        }

        LibpqBackend(const LibpqBackend&)            = delete;
        LibpqBackend& operator=(const LibpqBackend&) = delete;

        void post_task(std::function<void()> f) {
            boost::asio::post(pool_, std::move(f));
        }

        bool run_query(int id) {
            auto* conn = thread_conn();
            if (conn == nullptr) {
                return false;
            }
            const auto id_str    = std::to_string(id);
            const char* values[] = {id_str.c_str()};
            const int lengths[]  = {static_cast<int>(id_str.size())};
            const int formats[]  = {0};
            PGresult* res        = PQexecParams(conn, query_, 1, nullptr, values, lengths, formats, 0);
            const bool ok =
                res != nullptr && (PQresultStatus(res) == PGRES_TUPLES_OK || PQresultStatus(res) == PGRES_COMMAND_OK);
            PQclear(res);
            return ok;
        }

    private:
        boost::asio::thread_pool pool_;
        const char* query_;
    };

    void RunScenario(::benchmark::State& state, std::size_t scenario_index) {
        const auto& scenario = bench::pg::SCENARIOS[scenario_index];
        LibpqBackend backend{scenario};
        bench::pg::run_sync_workload(state, scenario, backend);
    }

    int RegisterAll() {
        for (std::size_t i = 0; i < bench::pg::SCENARIOS.size(); ++i) {
            const auto name = std::format("BM_LibPQ_{}", bench::pg::SCENARIOS[i].name);
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
