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
        virtual ~ConfigInterface()    = default;
        constexpr virtual void validate() const = 0;

        // Finalize with validation - returns Derived by value
        Derived finalize() && {
            validate();
            return std::move(static_cast<Derived&>(*this));
        }
        Derived finalize() & {
            validate();
            return static_cast<Derived&>(*this);
        }
        // Serialization - use the SerializeStruct template param
        [[nodiscard]] SerializeStruct serialize() const {
            validate();
            return wrapped_serialize();
        }

        void deserialize(const SerializeStruct& config) {
            validate();
            wrapped_deserialize(config);
        }
        // Static deserialization enforcement via static_assert in constructor
    protected:
        [[nodiscard]] virtual SerializeStruct wrapped_serialize() const = 0;

        virtual void wrapped_deserialize(const SerializeStruct& config) = 0;
    };

    template <typename T>
    struct TypePrinter;
}  // namespace demiplane::gears
