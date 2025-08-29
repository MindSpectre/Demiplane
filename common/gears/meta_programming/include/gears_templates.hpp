#pragma once

#include <type_traits>
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

    template <typename, template <class...> class>
    struct is_specialization_of : std::false_type {};

    template <template <class...> class Template, typename... Args>
    struct is_specialization_of<Template<Args...>, Template> : std::true_type {};

    template <typename ClassWithTemplate, template <class...> class BaseClass>
    inline constexpr bool is_specialization_of_v = is_specialization_of<ClassWithTemplate, BaseClass>::value;


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
    template <class... Ts>
    struct type_list {};

    // ── pick<T>() → std::get<T>(tuple) wrapper ────────────────────────
    template <class T, class Tuple>
    decltype(auto) pick(Tuple&& t) {
        return std::get<T>(std::forward<Tuple>(t));
    }

    // ── turn a type_list<Ts…> into a tuple of objects from `tuple` ───
    template <class List, class Tuple>
    struct make_arg_tuple;

    template <class... Ts, class Tuple>
    struct make_arg_tuple<type_list<Ts...>, Tuple> {
        static auto from(Tuple&& tup) {
            return std::tuple<decltype(pick<Ts>(tup))...>(pick<Ts>(tup)...);
        }
    };
    // Generic function to extract any type T from arguments using existing gears::pick
    template <typename T, typename... Args>
    decltype(auto) get_arg(Args&&... args) {
        auto args_tuple = std::forward_as_tuple(args...);
        return gears::pick<T>(args_tuple);
    }

    // Check if arguments contain type T
    template <typename T, typename... Args>
    constexpr bool has_arg_type() {
        return (std::is_same_v<std::remove_cvref_t<Args>, T> || ...);
    }
    template <typename T, typename... Args>
    constexpr bool has_exact_arg_type() {
        return (std::is_same_v<Args, T> || ...);
    }
}  // namespace demiplane::gears
