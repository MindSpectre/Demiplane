#pragma once

#include <iostream>

#include "../logger_interface.hpp"


namespace demiplane::scroll {
    struct ConsoleLoggerConfig {
        NEXUS_REGISTER(0x405ADA4C, nexus::Resettable); // CRC32/ISO-HDLC of demiplane::scroll::ConsoleLoggerConfig

        LogLevel threshold{LogLevel::Debug};
        bool flush_each_entry{false};
    };

    template <detail::EntryConcept EntryType>
    class ConsoleLogger final : public Logger {
    public:
        NEXUS_REGISTER(0xDCBA748C, nexus::Resettable); // CRC32/ISO-HDLC of demiplane::scroll::ConsoleLogger

        explicit ConsoleLogger(const ConsoleLoggerConfig cfg)
            : config_{cfg} {}


        void log(LogLevel lvl, std::string_view msg, const detail::MetaSource& loc) override {
            auto entry = make_entry<EntryType>(lvl, msg, loc);
            log(entry);
        }

        void log(const EntryType& entry) {
            if (static_cast<int8_t>(entry.level()) < static_cast<int8_t>(config_.threshold)) {
                return;
            }
            std::lock_guard lock{mutex_};
            std::cout << entry.to_string();
            if (config_.flush_each_entry) {
                std::cout << std::flush;
            }
        }

        ConsoleLoggerConfig& config() {
            return config_;
        }

    protected:
        mutable std::mutex mutex_;
        ConsoleLoggerConfig config_;
    };
} // namespace demiplane::scroll
