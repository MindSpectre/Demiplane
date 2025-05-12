#pragma once

namespace demiplane::gears {
    template <typename V>
    consteval void unused_value(const V& value) {
        static_cast<void>(value);
    }
} // namespace demiplane
