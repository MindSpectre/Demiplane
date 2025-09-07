#pragma once

#include <cstdint>
#include <typeindex>

namespace demiplane::nexus {

    using nexus_id_t = std::uint32_t;

    namespace detail {

        struct Key {
            std::type_index type;
            nexus_id_t id;
            bool operator==(const Key& o) const noexcept {
                return type == o.type && id == o.id;
            }
        };

        struct KeyHash {
            std::uint64_t operator()(const Key& k) const noexcept {
                // 64‑bit splitmix‑style combine
                std::uint64_t h  = k.type.hash_code();
                h               ^= static_cast<std::uint64_t>(k.id) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                return h;
            }
        };

    }  // namespace detail
}  // namespace demiplane::nexus
