#pragma once

// Channel dispatch: one reader coroutine per shard drains the disruptor in order and hands
// records off over an unbuffered asio channel; the shard's workers receive from it. The
// reader is the ONLY code that closes the channel (on drain); workers exit on the resulting ec.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>

#include "flagship/flagship_producer.hpp"
#include "flagship/flagship_worker.hpp"

namespace bench::pool {

    using Chan = boost::asio::experimental::channel<void(boost::system::error_code, WorkItem)>;

    // Receives records handed off by the shard's reader; exits when the reader closes the
    // channel (observes ec). Must NOT close the channel itself.
    inline void spawn_channel_worker(const boost::asio::any_io_executor& exec,
                                     WorkerContext ctx,
                                     Chan& ch,
                                     const std::size_t i,
                                     const std::size_t gw) {
        boost::asio::co_spawn(
            exec,
            [exec, ctx, &ch, i, gw]() -> boost::asio::awaitable<void> {
                for (;;) {
                    auto [ec, item] = co_await ch.async_receive(boost::asio::as_tuple(boost::asio::use_awaitable));
                    if (ec) {
                        break;  // channel closed → shard drained
                    }
                    const std::uint64_t t_run = TscClock::now();
                    auto lease                = co_await ctx.pool.async_acquire_for(exec, ctx.cfg.acquire_timeout);
                    if (lease.has_value()) {
                        const std::uint64_t t_start = TscClock::now();
                        ctx.disp_wait[i].record(TscClock::to_duration(item.t_dispatch - item.t_produced));
                        ctx.queue_wait[i].record(TscClock::to_duration(t_run - item.t_dispatch));
                        ctx.acq_wait[i].record(TscClock::to_duration(t_start - t_run));
                        ctx.collectors[i].record(TscClock::to_duration(t_start - item.t_produced));
                        co_await do_work(
                            exec, ctx.cfg.work, ctx.cfg.work_duration, **lease, ctx.ws, gw, ctx.work_ns[i]);
                    } else {
                        ctx.dropped.fetch_add(1, std::memory_order_relaxed);
                    }
                    ctx.processed.fetch_add(1, std::memory_order_relaxed);
                }
                ctx.live_coros.fetch_sub(1, std::memory_order_acq_rel);  // SINGLE exit
                co_return;
            },
            boost::asio::detached);
    }

    // One per shard: in-order disruptor drain → channel hand-off (stamping t_dispatch).
    inline void spawn_channel_reader(const boost::asio::any_io_executor& exec,
                                     std::vector<std::unique_ptr<Disruptor>>& disruptors,
                                     std::atomic<bool>& producer_done,
                                     std::atomic<std::size_t>& live_coros,
                                     Chan& ch,
                                     const std::size_t i) {
        boost::asio::co_spawn(
            exec,
            [exec, &disruptors, &producer_done, &live_coros, &ch, i]() -> boost::asio::awaitable<void> {
                using namespace std::chrono_literals;
                Disruptor& d = *disruptors[i];
                boost::asio::steady_timer poll{exec};
                std::int64_t next_seq = 0;
                for (;;) {
                    const std::int64_t cursor = d.sequencer().get_cursor();
                    const std::int64_t avail  = d.sequencer().get_highest_published(next_seq, cursor);
                    if (avail >= next_seq) {
                        for (std::int64_t seq = next_seq; seq <= avail; ++seq) {
                            const BurstEvent ev = d.ring_buffer()[seq];
                            const WorkItem item{ev.t_produced, TscClock::now()};  // t_dispatch = hand-off
                            co_await ch.async_send(
                                boost::system::error_code{}, item, boost::asio::as_tuple(boost::asio::use_awaitable));
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
                ch.close();                                          // wake idle workers → they observe ec and exit
                live_coros.fetch_sub(1, std::memory_order_acq_rel);  // SINGLE exit
                co_return;
            },
            boost::asio::detached);
    }

}  // namespace bench::pool
