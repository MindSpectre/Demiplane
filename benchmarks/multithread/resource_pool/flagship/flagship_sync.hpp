#pragma once

// Flagship sync runner: `n_consumers` threads, one disruptor each (SPSC, CAS-free). The
// producer round-robins the configured arrival across the disruptors; each consumer spins its
// disruptor and, per record, blocking-acquires a pool slot, does work, releases.
//
// Ws mode: a shared io_context drives all N client WS streams (one per consumer). Each consumer
// calls async_write(..., use_future).get() — blocking on the future is fine since sync consumers
// are dedicated threads. Each stream is owned by exactly one consumer (one op in flight at a
// time per stream), so there is no concurrent access despite the shared io_context.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <demiplane/multithread>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include "common/mock_resource.hpp"
#include "flagship_producer.hpp"
#include "ws_harness.hpp"

namespace bench::pool {

    inline Result run_sync(const std::string& name, const std::size_t n_consumers, const FlagshipConfig& cfg) {
        using PoolT = demiplane::multithread::ResourcePool<MockResource, 1024>;
        print_config(cfg, name);
        PoolT pool{cfg.pool_size, [](const std::size_t i) noexcept { return MockResource{i}; }};

        std::vector<std::unique_ptr<Disruptor>> disruptors;
        disruptors.reserve(n_consumers);
        for (std::size_t i = 0; i < n_consumers; ++i) {
            disruptors.push_back(
                std::make_unique<Disruptor>(cfg.disruptor_buf, demiplane::multithread::BusySpinWaitStrategy{}));
        }

        std::vector<LatencyCollector> collectors(n_consumers);
        std::vector<LatencyCollector> disp_wait(n_consumers);
        std::vector<LatencyCollector> acq_wait(n_consumers);
        std::vector<LatencyCollector> work_ns(n_consumers);
        const auto cap = static_cast<std::size_t>(total_records(cfg.arrival)) / n_consumers + 64;
        for (std::size_t i = 0; i < n_consumers; ++i) {
            collectors[i].reserve(cap);
            disp_wait[i].reserve(cap);
            acq_wait[i].reserve(cap);
            work_ns[i].reserve(cap);
        }

        std::atomic<std::int64_t> produced{0};
        std::atomic<std::int64_t> processed{0};
        std::atomic<std::int64_t> dropped{0};
        std::atomic<bool> producer_done{false};

        // Ws work instrument: one shared io_context drives all N client streams, backed by N
        // threads (one per consumer stream). Each consumer calls async_write(..., use_future).get()
        // — blocking is fine since sync consumers are dedicated. N threads avoid serializing all
        // writes through a single thread, matching the parallelism async gets from N coroutines.
        boost::asio::io_context ws_ioc;
        auto ws_guard = boost::asio::make_work_guard(ws_ioc);
        std::unique_ptr<WsHarness> ws;
        std::vector<std::jthread> ws_ioc_threads;
        if (cfg.work == Work::Ws) {
            std::vector<boost::asio::any_io_executor> execs;
            execs.assign(n_consumers, ws_ioc.get_executor());
            ws = std::make_unique<WsHarness>(execs, static_cast<int>(cfg.shards) + 1);
            ws_ioc_threads.reserve(n_consumers);
            for (std::size_t j = 0; j < n_consumers; ++j) {
                ws_ioc_threads.emplace_back([&ws_ioc, n_consumers, j] {
                    demiplane::multithread::pin_current_thread_to_core(static_cast<int>(n_consumers) + 2 +
                                                                       static_cast<int>(j));
                    ws_ioc.run();
                });
            }
            ws->start_and_await_handshakes();
        }

        // Consumers.
        std::vector<std::jthread> consumers;
        consumers.reserve(n_consumers);
        for (std::size_t i = 0; i < n_consumers; ++i) {
            consumers.emplace_back([&, i] {
                demiplane::multithread::pin_current_thread_to_core(static_cast<int>(i % cfg.shards));
                Disruptor& d          = *disruptors[i];
                std::int64_t next_seq = 0;
                while (true) {
                    const std::int64_t cursor = d.sequencer().get_cursor();
                    if (const std::int64_t avail = d.sequencer().get_highest_published(next_seq, cursor);
                        avail >= next_seq) {
                        for (std::int64_t seq = next_seq; seq <= avail; ++seq) {
                            const auto [t_produced]        = d.ring_buffer()[seq];
                            const std::uint64_t t_dispatch = TscClock::now();
                            if (auto lease = pool.acquire_for(cfg.acquire_timeout)) {
                                const std::uint64_t t_start = TscClock::now();
                                collectors[i].record(TscClock::to_duration(t_start - t_produced));
                                disp_wait[i].record(TscClock::to_duration(t_dispatch - t_produced));
                                acq_wait[i].record(TscClock::to_duration(t_start - t_dispatch));
                                if (ws) {
                                    const std::uint64_t tw0 = TscClock::now();
                                    ws->client(i)
                                        .async_write(boost::asio::buffer(WS_PAYLOAD), boost::asio::use_future)
                                        .get();
                                    work_ns[i].record(TscClock::to_duration(TscClock::now() - tw0));
                                } else {
                                    (*lease)->work_for(cfg.work_duration);
                                }
                            } else {
                                dropped.fetch_add(1, std::memory_order_relaxed);
                            }
                            processed.fetch_add(1, std::memory_order_relaxed);
                        }
                        next_seq = avail + 1;
                        d.sequencer().update_gating_sequence(avail);
                    } else if (producer_done.load(std::memory_order_acquire) &&
                               processed.load(std::memory_order_acquire) >= produced.load(std::memory_order_acquire)) {
                        break;
                    }
                }
            });
        }

        // Producer (this thread): the configured arrival, round-robined across disruptors.
        demiplane::multithread::pin_current_thread_to_core(static_cast<int>(cfg.shards));
        const auto t0 = std::chrono::steady_clock::now();
        run_producer(cfg.arrival, std::span<const std::unique_ptr<Disruptor>>{disruptors}, produced, producer_done);
        consumers.clear();  // jthread dtors join

        if (ws) {
            ws->shutdown();
            ws_guard.reset();  // lets ws_ioc.run() return → ws_ioc_thread joins
        }
        const double elapsed_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        const CompStat ds         = component_stats(disp_wait);
        const CompStat as         = component_stats(acq_wait);
        const CompStat ws_stat    = component_stats(work_ns);
        const auto us             = [](const double v) { return std::format("{:.2f}", v); };
        const std::string ws_p50  = ws ? us(ws_stat.p50) : "n/a";
        const std::string ws_p99  = ws ? us(ws_stat.p99) : "n/a";
        const std::string ws_mean = ws ? us(ws_stat.mean) : "n/a";
        std::cout << '\n'
                  << ink::colors::colorize(std::format("{} — latency decomposition (us)", name), ink::colors::bold_cyan)
                  << '\n'
                  << ink::table()
                         .headers("stage", "p50", "p99", "mean")
                         .add_row("disruptor wait   (produced→disp)", us(ds.p50), us(ds.p99), us(ds.mean))
                         .add_row("pool acquire     (disp→slot)", us(as.p50), us(as.p99), us(as.mean))
                         .add_row("ws write         (async_write)", ws_p50, ws_p99, ws_mean)
                         .border(ink::border::unicode)
                         .header_style(ink::colors::bold_cyan)
                         .align(ink::Align::Right)
                         .column_align(0, ink::Align::Left)
                         .render()
                  << '\n';
        const std::int64_t drp  = dropped.load();
        const std::int64_t prod = produced.load();
        std::cout << ink::box(
                         ink::colors::colorize(
                             std::format("dropped (acquire_for timeout): {} / {} ({:.1f}%)",
                                         drp,
                                         prod,
                                         prod ? 100.0 * static_cast<double>(drp) / static_cast<double>(prod) : 0.0),
                             ink::colors::green))
                         .render();
        return summarize(name, collectors, produced.load(), processed.load(), elapsed_s, drain_budget(cfg.arrival));
    }

}  // namespace bench::pool
