#pragma once

// Shared model for the flagship burst benchmark: the disruptor payload, the result/
// decomposition statistics + reporting, and the producer loop. Run configuration lives in
// common/flagship_config.hpp. Used by both the sync runner (sync/burst_sync.hpp) and the
// async runner (async/burst_async.hpp). Header-only; included by the single flagship TU.

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <demiplane/chrono>
#include <memory>
#include <span>
#include <thread>
#include <variant>

#include "flagship_utils.hpp"
namespace bench::pool {


    // Disruptor-feeding producer, shared by the sync and async runners. Round-robins
    // timestamped records across `disruptors`; Burst dumps `size` then sleeps `cooldown`,
    // Steady emits at a uniform rate on absolute deadlines. The caller pins the producer
    // thread before calling. Sets `producer_done` once every record is published.
    inline void run_producer(const std::variant<Burst, Steady>& arrival,
                             const std::span<const std::unique_ptr<Disruptor>> disruptors,
                             std::atomic<std::int64_t>& produced,
                             std::atomic<bool>& producer_done) {
        const std::size_t n_disruptors = disruptors.size();
        if (const auto* steady = std::get_if<Steady>(&arrival)) {
            // Uniform arrival: one record every (1s / rps). Absolute deadlines so jitter
            // doesn't accumulate; t_produced is stamped at the actual push, so emission
            // jitter never biases the measured latency.
            const auto interval = std::chrono::nanoseconds{1'000'000'000 / steady->rps};
            const auto t_emit0  = std::chrono::steady_clock::now();
            for (std::int64_t k = 0; k < steady->total; ++k) {
                std::this_thread::sleep_until(t_emit0 + interval * k);
                Disruptor& d           = *disruptors[static_cast<std::size_t>(k) % n_disruptors];
                const std::int64_t seq = d.sequencer().next();
                d.ring_buffer()[seq]   = BurstEvent{TscClock::now()};
                d.sequencer().publish(seq);
                produced.fetch_add(1, std::memory_order_release);
            }
        } else {
            const auto& [count, size, cooldown] = std::get<Burst>(arrival);
            for (std::int64_t b = 0; b < count; ++b) {
                for (std::int64_t k = 0; k < size; ++k) {
                    Disruptor& d           = *disruptors[static_cast<std::size_t>(k) % n_disruptors];
                    const std::int64_t seq = d.sequencer().next();
                    d.ring_buffer()[seq]   = BurstEvent{TscClock::now()};
                    d.sequencer().publish(seq);
                }
                produced.fetch_add(size, std::memory_order_release);
                std::this_thread::sleep_for(cooldown);
            }
        }
        producer_done.store(true, std::memory_order_release);
    }

}  // namespace bench::pool
