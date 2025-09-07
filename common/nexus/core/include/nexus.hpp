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

/**
 * @namespace demiplane::nexus
 * @brief Service locator framework with lifetime management and thread-safe access
 */
namespace demiplane::nexus {

    /**
     * @class Nexus
     * @brief Thread-safe service locator with configurable lifetime policies
     *
     * @details Nexus provides a centralized registry for managing service instances
     * with support for:
     * - Multiple lifetime policies (Immortal, Resettable, Scoped, Timed)
     * - Thread-safe lazy initialization
     * - Multiple instances of the same type via unique IDs
     * - Automatic cleanup via background janitor thread
     *
     * @note All operations are thread-safe
     */
    class Nexus {
    public:
        /**
         * @brief Constructs the Nexus instance and starts the janitor thread
         * @note The janitor thread periodically cleans up expired services
         */
        Nexus() noexcept;

        /**
         * @brief Destroys the Nexus instance and stops the janitor thread
         * @note Clears all registered services
         */
        ~Nexus() noexcept;

        // ═══════════════════════════════════════════════════════════════════════
        // Registration Methods
        // ═══════════════════════════════════════════════════════════════════════

        /**
         * @brief Registers a singleton factory function
         *
         * @tparam T Service type to register
         * @tparam Factory Callable type that returns std::shared_ptr<T>
         *
         * @param f Factory function that creates the service instance
         * @param lt Lifetime policy (defaults to type's nexus_policy if defined)
         *
         * @details The factory will be invoked lazily on first access via get().
         * Later calls to get() return the same instance (for singleton policies).
         *
         * @note Thread-safe: prevents duplicate construction if called concurrently
         */
        template <class T, typename Factory>
            requires std::is_invocable_v<Factory>
        void register_singleton(Factory&& f, Lifetime lt = get_nexus_policy<T>());

        /**
         * @brief Registers an existing shared_ptr as a singleton
         *
         * @tparam T Service type
         * @param sp Existing shared_ptr to register
         * @param lt Lifetime policy
         *
         * @details Useful when the service is created externally or needs
         * special initialization that can't be done in a factory.
         */
        template <class T>
        void register_singleton(std::shared_ptr<T> sp, Lifetime lt = get_nexus_policy<T>());

        /**
         * @brief Registers a value as a singleton (copies the value)
         *
         * @tparam T Service type (must be copyable)
         * @param value Value to copy and store
         * @param lt Lifetime policy
         */
        template <class T>
            requires std::is_object_v<T>
        void register_singleton(T value, Lifetime lt = get_nexus_policy<T>());

        /**
         * @brief Registers a factory for a specific instance ID
         *
         * @tparam T Service type
         * @tparam Factory Callable returning std::shared_ptr<T>
         *
         * @param f Factory function
         * @param id Unique identifier for this instance
         * @param lt Lifetime policy
         */
        template <class T, typename Factory>
            requires std::is_invocable_v<Factory>
        void register_instance(Factory&& f, nexus_id_t id, Lifetime lt = get_nexus_policy<T>());

        /**
         * @brief Registers an existing shared_ptr with a specific ID
         *
         * @tparam T Service type
         * @param sp Existing shared_ptr
         * @param id Unique identifier
         * @param lt Lifetime policy
         */
        template <class T>
        void register_instance(std::shared_ptr<T> sp, nexus_id_t id, Lifetime lt = get_nexus_policy<T>());

        /**
         * @brief Registers a value with a specific ID
         *
         * @tparam T Service type
         * @param value Value to copy and store
         * @param id Unique identifier
         * @param lt Lifetime policy
         */
        template <class T>
            requires std::is_object_v<T>
        void register_instance(T value, nexus_id_t id, Lifetime lt = get_nexus_policy<T>());

        // ═══════════════════════════════════════════════════════════════════════
        // Access Methods
        // ═══════════════════════════════════════════════════════════════════════

        /**
         * @brief Retrieves a service instance
         *
         * @tparam Interface Service type to retrieve
         * @param id Instance ID (0 for singleton)
         * @return Shared pointer to the service instance
         *
         * @throws std::runtime_error If the service is not registered
         *
         * @details For lazy-initialized services, the factory is called
         * on first access.
         * Thread-safe: prevents duplicate construction.
         *
         * @note The returned shared_ptr extends the service lifetime
         * beyond the Nexus's internal reference for Scoped/Timed policies.
         */
        template <class Interface>
        std::shared_ptr<Interface> get(nexus_id_t id = 0);

