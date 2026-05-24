#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <demiplane/multithread>
#include <format>
#include <iostream>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "common/bench_latency.hpp"
#include "common/mock_resource.hpp"
#include "common/pool_bench_main.hpp"
#include "common/rdtsc_clock.hpp"
#include "common/sharded_asio_backend.hpp"
#include "common/ws_harness.hpp"

using namespace demiplane::multithread;
using namespace bench::pool;
using namespace std::chrono_literals;

namespace {

    // Spike-that-drains burst: 3750 records dumped, 0.5s cooldown, repeat.
    constexpr std::size_t SHARDS                = 10;  // worker cores: async io threads / sync consumers
    constexpr std::int64_t BURST_SIZE           = 3750;
    constexpr std::chrono::nanoseconds COOLDOWN = 500ms;
    constexpr std::chrono::nanoseconds WORK     = 1us;    // real work
    std::chrono::nanoseconds WAIT               = 2us;    // acquire_for timeout (set once from --wait-us)
    std::size_t POOL_SIZE                       = 128;    // pool connections (set once from --pool)
    constexpr std::size_t DISRUPTOR_BUF         = 16384;  // power of 2, > peak per-disruptor depth

    // The disruptor payload: a producer timestamp (rdtsc).
    struct BurstEvent {
        std::uint64_t t_produced{0};
    };
    using Disruptor = Disruptor<BurstEvent, SingleProducerSequencer, AnyWaitStrategy>;

    // Channel-mode hand-off payload: producer timestamp + the reader's hand-off timestamp.
    struct WorkItem {
        std::uint64_t t_produced{0};
        std::uint64_t t_dispatch{0};
    };

    // How worker coroutines receive records from the disruptor.
    enum class Dispatch {
        Pull,           // workers share one disruptor per shard via an atomic claim cursor (CAS)
        PullDedicated,  // each worker owns its own disruptor (SPSC, no shared claim/CAS)
        Channel,        // one reader per shard fans out via an unbuffered asio channel
    };

    // What the worker does while holding a slot.
    enum class Work {
        Spin,  // work_for(WORK) busy-spin + co_await post (synthetic)
        Ws,    // co_await beast websocket async_write of a tiny frame (real, yields)
    };

    // Payload for the WS work frame (~16 bytes => sub-us write).
    inline constexpr std::array<std::byte, 16> WS_PAYLOAD{};

    // Arrival pattern.
    enum class Load {
        Burst,   // BURST_SIZE records dumped, COOLDOWN idle, repeated `bursts` times
        Steady,  // `total` records at a uniform `rps` arrival rate
    };
    struct LoadCfg {
        Load mode           = Load::Burst;
        std::int64_t bursts = 20;     // Burst: number of bursts
        std::int64_t rps    = 250;    // Steady: uniform arrival rate (records/sec)
        std::int64_t total  = 15000;  // Steady: total records
    };
    // Total records a run produces (for collector sizing).
    std::int64_t total_records(const LoadCfg& l) {
        return (l.mode == Load::Steady) ? l.total : l.bursts * BURST_SIZE;
    }

    struct Result {
        std::string name;
        double p50_us{}, p95_us{}, p99_us{}, max_us{};
        double rps{};
        std::int64_t processed{};
        std::int64_t produced{};
        bool drained{};  // max latency < cooldown => each burst cleared
    };

    // Merge per-shard samples, sort, compute percentiles (ns -> us).
    Result summarize(std::string name,
                     const std::vector<LatencyCollector>& collectors,
                     const std::int64_t produced,
                     const std::int64_t processed,
                     const double elapsed_s) {
        std::vector<std::chrono::nanoseconds> merged;
        std::size_t total = 0;
        for (const auto& c : collectors) {
            total += c.samples.size();
        }
        merged.reserve(total);
        for (const auto& c : collectors) {
            merged.insert(merged.end(), c.samples.begin(), c.samples.end());
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
        r.drained   = r.max_us * 1000.0 <
                    static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(COOLDOWN).count());
        return r;
    }

    struct CompStat {
        double p50{}, p99{}, mean{};
    };

