#pragma once

#include <ctime>
#include <span>
#include <string_view>

namespace demiplane::http {

    /// "Sun, 06 Nov 1994 08:49:37 GMT" — IMF-fixdate is fixed-width.
    inline constexpr std::size_t IMF_FIXDATE_LEN = 29;

    /// Render `t` as an RFC 9110 §5.6.7 IMF-fixdate into `out` (NUL-terminated).
    /// Day/month names come from fixed English tables, never strftime("%a"/"%b"),
    /// which honor the process's global LC_TIME locale — a reusable server must
    /// not emit a locale-dependent (non-compliant) Date header.
    /// Returns the rendered length (always IMF_FIXDATE_LEN).
    std::size_t format_imf_fixdate(std::time_t t, std::span<char, IMF_FIXDATE_LEN + 1> out) noexcept;

    /// The current wall-clock second as IMF-fixdate, rebuilt at most once per
    /// second per thread (thread_local cache — no synchronization on the
    /// response hot path). The view points into thread-local storage: copy it
    /// before the next call on the same thread if it must outlive one.
    [[nodiscard]] std::string_view imf_fixdate_now() noexcept;

}  // namespace demiplane::http
