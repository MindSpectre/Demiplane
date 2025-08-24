#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <thread>
#include <chrono>
#include <sliding_window_sorter.hpp>

using namespace demiplane::algorithms; // Updated namespace

// Test data structures
struct TestData {
    int value;
    std::string label;

    TestData(int v, const std::string& l)
        : value(v),
          label(l) {}

    explicit TestData(const int v)
        : value(v),
          label(std::to_string(v)) {}

    bool operator<(const TestData& other) const {
        return value < other.value;
    }

    bool operator==(const TestData& other) const {
        return value == other.value && label == other.label;
    }

    static bool comp(const TestData& a, const TestData& b) {
        return a.value < b.value;
    }
};

struct ReverseData {
    int value;

    ReverseData(int v)
        : value(v) {}

    bool operator<(const ReverseData& other) const {
        return value > other.value; // Reverse order
    }

    bool operator==(const ReverseData& other) const {
        return value == other.value;
    }
};

class SlidingWindowSorterTest : public ::testing::Test {
protected:
    std::vector<std::vector<int>> captured_outputs;
    std::vector<std::vector<TestData>> captured_test_data_outputs;

    void SetUp() override {
        captured_outputs.clear();
        captured_test_data_outputs.clear();
    }

    auto make_int_consumer() {
        return [this](const std::vector<int>& output) {
            captured_outputs.push_back(output);
        };
    }

    auto make_test_data_consumer() {
        return [this](const std::vector<TestData>& output) {
            captured_test_data_outputs.push_back(output);
        };
    }
};

// Test DefaultComparator
TEST_F(SlidingWindowSorterTest, DefaultComparatorWithBuiltinTypes) {
    DefaultComparator<int> comp;
    EXPECT_TRUE(comp(1, 2));
    EXPECT_FALSE(comp(2, 1));
    EXPECT_FALSE(comp(1, 1));
}

TEST_F(SlidingWindowSorterTest, DefaultComparatorWithCustomComp) {
    DefaultComparator<TestData> comp;
    TestData a(1, "a");
    TestData b(2, "b");
    EXPECT_TRUE(comp(a, b));
    EXPECT_FALSE(comp(b, a));
    EXPECT_FALSE(comp(a, a));
}

TEST_F(SlidingWindowSorterTest, DefaultComparatorWithOperatorLess) {
    DefaultComparator<ReverseData> comp;
    ReverseData a(1);
    ReverseData b(2);
    EXPECT_FALSE(comp(a, b)); // 1 > 2 in reverse order
    EXPECT_TRUE(comp(b, a));
}

// Test basic configuration
TEST_F(SlidingWindowSorterTest, BasicConfiguration) {
    SlidingWindowConfig<int> config;
    config.window_size = 100;
    config.batch_size  = 50;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    // Add some entries - this should trigger processing since batch_size=50 and we add 5
    sorter.add_entries({5, 1, 3, 2, 4});

    // No output yet because batch_size=50 but we only added 5 entries
    EXPECT_EQ(captured_outputs.size(), 0);

    sorter.flush();

    ASSERT_EQ(captured_outputs.size(), 1);
    std::vector<int> expected = {1, 2, 3, 4, 5};
    EXPECT_EQ(captured_outputs[0], expected);
}

// Test exact sliding window behavior as described
TEST_F(SlidingWindowSorterTest, ExactSlidingWindowBehavior) {
    SlidingWindowConfig<int> config;
    config.window_size = 6;
    config.batch_size  = 3;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    // Step 1: [3,1,1] → sort to [1,1,3], no flush (window size not exceeded)
    sorter.add_entries({3, 1, 1});
    EXPECT_EQ(captured_outputs.size(), 0); // No output yet

    // Step 2: [2,6,5] → sort to [2,5,6] → merge → window=[1,1,2,3,5,6] → flush oldest 3
    sorter.add_entries({2, 6, 5});
    EXPECT_EQ(captured_outputs.size(), 1);
    EXPECT_EQ(captured_outputs[0], (std::vector<int>{1, 1, 2})); // Oldest 3 entries

    // Step 3: [3,4,1] → sort to [1,3,4] → merge → window=[1,3,3,4,5,6] → flush oldest 3
    sorter.add_entries({3, 4, 1});
    EXPECT_EQ(captured_outputs.size(), 2);
    EXPECT_EQ(captured_outputs[1], (std::vector<int>{1, 3, 3})); // Next oldest 3 entries
}

