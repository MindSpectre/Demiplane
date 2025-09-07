#pragma once
#include <string>

#include <db_field_value.hpp>

namespace demiplane::db {
    class SqlDialect {
    public:
        virtual ~SqlDialect() = default;

        // Identifier quoting
        [[nodiscard]] virtual std::string quote_identifier(std::string_view name) const = 0;

        // Parameter placeholders
        [[nodiscard]] virtual std::string placeholder(std::size_t index) const = 0;

        // Limit/Offset syntax
        [[nodiscard]] virtual std::string limit_clause(std::size_t limit, std::size_t offset) const = 0;

        // Type mapping
        [[nodiscard]] virtual std::string map_type(const std::string_view db_type) const {
            return std::string(db_type);
            // TODO do proper mapping Issue#40
        }

        // Feature support
        [[nodiscard]] virtual bool supports_returning() const {
            return false;
        }

        [[nodiscard]] virtual bool supports_cte() const {
            return true;
        }

        [[nodiscard]] virtual bool supports_window_functions() const {
            return true;
        }

        [[nodiscard]] virtual bool supports_lateral_joins() const {
            return false;
        }

        // Value formatting
        [[nodiscard]] virtual std::string format_value(const FieldValue& value) = 0;

        // TODO: implement rvalue format_value method
    };
}  // namespace demiplane::db
