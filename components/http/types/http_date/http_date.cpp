#include "http_date.hpp"

#include <cstdio>

namespace demiplane::http {

    std::size_t format_imf_fixdate(const std::time_t t, const std::span<char, IMF_FIXDATE_LEN + 1> out) noexcept {
        static constexpr const char* days[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        static constexpr const char* months[] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        std::tm tm{};
        ::gmtime_r(&t, &tm);
        const int n = std::snprintf(out.data(),
                                    out.size(),
                                    "%s, %02d %s %04d %02d:%02d:%02d GMT",
                                    days[tm.tm_wday],
                                    tm.tm_mday,
                                    months[tm.tm_mon],
                                    tm.tm_year + 1900,
                                    tm.tm_hour,
                                    tm.tm_min,
                                    tm.tm_sec);
        return n > 0 ? static_cast<std::size_t>(n) : 0;
    }

    std::string_view imf_fixdate_now() noexcept {
        // `!=` rather than `<`: an NTP step backwards must still rebuild.
        thread_local std::time_t cached_second = -1;
        thread_local char buf[IMF_FIXDATE_LEN + 1];
        thread_local std::size_t len = 0;
        if (const std::time_t now = std::time(nullptr); now != cached_second) {
            len           = format_imf_fixdate(now, std::span<char, IMF_FIXDATE_LEN + 1>{buf});
            cached_second = now;
        }
        return {buf, len};
    }

}  // namespace demiplane::http
