#pragma once
#include <demiplane/ink>
#include <demiplane/multithread>
#include <iostream>

#include <tsc_clock.hpp>

#include "common/bench_latency.hpp"
#include "flagship_config.hpp"
namespace bench::pool {

    using TscClock = demiplane::chrono::TscClock;
    namespace ink  = demiplane::ink;

    // The disruptor payload: a producer timestamp (rdtsc).
    struct BurstEvent {
        std::uint64_t t_produced{0};
    };
    using Disruptor = demiplane::multithread::
        Disruptor<BurstEvent, demiplane::multithread::SingleProducerSequencer, demiplane::multithread::AnyWaitStrategy>;

    // Channel-mode hand-off payload: producer timestamp + the reader's hand-off timestamp.
    struct WorkItem {
        std::uint64_t t_produced{0};
        std::uint64_t t_dispatch{0};
    };

    // Payload for the WS work frame (~16 bytes => sub-us write).
    inline constexpr std::array<std::byte, 16> WS_PAYLOAD{};

    struct Result {
        std::string name;
        double p50_us{}, p95_us{}, p99_us{}, max_us{};
        double rps{};
        std::int64_t processed{};
        std::int64_t produced{};
        bool drained{};  // max latency < drain budget => each burst cleared
    };

    // Merge per-shard samples, sort, compute percentiles (ns -> us). `budget` is the
    // drain budget (drain_budget(arrival)): max latency below it ⇒ the pool kept up.
    inline Result summarize(std::string name,
                            const std::vector<LatencyCollector>& collectors,
                            const std::int64_t produced,
                            const std::int64_t processed,
                            const double elapsed_s,
                            const std::chrono::nanoseconds budget) {
        std::vector<std::chrono::nanoseconds> merged;
        std::size_t total = 0;
        for (const auto& [samples] : collectors) {
            total += samples.size();
        }
        merged.reserve(total);
        for (const auto& [samples] : collectors) {
            merged.insert(merged.end(), samples.begin(), samples.end());
        }
        std::ranges::sort(merged);

        const auto pct = [&](const double p) -> double {
            if (merged.empty()) {
                return 0.0;
            }
            const auto idx = static_cast<std::size_t>(p * static_cast<double>(merged.size() - 1));
            return static_cast<double>(merged[idx].count()) / 1000.0;  // ns -> us
        };

        Result r;
        r.name      = std::move(name);
        r.p50_us    = pct(0.50);
        r.p95_us    = pct(0.95);
        r.p99_us    = pct(0.99);
        r.max_us    = merged.empty() ? 0.0 : static_cast<double>(merged.back().count()) / 1000.0;
        r.rps       = elapsed_s > 0.0 ? static_cast<double>(processed) / elapsed_s : 0.0;
        r.processed = processed;
        r.produced  = produced;
        r.drained   = r.max_us * 1000.0 < static_cast<double>(budget.count());
        return r;
    }

    struct CompStat {
        double p50{}, p99{}, mean{};
    };

    // p50/p99/mean (us) over all per-shard samples of one end-to-end latency component.
    inline CompStat component_stats(const std::vector<LatencyCollector>& collectors) {
        std::vector<std::chrono::nanoseconds> m;
        std::size_t total = 0;
        for (const auto& [samples] : collectors) {
            total += samples.size();
        }
        m.reserve(total);
        for (const auto& [samples] : collectors) {
            m.insert(m.end(), samples.begin(), samples.end());
        }
        std::ranges::sort(m);
        if (m.empty()) {
            return {};
        }
        const auto pct = [&](const double p) {
            return static_cast<double>(m[static_cast<std::size_t>(p * static_cast<double>(m.size() - 1))].count()) /
                   1000.0;
        };
        double sum = 0;
        for (const auto v : m) {
            sum += static_cast<double>(v.count());
        }
        return {pct(0.50), pct(0.99), sum / static_cast<double>(m.size()) / 1000.0};
    }

