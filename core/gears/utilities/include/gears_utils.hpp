#pragma once

namespace demiplane {
    template <typename V>
    consteval void unused_value(const V& value) {
        static_cast<void>(value);
    }

    template <typename>
    struct always_false : std::false_type {};

    template <typename T>
    inline constexpr bool always_false_v = always_false<T>::value;

    // A simple trait to detect std::vector.
    template <typename>
    struct is_vector : std::false_type {};

    template <typename T, typename Alloc>
    struct is_vector<std::vector<T, Alloc>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_vector_v = is_vector<T>::value;


} // namespace demiplane
