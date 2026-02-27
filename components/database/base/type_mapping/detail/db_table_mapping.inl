#pragma once

#include <db_table.hpp>
#include <gears_concepts.hpp>

namespace demiplane::db {

    template <HasSchemaFields Schema, gears::IsStringLike StringTp>
    constexpr Table::Table(StringTp&& table_name, [[maybe_unused]] Schema schema, Providers provider)
        : table_name_{std::forward<StringTp>(table_name)},
          provider_{provider} {
        // Use compile-time reflection to add all fields from Schema (with empty db_type)
        [provider]<typename... FieldDefs>(gears::type_list<FieldDefs...>, Table* table) {
            (table->add_field<typename FieldDefs::value_type>(std::string(FieldDefs::name()),
                                                              sql_type_for<typename FieldDefs::value_type>(provider)),
             ...);
        }(typename Schema::fields{}, this);
    }

    // Infer SQL type from stored provider - throws if provider not set
    template <typename T, gears::IsStringLike StringTp>
    Table& Table::add_field(StringTp&& name) {
        if (provider_ == Providers::None) {
            throw std::logic_error{"Cannot infer SQL type: Table has no provider set. "
                                   "Use Table(name, provider) constructor."};
        }
        return add_field<T>(std::forward<StringTp>(name), sql_type<T>(provider_));
    }

    template <HasSchemaInfo Schema>
    std::shared_ptr<Table> Table::make(Providers provider) {
        return std::make_shared<Table>(std::string(Schema::table_name), Schema{}, provider);
    }
}  // namespace demiplane::db