    // Final sync-vs-async comparison table (one row per config).
    inline void print_results(const std::vector<Result>& rows) {
        std::cout << '\n'
                  << ink::colors::colorize("Flagship Burst — sync vs async", ink::colors::bold_cyan) << '\n'
                  << ink::table(rows)
                         .column("config", [](const Result& r) { return r.name; })
                         .column("p50(us)", [](const Result& r) { return std::format("{:.2f}", r.p50_us); })
                         .column("p95(us)", [](const Result& r) { return std::format("{:.2f}", r.p95_us); })
                         .column("p99(us)", [](const Result& r) { return std::format("{:.2f}", r.p99_us); })
                         .column("max(us)", [](const Result& r) { return std::format("{:.1f}", r.max_us); })
                         .column("rps", [](const Result& r) { return std::format("{:.0f}", r.rps); })
                         .column("processed", [](const Result& r) { return r.processed; })
                         .column("drained", [](const Result& r) { return r.drained ? "yes" : "NO(overload)"; })
                         .border(ink::border::unicode)
                         .header_style(ink::colors::bold_cyan)
                         .align(ink::Align::Right)
                         .column_align(0, ink::Align::Left)  // config
                         .column_align(7, ink::Align::Left)  // drained
                         .render()
                  << '\n';
        for (const auto& r : rows) {
            if (r.processed != r.produced) {
                std::cout << ink::colors::colorize(
                                 std::format("  WARN {}: processed {} != produced {} (records lost/incomplete)",
                                             r.name,
                                             r.processed,
                                             r.produced),
                                 ink::colors::bold_red)
                          << '\n';
            }
        }
    }

    constexpr std::string_view dispatch_name(const Dispatch d) noexcept {
        switch (d) {
            case Dispatch::Pull:
                return "pull";
            case Dispatch::PullDedicated:
                return "dedicated";
            case Dispatch::Channel:
                return "channel";
        }
        return "?";
    }

    constexpr std::string_view work_name(const Work w) noexcept {
        switch (w) {
            case Work::Spin:
                return "spin";
            case Work::Ws:
                return "ws";
        }
        return "?";
    }

    inline void print_config(const FlagshipConfig& cfg, const std::string_view header) {
        const auto arrival_str = [&]() -> std::string {
            if (const auto* b = std::get_if<Burst>(&cfg.arrival)) {
                return std::format("burst  count={}  size={}  cooldown={}ms",
                                   b->count,
                                   b->size,
                                   std::chrono::duration_cast<std::chrono::milliseconds>(b->cooldown).count());
            }
            const auto& s = std::get<Steady>(cfg.arrival);
            return std::format("steady  rps={}  total={}", s.rps, s.total);
        }();
        std::cout
            << '\n'
            << ink::box(ink::colors::colorize(header, ink::colors::hi_yellow))
                   .border_style(ink::colors::bold_red)
                   .render()
            << '\n'
            << ink::table()
                   .headers("setting", "value")
                   .add_row("arrival", arrival_str)
                   .add_row("pool_size", std::format("{}", cfg.pool_size))
                   .add_row(
                       "acquire_timeout_us",
                       std::format("{}",
                                   std::chrono::duration_cast<std::chrono::microseconds>(cfg.acquire_timeout).count()))
                   .add_row("shards", std::format("{}", cfg.shards))
                   .add_row("disruptor_buf", std::format("{}", cfg.disruptor_buf))
                   .add_row("workers_per_shard", std::format("{}", cfg.workers_per_shard))
                   .add_row("dispatch", std::string{dispatch_name(cfg.dispatch)})
                   .add_row("work", std::string{work_name(cfg.work)})
                   .add_row("work_duration_us",
                            std::format(
                                "{}", std::chrono::duration_cast<std::chrono::microseconds>(cfg.work_duration).count()))
                   .border(ink::border::unicode)
                   .header_style(ink::colors::bold_cyan)
                   .align(ink::Align::Right)
                   .column_align(0, ink::Align::Left)
                   .render()
            << '\n';
    }
}  // namespace bench::pool
