
#pragma once

#include <iostream>

#include "../logger_interface.hpp"

namespace demiplane::scroll {

    template <IsEntry EntryType>
    class ConsoleLogger final : public Logger<EntryType> {
    public:
        ConsoleLogger() = default;

        void log(LogLevel level, const std::string_view message, const char* file, const int line,
            const char* function) override {
            // Skip logging if below the threshold
            if (static_cast<int>(level) < static_cast<int>(this->threshold_)) {
                return;
            }

            EntryType entry(level, message, file, line, function);
            std::cout << entry.to_string() << std::endl;
        }
        void log(EntryType entry) override {
            if (static_cast<int>(entry.level()) < static_cast<int>(this->threshold_)) {
                return;
            }
            std::cout << entry.to_string() << std::endl;
        }
    };

} // namespace demiplane::scroll
