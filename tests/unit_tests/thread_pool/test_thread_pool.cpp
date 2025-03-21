#include <chrono>
#include <gtest/gtest.h>
#include <vector>

#include "thread_pool.hpp"

using namespace demiplane::multithread;

// Test: Constructor initializes pool with correct thread count
TEST(ThreadPoolTest, ConstructorInitialization) {
    ThreadPool pool(2, 4);

    // Verify that no exceptions are thrown
    EXPECT_NO_THROW(ThreadPool(2, 4));
}

// Test: Invalid constructor parameters throw exception
TEST(ThreadPoolTest, InvalidConstructorParameters) {
    EXPECT_THROW(ThreadPool(5, 3), std::invalid_argument); // min > max
    EXPECT_THROW(ThreadPool(0, 0), std::invalid_argument); // max == 0
}

// Test: Tasks are executed correctly
TEST(ThreadPoolTest, TaskExecution) {
    ThreadPool pool(2, 4);

    auto result = pool.enqueue([] { return 42; });
    EXPECT_EQ(result.get(), 42);
}

// Test: Tasks execute in order of priority
TEST(ThreadPoolTest, PriorityExecution) {
    ThreadPool pool(1, 1);

    std::vector<int> results;
    std::mutex result_mutex;

    pool.enqueue(
        [&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            std::lock_guard lock(result_mutex);
            results.push_back(1);
        },
        ThreadPool::TaskPriority::Low);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.enqueue(
        [&] {
            std::lock_guard lock(result_mutex);
            results.push_back(2);
        },
        ThreadPool::TaskPriority::High);

    pool.enqueue(
        [&] {
            std::lock_guard lock(result_mutex);
            results.push_back(3);
        },
        ThreadPool::TaskPriority::Extreme);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_EQ(results.size(), 3);
    EXPECT_EQ(results[0], 1); // Extreme priority first
    EXPECT_EQ(results[1], 3); // High priority second
    EXPECT_EQ(results[2], 2); // Low priority last
}

// Test: Thread pool can scale up to max threads
TEST(ThreadPoolTest, ScalingThreads) {
    ThreadPool pool(2, 4);

    std::vector<std::future<void>> results;

    results.reserve(10);
    for (int i = 0; i < 10; ++i) {
        results.push_back(pool.enqueue([] { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }));
    }

    for (auto& res : results) {
        res.get(); // Wait for all tasks to finish
    }

    EXPECT_TRUE(true); // No errors should occur
}

// Test: Pool shuts down gracefully
TEST(ThreadPoolTest, ShutdownGracefully) {
    ThreadPool pool(2, 4);

    auto result = pool.enqueue([] { return 42; });
    EXPECT_EQ(result.get(), 42);

    // Check for graceful shutdown
    EXPECT_NO_THROW(pool.shutdown());
}

// Test: Pool throws after shutdown
TEST(ThreadPoolTest, ThrowsAfterShutdown) {
    ThreadPool pool(2, 4);
    pool.shutdown();

    EXPECT_THROW(pool.enqueue([] { return 42; }), std::runtime_error);
}

// Test: Task exceptions propagate
TEST(ThreadPoolTest, TaskExceptionPropagation) {
    ThreadPool pool(2, 4);

    auto result = pool.enqueue([] { throw std::runtime_error("Task error"); });

    EXPECT_THROW(result.get(), std::runtime_error);
}
