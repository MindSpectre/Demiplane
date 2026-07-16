#pragma once

#include <string_view>

namespace demiplane::http {

    /// The current wall-clock second as an RFC 9110 IMF-fixdate, rebuilt at most
    /// once per second per thread (thread_local cache — no synchronization on the
    /// response hot path). The rendering itself lives in `demiplane::chrono`
    /// (`format_imf_fixdate`); this wrapper adds only the per-second caching that
    /// the Date-header hot path needs. The view points into thread-local storage:
    /// copy it before the next call on the same thread if it must outlive one.
    [[nodiscard]] std::string_view imf_fixdate_now() noexcept;

}  // namespace demiplane::http
