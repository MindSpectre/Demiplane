#pragma once
#include <demiplane/gears>
namespace demiplane::nexus {
    template <class T>
    class unique_pylon : gears::NonCopyable {
        std::shared_ptr<T> object_ptr_;

    public:
        unique_pylon() = default;
        explicit unique_pylon(std::shared_ptr<T> sp) {
            auto* raw = sp.get();
            std::lock_guard lk(detail::unique_guard_mtx);
            if(detail::unique_counts[raw] > 0) {
                throw std::runtime_error("unique_pylon: instance already exists for this object");
            }
            detail::unique_counts[raw] = 1;
            object_ptr_ = std::move(sp);
        }
        ~unique_pylon() {
            if(object_ptr_) {
                auto* raw = object_ptr_.get();
                std::lock_guard lk(detail::unique_guard_mtx);
                detail::unique_counts.erase(raw); //TODO: follow IDs
            }
        }
        T* operator->() noexcept {
            return object_ptr_.get();
        }
        T& operator*() noexcept {
            return *object_ptr_;
        }
        const T* operator->() const noexcept {
            return object_ptr_.get();
        }
        const T& operator*() const noexcept {
            return *object_ptr_;
        }

        explicit operator bool() const noexcept {
            return static_cast<bool>(object_ptr_);
        }
    };
} // namespace demiplane::nexus
