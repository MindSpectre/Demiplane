#pragma once

#include <variant>
#include <type_traits>
#include "gears_concepts.hpp"
#include "gears_templates.hpp"
namespace demiplane::gears {

    // Forward declaration
    template <typename T, typename... Errors>
    class Outcome;

    // Error wrapper for implicit construction
    template <typename E>
    struct ErrorTag {
        E error;
        explicit ErrorTag(E&& e) : error(std::forward<E>(e)) {}
    };

    // Helper to deduce error type
    template <typename E>
    ErrorTag<std::decay_t<E>> Err(E&& e) {
        return ErrorTag<std::decay_t<E>>{std::forward<E>(e)};
    }

    // Success wrapper for implicit construction
    template <typename T>
    struct SuccessTag {
        T value;
        explicit SuccessTag(T&& v) : value(std::forward<T>(v)) {}
    };

    template <typename T>
    SuccessTag<std::decay_t<T>> Ok(T&& v) {
        return SuccessTag<std::decay_t<T>>{std::forward<T>(v)};
    }

    template <typename T, typename... Errors>
    class Outcome {
    public:
        using value_type = T;
        using error_types = std::variant<Errors...>;

        // Default constructor (requires T to be default constructible)
        Outcome() requires std::default_initializable<T>
            : value_(T{}) {}

        // Implicit construction from value
        template<typename U = std::remove_cv_t<T>>
        constexpr  explicit(!std::is_convertible_v<U, T>) Outcome(T value)
            : value_(std::move(value)) {}

        // Implicit construction from SuccessTag
        template <typename U = std::remove_cv_t<T>>
        // ReSharper disable once CppNonExplicitConvertingConstructor
        constexpr explicit(!std::is_convertible_v<U, T>) Outcome(SuccessTag<U>&& tag)
            : value_(std::move(tag.value)) {}

        // Implicit construction from ErrorTag
        template <OneOf<Errors...> E>
        constexpr explicit(!gears::OneOf<E, Errors...>) Outcome(ErrorTag<E>&& tag)
            : value_(std::move(tag.error)) {}

        // Direct error construction - more ergonomic
        template <OneOf<Errors...> E>
        static constexpr Outcome error(E&& e) {
            Outcome result;
            result.value_ = std::forward<E>(e);
            return result;
        }

        // Success factory
        static constexpr Outcome success(T&& value) {
            return Outcome{std::forward<T>(value)};
        }

        [[nodiscard]] constexpr bool is_success() const noexcept {
            return std::holds_alternative<T>(value_);
        }

        [[nodiscard]] constexpr bool is_error() const noexcept {
            return !is_success();
        }

        template <OneOf<Errors...> E>
        [[nodiscard]] constexpr bool holds_error() const noexcept {
            return std::holds_alternative<E>(value_);
        }

        // Operator bool for if statements
        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return is_success();
        }

        // Value accessors with better names
        [[nodiscard]] constexpr const T& value() const& {
            if (!is_success()) {
                throw std::bad_variant_access{};
            }
            return std::get<T>(value_);
        }

        [[nodiscard]] constexpr T& value() & {
            if (!is_success()) {
                throw std::bad_variant_access{};
            }
            return std::get<T>(value_);
        }

        [[nodiscard]] constexpr T&& value() && {
            if (!is_success()) {
                throw std::bad_variant_access{};
            }
            return std::move(std::get<T>(value_));
        }

        // Operator * for dereferencing like a pointer
        [[nodiscard]] constexpr const T& operator*() const& {
            return value();
        }

        [[nodiscard]] constexpr T& operator*() & {
            return value();
        }

        [[nodiscard]] constexpr T&& operator*() && {
            return std::move(value());
        }

        // Operator -> for member access
        [[nodiscard]] constexpr const T* operator->() const {
            return &value();
        }

        [[nodiscard]] constexpr T* operator->() {
            return &value();
        }

        template <class U = T>
        constexpr T value_or(U&& default_value) const& {
            if (is_success()) {
                return std::get<T>(value_);
            }
            return static_cast<T>(std::forward<U>(default_value));
        }

        template <class U = T>
        constexpr T value_or(U&& default_value) && {
            if (is_success()) {
                return std::move(std::get<T>(value_));
            }
            return static_cast<T>(std::forward<U>(default_value));
        }

        // Get specific error type
        template <OneOf<Errors...> E>
        [[nodiscard]] constexpr const E& error() const& {
            return std::get<E>(value_);
        }

        template <OneOf<Errors...> E>
        [[nodiscard]] constexpr E& error() & {
            return std::get<E>(value_);
        }

        template <OneOf<Errors...> E>
        [[nodiscard]] constexpr E&& error() && {
            return std::move(std::get<E>(value_));
        }

        // Get error as variant
        [[nodiscard]] constexpr const error_types& error_variant() const& {
            if (is_success()) {
                throw std::bad_variant_access{};
            }
            return std::get<error_types>(
                reinterpret_cast<const std::variant<T, error_types>&>(value_)
            );
        }

        // Monadic operations
        template <typename F>
        constexpr auto and_then(F&& f) & -> decltype(f(value())) {
            if (is_success()) {
                return f(value());
            }
            return *this;
        }

        template <typename F>
        constexpr auto and_then(F&& f) && -> decltype(f(std::move(value()))) {
            if (is_success()) {
                return f(std::move(value()));
            }
            return std::move(*this);
        }

        template <typename F>
        constexpr auto or_else(F&& f) & {
            if (!is_success()) {
                return f();
            }
            return *this;
        }

        template <typename F>
        constexpr auto transform(F&& f) & {
            using U = decltype(f(value()));
            if (is_success()) {
                return Outcome<U, Errors...>{f(value())};
            }
            // Need to convert errors
            Outcome<U, Errors...> result;
            std::visit([&result](auto&& err) {
                if constexpr (!std::is_same_v<std::decay_t<decltype(err)>, T>) {
                    result = ErrorTag{std::forward<decltype(err)>(err)};
                }
            }, value_);
            return result;
        }

        // Visit pattern for exhaustive matching
        template <typename... Fs>
        constexpr decltype(auto) visit(Fs&&... fs) & {
            return std::visit(overloaded{std::forward<Fs>(fs)...}, value_);
        }

        template <typename... Fs>
        constexpr decltype(auto) visit(Fs&&... fs) const& {
            return std::visit(overloaded{std::forward<Fs>(fs)...}, value_);
        }

        template <typename... Fs>
        constexpr decltype(auto) visit(Fs&&... fs) && {
            return std::visit(overloaded{std::forward<Fs>(fs)...}, std::move(value_));
        }

    private:
        std::variant<T, Errors...> value_;
    };



    // Convenience factory functions
    template <typename T, typename... Errors>
    constexpr auto success(T&& value) -> Outcome<std::decay_t<T>, Errors...> {
        return Outcome<std::decay_t<T>, Errors...>::success(std::forward<T>(value));
    }

    template <typename... Errors, typename E>
    requires OneOfDecayed<E, Errors...>
    constexpr auto failure(E&& error) -> Outcome<void, Errors...> {
        return Outcome<void, Errors...>::error(std::forward<E>(error));
    }

} // namespace demiplane::gears