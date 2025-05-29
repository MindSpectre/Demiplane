#pragma once

#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <utility>

namespace demiplane::chrono {

    class LWStopwatch {
    public:
        using clock      = std::chrono::high_resolution_clock;
        using time_point = clock::time_point;
        using duration   = std::chrono::milliseconds;

        explicit LWStopwatch(std::string name = "", std::size_t reserve_flags = 20);

        void start() noexcept;

        // Returns: pair { delta_since_prev, delta_since_start }
        std::pair<duration, duration> flag(std::string name = {});

        static constexpr const char* name() {
            return "Lightweight Stopwatch";
        }

    private:
        struct Flag {
            std::string name;
            time_point time;

            Flag(std::string name, time_point time) : name(std::move(name)), time(time) {}
        };

        std::string name_;
        time_point start_time_;
        std::vector<Flag> flags_;

        static constexpr time_point now() noexcept {
            return clock::now();
        }
    };


} // namespace demiplane::chrono
