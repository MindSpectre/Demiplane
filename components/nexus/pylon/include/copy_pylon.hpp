#pragma once

#include <memory>

namespace demiplane::nexus {

    /// @brief We guarantee pylon won't outlive nexus, so we can destroy only there
    template <class T>
    class copy_pylon {

    public:
        explicit copy_pylon(T sp) {
            object_ = new T(std::move(sp));
        }
        ~copy_pylon() {
            delete object_;
        }
        // доступ
        T* operator->() noexcept {
            return object_;
        }
        T& operator*() noexcept {
            return *object_;
        }
        const T* operator->() const noexcept {
            return object_;
        }
        const T& operator*() const noexcept {
            return *object_;
        }

        explicit operator bool() const noexcept {
            return static_cast<bool>(object_);
        }

    private:
        T* object_;
    };

} // namespace demiplane::nexus
