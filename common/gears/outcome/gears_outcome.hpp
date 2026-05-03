#pragma once

#include <exception>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "gears_concepts.hpp"
#include "gears_templates.hpp"

namespace demiplane::gears {
    /**
     * @brief Exception thrown by Outcome accessors when called on an alternative
     *        that is not currently held.
     *
     * Trigger sites:
     *  - value() / operator*() / operator-> on an Outcome in error state
     *  - error<E>() with an E that is not the active error type
     *  - ensure_success() on Outcome<void, ...> in error state
     *
     * The optional message is a `const char*` literal — no allocation, safe to
     * throw on hot paths and from constexpr-evaluated code.
     */
    class [[nodiscard]] BadOutcomeAccess final : public std::exception {
    public:
        BadOutcomeAccess() noexcept = default;
        explicit BadOutcomeAccess(const char* msg) noexcept
            : msg_{msg} {
        }

        [[nodiscard]] const char* what() const noexcept override {
            return msg_ != nullptr ? msg_ : "demiplane::gears::BadOutcomeAccess";
        }

    private:
        const char* msg_ = nullptr;
    };

    /// Disambiguation tag — wraps an error value so Outcome<T, Es...> can be
    /// implicitly constructed from Err(...) even when T shares the type.
    template <typename E>
    struct [[nodiscard]] ErrorTag {
        E error;
        constexpr explicit ErrorTag(E e)
            : error{std::move(e)} {
        }
    };

    /// Build an ErrorTag with the decayed type of e deduced.
    template <typename E>
    constexpr ErrorTag<std::decay_t<E>> err(E&& e) {
        return ErrorTag<std::decay_t<E>>{std::forward<E>(e)};
    }

    /// Disambiguation tag — symmetric to ErrorTag, signaling a success value.
    template <typename T>
    struct [[nodiscard]] SuccessTag {
        T value;
        constexpr explicit SuccessTag(T v)
            : value{std::move(v)} {
        }
    };

    /// Build a SuccessTag with the decayed type of v deduced.
    template <typename T>
    constexpr SuccessTag<std::decay_t<T>> ok(T&& v) {
        return SuccessTag<std::decay_t<T>>{std::forward<T>(v)};
    }

    /// Tag for a void-success outcome (Outcome<void, Es...>).
    constexpr std::monostate ok() {
        return {};
    }

    namespace detail {
        /// Build Outcome<U, Es...> from (U, std::variant<Es...>).
        template <typename U, typename ErrorVariant>
        struct rebuild_outcome;

        template <typename U, typename... Es>
        struct rebuild_outcome<U, std::variant<Es...>> {
            using type = Outcome<U, Es...>;
        };

        template <typename U, typename ErrorVariant>
        using rebuild_outcome_t = rebuild_outcome<U, ErrorVariant>::type;

        /// Fold merge_variants_t over a list of variants.
        template <typename...>
        struct merge_all;

        template <>
        struct merge_all<> {
            using type = std::variant<>;
        };

        template <typename V>
        struct merge_all<V> {
            using type = V;
        };

        template <typename V1, typename V2, typename... Rest>
        struct merge_all<V1, V2, Rest...> {
            using type = merge_all<merge_variants_t<V1, V2>, Rest...>::type;
        };

        template <typename... Vs>
        using merge_all_t = merge_all<Vs...>::type;

        /// Convert a narrower Outcome into a wider one whose variant
        /// alternatives are a superset. Used by widened monadic ops and
        /// chain_outcomes. Implemented via the public visit.
        template <typename Wider, typename Narrower>
        constexpr Wider widen_outcome(Narrower&& src) {
            using NarrowerValueType = std::remove_cvref_t<Narrower>::value_type;
            return std::forward<Narrower>(src).visit([&]<typename A>(A&& alt) -> Wider {
                if constexpr (std::same_as<std::remove_cvref_t<A>, std::monostate>) {
                    return Wider{};
                } else if constexpr (!std::same_as<NarrowerValueType, void> &&
                                     std::same_as<std::remove_cvref_t<A>, NarrowerValueType>) {
                    return Wider{SuccessTag<NarrowerValueType>{std::forward<A>(alt)}};
                } else {
                    return Wider{ErrorTag{std::forward<A>(alt)}};
                }
            });
        }

