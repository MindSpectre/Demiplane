#pragma once

#include <memory>

namespace demiplane::nexus {

    /// @brief
    template <class T>
    class pylon {
        std::shared_ptr<T> sp_;

    public:
        pylon() = default;
        explicit pylon(std::shared_ptr<T> sp) noexcept : sp_{std::move(sp)} {}

        // доступ
        T* operator->() noexcept {
            return sp_.get();
        }
        T& operator*() noexcept {
            return *sp_;
        }
        const T* operator->() const noexcept {
            return sp_.get();
        }
        const T& operator*() const noexcept {
            return *sp_;
        }

        explicit operator bool() const noexcept {
            return static_cast<bool>(sp_);
        }
    };

} // namespace dp
