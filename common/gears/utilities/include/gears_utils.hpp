#pragma once

#include "templates.hpp"
namespace demiplane::gears {
    template <typename V>
    consteval void unused_value(const V& value) {
        static_cast<void>(value);
    }
    template <typename V, typename... Rest>
    consteval void unused_value(const V& value, const Rest&... rest) {
        static_cast<void>(value);
        unused_value(rest...); // Recursively process remaining arguments
    }
    template <class T = void>
    constexpr void unreachable() {
        static_assert(dependent_false_v<T>, "Unreachable branch reached.");
    }

    constexpr void force_non_static(const void* this_) {
        static_cast<void>(this_);
    }

} // namespace demiplane::gears
