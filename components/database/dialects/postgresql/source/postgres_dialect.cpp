#include "postgres_dialect.hpp"

#include <demiplane/gears>


namespace demiplane::db {
    std::string PostgresDialect::quote_identifier(const std::string_view name) const {
        std::ostringstream os;
        os << "\"" << name.data() << "\"";
        return os.str();
    }

    std::string PostgresDialect::placeholder(const std::size_t index) const {
        return "$" + std::to_string(index + 1);
    }

    std::string PostgresDialect::limit_clause(const std::size_t limit, const std::size_t offset) const {
        std::string clause = " LIMIT " + std::to_string(limit);
        if (offset > 0) {
            clause += " OFFSET " + std::to_string(offset);
        }
        return clause;
    }

    std::string PostgresDialect::format_value(const FieldValue& value) {
        return std::visit(
            []<typename TX>(const TX& val) -> std::string {
                using T = std::decay_t<TX>;

                if constexpr (std::is_same_v<T, std::monostate>) {
                    return "NULL";
                } else if constexpr (std::is_same_v<T, bool>) {
                    return val ? "TRUE" : "FALSE";
                } else if constexpr (std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t> ||
                                     std::is_same_v<T, double>) {
                    return std::to_string(val);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return "'" + escape_string(val) + "'";
                } else if constexpr (std::is_same_v<T, std::span<const uint8_t>>) {
                    return format_binary_data(val);
                } else {
                    gears::unreachable_c<T>();
                    return {};
                }
            },
            value);
    }

    std::string PostgresDialect::escape_string(const std::string& str) {
        std::string result;
        result.reserve(str.size() * 2);  // Reserve space for potential escaping

        for (const char c : str) {
            if (c == '\'') {
                result += "''";  // SQL standard: escape single quote with double single quote
            } else if (c == '\\') {
                result += "\\\\";  // Escape backslash
            } else {
                result += c;
            }
        }
        return result;
    }

    std::string PostgresDialect::format_binary_data(const std::span<const uint8_t> data) {
        std::ostringstream oss;
        oss << "\\x";  // PostgreSQL bytea hex format

        for (const uint8_t byte : data) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
        }

        return "'" + oss.str() + "'";
    }
}  // namespace demiplane::db
