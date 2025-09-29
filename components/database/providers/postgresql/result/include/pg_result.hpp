#pragma once
#include <memory>
#include <optional>

#include <libpq-fe.h>
#include <gears_class_traits.hpp>
#include "pg_result_views.hpp"

namespace demiplane::db::postgres {

    class Result : gears::NonCopyable {
    public:
        explicit Result(PGresult* r)
            : res_(r) {
        }

        [[nodiscard]] bool empty() const noexcept {
            return rows() == 0;
        }
        [[nodiscard]] std::size_t rows() const noexcept {
            return static_cast<std::size_t>(PQntuples(res_.get()));
        }
        [[nodiscard]] std::size_t cols() const noexcept {
            return static_cast<std::size_t>(PQnfields(res_.get()));
        }

        [[nodiscard]] RowView row(const std::size_t i) const {
            return RowView{res_.get(), i};
        }

        // convenience
        template <class T>
        std::optional<T> get_opt(const std::size_t r, const std::size_t c) const {
            const auto f = row(r)[c];
            if (f.is_null())
                return std::nullopt;
            return f.as<T>();
        }

        [[nodiscard]] PGresult* raw() const noexcept {
            return res_.get();
        }
    private:
        struct PgResultDeleter {
            void operator()(PGresult* r) const noexcept {
                if (r)
                    PQclear(r);
            }
        };
        std::unique_ptr<PGresult, PgResultDeleter> res_;
    };
}  // namespace demiplane::db
