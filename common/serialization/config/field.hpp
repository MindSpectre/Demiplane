#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <gears_strings.hpp>

namespace demiplane::serialization {

    enum class FieldPolicy : std::uint8_t {
        Normal,    // serialize + deserialize
        Secret,    // deserialize only (e.g., passwords)
        Excluded,  // skip both directions
        ReadOnly,  // serialize only
    };

    /// Key wrapper for the read_field/write_field extension points. A domain
    /// type (rather than a bare string) so the machinery's unqualified,
    /// DEPENDENT calls always reach the format overloads: FieldName makes
    /// demiplane::serialization an ASSOCIATED NAMESPACE of every call, which
    /// two-phase lookup requires — the format headers (json.hpp) are normally
    /// included AFTER config_interface.hpp, so ordinary lookup at the template
    /// definition point sees none of them, and a plain string key carries no
    /// namespace ADL could find them through.
    struct FieldName {
        std::string_view value;

        [[nodiscard]] std::string str() const {
            return std::string{value};
        }
    };

    namespace detail {
        template <typename T>
        struct member_pointer_traits;

        template <typename C, typename V>
        struct member_pointer_traits<V C::*> {
            using owner_type = C;
            using value_type = V;
        };
    }  // namespace detail

    template <auto Ptr, gears::FixedString Name, FieldPolicy Policy = FieldPolicy::Normal>
    struct Field {
        static constexpr auto ptr    = Ptr;
        static constexpr auto name   = Name;
        static constexpr auto policy = Policy;

        using owner_type = detail::member_pointer_traits<decltype(Ptr)>::owner_type;
        using value_type = detail::member_pointer_traits<decltype(Ptr)>::value_type;
    };

}  // namespace demiplane::serialization
