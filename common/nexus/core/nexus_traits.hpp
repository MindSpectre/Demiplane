#pragma once

#include <cstdint>
#include <type_traits>

namespace demiplane::nexus {
    // detect T::nexus_id → default id

    template <class, class = void>
    struct has_nexus_id : std::false_type {};

    template <class T>
    struct has_nexus_id<T, std::void_t<decltype(T::nexus_id)>> : std::true_type {};

    template <class T>
    constexpr std::uint32_t default_id_v = has_nexus_id<T>::value ? T::nexus_id : 0u;
}