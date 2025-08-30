#pragma once

#include <chrono>
#include <variant>

namespace demiplane::nexus {
    struct Resettable {};

    struct Immortal {};

    struct Timed {
        std::chrono::seconds idle{60};
    };

    struct Scoped {};

    using Lifetime = std::variant<Resettable, Scoped, Timed, Immortal>;
}  // namespace demiplane::nexus
