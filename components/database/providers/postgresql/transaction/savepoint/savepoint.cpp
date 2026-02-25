#include "savepoint.hpp"

#include <postgres_sync_executor.hpp>

#include "log_macros.hpp"

namespace demiplane::db::postgres {

    Savepoint::Savepoint(PGconn* conn, std::string name)
        : conn_{conn},
          name_{std::move(name)} {
        COMPONENT_LOG_INF() << "Savepoint '" << name_ << "' created";
    }

    Savepoint::~Savepoint() {
        if (active_) {
            [[maybe_unused]] auto _ = release();
        }
    }

    Savepoint::Savepoint(Savepoint&& other) noexcept
        : conn_{std::exchange(other.conn_, nullptr)},
          name_{std::move(other.name_)},
          active_{std::exchange(other.active_, false)} {
    }

    Savepoint& Savepoint::operator=(Savepoint&& other) noexcept {
        if (this != &other) {
            if (active_) {
                [[maybe_unused]] auto _ = release();
            }
            conn_   = std::exchange(other.conn_, nullptr);
            name_   = std::move(other.name_);
            active_ = std::exchange(other.active_, false);
        }
        return *this;
    }

    gears::Outcome<void, ErrorContext> Savepoint::rollback() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (!active_) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        // TODO: Savepoint name is concatenated into SQL without quoting — see Transaction::savepoint() TODO.
        auto result = execute_control("ROLLBACK TO SAVEPOINT " + name_);
        if (result.is_success()) {
            active_ = false;
        }
        return result;
    }

    gears::Outcome<void, ErrorContext> Savepoint::release() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (!active_) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        // TODO: Savepoint name is concatenated into SQL without quoting — see Transaction::savepoint() TODO.
        auto result = execute_control("RELEASE SAVEPOINT " + name_);
        if (result.is_success()) {
            active_ = false;
        }
        return result;
    }

    std::string_view Savepoint::name() const noexcept {
        return name_;
    }
    bool Savepoint::is_active() const noexcept {
        return active_;
    }

    gears::Outcome<void, ErrorContext> Savepoint::execute_control(const std::string_view sql) const {
        const SyncExecutor exec{conn_};
        if (auto result = exec.execute(sql); !result.is_success()) {
            return gears::Err(result.error<ErrorContext>());
        }
        return gears::Ok();
    }

}  // namespace demiplane::db::postgres
