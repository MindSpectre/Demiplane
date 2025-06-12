
#pragma once

#include <iostream>

#include "../logger_interface.hpp"

namespace demiplane::scroll {

    template <IsEntry EntryType>
    class ConsoleLogger final : public Logger<EntryType> {
    public:
        explicit ConsoleLogger(LogLevel level) : Logger<EntryType>{level} {}

        ConsoleLogger() = default;

        void log(LogLevel level, const std::string_view message, const char* file, const uint32_t line,
            const char* function) override {
            // Skip logging if below the threshold
            if (static_cast<int>(level) < static_cast<int>(this->threshold_)) {
                return;
            }

            EntryType entry(level, message, file, line, function);
            std::cout << entry.to_string();
        }

        void log(const EntryType &entry) override {
            if (static_cast<int>(entry.level()) < static_cast<int>(this->threshold_)) {
                return;
            }
            std::cout << entry.to_string();
        }
    };

} // namespace demiplane::scroll
