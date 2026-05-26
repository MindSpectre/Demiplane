// Flagship end-to-end burst-latency benchmark: a disruptor-fed producer feeds either sync
// thread-consumers or async coroutine-workers; measures the end-to-end latency decomposition
// and prints a unified sync-vs-async comparison. The runners live in sync/burst_sync.hpp and
// async/burst_async.hpp; the shared model/reporting/producer in common/burst_model.hpp; the run
// configuration in common/flagship_config.hpp. This TU is just CLI parsing + orchestration.

#include <chrono>
#include <cstddef>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "common/pool_bench_main.hpp"
#include "flagship_async.hpp"
#include "flagship_producer.hpp"
#include "flagship_sync.hpp"

using namespace bench::pool;

namespace {

    struct Args {
        std::int64_t bursts           = 20;
        std::int64_t burst_size       = -1;       // -1 = use Burst struct default
        std::string config            = "all";    // sync10 | async | all
        std::size_t workers_per_shard = 8;        // async: pre-spawned worker coroutines per shard
        std::string dispatch          = "all";    // async feed: pull | dedicated | channel | both | all
        std::string work              = "ws";     // async work unit: spin | ws
        std::string load              = "burst";  // arrival: burst | steady
        std::int64_t rps              = 125;      // steady: records/sec
        std::int64_t total            = 15000;    // steady: total records
        std::int64_t pool             = 128;      // pool connections
        std::int64_t wait_us          = 2;        // acquire_for timeout in us — raise to wait, not drop
        std::string out_path;                     // if non-empty, write results as JSON here
    };

    Args parse_args(const int argc, char** argv) {
        Args a{};
        for (int i = 1; i < argc; ++i) {
            if (const std::string_view s{argv[i]}; s.starts_with("--bursts=")) {
                a.bursts = std::stoll(std::string{s.substr(9)});
            } else if (s.starts_with("--burst-size=")) {
                a.burst_size = std::stoll(std::string{s.substr(13)});
            } else if (s.starts_with("--config=")) {
                a.config = std::string{s.substr(9)};
            } else if (s.starts_with("--workers=")) {
                a.workers_per_shard = static_cast<std::size_t>(std::stoull(std::string{s.substr(10)}));
            } else if (s.starts_with("--dispatch=")) {
                a.dispatch = std::string{s.substr(11)};
            } else if (s.starts_with("--work=")) {
                a.work = std::string{s.substr(7)};
            } else if (s.starts_with("--load=")) {
                a.load = std::string{s.substr(7)};
            } else if (s.starts_with("--rps=")) {
                a.rps = std::stoll(std::string{s.substr(6)});
            } else if (s.starts_with("--total=")) {
                a.total = std::stoll(std::string{s.substr(8)});
            } else if (s.starts_with("--pool=")) {
                a.pool = std::stoll(std::string{s.substr(7)});
            } else if (s.starts_with("--wait-us=")) {
                a.wait_us = std::stoll(std::string{s.substr(10)});
            } else if (s.starts_with("--out=")) {
                a.out_path = std::string{s.substr(6)};
            }
        }
        return a;
    }

    FlagshipConfig make_config(const Args& a) {
        FlagshipConfig cfg{};
        if (a.load == "steady") {
            cfg.arrival = Steady{a.rps, a.total};
        } else {
            Burst b{.count = a.bursts};
            if (a.burst_size > 0) {
                b.size = a.burst_size;
            }
            cfg.arrival = b;
        }
        cfg.workers_per_shard = a.workers_per_shard;
        cfg.work              = (a.work == "ws") ? Work::Ws : Work::Spin;
        cfg.pool_size         = static_cast<std::size_t>(a.pool);
        cfg.acquire_timeout   = std::chrono::microseconds{a.wait_us};
        return cfg;
    }

    void
    write_json(const std::string& path, const Args& a, const FlagshipConfig& cfg, const std::vector<Result>& rows) {
        std::ofstream f(path);
        f << "{\n";
        f << std::format("  \"load\": \"{}\",\n", a.load);
        f << std::format("  \"pool\": {},\n", a.pool);
        if (a.load == "steady") {
            f << std::format("  \"rps\": {},\n", a.rps);
            f << std::format("  \"total\": {},\n", a.total);
        } else {
            const auto& b          = std::get<Burst>(cfg.arrival);
            const auto cooldown_ms = std::chrono::duration_cast<std::chrono::milliseconds>(b.cooldown).count();
            const double eff_rps   = static_cast<double>(b.size) / (static_cast<double>(cooldown_ms) / 1000.0);
            f << std::format("  \"burst_size\": {},\n", b.size);
            f << std::format("  \"burst_count\": {},\n", b.count);
            f << std::format("  \"burst_cooldown_ms\": {},\n", cooldown_ms);
            f << std::format("  \"burst_effective_rps\": {:.0f},\n", eff_rps);
        }
        f << "  \"results\": [\n";
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            f << std::format("    {{\"name\": \"{}\", \"p50_us\": {:.3f}, \"p95_us\": {:.3f}, "
                             "\"p99_us\": {:.3f}, \"max_us\": {:.3f}, \"throughput_rps\": {:.0f}, "
                             "\"processed\": {}, \"produced\": {}, \"drained\": {}}}",
                             r.name,
                             r.p50_us,
                             r.p95_us,
                             r.p99_us,
                             r.max_us,
                             r.rps,
                             r.processed,
                             r.produced,
                             r.drained ? "true" : "false");
            f << (i + 1 < rows.size() ? ",\n" : "\n");
        }
        f << "  ]\n}\n";
    }

}  // namespace

int main(const int argc, char** argv) {
    const Args args    = parse_args(argc, argv);
    FlagshipConfig cfg = make_config(args);
    print_calibration();  // from TscClock via pool_bench_main.hpp
    std::vector<Result> rows;
    if (args.config == "sync10" || args.config == "all") {
        rows.push_back(run_sync("sync@10", 10, cfg));
    }
    if (args.config == "async" || args.config == "all") {
        const std::string& d = args.dispatch;
        if (d == "pull" || d == "both" || d == "all") {
            cfg.dispatch = Dispatch::Pull;
            rows.push_back(run_async("async-pull", cfg));
        }
        if (d == "dedicated" || d == "all") {
            cfg.dispatch = Dispatch::PullDedicated;
            rows.push_back(run_async("async-ded", cfg));
        }
        if (d == "channel" || d == "both" || d == "all") {
            cfg.dispatch = Dispatch::Channel;
            rows.push_back(run_async("async-chan", cfg));
        }
    }
    print_results(rows);
    if (!args.out_path.empty()) {
        write_json(args.out_path, args, cfg, rows);
    }
    return 0;
}
