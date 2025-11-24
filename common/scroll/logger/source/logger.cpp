#include "logger.hpp"

namespace demiplane::scroll {
    void Logger::shutdown() {
        if (!running_.load(std::memory_order_acquire)) {
            return;  // Already shut down
        }

        // Send shutdown signal
        const std::int64_t seq            = disruptor_.sequencer().next();
        disruptor_.ring_buffer()[seq].shutdown_signal = true;
        disruptor_.sequencer().publish(seq);

        // Wait for consumer to finish
        if (consumer_thread_.joinable()) {
            consumer_thread_.join();
        }

        running_.store(false, std::memory_order_release);
    }
    void Logger::consumer_loop() {
        std::int64_t next_seq = 0;

        while (running_.load(std::memory_order_acquire)) {
            // Wait for published sequences
            const std::int64_t available = disruptor_.sequencer().get_highest_published(next_seq, disruptor_.sequencer().get_cursor());

            // Process batch
            for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                const auto& event = disruptor_.ring_buffer()[seq];

                if (event.shutdown_signal) {
                    running_.store(false, std::memory_order_release);
                    break;
                }

                // Dispatch to all sinks
                for (const auto& sink : sinks_) {
                    sink->process(event);
                }

                disruptor_.sequencer().mark_consumed(seq);
            }

            if (available >= next_seq) {
                next_seq = available + 1;
                disruptor_.sequencer().update_gating_sequence(available);
            }
        }

        // Flush all sinks on shutdown
        for (const auto& sink : sinks_) {
            sink->flush();
        }
    }
}  // namespace demiplane::scroll
