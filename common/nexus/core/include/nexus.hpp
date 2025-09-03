#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <boost/thread/shared_mutex.hpp>

#include "../details.hpp"
#include "../nexus_traits.hpp"
#include "policies.hpp"

namespace demiplane::nexus {
    class Nexus {  //  ─── public façade ───
    public:
        Nexus();
        ~Nexus();
        // ───────── registration ─────────
        template <class T, class Factory>
        void register_factory(Factory&& f, Lifetime lt = get_nexus_policy<T>(), std::uint32_t id = get_nexus_id<T>());

        template <class T>
        void register_shared(std::shared_ptr<T> sp,
                             Lifetime lt      = get_nexus_policy<T>(),
                             std::uint32_t id = get_nexus_id<T>());

        template <class T>
        void register_instance(T value, Lifetime lt = get_nexus_policy<T>(), std::uint32_t id = get_nexus_id<T>());

        // ───────── access ─────────
        template <class T>
        std::shared_ptr<T> spawn(std::uint32_t id = get_nexus_id<T>());

        // ───────── management ────────
        template <class T>
        void reset(std::uint32_t id = get_nexus_id<T>());  // only Flex

        std::size_t size() const noexcept;
        void clear();

        void set_sweep_interval(const std::chrono::seconds sweep_interval) {
            sweep_interval_.store(sweep_interval);
        }

    private:
        using Key     = detail::Key;
        using KeyHash = detail::KeyHash;

        struct Slot {
            std::shared_ptr<void> obj;
            std::function<std::shared_ptr<void>()> factory;
            Lifetime lt;
            std::chrono::steady_clock::time_point last_touch{};

            // For Timed lifetime - store the expiry flag
            std::shared_ptr<std::atomic<bool>> expired_flag;

            // For preventing duplicate construction during concurrent spawns
            std::mutex construction_mutex;
            std::atomic<bool> constructing{false};
        };

        // helpers
        template <class F>
        static auto to_void_factory(F&& f) {
            return [fn = std::forward<F>(f)] { return std::shared_ptr<void>(fn()); };
        }

        template <class T>
        std::shared_ptr<T> build_handle(Slot& slot);

        // janitor
        void sweep_loop();
        void sweep();

        // state
        std::unordered_map<Key, Slot, KeyHash> map_;
        mutable boost::shared_mutex mtx_;
        std::jthread janitor_;
        std::atomic<bool> stop_{false};
        std::atomic<std::chrono::seconds> sweep_interval_{std::chrono::seconds(5)};
    };

    inline Nexus& instance() {
        static Nexus instance;
        return instance;
    }
}  // namespace demiplane::nexus

#define NEXUS_REGISTER(id, Policy)                                                                                     \
    static constexpr std::uint32_t nexus_id = id;                                                                      \
    static constexpr Policy nexus_policy {                                                                             \
    }


#include "../source/nexus.inl"
