#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "gears_templates.hpp"

namespace demiplane::gears {
    template <typename V>
    constexpr void unused_value(const V& value) {
        static_cast<void>(value);
    }

    template <typename V, typename... Rest>
    consteval void unused_value(const V& value, const Rest&... rest) {
        static_cast<void>(value);
        unused_value(rest...);  // Recursively process remaining arguments
    }

    template <class T = void>
    constexpr void unreachable() {
        static_assert(dependent_false_v<T>, "Unreachable branch reached.");
    }

    template <typename CheckedType = void>
    consteval void unreachable_c() {
        static_assert(dependent_false_v<CheckedType>, "Unreachable branch reached.");
        std::unreachable();
    }

    template <typename T>
    constexpr void force_non_static(const T* this_ptr) {
        static_assert(!std::is_same_v<T, std::nullptr_t>, "This function cannot be called from a static context");
        static_cast<void>(this_ptr);
    }


    template <typename T>
    constexpr void force_non_const(T) {
        static_assert(!std::is_const_v<std::remove_pointer_t<T>>, "Function must not be const-qualified!");
    }

    // Elvis operator equivalent: value_or(x, y) == (x ?: y) in GCC
    // Returns value if truthy, otherwise fallback. Value is evaluated only once.
    template <typename T, typename U>
    constexpr auto value_or(T&& value, U&& fallback) noexcept(noexcept(value ? std::forward<T>(value)
                                                                             : std::forward<U>(fallback))) {
        return value ? std::forward<T>(value) : std::forward<U>(fallback);
    }

    /// Constexpr integer-to-string conversion for use in compile-time SQL generation.
    /// std::to_string is not constexpr in C++23, so this provides an alternative
    /// for placeholder indices ($1, $2) and LIMIT/OFFSET clauses.
    constexpr std::string constexpr_to_string(const std::size_t value) {
        if (value == 0) {
            return "0";
        }

        // Maximum digits for std::size_t (64-bit): 20 digits
        char buffer[20] = {};
        std::size_t pos = 0;

        // Extract digits in reverse order
        auto temp = value;
        while (temp > 0) {
            buffer[pos++]  = static_cast<char>('0' + temp % 10);
            temp          /= 10;
        }

        // Build string by reading buffer in reverse
        std::string result{};
        result.reserve(pos);
        for (std::size_t i = pos; i > 0; --i) {
            result += buffer[i - 1];
        }

        return result;
    }
}  // namespace demiplane::gears
