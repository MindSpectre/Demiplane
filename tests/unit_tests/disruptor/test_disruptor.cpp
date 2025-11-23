#include <algorithm>
#include <barrier>
#include <chrono>
#include <demiplane/multithread>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace demiplane::multithread;

/**
 * @brief Test fixture for Disruptor tests
 */
class DisruptorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup if needed
    }

    void TearDown() override {
        // Common cleanup if needed
    }
};

/* *==============================================================================
 * SEQUENCE TESTS - Cache-aligned atomic counters
 * *============================================================================*/

TEST_F(DisruptorTest, SequenceInitialization) {
    const Sequence seq{42};
    EXPECT_EQ(seq.get(), 42);

    const Sequence default_seq{};
    EXPECT_EQ(default_seq.get(), -1);  // Default is -1 (nothing published)
}

TEST_F(DisruptorTest, SequenceSetAndGet) {
    Sequence seq{0};
    seq.set(100);
    EXPECT_EQ(seq.get(), 100);

    seq.set(1000000);
    EXPECT_EQ(seq.get(), 1000000);
}

TEST_F(DisruptorTest, SequenceIncrementAndGet) {
    Sequence seq{0};

    EXPECT_EQ(seq.increment_and_get(), 1);
    EXPECT_EQ(seq.increment_and_get(), 2);
    EXPECT_EQ(seq.increment_and_get(), 3);
    EXPECT_EQ(seq.get(), 3);
}

TEST_F(DisruptorTest, SequenceAddAndGet) {
    Sequence seq{10};

    EXPECT_EQ(seq.add_and_get(5), 15);
    EXPECT_EQ(seq.add_and_get(10), 25);
    EXPECT_EQ(seq.get(), 25);
}

TEST_F(DisruptorTest, SequenceCompareAndSet) {
    Sequence seq{100};

    std::int64_t expected = 100;
    EXPECT_TRUE(seq.compare_and_set(expected, 200));
    EXPECT_EQ(seq.get(), 200);

    // CAS should fail if expected doesn't match
    expected = 100;  // Wrong value
    EXPECT_FALSE(seq.compare_and_set(expected, 300));
    EXPECT_EQ(expected, 200);   // Updated with actual value
    EXPECT_EQ(seq.get(), 200);  // Value unchanged
}

TEST_F(DisruptorTest, SequenceCacheLineAlignment) {
    // Verify cache-line alignment to prevent false sharing
    EXPECT_EQ(sizeof(Sequence), 64);
    EXPECT_EQ(alignof(Sequence), 64);

    // Verify multiple sequences don't share cache lines
    Sequence seq1{};
    Sequence seq2{};

    const auto addr1 = reinterpret_cast<uintptr_t>(&seq1);
    const auto addr2 = reinterpret_cast<uintptr_t>(&seq2);

    // They should be at least 64 bytes apart
    EXPECT_GE(std::abs(static_cast<long long>(addr2 - addr1)), 64);
}

/*==============================================================================
 * RING BUFFER TESTS - Power-of-2 circular buffer
 *============================================================================*/

TEST_F(DisruptorTest, RingBufferPowerOf2Sizing) {
    // These should compile (power of 2)
    RingBuffer<int, 4> tiny_buffer;
    RingBuffer<int, 1024> buffer;
    RingBuffer<int, 16384> large_buffer;

    EXPECT_EQ(tiny_buffer.capacity(), 4);
    EXPECT_EQ(buffer.capacity(), 1024);
    EXPECT_EQ(large_buffer.capacity(), 16384);

    // This would fail to compile (not power of 2):
    // RingBuffer<int, 1000> bad_buffer;  // Compile error!
}

