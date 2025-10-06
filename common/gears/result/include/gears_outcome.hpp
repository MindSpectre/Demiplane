#pragma once

#include <functional>
#include <type_traits>
#include <variant>

#include "gears_concepts.hpp"
#include "gears_templates.hpp"

namespace demiplane::gears {
    // Error wrapper for implicit construction
    template <typename E>
    struct ErrorTag {
        E error;
        constexpr explicit ErrorTag(E e)
            : error(std::move(e)) {
        }
    };

    // Helper to deduce error type
    template <typename E>
    constexpr ErrorTag<std::decay_t<E>> Err(E&& e) {
        return ErrorTag<std::decay_t<E>>{std::forward<E>(e)};
    }

    // Success wrapper for implicit construction
    template <typename T>
    struct SuccessTag {
        T value;
        constexpr explicit SuccessTag(T v)
            : value(std::move(v)) {
        }
    };

    template <typename T>
    constexpr SuccessTag<std::decay_t<T>> Ok(T&& v) {
        return SuccessTag<std::decay_t<T>>{std::forward<T>(v)};
    }

    constexpr std::monostate Ok() {
        return {};
    }


    template <typename T, typename... Errors>
    class Outcome {
    public:
        using value_type  = T;
        using error_types = std::variant<Errors...>;

        // Default constructor (requires T to be default constructible)
        Outcome() noexcept(std::is_nothrow_constructible_v<T>)
            requires std::default_initializable<T>
            : value_(T{}) {
        }

        // Implicit construction from value - ensure U is not an error type
        template <typename U = std::remove_cv_t<T>>
            requires std::constructible_from<T, U> &&
                     (!OneOf<std::decay_t<U>, Errors...>) &&
                     (!std::same_as<std::decay_t<U>, ErrorTag<std::decay_t<U>>>) &&
                     (!std::same_as<std::decay_t<U>, SuccessTag<std::decay_t<U>>>)
        constexpr explicit(!std::is_convertible_v<U, T>) Outcome(U&& value)
            noexcept(std::is_nothrow_constructible_v<T, U>)
            : value_(std::forward<U>(value)) {
        }

        // Implicit construction from SuccessTag
        template <typename U>
            requires std::constructible_from<T, U>
        constexpr explicit(!std::is_convertible_v<U, T>) Outcome(SuccessTag<U>&& tag)
            noexcept(std::is_nothrow_constructible_v<T, U>)
            : value_(std::move(tag.value)) {
        }

        // Implicit construction from ErrorTag
        template <OneOf<Errors...> E>
        constexpr explicit(false) Outcome(ErrorTag<E>&& tag)
            noexcept(std::is_nothrow_move_constructible_v<E>)
            : value_(std::move(tag.error)) {
        }

        // Direct error construction
        template <OneOf<Errors...> E>
        static constexpr Outcome error(E&& e) {
            Outcome result;
            result.value_ = std::forward<E>(e);
            return result;
        }

