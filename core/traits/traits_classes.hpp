#pragma once

namespace demiplane {
    class NonCopyable {
    public:
        NonCopyable()                                        = default;
        NonCopyable(const NonCopyable& other)                = delete;
        NonCopyable& operator=(const NonCopyable& other)     = delete;
        NonCopyable(NonCopyable&& other) noexcept            = default;
        NonCopyable& operator=(NonCopyable&& other) noexcept = default;
    };

    class Immovable {
    public:
        Immovable()                                       = default;
        Immovable(const Immovable&)                      = default;
        Immovable& operator=(const Immovable& other)     = default;
        Immovable(Immovable&& other) noexcept            = delete;
        Immovable& operator=(Immovable&& other) noexcept = delete;

        ~Immovable() = default;
    };
    class Singleton {
    protected:
        Singleton()  = default;
        virtual ~Singleton() = default;

    public:
        Singleton(const Singleton&)            = delete;
        Singleton& operator=(const Singleton&) = delete;
        Singleton(Singleton&&)                 = delete;
        Singleton& operator=(Singleton&&)      = delete;
    };
    class Immutable {
    public:
        Immutable()                            = default;
        Immutable(const Immutable&)            = delete;
        Immutable& operator=(const Immutable&) = delete;
        Immutable(Immutable&&)                 = delete;
        Immutable& operator=(Immutable&&)      = delete;
    };


    template <class Derived>
    class HasName {
    public:
        HasName() {
            check_static_interface(); // Triggers error on construction
        }

    protected:
        // This will cause a compile error if Derived::name() is missing
        static void check_static_interface() {
            (void) Derived::name(); // Use it to enforce the existence
        }
    };

    class NoName : public HasName<NoName> {
    public:
        static constexpr const char* name() {
            return "NoName";
        }
    };
} // namespace demiplane