TEST_F(DisruptorTest, RingBufferIndexWrapping) {
    constexpr size_t SIZE = 8;
    RingBuffer<int, SIZE> buffer;

    // Test wrapping behavior
    for (std::int64_t seq = 0; seq < 100; ++seq) {
        buffer[seq] = static_cast<int>(seq);
    }

    // Verify wrap-around: sequences 0, 8, 16, 24... map to index 0
    EXPECT_EQ(buffer[0], 96);  // Overwritten by seq 96 (96 % 8 = 0)
    EXPECT_EQ(buffer[8], 96);  // Same slot!
    EXPECT_EQ(buffer[16], 96);
    EXPECT_EQ(buffer[96], 96);

    // Verify different slots
    EXPECT_EQ(buffer[1], 97);   // seq 97 % 8 = 1
    EXPECT_EQ(buffer[99], 99);  // seq 99 % 8 = 3
}

TEST_F(DisruptorTest, RingBufferSequentialAccess) {
    RingBuffer<int, 16> buffer;

    // Write sequential data
    for (std::int64_t i = 0; i < 16; ++i) {
        buffer[i] = static_cast<int>(i * 10);
    }

    // Read back
    for (std::int64_t i = 0; i < 16; ++i) {
        EXPECT_EQ(buffer[i], i * 10);
    }
}

TEST_F(DisruptorTest, RingBufferGetVsOperator) {
    RingBuffer<int, 8> buffer;

    buffer.get(5) = 100;
    EXPECT_EQ(buffer[5], 100);
    EXPECT_EQ(buffer.get(5), 100);
}

/*==============================================================================
 * WAIT STRATEGY TESTS
 *============================================================================*/

TEST_F(DisruptorTest, BusySpinWaitStrategyNoBlock) {
    BusySpinWaitStrategy strategy;
    const Sequence cursor{10};

    // Requesting sequence <= cursor should return immediately
    const auto start         = std::chrono::steady_clock::now();
    const std::int64_t result = strategy.wait_for(5, cursor);
    const auto elapsed       = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(result, 10);
    EXPECT_LT(elapsed, std::chrono::microseconds{100});  // Should be nearly instant
}

TEST_F(DisruptorTest, YieldingWaitStrategyNoBlock) {
    YieldingWaitStrategy strategy;
    const Sequence cursor{10};

    const std::int64_t result = strategy.wait_for(5, cursor);
    EXPECT_EQ(result, 10);
}

TEST_F(DisruptorTest, BlockingWaitStrategyNoBlock) {
    BlockingWaitStrategy strategy;
    const Sequence cursor{10};

    const std::int64_t result = strategy.wait_for(5, cursor);
    EXPECT_EQ(result, 10);
}

