#pragma once

#include <iostream>

#include "../logger_interface.hpp"
#include "entry/factory/entry_factory.hpp"
namespace demiplane::scroll {

    template <class EntryType>
    class ConsoleLogger final : public Logger<EntryType> {
    public:
        explicit ConsoleLogger(LogLevel threshold) : Logger<EntryType>{threshold} {}

        ConsoleLogger() = default;

        void log(LogLevel lvl, const std::string_view msg, const std::source_location loc) override {
            if (static_cast<int8_t>(lvl) < static_cast<int8_t>(this->threshold_)) {
                return;
            }
            auto entry = make_entry<EntryType>(lvl, msg, loc);
            std::cout << entry.to_string();
        }

        void log(const EntryType& entry) override {
            if (static_cast<int8_t>(entry.level()) < static_cast<int8_t>(this->threshold_)) {
                return;
            }
            std::cout << entry.to_string();
        }
    };

} // namespace demiplane::scroll
