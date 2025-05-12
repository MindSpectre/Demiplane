#pragma once

#include <demiplane/gears>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <variant>

#include "../details.hpp"
#include "../nexus_traits.hpp"
#include <access_policy.hpp>
#include <life_policy.hpp>

namespace demiplane::nexus {

    class Nexus : gears::Immutable {
    public:
        Nexus(); ///< ctor (реализация в .cpp)
        ~Nexus(); ///< dtor


        template <class T, class Factory>
        void register_factory(Factory ctor, LifetimeControlVariant policy, std::uint32_t id = default_id_v<T>);

        template <class T>
        void register_instance(T obj, LifetimeControlVariant policy, std::uint32_t id = default_id_v<T>);

        template <class T>
        void register_shared(std::shared_ptr<T> sp, LifetimeControlVariant policy, std::uint32_t id = default_id_v<T>);

        template <class T, class Factory>
        void register_factory(
            Factory ctor, const LifePolicy policy = DefaultLifePolicy, const std::uint32_t id = default_id_v<T>) {
            this->register_factory<T, Factory>(ctor, make_default_lifetime_policy(policy), id);
        }

        template <class T>
        void register_instance(
            T obj, const LifePolicy policy = DefaultLifePolicy, const std::uint32_t id = default_id_v<T>) {
            this->register_instance<T>(obj, make_default_lifetime_policy(policy), id);
        }

        template <class T>
        void register_shared(std::shared_ptr<T> sp, const LifePolicy policy = DefaultLifePolicy,
            const std::uint32_t id = default_id_v<T>) {
            this->register_shared<T>(sp, make_default_lifetime_policy(policy), id);
        }


        // ---------------- получение ---------------------------------------------
        template <class T, AccessPolicy A = AccessPolicy::Alias>
        auto warp(std::uint32_t id = default_id_v<T>());

        // ---------------- управление --------------------------------------------
        template <class T>
        void reset(std::uint32_t id = default_id_v<T>());

        template <class T>
            requires std::is_move_constructible_v<T>
        T revoke(std::uint32_t id = default_id_v<T>());

        void clear();
        std::size_t size() const noexcept;

    private:
        struct Slot {
            std::shared_ptr<void> object; ///< хранимый объект
            LifetimeControlVariant lifetime;
            std::function<std::shared_ptr<void>()> factory;
            [[nodiscard]] LifePolicy policy() const noexcept {
                return static_cast<LifePolicy>(lifetime.index());
            }
            template <class T>
            [[nodiscard]] std::shared_ptr<T> cast() const {
                return std::static_pointer_cast<T>(object);
            }
        };
        static_assert(sizeof(Slot) <= 80, "Slot grew too much – check memory layout");

        mutable std::shared_mutex map_mtx_;
        std::unordered_map<detail::Key, Slot, detail::KeyHash> map_;
        // janitor‑тред + другие детали — TODO
    };


} // namespace demiplane::nexus