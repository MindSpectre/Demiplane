#pragma once

#include "gears_concepts.hpp"

namespace demiplane::gears {
    struct NonCopyable {
        NonCopyable()                                        = default;
        NonCopyable& operator=(NonCopyable&& other) noexcept = default;
        NonCopyable(NonCopyable&& other) noexcept            = default;
        NonCopyable(const NonCopyable& other)                = delete;
        NonCopyable& operator=(const NonCopyable& other)     = delete;
    };

    struct Immovable {
        Immovable()                                      = default;
        Immovable(const Immovable&)                      = default;
        Immovable& operator=(const Immovable& other)     = default;
        Immovable(Immovable&& other) noexcept            = delete;
        Immovable& operator=(Immovable&& other) noexcept = delete;

        ~Immovable() = default;
    };
    struct Immutable {
        Immutable()                            = default;
        Immutable(const Immutable&)            = delete;
        Immutable& operator=(const Immutable&) = delete;
        Immutable(Immutable&&)                 = delete;
        Immutable& operator=(Immutable&&)      = delete;
    };


    template <IsInterface... Interfaces>
    struct InterfaceBundle : Interfaces... {
        using Interfaces::Interfaces...;
        InterfaceBundle()           = default;
        ~InterfaceBundle() override = default;  // optional if Traits have virtual dtors
    };

    template <typename T>
    struct InterfaceBundleFor {
        template <template <typename> class... Interfaces>
        using From = InterfaceBundle<Interfaces<T>...>;
    };

    template <typename Derived, typename SerializeStruct>
    class ConfigInterface {
    public:
        virtual ~ConfigInterface() = default;
        virtual void validate()    = 0;

        // Finalize with validation - returns Derived by value
        Derived finalize() && {
            validate();
            return std::move(static_cast<Derived&>(*this));
        }

        // Serialization - use the SerializeStruct template param
        virtual SerializeStruct serialize() const = 0;

        // Static deserialization enforcement via static_assert in constructor
    protected:
        ConfigInterface() {
            // Compile-time check that Derived has static deserialize method
            static_assert(IsDeserializable<Derived, SerializeStruct>,
                          "Derived config must implement: static Derived deserialize(const SerializeStruct&)");
        }
    };
}  // namespace demiplane::gears
