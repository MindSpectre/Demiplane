#pragma once

#include <concepts>
#include <type_traits>

namespace demiplane::serialization {

    template <typename T>
    concept HasFields = requires { T::fields(); };

    template <typename T, typename Format>
    concept HasCustomSerialize = requires(const T& t) {
        { t.custom_serialize(std::type_identity<Format>{}) } -> std::same_as<Format>;
    };

    template <typename T, typename Format>
    concept HasCustomDeserialize = requires(const Format& f) {
        { T::custom_deserialize(f) } -> std::same_as<T>;
    };

}  // namespace demiplane::serialization
