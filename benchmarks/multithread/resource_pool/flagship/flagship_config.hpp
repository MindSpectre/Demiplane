#pragma once

// Single source of truth for the flagship burst benchmark's run configuration. Every tunable
// lives here; the arrival pattern is a variant so steady-vs-burst is unambiguous — each
// alternative carries only its own knobs. No mutable globals: the config is built once in
// main() and threaded (by const reference) into the runners.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace bench::pool {

    // How worker coroutines receive records from the disruptor.
    enum class Dispatch {
        Pull,           // workers share one disruptor per shard via an atomic claim cursor (CAS)
        PullDedicated,  // each worker owns its own disruptor (SPSC, no shared claim/CAS)
        Channel,        // one reader per shard fans out via an unbuffered asio channel
    };

    // What the worker does while holding a slot.
    enum class Work {
        Spin,  // work_for(work_duration) busy-spin + co_await post (synthetic)
        Ws,    // co_await beast websocket async_write of a tiny frame (real, yields)
    };

    // ─── Arrival pattern: a variant. Each alternative carries only its own knobs. ───
    struct Burst {
        static constexpr std::int64_t k   = 10;
        std::int64_t count                = 20 * k;                          // number of bursts
        std::int64_t size                 = 3750 / k;                        // records dumped per burst
        std::chrono::nanoseconds cooldown = std::chrono::milliseconds{500};  // idle between bursts
    };
    struct Steady {
        static constexpr std::int64_t k = 5;
        std::int64_t rps                = 125 * k;    // uniform arrival rate (records/sec)
        std::int64_t total              = 15000 * k;  // total records
    };

    // ─── The one run config: every flagship setting in one place. ───
    struct FlagshipConfig {
        std::size_t shards                       = 10;     // io threads / sync consumers (cores 0..shards-1)
        std::size_t disruptor_buf                = 16384;  // per-disruptor ring size (power of 2)
        std::variant<Burst, Steady> arrival      = Steady{};
        std::size_t workers_per_shard            = 8;  // async: pre-spawned coroutine workers per shard
        Dispatch dispatch                        = Dispatch::Pull;
        Work work                                = Work::Ws;
        std::size_t pool_size                    = 128;                           // pool connections
        std::chrono::nanoseconds acquire_timeout = std::chrono::microseconds{2};  // acquire_for timeout
        std::chrono::nanoseconds work_duration   = std::chrono::microseconds{1};  // synthetic work (Spin)
    };

    // Total records an arrival produces (for collector sizing).
    inline std::int64_t total_records(const std::variant<Burst, Steady>& arrival) {
        if (const auto* b = std::get_if<Burst>(&arrival)) {
            return b->count * b->size;
        }
        return std::get<Steady>(arrival).total;
    }

    // Budget for the `drained` check ("did the pool keep up?"): a burst must clear within its
    // cooldown; steady arrival has no burst-pileup notion, so it is always considered drained.
    inline std::chrono::nanoseconds drain_budget(const std::variant<Burst, Steady>& arrival) {
        if (const auto* b = std::get_if<Burst>(&arrival)) {
            return b->cooldown;
        }
        return std::chrono::nanoseconds::max();
    }

}  // namespace bench::pool
