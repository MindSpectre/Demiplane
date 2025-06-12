#pragma once

#include <sstream>
#include <string>

#include "../entry_interface.hpp"

namespace demiplane::scroll {
    template <class Service>
    // TODO: add has static name concept
    class ServiceEntry final : public Entry {
    public:
        ServiceEntry(const LogLevel level, const std::string_view& message, const std::string_view& file, uint32_t line,
            const std::string_view& function)
            : Entry(level, message, file, line, function) {}

        [[nodiscard]] std::string to_string() const override {
            std::ostringstream formatter;
            formatter << "[" << scroll::to_string(level_) << "] [" << Service::name() << "] [" << file_ << ":" << line_
                      << " " << function_ << "] " << message_ << "\n";
            return formatter.str();
        }
    };
} // namespace demiplane::scroll
