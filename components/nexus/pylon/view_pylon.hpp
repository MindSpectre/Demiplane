#pragma once

namespace demiplane::nexus {
    template <class T>
    class view_pylon {
        const T* ptr_{nullptr};

    public:
        view_pylon() = default;
        explicit view_pylon(const std::shared_ptr<T>& sp) noexcept : ptr_{sp.get()} {}

        const T* operator->() const noexcept {
            return ptr_;
        }
        const T& operator*() const noexcept {
            return *ptr_;
        }

        explicit operator bool() const noexcept {
            return ptr_;
        }
    };
} // namespace demiplane::nexus
