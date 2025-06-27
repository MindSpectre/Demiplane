#pragma once

#include <string_view>

namespace demiplane::gears {

    template <typename T>
    concept HasStaticNameM = requires {
        { T::name } -> std::convertible_to<std::string_view>;
    };
    template <typename T>
    concept HasStaticNameF = requires {
        { T::name } -> std::convertible_to<std::string_view>;
    };
    template <typename T>
    concept HasStaticComp = requires(const T& a, const T& b) {
        { T::comp(a, b) } -> std::same_as<bool>;
    };


} // namespace demiplane::gears
