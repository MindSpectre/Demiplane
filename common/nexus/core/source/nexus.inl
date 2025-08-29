#pragma once
namespace demiplane::nexus {
    template <class T, class Factory>
    void Nexus::register_factory(Factory&& f, const Lifetime lt, const std::uint32_t id) {
        boost::unique_lock lk{mtx_};
        Slot& s   = map_[Key{typeid(T), id}];
        s.obj     = nullptr;  // lazy
        s.factory = to_void_factory(std::forward<Factory>(f));
        s.lt      = lt;
    }

    template <class T>
    void Nexus::register_shared(std::shared_ptr<T> sp, const Lifetime lt, const std::uint32_t id) {
        boost::unique_lock lk{mtx_};
        Slot& s      = map_[Key{typeid(T), id}];
        s.obj        = std::move(sp);
        s.factory    = nullptr;
        s.lt         = lt;
        s.last_touch = std::chrono::steady_clock::now();
    }

    template <class T>
    void Nexus::register_instance(T value, Lifetime lt, const std::uint32_t id) {
        register_shared<T>(std::make_shared<T>(std::move(value)), std::move(lt), id);
    }

    // ───────── spawn<T>() ─────────

    template <class T>
    std::shared_ptr<T> Nexus::spawn(const std::uint32_t id) {
        const Key k{typeid(T), id};

        // Fast path - check if already constructed
        {
            boost::shared_lock r_lock{mtx_};
            const auto it = map_.find(k);
            if (it == map_.end()) {
                throw std::runtime_error("Nexus::spawn – not registered");
            }

            auto& sl = it->second;

            // Check for expired Timed objects
            if (std::holds_alternative<Timed>(sl.lt) && sl.expired_flag &&
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
            boost::shared_lock r_lock{mtx_};
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
        std::lock_guard<std::mutex> construct_lock(*construction_mtx);

        // Double-check after acquiring construction lock
        {
            boost::shared_lock r_lock{mtx_};
            const auto it = map_.find(k);
            if (it != map_.end()) {
                auto& sl = it->second;

                // Check again if object was constructed by another thread
                if (std::holds_alternative<Timed>(sl.lt) && sl.expired_flag &&
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
            boost::unique_lock w_lock{mtx_};
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
        boost::unique_lock lk{mtx_};
        const Key k{typeid(T), id};
        const auto it = map_.find(k);
        if (it == map_.end()) {
            throw std::runtime_error("Nexus::reset – no such object");
        }
        if (!std::holds_alternative<Resettable>(it->second.lt)) {
            throw std::runtime_error("Nexus::reset – only Flex lifetime can be reset");
        }
        map_.erase(it);
    }

    template <class T>
    std::shared_ptr<T> Nexus::build_handle(Slot& slot) {
        using namespace std::chrono;
        if (std::holds_alternative<Timed>(slot.lt)) {
            slot.last_touch = steady_clock::now();
        }
        return std::static_pointer_cast<T>(slot.obj);
    }

    inline std::size_t Nexus::size() const noexcept {
        boost::shared_lock lk{mtx_};
        return map_.size();
    }
}  // namespace demiplane::nexus
