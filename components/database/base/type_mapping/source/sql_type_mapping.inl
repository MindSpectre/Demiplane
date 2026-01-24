#pragma once

#include <db_table.hpp>
#include <gears_concepts.hpp>
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

    // DIALECT-BASED TABLE CONSTRUCTORS
    // (Implemented here because sql_dialect.hpp is available)

    // Constructor with dialect reference (extracts provider)
    template <gears::IsStringLike StringTp>
    constexpr Table::Table(StringTp&& table_name, const SqlDialect& dialect)
        : table_name_{std::forward<StringTp>(table_name)},
          provider_{dialect.type()} {
    }

    // Constructor with dialect pointer (extracts provider, nullptr -> None)
    template <gears::IsStringLike StringTp>
    constexpr Table::Table(StringTp&& table_name, const SqlDialect* dialect)
        : table_name_{std::forward<StringTp>(table_name)},
          provider_{dialect ? dialect->type() : Providers::None} {
    }

    // Infer SQL type from stored provider - throws if provider not set
    template <typename T, gears::IsStringLike StringTp>
    Table& Table::add_field(StringTp&& name) {
        if (provider_ == Providers::None) {
            throw std::logic_error{
                "Cannot infer SQL type: Table has no provider set. "
                "Use Table(name, provider) constructor."};
        }
        return add_field<T>(std::forward<StringTp>(name), sql_type<T>(provider_));
    }
    template <HasSchemaInfo Schema>
    std::shared_ptr<Table> Table::make(Providers provider) {
        return std::make_shared<Table>(std::string(Schema::table_name), Schema{}, provider);
    }
}  // namespace demiplane::db
