
#pragma once

#include <iostream>

#include "../logger_interface.hpp"
#include "chrono_utils.hpp"

namespace demiplane::scroll {

    template <IsEntry EntryType>
    class ConsoleLogger final : public Logger<EntryType> {
    public:
        explicit ConsoleLogger(LogLevel level) : Logger<EntryType>{level} {}

        ConsoleLogger() = default;

        void log(LogLevel lvl, std::string_view msg, std::source_location loc) override {
            if (static_cast<int>(lvl) < static_cast<int>(this->threshold_)) {
                return;
            }
            auto entry = make_entry<EntryType>(lvl, msg, loc);
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