// Test single entry addition with correct batching
TEST_F(SlidingWindowSorterTest, SingleEntryAddition) {
    SlidingWindowConfig<int> config;
    config.window_size = 10;
    config.batch_size  = 3;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    // Add entries one by one
    sorter.add_entry(3);
    sorter.add_entry(1);
    EXPECT_EQ(captured_outputs.size(), 0); // Not enough for batch

    sorter.add_entry(2);                   // Triggers first batch processing
    EXPECT_EQ(captured_outputs.size(), 0); // Window not exceeded yet

    sorter.add_entry(4);
    sorter.add_entry(5);
    sorter.add_entry(6); // Triggers second batch processing

    // Now window=[1,2,3,4,5,6] (6 entries), need to flush 3 to maintain window_size=10
    // Since window_size=10 > 6, no flush yet
    EXPECT_EQ(captured_outputs.size(), 0);

    sorter.flush();
    EXPECT_EQ(captured_outputs.size(), 1);
    EXPECT_EQ(captured_outputs[0], (std::vector<int>{1, 2, 3, 4, 5, 6}));
}

// Test empty input handling
TEST_F(SlidingWindowSorterTest, EmptyInputHandling) {
    SlidingWindowConfig<int> config;
    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    sorter.add_entries({});
    sorter.flush();

    EXPECT_EQ(captured_outputs.size(), 0);
}

// Test sorting disabled
TEST_F(SlidingWindowSorterTest, SortingDisabled) {
    SlidingWindowConfig<int> config;
    config.enable_sorting = false;
    config.window_size    = 6;
    config.batch_size     = 3;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    // First batch: [5,1,3] (no sorting)
    sorter.add_entries({5, 1, 3});
    EXPECT_EQ(captured_outputs.size(), 0); // Window not exceeded

    // Second batch: [2,4,6] → window=[5,1,3,2,4,6] → flush first 3
    sorter.add_entries({2, 4, 6});
    EXPECT_EQ(captured_outputs.size(), 1);
    EXPECT_EQ(captured_outputs[0], (std::vector<int>{5, 1, 3})); // Original order, no sorting
}

// Test custom comparator (reverse order)
TEST_F(SlidingWindowSorterTest, CustomComparator) {
    SlidingWindowConfig<int> config;
    config.window_size = 10;
    config.batch_size  = 5;
    config.comparator  = [](const int& a, const int& b) {
        return a > b; // Reverse order
    };

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    sorter.add_entries({1, 2, 3, 4, 5});
    EXPECT_EQ(captured_outputs.size(), 0); // Window not exceeded

    sorter.flush();
    ASSERT_EQ(captured_outputs.size(), 1);
    std::vector<int> expected = {5, 4, 3, 2, 1}; // Reverse sorted
    EXPECT_EQ(captured_outputs[0], expected);
}

// Test window overflow behavior
TEST_F(SlidingWindowSorterTest, WindowOverflowBehavior) {
    SlidingWindowConfig<int> config;
    config.window_size = 4; // Small window
    config.batch_size  = 3;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    // First batch: [3,1,2] → sorted [1,2,3], window=[1,2,3] (size 3 ≤ 4)
    sorter.add_entries({3, 1, 2});
    EXPECT_EQ(captured_outputs.size(), 0);

    // Second batch: [6,4,5] → sorted [4,5,6] → merged window=[1,2,3,4,5,6]
    // Window size exceeds 4, so flush oldest entries to maintain window_size=4
    sorter.add_entries({6, 4, 5});
    EXPECT_EQ(captured_outputs.size(), 1);

    // Should output enough entries to bring window back to acceptable size
    // Window was [1,2,3,4,5,6] (6 entries), need to remove 2+3=5 entries to get to batch_size or less
    EXPECT_EQ(captured_outputs[0].size(), 3); // Should output batch_size entries
    EXPECT_EQ(captured_outputs[0], (std::vector<int>{1, 2, 3}));
}

// Test batch processing triggers
TEST_F(SlidingWindowSorterTest, BatchProcessingTriggers) {
    SlidingWindowConfig<int> config;
    config.window_size = 20; // Large window to avoid early flushing
    config.batch_size  = 3;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    // Add exactly batch_size entries
    sorter.add_entries({3, 1, 2});
    EXPECT_EQ(captured_outputs.size(), 0); // Window not exceeded

    // Add more entries
    sorter.add_entry(4);
    sorter.add_entry(5);
    EXPECT_EQ(captured_outputs.size(), 0); // Still under batch_size

    sorter.add_entry(6);                   // This should trigger processing
    EXPECT_EQ(captured_outputs.size(), 0); // But still no output due to large window

    sorter.flush();
    EXPECT_EQ(captured_outputs.size(), 1);
    EXPECT_EQ(captured_outputs[0], (std::vector<int>{1, 2, 3, 4, 5, 6}));
}