        /// Recursively walk outcomes left-to-right and produce the first error,
        /// already widened into Result. Caller has already verified at least
        /// one input is in the error state, so falling off the end is unreachable.
        template <typename Result, typename First, typename... Rest>
        constexpr Result extract_first_error(First&& first, Rest&&... rest) {
            if (!first.is_success()) {
                return std::forward<First>(first).visit([&]<typename A>(A&& alt) -> Result {
                    using AD = std::remove_cvref_t<A>;
                    using FV = std::remove_cvref_t<First>::value_type;
                    // monostate (void-success) and the value alternative both indicate
                    // success — unreachable here because is_success() returned false.
                    if constexpr (std::same_as<AD, std::monostate> ||
                                  (!std::same_as<FV, void> && std::same_as<AD, FV>)) {
                        // Unreachable: this visit branch only runs when the
                        // outer is_success()/is_error() check selected the
                        // error path. Reaching here means the held alternative
                        // changed unexpectedly between the check and the visit.
                        throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
                    } else {
                        return Result{ErrorTag{std::forward<A>(alt)}};
                    }
                });
            }
            if constexpr (sizeof...(Rest) == 0) {
                // Unreachable: caller guarantees at least one input is in the
                // error state. Reaching here means an outcome's state changed
                // unexpectedly between the caller's check and this walk.
                throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
            } else {
                return extract_first_error<Result>(std::forward<Rest>(rest)...);
            }
        }

        /// Type-list helpers for chain_outcomes's value-tuple synthesis.
        /// Lifted unchanged from the previous combine_outcomes machinery.
        template <typename... Ts>
        struct NonVoidTypes;

        template <>
        struct NonVoidTypes<> {
            using tuple_type = std::tuple<>;
        };

        template <typename T, typename... Rest>
        struct NonVoidTypes<T, Rest...> {
            using rest_tuple = NonVoidTypes<Rest...>::tuple_type;
            using tuple_type =
                std::conditional_t<std::same_as<T, void>,
                                   rest_tuple,
                                   decltype(std::tuple_cat(std::declval<std::tuple<T>>(), std::declval<rest_tuple>()))>;
        };

        template <typename Tuple>
        struct SimplifyTuple {
            using type = Tuple;
        };

        template <>
        struct SimplifyTuple<std::tuple<>> {
            using type = void;
        };

        template <typename T>
        struct SimplifyTuple<std::tuple<T>> {
            using type = T;
        };
    }  // namespace detail


    /**
     * @brief Sum type holding either a value of T or one of the listed Errors.
     *
     * Hot-path result type used in place of exceptions. Provides bool-conversion
     * for branching, value()/error<E>() accessors, and monadic combinators
     * (and_then, or_else, transform, visit) that propagate the error
     * variant unchanged when the outcome is in an error state. and_then
     * additionally widens the error variant to the union of *this's errors
     * and the callable's errors, so chains can compose pieces with disjoint
     * error sets without manually pre-declaring the widest variant up front.
     */
    template <typename T, typename... Errors>
    class [[nodiscard]] Outcome {
        static_assert(all_unique_v<Errors...>, "Outcome<T, Errors...>: error types must be distinct");
        static_assert((!std::same_as<T, Errors> && ...), "Outcome<T, Errors...>: T must differ from every error type");

    public:
        using value_type  = T;
        using error_types = std::variant<Errors...>;

        // Default constructor (requires T to be default constructible)
        constexpr Outcome() noexcept(std::is_nothrow_constructible_v<T>)
            requires std::default_initializable<T>
            : value_{T{}} {
        }

