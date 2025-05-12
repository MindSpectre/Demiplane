#pragma once
#include <demiplane/gears>
namespace demiplane::nexus {
    template <class T>
    class unique_pylon : gears::NonCopyable {

    public:
        unique_pylon() = default;
        explicit unique_pylon(std::shared_ptr<T> sp) {
            auto* raw = sp.get();
            // std::lock_guard lk(detail::unique_guard_mtx);
            // if(detail::unique_counts[raw] > 0) {
            //     throw std::runtime_error("unique_pylon: instance already exists for this object");
            // }
            // detail::unique_counts[raw] = 1;
            holden_object_ = std::move(sp);
        }
        ~unique_pylon() {
            if(holden_object_) {
                auto* raw = holden_object_.get();
                // std::lock_guard lk(detail::unique_guard_mtx);
                // detail::unique_counts.erase(raw); //TODO: follow IDs
            }
        }
        T* operator->() noexcept {
            return holden_object_.get();
        }
        T& operator*() noexcept {
            return *holden_object_;
        }
        const T* operator->() const noexcept {
            return holden_object_.get();
        }
        const T& operator*() const noexcept {
            return *holden_object_;
        }

        explicit operator bool() const noexcept {
            return static_cast<bool>(holden_object_);
        }
    private:
        std::shared_ptr<T> holden_object_;
    };
} // namespace demiplane::nexus
