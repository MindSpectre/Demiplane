#pragma once
#include <array>
#include <cstddef>

namespace demiplane::utilities::compile_time {
    template <std::size_t N>
    static constexpr std::size_t strlen_constexpr(const char (&w)[N]) {
        return N - 1;  // Exclude the null terminator
    }

    // Variadic concatenation function
    template <typename... Args>
    consteval auto consteval_concat(Args... args) {
        // Calculate total size of all strings combined
        constexpr std::size_t total_size = (strlen_constexpr(args) + ...);  // Fold expression

        // Create a result array
        std::array<char, total_size + 1> result = {};  // +1 for the final null terminator
        std::size_t offset                      = 0;

        // Lambda to copy each string into the result array
        auto copy_to_result = [&offset, &result](const auto& str) {
            for (std::size_t i = 0; i < strlen_constexpr(str); ++i) {
                result[offset + i] = str[i];
            }
            offset += strlen_constexpr(str);
        };

        // Expand each argument using the lambda
        (copy_to_result(args), ...);  // Fold expression to apply the lambda to each string

        // Add the final null terminator
        result[total_size] = '\0';

        return result;
    }
    // Overload + operator for compile-time string concatenation
}  // namespace demiplane::utilities::compile_time
