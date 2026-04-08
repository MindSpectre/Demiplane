#pragma once

#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>

#include <config_interface.hpp>
#include <json/json.hpp>


namespace demiplane::db::postgres {

    /**
     * @brief Configuration for the connection cylinder
     *
     * Controls cylinder sizing, connection timeouts, health check intervals,
     * and cleanup behavior. Capacity must be a power of 2 for efficient
     * ring buffer masking.
     *
     * Usage:
     *   auto cfg = CylinderConfig::Builder{}
     *       .capacity(16)
     *       .min_connections(2)
     *       .connect_timeout(std::chrono::seconds{5})
     *       .finalize();
     */
    class CylinderConfig final : public serialization::ConfigInterface<CylinderConfig, Json::Value> {
    public:
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

        // ============== Factory Methods (defined after Builder) ==============

        [[nodiscard]] static CylinderConfig minimal();
        [[nodiscard]] static CylinderConfig standard();
        [[nodiscard]] static CylinderConfig high_performance();

        // ============== Field Descriptors ==============

        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&CylinderConfig::capacity_, "capacity">{},
                serialization::Field<&CylinderConfig::min_connections_, "min_connections">{},
                serialization::Field<&CylinderConfig::connect_timeout_, "connect_timeout">{},
                serialization::Field<&CylinderConfig::idle_timeout_, "idle_timeout">{},
                serialization::Field<&CylinderConfig::health_check_interval_, "health_check_interval">{},
                serialization::Field<&CylinderConfig::max_lifetime_, "max_lifetime">{},
                serialization::Field<&CylinderConfig::cleanup_sql_, "cleanup_sql">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr CylinderConfig() = default;

        std::size_t capacity_        = 16;
        std::size_t min_connections_ = 2;

        std::chrono::seconds connect_timeout_       = std::chrono::seconds{10};
        std::chrono::seconds idle_timeout_          = std::chrono::seconds{300};
        std::chrono::seconds health_check_interval_ = std::chrono::seconds{30};
        std::chrono::seconds max_lifetime_          = std::chrono::seconds{3600};

        std::string cleanup_sql_ = "DISCARD ALL";
    };

    class CylinderConfig::Builder {
    public:
        Builder() = default;
        explicit Builder(const CylinderConfig& existing)
            : config_{existing} {
        }
        explicit Builder(CylinderConfig&& existing)
            : config_{std::move(existing)} {
        }

        template <typename Self>
        constexpr auto&& capacity(this Self&& self, std::size_t value) noexcept {
            self.config_.capacity_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& min_connections(this Self&& self, std::size_t value) noexcept {
            self.config_.min_connections_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& connect_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.connect_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& idle_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.idle_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& health_check_interval(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.health_check_interval_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& max_lifetime(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.max_lifetime_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& cleanup_sql(this Self&& self, std::string value) noexcept {
            self.config_.cleanup_sql_ = std::move(value);
            return std::forward<Self>(self);
        }

        [[nodiscard]] CylinderConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class CylinderConfig;
        friend class ConfigInterface;
        CylinderConfig config_;
    };

    // ============== CylinderConfig Factory Method Definitions ==============

    inline CylinderConfig CylinderConfig::minimal() {
        return Builder{}.capacity(2).min_connections(1).finalize();
    }

    inline CylinderConfig CylinderConfig::standard() {
        return Builder{}.finalize();
    }

    inline CylinderConfig CylinderConfig::high_performance() {
        return Builder{}.capacity(64).min_connections(8).health_check_interval(std::chrono::seconds{15}).finalize();
    }

}  // namespace demiplane::db::postgres
