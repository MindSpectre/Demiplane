#pragma once

#include <db_table.hpp>
#include <gears_concepts.hpp>

namespace demiplane::db {

    // Infer SQL type from stored provider - throws if provider not set
    template <typename T, gears::IsStringLike StringTp>
    DynamicTable& DynamicTable::add_field(StringTp&& name) {
        if (provider_ == Providers::None) {
            throw std::logic_error{"Cannot infer SQL type: DynamicTable has no provider set. "
                                   "Use DynamicTable(name, provider) constructor."};
        }
        return add_field<T>(std::forward<StringTp>(name), sql_type<T>(provider_));
    }

}  // namespace demiplane::db
