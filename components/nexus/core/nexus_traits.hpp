#pragma once

#include <cstdint>
#include <type_traits>
#include <typeindex>

namespace demiplane::nexus {

    // detect T::nx_id → default id

    template<class, class = void>
    struct has_nx_id : std::false_type {};

    template<class T>
    struct has_nx_id<T, std::void_t<decltype(T::nx_id)>> : std::true_type {};

    template<class T>
    constexpr std::uint32_t default_id_v = has_nx_id<T>::value ? T::nx_id : 0u;

}