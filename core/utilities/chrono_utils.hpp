#pragma once
#include <chrono>
#include <random>
#include <stdexcept>
namespace demiplane::utilities::chrono {
    class RandomTimeGenerator {
    public:
        RandomTimeGenerator() = default;
        /// @brief Generate a random time in range [ms * (100 - pc) / 100, ms * (100 + pc) / 100]. It means generating
        /// around specific time eg (100ms) and add random for this.
        /// @param target_ms The base time in milliseconds.
        /// @param deviation The percentage variation allowed. Default value is 15%
        /// @return A random time in milliseconds.
        /// @throws std::invalid_argument if pc < 0 or ms < 0.
        static std::chrono::milliseconds generate_milliseconds(const int32_t target_ms, const int32_t deviation = 15) {
            std::mt19937 gen(std::random_device{}()); // Mersenne Twister random number generator
            if (target_ms < 0 || deviation < 0 || deviation > 100) {
                throw std::invalid_argument("Both ms and pc must be non-negative.");
            }

            // Calculate bounds
            const auto lower_bound = static_cast<int32_t>(target_ms * (100.0 - deviation) / 100.0);
            const auto upper_bound = static_cast<int32_t>(target_ms * (100.0 + deviation) / 100.0);
            if (lower_bound > upper_bound) {
                throw std::invalid_argument("Lower bound must be less than upper bound.");
            }
            // Generate random value in range
            std::uniform_int_distribution dist(lower_bound, upper_bound);
            return std::chrono::milliseconds(dist(gen));
        }
    };
    class Clock {
    public:
        [[nodiscard]] static time_t current_time() {
            return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        }
    };
    class LocalClock : public Clock {
    public:
        // Helper to format current time.
        [[nodiscard]] static std::string current_time_ymd() {
            const auto tt = current_time();
            std::ostringstream ss;
            ss << std::put_time(std::localtime(&tt), "%Y-%m-%d %X");
            return ss.str();
        }
        [[nodiscard]] static std::string current_time_dmy() {
            const auto tt = current_time();
            std::ostringstream ss;
            ss << std::put_time(std::localtime(&tt), "%d-%m-%Y %X");
            return ss.str();
        }
        [[nodiscard]] static std::string current_time_custom_fmt(const std::string_view format) {
            const auto tt = current_time();
            std::ostringstream ss;
            ss << std::put_time(std::localtime(&tt), format.data());
            return ss.str();
        }
    };
    class UTCClock : public Clock {
    public:
        // Helper to format current time.
        [[nodiscard]] static std::string current_time_ymd() {
            const auto tt = current_time();
            std::ostringstream ss;
            ss << std::put_time(std::gmtime(&tt), "%Y-%m-%d %X");
            return ss.str();
        }
        [[nodiscard]] static std::string current_time_dmy() {
            const auto tt = current_time();
            std::ostringstream ss;
            ss << std::put_time(std::gmtime(&tt), "%d-%m-%Y %X");
            return ss.str();
        }
        [[nodiscard]] static std::string current_time_custom_fmt(const std::string_view format) {
            const auto tt = current_time();
            std::ostringstream ss;
            ss << std::put_time(std::gmtime(&tt), format.data());
            return ss.str();
        }
    };
} // namespace demiplane::utilities::chrono