    // p50/p99/mean (us) over all per-shard samples of one end-to-end latency component.
    CompStat component_stats(const std::vector<LatencyCollector>& collectors) {
        std::vector<std::chrono::nanoseconds> m;
        std::size_t total = 0;
        for (const auto& c : collectors) {
            total += c.samples.size();
        }
        m.reserve(total);
        for (const auto& c : collectors) {
            m.insert(m.end(), c.samples.begin(), c.samples.end());
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

    void print_results(const std::vector<Result>& rows) {
        std::cout << std::format("\n{:<14} {:>9} {:>9} {:>9} {:>9} {:>10} {:>12} {:>8}\n",
                                 "config",
                                 "p50(us)",
                                 "p95(us)",
                                 "p99(us)",
                                 "max(us)",
                                 "rps",
                                 "processed",
                                 "drained");
        for (const auto& r : rows) {
            std::cout << std::format("{:<14} {:>9.2f} {:>9.2f} {:>9.2f} {:>9.1f} {:>10.0f} {:>12} {:>8}\n",
                                     r.name,
                                     r.p50_us,
                                     r.p95_us,
                                     r.p99_us,
                                     r.max_us,
                                     r.rps,
                                     r.processed,
                                     r.drained ? "yes" : "NO(overload)");
            if (r.processed != r.produced) {
                std::cout << std::format("  WARN {}: processed {} != produced {} (records lost/incomplete)\n",
                                         r.name,
                                         r.processed,
                                         r.produced);
            }
        }
    }

    // Sync config: `n_consumers` threads, one disruptor each (SPSC, CAS-free). The
    // producer round-robins BURST_SIZE timestamped records across the disruptors per
    // burst, then sleeps COOLDOWN. Each consumer spins its disruptor and, per record,
    // blocking-acquires a pool slot, stamps t_start, does WORK, releases.
    [[maybe_unused]] Result
    run_sync(const std::string& name, const std::size_t n_consumers, const std::int64_t bursts) {
        using PoolT = ResourcePool<MockResource, 1024>;
        PoolT pool{POOL_SIZE, [](std::size_t i) noexcept { return MockResource{i}; }};

        std::vector<std::unique_ptr<Disruptor>> disruptors;
        disruptors.reserve(n_consumers);
        for (std::size_t i = 0; i < n_consumers; ++i) {
            disruptors.push_back(std::make_unique<Disruptor>(DISRUPTOR_BUF, BusySpinWaitStrategy{}));
        }

        std::vector<LatencyCollector> collectors(n_consumers);
        for (auto& c : collectors) {
            c.reserve(static_cast<std::size_t>(bursts) * static_cast<std::size_t>(BURST_SIZE) / n_consumers + 64);
        }

        std::atomic<std::int64_t> produced{0};
        std::atomic<std::int64_t> processed{0};
        std::atomic<bool> producer_done{false};

        // Consumers.
        std::vector<std::jthread> consumers;
        consumers.reserve(n_consumers);
        for (std::size_t i = 0; i < n_consumers; ++i) {
            consumers.emplace_back([&, i] {
                pin_current_thread_to_core(static_cast<int>(i % SHARDS));
                Disruptor& d          = *disruptors[i];
                LatencyCollector& col = collectors[i];
                std::int64_t next_seq = 0;
                while (true) {
                    const std::int64_t cursor = d.sequencer().get_cursor();
                    const std::int64_t avail  = d.sequencer().get_highest_published(next_seq, cursor);
                    if (avail >= next_seq) {
                        for (std::int64_t seq = next_seq; seq <= avail; ++seq) {
                            const BurstEvent ev = d.ring_buffer()[seq];
                            if (auto lease = pool.acquire_for(WAIT)) {
                                const std::uint64_t t_start = rdtsc_now();
                                col.record(cycles_to_ns(t_start - ev.t_produced));
                                (*lease)->work_for(WORK);
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

        // Producer (this thread): bursts of BURST_SIZE round-robined across disruptors.
        pin_current_thread_to_core(static_cast<int>(SHARDS));  // dedicated producer core, off the worker cores
        const auto t0 = std::chrono::steady_clock::now();
        for (std::int64_t b = 0; b < bursts; ++b) {
            for (std::int64_t k = 0; k < BURST_SIZE; ++k) {
                Disruptor& d           = *disruptors[static_cast<std::size_t>(k) % n_consumers];
                const std::int64_t seq = d.sequencer().next();
                d.ring_buffer()[seq]   = BurstEvent{rdtsc_now()};
                d.sequencer().publish(seq);
            }
            produced.fetch_add(BURST_SIZE, std::memory_order_release);
            std::this_thread::sleep_for(COOLDOWN);
        }
        producer_done.store(true, std::memory_order_release);
        consumers.clear();  // jthread dtors join (consumers exit once drained)
        const double elapsed_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        return summarize(name, collectors, produced.load(), processed.load(), elapsed_s);
    }

    // Async config: SHARDS single-threaded io_contexts, one disruptor each, fed by a
    // pre-spawned pool of `workers_per_shard` worker coroutines per shard (NO co_spawn per
    // record). `mode` selects how workers get records: Pull (claim directly from the disruptor
    // via an atomic cursor) or Channel (one reader per shard fans out via an unbuffered asio
    // channel). Held work is work_for(WORK) + co_await post (floor-free async-write model).
    Result run_async(const std::string& name,
                     const LoadCfg load,
                     const std::size_t workers_per_shard,
                     const Dispatch mode,
                     const Work work) {
        using PoolT = AsyncResourcePool<MockResource, 1024>;
        PoolT pool{POOL_SIZE, [](std::size_t i) noexcept { return MockResource{i}; }};

        // Pull/Channel: one disruptor per shard (workers share it). PullDedicated: one
        // disruptor per worker (SPSC, no shared claim cursor) → SHARDS * workers_per_shard.
        const std::size_t n_disruptors = (mode == Dispatch::PullDedicated) ? SHARDS * workers_per_shard : SHARDS;
        std::vector<std::unique_ptr<Disruptor>> disruptors;
        disruptors.reserve(n_disruptors);
        for (std::size_t i = 0; i < n_disruptors; ++i) {
            disruptors.push_back(std::make_unique<Disruptor>(DISRUPTOR_BUF, BusySpinWaitStrategy{}));
        }
        std::vector<LatencyCollector> collectors(SHARDS);
        std::vector<LatencyCollector> disp_wait(SHARDS);   // produced -> dispatched (disruptor wait)
        std::vector<LatencyCollector> queue_wait(SHARDS);  // dispatched -> coroutine first run (io_context queue)
        std::vector<LatencyCollector> acq_wait(SHARDS);    // first run -> slot acquired (pool wait)
        std::vector<LatencyCollector> work_ns(SHARDS);     // ws mode: measured async_write duration
        const auto cap = static_cast<std::size_t>(total_records(load)) / SHARDS + 64;
        for (std::size_t s = 0; s < SHARDS; ++s) {
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
        const std::size_t coros_per_shard = workers_per_shard + (mode == Dispatch::Channel ? 1u : 0u);
        std::atomic<std::size_t> live_coros{SHARDS * coros_per_shard};
        // Pull mode: shared per-shard claim cursor (uncontended within a single-threaded shard).
        std::vector<std::atomic<std::int64_t>> claim_cursors(SHARDS);

        using Chan = boost::asio::experimental::channel<void(boost::system::error_code, WorkItem)>;

        const auto t0 = std::chrono::steady_clock::now();
        {
            ShardedAsioBackend backend{SHARDS, /*pin=*/true};
            // Channel-mode hand-off queues (one per shard). Declared AFTER backend so they
            // destruct BEFORE it (a channel holds an executor → must die before the io_context),
            // gated by live_coros==0. Unused in Pull mode.
            std::vector<std::unique_ptr<Chan>> channels;
            if (mode == Dispatch::Channel) {
                channels.reserve(SHARDS);
                for (std::size_t i = 0; i < SHARDS; ++i) {
                    channels.push_back(std::make_unique<Chan>(backend.executor(i), 0));  // unbuffered
                }
            }

            // WS work instrument: one client stream per worker on its shard executor; a sink
            // thread (core SHARDS, shared with the producer) drains the peer ends.
            const std::size_t n_workers = SHARDS * workers_per_shard;
            std::unique_ptr<WsHarness> ws;
            if (work == Work::Ws) {
                std::vector<boost::asio::any_io_executor> client_execs;
                client_execs.reserve(n_workers);
                for (std::size_t gw = 0; gw < n_workers; ++gw) {
                    client_execs.emplace_back(backend.executor(gw / workers_per_shard));
                }
                ws = std::make_unique<WsHarness>(client_execs,
                                                 static_cast<int>(SHARDS) + 1);  // sink core, off producer's
                ws->start_and_await_handshakes();  // barrier: handshakes done before producing
            }
            WsHarness* ws_ptr = ws.get();  // null in spin mode; captured by workers

            for (std::size_t i = 0; i < SHARDS; ++i) {
                const auto exec_i = backend.executor(i);

                // Pre-spawned worker pool (the "ready to dispatch" coroutines).
                for (std::size_t w = 0; w < workers_per_shard; ++w) {
                    const std::size_t gw = i * workers_per_shard + w;  // global worker index = WS stream id
                    if (mode == Dispatch::Pull) {
                        // Pull: claim records directly from the disruptor via the shard's atomic cursor.
                        boost::asio::co_spawn(
                            exec_i,
                            [&pool,
                             &collectors,
                             &disp_wait,
                             &queue_wait,
                             &acq_wait,
                             &work_ns,
                             &processed,
                             &dropped,
                             &live_coros,
                             &disruptors,
                             &claim_cursors,
                             &producer_done,
                             exec_i,
                             i,
                             gw,
                             ws_ptr,
                             work]() -> boost::asio::awaitable<void> {
                                Disruptor& d                     = *disruptors[i];
                                std::atomic<std::int64_t>& claim = claim_cursors[i];
                                boost::asio::steady_timer poll{exec_i};
                                for (;;) {
                                    std::int64_t seq = -1;
                                    for (;;) {  // CAS-claim the next published sequence (uncontended in-shard)
                                        const std::int64_t c = claim.load(std::memory_order_acquire);
                                        const std::int64_t avail =
                                            d.sequencer().get_highest_published(c, d.sequencer().get_cursor());
                                        if (c > avail) {
                                            break;  // nothing published beyond c
                                        }
                                        std::int64_t expected = c;
                                        if (claim.compare_exchange_weak(expected, c + 1, std::memory_order_acq_rel)) {
                                            seq = c;
                                            break;
                                        }
                                    }
                                    if (seq >= 0) {
                                        const std::uint64_t t_run = rdtsc_now();  // pull: dispatch == run
                                        const BurstEvent ev       = d.ring_buffer()[seq];
                                        auto lease                = co_await pool.async_acquire_for(exec_i, WAIT);
                                        if (lease.has_value()) {
                                            const std::uint64_t t_start = rdtsc_now();
                                            disp_wait[i].record(cycles_to_ns(t_run - ev.t_produced));
                                            queue_wait[i].record(std::chrono::nanoseconds{0});  // no hand-off
                                            acq_wait[i].record(cycles_to_ns(t_start - t_run));
                                            collectors[i].record(cycles_to_ns(t_start - ev.t_produced));
                                            if (work == Work::Ws) {
                                                const std::uint64_t tw0 = rdtsc_now();
                                                co_await ws_ptr->client(gw).async_write(boost::asio::buffer(WS_PAYLOAD),
                                                                                        boost::asio::use_awaitable);
                                                work_ns[i].record(cycles_to_ns(rdtsc_now() - tw0));
                                            } else {
                                                (*lease)->work_for(WORK);
                                                co_await boost::asio::post(exec_i, boost::asio::use_awaitable);
                                            }
                                        } else {
                                            dropped.fetch_add(1, std::memory_order_relaxed);
                                        }
                                        processed.fetch_add(1, std::memory_order_relaxed);
                                    } else if (producer_done.load(std::memory_order_acquire)) {
                                        break;  // drained: no more will publish
                                    } else {
                                        poll.expires_after(20us);
                                        co_await poll.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));
                                    }
                                }
                                live_coros.fetch_sub(1, std::memory_order_acq_rel);  // SINGLE exit
                                co_return;
                            },
                            boost::asio::detached);
                    } else if (mode == Dispatch::PullDedicated) {
                        // PullDedicated: each worker drains its OWN disruptor (SPSC, no shared claim).
                        const std::size_t didx = i * workers_per_shard + w;
                        boost::asio::co_spawn(
                            exec_i,
                            [&pool,
                             &collectors,
                             &disp_wait,
                             &queue_wait,
                             &acq_wait,
                             &work_ns,
                             &processed,
                             &dropped,
                             &live_coros,
                             &disruptors,
                             &producer_done,
                             exec_i,
                             i,
                             didx,
                             gw,
                             ws_ptr,
                             work]() -> boost::asio::awaitable<void> {
                                Disruptor& d = *disruptors[didx];
                                boost::asio::steady_timer poll{exec_i};
                                std::int64_t next_seq = 0;
                                for (;;) {
                                    const std::int64_t cursor = d.sequencer().get_cursor();
                                    const std::int64_t avail  = d.sequencer().get_highest_published(next_seq, cursor);
                                    if (avail >= next_seq) {
                                        for (std::int64_t seq = next_seq; seq <= avail; ++seq) {
                                            const std::uint64_t t_run = rdtsc_now();
                                            const BurstEvent ev       = d.ring_buffer()[seq];
                                            auto lease                = co_await pool.async_acquire_for(exec_i, WAIT);
                                            if (lease.has_value()) {
                                                const std::uint64_t t_start = rdtsc_now();
                                                disp_wait[i].record(cycles_to_ns(t_run - ev.t_produced));
                                                queue_wait[i].record(std::chrono::nanoseconds{0});  // no hand-off
                                                acq_wait[i].record(cycles_to_ns(t_start - t_run));
                                                collectors[i].record(cycles_to_ns(t_start - ev.t_produced));
                                                if (work == Work::Ws) {
                                                    const std::uint64_t tw0 = rdtsc_now();
                                                    co_await ws_ptr->client(gw).async_write(
                                                        boost::asio::buffer(WS_PAYLOAD), boost::asio::use_awaitable);
                                                    work_ns[i].record(cycles_to_ns(rdtsc_now() - tw0));
                                                } else {
                                                    (*lease)->work_for(WORK);
                                                    co_await boost::asio::post(exec_i, boost::asio::use_awaitable);
                                                }
                                            } else {
                                                dropped.fetch_add(1, std::memory_order_relaxed);
                                            }
                                            processed.fetch_add(1, std::memory_order_relaxed);
                                        }
                                        next_seq = avail + 1;
                                        d.sequencer().update_gating_sequence(avail);
                                    } else if (producer_done.load(std::memory_order_acquire)) {
                                        break;  // drained
                                    } else {
                                        poll.expires_after(20us);
                                        co_await poll.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));
                                    }
                                }
                                live_coros.fetch_sub(1, std::memory_order_acq_rel);  // SINGLE exit
                                co_return;
                            },
                            boost::asio::detached);
                    } else {
                        // Channel: receive records handed off by the shard's reader coroutine.
                        Chan& ch = *channels[i];
                        boost::asio::co_spawn(
                            exec_i,
                            [&pool,
                             &collectors,
                             &disp_wait,
                             &queue_wait,
                             &acq_wait,
                             &work_ns,
                             &processed,
                             &dropped,
                             &live_coros,
                             &ch,
                             exec_i,
                             i,
                             gw,
                             ws_ptr,
                             work]() -> boost::asio::awaitable<void> {
                                for (;;) {
                                    auto [ec, item] =
                                        co_await ch.async_receive(boost::asio::as_tuple(boost::asio::use_awaitable));
                                    if (ec) {
                                        break;  // channel closed → shard drained
                                    }
                                    const std::uint64_t t_run = rdtsc_now();
                                    auto lease                = co_await pool.async_acquire_for(exec_i, WAIT);
                                    if (lease.has_value()) {
                                        const std::uint64_t t_start = rdtsc_now();
                                        disp_wait[i].record(cycles_to_ns(item.t_dispatch - item.t_produced));
                                        queue_wait[i].record(cycles_to_ns(t_run - item.t_dispatch));
                                        acq_wait[i].record(cycles_to_ns(t_start - t_run));
                                        collectors[i].record(cycles_to_ns(t_start - item.t_produced));
                                        if (work == Work::Ws) {
                                            const std::uint64_t tw0 = rdtsc_now();
                                            co_await ws_ptr->client(gw).async_write(boost::asio::buffer(WS_PAYLOAD),
                                                                                    boost::asio::use_awaitable);
                                            work_ns[i].record(cycles_to_ns(rdtsc_now() - tw0));
                                        } else {
                                            (*lease)->work_for(WORK);
                                            co_await boost::asio::post(exec_i, boost::asio::use_awaitable);
                                        }
                                    } else {
                                        dropped.fetch_add(1, std::memory_order_relaxed);
                                    }
                                    processed.fetch_add(1, std::memory_order_relaxed);
                                }
                                live_coros.fetch_sub(1, std::memory_order_acq_rel);  // SINGLE exit
                                co_return;
                            },
                            boost::asio::detached);
                    }
                }

                // Channel mode: one reader coroutine per shard (in-order drain → channel hand-off).
                if (mode == Dispatch::Channel) {
                    Chan& ch = *channels[i];
                    boost::asio::co_spawn(
                        exec_i,
                        [&disruptors, &producer_done, &live_coros, &ch, exec_i, i]() -> boost::asio::awaitable<void> {
                            Disruptor& d = *disruptors[i];
                            boost::asio::steady_timer poll{exec_i};
                            std::int64_t next_seq = 0;
                            for (;;) {
                                const std::int64_t cursor = d.sequencer().get_cursor();
                                const std::int64_t avail  = d.sequencer().get_highest_published(next_seq, cursor);
                                if (avail >= next_seq) {
                                    for (std::int64_t seq = next_seq; seq <= avail; ++seq) {
                                        const BurstEvent ev = d.ring_buffer()[seq];
                                        const WorkItem item{ev.t_produced, rdtsc_now()};  // t_dispatch = hand-off
                                        co_await ch.async_send(boost::system::error_code{},
                                                               item,
                                                               boost::asio::as_tuple(boost::asio::use_awaitable));
                                    }
                                    next_seq = avail + 1;
                                    d.sequencer().update_gating_sequence(avail);
                                } else if (producer_done.load(std::memory_order_acquire)) {
                                    break;  // no more will be published
                                } else {
                                    poll.expires_after(20us);
                                    co_await poll.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));
                                }
                            }
                            ch.close();  // wake idle workers → they observe ec and exit
                            live_coros.fetch_sub(1, std::memory_order_acq_rel);  // SINGLE exit
                            co_return;
                        },
                        boost::asio::detached);
                }
            }

            // Producer thread (separate from the io threads).
            std::jthread producer{[&] {
                pin_current_thread_to_core(static_cast<int>(SHARDS));  // dedicated producer core, off the worker
                if (load.mode == Load::Steady) {
                    // Uniform arrival: one record every (1s / rps). Absolute deadlines so jitter
                    // doesn't accumulate; t_produced is stamped at the actual push, so emission
                    // jitter never biases the measured latency.
                    const auto interval = std::chrono::nanoseconds{1'000'000'000 / load.rps};
                    const auto t_emit0  = std::chrono::steady_clock::now();
                    for (std::int64_t k = 0; k < load.total; ++k) {
                        std::this_thread::sleep_until(t_emit0 + interval * k);
                        Disruptor& d           = *disruptors[static_cast<std::size_t>(k) % n_disruptors];
                        const std::int64_t seq = d.sequencer().next();
                        d.ring_buffer()[seq]   = BurstEvent{rdtsc_now()};
                        d.sequencer().publish(seq);
                        produced.fetch_add(1, std::memory_order_release);
                    }
                } else {
                    for (std::int64_t b = 0; b < load.bursts; ++b) {
                        for (std::int64_t k = 0; k < BURST_SIZE; ++k) {
                            Disruptor& d           = *disruptors[static_cast<std::size_t>(k) % n_disruptors];
                            const std::int64_t seq = d.sequencer().next();
                            d.ring_buffer()[seq]   = BurstEvent{rdtsc_now()};
                            d.sequencer().publish(seq);
                        }
                        produced.fetch_add(BURST_SIZE, std::memory_order_release);
                        std::this_thread::sleep_for(COOLDOWN);
                    }
                }
                producer_done.store(true, std::memory_order_release);
            }};

            producer.join();
            // Wait for every record to be processed, then for every coroutine to exit (readers
            // close their channels on drain), before the channels / backend destruct.
            while (processed.load(std::memory_order_acquire) < produced.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(1ms);
            }
            while (live_coros.load(std::memory_order_acquire) != 0) {
                std::this_thread::sleep_for(1ms);
            }
            if (ws) {
                ws->shutdown();  // close clients → sinks drain & exit → join sink thread
            }
            // End of scope: ws/channels destruct (no live coroutine) → backend dtor joins.
        }
        const double elapsed_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        // End-to-end latency decomposition (successful acquires only).
        const CompStat ds = component_stats(disp_wait);
        const CompStat qs = component_stats(queue_wait);
        const CompStat as = component_stats(acq_wait);
        std::cout << std::format(
            "\n[{} latency decomposition, us]      {:>9} {:>9} {:>9}\n", name, "p50", "p99", "mean");
        std::cout << std::format(
            "  disruptor wait   (produced->disp)  {:>9.2f} {:>9.2f} {:>9.2f}\n", ds.p50, ds.p99, ds.mean);
        std::cout << std::format(
            "  io_context queue (disp->coro run)  {:>9.2f} {:>9.2f} {:>9.2f}\n", qs.p50, qs.p99, qs.mean);
        std::cout << std::format(
            "  pool acquire     (run->slot)       {:>9.2f} {:>9.2f} {:>9.2f}\n", as.p50, as.p99, as.mean);
        const CompStat ws_stat = component_stats(work_ns);
        std::cout << std::format("  ws write         (async_write)     {:>9.2f} {:>9.2f} {:>9.2f}\n",
                                 ws_stat.p50,
                                 ws_stat.p99,
                                 ws_stat.mean);
        const std::int64_t drp  = dropped.load();
        const std::int64_t prod = produced.load();
        std::cout << std::format("  dropped (acquire_for timeout): {} / {} ({:.1f}%)\n",
                                 drp,
                                 prod,
                                 prod ? 100.0 * static_cast<double>(drp) / static_cast<double>(prod) : 0.0);

        return summarize(name, collectors, prod, processed.load(), elapsed_s);
    }

    struct Args {
        std::int64_t bursts           = 20;
        std::string config            = "all";    // sync5 | sync128 | async | all
        std::size_t workers_per_shard = 8;        // async: pre-spawned worker coroutines per shard
        std::string dispatch          = "both";   // async feed: pull | dedicated | channel | both | all
        std::string work              = "spin";   // async work unit: spin | ws
        std::string load              = "burst";  // arrival: burst | steady
        std::int64_t rps              = 250;      // steady: records/sec
        std::int64_t total            = 15000;    // steady: total records
        std::int64_t pool             = 128;      // pool connections (POOL_SIZE)
        std::int64_t wait_us          = 2;        // acquire_for timeout in us (WAIT) — raise to wait, not drop
    };

    Args parse_args(int argc, char** argv) {
        Args a;
        for (int i = 1; i < argc; ++i) {
            const std::string_view s{argv[i]};
            if (s.starts_with("--bursts=")) {
                a.bursts = std::stoll(std::string{s.substr(9)});
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
            }
        }
        return a;
    }

}  // namespace

int main(int argc, char** argv) {
    const Args args = parse_args(argc, argv);
    POOL_SIZE       = static_cast<std::size_t>(args.pool);
    WAIT            = std::chrono::microseconds{args.wait_us};  // set once, before any worker threads start
    print_calibration();                                        // from rdtsc_clock via pool_bench_main.hpp
    std::vector<Result> rows;
    if (args.config == "sync5" || args.config == "all") {
        // rows.push_back(run_sync("sync@5", 5, args.bursts));
    }
    if (args.config == "sync128" || args.config == "all") {
        // rows.push_back(run_sync("sync@128", 128, args.bursts));
    }
    if (args.config == "async" || args.config == "all") {
        const std::string& d = args.dispatch;
        const Work wrk       = (args.work == "ws") ? Work::Ws : Work::Spin;
        const LoadCfg lc{(args.load == "steady") ? Load::Steady : Load::Burst, args.bursts, args.rps, args.total};
        if (d == "pull" || d == "both" || d == "all") {
            rows.push_back(run_async("async-pull", lc, args.workers_per_shard, Dispatch::Pull, wrk));
        }
        if (d == "dedicated" || d == "all") {
            rows.push_back(run_async("async-ded", lc, args.workers_per_shard, Dispatch::PullDedicated, wrk));
        }
        if (d == "channel" || d == "both" || d == "all") {
            rows.push_back(run_async("async-chan", lc, args.workers_per_shard, Dispatch::Channel, wrk));
        }
    }
    print_results(rows);
    return 0;
}
