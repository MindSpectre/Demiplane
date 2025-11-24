#pragma once

#include "wait_strategy.hpp"

namespace demiplane::multithread {

    /**
     * @brief Yielding wait strategy - balanced latency and CPU usage.
     *
     * ## How It Works
     *
     * ```
     * attempt = 0
     * while (cursor.get() < sequence) {
     *     if (++attempt > 100) {
     *         std::this_thread::yield();  // Give up CPU slice
     *     }
     * }
     * ```
     *
     * Spins briefly, then yields CPU to other threads.
     *
     * ## CPU Behavior
     * - First 100 attempts: Busy spin (~50ns each)
     * - After 100 attempts: yield() -> context switch (~1-5μs)
     * - If no other threads: yield() returns immediately
     * - If other threads ready: OS schedules them
     *
     * ## std::this_thread::yield() Explained
     *
     * Tells OS: "I'm waiting, schedule someone else if they're ready"
     * - NOT a sleep (no timer)
     * - NOT a block (no waiting on condition)
     * - Just a hint to scheduler
     *
     * Best case: No context switch, returns in ~100ns
     * Worst case: Context switch, returns in ~5μs
     *
     * ## When To Use
     * - **RECOMMENDED DEFAULT** for most use cases
     * - Shared CPU cores with other workloads
     * - Good balance of latency (<1μs) and CPU efficiency
     * - Examples: Logging, metrics, event processing
     */
    class YieldingWaitStrategy final : public WaitStrategy {
    public:
        std::int64_t wait_for(const std::int64_t sequence, const Sequence& cursor) override {
            std::int64_t available_sequence;
            int spin_tries = 0;

            // Spin for a bit before yielding
            while ((available_sequence = cursor.get()) < sequence) {
                if (++spin_tries > 100) {
                    // Yielding reduces CPU usage but adds latency
                    std::this_thread::yield();
                    spin_tries = 0;  // Reset counter after yield
                }
            }

            return available_sequence;
        }


        void signal() noexcept override {
            // No-op: yield() is not blockable
        }

        void signal_all() noexcept override {
            // No-op
        }

    private:
        std::int64_t wait_for(const std::int64_t sequence, const Sequence& cursor, const Sequence* dependent) override {
            gears::unused_value(sequence, cursor, dependent);
            throw std::logic_error{"YieldingWaitStrategy does not support dependent sequences"};
        }
    };


}  // namespace demiplane::multithread
