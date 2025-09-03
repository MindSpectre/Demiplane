#pragma once

#include <chrono>
#include <string_view>

namespace demiplane::gears {
    template <typename T>
    concept HasStaticNameMember = requires {
        { T::name } -> std::convertible_to<std::string_view>;
    };
    template <typename T>
    concept HasStaticNameFunction = requires {
        { T::name() } -> std::convertible_to<std::string_view>;
    };
    template <typename T>
    concept HasStaticComparator = requires(const T& a, const T& b) {
        { T::comp(a, b) } -> std::same_as<bool>;
    };

    template <typename T>
    concept IsInterface = std::is_abstract_v<T>;

    template <typename T>
    concept IsDuration = std::chrono::__is_duration_v<T>;
}  // namespace demiplane::gears
