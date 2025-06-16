#pragma once

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace demiplane::chrono {

    class Clock {
    public:
        [[nodiscard]] static std::time_t current_time() noexcept {
            return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        }
    };

    namespace clock_formats {
        constexpr auto ymd_hms = "%Y-%m-%d %H:%M:%S";
        constexpr auto dmy_hms = "%d-%m-%Y %H:%M:%S";
        constexpr auto mdy_hms = "%m-%d-%Y %H:%M:%S";
    }

    class LocalClock : public Clock {
    public:
        [[nodiscard]] static std::string current_time_ymd() {
            return current_time_fmt("%Y-%m-%d %H:%M:%S");
        }

        [[nodiscard]] static std::string current_time_dmy() {
            return current_time_fmt("%d-%m-%Y %H:%M:%S");
        }

        [[nodiscard]] static std::string current_time_fmt(const std::string_view format) {
            return format_time(current_time(), format);
        }

        [[nodiscard]] static std::string format_time(const std::time_t tt, const std::string_view format) {
            std::ostringstream ss;
            std::tm buf{};
            if (!localtime_r(&tt, &buf)) {
                throw std::runtime_error("Failed to format local time");
            }
            ss << std::put_time(&buf, format.data());
            return ss.str();
        }
    };

    class UTCClock : public Clock {
    public:
        [[nodiscard]] static std::string current_time_ymd() {
            return current_time_fmt("%Y-%m-%d %H:%M:%S");
        }

        [[nodiscard]] static std::string current_time_dmy() {
            return current_time_fmt("%d-%m-%Y %H:%M:%S");
        }

        [[nodiscard]] static std::string current_time_fmt(const std::string_view format) {
            return format_time(current_time(), format);
        }

        [[nodiscard]] static std::string format_time(const std::time_t tt, const std::string_view format) {
            std::ostringstream ss;
            std::tm buf{};
            if (!gmtime_r(&tt, &buf)) {
                throw std::runtime_error("Failed to format UTC time");
            }
            ss << std::put_time(&buf, format.data());
            return ss.str();
        }
    };

} // namespace demiplane::chrono
