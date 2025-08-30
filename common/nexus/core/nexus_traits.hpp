#pragma once

#include <cstdint>
#include <type_traits>

#include "policies.hpp"

namespace demiplane::nexus {
    // detect T::nexus_id → default id

    template <class, class = void>
    struct has_nexus_id : std::false_type {};

    template <class T>
    struct has_nexus_id<T, std::void_t<decltype(T::nexus_id)>> : std::true_type {};


    template <class, class = void>
    struct has_nexus_policy : std::false_type {};

    template <class T>
    struct has_nexus_policy<T, std::void_t<decltype(T::nexus_policy)>> : std::true_type {};

    template <class T>
    constexpr std::uint32_t get_nexus_id() {
        if constexpr (has_nexus_id<T>::value) {
            return T::nexus_id;
        } else {
            return 0u;
        }
    }

    template <class T>
    constexpr Lifetime get_nexus_policy() {
        if constexpr (has_nexus_policy<T>::value) {
            return T::nexus_policy;
        } else {
            return Resettable{};
        }
    }
}  // namespace demiplane::nexus
