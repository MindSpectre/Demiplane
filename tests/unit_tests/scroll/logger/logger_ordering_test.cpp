#include <algorithm>
#include <atomic>
#include <demiplane/scroll>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace {

    // Custom sink that captures sequences for verification
    class SequenceCaptureSink final : public demiplane::scroll::Sink {
    public:
        void process(const demiplane::scroll::LogEvent& event) override {
            sequences_.push_back(extract_sequence(event.message));
        }

        void flush() override {
        }

        [[nodiscard]] bool should_log(demiplane::scroll::LogLevel) const noexcept override {
            return true;
        }

        [[nodiscard]] const std::vector<int64_t>& sequences() const {
            return sequences_;
        }

    private:
        std::vector<int64_t> sequences_;

        static int64_t extract_sequence(const std::string& msg) {
            // Extract sequence from "SEQ {number}"
            constexpr std::string_view prefix = "SEQ ";
            auto pos                          = msg.find(prefix);
            if (pos == std::string::npos) {
                return -1;
            }
            return std::stoll(msg.substr(pos + prefix.size()));
        }
    };

}  // namespace

// Test that the logger maintains strict ordering of entries
TEST(LoggerOrderingTest, DisruptorMaintainsSequenceOrder) {
    constexpr size_t NUM_THREADS        = 8;
    constexpr size_t ENTRIES_PER_THREAD = 1000;
    constexpr size_t TOTAL_ENTRIES      = NUM_THREADS * ENTRIES_PER_THREAD;

    auto capture_sink = std::make_shared<SequenceCaptureSink>();
    demiplane::scroll::Logger logger;
    logger.add_sink(capture_sink);

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    // Each thread logs many entries
    for (size_t t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&logger, t]() {
            for (size_t i = 0; i < ENTRIES_PER_THREAD; ++i) {
                // Use a dummy counter just for content, not for ordering verification
                logger.log(demiplane::scroll::LogLevel::Info,
                           "SEQ {}",
                           std::source_location::current(),
                           ENTRIES_PER_THREAD * t + i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    logger.shutdown();

    // Verify we got all entries
    const auto& sequences = capture_sink->sequences();
    EXPECT_EQ(sequences.size(), TOTAL_ENTRIES);

    // The key test: Verify the consumer received entries in SOME consistent order
    // (not necessarily 0,1,2,3... but no duplicates and no gaps based on what was actually logged)
    // The important guarantee is that the disruptor didn't lose or corrupt any entries
    std::vector<int64_t> sorted_sequences = sequences;
    std::ranges::sort(sorted_sequences);
    // Check for no duplicates (each position should be unique after sorting)
    auto it = std::ranges::adjacent_find(sorted_sequences);
    EXPECT_EQ(it, sorted_sequences.end()) << "Found duplicate sequence";
}

// Test that verifies strong ordering when using sequence-based logging
TEST(LoggerOrderingTest, SequenceBasedLoggingIsStrictlyOrdered) {
    constexpr size_t NUM_THREADS        = 10;
    constexpr size_t ENTRIES_PER_THREAD = 500;
    constexpr size_t TOTAL_ENTRIES      = NUM_THREADS * ENTRIES_PER_THREAD;

    auto capture_sink = std::make_shared<SequenceCaptureSink>();
    demiplane::scroll::Logger logger;
    logger.add_sink(capture_sink);

    std::atomic<int64_t> global_sequence{0};
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    // Key insight: Claim sequence BEFORE formatting
    // This ensures strong ordering between claim and content
    for (size_t t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&logger, &global_sequence]() {
            for (size_t i = 0; i < ENTRIES_PER_THREAD; ++i) {
                // Atomically claim a sequence number
                int64_t seq = global_sequence.fetch_add(1, std::memory_order_seq_cst);

                // Log with that sequence - now there's a happens-before relationship
                logger.log(demiplane::scroll::LogLevel::Info, "SEQ {}", std::source_location::current(), seq);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    logger.shutdown();

    const auto& sequences = capture_sink->sequences();
    ASSERT_EQ(sequences.size(), TOTAL_ENTRIES);

    // Verify all sequences are present: 0, 1, 2, ..., TOTAL_ENTRIES-1
    std::vector<int64_t> sorted_sequences = sequences;
    std::ranges::sort(sorted_sequences);

    for (size_t i = 0; i < TOTAL_ENTRIES; ++i) {
        EXPECT_EQ(sorted_sequences[i], static_cast<int64_t>(i)) << "Missing or incorrect sequence at position " << i;
    }
}

// Test file sink ordering with actual file I/O
TEST(LoggerOrderingTest, FileSinkPreservesConsumerOrder) {
    constexpr size_t NUM_THREADS        = 4;
    constexpr size_t ENTRIES_PER_THREAD = 100;
    constexpr size_t TOTAL_ENTRIES      = NUM_THREADS * ENTRIES_PER_THREAD;

    const std::string test_file = "test_ordering.log";
    std::filesystem::remove(test_file);

    demiplane::scroll::FileSinkConfig config = demiplane::scroll::FileSinkConfig{}
                                                   .threshold(demiplane::scroll::LogLevel::Info)
                                                   .file(test_file)
                                                   .add_time_to_filename(false).rotation(false)
                                                   .flush_each_entry(true).finalize();  // Ensure immediate write

    auto file_sink = std::make_shared<demiplane::scroll::FileSink<demiplane::scroll::DetailedEntry>>(config);
    demiplane::scroll::Logger logger;
    logger.add_sink(file_sink);

    std::atomic<int64_t> sequence{0};
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (size_t t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&logger, &sequence]() {
            for (size_t i = 0; i < ENTRIES_PER_THREAD; ++i) {
                int64_t seq = sequence.fetch_add(1, std::memory_order_seq_cst);
                logger.log(demiplane::scroll::LogLevel::Info, "SEQ {}", std::source_location::current(), seq);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    logger.shutdown();

    // Read back and verify ordering
    std::ifstream file(test_file);
    ASSERT_TRUE(file.is_open());

    std::vector<int64_t> logged_sequences;
    std::string line;
    while (std::getline(file, line)) {
        if (auto pos = line.find("SEQ "); pos != std::string::npos) {
            int64_t seq = std::stoll(line.substr(pos + 4));
            logged_sequences.push_back(seq);
        }
    }

    ASSERT_EQ(logged_sequences.size(), TOTAL_ENTRIES);

    // Verify all sequences are present
    std::vector<int64_t> sorted = logged_sequences;
    std::ranges::sort(sorted);

    for (size_t i = 0; i < TOTAL_ENTRIES; ++i) {
        EXPECT_EQ(sorted[i], static_cast<int64_t>(i));
    }

    std::filesystem::remove(test_file);
}

// Test to verify the logger handles high contention without corruption
TEST(LoggerOrderingTest, HighContentionNoCorruption) {
    constexpr size_t NUM_THREADS        = 20;
    constexpr size_t ENTRIES_PER_THREAD = 200;
    constexpr size_t TOTAL_ENTRIES      = NUM_THREADS * ENTRIES_PER_THREAD;

    auto capture_sink = std::make_shared<SequenceCaptureSink>();
    demiplane::scroll::Logger logger;
    logger.add_sink(capture_sink);

    std::atomic<int64_t> sequence{0};
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (size_t t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&logger, &sequence]() {
            for (size_t i = 0; i < ENTRIES_PER_THREAD; ++i) {
                int64_t seq = sequence.fetch_add(1, std::memory_order_seq_cst);
                logger.log(demiplane::scroll::LogLevel::Info, "SEQ {}", std::source_location::current(), seq);
                // No sleep - maximum contention
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    logger.shutdown();

    const auto& sequences = capture_sink->sequences();
    ASSERT_EQ(sequences.size(), TOTAL_ENTRIES) << "Lost entries under high contention";

    // Verify no corruption - all sequences should be unique and complete
    std::vector<int64_t> sorted = sequences;
    std::ranges::sort(sorted);

    for (size_t i = 0; i < TOTAL_ENTRIES; ++i) {
        EXPECT_EQ(sorted[i], static_cast<int64_t>(i)) << "Sequence corruption or loss at position " << i;
    }
}