        // Implicit construction from value - exclude error types and tag wrappers
        template <typename U = std::remove_cv_t<T>>
            requires std::constructible_from<T, U> && (!OneOf<std::decay_t<U>, Errors...>) &&
                     (!is_specialization_of_v<std::decay_t<U>, ErrorTag>) &&
                     (!is_specialization_of_v<std::decay_t<U>, SuccessTag>)
        constexpr explicit(!std::is_convertible_v<U, T>)
            Outcome(U&& value) noexcept(std::is_nothrow_constructible_v<T, U>)
            : value_{std::forward<U>(value)} {
        }

        // Implicit construction from SuccessTag
        template <typename U>
            requires std::constructible_from<T, U>
        constexpr explicit(!std::is_convertible_v<U, T>)
            Outcome(SuccessTag<U>&& tag) noexcept(std::is_nothrow_constructible_v<T, U>)
            : value_{std::move(tag.value)} {
        }

        // Implicit construction from ErrorTag
        template <OneOf<Errors...> E>
        constexpr explicit(false) Outcome(ErrorTag<E>&& tag) noexcept(std::is_nothrow_move_constructible_v<E>)
            : value_{std::move(tag.error)} {
        }

        // Direct error construction — routes through the ErrorTag ctor so
        // T's default constructor is never required (Outcome::error works
        // for non-default-constructible value types) and the variant cannot
        // be left valueless_by_exception by an assignment partway through.
        template <OneOf<Errors...> E>
        static constexpr Outcome error(E&& e) noexcept(std::is_nothrow_move_constructible_v<std::decay_t<E>>) {
            return Outcome{ErrorTag<std::decay_t<E>>{std::forward<E>(e)}};
        }