TEST_F(DisruptorTest, BlockingWaitStrategyWithSignal) {
    BlockingWaitStrategy strategy;
    Sequence cursor{0};

    std::atomic<bool> consumer_ready{false};
    std::atomic<std::int64_t> result{-1};

    // Consumer thread - waits for sequence 10
    std::thread consumer{[&]() {
        consumer_ready.store(true);
        result.store(strategy.wait_for(10, cursor));
    }};

    // Wait for consumer to start waiting
    while (!consumer_ready.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    // Producer publishes sequence 10
    cursor.set(10);
    strategy.signal();

    consumer.join();
    EXPECT_EQ(result.load(), 10);
}

/*==============================================================================
 * MULTI-PRODUCER SEQUENCER TESTS - Claim/Publish Protocol
 *============================================================================*/

TEST_F(DisruptorTest, SequencerSingleClaim) {
    MultiProducerSequencer<1024> sequencer{std::make_unique<YieldingWaitStrategy>()};

    EXPECT_EQ(sequencer.get_cursor(), -1);  // Initial state (nothing claimed yet)

    const std::int64_t seq1 = sequencer.next();
    EXPECT_EQ(seq1, 0);

    const std::int64_t seq2 = sequencer.next();
    EXPECT_EQ(seq2, 1);

    const std::int64_t seq3 = sequencer.next();
    EXPECT_EQ(seq3, 2);
}

TEST_F(DisruptorTest, SequencerBatchClaim) {
    MultiProducerSequencer<1024> sequencer{std::make_unique<YieldingWaitStrategy>()};

    const std::int64_t first = sequencer.next_batch(5);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(sequencer.get_cursor(), 4);  // Claimed 0-4

    const std::int64_t second = sequencer.next_batch(3);
    EXPECT_EQ(second, 5);
    EXPECT_EQ(sequencer.get_cursor(), 7);  // Claimed 5-7
}

TEST_F(DisruptorTest, SequencerPublishAndAvailability) {
    MultiProducerSequencer<1024> sequencer{std::make_unique<YieldingWaitStrategy>()};

    const std::int64_t seq = sequencer.next();
    EXPECT_FALSE(sequencer.is_available(seq));  // Not published yet

    sequencer.publish(seq);
    EXPECT_TRUE(sequencer.is_available(seq));  // Now available
}

TEST_F(DisruptorTest, SequencerGapDetection) {
    /**
     * Test the critical gap detection logic:
     * - Thread A claims seq 0
     * - Thread B claims seq 1
     * - Thread B publishes seq 1 FIRST
     * - Consumer should NOT see seq 1 until seq 0 is published
     */
    MultiProducerSequencer<1024> sequencer{std::make_unique<YieldingWaitStrategy>()};

    const std::int64_t seq0 = sequencer.next();  // 0
    const std::int64_t seq1 = sequencer.next();  // 1
    const std::int64_t seq2 = sequencer.next();  // 2

    EXPECT_EQ(seq0, 0);
    EXPECT_EQ(seq1, 1);
    EXPECT_EQ(seq2, 2);

    // Publish out of order: 1, 2 (skip 0)
    sequencer.publish(seq1);
    sequencer.publish(seq2);

    // get_highest_published should find gap at seq0
    std::int64_t highest = sequencer.get_highest_published(0, 2);
    EXPECT_EQ(highest, -1);  // Gap at 0, so return -1 (0 - 1)

    // Now publish seq0
    sequencer.publish(seq0);

    // Now all are available
    highest = sequencer.get_highest_published(0, 2);
    EXPECT_EQ(highest, 2);  // All sequences 0-2 available
}

TEST_F(DisruptorTest, SequencerBackpressure) {
    /**
     * Test backpressure: when buffer is full, next() should block
     * until consumer advances gating sequence
     */
    constexpr size_t BUFFER_SIZE = 8;
    MultiProducerSequencer<BUFFER_SIZE> sequencer{std::make_unique<YieldingWaitStrategy>()};

    // Fill the buffer (claim 8 sequences)
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
        std::ignore = sequencer.next();
    }

    // Next claim would wrap around and overwrite seq 0
    // Should block until we update gating sequence
    std::atomic<bool> blocked{true};
    std::atomic<std::int64_t> claimed_seq{-1};

    std::thread producer{[&]() {
        claimed_seq.store(sequencer.next());  // This should block
        blocked.store(false);
    }};

    // Give producer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    EXPECT_TRUE(blocked.load());  // Should still be blocked

    // Consumer advances (consumed seq 0)
    sequencer.update_gating_sequence(0);

    // Now producer should unblock
    producer.join();
    EXPECT_FALSE(blocked.load());
    EXPECT_EQ(claimed_seq.load(), 8);  // Successfully claimed next sequence
}

TEST_F(DisruptorTest, SequencerRemainingCapacity) {
    constexpr size_t BUFFER_SIZE = 16;
    MultiProducerSequencer<BUFFER_SIZE> sequencer{std::make_unique<YieldingWaitStrategy>()};

    // Initially, full capacity available
    EXPECT_EQ(sequencer.remaining_capacity(), BUFFER_SIZE);

    // Claim 5 sequences
    for (int i = 0; i < 5; ++i) {
        std::ignore = sequencer.next();
    }

    EXPECT_EQ(sequencer.remaining_capacity(), BUFFER_SIZE - 5);

    // Consumer processes 3
    sequencer.update_gating_sequence(2);  // Consumed up to seq 2

    EXPECT_EQ(sequencer.remaining_capacity(), BUFFER_SIZE - 2);  // 3 consumed, 2 still pending
}