        // Success factory
        static constexpr Outcome success(T&& value) noexcept(std::is_nothrow_constructible_v<T, T&&>) {
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

        // Value accessors
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

        // Operator * for dereferencing
        [[nodiscard]] constexpr const T& operator*() const& {
            return value();
        }

        [[nodiscard]] constexpr T& operator*() & {
            return value();
        }

        [[nodiscard]] constexpr T&& operator*() && {
            return std::move(*this).value();
        }

        // Operator -> for member access
        [[nodiscard]] constexpr const T* operator->() const {
            return &value();
        }

        [[nodiscard]] constexpr T* operator->() {
            return &value();
        }

        // value_or
        template <class U = T>
        constexpr T value_or(U&& default_value) const& {
            return is_success() ? std::get<T>(value_) : static_cast<T>(std::forward<U>(default_value));
        }

        template <class U = T>
        constexpr T value_or(U&& default_value) && {
            return is_success() ? std::move(std::get<T>(value_)) : static_cast<T>(std::forward<U>(default_value));
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

        // Monadic operations - and_then
        template <typename F>
            requires std::invocable<F, T&>
        constexpr auto and_then(F&& f) & -> std::invoke_result_t<F, T&> {
            using Result = std::invoke_result_t<F, T&>;
            if (is_success()) {
                return std::invoke(std::forward<F>(f), value());
            }
            // Propagate error to Result type
            Result result;
            std::visit([&result]<typename ErrT>(ErrT&& err) {
                if constexpr (!std::same_as<std::decay_t<ErrT>, T>) {
                    result = ErrorTag{std::forward<ErrT>(err)};
                }
            }, value_);
            return result;
        }

        template <typename F>
            requires std::invocable<F, const T&>
        constexpr auto and_then(F&& f) const& -> std::invoke_result_t<F, const T&> {
            using Result = std::invoke_result_t<F, const T&>;
            if (is_success()) {
                return std::invoke(std::forward<F>(f), value());
            }
            Result result;
            std::visit([&result]<typename ErrT>(const ErrT& err) {
                if constexpr (!std::same_as<std::decay_t<ErrT>, T>) {
                    result = ErrorTag{err};
                }
            }, value_);
            return result;
        }

        template <typename F>
            requires std::invocable<F, T&&>
        constexpr auto and_then(F&& f) && -> std::invoke_result_t<F, T&&> {
            using Result = std::invoke_result_t<F, T&&>;
            if (is_success()) {
                return std::invoke(std::forward<F>(f), std::move(*this).value());
            }
            Result result;
            std::visit([&result]<typename ErrT>(ErrT&& err) {
                if constexpr (!std::same_as<std::decay_t<ErrT>, T>) {
                    result = ErrorTag{std::forward<ErrT>(err)};
                }
            }, std::move(value_));
            return result;
        }

        // or_else - execute function if error
        template <typename F>
            requires std::invocable<F>
        constexpr auto or_else(F&& f) & -> std::invoke_result_t<F> {
            if (is_error()) {
                return std::invoke(std::forward<F>(f));
            }
            // Need to convert success to result type
            using Result = std::invoke_result_t<F>;
            return Result{value()};
        }

        template <typename F>
            requires std::invocable<F>
        constexpr auto or_else(F&& f) const& -> std::invoke_result_t<F> {
            if (is_error()) {
                return std::invoke(std::forward<F>(f));
            }
            using Result = std::invoke_result_t<F>;
            return Result{value()};
        }

        template <typename F>
            requires std::invocable<F>
        constexpr auto or_else(F&& f) && -> std::invoke_result_t<F> {
            if (is_error()) {
                return std::invoke(std::forward<F>(f));
            }
            using Result = std::invoke_result_t<F>;
            return Result{std::move(*this).value()};
        }

        // transform - map over success value
        template <typename F>
            requires std::invocable<F, T&>
        constexpr auto transform(F&& f) & {
            using U = std::invoke_result_t<F, T&>;
            if (is_success()) {
                return Outcome<U, Errors...>{std::invoke(std::forward<F>(f), value())};
            }
            // Propagate errors
            Outcome<U, Errors...> result;
            std::visit([&result]<typename ErrT>(ErrT&& err) {
                if constexpr (!std::same_as<std::decay_t<ErrT>, T>) {
                    result = ErrorTag{std::forward<ErrT>(err)};
                }
            }, value_);
            return result;
        }

        template <typename F>
            requires std::invocable<F, const T&>
        constexpr auto transform(F&& f) const& {
            using U = std::invoke_result_t<F, const T&>;
            if (is_success()) {
                return Outcome<U, Errors...>{std::invoke(std::forward<F>(f), value())};
            }
            Outcome<U, Errors...> result;
            std::visit([&result]<typename ErrT>(const ErrT& err) {
                if constexpr (!std::same_as<std::decay_t<ErrT>, T>) {
                    result = ErrorTag{err};
                }
            }, value_);
            return result;
        }

        template <typename F>
            requires std::invocable<F, T&&>
        constexpr auto transform(F&& f) && {
            using U = std::invoke_result_t<F, T&&>;
            if (is_success()) {
                return Outcome<U, Errors...>{std::invoke(std::forward<F>(f), std::move(*this).value())};
            }
            Outcome<U, Errors...> result;
            std::visit([&result]<typename ErrT>(ErrT&& err) {
                if constexpr (!std::same_as<std::decay_t<ErrT>, T>) {
                    result = ErrorTag{std::forward<ErrT>(err)};
                }
            }, std::move(value_));
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


    // Void specialization
    template <typename... Errors>
    class Outcome<void, Errors...> {
    public:
        using value_type  = void;
        using error_types = std::variant<Errors...>;

        // Default constructor - represents success
        constexpr Outcome() noexcept
            : value_(std::monostate{}) {
        }

        // Construct from monostate (success)
        constexpr explicit(false) Outcome(std::monostate) noexcept
            : value_(std::monostate{}) {
        }

        // Implicit construction from ErrorTag
        template <OneOf<Errors...> E>
        constexpr explicit(false) Outcome(ErrorTag<E>&& tag)
            noexcept(std::is_nothrow_move_constructible_v<E>)
            : value_(std::move(tag.error)) {
        }

        // Direct error construction
        template <OneOf<Errors...> E>
        static constexpr Outcome error(E&& e) {
            Outcome result;
            result.value_ = std::forward<E>(e);
            return result;
        }

        // Success factory
        static constexpr Outcome success() noexcept {
            return Outcome{};
        }

        [[nodiscard]] constexpr bool is_success() const noexcept {
            return std::holds_alternative<std::monostate>(value_);
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

        // Assert success (for void, we can't return a value)
        constexpr void ensure_success() const {
            if (!is_success()) {
                throw std::bad_variant_access{};
            }
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

        // Monadic operations - and_then
        template <typename F>
            requires std::invocable<F>
        constexpr auto and_then(F&& f) const& -> std::invoke_result_t<F> {
            using Result = std::invoke_result_t<F>;
            if (is_success()) {
                return std::invoke(std::forward<F>(f));
            }
            // Propagate error
            Result result;
            std::visit([&result]<typename ErrT>(const ErrT& err) {
                if constexpr (!std::same_as<std::decay_t<ErrT>, std::monostate>) {
                    result = ErrorTag{err};
                }
            }, value_);
            return result;
        }

        template <typename F>
            requires std::invocable<F>
        constexpr auto and_then(F&& f) && -> std::invoke_result_t<F> {
            using Result = std::invoke_result_t<F>;
            if (is_success()) {
                return std::invoke(std::forward<F>(f));
            }
            Result result;
            std::visit([&result]<typename ErrT>(ErrT&& err) {
                if constexpr (!std::same_as<std::decay_t<ErrT>, std::monostate>) {
                    result = ErrorTag{std::forward<ErrT>(err)};
                }
            }, std::move(value_));
            return result;
        }

        // or_else
        template <typename F>
            requires std::invocable<F>
        constexpr auto or_else(F&& f) const& -> std::invoke_result_t<F> {
            if (is_error()) {
                return std::invoke(std::forward<F>(f));
            }
            using Result = std::invoke_result_t<F>;
            return Result{};
        }

        template <typename F>
            requires std::invocable<F>
        constexpr auto or_else(F&& f) && -> std::invoke_result_t<F> {
            if (is_error()) {
                return std::invoke(std::forward<F>(f));
            }
            using Result = std::invoke_result_t<F>;
            return Result{};
        }

        // transform - converts void to non-void outcome
        template <typename F>
            requires std::invocable<F>
        constexpr auto transform(F&& f) const& {
            using U = std::invoke_result_t<F>;
            if (is_success()) {
                return Outcome<U, Errors...>{std::invoke(std::forward<F>(f))};
            }
            // Propagate errors
            Outcome<U, Errors...> result;
            std::visit([&result]<typename ErrT>(const ErrT& err) {
                if constexpr (!std::same_as<std::decay_t<ErrT>, std::monostate>) {
                    result = ErrorTag{err};
                }
            }, value_);
            return result;
        }

        template <typename F>
            requires std::invocable<F>
        constexpr auto transform(F&& f) && {
            using U = std::invoke_result_t<F>;
            if (is_success()) {
                return Outcome<U, Errors...>{std::invoke(std::forward<F>(f))};
            }
            Outcome<U, Errors...> result;
            std::visit([&result]<typename ErrT>(ErrT&& err) {
                if constexpr (!std::same_as<std::decay_t<ErrT>, std::monostate>) {
                    result = ErrorTag{std::forward<ErrT>(err)};
                }
            }, std::move(value_));
            return result;
        }

        // Visit pattern
        template <typename... Fs>
        constexpr decltype(auto) visit(Fs&&... fs) const& {
            return std::visit(overloaded{std::forward<Fs>(fs)...}, value_);
        }

        template <typename... Fs>
        constexpr decltype(auto) visit(Fs&&... fs) && {
            return std::visit(overloaded{std::forward<Fs>(fs)...}, std::move(value_));
        }

    private:
        std::variant<std::monostate, Errors...> value_;
    };

    // Utility to combine multiple outcomes
    // Returns Outcome<std::tuple<Values...>, Errors...> or Outcome<void, Errors...>
    template <typename T, typename... Errors, typename... Rest>
    constexpr auto combine_outcomes(const Outcome<T, Errors...>& first, const Rest&... rest) {
        // Check if all are successful
        if (!first.is_success() || (!rest.is_success() || ...)) {
            // Return first error found
            if (!first.is_success()) {
                if constexpr (std::same_as<T, void>) {
                    return first;
                } else {
                    Outcome<void, Errors...> result;
                    first.visit([&result]<typename ErrT>(const ErrT& err) {
                        if constexpr (!std::same_as<std::decay_t<ErrT>, T>) {
                            result = ErrorTag{err};
                        }
                    });
                    return result;
                }
            }

            // Check rest for first error
            Outcome<void, Errors...> result;
            auto check_error = [&result]<typename OutcomeT>(const OutcomeT& outcome) {
                if (!outcome.is_success() && result.is_success()) {
                    outcome.visit([&result]<typename ErrT>(const ErrT& err) {
                        using OutcomeType = std::decay_t<OutcomeT>;
                        using ValueType = OutcomeType::value_type;
                        if constexpr (!std::same_as<std::decay_t<ErrT>, ValueType> &&
                                      !std::same_as<std::decay_t<ErrT>, std::monostate>) {
                            result = ErrorTag{err};
                        }
                    });
                }
            };
            (check_error(rest), ...);
            return result;
        }

        // All successful - combine values
        if constexpr (std::same_as<T, void> && (std::same_as<typename std::decay_t<Rest>::value_type, void> && ...)) {
            // All void outcomes - return void success
            return Outcome<void, Errors...>{};
        } else if constexpr (std::same_as<T, void>) {
            // Mixed void and non-void - return tuple of non-void values
            return Outcome<std::tuple<typename std::decay_t<Rest>::value_type...>, Errors...>{
                std::make_tuple(rest.value()...)
            };
        } else if constexpr ((std::same_as<typename std::decay_t<Rest>::value_type, void> && ...)) {
            // First is non-void, rest are void
            return Outcome<T, Errors...>{first.value()};
        } else {
            // All non-void - return tuple
            return Outcome<std::tuple<T, typename std::decay_t<Rest>::value_type...>, Errors...>{
                std::make_tuple(first.value(), rest.value()...)
            };
        }
    }

}  // namespace demiplane::gears