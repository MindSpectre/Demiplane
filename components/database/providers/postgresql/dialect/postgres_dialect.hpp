#pragma once

#include <dialect_concepts.hpp>
#include <gears_utils.hpp>
#include <sql_params.hpp>

namespace demiplane::db::postgres {

    struct PostgresDialect : DialectBase<PostgresDialect> {
        template <Appendable StringT>
        static constexpr void quote_identifier(StringT& query, std::string_view name) {
            query += '"';
            query += name;
            query += '"';
        }

        template <Appendable StringT>
        static constexpr void placeholder(StringT& query, const std::size_t index) {
            query += '$';
            query += gears::constexpr_to_string(index);
        }

        template <Appendable StringT>
        static constexpr void limit_clause(StringT& query, const std::size_t limit, const std::size_t offset) {
            query += " LIMIT ";
            query += gears::constexpr_to_string(limit);
            if (offset > 0) {
                query += " OFFSET ";
                query += gears::constexpr_to_string(offset);
            }
        }

        template <Appendable StringT>
        static constexpr void format_value(StringT& query, const FieldValue& value) {
            format_value_impl(query, value);
        }

        [[nodiscard]] static constexpr bool supports_returning() noexcept {
            return true;
        }
        [[nodiscard]] static constexpr bool supports_lateral_joins() noexcept {
            return true;
        }
        [[nodiscard]] static constexpr Providers type() noexcept {
            return Providers::PostgreSQL;
        }

        // Runtime only - param binding
        static DialectBindPacket make_param_sink(std::pmr::memory_resource* memory_resource);

    private:
        template <Appendable StringT>
        static constexpr void format_value_impl(StringT& query, const FieldValue& value);

        // ── Per-type formatters ──────────────────────────────────────────────

        template <Appendable StringT>
        static constexpr void format_null(StringT& query);

        template <Appendable StringT>
        static constexpr void format_bool(StringT& query, bool val);

        template <Appendable StringT>
        static constexpr void format_char(StringT& query, char val);

        template <Appendable StringT, std::floating_point FloatT>
        static constexpr void format_floating(StringT& query, FloatT val);

        template <Appendable StringT, std::integral IntT>
        static constexpr void format_integral(StringT& query, IntT val);

        template <Appendable StringT, typename StringValT>
        static constexpr void format_string_value(StringT& query, const StringValT& val);

        template <Appendable StringT, typename Container>
        static constexpr void format_binary(StringT& query, const Container& val);

        // ── Helpers ──────────────────────────────────────────────────────────

        template <Appendable StringT>
        static constexpr void escape_char(StringT& out, char c);

        template <Appendable StringT>
        static constexpr void escape_string(StringT& out, std::string_view str);

        template <typename Container>
        static constexpr std::string format_binary_data(const Container& data);
    };

    static_assert(IsSqlDialect<PostgresDialect>);

}  // namespace demiplane::db::postgres

#include "detail/postgres_dialect.inl"
