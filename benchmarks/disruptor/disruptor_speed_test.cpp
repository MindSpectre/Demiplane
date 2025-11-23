#include <atomic>
#include <barrier>
#include <iomanip>
#include <iostream>
#include <vector>

#include <disruptor.hpp>

using namespace demiplane::multithread;

/*==============================================================================
 * SINGLE CONSUMER - ONE-AT-A-TIME (Baseline)
 *============================================================================*/
void single_consumer_baseline_test() {
    constexpr std::int64_t BUFFER_SIZE          = 8192;
    constexpr std::int64_t NUM_PRODUCERS        = 8;
    constexpr std::int64_t ENTRIES_PER_PRODUCER = 1'000'000;
    constexpr std::int64_t TOTAL_ENTRIES        = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;

    RingBuffer<std::int64_t, BUFFER_SIZE> ring_buffer;
    MultiProducerSequencer<BUFFER_SIZE> sequencer{std::make_unique<BusySpinWaitStrategy>()};

    std::barrier sync_point{NUM_PRODUCERS + 1};
    std::atomic<bool> running{true};

    // Single consumer thread
    std::thread consumer{[&]() {
        sync_point.arrive_and_wait();

        std::int64_t next_seq           = 0;
        std::int64_t processed          = 0;
        constexpr std::int64_t expected = TOTAL_ENTRIES;

        while (running.load(std::memory_order_acquire) || processed < expected) {
            const std::int64_t cursor = sequencer.get_cursor();

            if (const std::int64_t available = sequencer.get_highest_published(next_seq, cursor);
                available >= next_seq) {
                // Process batch
                for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                    [[maybe_unused]] std::int64_t value = ring_buffer[seq];
                    sequencer.mark_consumed(seq);
                }

                processed += (available - next_seq + 1);
                next_seq   = available + 1;
                sequencer.update_gating_sequence(available);
            }
        }
    }};

    // Producers - one at a time
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);

    const auto start_time = std::chrono::steady_clock::now();

    for (std::int64_t tid = 0; tid < NUM_PRODUCERS; ++tid) {
        producers.emplace_back([&, tid]() {
            sync_point.arrive_and_wait();

            for (std::int64_t i = 0; i < ENTRIES_PER_PRODUCER; ++i) {
                const std::int64_t seq = sequencer.next();  // One CAS per entry
                ring_buffer[seq]       = static_cast<std::int64_t>(tid * ENTRIES_PER_PRODUCER + i);
                sequencer.publish(seq);
            }
        });
    }

    for (auto& p : producers) {
        p.join();
    }

    running.store(false, std::memory_order_release);
    consumer.join();

    const auto elapsed       = std::chrono::steady_clock::now() - start_time;
    const double elapsed_sec = std::chrono::duration<double>(elapsed).count();
    const double throughput  = TOTAL_ENTRIES / elapsed_sec;

    std::cout << "\n╔════════════════════════════════════════╗\n";
    std::cout << "║   Baseline: One-at-a-time Publishing  ║\n";
    std::cout << "╠════════════════════════════════════════╣\n";
    std::cout << "║ Producers:         " << std::setw(18) << NUM_PRODUCERS << "  ║\n";
    std::cout << "║ Consumers:         " << std::setw(18) << 1 << "  ║\n";
    std::cout << "║ Entries/producer:  " << std::setw(18) << ENTRIES_PER_PRODUCER << "  ║\n";
    std::cout << "║ Total entries:     " << std::setw(18) << TOTAL_ENTRIES << "  ║\n";
    std::cout << "║ Buffer size:       " << std::setw(18) << BUFFER_SIZE << "  ║\n";
    std::cout << "╠════════════════════════════════════════╣\n";
    std::cout << "║ Elapsed time:      " << std::fixed << std::setprecision(3) << std::setw(13) << elapsed_sec
              << " s    ║\n";
    std::cout << "║ Throughput:        " << std::fixed << std::setprecision(0) << std::setw(10) << throughput
              << " ops/s ║\n";
    std::cout << "║ Avg latency:       " << std::fixed << std::setprecision(2) << std::setw(13)
              << (elapsed_sec * 1e9 / TOTAL_ENTRIES) << " ns   ║\n";
    std::cout << "╚════════════════════════════════════════╝\n";
}

/*==============================================================================
 * SINGLE CONSUMER - BATCHED PUBLISHING
 *============================================================================*/
