#pragma once

#include <source_location>
#include <string_view>

#include <demiplane/nexus>

#include "../core/entry_interface.hpp"
#include "../core/log_level.hpp"


namespace demiplane::scroll {
    class Logger {
    public:
        NEXUS_REGISTER(0x8F8CA6F5, nexus::Immortal); // CRC32/ISO-HDLC of demiplane::scroll::Logger

        virtual ~Logger() = default;

        virtual void log(LogLevel lvl, std::string_view msg, const detail::MetaSource& loc) = 0;
    };
} // namespace demiplane::scroll
