#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "../details.hpp"
#include "../nexus_traits.hpp"
#include "policies.hpp"
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
namespace demiplane::nexus {

    class Nexus { //  ─── public façade ───
    public:
        Nexus();
        ~Nexus();

        static Nexus& instance() {
            static Nexus instance;
            return instance;
        }

        // ───────── registration ─────────
        template <class T, class Factory>
        void register_factory(Factory&& f, Lifetime lt = Flex{}, std::uint32_t id = default_id_v<T>);

        template <class T>
        void register_shared(std::shared_ptr<T> sp, Lifetime lt = Flex{}, std::uint32_t id = default_id_v<T>);

        template <class T>
        void register_instance(T value, Lifetime lt = Flex{}, std::uint32_t id = default_id_v<T>);

        // ───────── access ─────────
        template <class T>
        std::shared_ptr<T> spawn(std::uint32_t id = default_id_v<T>);

        // ───────── management ────────
        template <class T>
        void reset(std::uint32_t id = default_id_v<T>); // only Flex

        std::size_t size() const noexcept;
        void clear();

    private:
        using Key     = detail::Key;
        using KeyHash = detail::KeyHash;

        struct Slot {
            std::shared_ptr<void> obj;
            std::function<std::shared_ptr<void>(Nexus&)> factory;
            Lifetime lt;
            std::atomic<std::size_t> scoped_refs{0};
            std::chrono::steady_clock::time_point last_touch{};
        };

        // helpers
        template <class F>
        static auto to_void_factory(F&& f) {
            return [fn = std::forward<F>(f)](Nexus& nx) { return std::shared_ptr<void>(fn(nx)); };
        }
        template <class T>
        std::shared_ptr<T> build_handle(Slot& slot) {
            using namespace std::chrono;
            if (std::holds_alternative<Timed>(slot.lt)) {
                slot.last_touch = steady_clock::now();
            }

            if (std::holds_alternative<Scoped>(slot.lt)) {
                auto base = std::static_pointer_cast<T>(slot.obj);
                slot.scoped_refs.fetch_add(1, std::memory_order_relaxed);

                return std::shared_ptr<T>(base.get(), [base, &slot](T*) mutable {
                    slot.scoped_refs.fetch_sub(1, std::memory_order_relaxed);
                    base.reset(); // drop alias’s keep-alive
                });
            }
            return std::static_pointer_cast<T>(slot.obj);
        }
        // janitor
        void janitor_loop();
        void sweep();

        // state
        std::unordered_map<Key, Slot, KeyHash> map_;
        mutable boost::shared_mutex mtx_;
        std::jthread janitor_;
        std::atomic<bool> stop_{false};
    };

    // ────────────────────────────────────────────────────────────────
    //                        TEMPLATE  DEFINITIONS
    // ────────────────────────────────────────────────────────────────

    template <class T, class Factory>
    void Nexus::register_factory(Factory&& f, const Lifetime lt, const std::uint32_t id) {
        boost::unique_lock lk{mtx_};
        Slot& s   = map_[Key{typeid(T), id}];
        s.obj     = nullptr; // lazy
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

        /* ① shared lock – fast path */
        boost::upgrade_lock r_lock{mtx_};
        auto it = map_.find(k);
        if (it == map_.end()) {
            throw std::runtime_error("Nexus::spawn – not registered");
        }
        auto& sl = it->second;
        if (sl.obj) {
            return build_handle<T>(sl); // ← NEW helper inline below
        }


        boost::upgrade_to_unique_lock uniqueLock{r_lock};
        it = map_.find(k); // re-find after lock upgrade
        if (it == map_.end()) {
            throw std::runtime_error("Nexus::spawn – not registered (raced)");
        }

        if (!sl.obj) {
            sl.obj        = sl.factory(*this);
            sl.last_touch = std::chrono::steady_clock::now();
        }
        return build_handle<T>(sl); // handle with proper deleter
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
        if (!std::holds_alternative<Flex>(it->second.lt)) {
            throw std::runtime_error("Nexus::reset – only Flex lifetime can be reset");
        }
        map_.erase(it);
    }

    inline std::size_t Nexus::size() const noexcept {
        boost::shared_lock lk{mtx_};
        return map_.size();
    }

} // namespace demiplane::nexus