void single_consumer_batched_test() {
    constexpr std::int64_t BUFFER_SIZE          = 8192;
    constexpr std::int64_t NUM_PRODUCERS        = 4;
    constexpr std::int64_t ENTRIES_PER_PRODUCER = 10'000'000;
    constexpr std::int64_t TOTAL_ENTRIES        = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;
    constexpr std::int64_t BATCH_SIZE           = 16;  // Claim 64 sequences at once

    RingBuffer<std::int64_t, BUFFER_SIZE> ring_buffer;
    MultiProducerSequencer<BUFFER_SIZE> sequencer{std::make_unique<BusySpinWaitStrategy>()};

    std::barrier sync_point{NUM_PRODUCERS + 1};
    std::atomic<bool> running{true};

    // Single consumer thread (same as before - consumer side unchanged)
    std::thread consumer{[&]() {
        sync_point.arrive_and_wait();

        std::int64_t next_seq           = 0;
        std::int64_t processed          = 0;
        constexpr std::int64_t expected = static_cast<std::int64_t>(TOTAL_ENTRIES);

        while (running.load(std::memory_order_acquire) || processed < expected) {
            const std::int64_t cursor = sequencer.get_cursor();

            if (const std::int64_t available = sequencer.get_highest_published(next_seq, cursor);
                available >= next_seq) {
                // Process batch
                for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                    [[maybe_unused]] std::int64_t value = ring_buffer[seq];
                    sequencer.mark_consumed(seq);
                }

                processed += (available - next_seq + 1);
                next_seq   = available + 1;
                sequencer.update_gating_sequence(available);
            }
        }
    }};

    // Producers - batched claiming
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);

    const auto start_time = std::chrono::steady_clock::now();

    for (std::int64_t tid = 0; tid < NUM_PRODUCERS; ++tid) {
        producers.emplace_back([&, tid]() {
            sync_point.arrive_and_wait();

            for (std::int64_t i = 0; i < ENTRIES_PER_PRODUCER; i += BATCH_SIZE) {
                // Claim batch of sequences (ONE CAS for entire batch!)
                const std::int64_t first_seq = sequencer.next_batch(BATCH_SIZE);

                // Fill the batch
                for (std::int64_t j = 0; j < BATCH_SIZE && (i + j) < ENTRIES_PER_PRODUCER; ++j) {
                    const std::int64_t seq = first_seq + j;
                    ring_buffer[seq]       = tid * ENTRIES_PER_PRODUCER + i + j;
                }

                // Publish entire batch at once
                const std::int64_t last_seq = first_seq + static_cast<std::int64_t>(BATCH_SIZE) - 1;
                sequencer.publish_batch(first_seq, last_seq);
            }
        });
    }

    for (auto& p : producers) {
        p.join();
    }

    running.store(false, std::memory_order_release);
    consumer.join();

    const auto elapsed       = std::chrono::steady_clock::now() - start_time;
    const double elapsed_sec = std::chrono::duration<double>(elapsed).count();
    const double throughput  = TOTAL_ENTRIES / elapsed_sec;

    std::cout << "\n╔════════════════════════════════════════╗\n";
    std::cout << "║   Optimized: Batched Publishing (64)   ║\n";
    std::cout << "╠════════════════════════════════════════╣\n";
    std::cout << "║ Producers:         " << std::setw(18) << NUM_PRODUCERS << "  ║\n";
    std::cout << "║ Consumers:         " << std::setw(18) << 1 << "  ║\n";
    std::cout << "║ Batch size:        " << std::setw(18) << BATCH_SIZE << "  ║\n";
    std::cout << "║ Entries/producer:  " << std::setw(18) << ENTRIES_PER_PRODUCER << "  ║\n";
    std::cout << "║ Total entries:     " << std::setw(18) << TOTAL_ENTRIES << "  ║\n";
    std::cout << "║ Buffer size:       " << std::setw(18) << BUFFER_SIZE << "  ║\n";
    std::cout << "╠════════════════════════════════════════╣\n";
    std::cout << "║ Elapsed time:      " << std::fixed << std::setprecision(3) << std::setw(13) << elapsed_sec
              << " s    ║\n";
    std::cout << "║ Throughput:        " << std::fixed << std::setprecision(0) << std::setw(10) << throughput
              << " ops/s ║\n";
    std::cout << "║ Avg latency:       " << std::fixed << std::setprecision(2) << std::setw(13)
              << (elapsed_sec * 1e9 / TOTAL_ENTRIES) << " ns   ║\n";
    std::cout << "╚════════════════════════════════════════╝\n";
}


int main() {
    std::cout << "Running Disruptor Performance Benchmarks...\n";

    single_consumer_baseline_test();

    single_consumer_batched_test();

    return 0;
}
