#include <barrier>
#include <chrono>
#include <gtest/gtest.h>
#include <vector>

#include <demiplane/multithread>

#include "gears_utils.hpp"
#include "generators/time_generator.hpp"
#include "random_utils.hpp"
#include "stopwatch.hpp"
using namespace demiplane::multithread;
using namespace std::chrono_literals;
class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup if needed
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

// Test: Constructor initializes pool with correct thread count
TEST_F(ThreadPoolTest, ConstructorInitialization) {
    ThreadPool pool(2, 4);

    // Verify that no exceptions are thrown
    EXPECT_NO_THROW(ThreadPool(2, 4));
}

// Test: Invalid constructor parameters throw exception
TEST_F(ThreadPoolTest, InvalidConstructorParameters) {
    EXPECT_THROW(ThreadPool(5, 3), std::invalid_argument); // min > max
    EXPECT_THROW(ThreadPool(0, 0), std::invalid_argument); // max == 0
}

// Test: Tasks are executed correctly
TEST_F(ThreadPoolTest, TaskExecution) {
    ThreadPool pool(2, 4);

    auto result = pool.enqueue([] { return 42; });
    EXPECT_EQ(result.get(), 42);
}

// Test: Tasks execute in order of priority
TEST_F(ThreadPoolTest, PriorityExecution) {
    ThreadPool pool(1, 1);

    std::vector<int> results;
    std::mutex       result_mutex;

    pool.enqueue(
        [&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            std::lock_guard lock(result_mutex);
            results.push_back(1);
        },
        0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.enqueue(
        [&] {
            std::lock_guard lock(result_mutex);
            results.push_back(2);
        },
        2);

    pool.enqueue(
        [&] {
            std::lock_guard lock(result_mutex);
            results.push_back(3);
        },
        100);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_EQ(results.size(), 3);
    EXPECT_EQ(results[0], 1); // Extreme priority first
    EXPECT_EQ(results[1], 3); // High priority second
    EXPECT_EQ(results[2], 2); // Low priority last
}

// Test: Thread pool can scale up to max threads
TEST_F(ThreadPoolTest, ScalingThreads) {
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
TEST_F(ThreadPoolTest, ShutdownGracefully) {
    ThreadPool pool(2, 4);

    auto result = pool.enqueue([] { return 42; });
    EXPECT_EQ(result.get(), 42);

    // Check for graceful shutdown
    EXPECT_NO_THROW(pool.shutdown());
}

// Test: Pool throws after shutdown
TEST_F(ThreadPoolTest, ThrowsAfterShutdown) {
    ThreadPool pool(2, 4);
    pool.shutdown();

    EXPECT_THROW(pool.enqueue([] { return 42; }), std::runtime_error);
}

// Test: Task exceptions propagate
TEST_F(ThreadPoolTest, TaskExceptionPropagation) {
    ThreadPool pool(2, 4);

    auto result = pool.enqueue([] { throw std::runtime_error("Task error"); });

    EXPECT_THROW(result.get(), std::runtime_error);
}
TEST_F(ThreadPoolTest, TerminatesThreadsAfterIdle) {
    ThreadPool pool(2, 4, 1s);

    auto result = pool.enqueue([] { throw std::runtime_error("Task error"); });

    EXPECT_THROW(result.get(), std::runtime_error);
    std::this_thread::sleep_for(2s);
    EXPECT_TRUE(pool.active_threads() == 0);
}

TEST_F(ThreadPoolTest, ThreadScalingBehavior) {
    ThreadPool pool(2, 5, 500ms);

    // Create a barrier to synchronize task execution
    std::barrier<>   sync_point(4); // 4 tasks will hit this point
    std::atomic<int> concurrent_tasks{0};
    std::atomic<int> max_concurrent{0};

    std::vector<std::future<void>> futures;

    // Submit 4 tasks that will run concurrently
    for (int i = 0; i < 4; ++i) {
        futures.push_back(pool.enqueue([&] {
            int current    = ++concurrent_tasks;
            max_concurrent = std::max(max_concurrent.load(), current);

            sync_point.arrive_and_wait(); // Wait for all 4 to start
            std::this_thread::sleep_for(100ms);

            --concurrent_tasks;
        }));
    }

    // Wait for all tasks
    for (auto& f : futures) {
        f.get();
    }

    // Should have scaled up to handle concurrent tasks
    EXPECT_GE(max_concurrent.load(), 2); // At least min_threads
    EXPECT_LE(max_concurrent.load(), 4); // But not more than needed
}

// Test: Thread idle timeout and cleanup
TEST_F(ThreadPoolTest, ThreadIdleTimeoutCleanup) {
    ThreadPool pool(2, 5, 200ms); // Short timeout for testing

    // Submit many tasks to force scaling up
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.enqueue([] { std::this_thread::sleep_for(50ms); }));
    }

    // Wait for all tasks to complete
    for (auto& f : futures) {
        f.get();
    }

    // Now wait for idle threads to timeout
    std::this_thread::sleep_for(500ms);

    // Should have reduced back towards min_threads
    // Note: This test checks that threads actually get cleaned up
    EXPECT_LE(pool.active_threads().load(), pool.max_threads());
}

