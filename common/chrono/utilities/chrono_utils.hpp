#pragma once

#include <chrono>
#include <thread>

namespace demiplane::chrono {
    template <typename DurationClass>
        requires std::chrono::__is_duration_v<DurationClass>
    void sleep_for(const DurationClass duration) {
        std::this_thread::sleep_for(duration);
    }
} // namespace demiplane::chrono
