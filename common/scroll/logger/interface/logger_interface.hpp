#pragma once

#include <demiplane/nexus>
#include <source_location>
#include <string_view>

#include "../entry_interface.hpp"
#include "../log_level.hpp"


namespace demiplane::scroll {
    class Logger {
    public:
        NEXUS_REGISTER(nexus::Immortal);

        virtual ~Logger() = default;

        virtual void log(LogLevel lvl, std::string_view msg, const std::source_location& loc) = 0;
    };
}  // namespace demiplane::scroll
