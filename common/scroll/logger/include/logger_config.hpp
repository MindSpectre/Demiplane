#pragma once

#include <thread>

#include <config_interface.hpp>
#include <json/json.hpp>
namespace demiplane::scroll {

    class LoggerConfig final : public serialization::ConfigInterface<LoggerConfig, Json::Value> {
    public:
        enum class WaitStrategy {
            BusySpin,  // Lowest latency
            Yielding,  // Balanced
            Blocking   // Lowest CPU
        };

        struct BufferCapacity {
            static constexpr std::size_t Small  = 1024;
            static constexpr std::size_t Medium = 8192;
            static constexpr std::size_t Large  = 65536;
            static constexpr std::size_t Huge   = 131072;
        };

        // Full constructor (escape hatch)
        constexpr LoggerConfig(const std::size_t ring_buffer_size,
                               const std::size_t pool_size,
                               const WaitStrategy wait_strategy)
            : ring_buffer_size_{ring_buffer_size},
              pool_size_{pool_size},
              wait_strategy_{wait_strategy} {
        }

        constexpr void validate() const override {
            if (std::popcount(ring_buffer_size_) != 1) {
                throw std::invalid_argument("Ring buffer size must be a power of 2");
            }
        }

        [[nodiscard]] constexpr std::size_t ring_buffer_size() const noexcept {
            return ring_buffer_size_;
        }

        [[nodiscard]] constexpr WaitStrategy wait_strategy() const noexcept {
            return wait_strategy_;
        }

        [[nodiscard]] constexpr std::size_t pool_size() const noexcept {
            return pool_size_;
        }

        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&LoggerConfig::ring_buffer_size_, "ring_buffer_size">{},
                serialization::Field<&LoggerConfig::pool_size_, "pool_size">{},
                serialization::Field<&LoggerConfig::wait_strategy_, "wait_strategy">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr LoggerConfig() = default;

        std::size_t ring_buffer_size_ = BufferCapacity::Medium;
        std::size_t pool_size_        = std::thread::hardware_concurrency();
        WaitStrategy wait_strategy_   = WaitStrategy::Yielding;
    };

    class LoggerConfig::Builder {
    public:
        Builder() = default;
        explicit Builder(const LoggerConfig& existing)
            : config_{existing} {
        }
        explicit Builder(LoggerConfig&& existing)
            : config_{std::move(existing)} {
        }

        template <typename Self>
        constexpr auto&& ring_buffer_size(this Self&& self, const std::size_t value) noexcept {
            self.config_.ring_buffer_size_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& wait_strategy(this Self&& self, const WaitStrategy value) noexcept {
            self.config_.wait_strategy_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& pool_size(this Self&& self, const std::size_t value) noexcept {
            self.config_.pool_size_ = value;
            return std::forward<Self>(self);
        }

        [[nodiscard]] LoggerConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class LoggerConfig;
        friend class ConfigInterface;
        LoggerConfig config_;
    };

}  // namespace demiplane::scroll
