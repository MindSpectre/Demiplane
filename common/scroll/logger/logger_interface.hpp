#pragma once

#include <source_location>
#include <string_view>

#include "../core/log_level.hpp"


namespace demiplane::scroll {
    class Logger {
    public:
        virtual ~Logger() = default;

        virtual void log(LogLevel lvl, std::string_view msg, std::source_location loc) = 0;
    };
} // namespace demiplane::scroll
