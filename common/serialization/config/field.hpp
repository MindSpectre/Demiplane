#pragma once

#include <cstdint>

#include <gears_strings.hpp>

namespace demiplane::serialization {

    enum class FieldPolicy : std::uint8_t {
        Normal,    // serialize + deserialize
        Secret,    // deserialize only (e.g., passwords)
        Excluded,  // skip both directions
        ReadOnly,  // serialize only
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
