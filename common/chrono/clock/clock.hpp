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
        [[nodiscard]] static std::chrono::time_point<std::chrono::system_clock> now() noexcept {
            return std::chrono::system_clock::now();
        }
    };
    namespace clock_formats {
        constexpr auto ymd_hms     = "%Y-%m-%d %H:%M:%S";
        constexpr auto dmy_hms     = "%d-%m-%Y %H:%M:%S";
        constexpr auto mdy_hms     = "%m-%d-%Y %H:%M:%S";
    } // namespace clock_formats
    enum class ClockType {
        Local,
        UTC
    };
    template <ClockType CT>
    class AClock : public Clock {
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
            if (CT == ClockType::Local) {
                if (!localtime_r(&tt, &buf)) {
                    throw std::runtime_error("Failed to format local time");
                }
            } else {
                if (!gmtime_r(&tt, &buf)) {
                    throw std::runtime_error("Failed to format UTC time");
                }
            }

            ss << std::put_time(&buf, format.data());
            return ss.str();
        }
        [[nodiscard]] static std::string format_time(
            const std::chrono::time_point<std::chrono::system_clock> tt, const std::string_view format) {
            return format_time(std::chrono::system_clock::to_time_t(tt), format);
        }
        [[nodiscard]] static std::string format_time_iso8601(
            const std::chrono::time_point<std::chrono::system_clock> time_point, const std::string_view format) {
            const auto ms     = duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()) % 1000;

            std::string iso = format_time(std::chrono::system_clock::to_time_t(time_point), format);
            // Append ".mmm"
            iso += '.';
            iso += static_cast<char>('0' + (ms.count() / 100) % 10);
            iso += static_cast<char>('0' + (ms.count() / 10) % 10);
            iso += static_cast<char>('0' + (ms.count()) % 10);
            if (CT == ClockType::UTC) {
                // Append "Z"
                iso += 'Z';
            }
            return iso;
        }

    };


    using LocalClock = AClock<ClockType::Local>;
    using UTCClock   = AClock<ClockType::UTC>;



} // namespace demiplane::chrono