/*==============================================================================
 * MULTI-THREADED ORDERING TESTS - The Critical Test!
 *============================================================================*/

struct TestEntry {
    std::int64_t sequence;
    std::int64_t thread_id;
    std::int64_t timestamp_ns;
};

TEST_F(DisruptorTest, MultiThreadedStrictOrdering) {
    /**
     * THE MOST IMPORTANT TEST: Verify strict ordering with multiple producers
     *
     * Setup:
     * - 4 producer threads, each publishing 1000 entries
     * - 1 consumer thread processing in order
     * - Consumer must see ALL 4000 entries in strict sequence order
     */
    constexpr size_t BUFFER_SIZE       = 1024;
    constexpr int NUM_PRODUCERS        = 4;
    constexpr int ENTRIES_PER_PRODUCER = 1000;
    constexpr int TOTAL_ENTRIES        = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;

    RingBuffer<TestEntry, BUFFER_SIZE> ring_buffer;
    MultiProducerSequencer<BUFFER_SIZE> sequencer{std::make_unique<YieldingWaitStrategy>()};

    std::atomic<bool> start{false};
    std::atomic<int> ready_count{0};

    // Consumer results
    std::vector<TestEntry> consumed_entries;
    consumed_entries.reserve(TOTAL_ENTRIES);

    // Consumer thread
    std::thread consumer{[&]() {
        std::int64_t next_sequence = 0;
        int processed              = 0;

        while (processed < TOTAL_ENTRIES) {
            const std::int64_t cursor = sequencer.get_cursor();

            // Skip if nothing has been claimed yet
            if (cursor == -1) {
                std::this_thread::yield();
                continue;
            }

            // -1 means gap found, nothing available
            if (const std::int64_t available = sequencer.get_highest_published(next_sequence, cursor);
                available != -1 && available >= next_sequence) {
                // Process batch
                for (std::int64_t seq = next_sequence; seq <= available; ++seq) {
                    consumed_entries.push_back(ring_buffer[seq]);
                    sequencer.mark_consumed(seq);
                    ++processed;
                }

                next_sequence = available + 1;
                sequencer.update_gating_sequence(available);
            } else {
                // No data available, yield
                std::this_thread::yield();
            }
        }
    }};

    // Producer threads
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (std::int64_t tid = 0; tid < NUM_PRODUCERS; ++tid) {
        producers.emplace_back([&, tid]() {
            ready_count.fetch_add(1);

            // Wait for all producers to be ready
            while (!start.load()) {
                std::this_thread::yield();
            }

            // Publish entries
            for (int i = 0; i < ENTRIES_PER_PRODUCER; ++i) {
                const std::int64_t seq = sequencer.next();

                // Write entry
                ring_buffer[seq].sequence  = seq;
                ring_buffer[seq].thread_id = tid;
                ring_buffer[seq].timestamp_ns =
                    static_cast<std::int64_t>(std::chrono::steady_clock::now().time_since_epoch().count());

                // Publish
                sequencer.publish(seq);

                // Small delay to increase chance of out-of-order publishing
                if (i % 100 == 0) {
                    // std::this_thread::yield();
                }
            }
        });
    }

    // Wait for all producers to be ready
    while (ready_count.load() < NUM_PRODUCERS) {
        std::this_thread::yield();
    }

    // Start!
    start.store(true);

    // Wait for completion
    for (auto& p : producers) {
        p.join();
    }
    consumer.join();

    // VERIFY RESULTS
    ASSERT_EQ(consumed_entries.size(), TOTAL_ENTRIES);

    // Check strict sequence ordering
    for (size_t i = 0; i < consumed_entries.size(); ++i) {
        EXPECT_EQ(consumed_entries[i].sequence, static_cast<std::int64_t>(i))
            << "Entry at position " << i << " has wrong sequence number";
    }

    // Verify all thread IDs are present
    std::array<int, NUM_PRODUCERS> counts{};
    for (const auto& entry : consumed_entries) {
        ASSERT_GE(entry.thread_id, 0);
        ASSERT_LT(entry.thread_id, NUM_PRODUCERS);
        counts[static_cast<std::size_t>(entry.thread_id)]++;
    }

    for (std::int64_t tid = 0; tid < NUM_PRODUCERS; ++tid) {
        EXPECT_EQ(counts[static_cast<std::size_t>(tid)], ENTRIES_PER_PRODUCER)
            << "Thread " << tid << " published wrong number of entries";
    }
}

