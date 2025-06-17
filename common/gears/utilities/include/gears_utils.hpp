#pragma once

namespace demiplane::gears {
    template <typename V>
    consteval void unused_value(const V& value) {
        static_cast<void>(value);
    }

    template<class T = void>
    consteval void unreachable()
    {
        static_assert(dependent_false_v<T>, "Unreachable branch reached.");
    }

    consteval void force_non_static(const void* this_) {
        static_cast<void>(this_);
    }

} // namespace demiplane
