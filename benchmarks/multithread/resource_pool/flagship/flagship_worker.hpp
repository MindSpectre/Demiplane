#pragma once

// Shared infrastructure for the async dispatch workers (pull / pull_dedicated / channel):
// the pool type alias, the WorkerContext reference-bundle every worker shares, and the
// held-work step they all run while holding a slot.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <demiplane/multithread>  // AsyncResourcePool
#include <memory>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>

#include "common/bench_latency.hpp"
#include "common/mock_resource.hpp"
#include "flagship_config.hpp"  // FlagshipConfig, Work
#include "flagship_producer.hpp"
#include "ws_harness.hpp"

namespace bench::pool {

    using AsyncPoolT = demiplane::multithread::AsyncResourcePool<MockResource, 1024>;

    /// State every dispatch worker references — a value bundle of references. Cheap to copy
    /// per spawn; the referenced objects all live in run_async's scope and outlive every
    /// coroutine (drained via the live_coros==0 barrier before that scope destructs). The
    /// per-shard collector vectors are indexed by the worker's shard index `i`.
    struct WorkerContext {
        AsyncPoolT& pool;
        std::vector<LatencyCollector>& collectors;
        std::vector<LatencyCollector>& disp_wait;
        std::vector<LatencyCollector>& queue_wait;
        std::vector<LatencyCollector>& acq_wait;
        std::vector<LatencyCollector>& work_ns;
        std::atomic<std::int64_t>& processed;
        std::atomic<std::int64_t>& dropped;
        std::atomic<std::size_t>& live_coros;
        std::vector<std::unique_ptr<Disruptor>>& disruptors;
        std::atomic<bool>& producer_done;
        WsHarness* ws;  // null in Spin mode
        const FlagshipConfig& cfg;
    };

    /// The held-work step, identical across all three dispatch workers.
    /// Spin: work_for(work_duration) + co_await post.  Ws: async_write a tiny frame + record it.
    inline boost::asio::awaitable<void> do_work(const boost::asio::any_io_executor& exec,
                                                const Work mode,
                                                const std::chrono::nanoseconds work_duration,
                                                MockResource& res,
                                                WsHarness* ws,
                                                const std::size_t gw,
                                                LatencyCollector& work_collector) {
        if (mode == Work::Ws) {
            const std::uint64_t tw0 = TscClock::now();
            co_await ws->client(gw).async_write(boost::asio::buffer(WS_PAYLOAD), boost::asio::use_awaitable);
            work_collector.record(TscClock::to_duration(TscClock::now() - tw0));
        } else {
            res.work_for(work_duration);
            co_await boost::asio::post(exec, boost::asio::use_awaitable);
        }
        co_return;
    }

}  // namespace bench::pool
