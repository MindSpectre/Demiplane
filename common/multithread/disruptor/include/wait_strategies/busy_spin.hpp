#pragma once

#include "wait_strategy.hpp"

namespace demiplane::multithread {

    /**
     * @brief Busy-spin wait strategy - lowest latency, highest CPU usage.
     *
     * ## How It Works
     *
     * ```
     * while (cursor.get() < sequence) {
     *     // Do nothing, just keep checking (SPIN!)
     * }
     * ```
     *
     * CPU is 100% utilized, constantly checking memory.
     *
     * ## CPU Behavior
     * - No context switches
     * - No system calls
     * - L1 cache hit every iteration (~1-2 cycles)
     * - Memory ordering overhead (~5-10 cycles)
     * - Total: ~50-100ns per check
     *
     * ## When To Use
     * - Ultra-low latency required (<100ns)
     * - Dedicated CPU cores available
     * - High throughput (producer rarely behind)
     * - Examples: HFT trading, market data processing
     *
     * ## When NOT To Use
     * - Shared CPU cores (starves other threads)
     * - Battery-powered devices
     * - Low message rate (wastes power)
     */
    class BusySpinWaitStrategy final : public WaitStrategy {
    public:
        std::int64_t wait_for(std::int64_t sequence, const Sequence& cursor) override {
            std::int64_t available_sequence;

            // Tight spin loop - no pauses, no yields
            while ((available_sequence = cursor.get()) < sequence) {
                // Optional: CPU pause hint (x86: PAUSE instruction)
                // Reduces power and gives hyperthread a chance
                // std::this_thread::yield();  // Uncomment for slightly lower power
            }

            return available_sequence;
        }


        void signal() noexcept override {
            // No-op: Spinning threads will see the update via acquire load
        }

        void signal_all() noexcept override {
            // No-op: Nothing to wake up
        }

    private:
        std::int64_t wait_for(std::int64_t sequence, const Sequence& cursor, const Sequence* dependent) override {
            gears::unused_value(sequence, cursor, dependent);
            throw std::logic_error{"BusySpinWaitStrategy does not support dependent sequences"};
        }
    };

}  // namespace demiplane::multithread