// Test flush functionality
TEST_F(SlidingWindowSorterTest, FlushFunctionality) {
    SlidingWindowConfig<int> config;
    config.window_size = 100; // Large window
    config.batch_size  = 10;  // Large batch size

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    sorter.add_entries({5, 1, 3});
    EXPECT_EQ(captured_outputs.size(), 0); // Not enough for batch and window not exceeded

    sorter.flush();
    EXPECT_EQ(captured_outputs.size(), 1); // Forced processing
    EXPECT_EQ(captured_outputs[0], (std::vector<int>{1, 3, 5}));
}

// Test statistics tracking
TEST_F(SlidingWindowSorterTest, StatisticsTracking) {
    SlidingWindowConfig<int> config;
    config.window_size = 10;
    config.batch_size  = 3;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    auto stats = sorter.get_statistics();
    EXPECT_EQ(stats.total_processed, 0);
    EXPECT_EQ(stats.sort_operations, 0);
    EXPECT_EQ(stats.merge_operations, 0);

    // First batch
    sorter.add_entries({3, 1, 2});
    stats = sorter.get_statistics();
    EXPECT_EQ(stats.total_processed, 0);  // No output yet
    EXPECT_EQ(stats.sort_operations, 1);  // First batch sorted
    EXPECT_EQ(stats.merge_operations, 0); // No merge yet

    // Second batch - should trigger merge
    sorter.add_entries({6, 4, 5});
    stats = sorter.get_statistics();
    EXPECT_EQ(stats.sort_operations, 2);  // Second batch sorted
    EXPECT_EQ(stats.merge_operations, 1); // First merge operation
}

// Test reconfiguration
TEST_F(SlidingWindowSorterTest, Reconfiguration) {
    SlidingWindowConfig<int> config;
    config.window_size = 10;
    config.batch_size  = 3;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    sorter.add_entries({3, 1});
    EXPECT_EQ(captured_outputs.size(), 0);

    // Reconfigure with new batch size
    config.batch_size = 2;
    sorter.reconfigure(config);
    EXPECT_EQ(captured_outputs.size(), 1); // Should flush before reconfiguring
    EXPECT_EQ(captured_outputs[0], (std::vector<int>{1, 3}));

    // New configuration should be active
    sorter.add_entries({5, 4});            // Should trigger with new batch_size=2
    EXPECT_EQ(captured_outputs.size(), 1); // But no output yet due to window

    sorter.add_entries({6, 7});            // Another batch
    EXPECT_EQ(captured_outputs.size(), 1); // Still no output

    sorter.flush();
    EXPECT_EQ(captured_outputs.size(), 2);
    EXPECT_EQ(captured_outputs[1], (std::vector<int>{4, 5, 6, 7}));
}

// Test window size maintenance
TEST_F(SlidingWindowSorterTest, WindowSizeMaintenance) {
    SlidingWindowConfig<int> config;
    config.window_size = 5;
    config.batch_size  = 3;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    // Add data in multiple batches to test window maintenance
    sorter.add_entries({3, 1, 2}); // First batch: window=[1,2,3]
    EXPECT_EQ(captured_outputs.size(), 0);

    sorter.add_entries({6, 4, 5}); // Second batch: merged=[1,2,3,4,5,6], exceeds window_size=5
    EXPECT_EQ(captured_outputs.size(), 1);
    EXPECT_EQ(captured_outputs[0], (std::vector<int>{1, 2, 3})); // Flush to maintain window size

    sorter.add_entries({9, 7, 8}); // Third batch: merged=[4,5,6,7,8,9], exceeds again
    EXPECT_EQ(captured_outputs.size(), 2);
    EXPECT_EQ(captured_outputs[1], (std::vector<int>{4, 5, 6})); // Flush again

    sorter.flush(); // Flush remaining
    EXPECT_EQ(captured_outputs.size(), 3);
    EXPECT_EQ(captured_outputs[2], (std::vector<int>{7, 8, 9}));
}

// Test complex data types
TEST_F(SlidingWindowSorterTest, ComplexDataTypes) {
    SlidingWindowConfig<TestData> config;
    config.window_size = 10;
    config.batch_size  = 3;

    SlidingWindowSorter<TestData> sorter(config, make_test_data_consumer());

    std::vector<TestData> data = {
        TestData(3, "three"),
        TestData(1, "one"),
        TestData(2, "two")
    };

    sorter.add_entries(std::move(data));
    EXPECT_EQ(captured_test_data_outputs.size(), 0); // Window not exceeded

    sorter.flush();
    ASSERT_EQ(captured_test_data_outputs.size(), 1);
    EXPECT_EQ(captured_test_data_outputs[0][0].value, 1);
    EXPECT_EQ(captured_test_data_outputs[0][1].value, 2);
    EXPECT_EQ(captured_test_data_outputs[0][2].value, 3);
}

