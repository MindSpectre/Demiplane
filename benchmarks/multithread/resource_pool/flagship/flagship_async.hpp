#pragma once

// Flagship async runner. Owns all per-run state and the delicate teardown scope; the worker
// coroutine bodies live in async/dispatch/*. cfg.shards single-threaded io_contexts, one
// disruptor each (Pull/Channel) or one per worker (PullDedicated), fed by a pre-spawned pool of
// cfg.workers_per_shard worker coroutines per shard (no co_spawn per record). cfg.dispatch
// selects how workers get records; cfg.work selects the held-work step.
//
// LIFETIME CONTRACT: every worker references state owned by this function via a copied
// WorkerContext (a bundle of references, including `cfg`). Those referents MUST outlive every
// coroutine — keep them owned here (cfg is owned by the caller and outlives this call) and keep
// the scope-destruction order below (ws/channels before backend), gated by the live_coros==0
// drain barrier. Moving an owned object out, or reordering the inner scope, is a use-after-free.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <demiplane/ink>
#include <demiplane/multithread>  // AsyncResourcePool, ShardedAsioBackend, BusySpinWaitStrategy
#include <format>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/any_io_executor.hpp>

#include "common/mock_resource.hpp"
#include "dispatch/channel.hpp"
#include "dispatch/pull.hpp"
#include "dispatch/pull_dedicated.hpp"
#include "flagship_producer.hpp"
#include "flagship_worker.hpp"
#include "ws_harness.hpp"

namespace bench::pool {

