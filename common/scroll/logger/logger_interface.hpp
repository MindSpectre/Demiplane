#pragma once

#include <demiplane/nexus>
#include <source_location>
#include <string_view>

#include "../entry/entry_interface.hpp"
#include "../entry/log_level.hpp"


namespace demiplane::scroll {
    class Logger {
        public:
        NEXUS_REGISTER(0x8F8CA6F5, nexus::Immortal);  // CRC32/ISO-HDLC of demiplane::scroll::Logger

        virtual ~Logger() = default;

        virtual void log(LogLevel lvl, std::string_view msg, const std::source_location& loc) = 0;
    };
}  // namespace demiplane::scroll
