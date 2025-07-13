#include <functional>
#include <gtest/gtest.h>
#include <random>
#include <thread>

#include "stopwatch.hpp"

using namespace demiplane::chrono;

class StopwatchTest : public ::testing::Test {
protected:
    Stopwatch<> stopwatch;

    void SetUp() override {
        // Initialize stopwatch before each test
        stopwatch = Stopwatch(20);
    }

    void TearDown() override {
        // Clean up after each test if needed
    }

    // Helper function to simulate work for a specified duration
    static void sleepFor(const int milliseconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
};

// Test basic functionality - start, add flag, stop
TEST_F(StopwatchTest, BasicFunctionality) {
    stopwatch.start();
    sleepFor(10);
    stopwatch.add_flag();
    sleepFor(10);
    auto flags = stopwatch.stop();

    // Should have 3 flags: start, add_flag, and stop
    EXPECT_EQ(flags.size(), 3);
}

// Test delta_t functionality
TEST_F(StopwatchTest, DeltaTime) {
    stopwatch.start();
    sleepFor(50);
    stopwatch.add_flag();

    auto delta = stopwatch.delta_t(1);

    // The delta_since_prev should be approximately 50ms
    // Use a tolerance for timing variations
    EXPECT_GE(delta.first.count(), 40);
    EXPECT_LE(delta.first.count(), 60);

    // The delta_since_start should be the same as delta_since_prev here
    EXPECT_GE(delta.second.count(), 40);
    EXPECT_LE(delta.second.count(), 60);
}

// Test delta_t with invalid index
TEST_F(StopwatchTest, DeltaTimeInvalidIndex) {
    stopwatch.start();

    // Index 0 should return zeros
    auto delta = stopwatch.delta_t(0);
    EXPECT_EQ(delta.first.count(), 0);
    EXPECT_EQ(delta.second.count(), 0);

    // Out of range index should return zeros
    delta = stopwatch.delta_t(100);
    EXPECT_EQ(delta.first.count(), 0);
    EXPECT_EQ(delta.second.count(), 0);
}

// Test get_flags
TEST_F(StopwatchTest, GetFlags) {
    stopwatch.start();
    stopwatch.add_flag();
    stopwatch.add_flag();

    const auto& flags = stopwatch.get_flags();
    EXPECT_EQ(flags.size(), 3);
}

// Test average_delta
TEST_F(StopwatchTest, AverageDelta) {
    stopwatch.add_flag();
    sleepFor(10);
    stopwatch.add_flag();
    sleepFor(20);
    stopwatch.add_flag();
    sleepFor(30);
    stopwatch.add_flag();

    auto avg = stopwatch.average_delta();

    // Average should be approximately (10+20+30)/3 = 20ms
    // Using a tolerance for system timing variations
    EXPECT_GE(avg.count(), 19);
    EXPECT_LE(avg.count(), 21);
}

// Test the new measure function
TEST_F(StopwatchTest, MeasureLambda) {
    // Measure a lambda that sleeps for 50ms
    auto duration = stopwatch.measure([]() { sleepFor(50); });

    // Check that the duration is approximately 50ms
    EXPECT_GE(duration.count(), 45);
    EXPECT_LE(duration.count(), 60);
}

// Test measure with complex logic
TEST_F(StopwatchTest, MeasureComplexLogic) {
    long long result = 0;

    const auto duration = stopwatch.measure([&result] {
        // Do some computational work
        std::uniform_int_distribution dist(0, 5);
        for (std::size_t i = 0; i < 10000; ++i) {
            std::random_device rd;
            std::mt19937 gen(rd());
            result += dist(gen);
        }
    });

    // Verify that the work was done
    EXPECT_GT(result, 0);

    // Duration should be greater than 0
    EXPECT_GT(duration.count(), 0);
}

// Test measure with function reference
TEST_F(StopwatchTest, MeasureFunctionReference) {
    // Define a function to be measured
    std::function<void()> testFunc = []() { sleepFor(20); };

    auto duration = stopwatch.measure(testFunc);

    // Check duration
    EXPECT_GE(duration.count(), 15);
    EXPECT_LE(duration.count(), 30);
}

// Test measure with multiple calls
TEST_F(StopwatchTest, MeasureMultipleCalls) {
    // Measure multiple different durations
    auto d1 = stopwatch.measure([]() { sleepFor(10); });
    auto d2 = stopwatch.measure([]() { sleepFor(20); });
    auto d3 = stopwatch.measure([]() { sleepFor(30); });

    // Each duration should be approximately its sleep time
    EXPECT_GE(d1.count(), 5);
    EXPECT_LE(d1.count(), 20);

    EXPECT_GE(d2.count(), 15);
    EXPECT_LE(d2.count(), 30);

    EXPECT_GE(d3.count(), 25);
    EXPECT_LE(d3.count(), 40);
}

// Test that measure doesn't interfere with stopwatch flags
TEST_F(StopwatchTest, MeasureDoesntChangeFlags) {
    stopwatch.start();
    stopwatch.add_flag();

    auto flagsBefore = stopwatch.get_flags().size();

    // Measure something
    stopwatch.measure([] { sleepFor(10); });

    auto flagsAfter = stopwatch.get_flags().size();

    // Flag count should remain the same
    EXPECT_EQ(flagsBefore, flagsAfter);
}
