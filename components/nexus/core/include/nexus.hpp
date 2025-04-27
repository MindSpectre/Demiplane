#pragma once

#include <demiplane/gears>
#include <functional>
#include <memory>
#include <shared_mutex>

#include "../nexus_traits.hpp"
#include "../details.hpp"
#include <access_policy.hpp>
#include <life_policy.hpp>

namespace demiplane::nexus {

    class Nexus : gears::Immutable {
    public:
        Nexus(); ///< ctor (реализация в .cpp)
        ~Nexus(); ///< dtor

        // ---------------- регистрация -------------------------------------------
        template <class T>
        void register_instance(std::unique_ptr<T> up, LifePolicy policy = LifePolicy::Singleton,
            std::uint32_t id = default_id_v<T>, TimedOpts topts = {}, std::function<void()> on_dtor = {});
        // TODO: register_factory()

        // ---------------- получение ---------------------------------------------
        template <class T, Access A = Access::Default>
        auto warp(std::uint32_t id = default_id_v<T>());

        // ---------------- управление --------------------------------------------
        template <class T>
        void reset(std::uint32_t id = default_id_v<T>());
        void clear();
        std::size_t size() const;

    private:
        struct Slot {
            std::shared_ptr<void> object; ///< хранимый объект
            LifePolicy policy{LifePolicy::Trivial};
            TimedOpts t_opts{};
            // дополнительные метаданные — TODO
        };

        using Map = std::unordered_map<detail::Key, Slot, detail::KeyHash>;

        mutable std::shared_mutex map_mtx_;
        Map map_;
        // janitor‑тред + другие детали — TODO
    };


} // namespace demiplane::nexus
