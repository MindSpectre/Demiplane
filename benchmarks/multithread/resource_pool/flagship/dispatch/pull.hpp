#pragma once

// Pull dispatch: workers share one disruptor per shard, claiming records via the shard's
// atomic cursor (uncontended within a single-threaded shard). dispatch == run (no hand-off).

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

    inline void spawn_pull_worker(const boost::asio::any_io_executor& exec,
                                  WorkerContext ctx,
                                  std::atomic<std::int64_t>& claim,
                                  const std::size_t i,
                                  const std::size_t gw) {
        boost::asio::co_spawn(
            exec,
            [exec, ctx, &claim, i, gw]() -> boost::asio::awaitable<void> {
                using namespace std::chrono_literals;
                Disruptor& d = *ctx.disruptors[i];
                boost::asio::steady_timer poll{exec};
                for (;;) {
                    std::int64_t seq = -1;
                    for (;;) {  // CAS-claim the next published sequence (uncontended in-shard)
                        const std::int64_t c     = claim.load(std::memory_order_acquire);
                        const std::int64_t avail = d.sequencer().get_highest_published(c, d.sequencer().get_cursor());
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
                        const std::uint64_t t_run = TscClock::now();  // pull: dispatch == run
                        const BurstEvent ev       = d.ring_buffer()[seq];
                        // Release backpressure on the SingleProducerSequencer: the event is copied
                        // out above, so the producer may now reuse slot `seq`. Advancing the gate to
                        // `seq` is monotonic and race-free because every worker on this shard shares
                        // one single-threaded executor — there is no co_await between the CAS-claim
                        // and this copy, so slots are claimed/copied strictly in increasing order.
                        // Without this the gate stays pinned at -1 and the producer deadlocks once a
                        // shard publishes buffer_size records.
                        d.sequencer().update_gating_sequence(seq);
                        auto lease                = co_await ctx.pool.async_acquire_for(exec, ctx.cfg.acquire_timeout);
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
                    } else if (ctx.producer_done.load(std::memory_order_acquire)) {
                        break;  // drained: no more will publish
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
