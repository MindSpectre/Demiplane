#pragma once

#include <memory>

#include "console_logger.hpp"
#include "file_logger.hpp"

// For test logger provider
#include "entry/detailed_entry.hpp"


namespace demiplane::scroll {
    template <LoggerConcept LoggerType>
    class LoggerProvider {
    public:
        virtual ~LoggerProvider() = default;

        LoggerProvider() = default;

        explicit LoggerProvider(const std::shared_ptr<LoggerType>& logger) : logger_(logger) {}

        [[nodiscard]] LoggerType* get_logger() noexcept {
            return logger_.get();
        }

        [[nodiscard]] const LoggerType* get_logger() const noexcept {
            return logger_.get();
        }

        void set_logger(std::shared_ptr<LoggerType> logger) noexcept {
            logger_ = std::move(logger);
        }

    private:
        std::shared_ptr<LoggerType> logger_;
    };


    template <detail::EntryConcept EntryType>
    class ConsoleLoggerProvider : public LoggerProvider<ConsoleLogger<EntryType>> {};

    template <detail::EntryConcept EntryType>
    class FileLoggerProvider : public LoggerProvider<FileLogger<EntryType>> {};

    class TestLoggerProvider : public LoggerProvider<ConsoleLogger<DetailedEntry>> {
    public:
        explicit TestLoggerProvider()
            : LoggerProvider(
                  std::make_shared<ConsoleLogger<DetailedEntry>>(ConsoleLoggerConfig{.flush_each_entry = true})) {}
    };
} // namespace demiplane::scroll