        // Success factory — accept lvalues and rvalues
        template <typename U = T>
            requires std::constructible_from<T, U>
        static constexpr Outcome success(U&& value) noexcept(std::is_nothrow_constructible_v<T, U>) {
            return Outcome{std::forward<U>(value)};
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

        /// Access the success value.
        /// @throws BadOutcomeAccess if the Outcome is in an error state.
        [[nodiscard]] constexpr const T& value() const& {
            [[unlikely]] if (!is_success()) { throw BadOutcomeAccess{"Outcome::value() called on error state"}; }
            return std::get<T>(value_);
        }

        /// Access the success value.
        /// @throws BadOutcomeAccess if the Outcome is in an error state.
        [[nodiscard]] constexpr T& value() & {
            [[unlikely]] if (!is_success()) { throw BadOutcomeAccess{"Outcome::value() called on error state"}; }
            return std::get<T>(value_);
        }

        /// Access the success value (move-out).
        /// @throws BadOutcomeAccess if the Outcome is in an error state.
        [[nodiscard]] constexpr T&& value() && {
            [[unlikely]] if (!is_success()) { throw BadOutcomeAccess{"Outcome::value() called on error state"}; }
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
            [[likely]] if (is_success()) { return std::get<T>(value_); }
            return static_cast<T>(std::forward<U>(default_value));
        }

        template <class U = T>
        constexpr T value_or(U&& default_value) && {
            [[likely]] if (is_success()) { return std::move(std::get<T>(value_)); }
            return static_cast<T>(std::forward<U>(default_value));
        }

        /// Access the error of type `E`.
        /// @throws std::bad_variant_access if the active alternative is not `E`
        ///         (delegated to std::get; we don't wrap because the wrap would
        ///         add a redundant holds_alternative check on the hot path).
        template <OneOf<Errors...> E>
        [[nodiscard]] constexpr const E& error() const& {
            return std::get<E>(value_);
        }

        /// @copydoc error
        template <OneOf<Errors...> E>
        [[nodiscard]] constexpr E& error() & {
            return std::get<E>(value_);
        }

        /// @copydoc error
        template <OneOf<Errors...> E>
        [[nodiscard]] constexpr E&& error() && {
            return std::move(std::get<E>(value_));
        }

        /**
         * @brief If success, invoke f(value) and return its Outcome,
         *        widened to hold both *this's errors and the callable's errors.
         *        If error, propagate the existing error into the widened result.
         *
         * F must return an Outcome. The result type is
         * Outcome<F::value_type, union (Errors..., F::error_types...)>.
         */
        template <typename F>
            requires std::invocable<F, T&> && IsOutcome<std::invoke_result_t<F, T&>>
        constexpr auto and_then(F&& f) & {
            using FRes         = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
            using MergedErrors = detail::merge_all_t<error_types, typename FRes::error_types>;
            using Result       = detail::rebuild_outcome_t<typename FRes::value_type, MergedErrors>;
            [[likely]] if (is_success()) {
                if constexpr (std::same_as<FRes, Result>) {
                    return std::invoke(std::forward<F>(f), value());
                } else {
                    return detail::widen_outcome<Result>(std::invoke(std::forward<F>(f), value()));
                }
            }
            return std::visit(
                [&]<typename A>(A&& alt) -> Result {
                    if constexpr (std::same_as<std::remove_cvref_t<A>, T>) {
                        // Unreachable: this visit branch only runs when the
                        // outer is_success()/is_error() check selected the
                        // error path. Reaching here means the held alternative
                        // changed unexpectedly between the check and the visit.
                        throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
                    } else {
                        return Result{ErrorTag{std::forward<A>(alt)}};
                    }
                },
                value_);
        }

        template <typename F>
            requires std::invocable<F, const T&> && IsOutcome<std::invoke_result_t<F, const T&>>
        constexpr auto and_then(F&& f) const& {
            using FRes         = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
            using MergedErrors = detail::merge_all_t<error_types, typename FRes::error_types>;
            using Result       = detail::rebuild_outcome_t<typename FRes::value_type, MergedErrors>;
            [[likely]] if (is_success()) {
                if constexpr (std::same_as<FRes, Result>) {
                    return std::invoke(std::forward<F>(f), value());
                } else {
                    return detail::widen_outcome<Result>(std::invoke(std::forward<F>(f), value()));
                }
            }
            return std::visit(
                [&]<typename A>(const A& alt) -> Result {
                    if constexpr (std::same_as<std::remove_cvref_t<A>, T>) {
                        // Unreachable: this visit branch only runs when the
                        // outer is_success()/is_error() check selected the
                        // error path. Reaching here means the held alternative
                        // changed unexpectedly between the check and the visit.
                        throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
                    } else {
                        return Result{ErrorTag{alt}};
                    }
                },
                value_);
        }

        template <typename F>
            requires std::invocable<F, T&&> && IsOutcome<std::invoke_result_t<F, T&&>>
        constexpr auto and_then(F&& f) && {
            using FRes         = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
            using MergedErrors = detail::merge_all_t<error_types, typename FRes::error_types>;
            using Result       = detail::rebuild_outcome_t<typename FRes::value_type, MergedErrors>;
            [[likely]] if (is_success()) {
                if constexpr (std::same_as<FRes, Result>) {
                    return std::invoke(std::forward<F>(f), std::move(*this).value());
                } else {
                    return detail::widen_outcome<Result>(std::invoke(std::forward<F>(f), std::move(*this).value()));
                }
            }
            return std::visit(
                [&]<typename A>(A&& alt) -> Result {
                    if constexpr (std::same_as<std::remove_cvref_t<A>, T>) {
                        // Unreachable: this visit branch only runs when the
                        // outer is_success()/is_error() check selected the
                        // error path. Reaching here means the held alternative
                        // changed unexpectedly between the check and the visit.
                        throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
                    } else {
                        return Result{ErrorTag{std::forward<A>(alt)}};
                    }
                },
                std::move(value_));
        }

        /**
         * @brief If error, invoke f() and return its result; otherwise
         *        re-wrap the existing success value into the result type.
         *
         * Mirror image of and_then — recovery hook for error states.
         * The result type is whatever f() returns; the error variant
         * is *replaced* (not unioned), since recovery semantics dictate
         * that the prior error set is gone once f runs.
         */
        template <typename F>
            requires std::invocable<F> && IsOutcome<std::invoke_result_t<F>>
        constexpr std::invoke_result_t<F> or_else(F&& f) & {
            [[unlikely]] if (is_error()) { return std::invoke(std::forward<F>(f)); }
            using Result = std::invoke_result_t<F>;
            return Result{value()};
        }

        template <typename F>
            requires std::invocable<F> && IsOutcome<std::invoke_result_t<F>>
        constexpr std::invoke_result_t<F> or_else(F&& f) const& {
            [[unlikely]] if (is_error()) { return std::invoke(std::forward<F>(f)); }
            using Result = std::invoke_result_t<F>;
            return Result{value()};
        }

        template <typename F>
            requires std::invocable<F> && IsOutcome<std::invoke_result_t<F>>
        constexpr std::invoke_result_t<F> or_else(F&& f) && {
            [[unlikely]] if (is_error()) { return std::invoke(std::forward<F>(f)); }
            using Result = std::invoke_result_t<F>;
            return Result{std::move(*this).value()};
        }

        /**
         * @brief Apply f to the success value, returning an
         *        Outcome<invoke_result_t<F>, Errors...>. Error states are
         *        forwarded through unchanged; the error variant is preserved.
         *
         * Unlike and_then, f returns a plain value, not an Outcome,
         * so there is nothing to widen.
         */
        template <typename F>
            requires std::invocable<F, T&>
        constexpr auto transform(F&& f) & {
            using Result = Outcome<std::invoke_result_t<F, T&>, Errors...>;
            [[likely]] if (is_success()) {
                if constexpr (std::same_as<std::invoke_result_t<F, T&>, void>) {
                    std::invoke(std::forward<F>(f), value());
                    return Result{};
                } else {
                    return Result{std::invoke(std::forward<F>(f), value())};
                }
            }
            return std::visit(
                [&]<typename A>(A&& alt) -> Result {
                    if constexpr (std::same_as<std::remove_cvref_t<A>, T>) {
                        // Unreachable: this visit branch only runs when the
                        // outer is_success()/is_error() check selected the
                        // error path. Reaching here means the held alternative
                        // changed unexpectedly between the check and the visit.
                        throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
                    } else {
                        return Result{ErrorTag{std::forward<A>(alt)}};
                    }
                },
                value_);
        }

        template <typename F>
            requires std::invocable<F, const T&>
        constexpr auto transform(F&& f) const& {
            using Result = Outcome<std::invoke_result_t<F, const T&>, Errors...>;
            [[likely]] if (is_success()) {
                if constexpr (std::same_as<std::invoke_result_t<F, const T&>, void>) {
                    std::invoke(std::forward<F>(f), value());
                    return Result{};
                } else {
                    return Result{std::invoke(std::forward<F>(f), value())};
                }
            }
            return std::visit(
                [&]<typename A>(const A& alt) -> Result {
                    if constexpr (std::same_as<std::remove_cvref_t<A>, T>) {
                        // Unreachable: this visit branch only runs when the
                        // outer is_success()/is_error() check selected the
                        // error path. Reaching here means the held alternative
                        // changed unexpectedly between the check and the visit.
                        throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
                    } else {
                        return Result{ErrorTag{alt}};
                    }
                },
                value_);
        }

        template <typename F>
            requires std::invocable<F, T&&>
        constexpr auto transform(F&& f) && {
            using Result = Outcome<std::invoke_result_t<F, T&&>, Errors...>;
            [[likely]] if (is_success()) {
                if constexpr (std::same_as<std::invoke_result_t<F, T&&>, void>) {
                    std::invoke(std::forward<F>(f), std::move(*this).value());
                    return Result{};
                } else {
                    return Result{std::invoke(std::forward<F>(f), std::move(*this).value())};
                }
            }
            return std::visit(
                [&]<typename A>(A&& alt) -> Result {
                    if constexpr (std::same_as<std::remove_cvref_t<A>, T>) {
                        // Unreachable: this visit branch only runs when the
                        // outer is_success()/is_error() check selected the
                        // error path. Reaching here means the held alternative
                        // changed unexpectedly between the check and the visit.
                        throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
                    } else {
                        return Result{ErrorTag{std::forward<A>(alt)}};
                    }
                },
                std::move(value_));
        }

        /**
         * @brief Exhaustively match the held alternative against the supplied
         *        callables. Equivalent to std::visit(overloaded{fs...}, ...).
         */
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

        // Equality / ordering — defaulted; participates only when alternatives are comparable.
        [[nodiscard]] friend constexpr bool operator==(const Outcome&, const Outcome&) = default;

        constexpr void swap(Outcome& other) noexcept(std::is_nothrow_swappable_v<std::variant<T, Errors...>>) {
            std::swap(value_, other.value_);
        }

        friend constexpr void swap(Outcome& a,
                                   Outcome& b) noexcept(std::is_nothrow_swappable_v<std::variant<T, Errors...>>) {
            a.swap(b);
        }

    private:
        // TODO: reimplement std::variant suggested gears::variant
        std::variant<T, Errors...> value_;
    };


