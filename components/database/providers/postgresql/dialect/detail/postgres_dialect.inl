#pragma once
#include <charconv>

#include <gears_utils.hpp>

namespace demiplane::db::postgres {

    template <Appendable StringT>
    constexpr void PostgresDialect::escape_char(StringT& out, const char c) {
        if (c == '\'') {
            out += "''";  // SQL standard: escape single quote with double single quote
        } else if (c == '\\') {
            out += "\\\\";  // Escape backslash
        } else {
            out += c;
        }
    }

    template <Appendable StringT>
    constexpr void PostgresDialect::escape_string(StringT& out, const std::string_view str) {
        for (const char c : str) {
            escape_char(out, c);
        }
    }

    template <Appendable StringT>
    constexpr void PostgresDialect::escape_identifier(StringT& out, const std::string_view name) {
        for (const char c : name) {
            if (c == '"') {
                out += "\"\"";
            } else {
                out += c;
            }
        }
    }

    template <typename Container>
    constexpr std::string PostgresDialect::format_binary_data(const Container& data) {
        std::string result;
        result.reserve(5 + data.size() * 2);  // Reserve: "'" + "\x" + 2chars_per_byte + "'"

        result = "'\\x";

        for (const std::uint8_t byte : data) {
            constexpr char hex_chars[]  = "0123456789abcdef";
            result                     += hex_chars[byte >> 4];    // High nibble
            result                     += hex_chars[byte & 0x0F];  // Low nibble
        }

        result += "'";
        return result;
    }

    // ── Per-type formatters ──────────────────────────────────────────────────

    template <Appendable StringT>
    constexpr void PostgresDialect::format_null(StringT& query) {
        query += "NULL";
    }

    template <Appendable StringT>
    constexpr void PostgresDialect::format_bool(StringT& query, const bool val) {
        query += val ? "TRUE" : "FALSE";
    }

    template <Appendable StringT>
    constexpr void PostgresDialect::format_char(StringT& query, const char val) {
        query += "'";
        escape_char(query, val);
        query += "'";
    }

    template <Appendable StringT, std::floating_point FloatT>
    constexpr void PostgresDialect::format_floating(StringT& query, const FloatT val) {
        if consteval {
            query += gears::constexpr_to_string(val);  // TODO:C++26: to_chars for floating is not marked as constexpr
        } else {
            char buf[32];
            auto [end, ec] = std::to_chars(buf, buf + sizeof(buf), val);
            gears::unused_value(ec);  // ignore errors
            std::string_view view{buf, static_cast<std::size_t>(end - buf)};
            query += view;
        }
    }

    template <Appendable StringT, std::integral IntT>
    constexpr void PostgresDialect::format_integral(StringT& query, const IntT val) {
        char buf[24];
        auto [end, ec] = std::to_chars(buf, buf + sizeof(buf), val);
        gears::unused_value(ec);  // ignore errors
        std::string_view view{buf, static_cast<std::size_t>(end - buf)};
        query += view;
    }

    template <Appendable StringT, typename StringValT>
    constexpr void PostgresDialect::format_string_value(StringT& query, const StringValT& val) {
        query += "'";
        escape_string(query, val);
        query += "'";
    }

    template <Appendable StringT, typename Container>
    constexpr void PostgresDialect::format_binary(StringT& query, const Container& val) {
        query += format_binary_data(val);
    }

    // ── Dispatch ─────────────────────────────────────────────────────────────

    template <Appendable StringT>
    constexpr void PostgresDialect::format_value_impl(StringT& query, const FieldValue& value) {
        std::visit(
            [&query]<typename TX>(const TX& val) -> void {
                using T = std::decay_t<TX>;

                if constexpr (std::is_same_v<T, std::monostate>) {
                    format_null(query);
                } else if constexpr (std::is_same_v<T, bool>) {
                    format_bool(query, val);
                } else if constexpr (std::is_same_v<T, char>) {
                    format_char(query, val);
                } else if constexpr (std::is_floating_point_v<T>) {
                    format_floating(query, val);
                } else if constexpr (std::is_integral_v<T>) {
                    format_integral(query, val);
                } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> ||
                                     std::is_same_v<T, std::pmr::string>) {
                    format_string_value(query, val);
                } else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>> ||
                                     std::is_same_v<T, std::span<const std::uint8_t>>) {
                    format_binary(query, val);
                } else {
                    gears::unreachable_c<T>();
                }
            },
            value);
    }

}  // namespace demiplane::db::postgres
