#pragma once

#include <chrono>
#include <variant>

namespace demiplane::nexus {
    /// @brief This instance is resettable
    struct Resettable {};

    /// @brief This instance is not resettable
    struct Immortal {};

    /// @brief Limited lifetime when instance is not in use (stored only in registry)
    struct Timed {
        std::chrono::seconds idle{60};
    };

    /**
     * @brief Instance is destroyed instantly when all refs go out of scope.
     *
     * @details When you got the object, you add ref count by one;
     * when it drops to 1(only Nexus copy), it will be destructed.
     *
     * @warning If you register an object (not factory) and an object not in use,
     * it will be swept after the first cycle causing unwanted behavior
     *
     * @note Supposed with factory using, creating the object in place
     */
    struct Scoped {};

    using Lifetime = std::variant<Resettable, Scoped, Timed, Immortal>;
}  // namespace demiplane::nexus
