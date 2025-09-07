#pragma once

#include <type_traits>

#include "policies.hpp"

namespace demiplane::nexus {
    // detect T::nexus_id → default id

    template <class, class = void>
    struct has_nexus_policy : std::false_type {};

    template <class T>
    struct has_nexus_policy<T, std::void_t<decltype(T::nexus_policy)>> : std::true_type {};

    template <class T>
    constexpr Lifetime get_nexus_policy() {
        if constexpr (has_nexus_policy<T>::value) {
            return T::nexus_policy;
        } else {
            return Resettable{};
        }
    }
}  // namespace demiplane::nexus
