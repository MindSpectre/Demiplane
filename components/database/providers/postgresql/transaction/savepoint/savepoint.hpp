#pragma once

#include <string>
#include <string_view>

#include <gears_class_traits.hpp>
#include <gears_outcome.hpp>
#include <postgres_errors.hpp>

struct pg_conn;
using PGconn = pg_conn;

namespace demiplane::db::postgres {

    class Savepoint : gears::NonCopyable {
    public:
        ~Savepoint();

        Savepoint(Savepoint&& other) noexcept;
        Savepoint& operator=(Savepoint&& other) noexcept;

        // TODO: Unlike PostgreSQL's native behavior where ROLLBACK TO SAVEPOINT keeps the savepoint
        //       active, this implementation treats rollback as a terminal operation. Create a new
        //       savepoint if retry semantics are needed.
        [[nodiscard]] gears::Outcome<void, ErrorContext> rollback();
        [[nodiscard]] gears::Outcome<void, ErrorContext> release();

        [[nodiscard]] std::string_view name() const noexcept;
        [[nodiscard]] bool is_active() const noexcept;

    private:
        friend class Transaction;
        Savepoint(PGconn* conn, std::string name);

        [[nodiscard]] gears::Outcome<void, ErrorContext> execute_control(std::string_view sql) const;

        PGconn* conn_;
        std::string name_;
        bool active_ = true;
    };

}  // namespace demiplane::db::postgres
