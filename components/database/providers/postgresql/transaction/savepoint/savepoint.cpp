#include "savepoint.hpp"

#include <format>

#include <postgres_sync_executor.hpp>


namespace demiplane::db::postgres {

    Savepoint::~Savepoint() {
        if (active_) {
            GEARS_UNUSED_VAR = release();
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
                GEARS_UNUSED_VAR = release();
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
        auto result = execute_control(std::format(R"(ROLLBACK TO SAVEPOINT "{}")", name_));
        return result;
    }

    gears::Outcome<void, ErrorContext> Savepoint::release() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (!active_) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control(std::format(R"(RELEASE SAVEPOINT "{}")", name_));
        if (result.is_success()) {
            active_ = false;
        }
        return result;
    }

    gears::Outcome<void, ErrorContext> Savepoint::execute_control(const std::string& sql) const {
        const SyncExecutor exec{conn_};
        if (auto result = exec.execute(sql); !result.is_success()) {
            return gears::Err(result.error<ErrorContext>());
        }
        return gears::Ok();
    }

}  // namespace demiplane::db::postgres
