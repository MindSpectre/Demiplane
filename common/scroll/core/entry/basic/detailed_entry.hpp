#pragma once

#include <sstream>
#include <string>

#include "../entry_interface.hpp"

namespace demiplane::scroll {
    class DetailedEntry final : public Entry {
    public:
        DetailedEntry(LogLevel level, const std::string_view& message, const std::string_view& file, uint32_t line,
            const std::string_view& function)
            : Entry(level, message, file, line, function) {}
        [[nodiscard]] std::string to_string() const override {
            std::ostringstream formatter;
            formatter << "[" << scroll::to_string(level_) << "] "
                      << "[" << file_ << ":" << line_ << " " << function_ << "] " << message_;
            return formatter.str();
        }
    };
    #define DETAILED_LOG_ENTRY(Level, Message) demiplane::scroll::DetailedEntry(Level, Message, __FILE__, __LINE__, __FUNCTION__)
} // namespace demiplane::scroll
