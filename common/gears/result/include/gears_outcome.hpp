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

    // Helper to build non-void value tuple at compile time
    namespace detail {
        // Collect only non-void types
        template<typename... Ts>
        struct NonVoidTypes;

        template<>
        struct NonVoidTypes<> {
            using tuple_type = std::tuple<>;
        };

        template<typename T, typename... Rest>
        struct NonVoidTypes<T, Rest...> {
            using rest_tuple = NonVoidTypes<Rest...>::tuple_type;
            using tuple_type = std::conditional_t<
                std::same_as<T, void>,
                rest_tuple,  // Skip void
                decltype(std::tuple_cat(std::declval<std::tuple<T>>(), std::declval<rest_tuple>()))
            >;
        };

        // Convert tuple<> to void, tuple<T> to T, tuple<T, U, ...> stays as tuple
        template<typename Tuple>
        struct SimplifyTuple {
            using type = Tuple;
        };

        template<>
        struct SimplifyTuple<std::tuple<>> {
            using type = void;
        };

        template<typename T>
        struct SimplifyTuple<std::tuple<T>> {
            using type = T;
        };
    }

    // Combine multiple outcomes - returns first error or combined success values
    template <typename... Outcomes>
        requires (sizeof...(Outcomes) > 0)
    constexpr auto combine_outcomes(const Outcomes&... outcomes) {
        // Compute result value type
        using NonVoidTuple = detail::NonVoidTypes<typename std::decay_t<Outcomes>::value_type...>::tuple_type;
        using ValueType = detail::SimplifyTuple<NonVoidTuple>::type;

        // Get error type from first outcome
        using FirstOutcome = std::tuple_element_t<0, std::tuple<std::decay_t<Outcomes>...>>;
        using ErrorVariant = FirstOutcome::error_types;

        // Extract first error type from variant
        using ErrorType = std::variant_alternative_t<0, ErrorVariant>;

        using ResultType = Outcome<ValueType, ErrorType>;

        // Check for any errors
        if ((!outcomes.is_success() || ...)) {
            ResultType result;
            auto check = [&]<typename OutcomeT>(const OutcomeT& outcome) {
                if (!outcome.is_success() && result.is_success()) {
                    outcome.visit([&]<typename ErrT>(const ErrT& err) {
                        using OutT = std::decay_t<OutcomeT>;
                        using ValT = OutT::value_type;
                        if constexpr (!std::same_as<std::decay_t<ErrT>, ValT> &&
                                      !std::same_as<std::decay_t<ErrT>, std::monostate>) {
                            result = ErrorTag{err};
                        }
                    });
                }
            };
            (check(outcomes), ...);
            return result;
        }

        // All successful - build result
        if constexpr (std::same_as<ValueType, void>) {
            return ResultType{};
        } else {
            // Collect non-void values
            auto values = std::tuple_cat(
                [&]<typename T>(const T& outcome) {
                    if constexpr (std::same_as<typename std::decay_t<T>::value_type, void>) {
                        return std::tuple<>();
                    } else {
                        return std::make_tuple(outcome.value());
                    }
                }(outcomes)...
            );

            if constexpr (std::tuple_size_v<decltype(values)> == 1) {
                return ResultType{std::get<0>(values)};
            } else {
                return ResultType{values};
            }
        }
    }

}  // namespace demiplane::gears