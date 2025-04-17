#pragma once

namespace demiplane::gears {


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


    template <typename T>
    concept Interface = std::is_abstract_v<T>;


}