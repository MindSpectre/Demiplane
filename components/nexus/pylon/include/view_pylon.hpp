#pragma once

namespace demiplane::nexus {
    template <class T>
    class view_pylon {
    public:
        view_pylon() = default;
        explicit view_pylon(const std::shared_ptr<T>& sp) noexcept : object_ptr_{sp.get()} {}

        const T* operator->() const noexcept {
            return object_ptr_;
        }
        const T& operator*() const noexcept {
            return *object_ptr_;
        }

        explicit operator bool() const noexcept {
            return object_ptr_;
        }

    private:
        const T* object_ptr_{nullptr};
    };
} // namespace demiplane::nexus
