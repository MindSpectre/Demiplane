#pragma once

#include <array>
#include <string_view>

#include <db_typed_column.hpp>
#include <gears_utils.hpp>
#include <providers.hpp>
#include <schema/db_static_field_schema.hpp>

namespace demiplane::db {

    /// Helper to get the Nth type from a parameter pack
    template <std::size_t I, typename... Ts>
    using pack_element_t = std::tuple_element_t<I, std::tuple<Ts...>>;

    /**
     * @brief Compile-time static table parameterized on StaticFieldSchema types
     *
     * Primary table type for schemas known at compile time. The table name is
     * an NTTP (FixedString), eliminating runtime storage and SSO dependency.
     * Fields are specified as template parameters using
     * StaticFieldSchema<CppType, Name, Constraints...>.
     *
     * @tparam TableName    Table name as compile-time FixedString
     * @tparam FieldSchemas Field schemas satisfying IsStaticFieldSchema
     */
    template <gears::FixedString TableName, typename... FieldSchemas>
        requires(IsStaticFieldSchema<FieldSchemas> && ...)
    class StaticTable {
    public:
        static constexpr std::size_t N = sizeof...(FieldSchemas);

        [[nodiscard]] explicit constexpr StaticTable(const Providers provider = Providers::None) noexcept
            : provider_{provider} {
        }

        /// Compile-time name lookup — auto-deduces value type, returns TypedColumn
        template <gears::FixedString Name>
        [[nodiscard]] constexpr auto column() const {
            constexpr auto I = find_index<Name>();
            using T          = pack_element_t<I, FieldSchemas...>::value_type;
            return TypedColumn<T>{Name.view(), table_name()};
        }

        /// Positional access — returns TypedColumn with deduced name and type
        template <std::size_t I>
            requires(I < N)
        [[nodiscard]] constexpr auto column() const {
            using Schema = pack_element_t<I, FieldSchemas...>;
            using T      = Schema::value_type;
            return TypedColumn<T>{Schema::name(), table_name()};
        }

        [[nodiscard]] static constexpr std::string_view table_name() noexcept {
            return TableName.view();
        }

        [[nodiscard]] constexpr std::size_t field_count() const noexcept {
            gears::force_non_static(this);
            return N;
        }

        [[nodiscard]] constexpr Providers provider() const noexcept {
            return provider_;
        }

        /// Compile-time iteration over all field schemas
        template <typename Visitor>
        constexpr void for_each_field(Visitor&& v) const {
            gears::force_non_static(this);
            (v.template operator()<FieldSchemas>(), ...);
        }

    private:
        template <gears::FixedString Name>
        static constexpr std::size_t find_index() {
            constexpr std::array<std::string_view, N> names = {FieldSchemas::name()...};
            for (std::size_t i = 0; i < names.size(); ++i) {
                if (names[i] == std::string_view{Name})
                    return i;
            }
            std::unreachable();
        }

        Providers provider_ = Providers::None;
    };

}  // namespace demiplane::db
