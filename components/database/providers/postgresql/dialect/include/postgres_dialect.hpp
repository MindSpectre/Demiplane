#pragma once

#include <iomanip>

#include <sql_dialect.hpp>

namespace demiplane::db::postgres {

    class Dialect final : public SqlDialect {
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

        [[nodiscard]] DialectBindPacket make_param_sink(std::pmr::memory_resource* memory_resource) const override;

        [[nodiscard]] constexpr SupportedProviders type() const override;

    private:
        template <typename String>
        static void format_value_impl(String& query, const FieldValue& value);

        // Helper methods for formatting
        template <typename String>
        static void escape_char(String& out, char c);

        template <typename String>
        static void escape_string(String& out, std::string_view str);

        template <typename Container>
        static std::string format_binary_data(const Container& data);
    };
}  // namespace demiplane::db::postgres

#include "../source/postgres_dialect.inl"
