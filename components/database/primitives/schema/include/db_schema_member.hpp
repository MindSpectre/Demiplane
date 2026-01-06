#pragma once

#include <string_view>

#include <db_field_def.hpp>
#include <gears_types.hpp>

namespace demiplane::db {

    // SchemaMember: Compile-time field definition for type-safe column access
    //
    // Usage:
    //   struct Customer {
    //       int id;
    //       std::string name;
    //
    //       DB_ENTITY(Customer, "customers", id, name);
    //   };
    //
    //   auto table = Table::make<Customer::Schema>();
    //   auto col = table->column(Customer::Schema::id);  // TableColumn<int>
    //

    template <gears::FixedString Name, typename MemberType>
    struct SchemaMember {
        using value_type = MemberType;

        [[nodiscard]] static constexpr std::string_view name() noexcept {
            return std::string_view(Name);
        }
    };

    // Factory: db::field<FixedString{"name"}>(&Class::member)
    template <gears::FixedString Name, typename Class, typename MemberType>
    [[nodiscard]] constexpr auto field(MemberType Class::*) noexcept {
        return SchemaMember<Name, MemberType>{};
    }

    // Concept for schema types with table_name and fields
    template <typename T>
    concept HasSchemaInfo = requires {
        { T::table_name } -> std::convertible_to<std::string_view>;
        typename T::fields;
    };

}  // namespace demiplane::db