    /// void-value specialization — success is signaled by std::monostate,
    /// errors are stored exactly as in the primary template.
    template <typename... Errors>
    class [[nodiscard]] Outcome<void, Errors...> {
        static_assert(all_unique_v<Errors...>, "Outcome<void, Errors...>: error types must be distinct");

    public:
        using value_type  = void;
        using error_types = std::variant<Errors...>;

        // Default constructor - represents success
        constexpr Outcome() noexcept
            : value_{std::monostate{}} {
        }

        // Construct from monostate (success)
        constexpr explicit(false) Outcome(std::monostate) noexcept
            : value_{std::monostate{}} {
        }

        // Implicit construction from ErrorTag
        template <OneOf<Errors...> E>
        constexpr explicit(false) Outcome(ErrorTag<E>&& tag) noexcept(std::is_nothrow_move_constructible_v<E>)
            : value_{std::move(tag.error)} {
        }

        // Direct error construction — routes through the ErrorTag ctor so
        // the variant cannot be left valueless_by_exception by an assignment
        // partway through.
        template <OneOf<Errors...> E>
        static constexpr Outcome error(E&& e) noexcept(std::is_nothrow_move_constructible_v<std::decay_t<E>>) {
            return Outcome{ErrorTag<std::decay_t<E>>{std::forward<E>(e)}};
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

        /// Assert success — there is no value to return for `Outcome<void>`.
        /// @throws BadOutcomeAccess if the Outcome is in an error state.
        constexpr void ensure_success() const {
            [[unlikely]] if (!is_success()) {
                throw BadOutcomeAccess{"Outcome<void>::ensure_success() called on error state"};
            }
        }

        /// Access the error of type `E`.
        /// @throws std::bad_variant_access if the active alternative is not `E`
        ///         (delegated to std::get; we don't wrap because the wrap would
        ///         add a redundant holds_alternative check on the hot path).
        template <OneOf<Errors...> E>
        [[nodiscard]] constexpr const E& error() const& {
            return std::get<E>(value_);
        }

        /// @copydoc error
        template <OneOf<Errors...> E>
        [[nodiscard]] constexpr E& error() & {
            return std::get<E>(value_);
        }

        /// @copydoc error
        template <OneOf<Errors...> E>
        [[nodiscard]] constexpr E&& error() && {
            return std::move(std::get<E>(value_));
        }

        // and_then with widening
        template <typename F>
            requires std::invocable<F> && IsOutcome<std::invoke_result_t<F>>
        constexpr auto and_then(F&& f) const& {
            using FRes         = std::remove_cvref_t<std::invoke_result_t<F>>;
            using MergedErrors = detail::merge_all_t<error_types, typename FRes::error_types>;
            using Result       = detail::rebuild_outcome_t<typename FRes::value_type, MergedErrors>;
            [[likely]] if (is_success()) {
                if constexpr (std::same_as<FRes, Result>) {
                    return std::invoke(std::forward<F>(f));
                } else {
                    return detail::widen_outcome<Result>(std::invoke(std::forward<F>(f)));
                }
            }
            return std::visit(
                [&]<typename A>(const A& alt) -> Result {
                    if constexpr (std::same_as<std::remove_cvref_t<A>, std::monostate>) {
                        // Unreachable: this visit branch only runs when the
                        // outer is_success()/is_error() check selected the
                        // error path. Reaching here means the held alternative
                        // changed unexpectedly between the check and the visit.
                        throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
                    } else {
                        return Result{ErrorTag{alt}};
                    }
                },
                value_);
        }

        template <typename F>
            requires std::invocable<F> && IsOutcome<std::invoke_result_t<F>>
        constexpr auto and_then(F&& f) && {
            using FRes         = std::remove_cvref_t<std::invoke_result_t<F>>;
            using MergedErrors = detail::merge_all_t<error_types, typename FRes::error_types>;
            using Result       = detail::rebuild_outcome_t<typename FRes::value_type, MergedErrors>;
            [[likely]] if (is_success()) {
                if constexpr (std::same_as<FRes, Result>) {
                    return std::invoke(std::forward<F>(f));
                } else {
                    return detail::widen_outcome<Result>(std::invoke(std::forward<F>(f)));
                }
            }
            return std::visit(
                [&]<typename A>(A&& alt) -> Result {
                    if constexpr (std::same_as<std::remove_cvref_t<A>, std::monostate>) {
                        // Unreachable: this visit branch only runs when the
                        // outer is_success()/is_error() check selected the
                        // error path. Reaching here means the held alternative
                        // changed unexpectedly between the check and the visit.
                        throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
                    } else {
                        return Result{ErrorTag{std::forward<A>(alt)}};
                    }
                },
                std::move(value_));
        }

        // or_else — replaces error variant with f()'s outcome type.
        template <typename F>
            requires std::invocable<F> && IsOutcome<std::invoke_result_t<F>>
        constexpr std::invoke_result_t<F> or_else(F&& f) const& {
            [[unlikely]] if (is_error()) { return std::invoke(std::forward<F>(f)); }
            using Result = std::invoke_result_t<F>;
            return Result{};
        }

        template <typename F>
            requires std::invocable<F> && IsOutcome<std::invoke_result_t<F>>
        constexpr std::invoke_result_t<F> or_else(F&& f) && {
            [[unlikely]] if (is_error()) { return std::invoke(std::forward<F>(f)); }
            using Result = std::invoke_result_t<F>;
            return Result{};
        }

        // transform - converts void to non-void outcome; errors preserved.
        template <typename F>
            requires std::invocable<F>
        constexpr auto transform(F&& f) const& {
            using Result = Outcome<std::invoke_result_t<F>, Errors...>;
            [[likely]] if (is_success()) {
                if constexpr (std::same_as<std::invoke_result_t<F>, void>) {
                    std::invoke(std::forward<F>(f));
                    return Result{};
                } else {
                    return Result{std::invoke(std::forward<F>(f))};
                }
            }
            return std::visit(
                [&]<typename A>(const A& alt) -> Result {
                    if constexpr (std::same_as<std::remove_cvref_t<A>, std::monostate>) {
                        // Unreachable: this visit branch only runs when the
                        // outer is_success()/is_error() check selected the
                        // error path. Reaching here means the held alternative
                        // changed unexpectedly between the check and the visit.
                        throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
                    } else {
                        return Result{ErrorTag{alt}};
                    }
                },
                value_);
        }

        template <typename F>
            requires std::invocable<F>
        constexpr auto transform(F&& f) && {
            using Result = Outcome<std::invoke_result_t<F>, Errors...>;
            [[likely]] if (is_success()) {
                if constexpr (std::same_as<std::invoke_result_t<F>, void>) {
                    std::invoke(std::forward<F>(f));
                    return Outcome{};
                } else {
                    return Result{std::invoke(std::forward<F>(f))};
                }
            }
            return std::visit(
                [&]<typename A>(A&& alt) -> Result {
                    if constexpr (std::same_as<std::remove_cvref_t<A>, std::monostate>) {
                        // Unreachable: this visit branch only runs when the
                        // outer is_success()/is_error() check selected the
                        // error path. Reaching here means the held alternative
                        // changed unexpectedly between the check and the visit.
                        throw BadOutcomeAccess{"Outcome state changed unexpectedly"};
                    } else {
                        return Result{ErrorTag{std::forward<A>(alt)}};
                    }
                },
                std::move(value_));
        }

        // Visit pattern
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

        [[nodiscard]] friend constexpr bool operator==(const Outcome&, const Outcome&) = default;

        constexpr void
        swap(Outcome& other) noexcept(std::is_nothrow_swappable_v<std::variant<std::monostate, Errors...>>) {
            std::swap(value_, other.value_);
        }

        friend constexpr void
        swap(Outcome& a, Outcome& b) noexcept(std::is_nothrow_swappable_v<std::variant<std::monostate, Errors...>>) {
            a.swap(b);
        }

    private:
        // TODO: reimplement std::variant suggested gears::variant
        std::variant<std::monostate, Errors...> value_;
    };


