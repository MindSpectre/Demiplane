#include "postgres_dialect.hpp"

#include <demiplane/gears>


namespace demiplane::db {
    std::string PostgresDialect::quote_identifier(const std::string_view name) const {
        std::string quoted_name;
        quoted_name.reserve(name.size() + 2);
        quoted_name += "\"";
        quoted_name += name.data();
        quoted_name += "\"";
        return quoted_name;
    }

    void PostgresDialect::quote_identifier(std::string& query, const std::string_view name) const {
        query += "\"";
        query += name.data();
        query += "\"";
    }
    void PostgresDialect::quote_identifier(std::pmr::string& query, const std::string_view name) const {
        query += "\"";
        query += name.data();
        query += "\"";
    }

    std::string PostgresDialect::placeholder(const std::size_t index) const {
        std::string place_holder;
        place_holder.reserve(15);
        placeholder(place_holder, index);
        return place_holder;
    }

    void PostgresDialect::placeholder(std::string& query, const std::size_t index) const {
        query += "$" + std::to_string(index + 1);
    }
    void PostgresDialect::placeholder(std::pmr::string& query, const std::size_t index) const {
        query += "$" + std::to_string(index + 1);
    }

    std::string PostgresDialect::limit_clause(const std::size_t limit, const std::size_t offset) const {
        std::string clause;
        clause.reserve(32);
        limit_clause(clause, limit, offset);
        return clause;
    }

    void PostgresDialect::limit_clause(std::string& query, const std::size_t limit, const std::size_t offset) const {
        query += " LIMIT " + std::to_string(limit);
        if (offset > 0) {
            query += " OFFSET " + std::to_string(offset);
        }
    }
    void PostgresDialect::limit_clause(std::pmr::string& query, const std::size_t limit, const std::size_t offset) const {
        query += " LIMIT " + std::to_string(limit);
        if (offset > 0) {
            query += " OFFSET " + std::to_string(offset);
        }
    }

    void PostgresDialect::format_value(std::string& query, const FieldValue& value) {
        std::visit(
            [&query]<typename TX>(const TX& val) -> void {
                using T = std::decay_t<TX>;

                if constexpr (std::is_same_v<T, std::monostate>) {
                    query += "NULL";
                } else if constexpr (std::is_same_v<T, bool>) {
                    query += val ? "TRUE" : "FALSE";
                } else if constexpr (std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t> ||
                                     std::is_same_v<T, double>) {
                    query += std::to_string(val);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    query += "'" + escape_string(val) + "'";
                } else if constexpr (std::is_same_v<T, std::span<const uint8_t>>) {
                    query += format_binary_data(val);
                } else {
                    gears::unreachable_c<T>();
                }
            },
            value);
    }
    void PostgresDialect::format_value(std::pmr::string& query, const FieldValue& value) {
        std::visit(
            [&query]<typename TX>(const TX& val) -> void {
                using T = std::decay_t<TX>;

                if constexpr (std::is_same_v<T, std::monostate>) {
                    query += "NULL";
                } else if constexpr (std::is_same_v<T, bool>) {
                    query += val ? "TRUE" : "FALSE";
                } else if constexpr (std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t> ||
                                     std::is_same_v<T, double>) {
                    query += std::to_string(val);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    query += "'" + escape_string(val) + "'";
                } else if constexpr (std::is_same_v<T, std::span<const uint8_t>>) {
                    query += format_binary_data(val);
                } else {
                    gears::unreachable_c<T>();
                }
            },
            value);
    }
    DialectBindPacket PostgresDialect::make_param_sink(std::pmr::memory_resource*) const {
        return {};  //TODO
    }

    std::string PostgresDialect::escape_string(const std::string_view str) {
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
