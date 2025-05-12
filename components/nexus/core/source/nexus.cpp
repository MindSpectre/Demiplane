#include "nexus.hpp"

#include <mutex>
#include <utility>

#include <pylon.hpp>
namespace demiplane::nexus {
    // в demiplane/nexus/nexus.hpp

    template <class T, class Factory>
    void Nexus::register_factory(Factory ctor, LifetimeControlVariant policy, const std::uint32_t id) {
        std::unique_lock ul(map_mtx_);
        auto& [object, lifetime, factory] = map_[{typeid(T), id}];
        object                            = nullptr; // lazy
        lifetime                          = std::move(policy);
        factory                           = [c = std::move(ctor)]() { return std::shared_ptr<void>(c().release()); };
    }
    template <class T>
    void Nexus::register_instance(T obj, LifetimeControlVariant policy, const std::uint32_t id) {
        std::unique_lock ul(map_mtx_);
        auto& [object, lifetime, factory] = map_[{typeid(T), id}];
        object                            = std::move(obj); // Nexus владелец
        lifetime                          = std::move(policy);
    }
    template <class T>
    void Nexus::register_shared(std::shared_ptr<T> sp, LifetimeControlVariant policy, const std::uint32_t id) {
        std::unique_lock ul(map_mtx_);
        auto& [object, lifetime, factory] = map_[{typeid(T), id}];
        object                            = std::move(sp);
        lifetime                          = std::move(policy);
    }


    template <class T, AccessPolicy A /* = AccessPolicy::Alias */>
    auto Nexus::warp(std::uint32_t id /* = default_id_v<T>() */) {
        // --------  общая часть: достаём shared_ptr<T>  -------------------------
        std::shared_ptr<T> sp;
        const detail::Key k{std::type_index(typeid(T)), id};
        Slot* slot_ptr = nullptr;
        {
            std::shared_lock rlock(map_mtx_);

            const auto it = map_.find(k);
            if (it == map_.end()) {
                throw std::runtime_error("Nexus::warp – service not registered");
            }
            slot_ptr = &it->second;
            // ленивое создание через factory (если object ещё пустой)
            if (!slot_ptr->object && slot_ptr->factory) {
                //TODO: Fix thread window problem
                rlock.unlock(); // апгрейд до эксклюзива
                std::unique_lock write_lock(map_mtx_);
                if (!slot_ptr->object) { // двойная проверка
                    slot_ptr->object = slot_ptr->factory();
                }
            }

            if (!slot_ptr->object) {
                throw std::runtime_error("Nexus::warp – factory returned nullptr");
            }

            sp = slot_ptr->cast<T>();

            // Timed — отметим «последний доступ»
            if (const auto policy = std::get_if<TimedLifetimeControl>(&slot_ptr->lifetime)) {
                policy->last_access = std::chrono::steady_clock::now();
            }

            // Scoped — увеличим счётчик живых pylons
            if (const auto policy = std::get_if<ScopedLifetimeControl>(&slot_ptr->lifetime)) {
                policy->pylon_count.fetch_add(1, std::memory_order_relaxed);
            }
        }

        if constexpr (A == AccessPolicy::Alias) {
            return pylon<T>{sp.get()}; // Nexus владеет, Pylon – raw
        } else if constexpr (A == AccessPolicy::View) {
            return view_pylon<T>{sp}; // const-view
        } else if constexpr (A == AccessPolicy::Safe) {
            return safe_pylon<T>{std::move(sp)}; // weak_ptr + lock()
        } else if constexpr (A == AccessPolicy::Unique) {
            // unique_pylon кинет своё исключение, если второй раз
            return unique_pylon<T>{std::move(sp)};
        } else if constexpr (A == AccessPolicy::Copy) {
            static_assert(std::is_copy_constructible_v<T>, "AccessPolicy::Copy requires copy-constructible T");
            return copy_pylon<T>{*sp}; // глубокая (или move) копия
        } else {
            static_assert(gears::always_false_v<T>, "Unsupported AccessPolicy");
        }
    }


    template <class T>
    void Nexus::reset(const std::uint32_t id) {
        const detail::Key k{std::type_index(typeid(T)), id};
        std::lock_guard lk(map_mtx_);
        const auto it = map_.find(k);
        if (it == map_.end()) {
            throw std::out_of_range("Nexus::reset: no such object");
        }
        std::vector<int> x;
        if (std::holds_alternative<FlexLifetimeControl>(it->second.lifetime)) {
            map_.erase(it);
        } else {
            throw std::runtime_error("Nexus::reset: only FlexLifetimeControl is supported");
        }
    }


    template <class T>
        requires std::is_move_constructible_v<T>
    T Nexus::revoke(const std::uint32_t id) {
        try {
            std::lock_guard lk(map_mtx_);
            T tmp{std::move(map_.at(detail::Key{std::type_index(typeid(T)), id}).object.get())};
            Nexus::reset<T>(id);
            return tmp;
        } catch ([[maybe_unused]] const std::out_of_range& e) {
            throw std::out_of_range("Nexus::revoke: no such object");
        }
    }


    Nexus::Nexus() {
        std::cerr << "Nexus ctor" << std::endl;
    }
    Nexus::~Nexus() {
        std::cerr << "Nexus dtor" << std::endl;
    }
    void Nexus::clear() {
        std::lock_guard lk(map_mtx_);
        map_.clear();
    }
    std::size_t Nexus::size() const noexcept {
        return map_.size();
    }
} // namespace demiplane::nexus
