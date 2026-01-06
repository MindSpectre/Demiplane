#pragma once

#include <string_view>

#include <gears_templates.hpp>
#include <gears_types.hpp>

namespace demiplane::db {
    // Compile-time field definition - embeds type in template parameter
    template <gears::FixedString Name, typename CppType>
    struct FieldDef {
        using value_type = CppType;

        static constexpr std::string_view name() {
            return std::string_view(Name);
        }

        // Equality based on name (for uniqueness checks)
        template <gears::FixedString OtherName, typename OtherType>
        constexpr bool operator==(const FieldDef<OtherName, OtherType>&) const {
            return Name == OtherName;
        }
    };

    // Helper concept to check if a type is a FieldDef
    template <typename T>
    concept IsFieldDef = requires {
        typename T::value_type;
        { T::name() } -> std::convertible_to<std::string_view>;
    };

    // Concept to validate that a FieldDef belongs to a specific Schema
    // Note: We validate at runtime in the accessor methods since compile-time
    // field name checking with FixedString is complex. The Schema template parameter
    // provides the primary type safety.
    template <typename T, typename Schema>
    concept IsFieldDefFromSchema = IsFieldDef<T> && requires { typename Schema::fields; };

    // Helper to check if a schema has a specific field by name (compile-time)
    template <typename Schema, gears::FixedString FieldName>
    struct schema_has_field {
    private:
        template <typename... Fields>
        static constexpr bool check_impl(gears::type_list<Fields...>) {
            return ((Fields::name() == std::string_view(FieldName)) || ...);
        }

    public:
        static constexpr bool value = check_impl(typename Schema::fields{});
    };

    template <typename Schema, gears::FixedString FieldName>
    inline constexpr bool schema_has_field_v = schema_has_field<Schema, FieldName>::value;

}  // namespace demiplane::db
