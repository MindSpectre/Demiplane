#pragma once

#include <demiplane/scroll>
#include <memory>

#include <capability_provider.hpp>
#include <connection_holder.hpp>
#include <gears_class_traits.hpp>

#include "options/transaction_options.hpp"
#include "status/transaction_status.hpp"

namespace demiplane::db::postgres {

    class LockFreeSession;
    class BlockingSession;
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
            if (auto live = self.holder_.lock()) {
                live->set_cleanup(query);
            }
            return std::forward<Self>(self);
        }

        // ============== Lifecycle Control (sync) ==============

        [[nodiscard]] gears::Outcome<void, ErrorContext> begin();
        [[nodiscard]] gears::Outcome<void, ErrorContext> commit();
        [[nodiscard]] gears::Outcome<void, ErrorContext> rollback();

        // ============== Capability Provision ==============

        [[nodiscard]] gears::Outcome<SyncExecutor, ErrorContext> with_sync() const;
        [[nodiscard]] gears::Outcome<AsyncExecutor, ErrorContext> with_async(boost::asio::any_io_executor exec) const;

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
            return conn_;
        }

    private:
        friend class LockFreeSession;
        friend class BlockingSession;

        SCROLL_COMPONENT_PREFIX("Transaction");

        std::weak_ptr<ConnectionHolder> holder_;
        PGconn* conn_ = nullptr;
        TransactionOptions options_;
        TransactionStatus status_ = TransactionStatus::IDLE;

        Transaction(std::weak_ptr<ConnectionHolder> holder, TransactionOptions opts);

        [[nodiscard]] gears::Outcome<void, ErrorContext> execute_control(const std::string& sql) const;
    };

    static_assert(CapabilityProvider<Transaction>);
}  // namespace demiplane::db::postgres
