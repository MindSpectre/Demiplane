#pragma once

#include <iostream>

#include "../logger_interface.hpp"
#include "entry/factory/entry_factory.hpp"
namespace demiplane::scroll {

    struct ConsoleLoggerConfig  {
        LogLevel threshold{LogLevel::Debug};
        bool flush_each_entry{false};
    };

    template <detail::EntryConcept EntryType>
    class ConsoleLogger : public Logger<EntryType> {
    public:
        explicit ConsoleLogger(const ConsoleLoggerConfig cfg) : config_{cfg} {}

        void log(LogLevel lvl, const std::string_view msg, const std::source_location loc) override {
            auto entry = make_entry<EntryType>(lvl, msg, loc);
            log(entry);
        }

        void log(const EntryType& entry) override {
            if (static_cast<int8_t>(entry.level()) < static_cast<int8_t>(config_.threshold)) {
                return;
            }
            std::cout << entry.to_string();
            if (config_.flush_each_entry) {
                std::cout << std::flush;
            }
        }
        ConsoleLoggerConfig& config() {
            return config_;
        }
    protected:
        ConsoleLoggerConfig config_;
    };

} // namespace demiplane::scroll
