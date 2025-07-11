#pragma once

#include <utility>

#include "templates.hpp"
namespace demiplane::gears {
    template <typename V>
    constexpr void unused_value(const V& value) {
        static_cast<void>(value);
    }

    template <typename CheckedType = void>
    [[noreturn]] consteval auto unreachable() {
        static_assert(dependent_false_v<CheckedType>, "Unreachable branch reached.");
        std::unreachable();
    }

    template <typename T>
    constexpr void enforce_non_static(const T* this_ptr) {
        static_assert(!std::is_same_v<T, std::nullptr_t>, "This function cannot be called from a static context");

        static_cast<void>(this_ptr);
    }


    template <typename T>
    constexpr void enforce_non_const(T) {
        static_assert(!std::is_const_v<std::remove_pointer_t<T>>, "Function must not be const-qualified!");
    }

} // namespace demiplane::gears
