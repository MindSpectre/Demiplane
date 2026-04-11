#include "auto_transaction.hpp"

#include <savepoint/savepoint.hpp>

namespace demiplane::db::postgres {

    AutoTransaction::AutoTransaction(Transaction tx)
        : tx_{std::move(tx)} {
    }

    gears::Outcome<void, ErrorContext> AutoTransaction::commit() {
        return tx_.commit();
    }

    gears::Outcome<SyncExecutor, ErrorContext> AutoTransaction::with_sync() const {
        return tx_.with_sync();
    }

    gears::Outcome<AsyncExecutor, ErrorContext> AutoTransaction::with_async(boost::asio::any_io_executor exec) const {
        return tx_.with_async(std::move(exec));
    }

    gears::Outcome<Savepoint, ErrorContext> AutoTransaction::savepoint(std::string name) const {
        return tx_.savepoint(std::move(name));
    }

    TransactionStatus AutoTransaction::status() const noexcept {
        return tx_.status();
    }

    bool AutoTransaction::is_active() const noexcept {
        return tx_.is_active();
    }

    bool AutoTransaction::is_finished() const noexcept {
        return tx_.is_finished();
    }

}  // namespace demiplane::db::postgres
