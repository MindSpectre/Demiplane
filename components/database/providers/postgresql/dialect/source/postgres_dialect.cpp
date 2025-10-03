#include "postgres_dialect.hpp"

#include <demiplane/gears>

#include <postgres_params.hpp>
namespace demiplane::db::postgres {
    std::string Dialect::quote_identifier(const std::string_view name) const {
        std::string quoted_name;
        quoted_name.reserve(name.size() + 2);
        quoted_name += "\"";
        quoted_name += name.data();
        quoted_name += "\"";
        return quoted_name;
    }

    void Dialect::quote_identifier(std::string& query, const std::string_view name) const {
        query += "\"";
        query += name.data();
        query += "\"";
    }

    void Dialect::quote_identifier(std::pmr::string& query, const std::string_view name) const {
        query += "\"";
        query += name.data();
        query += "\"";
    }

    std::string Dialect::placeholder(const std::size_t index) const {
        std::string place_holder;
        place_holder.reserve(15);
        placeholder(place_holder, index);
        return place_holder;
    }

    void Dialect::placeholder(std::string& query, const std::size_t index) const {
        query += "$" + std::to_string(index + 1);
    }

    void Dialect::placeholder(std::pmr::string& query, const std::size_t index) const {
        query += "$" + std::to_string(index + 1);
    }

    std::string Dialect::limit_clause(const std::size_t limit, const std::size_t offset) const {
        std::string clause;
        clause.reserve(32);
        limit_clause(clause, limit, offset);
        return clause;
    }

    void Dialect::limit_clause(std::string& query, const std::size_t limit, const std::size_t offset) const {
        query += " LIMIT " + std::to_string(limit);
        if (offset > 0) {
            query += " OFFSET " + std::to_string(offset);
        }
    }

    void Dialect::limit_clause(std::pmr::string& query, const std::size_t limit, const std::size_t offset) const {
        query += " LIMIT " + std::to_string(limit);
        if (offset > 0) {
            query += " OFFSET " + std::to_string(offset);
        }
    }

    void Dialect::format_value(std::string& query, const FieldValue& value) {
        format_value_impl(query, value);
    }

    void Dialect::format_value(std::pmr::string& query, const FieldValue& value) {
        format_value_impl(query, value);
    }

    DialectBindPacket Dialect::make_param_sink(std::pmr::memory_resource* memory_resource) const {
        auto sink                          = std::make_unique<ParamSink>(memory_resource);
        const std::shared_ptr<void> packet = sink->packet();

        return DialectBindPacket{.sink = std::move(sink), .packet = packet};
    }

    std::string Dialect::escape_string(const std::string_view str) {
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
}  // namespace demiplane::db::postgres