    inline Result run_async(const std::string& name, const FlagshipConfig& cfg) {
        print_config(cfg, name);
        AsyncPoolT pool{cfg.pool_size, [](std::size_t i) noexcept { return MockResource{i}; }};

        // Pull/Channel: one disruptor per shard (workers share it). PullDedicated: one
        // disruptor per worker (SPSC, no shared claim cursor) → shards * workers_per_shard.
        const std::size_t n_disruptors =
            (cfg.dispatch == Dispatch::PullDedicated) ? cfg.shards * cfg.workers_per_shard : cfg.shards;
        std::vector<std::unique_ptr<Disruptor>> disruptors;
        disruptors.reserve(n_disruptors);
        for (std::size_t i = 0; i < n_disruptors; ++i) {
            disruptors.push_back(
                std::make_unique<Disruptor>(cfg.disruptor_buf, demiplane::multithread::BusySpinWaitStrategy{}));
        }
        std::vector<LatencyCollector> collectors(cfg.shards);
        std::vector<LatencyCollector> disp_wait(cfg.shards);   // produced -> dispatched (disruptor wait)
        std::vector<LatencyCollector> queue_wait(cfg.shards);  // dispatched -> coroutine first run (io_context queue)
        std::vector<LatencyCollector> acq_wait(cfg.shards);    // first run -> slot acquired (pool wait)
        std::vector<LatencyCollector> work_ns(cfg.shards);     // ws mode: measured async_write duration
        const auto cap = static_cast<std::size_t>(total_records(cfg.arrival)) / cfg.shards + 64;
        for (std::size_t s = 0; s < cfg.shards; ++s) {
            collectors[s].reserve(cap);
            disp_wait[s].reserve(cap);
            queue_wait[s].reserve(cap);
            acq_wait[s].reserve(cap);
            work_ns[s].reserve(cap);
        }

        std::atomic<std::int64_t> produced{0};
        std::atomic<std::int64_t> processed{0};
        std::atomic<std::int64_t> dropped{0};
        std::atomic<bool> producer_done{false};
        // One coroutine per worker, plus (Channel mode) one reader per shard.
        const std::size_t coros_per_shard = cfg.workers_per_shard + (cfg.dispatch == Dispatch::Channel ? 1u : 0u);
        std::atomic<std::size_t> live_coros{cfg.shards * coros_per_shard};
        // Pull mode: shared per-shard claim cursor (uncontended within a single-threaded shard).
        std::vector<std::atomic<std::int64_t>> claim_cursors(cfg.shards);

        const auto t0 = std::chrono::steady_clock::now();
        {
            demiplane::multithread::ShardedAsioBackend backend{cfg.shards, /*pin=*/true, cfg.shards};
            // Channel hand-off queues (one per shard). Declared AFTER backend so they destruct
            // BEFORE it (a channel holds an executor → must die before the io_context), gated by
            // live_coros==0. Unused outside Channel mode.
            std::vector<std::unique_ptr<Chan>> channels;
            if (cfg.dispatch == Dispatch::Channel) {
                channels.reserve(cfg.shards);
                for (std::size_t i = 0; i < cfg.shards; ++i) {
                    channels.push_back(std::make_unique<Chan>(backend.executor(i), 0));  // unbuffered
                }
            }

            // WS work instrument: one client stream per worker on its shard executor; a sink
            // thread (core cfg.shards+1, off the producer's) drains the peer ends.
            const std::size_t n_workers = cfg.shards * cfg.workers_per_shard;
            std::unique_ptr<WsHarness> ws;
            if (cfg.work == Work::Ws) {
                std::vector<boost::asio::any_io_executor> client_execs;
                client_execs.reserve(n_workers);
                for (std::size_t gw = 0; gw < n_workers; ++gw) {
                    client_execs.emplace_back(backend.executor(gw / cfg.workers_per_shard));
                }
                ws = std::make_unique<WsHarness>(client_execs, static_cast<int>(cfg.shards) + 1);  // sink core
                ws->start_and_await_handshakes();  // barrier: handshakes done before producing
            }
            WsHarness* ws_ptr = ws.get();  // null in spin mode; captured by workers

            // Shared worker state — references into this scope, valid for every coroutine's life.
            const WorkerContext ctx{pool,
                                    collectors,
                                    disp_wait,
                                    queue_wait,
                                    acq_wait,
                                    work_ns,
                                    processed,
                                    dropped,
                                    live_coros,
                                    disruptors,
                                    producer_done,
                                    ws_ptr,
                                    cfg};

            for (std::size_t i = 0; i < cfg.shards; ++i) {
                const auto exec_i = backend.executor(i);
                // Pre-spawned worker pool (the "ready to dispatch" coroutines).
                for (std::size_t w = 0; w < cfg.workers_per_shard; ++w) {
                    const std::size_t gw = i * cfg.workers_per_shard + w;  // global worker index = WS stream id
                    switch (cfg.dispatch) {
                        case Dispatch::Pull:
                            spawn_pull_worker(exec_i, ctx, claim_cursors[i], i, gw);
                            break;
                        case Dispatch::PullDedicated:
                            spawn_pull_dedicated_worker(exec_i, ctx, i, i * cfg.workers_per_shard + w, gw);
                            break;
                        case Dispatch::Channel:
                            spawn_channel_worker(exec_i, ctx, *channels[i], i, gw);
                            break;
                    }
                }
                // Channel mode: one reader coroutine per shard (in-order drain → channel hand-off).
                if (cfg.dispatch == Dispatch::Channel) {
                    spawn_channel_reader(exec_i, disruptors, producer_done, live_coros, *channels[i], i);
                }
            }

            // Producer thread (separate from the io threads).
            std::jthread producer{[&] {
                demiplane::multithread::pin_current_thread_to_core(static_cast<int>(cfg.shards));  // off the workers
                run_producer(
                    cfg.arrival, std::span<const std::unique_ptr<Disruptor>>{disruptors}, produced, producer_done);
            }};

            producer.join();
            // Wait for every record to be processed, then for every coroutine to exit (readers
            // close their channels on drain), before the channels / backend destruct.
            while (processed.load(std::memory_order_acquire) < produced.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
            while (live_coros.load(std::memory_order_acquire) != 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
            if (ws) {
                ws->shutdown();  // close clients → sinks drain & exit → join sink thread
            }
            // End of scope: ws/channels destruct (no live coroutine) → backend dtor joins.
        }
        const double elapsed_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        // End-to-end latency decomposition (successful acquires only).
        const CompStat ds      = component_stats(disp_wait);
        const CompStat qs      = component_stats(queue_wait);
        const CompStat as      = component_stats(acq_wait);
        const CompStat ws_stat = component_stats(work_ns);
        const auto us          = [](const double v) { return std::format("{:.2f}", v); };
        std::cout << '\n'
                  << ink::colors::colorize(std::format("{} — latency decomposition (us)", name), ink::colors::bold_cyan)
                  << '\n'
                  << ink::table()
                         .headers("stage", "p50", "p99", "mean")
                         .add_row("disruptor wait   (produced→disp)", us(ds.p50), us(ds.p99), us(ds.mean))
                         .add_row("io_context queue (disp→coro run)", us(qs.p50), us(qs.p99), us(qs.mean))
                         .add_row("pool acquire     (run→slot)", us(as.p50), us(as.p99), us(as.mean))
                         .add_row("ws write         (async_write)", us(ws_stat.p50), us(ws_stat.p99), us(ws_stat.mean))
                         .border(ink::border::unicode)
                         .header_style(ink::colors::bold_cyan)
                         .align(ink::Align::Right)
                         .column_align(0, ink::Align::Left)  // stage
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

        return summarize(name, collectors, prod, processed.load(), elapsed_s, drain_budget(cfg.arrival));
    }

}  // namespace bench::pool