TEST_F(DisruptorTest, MultiThreadedHighContention) {
    /**
     * Stress test with many threads and high contention
     */
    constexpr size_t BUFFER_SIZE       = 512;
    constexpr int NUM_PRODUCERS        = 8;
    constexpr int ENTRIES_PER_PRODUCER = 500;
    constexpr int TOTAL_ENTRIES        = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;

    RingBuffer<std::int64_t, BUFFER_SIZE> ring_buffer{};
    MultiProducerSequencer<BUFFER_SIZE> sequencer{std::make_unique<YieldingWaitStrategy>()};

    std::barrier sync_point{NUM_PRODUCERS + 1};  // +1 for consumer
    std::vector<std::int64_t> consumed;
    consumed.reserve(TOTAL_ENTRIES);

    // Consumer
    std::thread consumer{[&]() {
        sync_point.arrive_and_wait();

        std::int64_t next_seq = 0;
        while (consumed.size() < TOTAL_ENTRIES) {
            const std::int64_t cursor = sequencer.get_cursor();


            // SIZE_MAX means gap found, nothing available
            if (const std::int64_t available = sequencer.get_highest_published(next_seq, cursor); available >= next_seq) {
                for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                    consumed.push_back(ring_buffer[seq]);
                    sequencer.mark_consumed(seq);
                }
                next_seq = available + 1;
                sequencer.update_gating_sequence(available);
            } else {
                std::this_thread::yield();
            }
        }
    }};

    // Producers
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (int tid = 0; tid < NUM_PRODUCERS; ++tid) {
        producers.emplace_back([&]() {
            sync_point.arrive_and_wait();

            for (int i = 0; i < ENTRIES_PER_PRODUCER; ++i) {
                const std::int64_t seq  = sequencer.next();
                ring_buffer[seq] = seq;
                sequencer.publish(seq);
            }
        });
    }

    for (auto& p : producers) {
        p.join();
    }
    consumer.join();

    // Verify strict ordering
    ASSERT_EQ(consumed.size(), TOTAL_ENTRIES);
    for (size_t i = 0; i < consumed.size(); ++i) {
        EXPECT_EQ(consumed[i], static_cast<std::int64_t>(i));
    }
}

TEST_F(DisruptorTest, TryNextNonBlocking) {
    /**
     * Test non-blocking try_next() behavior
     */
    constexpr size_t BUFFER_SIZE = 4;
    MultiProducerSequencer<BUFFER_SIZE> sequencer{std::make_unique<YieldingWaitStrategy>()};

    // Fill buffer
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
        std::int64_t seq = sequencer.try_next();
        EXPECT_NE(seq, -1);  // Should succeed
    }

    // Next try_next should fail (buffer full)
    std::int64_t seq = sequencer.try_next();
    EXPECT_EQ(seq, -1);  // Should fail without blocking

    // Advance consumer
    sequencer.update_gating_sequence(1);

    // Now should succeed again
    seq = sequencer.try_next();
    EXPECT_NE(seq, -1);  // Should succeed
}



int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
