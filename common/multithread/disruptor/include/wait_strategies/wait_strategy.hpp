#pragma once

#include <chrono>
#include <condition_variable>
#include <demiplane/gears>
#include <mutex>
#include <thread>

#include "../sequence.hpp"

namespace demiplane::multithread {

    /**
     * @brief Base interface for consumer wait strategies.
     *
     * ## Concept: Trade-off Between Latency and CPU Usage
     *
     * When the consumer catches up to the producer (no data available),
     * it must wait. Different strategies have different characteristics:
     *
     * | Strategy      | Latency | CPU Usage | Power Consumption | Use Case |
     * |---------------|---------|-----------|-------------------|----------|
     * | BusySpin      | ~50ns   | 100%      | Very High         | Trading systems, ultra-low latency |
     * | Yielding      | ~200ns  | 50-100%   | High              | Balanced performance (RECOMMENDED) |
     * | Blocking      | ~5μs    | Near 0%   | Low               | Background processing, batch jobs |
     *
     * ## How It Works
     *
     * Consumer loop:
     * ```
     * while (true) {
     *     int64_t available = wait_for(next_sequence, producer_cursor);
     *     // Process [next_sequence, available]
     *     next_sequence = available + 1;
     * }
     * ```
     *
     * Producer signals:
     * ```
     * publish(sequence);
     * wait_strategy.signal();  // Wake up consumer
     * ```
     */
    class WaitStrategy {
    public:
        virtual ~WaitStrategy() = default;

        /**
         * @brief Wait for sequence to become available
         * @param sequence Sequence we're waiting for
         * @param cursor Producer's cursor (what's been claimed)
         * @param dependent_sequence Other sequences we depend on (e.g., previous stage)
         * @return Highest available sequence (>= sequence)
         *
         * Example:
         * - We want sequence 100
         * - Producer cursor is at 105
         * - We can immediately return 105 (sequences 100-105 are available)
         *
         * - We want sequence 100
         * - Producer cursor is at 99
         * - We must WAIT until producer advances to >= 100
         */
        virtual std::int64_t
        wait_for(std::int64_t sequence, const Sequence& cursor, const Sequence* dependent_sequence) = 0;

        virtual std::int64_t wait_for(std::int64_t sequence, const Sequence& cursor) = 0;

        /**
         * @brief Signal waiting consumers that new data is available
         *
         * Called by producers after publishing data.
         * Implementation-specific (no-op for spinning strategies, notify for blocking).
         */
        virtual void signal() noexcept = 0;

        /**
         * @brief Signal all waiting consumers (for shutdown)
         */
        virtual void signal_all() noexcept = 0;
    };







}  // namespace demiplane::multithread
