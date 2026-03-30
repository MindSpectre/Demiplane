#pragma once

#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>

#include <gears_class_traits.hpp>
#include <json/value.h>

namespace demiplane::db::postgres {

    /**
     * @brief Configuration for the connection cylinder
     *
     * Controls cylinder sizing, connection timeouts, health check intervals,
     * and cleanup behavior. Capacity must be a power of 2 for efficient
     * ring buffer masking.
     *
     * Usage:
     *   auto cfg = CylinderConfig{}
     *       .capacity(16)
     *       .min_connections(2)
     *       .connect_timeout(std::chrono::seconds{5})
     *       .finalize();
     */
    class CylinderConfig final : public gears::ConfigInterface<CylinderConfig, Json::Value> {
    public:
        constexpr CylinderConfig() = default;

        // ============== ConfigInterface Implementation ==============

        constexpr void validate() const override {
            if (capacity_ == 0) {
                throw std::invalid_argument("Cylinder capacity must be greater than 0");
            }
            if ((capacity_ & (capacity_ - 1)) != 0) {
                throw std::invalid_argument("Cylinder capacity must be a power of 2");
            }
            if (min_connections_ > capacity_) {
                throw std::invalid_argument("min_connections cannot exceed capacity");
            }
            if (connect_timeout_.count() <= 0) {
                throw std::invalid_argument("connect_timeout must be positive");
            }
        }

        // ============== Fluent Setters ==============

        template <typename Self>
        constexpr auto&& capacity(this Self&& self, std::size_t value) noexcept {
            self.capacity_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& min_connections(this Self&& self, std::size_t value) noexcept {
            self.min_connections_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& connect_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.connect_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& idle_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.idle_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& health_check_interval(this Self&& self, std::chrono::seconds value) noexcept {
            self.health_check_interval_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& max_lifetime(this Self&& self, std::chrono::seconds value) noexcept {
            self.max_lifetime_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& cleanup_sql(this Self&& self, std::string value) noexcept {
            self.cleanup_sql_ = std::move(value);
            return std::forward<Self>(self);
        }

        // ============== Getters ==============

        [[nodiscard]] constexpr std::size_t capacity() const noexcept {
            return capacity_;
        }
        [[nodiscard]] constexpr std::size_t min_connections() const noexcept {
            return min_connections_;
        }
        [[nodiscard]] constexpr std::chrono::seconds connect_timeout() const noexcept {
            return connect_timeout_;
        }
        [[nodiscard]] constexpr std::chrono::seconds idle_timeout() const noexcept {
            return idle_timeout_;
        }
        [[nodiscard]] constexpr std::chrono::seconds health_check_interval() const noexcept {
            return health_check_interval_;
        }
        [[nodiscard]] constexpr std::chrono::seconds max_lifetime() const noexcept {
            return max_lifetime_;
        }
        [[nodiscard]] constexpr const char* cleanup_sql() const noexcept {
            return cleanup_sql_.c_str();
        }

        // ============== Factory Methods ==============

        [[nodiscard]] static CylinderConfig minimal() {
            return CylinderConfig{}.capacity(2).min_connections(1);
        }

        [[nodiscard]] static CylinderConfig standard() {
            return CylinderConfig{};
        }

        [[nodiscard]] static CylinderConfig high_performance() {
            return CylinderConfig{}.capacity(64).min_connections(8).health_check_interval(std::chrono::seconds{15});
        }

    protected:
        [[nodiscard]] Json::Value wrapped_serialize() const override;
        void wrapped_deserialize(const Json::Value& config) override;

    private:
        std::size_t capacity_        = 16;
        std::size_t min_connections_ = 2;

        std::chrono::seconds connect_timeout_       = std::chrono::seconds{10};
        std::chrono::seconds idle_timeout_          = std::chrono::seconds{300};
        std::chrono::seconds health_check_interval_ = std::chrono::seconds{30};
        std::chrono::seconds max_lifetime_          = std::chrono::seconds{3600};

        std::string cleanup_sql_ = "DISCARD ALL";
    };

}  // namespace demiplane::db::postgres
