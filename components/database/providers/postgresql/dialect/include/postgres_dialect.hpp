#pragma once

#include <iomanip>

#include <pg_type_registry.hpp>
#include <sql_dialect.hpp>

namespace demiplane::db {

    class PostgresDialect final : public SqlDialect {
    public:
        [[nodiscard]] std::string quote_identifier(std::string_view name) const override;

        void quote_identifier(std::string& query, std::string_view name) const override;

        void quote_identifier(std::pmr::string& query, std::string_view name) const override;

        [[nodiscard]] std::string placeholder(std::size_t index) const override;

        void placeholder(std::string& query, std::size_t index) const override;

        void placeholder(std::pmr::string& query, std::size_t index) const override;

        [[nodiscard]] std::string limit_clause(std::size_t limit, std::size_t offset) const override;

        void limit_clause(std::string& query, std::size_t limit, std::size_t offset) const override;

        void limit_clause(std::pmr::string& query, std::size_t limit, std::size_t offset) const override;

        [[nodiscard]] bool supports_returning() const override {
            return true;
        }

        [[nodiscard]] bool supports_lateral_joins() const override {
            return true;
        }

        void format_value(std::string& query, const FieldValue& value) override;

        void format_value(std::pmr::string& query, const FieldValue& value) override;

        DialectBindPacket make_param_sink(std::pmr::memory_resource* memory_resource) const override;

    private:
        // Helper methods for formatting
        static std::string escape_string(std::string_view str);

        static std::string format_binary_data(std::span<const uint8_t> data);
    };
}  // namespace demiplane::db