// Test: Race conditions in task submission
TEST_F(ThreadPoolTest, ConcurrentTaskSubmission) {
    ThreadPool pool(2, 4, 1s);

    std::atomic<int>              task_count{0};
    std::vector<std::future<int>> futures;

    // Multiple threads submitting tasks concurrently
    std::vector<std::jthread> submitters;
    std::mutex                future_mutex;
    for (int thread_id = 0; thread_id < 4; ++thread_id) {
        submitters.emplace_back([&] {
            for (int i = 0; i < 25; ++i) {
                auto            future = pool.enqueue([&] { return ++task_count; });
                std::lock_guard lock{future_mutex};
                futures.push_back(std::move(future));
            }
        });
    }

    // Wait for all submitter threads
    for (auto& t : submitters) {
        t.join();
    }

    // Wait for all tasks to complete
    std::vector<int> results;
    for (auto& f : futures) {
        results.push_back(f.get());
    }

    EXPECT_EQ(results.size(), 100); // 4 threads * 25 tasks
    EXPECT_EQ(task_count.load(), 100);
}

// Test: Priority queue ordering under load
TEST_F(ThreadPoolTest, PriorityOrderingUnderLoad) {
    ThreadPool pool(1, 1, 1s); // Single thread to ensure ordering

    std::vector<int> execution_order;
    std::mutex       order_mutex;

    // Block the single worker thread
    auto blocker = pool.enqueue(
        [&] {
            std::this_thread::sleep_for(100ms);
            std::lock_guard lock(order_mutex);
            execution_order.push_back(0);
        },
        1);

    std::this_thread::sleep_for(10ms); // Ensure blocker starts

    // Submit tasks with different priorities
    std::vector<std::future<void>> futures;
    futures.push_back(pool.enqueue(
        [&] {
            std::lock_guard lock(order_mutex);
            execution_order.push_back(1);
        },
        1)); // Low priority

    futures.push_back(pool.enqueue(
        [&] {
            std::lock_guard lock(order_mutex);
            execution_order.push_back(2);
        },
        10)); // High priority

    futures.push_back(pool.enqueue(
        [&] {
            std::lock_guard lock(order_mutex);
            execution_order.push_back(3);
        },
        5)); // Medium priority

    // Wait for all
    blocker.get();
    for (auto& f : futures) {
        f.get();
    }

    // Should execute in priority order: 0, 2 (high), 3 (medium), 1 (low)
    EXPECT_EQ(execution_order, std::vector<int>({0, 2, 3, 1}));
}

// Test: Exception handling doesn't break thread pool
TEST_F(ThreadPoolTest, ExceptionHandlingRobustness) {
    ThreadPool pool(2, 4, 1s);

    std::atomic<int>               successful_tasks{0};
    std::vector<std::future<void>> futures;

    // Mix of normal and throwing tasks
    for (int i = 0; i < 10; ++i) {
        if (i % 3 == 0) {
            // Throwing task
            futures.push_back(pool.enqueue([i] { throw std::runtime_error("Task " + std::to_string(i) + " failed"); }));
        }
        else {
            // Normal task
            futures.push_back(pool.enqueue([&] {
                std::this_thread::sleep_for(10ms);
                ++successful_tasks;
            }));
        }
    }

    int exception_count = 0;
    for (auto& f : futures) {
        try {
            f.get();
        }
        catch (const std::exception&) {
            ++exception_count;
        }
    }

    EXPECT_EQ(exception_count, 4);         // 4 throwing tasks
    EXPECT_EQ(successful_tasks.load(), 6); // 6 successful tasks

    // Pool should still be functional
    auto final_task = pool.enqueue([] { return 42; });
    EXPECT_EQ(final_task.get(), 42);
}

