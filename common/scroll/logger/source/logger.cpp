#include "logger.hpp"

#include "console_sink.hpp"
#include "light_entry.hpp"

namespace demiplane::scroll {
    void Logger::shutdown() {
        if (!running_.load(std::memory_order_acquire)) {
            return;  // Already shut down
        }

        // 1. Send shutdown signal through disruptor
        const std::int64_t seq                        = disruptor_.sequencer().next();
        disruptor_.ring_buffer()[seq].shutdown_signal = true;
        disruptor_.sequencer().publish(seq);

        // 2. Wait for consumer loop to exit (drains ring buffer, posts to strands)
        if (consumer_thread_.joinable()) {
            consumer_thread_.join();
        }

        // 3. Drain all pending strand work, then flush each sink.
        //    Since strands are serial, this flush runs AFTER all previously
        //    posted process() calls complete — no events are lost.
        if (!sink_slots_.empty()) {
            std::atomic<std::size_t> remaining{sink_slots_.size()};
            std::promise<void> done;

            for (auto& [sink, strand] : sink_slots_) {
                boost::asio::post(strand, [&sink, &remaining, &done] {
                    sink->flush();
                    if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        done.set_value();
                    }
                });
            }

            done.get_future().wait();
        }

        // 4. Shutdown internal pool if we own it
        if (owned_pool_) {
            owned_pool_->join();
        }

        running_.store(false, std::memory_order_release);
    }
    void Logger::consumer_loop() {
        std::int64_t next_seq = 0;
        while (running_.load(std::memory_order_acquire)) {
            const std::int64_t available =
                disruptor_.sequencer().get_highest_published(next_seq, disruptor_.sequencer().get_cursor());

            if (available < next_seq) {
                // Nothing published yet — back off using the configured wait strategy
                // (BusySpin / Yielding / Blocking) instead of tight-spinning
                std::ignore = disruptor_.sequencer().wait_for(next_seq);
                continue;
            }

            // Collect batch: move all available events out of ring buffer
            auto batch = std::make_shared<std::vector<LogEvent>>();
            batch->reserve(static_cast<std::size_t>(available - next_seq + 1));

            std::int64_t last_consumed = next_seq - 1;
            for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                auto& event = disruptor_.ring_buffer()[seq];

                if (event.shutdown_signal) {
                    disruptor_.sequencer().mark_consumed(seq);
                    last_consumed = seq;
                    running_.store(false, std::memory_order_release);
                    break;
                }

                batch->emplace_back(std::move(event));
                disruptor_.sequencer().mark_consumed(seq);
                last_consumed = seq;
            }

            // Bulk gating update — producers see all freed slots at once
            disruptor_.sequencer().update_gating_sequence(last_consumed);
            next_seq = last_consumed + 1;

            // Post batch to each sink's strand (one post per sink, not per event)
            if (!batch->empty()) {
                for (auto& [sink, strand] : sink_slots_) {
                    boost::asio::post(strand, [sink, batch] { sink->process_batch(batch); });
                }
            }
        }
    }
}  // namespace demiplane::scroll
