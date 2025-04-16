#pragma once

namespace demiplane {
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


    template <typename T>
    concept HasStaticName = requires {
        { T::name() } -> std::convertible_to<std::string_view>;
    };


    struct AnonymousClass {
        static constexpr const char* name() {
            return "Anonymous class";
        }
    };
} // namespace demiplane
