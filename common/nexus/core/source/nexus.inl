#pragma once
namespace demiplane::nexus {
    template <class T, typename Factory>
        requires std::is_invocable_v<Factory>
    void Nexus::register_singleton(Factory&& f, const Lifetime lt) {
        this->register_instance<T>(std::forward<Factory>(f), 0, std::move(lt));
    }

    template <class T>
    void Nexus::register_singleton(std::shared_ptr<T> sp, const Lifetime lt) {
        this->register_instance<T>(std::move(sp), 0, std::move(lt));
    }

    template <class T>
        requires std::is_object_v<T>
    void Nexus::register_singleton(T value, Lifetime lt) {
        this->register_instance<T>(std::make_shared<T>(std::move(value)), 0, std::move(lt));
    }

    template <class T, typename Factory>
        requires std::is_invocable_v<Factory>
    void Nexus::register_instance(Factory&& f, const nexus_id_t id, const Lifetime lt) {
        std::unique_lock lk{mtx_};
        Slot& s   = map_[Key{typeid(T), id}];
        s.obj     = nullptr;  // lazy
        s.factory = to_void_factory(std::forward<Factory>(f));
        s.lt      = lt;
    }

    template <class T>
    void Nexus::register_instance(std::shared_ptr<T> sp, const nexus_id_t id, const Lifetime lt) {
        std::unique_lock lk{mtx_};
        Slot& s      = map_[Key{typeid(T), id}];
        s.obj        = std::move(sp);
        s.factory    = nullptr;
        s.lt         = lt;
        s.last_touch = std::chrono::steady_clock::now();
    }

    template <class T>
        requires std::is_object_v<T>
    void Nexus::register_instance(T value, const nexus_id_t id, Lifetime lt) {
        this->register_instance<T>(std::make_shared<T>(std::move(value)), id, std::move(lt));
    }

    template <class T>
    std::shared_ptr<T> Nexus::get(const std::uint32_t id) {
        const Key k{typeid(T), id};

        // Fast path - check if already constructed
        {
            std::shared_lock r_lock{mtx_};
            const auto it = map_.find(k);
            if (it == map_.end()) {
                throw std::runtime_error("Nexus::spawn – not registered");
            }

            // Check for expired Timed objects
            if (auto& sl = it->second; std::holds_alternative<Timed>(sl.lt) && sl.expired_flag &&
                                       sl.expired_flag->load(std::memory_order_acquire)) {
                // Object has expired, need to recreate
                // Fall through to construction path
            } else if (sl.obj) {
                // Object exists and is valid
                return build_handle<T>(sl);
            }
        }

        // Slow path - need to construct
        // First, get the slot's construction mutex
        std::mutex* construction_mtx = nullptr;
        std::function<std::shared_ptr<void>()> factory_copy;

        {
            std::shared_lock r_lock{mtx_};
            const auto it = map_.find(k);
            if (it == map_.end()) {
                throw std::runtime_error("Nexus::spawn – not registered");
            }

            auto& sl         = it->second;
            construction_mtx = &sl.construction_mutex;
            factory_copy     = sl.factory;
        }

        // Lock the construction mutex for this specific slot
        // This allows other threads to construct different types concurrently
        std::lock_guard construct_lock(*construction_mtx);

        // Double-check after acquiring construction lock
        {
            std::shared_lock r_lock{mtx_};
            if (const auto it = map_.find(k); it != map_.end()) {
                // Check again if object was constructed by another thread
                if (auto& sl = it->second; std::holds_alternative<Timed>(sl.lt) && sl.expired_flag &&
                                           sl.expired_flag->load(std::memory_order_acquire)) {
                    // Still expired, continue with construction
                } else if (sl.obj) {
                    return build_handle<T>(sl);
                }
            }
        }

        // Execute factory OUTSIDE of any Nexus locks
        // This allows the factory to call spawn() for dependencies without deadlock
        std::shared_ptr<void> new_obj;
        if (factory_copy) {
            new_obj = factory_copy();
        } else {
            throw std::runtime_error("Nexus::spawn – no factory available");
        }

        // Store the constructed object
        {
            std::unique_lock w_lock{mtx_};
            const auto it = map_.find(k);
            if (it == map_.end()) {
                throw std::runtime_error("Nexus::spawn – registration removed during construction");
            }

            auto& sl      = it->second;
            sl.obj        = new_obj;
            sl.last_touch = std::chrono::steady_clock::now();

            // For Timed objects, create new expiry flag
            if (std::holds_alternative<Timed>(sl.lt)) {
                sl.expired_flag = std::make_shared<std::atomic<bool>>(false);
            }

            return build_handle<T>(sl);
        }
    }

    // ───────── reset<T>() ─────────

    template <class T>
    void Nexus::reset(const std::uint32_t id) {
        std::unique_lock lk{mtx_};
        const Key k{typeid(T), id};
        const auto it = map_.find(k);
        if (it == map_.end()) {
            throw std::runtime_error("Nexus::reset – no such object");
        }
        if (!std::holds_alternative<Resettable>(it->second.lt)) {
            throw std::runtime_error("Nexus::reset – only Resettable lifetime can be reset");
        }
        map_.erase(it);
    }
    template <class T>
    bool Nexus::has(const nexus_id_t id) const noexcept {
        std::shared_lock lk{mtx_};
        return map_.contains(Key{typeid(T), id});
    }

    template <class T>
    std::shared_ptr<T> Nexus::build_handle(Slot& slot) {
        if (std::holds_alternative<Timed>(slot.lt)) {
            slot.last_touch = std::chrono::steady_clock::now();
        }
        return std::static_pointer_cast<T>(slot.obj);
    }

    inline std::size_t Nexus::size() const noexcept {
        std::shared_lock lk{mtx_};
        return map_.size();
    }
}  // namespace demiplane::nexus
