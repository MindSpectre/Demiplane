#pragma once

#include <type_traits>
#include <variant>
#include <vector>
namespace demiplane::gears {
    template <typename>
    struct always_false : std::false_type {};

    template <typename T>
    inline constexpr bool always_false_v = always_false<T>::value;

    template <typename>
    struct always_true : std::true_type {};

    template <typename T>
    inline constexpr bool always_true_v = always_true<T>::value;

    /**
     * @brief True iff `T` is a specialization of the class template `Template`.
     *
     * Example: `is_specialization_of_v<std::vector<int>, std::vector>` is `true`.
     */
    template <typename, template <class...> class>
    struct is_specialization_of : std::false_type {};

    template <template <class...> class Template, typename... Args>
    struct is_specialization_of<Template<Args...>, Template> : std::true_type {};

    template <typename ClassWithTemplate, template <class...> class BaseClass>
    inline constexpr bool is_specialization_of_v = is_specialization_of<ClassWithTemplate, BaseClass>::value;


    /**
     * @brief True iff `Derived` inherits from some specialization of `BaseTemplate`,
     *        but is not itself such a specialization.
     *
     * Useful for catching CRTP-style descendants while excluding the base
     * template itself from the match.
     */
    template <typename Derived, template <typename...> class BaseTemplate>
    struct derived_from_specialization_of {
    private:
        // First, check if Derived is itself a specialization of BaseTemplate
        template <typename T>
        static constexpr bool is_specialization() {
            return is_specialization_of_v<T, BaseTemplate>;
        }

        // Then check for inheritance from any specialization
        template <typename... Args>
        static std::true_type test_inheritance(const BaseTemplate<Args...>*) {
            return std::true_type{};
        }

        static std::false_type test_inheritance(...) {
            return std::false_type{};
        }

        // Check if it's convertible to some specialization
        static constexpr bool is_convertible_to_specialization =
            decltype(test_inheritance(std::declval<Derived*>()))::value;

    public:
        // True only if it's convertible to a specialization AND not itself a specialization
        static constexpr bool value =
            is_convertible_to_specialization && !is_specialization<std::remove_cv_t<Derived>>();
    };

    // Concept for cleaner use
    template <typename Derived, template <typename...> class BaseTemplate>
    inline constexpr bool derived_from_specialization_of_v =
        derived_from_specialization_of<Derived, BaseTemplate>::value;


    template <class...>
    inline constexpr bool dependent_false_v = false;


    template <typename>
    struct is_vector : std::false_type {};

    template <typename T, typename Alloc>
    struct is_vector<std::vector<T, Alloc>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_vector_v = is_vector<T>::value;


    // ── tiny type-list ────────────────────────────────────────────────
    template <class...>
    struct type_list {};

    /// Wrapper around `std::get<T>(tuple)` — extract the element of type `T`.
    template <class T, class Tuple>
    decltype(auto) pick(Tuple&& t) {
        return std::get<T>(std::forward<Tuple>(t));
    }

    /**
     * @brief Project a `type_list<Ts...>` against a tuple, returning a tuple
     *        of references (or values) to the matching elements in list order.
     */
    template <class List, class Tuple>
    struct make_arg_tuple;

    template <class... Ts, class Tuple>
    struct make_arg_tuple<type_list<Ts...>, Tuple> {
        static auto from(Tuple&& tup) {
            return std::tuple<decltype(pick<Ts>(tup))...>(pick<Ts>(tup)...);
        }
    };

    /// Extract the argument of type `T` from a forwarded parameter pack.
    template <typename T, typename... Args>
    decltype(auto) get_arg(Args&&... args) {
        auto args_tuple = std::forward_as_tuple(args...);
        return gears::pick<T>(args_tuple);
    }

    /// True iff some argument's decayed type equals `T`.
    template <typename T, typename... Args>
    constexpr bool has_arg_type() {
        return (std::is_same_v<std::remove_cvref_t<Args>, T> || ...);
    }

    /// True iff some argument's exact (cv/ref-qualified) type equals `T`.
    template <typename T, typename... Args>
    constexpr bool has_exact_arg_type() {
        return (std::is_same_v<Args, T> || ...);
    }

    /// Aggregates a set of callables into one overload set — the standard
    /// `std::visit` helper. Used with the deduction guide below.
    template <class... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };

    template <class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;


    /**
     * @brief Helper to verify a predicate holds for all types in a variant.
     * Usage: all_types_satisfy<FieldValue, MyPredicate>() returns true if
     *        MyPredicate<T>::value is true for every T in FieldValue.
     */
    template <typename Variant, template <typename> class Predicate>
    struct all_variant_types_satisfy;

    template <template <typename> class Predicate, typename... Ts>
    struct all_variant_types_satisfy<std::variant<Ts...>, Predicate> {
        static constexpr bool value = (Predicate<Ts>::value && ...);
    };

    template <typename Variant, template <typename> class Predicate>
    inline constexpr bool all_variant_types_satisfy_v = all_variant_types_satisfy<Variant, Predicate>::value;


    /// True iff every type in `Ts...` is distinct (decay-insensitive comparison).
    template <typename...>
    struct all_unique : std::true_type {};

    template <typename Head, typename... Tail>
    struct all_unique<Head, Tail...>
        : std::bool_constant<(!std::is_same_v<Head, Tail> && ...) && all_unique<Tail...>::value> {};

    template <typename... Ts>
    inline constexpr bool all_unique_v = all_unique<Ts...>::value;


    /**
     * @brief Concatenate the alternatives of two `std::variant`s and remove duplicates,
     *        preserving the order of first occurrence (`V1`'s alternatives first).
     *
     * Useful when widening a variant to hold the union of two error sets — e.g.
     * `Outcome` chains where each step contributes its own error alternatives.
     */
    namespace detail {
        template <typename T, typename Variant>
        struct variant_contains;

        template <typename T, typename... Us>
        struct variant_contains<T, std::variant<Us...>> : std::bool_constant<(std::is_same_v<T, Us> || ...)> {};

        template <typename Acc, typename... Rest>
        struct variant_unique_append {
            using type = Acc;
        };

        template <typename... Acc, typename Head, typename... Rest>
        struct variant_unique_append<std::variant<Acc...>, Head, Rest...> {
            using type = std::conditional_t<variant_contains<Head, std::variant<Acc...>>::value,
                                            typename variant_unique_append<std::variant<Acc...>, Rest...>::type,
                                            typename variant_unique_append<std::variant<Acc..., Head>, Rest...>::type>;
        };
    }  // namespace detail

    template <typename V1, typename V2>
    struct merge_variants;

    template <typename... A, typename... B>
    struct merge_variants<std::variant<A...>, std::variant<B...>> {
        using type = detail::variant_unique_append<std::variant<A...>, B...>::type;
    };

    template <typename V1, typename V2>
    using merge_variants_t = merge_variants<V1, V2>::type;
}  // namespace demiplane::gears
