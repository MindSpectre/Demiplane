#pragma once

#include <demiplane/scroll>
#include <string>
#include <string_view>

#include <gears_class_traits.hpp>
#include <gears_outcome.hpp>
#include <postgres_errors.hpp>
namespace demiplane::db::postgres {

    class Savepoint : gears::NonCopyable {
    public:
        ~Savepoint();

        Savepoint(Savepoint&& other) noexcept;
        Savepoint& operator=(Savepoint&& other) noexcept;

        [[nodiscard]] gears::Outcome<void, ErrorContext> rollback();
        [[nodiscard]] gears::Outcome<void, ErrorContext> release();

        [[nodiscard]] constexpr std::string_view name() const noexcept {
            return name_;
        }

        [[nodiscard]] constexpr bool is_active() const noexcept {
            return active_;
        }

    private:
        friend class Transaction;
        Savepoint(PGconn* conn, std::string name)
            : conn_{conn},
              name_{std::move(name)} {
            COMPONENT_LOG_INF() << "Savepoint '" << name_ << "' created";
        }

        [[nodiscard]] gears::Outcome<void, ErrorContext> execute_control(const std::string& sql) const;

        PGconn* conn_;
        std::string name_;
        bool active_ = true;
    };

}  // namespace demiplane::db::postgres
