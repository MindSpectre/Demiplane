#include "lw_stopwatch.hpp"
#include "testing_stopwatch.hpp"
using namespace demiplane::chrono;
inline LWStopwatch::LWStopwatch(std::string name, std::size_t reserve_flags) : name_(std::move(name)) {
    flags_.reserve(reserve_flags);
    start_time_ = now();
}
inline void LWStopwatch::start() noexcept {
    flags_.clear();
    start_time_ = now();
}
inline std::pair<LWStopwatch::duration, LWStopwatch::duration> LWStopwatch::flag(std::string name) {
    auto now_time    = now();
    auto since_start = std::chrono::duration_cast<duration>(now_time - start_time_);
    auto since_prev =
        flags_.empty() ? since_start : std::chrono::duration_cast<duration>(now_time - flags_.back().time);

    flags_.emplace_back(std::move(name), now_time);
    return {since_prev, since_start};
}
