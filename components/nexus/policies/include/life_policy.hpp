#pragma once

#include <chrono>

namespace demiplane::nexus {
    /// @brief Правила жизни сервиса внутри Nexus
    enum class LifePolicy {
        Trivial, ///< каждый warp отдаёт копию (если объект копируем)
        Scoped, ///< shared_ptr живёт, пока есть хотя бы один pylon
        Timed, ///< shared_ptr + eviction по таймеру idle‑ttl
        Singleton, ///< один на процесс, допускает reset()
        Immortal ///< не уничтожается до выхода из процесса
    };

    struct TimedOpts {
        std::chrono::seconds idle_ttl{60};
    };
} // namespace demiplane::nexus
