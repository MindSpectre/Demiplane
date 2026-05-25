#pragma once

// PullDedicated dispatch: each worker drains its OWN disruptor (SPSC, no shared claim
// cursor / CAS). dispatch == run (no hand-off). Records into the shard's collectors (`i`).

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "flagship/flagship_producer.hpp"
#include "flagship/flagship_worker.hpp"

namespace bench::pool {

    inline void spawn_pull_dedicated_worker(const boost::asio::any_io_executor& exec,
                                            WorkerContext ctx,
                                            const std::size_t i,     // shard index (for collectors)
                                            const std::size_t didx,  // own disruptor index
                                            const std::size_t gw) {
        boost::asio::co_spawn(
            exec,
            [exec, ctx, i, didx, gw]() -> boost::asio::awaitable<void> {
                using namespace std::chrono_literals;
                Disruptor& d = *ctx.disruptors[didx];
                boost::asio::steady_timer poll{exec};
                std::int64_t next_seq = 0;
                for (;;) {
                    const std::int64_t cursor = d.sequencer().get_cursor();
                    const std::int64_t avail  = d.sequencer().get_highest_published(next_seq, cursor);
                    if (avail >= next_seq) {
                        for (std::int64_t seq = next_seq; seq <= avail; ++seq) {
                            const std::uint64_t t_run = TscClock::now();
                            const BurstEvent ev       = d.ring_buffer()[seq];
                            auto lease = co_await ctx.pool.async_acquire_for(exec, ctx.cfg.acquire_timeout);
                            if (lease.has_value()) {
                                const std::uint64_t t_start = TscClock::now();
                                ctx.disp_wait[i].record(TscClock::to_duration(t_run - ev.t_produced));
                                ctx.queue_wait[i].record(std::chrono::nanoseconds{0});  // no hand-off
                                ctx.acq_wait[i].record(TscClock::to_duration(t_start - t_run));
                                ctx.collectors[i].record(TscClock::to_duration(t_start - ev.t_produced));
                                co_await do_work(
                                    exec, ctx.cfg.work, ctx.cfg.work_duration, **lease, ctx.ws, gw, ctx.work_ns[i]);
                            } else {
                                ctx.dropped.fetch_add(1, std::memory_order_relaxed);
                            }
                            ctx.processed.fetch_add(1, std::memory_order_relaxed);
                        }
                        next_seq = avail + 1;
                        d.sequencer().update_gating_sequence(avail);
                    } else if (ctx.producer_done.load(std::memory_order_acquire)) {
                        break;  // drained
                    } else {
                        poll.expires_after(20us);
                        co_await poll.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));
                    }
                }
                ctx.live_coros.fetch_sub(1, std::memory_order_acq_rel);  // SINGLE exit
                co_return;
            },
            boost::asio::detached);
    }

}  // namespace bench::pool