// Test large dataset with proper windowing
TEST_F(SlidingWindowSorterTest, LargeDatasetHandling) {
    SlidingWindowConfig<int> config;
    config.window_size = 100;
    config.batch_size  = 50;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    // Add 200 elements in reverse order
    for (int i = 200; i > 0; --i) {
        sorter.add_entry(i);
    }
    sorter.flush();

    // Verify all data was processed
    size_t total_processed = 0;
    for (const auto& output : captured_outputs) {
        total_processed += output.size();
        // Verify each output is sorted
        EXPECT_TRUE(std::ranges::is_sorted(output));
    }
    EXPECT_EQ(total_processed, 200);

    // Verify the final result is completely sorted
    std::vector<int> all_output;
    for (const auto& output : captured_outputs) {
        all_output.insert(all_output.end(), output.begin(), output.end());
        for (int d : all_output) {
            std::cout << d << " ";
        }
        std::cout << std::endl;
    }
    EXPECT_TRUE(std::is_sorted(all_output.begin(), all_output.end()));
}

// Test duplicate handling
TEST_F(SlidingWindowSorterTest, DuplicateHandling) {
    SlidingWindowConfig<int> config;
    config.window_size = 10;
    config.batch_size  = 5;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    sorter.add_entries({3, 1, 2, 1, 3});
    EXPECT_EQ(captured_outputs.size(), 0); // Window not exceeded

    sorter.add_entries({2, 1}); // Total 7 elements, still under window_size
    EXPECT_EQ(captured_outputs.size(), 0);

    sorter.flush();
    ASSERT_EQ(captured_outputs.size(), 1);
    std::vector<int> expected = {1, 1, 1, 2, 2, 3, 3};
    EXPECT_EQ(captured_outputs[0], expected);
}

// Test edge cases
TEST_F(SlidingWindowSorterTest, EdgeCases) {
    SlidingWindowConfig<int> config;
    config.window_size = 1;
    config.batch_size  = 1;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    // Single element should trigger immediate output due to window_size=1
    sorter.add_entry(42);
    EXPECT_EQ(captured_outputs.size(), 1);
    EXPECT_EQ(captured_outputs[0], std::vector<int>{42});

    // Add another element
    sorter.add_entry(10);
    EXPECT_EQ(captured_outputs.size(), 2);
    EXPECT_EQ(captured_outputs[1], std::vector<int>{10});
}

// Corrected stress test
TEST_F(SlidingWindowSorterTest, RandomDataStressTest) {
    SlidingWindowConfig<int> config;
    config.window_size = 50; // Smaller window to force regular flushing
    config.batch_size  = 20;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    std::random_device rd;
    std::mt19937 gen(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<> dis(1, 1000);

    std::vector<int> all_input;

    // Generate and add random batches
    for (int i = 0; i < 10; ++i) {
        std::vector<int> batch;
        for (int j = 0; j < 25; ++j) {
            int val = dis(gen);
            batch.push_back(val);
            all_input.push_back(val);
        }
        sorter.add_entries(batch);
    }

    sorter.flush();

    // Verify all data was processed and outputs are sorted
    std::vector<int> all_output;
    for (const auto& output : captured_outputs) {
        EXPECT_TRUE(std::is_sorted(output.begin(), output.end()));
        all_output.insert(all_output.end(), output.begin(), output.end());
    }

    EXPECT_EQ(all_output.size(), all_input.size());

    // Verify the final result matches a complete sort of input
    std::sort(all_input.begin(), all_input.end());
    EXPECT_TRUE(std::is_sorted(all_output.begin(), all_output.end()));
    EXPECT_EQ(all_output, all_input);
}

// Test efficiency metrics
TEST_F(SlidingWindowSorterTest, EfficiencyMetrics) {
    SlidingWindowConfig<int> config;
    config.window_size = 50;
    config.batch_size  = 10;

    SlidingWindowSorter<int> sorter(config, make_int_consumer());

    // Add several batches to generate statistics
    for (int batch = 0; batch < 5; ++batch) {
        std::vector<int> data;
        for (int i = 0; i < 10; ++i) {
            data.push_back(batch * 10 + i);
        }
        sorter.add_entries(data);
    }
    sorter.flush();

    auto stats = sorter.get_statistics();
    EXPECT_EQ(stats.sort_operations, 5);  // 5 batches sorted
    EXPECT_EQ(stats.merge_operations, 4); // 4 merge operations (after first batch)
    EXPECT_EQ(stats.total_processed, 50);

    // Test avg_merge_efficiency calculation
    EXPECT_EQ(stats.avg_merge_efficiency, 4.0 / 5.0); // merge_operations / sort_operations
}