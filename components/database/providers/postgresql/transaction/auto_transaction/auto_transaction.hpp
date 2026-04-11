#pragma once

#include <capability_provider.hpp>

#include "base/transaction.hpp"

namespace demiplane::db::postgres {

    class AutoTransaction : public gears::NonCopyable {
    public:
        /**
         * @brief Set cleanup SQL to run when this transaction releases the slot
         * @param query Predefined cleanup query
         */
        template <typename Self>
        auto&& do_cleanup(this Self&& self, CleanupQuery query) noexcept {
            self.tx_.do_cleanup(query);
            return std::forward<Self>(self);
        }

        [[nodiscard]] gears::Outcome<void, ErrorContext> commit();

        [[nodiscard]] gears::Outcome<SyncExecutor, ErrorContext> with_sync() const;
        [[nodiscard]] gears::Outcome<AsyncExecutor, ErrorContext> with_async(boost::asio::any_io_executor exec) const;

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
