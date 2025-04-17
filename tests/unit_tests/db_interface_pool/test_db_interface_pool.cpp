#include <gtest/gtest.h>
#include <memory>
#include <thread>

#include "basic_mock_db_client.hpp"
#include "db_interface.hpp"
#include "db_interface_pool.hpp"

using namespace demiplane::database;

constexpr uint32_t pool_size = 5;
const static std::function<std::unique_ptr<BasicMockDbClient>()> create_mock =  []{
    return creational::DatabaseFactory::create_basic_mock_database();
};
class DatabasePoolTest : public testing::Test {
protected:
    pool::DatabasePool<BasicMockDbClient> pool;


    void SetUp() override {
        // Fill the pool with mock instances for testing
        pool.fill(pool_size, create_mock);
    }

    void TearDown() override {
        pool.graceful_shutdown();
    }
};

// Test acquiring and releasing an object
TEST_F(DatabasePoolTest, TestAcquireAndRelease) {
    auto db_interface = pool.acquire();
    EXPECT_NE(db_interface, nullptr); // Ensure an object was acquired

    // Release the object back to the pool
    EXPECT_EQ(pool.current_volume(), 4);
    pool.release(std::move(db_interface));
    EXPECT_NO_THROW(pool.acquire()); // Ensure it can be re-acquired
    EXPECT_EQ(pool.current_volume(), 4);
}

// Test pool exhaustion
TEST_F(DatabasePoolTest, TestPoolExhaustion) {
    std::vector<std::shared_ptr<BasicMockDbClient>> interfaces;

    // Acquire all available instances
    interfaces.reserve(5);
    for (int i = 0; i < 5; ++i) {
        interfaces.push_back(pool.acquire());
    }

    // The pool should now be empty
    EXPECT_EQ(pool.acquire(), nullptr);
    EXPECT_EQ(pool.current_volume(), 0);
}

// Test filling the pool with a custom size
TEST_F(DatabasePoolTest, TestCustomFillSize) {
    pool::DatabasePool<BasicMockDbClient> customPool;
    customPool.fill(3, create_mock);

    // Test that we can acquire 3 instances without exhaustion
    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(customPool.acquire());
    }

    // Expect exhaustion after acquiring all 3
    EXPECT_EQ(customPool.acquire(), nullptr);
}

// Test that the factory function fails gracefully
TEST_F(DatabasePoolTest, TestFactoryFunctionFailure) {
    pool::DatabasePool<BasicMockDbClient> failingPool;

    // Custom factory function that returns nullptr to simulate failure
    auto failingFactory = []() -> std::unique_ptr<BasicMockDbClient> { return nullptr; };

    // Expect the fill method to throw when the factory fails
    EXPECT_THROW(failingPool.fill(pool_size, failingFactory), std::runtime_error);
}

// Test multithreaded acquire and release operations
TEST_F(DatabasePoolTest, TestMultiThreadedAcquireRelease) {
    std::vector<std::thread> threads;

    // Launch multiple threads to acquire and release objects
    threads.reserve(10);
    std::mutex stream_mutex;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&] {
            try {
                auto db_interface = pool.acquire();
                if (db_interface == nullptr) {
                    throw std::runtime_error("db_interface is null");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate work
                pool.release(std::move(db_interface));
            } catch (const std::runtime_error& e) {
                std::lock_guard lock(stream_mutex);
                std::cerr << e.what() << "Caught in thread: " << std::this_thread::get_id() << std::endl;
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_NO_THROW(pool.acquire()); // Ensure pool is usable after multithreaded access
}
TEST_F(DatabasePoolTest, TestMultiThreadedSafe) {
    constexpr int thread_count = 50; // Number of threads
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    // Shared synchronization variables
    std::mutex mutex;
    std::condition_variable condition;

    // Atomic counters for assertions
    std::atomic acquired_count = 0;
    std::atomic released_count = 0;

    for (std::size_t i = 0; i < thread_count; ++i) {
        threads.emplace_back([&] {
            std::unique_lock lock(mutex);

            // Wait until there is an available object in the pool
            condition.wait(lock, [&] { return !pool.empty(); });

            try {
                // Acquire object from the pool
                auto db_interface = pool.acquire();
                acquired_count.fetch_add(1, std::memory_order_relaxed);

                lock.unlock(); // Unlock before work to allow parallel execution
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate work
                lock.lock(); // Lock again for releasing and notifying

                // Release object back to the pool
                pool.release(std::move(db_interface));
                released_count.fetch_add(1, std::memory_order_relaxed);

                // Notify one waiting thread that an object is available
                condition.notify_one();
            } catch (const std::exception& e) {
                FAIL() << "Exception during acquire: " << e.what();
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    std::cout << acquired_count << "/" << thread_count << " acquired" << std::endl;
    std::cout << released_count << "/" << thread_count << " released" << std::endl;
    // Final checks
    EXPECT_EQ(acquired_count.load(), thread_count); // All threads acquired objects
    EXPECT_EQ(released_count.load(), thread_count); // All threads released objects
    EXPECT_EQ(pool.current_volume(), pool.capacity()); // Pool returned to full capacity
}

// Test that releasing null pointers doesn't affect the pool
TEST_F(DatabasePoolTest, TestReleaseNull) {
    std::unique_ptr<BasicMockDbClient> nullInterface;
    EXPECT_THROW(pool.release(std::move(nullInterface)), std::invalid_argument);
    // Should throw or affect the pool
}