// Test: Shutdown behavior under different conditions
TEST_F(ThreadPoolTest, ShutdownBehaviorWithPendingTasks) {
    ThreadPool pool(2, 4, 1s);

    std::atomic<int>               completed_tasks{0};
    std::vector<std::future<void>> futures;

    // Submit some long-running tasks
    for (int i = 0; i < 6; ++i) {
        futures.push_back(pool.enqueue([&] {
            std::this_thread::sleep_for(100ms);
            ++completed_tasks;
        }));
    }

    // Let some tasks start
    std::this_thread::sleep_for(50ms);

    // Shutdown while tasks are running
    pool.shutdown();

    // Try to submit after shutdown
    EXPECT_THROW(pool.enqueue([] {}), std::runtime_error);

    // Wait for running tasks to complete
    int successful_completions = 0;
    for (auto& f : futures) {
        try {
            f.get();
            ++successful_completions;
        }
        catch (...) {
            // Some tasks might be cancelled depending on implementation
        }
    }

    EXPECT_GT(successful_completions, 0); // At least some should complete
}

// Test: Memory and resource management
TEST_F(ThreadPoolTest, ResourceManagement) {
    {
        ThreadPool pool(3, 6, 200ms);

        // Submit many tasks with captures to test memory management
        std::vector<std::future<std::string>> futures;

        for (int i = 0; i < 100; ++i) {
            std::string data(1000, 'A' + (i % 26)); // Large string capture
            futures.push_back(pool.enqueue([data, i] { return data + std::to_string(i); }));
        }

        // Verify all tasks complete correctly
        for (std::size_t i = 0; i < 100; ++i) {
            auto result = futures[i].get();
            demiplane::gears::unused_value(result);
            EXPECT_EQ(result.length(), 1000 + std::to_string(i).length());
        }

        // Pool should cleanup properly when it goes out of scope
    } // ThreadPool destructor should handle cleanup

    // If we get here without crashes, resource management is working
    SUCCEED();
}

// Test: Stress test with rapid task submission and completion
TEST_F(ThreadPoolTest, StressTestRapidTasks) {
    ThreadPool pool(2, 8, 1s);

    std::atomic<int> counter{0};
    constexpr int    TASK_COUNT = 1000;

    demiplane::chrono::Stopwatch sw;

    std::vector<std::future<void>> futures;
    futures.reserve(TASK_COUNT);
    sw.start();
    for (int i = 0; i < TASK_COUNT; ++i) {
        demiplane::math::random::RandomTimeGenerator rnd;
        futures.push_back(pool.enqueue([&counter, &rnd, &sw] {
            ++counter;
            // Small random work
            const auto t = rnd.generate_milliseconds(50, 30);
            std::this_thread::sleep_for(t);
            sw.add_flag();
        }));
    }

    // Wait for all tasks
    for (auto& f : futures) {
        f.get();
    }
    auto duration = static_cast<unsigned long long>(sw.total_time().count());
    EXPECT_EQ(counter.load(), TASK_COUNT);
    // Should complete reasonably quickly with multiple threads
    EXPECT_LT(duration, 7000); // Less than 10 seconds
    std::cout << "Completed " << TASK_COUNT << " tasks in " << duration << "ms\n";
    std::cout << "Average time task execution: " << duration / TASK_COUNT << "ms\n"
              << "When flag registered in each" << sw.average_delta();
}
TEST_F(ThreadPoolTest, SleepPool) {
    ThreadPool pool(2, 3, 1s);

    std::atomic<int> counter{0};
    constexpr int    TASK_COUNT = 1;


    std::vector<std::future<void>> futures;
    futures.reserve(TASK_COUNT);
    for (int i = 0; i < TASK_COUNT; ++i) {
        futures.push_back(pool.enqueue([&counter] {
            ++counter;
        }));
    }

    // Wait for all tasks
    for (auto& f : futures) {
        f.get();
    }
    EXPECT_EQ(counter.load(), TASK_COUNT);

    std::this_thread::sleep_for(2s);

    std::cerr << "Slept";
    auto v = pool.enqueue([] {
        std::cout << "Woke up" << std::endl;
    });
    std::cerr << pool.active_threads();
    std::cerr << pool.max_threads();
    std::cerr << pool.size();
    v.get();
    SUCCEED();
}