#pragma once
#include <chrono>
#include <format>
#include <string>
#include <string_view>

namespace demiplane::chrono {

    // ─────────────── Base clock (parse & now) ───────────────
    class Clock {
    public:
        using sys_tp = std::chrono::time_point<std::chrono::system_clock>;

        [[nodiscard]] static sys_tp now() noexcept {
            return std::chrono::system_clock::now();
        }

        template <class Dur = std::chrono::milliseconds>
        [[nodiscard]] static std::chrono::time_point<std::chrono::system_clock, Dur> parse(
            const std::string_view txt, std::string_view fmt) {
            using namespace std::chrono;
            std::istringstream is(std::string{txt});
            sys_time<Dur> tp;
            is >> parse(fmt.data(), tp);
            if (is.fail()) {
                throw std::invalid_argument(
                    "Invalid time format " + std::string{fmt} + " or value:" + std::string{txt});
            }
            return tp;
        }
    };

    // ─────────────── Canonical patterns ───────────────
    namespace clock_formats {
        constexpr auto eu_dmy_hms = "%d-%m-%Y %H:%M:%S";
        constexpr auto us_mdy_hms = "%m-%d-%Y %I:%M:%S %p";
        constexpr auto iso8601    = "%Y-%m-%dT%H:%M:%S";
    } // namespace clock_formats

    // ─────────────── Local / UTC clocks ───────────────
    enum class ClockType { Local, UTC };

    template <ClockType CT>
    class SpecClock : public Clock {


    public:
        // --- PUBLIC API -------------------------------------------------
        [[nodiscard]] static std::string current_time(const std::string_view fmt) {
            return format_time(now(), fmt);
        }
        static std::string format_time_iso_ms(const sys_tp tp) {
            const auto ms    = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
            std::string base = format_time(tp, clock_formats::iso8601);
            base += std::format(".{:03}", ms.count());
            if constexpr (CT == ClockType::UTC) {
                base += 'Z';
            }
            return base;
        }
        [[nodiscard]] static std::string format_time(const sys_tp tp, const std::string_view format) {
            std::ostringstream ss;
            const std::tm buf = to_tm(tp);
            ss << std::put_time(&buf, format.data());
            return ss.str();
        }
        [[nodiscard]] static std::tm to_tm(const sys_tp tp) {
            const std::time_t tt = std::chrono::system_clock::to_time_t(tp);
            std::tm out{};
            if constexpr (CT == ClockType::Local) {
                localtime_r(&tt, &out);
            } else {
                gmtime_r(&tt, &out);
            }
            return out;
        }
    };

    using LocalClock = SpecClock<ClockType::Local>;
    using UTCClock   = SpecClock<ClockType::UTC>;

} // namespace demiplane::chrono
