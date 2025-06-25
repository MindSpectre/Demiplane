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

    template <template <class...> class, typename>
    struct is_specialization_of : std::false_type {};

    template <template <class...> class Template, typename... Args>
    struct is_specialization_of<Template, Template<Args...>> : std::true_type {};

    template <template <class...> class Template, typename... Args>
    inline constexpr bool is_specialization_of_v = is_specialization_of<Template, Args...>::value;


    template <template <typename...> class BaseTemplate, typename Derived>
    struct derived_from_specialization_of {
    private:
        // Helper to detect convertibility to BaseTemplate<Args...>
        template <typename... Args>
        static std::true_type test(const BaseTemplate<Args...>*) {
            return std::true_type{};
        }

        static std::false_type test(...) {
            return std::false_type{};
        }

    public:
        static constexpr bool value = decltype(test(std::declval<Derived*>()))::value;
    };

    // Concept for cleaner use
    template <template <typename...> class BaseTemplate, typename Derived>
    inline constexpr bool derived_from_specialization_of_v = derived_from_specialization_of<BaseTemplate, Derived>::value;


    template <class...>
    inline constexpr bool dependent_false_v = false;


    template <typename>
    struct is_vector : std::false_type {};

    template <typename T, typename Alloc>
    struct is_vector<std::vector<T, Alloc>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_vector_v = is_vector<T>::value;

    template <typename T>
    concept Interface = std::is_abstract_v<T>;

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


} // namespace demiplane::gears
