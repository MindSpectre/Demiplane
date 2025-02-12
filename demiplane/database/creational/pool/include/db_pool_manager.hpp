#pragma once

#include <atomic>

#include "db_interface.hpp"
#include "db_interface_pool.hpp"
namespace demiplane::database::pool {

    class DbPoolManager final {
    public:
        /**
         *
         * @return Connection if any of pools have it, otherwise return nullptr without waiting connection.
         */
        [[nodiscard]] std::unique_ptr<interfaces::DbInterface> acquire() const {
            if (std::unique_ptr<interfaces::DbInterface> connect = this->dedicated_pool_->acquire()) {
                return connect;
            }
            return this->shared_pool_->acquire(awaiting_duration_);
        }

        /**
         *
         * @return Connection will be returned always. It can block a thread, because it will wait until connection
         * appeared
         */
        [[nodiscard]] std::unique_ptr<interfaces::DbInterface> safe_acquire() const {

            if (std::unique_ptr<interfaces::DbInterface> connect = this->dedicated_pool_->acquire(awaiting_duration_)) {
                return connect;
            }
            if (std::unique_ptr<interfaces::DbInterface> connect = this->shared_pool_->acquire(awaiting_duration_)) {
                return connect;
            }
            // here condition variable should handle signal from this->shared_pool_.empty_cv and dedicated_empty_cv
            return this->dedicated_pool_->safe_acquire();
        }

        // Release connection back to the pool
        /**
         * @return Always move connection to pool even if both of them are full
         * @param obj Connection, ownership moved to one of pools
         */
        void safe_release(std::unique_ptr<interfaces::DbInterface>&& obj) const {
            if (this->dedicated_pool_->release(std::move(obj))) {
                return;
            }
            if (this->shared_pool_->release(std::move(obj))) {
                return;
            }

            this->dedicated_pool_->safe_release(std::move(obj));
        }

        /**
         *
         * @param obj Connection, ownership moved to one of pools
         * @return Connection if both of pool are full. Guarantees that state of pool is always not overflow.
         */
        std::unique_ptr<interfaces::DbInterface> release(std::unique_ptr<interfaces::DbInterface>&& obj) const {
            // Attempt to release into the dedicated pool
            if (this->dedicated_pool_->release(std::move(obj))) {
                return nullptr;
            }
            if (this->shared_pool_->release(std::move(obj))) {
                return nullptr;
            }

            // Both pools are full; return the connection to the caller
            return std::move(obj);
        }
        // // Real-time check for idle connections and lend to shared pool
        // void lend_idle_to_shared() const {
        //     std::scoped_lock lock(this->dedicated_pool_->mutex(), this->shared_pool_->mutex());
        //     while (this->dedicated_pool_->has_idle() && check_and_notify_shared_overflow()) {
        //         auto conn = this->dedicated_pool_->lend();
        //         this->shared_pool_->release(std::move(conn));
        //     }
        // }
        //
        // // Recall connections from shared pool if under pressure
        // void recall_from_shared() const {
        //     std::scoped_lock lock(this->dedicated_pool_->mutex(), this->shared_pool_->mutex());
        //     while (!this->dedicated_pool_->full() && check_and_notify_shared_exhaustion()) {
        //         auto conn = this->shared_pool_->acquire(awaiting_duration_);
        //         this->dedicated_pool_->release(std::move(conn));
        //     }
        // }
        [[nodiscard]] bool check_shared_overflow() const {
            return this->shared_pool_->current_volume() > 2 * this->shared_pool_->capacity();
        }
        [[nodiscard]] bool check_shared_exhaustion() const {
            return this->shared_pool_->empty();
        }
        [[nodiscard]] bool check_and_notify_shared_overflow() const {
            // TODO: require setup eg when current volume > k*capacity
            if (check_shared_overflow()) {
                // alert(AlertType::SHARED_OVERFLOW);
                return true;
            }
            return false;
        }
        [[nodiscard]] bool check_and_notify_shared_exhaustion() const {
            if (this->shared_pool_->empty()) {
                // alert(AlertType::SHARED_EXHAUSTION);
                return true;
            }
            return false;
        }
        // Check if under high load
        bool is_under_pressure() const {
            // TODO: logic how we can define high load
            return high_load_flag_;
        }
        DbPoolManager(const DbPoolManager&)            = delete; // Copy constructor
        DbPoolManager& operator=(const DbPoolManager&) = delete; // Copy assignment
        DbPoolManager(DbPoolManager&&)                 = delete; // Move constructor
        DbPoolManager& operator=(DbPoolManager&&)      = delete; // Move assignment


        /**
         *
         * @param dedicated_pool Dedicated pool which owned by one DAO
         * @param shared_pool Shared pool with free connections each dao own it
         * @param awaiting_duration time during thread will wait free connection
         */
        DbPoolManager(std::unique_ptr<DbInterfacePool> dedicated_pool, std::shared_ptr<DbInterfacePool> shared_pool,
            const std::chrono::milliseconds& awaiting_duration)
            : awaiting_duration_{awaiting_duration}, shared_pool_{std::move(shared_pool)},
              dedicated_pool_{std::move(dedicated_pool)} {}

        DbPoolManager(std::unique_ptr<DbInterfacePool> dedicated_pool, std::shared_ptr<DbInterfacePool> shared_pool)
            : shared_pool_{std::move(shared_pool)}, dedicated_pool_{std::move(dedicated_pool)} {}


        void graceful_shutdown() const {
            this->dedicated_pool_->graceful_shutdown();
            this->shared_pool_->graceful_shutdown();
        }
        // pls don't blame me that an Easter egg
        void safe_kill() const noexcept {
            this->dedicated_pool_->safe_kill();
            this->shared_pool_->safe_kill();
        }
        DbPoolManager() noexcept = default;
        ~DbPoolManager() noexcept {
            safe_kill();
        }
        // TEMPORARY
        [[nodiscard]] std::shared_ptr<DbInterfacePool> get_shared_pool() const {
            return shared_pool_;
        }
        void set_shared_pool(std::shared_ptr<DbInterfacePool> shared_pool) {
            shared_pool_ = std::move(shared_pool);
        }
        [[nodiscard]] const std::unique_ptr<DbInterfacePool>& get_dedicated_pool() const {
            return dedicated_pool_;
        }
        void set_dedicated_pool(std::unique_ptr<DbInterfacePool> dedicated_pool) {
            dedicated_pool_ = std::move(dedicated_pool);
        }
        [[nodiscard]] std::chrono::milliseconds get_awaiting_duration() const {
            return awaiting_duration_;
        }
        void set_awaiting_duration(const std::chrono::milliseconds& awaiting_duration) {
            awaiting_duration_ = awaiting_duration;
        }
        void set_awaiting_duration(const uint32_t& awaiting_duration) {
            awaiting_duration_ = std::chrono::milliseconds(awaiting_duration);
        }

    private:
        std::chrono::milliseconds awaiting_duration_{1200};
        std::shared_ptr<DbInterfacePool> shared_pool_{};
        std::unique_ptr<DbInterfacePool> dedicated_pool_{};

        mutable std::atomic<bool> high_load_flag_; // High load indicator
    };
} // namespace demiplane::database::pool
