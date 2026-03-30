#pragma once

#include <capability_provider.hpp>

#include "base/transaction.hpp"

namespace demiplane::db::postgres {

    class AutoTransaction : public gears::NonCopyable {
    public:
        using executor_type = boost::asio::any_io_executor;

        [[nodiscard]] gears::Outcome<void, ErrorContext> commit();

        [[nodiscard]] SyncExecutor with_sync() const;
        [[nodiscard]] AsyncExecutor with_async(executor_type exec) const;

        [[nodiscard]] gears::Outcome<Savepoint, ErrorContext> savepoint(std::string name) const;

        [[nodiscard]] TransactionStatus status() const noexcept;
        [[nodiscard]] bool is_active() const noexcept;
        [[nodiscard]] bool is_finished() const noexcept;

    private:
        friend class Session;
        explicit AutoTransaction(Transaction tx);

        Transaction tx_;
    };

    static_assert(CapabilityProvider<AutoTransaction>);
}  // namespace demiplane::db::postgres
