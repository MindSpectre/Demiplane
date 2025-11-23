#include "logger.hpp"

namespace demiplane::scroll {
    void Logger::shutdown() {
        if (!running_.load(std::memory_order_acquire)) {
            return;  // Already shut down
        }

        // Send shutdown signal
        const std::int64_t seq            = sequencer_.next();
        ring_buffer_[seq].shutdown_signal = true;
        sequencer_.publish(seq);

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
            const std::int64_t available = sequencer_.get_highest_published(next_seq, sequencer_.get_cursor());

            // Process batch
            for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                const auto& event = ring_buffer_[seq];

                if (event.shutdown_signal) {
                    running_.store(false, std::memory_order_release);
                    break;
                }

                // Dispatch to all sinks
                for (const auto& sink : sinks_) {
                    sink->process(event);
                }

                sequencer_.mark_consumed(seq);
            }

            if (available >= next_seq) {
                next_seq = available + 1;
                sequencer_.update_gating_sequence(available);
            }
        }

        // Flush all sinks on shutdown
        for (const auto& sink : sinks_) {
            sink->flush();
        }
    }
}  // namespace demiplane::scroll
