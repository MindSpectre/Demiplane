#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "db_interface.hpp"
#include "db_interface_factory.hpp"
namespace demiplane::database::pool {
    // Thread safety is on
    class DbInterfacePool final {
    public:
        std::unique_ptr<DbInterface> acquire(const std::chrono::milliseconds timeout)
        {
            std::unique_lock lock(mutex_);
            empty_cv.wait_for(lock, timeout, [this] { return !pool_.empty(); });
            if (pool_.empty()) {
                return nullptr;
            }
            std::unique_ptr<DbInterface> obj = std::move(pool_.back().interface);
            pool_.pop_back();
            full_cv_.notify_one();
            return obj;
        }
        std::unique_ptr<DbInterface> acquire()
        {
            std::unique_lock lock(mutex_);
            if (pool_.empty()) {
                return nullptr;
            }
            std::unique_ptr<DbInterface> obj = std::move(pool_.back().interface);
            pool_.pop_back();
            full_cv_.notify_one();
            return obj;
        }
        std::unique_ptr<DbInterface> safe_acquire() noexcept {
            std::unique_lock lock(mutex_);
            empty_cv.wait(lock, [this] { return !empty(); });
            std::unique_ptr<DbInterface> obj = std::move(pool_.back().interface);
            pool_.pop_back();
            full_cv_.notify_one();
            return obj;
        }
        bool release(std::unique_ptr<DbInterface>&& obj) {
            std::lock_guard lock(mutex_);
            if (obj == nullptr) {
                throw std::invalid_argument("Invalid interface.\t");
            }
            if (full()) {
                return false;
            }
            pool_.emplace_back(std::move(obj));
            empty_cv.notify_one();
            return true;
        }
        void safe_release(std::unique_ptr<DbInterface>&& obj) noexcept {
            std::unique_lock lock(mutex_);
            full_cv_.wait(lock, [this] { return !full(); });
            pool_.emplace_back(std::move(obj));
            empty_cv.notify_one();
        }
        // Fills the pool with a specified number of instances, created by the provided factory function
        template <typename FactoryFunc, typename... Args>
            requires creational::DbInterfaceFactoryFunction<FactoryFunc, Args...>
        void fill(const std::size_t size, FactoryFunc&& factory, Args&&... args) {
            std::lock_guard lock(mutex_);
            capacity_ = size;
            for (std::size_t i = 0; i < capacity_; ++i) {
                if (std::unique_ptr<DbInterface> db_interface = factory(std::forward<Args>(args)...)) {
                    pool_.emplace_back(std::move(db_interface));
                } else {
                    throw std::runtime_error("Factory function failed to create a valid DbInterface instance.\t");
                }
            }
        }


        void graceful_shutdown() {
            std::lock_guard lock(mutex_);
            while (!pool_.empty()) {
                pool_.back().interface->drop_connect();
                pool_.pop_back();
            }
        }
        void safe_kill() noexcept {
            std::lock_guard lock(mutex_);
            while (!pool_.empty()) {
                try {
                    pool_.back().interface->drop_connect(); // Attempt cleanup
                } catch (const std::exception& e) {
                    std::cerr << "Failed to drop connection: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "Unknown error during dropping connection." << std::endl;
                }
                pool_.pop_back(); // Remove connection regardless
            }
        }
        std::unique_ptr<DbInterface> lend() {
            std::lock_guard lock(mutex_);
            if (pool_.empty()) {
                throw std::runtime_error("Pool is exhausted.\t");
            }
            if (!pool_.front().is_idle()) {
                throw std::runtime_error("Connection is waiting. But not idle\t");
            }
            std::unique_ptr<DbInterface> obj = std::move(pool_.front().interface);
            pool_.pop_front();
            return obj;
        }
        [[nodiscard]] bool has_idle() const {
            std::lock_guard lock(mutex_);
            return !pool_.front().is_idle();
        }
        [[nodiscard]] std::size_t capacity() const {
            std::lock_guard lock(mutex_);
            return capacity_;
        }
        [[nodiscard]] std::size_t current_volume() const {
            std::lock_guard lock(mutex_);
            return pool_.size();
        }
        [[nodiscard]] bool full() const {
            std::lock_guard lock(mutex_);
            return pool_.size() >= capacity_;
        }
        [[nodiscard]] bool empty() const {
            std::lock_guard lock(mutex_);
            return pool_.empty();
        }
        DbInterfacePool() = default;

        template <typename FactoryFunc, typename... Args>
            requires creational::DbInterfaceFactoryFunction<FactoryFunc, Args...>
        DbInterfacePool(const std::size_t size, FactoryFunc&& factory, Args&&... args) {
            this->fill(size, std::forward<FactoryFunc>(factory), std::forward<Args>(args)...);
        }
        // Delete copy operations
        DbInterfacePool(const DbInterfacePool&)            = delete; // Copy constructor
        DbInterfacePool& operator=(const DbInterfacePool&) = delete; // Copy assignment
        DbInterfacePool(DbInterfacePool&&)                 = delete; // Move constructor
        DbInterfacePool& operator=(DbInterfacePool&&)      = delete; // Move assignment


        ~DbInterfacePool() noexcept {
            safe_kill();
        }
        [[nodiscard]] std::mutex& mutex() const {
            return external_mutex_;
        }

    private:
        mutable std::mutex external_mutex_;
        class DbConnection {
        public:
            [[nodiscard]] bool is_idle() const {
                return std::chrono::steady_clock::now() - last_active_time_ > idle_period_;
            }
            void act() {
                last_active_time_ = std::chrono::steady_clock::now();
            }
            std::unique_ptr<DbInterface> interface;
            DbConnection(const DbConnection&) = delete;
            explicit DbConnection(std::unique_ptr<DbInterface>&& interface)
                : interface(std::move(interface)) {
                act();
            }
            DbConnection(DbConnection&&)                 = default;
            DbConnection& operator=(DbConnection&&)      = default;
            DbConnection& operator=(const DbConnection&) = delete;
            DbConnection& operator=(DbConnection&)       = delete;
            DbConnection()                               = default;

        private:
            constexpr static auto idle_period_ = std::chrono::seconds(60);
            std::chrono::steady_clock::time_point last_active_time_;
        };

        mutable std::recursive_mutex mutex_;
        mutable std::condition_variable_any full_cv_;
        mutable std::condition_variable_any empty_cv;
        std::size_t capacity_{0};
        std::deque<DbConnection> pool_;
    };

} // namespace demiplane::database::pool
