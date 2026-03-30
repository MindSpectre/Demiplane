#pragma once

#include <concepts>
#include <string>
#include <string_view>

#include <db_field_value.hpp>
#include <providers.hpp>

namespace demiplane::db {

    /// @brief Concept for types that support appending string_view and char.
    template <typename T>
    concept Appendable = requires(T& s, std::string_view sv, char c) {
        { s += sv };
        { s += c };
    };

    /// @brief Concept for static dialect types that provide SQL generation methods.
    /// @details Checks with std::string as the concrete Appendable type since the
    ///          dialect methods are templates constrained on Appendable.
    template <typename T>
    concept IsSqlDialect = requires(std::string& s,
                                    std::string_view name,
                                    std::size_t idx,
                                    std::size_t limit,
                                    std::size_t offset,
                                    const FieldValue& fv) {
        { T::quote_identifier(s, name) };
        { T::placeholder(s, idx) };
        { T::limit_clause(s, limit, offset) };
        { T::format_value(s, fv) };
        { T::supports_returning() } -> std::same_as<bool>;
        { T::supports_cte() } -> std::same_as<bool>;
        { T::supports_window_functions() } -> std::same_as<bool>;
        { T::supports_lateral_joins() } -> std::same_as<bool>;
        { T::type() } -> std::same_as<Providers>;
    };

    /// @brief CRTP base providing default feature-support flags for dialect implementations.
    template <typename>
    struct DialectBase {
        [[nodiscard]] static constexpr bool supports_cte() noexcept {
            return true;
        }
        [[nodiscard]] static constexpr bool supports_window_functions() noexcept {
            return true;
        }
        [[nodiscard]] static constexpr bool supports_lateral_joins() noexcept {
            return false;
        }
        [[nodiscard]] static constexpr bool supports_returning() noexcept {
            return false;
        }
    };

}  // namespace demiplane::db
