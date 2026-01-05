#pragma once

#include <db_table.hpp>
#include <sql_dialect.hpp>
namespace demiplane::db {

    template <typename T>
    std::string_view sql_type(const SqlDialect& dialect) {
        return sql_type<T>(dialect.type());
    }

    template <typename T>
    std::string_view sql_type(const SqlDialect* dialect) {
        // Note: caller should ensure dialect is not null
        // A null dialect will cause undefined behavior (dereferencing nullptr)
        return sql_type<T>(dialect->type());
    }

    // ═══════════════════════════════════════════════════════════════
    // TYPE-INFERRED ADD_FIELD
    // ═══════════════════════════════════════════════════════════════

    template <typename T>
    Table& Table::add_field(std::string name, const SqlDialect& dialect) {
        return add_field<T>(std::move(name), std::string(sql_type<T>(dialect)));
    }

    template <typename T>
    Table& Table::add_field(std::string name, const SqlDialect* dialect) {
        return add_field<T>(std::move(name), std::string(sql_type<T>(dialect)));
    }

    template <typename T>
    Table& Table::add_field(std::string name, SupportedProviders provider) {
        return add_field<T>(std::move(name), std::string(sql_type<T>(provider)));
    }
}  // namespace demiplane::db
