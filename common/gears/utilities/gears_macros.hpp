#pragma once

// Helper to detect if macro has arguments (0 = no args, 1 = has args)
// #define HAS_ARGS(...) HAS_ARGS_IMPL(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0)
// #define HAS_ARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...) N
#define HAS_ARGS(...) __VA_OPT__(TRUE)
// Concatenation macros
#define CONCAT(A, B) CONCAT_IMPL(A, B)
#define CONCAT_IMPL(A, B) A##B

#define GEARS_UNUSED_VAR [[maybe_unused]] auto _unused_var_

/**
 * @def GEARS_UNREACHABLE(TYPE, TEXT)
 * @brief Mark an `if constexpr` branch as unreachable.
 *
 * Expands to a `static_assert(dependent_false_v<TYPE>, TEXT)` followed by
 * `std::unreachable()`, so the assert fires only when the branch is actually
 * instantiated for `TYPE`.
 */
#define GEARS_UNREACHABLE(TYPE, TEXT)                                                                                  \
    static_assert(demiplane::gears::dependent_false_v<TYPE>, TEXT);                                                    \
    std::unreachable();
