#pragma once

#include <iomanip>

#include "sql_dialect.hpp"

namespace demiplane::db {
    class PostgresDialect final : public SqlDialect {
    public:
        [[nodiscard]] std::string quote_identifier(std::string_view name) const override;

        [[nodiscard]] std::string placeholder(std::size_t index) const override;

        [[nodiscard]] std::string limit_clause(std::size_t limit, std::size_t offset) const override;

        [[nodiscard]] bool supports_returning() const override {
            return true;
        }

        [[nodiscard]] bool supports_lateral_joins() const override {
            return true;
        }

        [[nodiscard]] std::string format_value(const FieldValue& value) override;

    private:
        // Helper methods for formatting
        static std::string escape_string(const std::string& str);

        static std::string format_binary_data(std::span<const uint8_t> data);
    };
}  // namespace demiplane::db
