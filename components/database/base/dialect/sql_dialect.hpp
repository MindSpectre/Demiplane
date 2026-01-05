#pragma once
#include <string>

#include <sql_params.hpp>

#include "supported_providers.hpp"

namespace demiplane::db {

    class SqlDialect {
    public:
        virtual ~SqlDialect() = default;

        // Identifier quoting
        /// @deprecated copying
        [[nodiscard]] virtual std::string quote_identifier(std::string_view name) const = 0;

        virtual void quote_identifier(std::string& query, std::string_view name) const = 0;

        virtual void quote_identifier(std::pmr::string& query, std::string_view name) const = 0;

        // Parameter placeholders
        [[nodiscard]] virtual std::string placeholder(std::size_t index) const = 0;

        virtual void placeholder(std::string& query, std::size_t index) const = 0;

        virtual void placeholder(std::pmr::string& query, std::size_t index) const = 0;

        // Limit/Offset syntax
        [[nodiscard]] virtual std::string limit_clause(std::size_t limit, std::size_t offset) const = 0;

        virtual void limit_clause(std::string& query, std::size_t limit, std::size_t offset) const = 0;

        virtual void limit_clause(std::pmr::string& query, std::size_t limit, std::size_t offset) const = 0;

        // Feature support
        [[nodiscard]] constexpr virtual bool supports_returning() const {
            return false;
        }

        [[nodiscard]] constexpr virtual bool supports_cte() const {
            return true;
        }

        [[nodiscard]] constexpr virtual bool supports_window_functions() const {
            return true;
        }

        [[nodiscard]] constexpr virtual bool supports_lateral_joins() const {
            return false;
        }
        // Value formatting
        virtual void format_value(std::string& query, const FieldValue& value) = 0;

        virtual void format_value(std::pmr::string& query, const FieldValue& value) = 0;

        virtual DialectBindPacket make_param_sink(std::pmr::memory_resource* memory_resource) const = 0;

        [[nodiscard]] constexpr virtual SupportedProviders type() const = 0;
    };
}  // namespace demiplane::db
