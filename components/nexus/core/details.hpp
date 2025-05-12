#pragma once

#include <cstdint>
#include <functional>
#include <typeindex>

namespace demiplane::nexus::detail {

    struct Key {
        std::type_index type;
        std::uint32_t id;
        bool operator==(const Key& o) const noexcept {
            return type == o.type && id == o.id;
        }
    };

    struct KeyHash {
        std::size_t operator()(const Key& k) const noexcept {
            return k.type.hash_code()
                 ^ (static_cast<std::size_t>(k.id) + 0x9e3779b97f4a7c15ULL + (k.type.hash_code() << 6)
                     + (k.type.hash_code() >> 2));
        }
    };

} // namespace dp::detail
