#pragma once

#include <atomic>
#include <chrono>

namespace demiplane::nexus {
    /// @brief Правила жизни сервиса внутри Nexus
    enum class LifePolicy : uint8_t { ///< каждый warp отдаёт копию (если объект копируем)
        Flex, ///< один на процесс, допускает reset()
        Scoped, ///< shared_ptr живёт, пока есть хотя бы один pylon
        Timed, ///< shared_ptr + eviction по таймеру idle‑ttl
        Immortal ///< не уничтожается до выхода из процесса
    };

    struct ObjectLifetimeControl {
        virtual ~ObjectLifetimeControl() = default;
    };

    struct TimedLifetimeControl final : ObjectLifetimeControl {
        std::chrono::seconds idle_ttl{60};
        std::chrono::steady_clock::time_point last_access{std::chrono::steady_clock::now()};
        [[nodiscard]] bool expired() const noexcept {
            return std::chrono::steady_clock::now() - last_access > idle_ttl;
        }
    };

    struct FlexLifetimeControl final : ObjectLifetimeControl {};

    struct ScopedLifetimeControl final : ObjectLifetimeControl {
        std::atomic<std::size_t> pylon_count;
        ScopedLifetimeControl() {
            pylon_count.store(1, std::memory_order_relaxed);
        }
        ScopedLifetimeControl(const ScopedLifetimeControl& other) {
            pylon_count.store(other.pylon_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        ScopedLifetimeControl& operator=(const ScopedLifetimeControl& other) {
            pylon_count.store(other.pylon_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
            return *this;
        }
        //todo: custom class create
    };

    struct ImmortalLifetimeControl final : ObjectLifetimeControl {};

    using LifetimeControlVariant =
        std::variant<FlexLifetimeControl, ScopedLifetimeControl, TimedLifetimeControl, ImmortalLifetimeControl>;

    constexpr LifetimeControlVariant make_default_lifetime_policy(const LifePolicy p) {
        static_assert(
            static_cast<uint8_t>(LifePolicy::Immortal) == 3, "Update make_lifetime when adding new LifePolicy values");

        switch (p) {
        case LifePolicy::Timed:
            return TimedLifetimeControl{};
        case LifePolicy::Scoped:
            return ScopedLifetimeControl{};
        case LifePolicy::Immortal:
            return ImmortalLifetimeControl{};
        case LifePolicy::Flex:
            return FlexLifetimeControl{};
        }
        // This line is unreachable but needed to silence compiler warnings
        return FlexLifetimeControl{};
    }


    constexpr auto DefaultLifePolicy = LifePolicy::Flex;
} // namespace demiplane::nexus
