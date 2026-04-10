#pragma once

#include <capability_provider.hpp>
#include <connection_slot.hpp>
#include <gears_class_traits.hpp>

#include "options/transaction_options.hpp"
#include "status/transaction_status.hpp"

namespace demiplane::db::postgres {

    class Session;
    class Savepoint;

    class Transaction : gears::NonCopyable {
    public:
        ~Transaction();

        Transaction(Transaction&& other) noexcept;
        Transaction& operator=(Transaction&& other) noexcept;

        // ============== Cleanup ==============

        /**
         * @brief Set cleanup SQL to run when this transaction releases the slot
         * @param query Predefined cleanup query
         */
        template <typename Self>
        constexpr auto&& do_cleanup(this Self&& self, const CleanupQuery query) noexcept {
            if (self.slot_) {
                self.slot_->cleanup_sql = to_sql(query);
            }
            return std::forward<Self>(self);
        }

        // ============== Lifecycle Control (sync) ==============

        [[nodiscard]] gears::Outcome<void, ErrorContext> begin();
        [[nodiscard]] gears::Outcome<void, ErrorContext> commit();
        [[nodiscard]] gears::Outcome<void, ErrorContext> rollback();

        // ============== Capability Provision ==============

        [[nodiscard]] SyncExecutor with_sync() const;
        [[nodiscard]] AsyncExecutor with_async(boost::asio::any_io_executor exec) const;

        // ============== Savepoints ==============

        [[nodiscard]] gears::Outcome<Savepoint, ErrorContext> savepoint(std::string name) const;

        // ============== Introspection ==============

        [[nodiscard]] constexpr TransactionStatus status() const noexcept {
            return status_;
        }

        [[nodiscard]] constexpr bool is_active() const noexcept {
            return status_ == TransactionStatus::ACTIVE;
        }

        [[nodiscard]] constexpr bool is_finished() const noexcept {
            return status_ == TransactionStatus::COMMITTED || status_ == TransactionStatus::ROLLED_BACK;
        }

        [[nodiscard]] constexpr PGconn* native_handle() const noexcept {
            return slot_ ? slot_->conn : nullptr;
        }

    private:
        friend class Session;
        Transaction(ConnectionSlot& slot, TransactionOptions opts);

        [[nodiscard]] gears::Outcome<void, ErrorContext> execute_control(const std::string& sql) const;

        ConnectionSlot* slot_;
        TransactionOptions options_;
        TransactionStatus status_ = TransactionStatus::IDLE;
    };

    static_assert(CapabilityProvider<Transaction>);
}  // namespace demiplane::db::postgres