        // ═══════════════════════════════════════════════════════════════════════
        // Management Methods
        // ═══════════════════════════════════════════════════════════════════════

        /**
         * @brief Resets a service instance (Resettable lifetime only)
         *
         * @tparam T Service type
         * @param id Instance ID (0 for singleton)
         *
         * @throws std::runtime_error If service isn't found or not Resettable
         *
         * @details Forces recreation on next access. Existing shared_ptrs
         * remain valid but point to the old instance.
         */
        template <class T>
        void reset(nexus_id_t id = 0);

        /**
         * @brief Returns the number of registered services
         * @return Total count of registered services (all types and IDs)
         */
        std::size_t size() const noexcept;

        /**
         * @brief Removes all registered services
         * @note Stops the janitor thread and clears the registry
         */
        void clear() noexcept;

        /**
         * @brief Checks if a service is registered
         *
         * @tparam T Service type
         * @param id Instance ID (0 for singleton)
         * @return true if registered, false otherwise
         */
        template <class T>
        [[nodiscard]] bool has(nexus_id_t id = 0) const noexcept;

        /**
         * @brief Sets the interval for the janitor thread cleanup sweep
         *
         * @param sweep_interval Time between cleanup sweeps
         *
         * @details The janitor thread periodically removes expired services
         * (Scoped with no references, Timed past expiration).
         *
         * @note Default is 5 seconds
         */
        void set_sweep_interval(const std::chrono::seconds sweep_interval) {
            sweep_interval_.store(sweep_interval);
        }

    private:
        using Key     = detail::Key;
        using KeyHash = detail::KeyHash;

        /**
         * @struct Slot
         * @brief Internal storage for a registered service
         */
        struct Slot {
            std::shared_ptr<void> obj;                           ///< Current instance (type-erased)
            std::function<std::shared_ptr<void>()> factory;      ///< Factory function
            Lifetime lt;                                         ///< Lifetime policy
            std::chrono::steady_clock::time_point last_touch{};  ///< Last access time (for Timed)
            std::shared_ptr<std::atomic<bool>> expired_flag;     ///< Expiration flag (for Timed)
            std::mutex construction_mutex;                       ///< Prevents concurrent construction
            std::atomic<bool> constructing{false};               ///< Construction in progress flag
        };

        /**
         * @brief Converts a typed factory to a type-erased factory
         */
        template <class F>
        static auto to_void_factory(F&& f) {
            return [fn = std::forward<F>(f)] { return std::shared_ptr<void>(fn()); };
        }

        /**
         * @brief Builds or retrieves a service instance from a slot
         * @details Handles lazy initialization and lifetime policies
         */
        template <class T>
        std::shared_ptr<T> build_handle(Slot& slot);

        /**
         * @brief Main loop for the janitor thread
         */
        void sweep_loop();

        /**
         * @brief Performs a single cleanup sweep
         * @details Removes expired Scoped and Timed services
         */
        void sweep();

        // State members
        std::unordered_map<Key, Slot, KeyHash> map_;                ///< Service registry
        mutable boost::shared_mutex mtx_;                           ///< Read-write lock for thread safety
        std::jthread janitor_;                                      ///< Background cleanup thread
        std::atomic<bool> stop_                           = false;  ///< Stop flag for janitor
        std::atomic<std::chrono::seconds> sweep_interval_ = std::chrono::seconds(5);  ///< Cleanup interval
    };

    /**
     * @brief Returns the global Nexus instance
     * @return Reference to the singleton Nexus instance
     *
     * @details Thread-safe: uses a Meyers singleton pattern.
     * The instance is created on first access and destroyed at program exit.
     */
    inline Nexus& instance() {
        static Nexus instance;
        return instance;
    }

}  // namespace demiplane::nexus

/**
 * @def NEXUS_REGISTER
 * @brief Macro for declaring a service's default lifetime policy
 *
 * @param Policy Lifetime policy type (Immortal, Resettable, Scoped, Timed)
 *
 * @see demiplane::nexus::Lifetime
 *
 * @details When placed in a class, allows the Nexus to automatically
 * use the specified policy when no explicit policy is provided.
 */
#define NEXUS_REGISTER(Policy) static constexpr Policy nexus_policy


#include "../source/nexus.inl"
