#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "gears_concepts.hpp"
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

    /// Compile-time error sink for `if constexpr` chains — fires only when the
    /// branch is actually instantiated, via `dependent_false_v<T>`.
    template <class T = void>
    constexpr void unreachable() {
        static_assert(dependent_false_v<T>, "Unreachable branch reached.");
    }

    /// `consteval` variant of `unreachable()` — same compile-time guarantee plus
    /// `std::unreachable()` to mark the branch as runtime-undefined.
    template <typename CheckedType = void>
    consteval void unreachable_c() {
        static_assert(dependent_false_v<CheckedType>, "Unreachable branch reached.");
        std::unreachable();
    }

    /// Static guard ensuring the enclosing function is non-static — pass
    /// `this`; it fails to compile when the type deduces to `nullptr_t`.
    template <typename T>
    constexpr void force_non_static(const T* this_ptr) {
        static_assert(!std::is_same_v<T, std::nullptr_t>, "This function cannot be called from a static context");
        static_cast<void>(this_ptr);
    }


    /// Static guard ensuring the deduced argument type is not `const`-qualified.
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
    /// Uses 2-digit lookup table to halve the number of divisions.
    constexpr std::string constexpr_to_string(const std::size_t value) {
        if (value == 0) {
            return "0";
        }

        // 2-digit lookup: digits[i*2] = tens digit, digits[i*2+1] = units digit of i
        constexpr char digits[] = "00010203040506070809"
                                  "10111213141516171819"
                                  "20212223242526272829"
                                  "30313233343536373839"
                                  "40414243444546474849"
                                  "50515253545556575859"
                                  "60616263646566676869"
                                  "70717273747576777879"
                                  "80818283848586878889"
                                  "90919293949596979899";

        // Maximum digits for std::size_t (64-bit): 20 digits
        char buffer[20] = {};
        std::size_t pos = 0;

        // Extract 2 digits at a time in reverse order
        auto temp = value;
        while (temp >= 100) {
            const auto idx  = (temp % 100) * 2;
            buffer[pos++]   = digits[idx + 1];  // units
            buffer[pos++]   = digits[idx];      // tens
            temp           /= 100;
        }

        // Handle remaining 1-2 digits
        if (temp >= 10) {
            const auto idx = temp * 2;
            buffer[pos++]  = digits[idx + 1];
            buffer[pos++]  = digits[idx];
        } else {
            buffer[pos++] = static_cast<char>('0' + temp);
        }

        // Build string by reading buffer in reverse
        std::string result{};
        result.reserve(pos);
        for (std::size_t i = pos; i > 0; --i) {
            result += buffer[i - 1];
        }

        return result;
    }

    /// Naive constexpr float/double-to-string via digit extraction.
    /// Extracts integer part via constexpr_to_string(size_t), then fractional
    /// digits by repeated multiply-by-10. Uses N-1 fractional digits (6 for float,
    /// 14 for double) to stay below representation noise in the last ULP.
    template <std::floating_point FloatT>
    constexpr std::string constexpr_to_string(const FloatT val) {
        // ReSharper disable once CppIdenticalOperandsInBinaryExpression
        if (val != val)
            return "NaN";
        if (val == std::numeric_limits<FloatT>::infinity())
            return "Infinity";
        if (val == -std::numeric_limits<FloatT>::infinity())
            return "-Infinity";

        std::string result;
        auto abs_val = val;
        if (val < FloatT{0}) {
            result  += '-';
            abs_val  = -val;
        }
        if (abs_val == FloatT{0})
            return result + '0';

        // Integer part
        const auto int_part  = static_cast<std::uint64_t>(abs_val);
        result              += constexpr_to_string(int_part);

        // Fractional part via digit extraction
        auto frac = abs_val - static_cast<FloatT>(int_part);
        if (frac == FloatT{0})
            return result;

        // N-1 digits: avoids noise from the last representable bit
        constexpr int max_frac = std::is_same_v<FloatT, float> ? 6 : 14;

        char frac_buf[16] = {};
        int frac_len      = 0;

        for (int i = 0; i < max_frac; ++i) {
            frac       *= FloatT{10};
            auto digit  = static_cast<unsigned>(frac);
            if (digit > 9)
                digit = 9;  // guard against fp accumulation
            frac_buf[frac_len++]  = static_cast<char>('0' + digit);
            frac                 -= static_cast<FloatT>(digit);
        }

        // Strip trailing zeros
        while (frac_len > 0 && frac_buf[frac_len - 1] == '0')
            --frac_len;

        if (frac_len > 0) {
            result += '.';
            for (int i = 0; i < frac_len; ++i)
                result += frac_buf[i];
        }

        return result;
    }

    /// Extract null-terminated const char* from any IsNullTerminatedString
    template <IsNullTerminatedString T>
    constexpr const char* as_c_str(const T& s) noexcept {
        if constexpr (requires { s.c_str(); }) {
            return s.c_str();
        } else {
            return s;
        }
    }
}  // namespace demiplane::gears
