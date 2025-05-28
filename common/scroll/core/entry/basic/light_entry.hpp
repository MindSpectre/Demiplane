#pragma once

#include <string>

#include "../entry_interface.hpp"

namespace demiplane::scroll {
    class LightEntry final : public Entry {
    public:
        LightEntry(LogLevel level, const std::string_view& message, const std::string_view& file, uint32_t line,
            const std::string_view& function)
            : Entry(level, message, file, line, function) {}
        [[nodiscard]] std::string to_string() const override {
            return std::string("[") + scroll::to_string(level_) + "] " + std::string(message_);
        }

    };
#define LIGHT_LOG_ENTRY(Level, Message) demiplane::scroll::LightEntry(Level, Message, __FILE__, __LINE__, __FUNCTION__)
}