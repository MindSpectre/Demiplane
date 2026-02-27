#pragma once
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
    constexpr void PostgresDialect::format_value_impl(StringT& query, const FieldValue& value) {
        std::visit(
            [&query]<typename TX>(const TX& val) -> void {
                using T = std::decay_t<TX>;

                if constexpr (std::is_same_v<T, std::monostate>) {
                    query += "NULL";
                } else if constexpr (std::is_same_v<T, bool>) {
                    query += val ? "TRUE" : "FALSE";
                } else if constexpr (std::is_same_v<T, char>) {
                    query += "'";
                    escape_char(query, val);
                    query += "'";
                } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
                    // float/double: not needed at compile time, use std::to_string //TODO? need to think about it
                    query += std::to_string(val);
                } else if constexpr (std::is_integral_v<T>) {
                    if constexpr (std::is_signed_v<T>) {
                        if (val < 0) {
                            query += '-';
                            query +=
                                gears::constexpr_to_string(static_cast<std::size_t>(-static_cast<std::int64_t>(val)));
                        } else {
                            query += gears::constexpr_to_string(static_cast<std::size_t>(val));
                        }
                    } else {
                        query += gears::constexpr_to_string(static_cast<std::size_t>(val));
                    }
                } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> ||
                                     std::is_same_v<T, std::pmr::string>) {
                    query += "'";
                    escape_string(query, val);
                    query += "'";
                } else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>> ||
                                     std::is_same_v<T, std::span<const std::uint8_t>>) {
                    query += format_binary_data(val);
                } else {
                    gears::unreachable_c<T>();
                }
            },
            value);
    }

    template <typename Container>
    std::string PostgresDialect::format_binary_data(const Container& data) {
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
}  // namespace demiplane::db::postgres
