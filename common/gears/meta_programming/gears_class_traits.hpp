#pragma once

#include "gears_concepts.hpp"

namespace demiplane::gears {
    /** @brief Mixin disabling copy operations while leaving move enabled. */
    struct NonCopyable {
        NonCopyable()                                        = default;
        NonCopyable& operator=(NonCopyable&& other) noexcept = default;
        NonCopyable(NonCopyable&& other) noexcept            = default;
        NonCopyable(const NonCopyable& other)                = delete;
        NonCopyable& operator=(const NonCopyable& other)     = delete;
    };

    /** @brief Mixin disabling move operations while leaving copy enabled. */
    struct Immovable {
        Immovable()                                      = default;
        Immovable(const Immovable&)                      = default;
        Immovable& operator=(const Immovable& other)     = default;
        Immovable(Immovable&& other) noexcept            = delete;
        Immovable& operator=(Immovable&& other) noexcept = delete;

        ~Immovable() = default;
    };

    /** @brief Mixin disabling both copy and move — pinned, single-instance object. */
    struct Immutable {
        Immutable()                            = default;
        Immutable(const Immutable&)            = delete;
        Immutable& operator=(const Immutable&) = delete;
        Immutable(Immutable&&)                 = delete;
        Immutable& operator=(Immutable&&)      = delete;
    };


    /**
     * @brief Aggregate that inherits from a pack of abstract interfaces and
     *        inherit-imports their constructors.
     *
     * Lets a single composite type satisfy several interface contracts without
     * declaring an empty class for each combination.
     */
    template <IsInterface... Interfaces>
    struct InterfaceBundle : Interfaces... {
        using Interfaces::Interfaces...;
        InterfaceBundle()           = default;
        ~InterfaceBundle() override = default;  // optional if Traits have virtual dtors
    };

    /**
     * @brief Curries `T` into a list of single-parameter interface templates,
     *        producing an `InterfaceBundle` specialised for `T`.
     *
     * Usage: `InterfaceBundleFor<T>::From<IfaceA, IfaceB>` ⇒
     *        `InterfaceBundle<IfaceA<T>, IfaceB<T>>`.
     */
    template <typename T>
    struct InterfaceBundleFor {
        template <template <typename> class... Interfaces>
        using From = InterfaceBundle<Interfaces<T>...>;
    };

    /**
     * @brief Diagnostic helper — instantiating it forces the compiler to print
     *        `T` in the error message. Intentionally left undefined.
     */
    template <typename T>
    struct TypePrinter;
}  // namespace demiplane::gears
