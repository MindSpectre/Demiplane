#pragma once

#include <memory>

namespace demiplane::nexus {

    /// @brief We guarantee pylon won't outlive nexus, so we can destroy only there
    template <class T>
    class pylon {

    public:
        pylon() = default;
        explicit pylon(std::shared_ptr<T> sp) noexcept : object_ptr_{std::move(sp)} {}
        explicit pylon(T* sp) noexcept : object_ptr_{sp} {}
        explicit pylon(const T* sp) noexcept : object_ptr_{const_cast<T*>(sp)} {}
        // доступ
        T* operator->() noexcept {
            return object_ptr_;
        }
        T& operator*() noexcept {
            return *object_ptr_;
        }
        const T* operator->() const noexcept {
            return object_ptr_;
        }
        const T& operator*() const noexcept {
            return *object_ptr_;
        }

        explicit operator bool() const noexcept {
            return static_cast<bool>(object_ptr_);
        }
    private:
        T* object_ptr_;
    };

} // namespace dp
