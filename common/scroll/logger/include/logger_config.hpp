#pragma once

#include <gears_class_traits.hpp>

#include <json/value.h>

namespace demiplane::scroll {
    /**
     * @brief Logger configuration
     */
    class LoggerConfig final : public gears::ConfigInterface<LoggerConfig, Json::Value> {
    public:
        void validate() override {
            if (std::popcount(ring_buffer_size_)) {
                throw std::invalid_argument("Ring buffer size must be a power of 2");
            }
        }
        [[nodiscard]] Json::Value serialize() const override {
            throw std::logic_error("Not implemented");
        }
        static LoggerConfig deserialize(const Json::Value& config) {
            gears::unused_value(config);
            std::unreachable();
        }
        /// @brief Ring buffer size (must be power of 2)

        /// @brief Wait strategy for consumer thread
        enum class WaitStrategy {
            BusySpin,  // Lowest latency
            Yielding,  // Balanced
            Blocking   // Lowest CPU
        };

        struct BufferCapacity {
            static constexpr std::size_t Small = 1024;
            static constexpr std::size_t Medium = 8192;
            static constexpr std::size_t Large = 65536;
            static constexpr std::size_t Huge = 131072;
        };

        LoggerConfig& ring_buffer_size(const std::size_t ring_buffer_size) {
            ring_buffer_size_ = ring_buffer_size;
            return *this;
        }

        template <WaitStrategy WaitStrategyT>
        LoggerConfig& wait_strategy() {
            wait_strategy_ = WaitStrategyT;
            return *this;
        }

        [[nodiscard]] std::size_t get_ring_buffer_size() const {
            return ring_buffer_size_;
        }

        [[nodiscard]] WaitStrategy get_wait_strategy() const {
            return wait_strategy_;
        }

    private:
        std::size_t ring_buffer_size_ = BufferCapacity::Medium;

        WaitStrategy wait_strategy_ = WaitStrategy::Yielding;
    };
}  // namespace demiplane::scroll
