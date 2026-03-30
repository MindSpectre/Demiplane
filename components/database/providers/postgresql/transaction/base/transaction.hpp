#pragma once

#include <capability_provider.hpp>
#include <gears_class_traits.hpp>

#include "options/transaction_options.hpp"
#include "status/transaction_status.hpp"

namespace demiplane::db::postgres {

    struct ConnectionSlot;
    class Session;
    class Savepoint;

    class Transaction : gears::NonCopyable {
    public:
        using executor_type = boost::asio::any_io_executor;

        ~Transaction();

        Transaction(Transaction&& other) noexcept;
        Transaction& operator=(Transaction&& other) noexcept;

        // ============== Lifecycle Control (sync) ==============

        [[nodiscard]] gears::Outcome<void, ErrorContext> begin();
        [[nodiscard]] gears::Outcome<void, ErrorContext> commit();
        [[nodiscard]] gears::Outcome<void, ErrorContext> rollback();

        // ============== Capability Provision ==============

        [[nodiscard]] SyncExecutor with_sync() const;
        [[nodiscard]] AsyncExecutor with_async(executor_type exec) const;

        // ============== Savepoints ==============

        [[nodiscard]] gears::Outcome<Savepoint, ErrorContext> savepoint(std::string name) const;

        // ============== Introspection ==============

        [[nodiscard]] TransactionStatus status() const noexcept;
        [[nodiscard]] bool is_active() const noexcept;
        [[nodiscard]] bool is_finished() const noexcept;
        [[nodiscard]] PGconn* native_handle() const noexcept;

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
