#pragma once
#include <chrono>
#include <random>
#include <stdexcept>
namespace demiplane::chrono::utilities {

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
