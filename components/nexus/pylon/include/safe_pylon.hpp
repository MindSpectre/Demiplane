#pragma once

#include "basic_pylon.hpp"

namespace demiplane::nexus {
    template <typename T>
    /**
     * @brief A safe wrapper around weak_ptr providing controlled access to shared resources
     * @tparam T The type of the managed object
     */
    class safe_pylon {
    public:
        // Type aliases for better readability
        using optional_pylon = std::optional<pylon<T>>;

        // Constructors
        safe_pylon() = default;
        explicit safe_pylon(const std::shared_ptr<T>& sp) noexcept : object_weak_ptr_{sp} {}
        explicit safe_pylon(std::shared_ptr<T>&& sp) noexcept : object_weak_ptr_{std::move(sp)} {}

        // Move operations
        safe_pylon(safe_pylon&&) noexcept            = default;
        safe_pylon& operator=(safe_pylon&&) noexcept = default;

        /**
         * @brief Attempts to acquire a shared_ptr wrapper
         * @return Optional containing pylon if the pointer is valid, nullopt if expired
         */
        [[nodiscard]] optional_pylon lock() const noexcept {
            if (auto sp = object_weak_ptr_.lock()) {
                return pylon<T>{std::move(sp)};
            }
            return std::nullopt;
        }

        /**
         * @brief Checks if the contained pointer is valid
         * @return true if the pointer is valid, false if expired
         */
        explicit operator bool() const noexcept {
            return !object_weak_ptr_.expired();
        }

        /**
         * @brief Checks if two safe_pylons point to the same object
         */
        bool operator==(const safe_pylon& other) const noexcept {
            return !object_weak_ptr_.owner_before(other.object_weak_ptr_) && !other.object_weak_ptr_.owner_before(object_weak_ptr_);
        }

    private:
        std::weak_ptr<T> object_weak_ptr_;
    };

} // namespace demiplane::nexus