    /**
     * @brief Combine several Outcomes left-to-right; short-circuit on the
     *        first error encountered.
     *
     * On all-success, returns an Outcome whose value is the combined success
     * payload: a tuple of every non-void value (or the bare value if exactly
     * one outcome carries one, or void if none does). The error variant is the
     * **union** of every input's error variant — so chain_outcomes over inputs
     * with disjoint error sets produces a result that can hold any of them.
     *
     * Short-circuit semantics: evaluation stops at the first input found to be
     * in the error state; subsequent inputs are not inspected. Identical to a
     * manual if (!a) return Err(a.error()); if (!b) return Err(b.error()); ...
     * chain — later errors are dropped, the same way they would be in imperative
     * early-return code.
     */
    template <typename... Outcomes>
        requires(sizeof...(Outcomes) > 0)
    constexpr auto chain_outcomes(Outcomes&&... outcomes) {
        using NonVoidTuple = detail::NonVoidTypes<typename std::remove_cvref_t<Outcomes>::value_type...>::tuple_type;
        using ValueType    = detail::SimplifyTuple<NonVoidTuple>::type;
        using MergedErrors = detail::merge_all_t<typename std::remove_cvref_t<Outcomes>::error_types...>;
        using Result       = detail::rebuild_outcome_t<ValueType, MergedErrors>;

        [[unlikely]] if ((!outcomes.is_success() || ...)) {
            return detail::extract_first_error<Result>(std::forward<Outcomes>(outcomes)...);
        }

        if constexpr (std::same_as<ValueType, void>) {
            return Result{};
        } else {
            auto values = std::tuple_cat([&]<typename O>(O&& outcome) {
                if constexpr (std::same_as<typename std::remove_cvref_t<O>::value_type, void>) {
                    return std::tuple();
                } else {
                    return std::make_tuple(std::forward<O>(outcome).value());
                }
            }(std::forward<Outcomes>(outcomes))...);

            if constexpr (std::tuple_size_v<decltype(values)> == 1) {
                return Result{std::get<0>(std::move(values))};
            } else {
                return Result{std::move(values)};
            }
        }
    }

}  // namespace demiplane::gears
