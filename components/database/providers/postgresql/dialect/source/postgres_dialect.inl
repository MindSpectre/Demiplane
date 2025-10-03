#pragma once
#include <gears_utils.hpp>
namespace demiplane::db::postgres {

    template <typename String>
    void Dialect::format_value_impl(String& query, const FieldValue& value) {
        std::visit(
            [&query]<typename TX>(const TX& val) -> void {
                using T = std::decay_t<TX>;

                if constexpr (std::is_same_v<T, std::monostate>) {
                    query += "NULL";
                } else if constexpr (std::is_same_v<T, bool>) {
                    query += val ? "TRUE" : "FALSE";
                } else if constexpr (std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t> ||
                                     std::is_same_v<T, double> || std::is_same_v<T, float>) {
                    query += std::to_string(val);
                } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> ||
                                     std::is_same_v<T, std::pmr::string>) {
                    query += "'" + escape_string(val) + "'";
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
    std::string Dialect::format_binary_data(const Container& data) {
        {
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
    }
}  // namespace demiplane::db::postgres
