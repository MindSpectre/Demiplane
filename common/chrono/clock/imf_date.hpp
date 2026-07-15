#pragma once

#include <cstddef>
#include <cstdio>
#include <ctime>
#include <span>

namespace demiplane::chrono {

    /// "Sun, 06 Nov 1994 08:49:37 GMT" — RFC 9110 §5.6.7 IMF-fixdate is fixed-width.
    inline constexpr std::size_t IMF_FIXDATE_LEN = 29;

    /// Render `t` as an RFC 9110 §5.6.7 IMF-fixdate into `out` (NUL-terminated).
    /// Day/month names come from fixed English tables, never strftime("%a"/"%b"),
    /// which honor the process's global LC_TIME locale — an RFC-compliant date
    /// (e.g. an HTTP Date header) must not be locale-dependent.
    /// Returns the rendered length (always IMF_FIXDATE_LEN).
    inline std::size_t format_imf_fixdate(const std::time_t t,
                                          const std::span<char, IMF_FIXDATE_LEN + 1> out) noexcept {
        static constexpr const char* days[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        static constexpr const char* months[] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        std::tm tm{};
        gmtime_r(&t, &tm);
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

}  // namespace demiplane::chrono
