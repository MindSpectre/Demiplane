#pragma once

#include <sstream>
#include <string>

#include "../entry_interface.hpp"

namespace demiplane::scroll {
    class DetailedEntry final : public Entry {
    public:
        DetailedEntry(const LogLevel level, const std::string_view message, const std::string_view file,
            const uint32_t line, const std::string_view function)
            : Entry{level, message, file, line, function} {}

        [[nodiscard]] std::string to_string() const override {
            std::ostringstream formatter;
            formatter << "[" << scroll::to_string(level_) << "] "
                      << "[" << file_ << ":" << line_ << " " << function_ << "] " << message_ << "\n";
            return formatter.str();
        }
    };
} // namespace demiplane::scroll
