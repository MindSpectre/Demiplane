#pragma once

#include <memory>

#include "console_logger.hpp"
#include "detailed_entry.hpp"
#include "file_logger.hpp"


namespace demiplane::scroll {
    template <typename LoggerType>
    class LoggerProvider {
    public:
        virtual ~LoggerProvider() = default;

        LoggerProvider()          = default;

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


    template <class EntryType>
    class ConsoleLoggerProvider : public LoggerProvider<ConsoleLogger<EntryType>> {};

    template <class EntryType>
    class FileLoggerProvider : public LoggerProvider<FileLogger<EntryType>> {};

    class TestLoggerProvider : public LoggerProvider<ConsoleLogger<DetailedEntry>>{
    public:
        explicit TestLoggerProvider()
            : LoggerProvider(std::make_shared<ConsoleLogger<DetailedEntry>>(LogLevel::Debug)) {}
    };
} // namespace demiplane::scroll
