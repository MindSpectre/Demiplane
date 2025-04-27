#pragma once

namespace demiplane::nexus {
    template <class T>
    class safe_pylon {
        std::weak_ptr<T> wp_;

    public:
        safe_pylon() = default;
        explicit safe_pylon(const std::shared_ptr<T>& sp) noexcept : wp_{sp} {}

        /// @brief Пытается захватить shared_ptr; возвращает nullopt, если expired
        [[nodiscard]] std::optional<pylon<T>> lock() const noexcept {
            if (auto sp = wp_.lock()) {
                return pylon<T>{std::move(sp)};
            }
            return std::nullopt;
        }

        explicit operator bool() const noexcept {
            return !wp_.expired();
        }
    };

} // namespace demiplane::nexus

