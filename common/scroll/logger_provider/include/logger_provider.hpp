#pragma once

#include <memory>

#include "console_logger.hpp"
#include "file_logger.hpp"

// For test logger provider
#include "entry/detailed_entry.hpp"


namespace demiplane::scroll {
    class LoggerProvider {
    public:
        virtual ~LoggerProvider() = default;

        LoggerProvider() = default;

        explicit LoggerProvider(const std::shared_ptr<Logger>& logger)
            : logger_(logger) {}

        [[nodiscard]] Logger* get_logger() noexcept {
            return logger_.get();
        }

        [[nodiscard]] const Logger* get_logger() const noexcept {
            return logger_.get();
        }

        void set_logger(std::shared_ptr<Logger> logger) noexcept {
            logger_ = std::move(logger);
        }

    private:
        std::shared_ptr<Logger> logger_;
    };


    class ConsoleLoggerProvider : public LoggerProvider {};

    class FileLoggerProvider : public LoggerProvider {};

    class TestLoggerProvider : public LoggerProvider {
    public:
        explicit TestLoggerProvider()
            : LoggerProvider(std::make_shared<ConsoleLogger<DetailedEntry>>(
                ConsoleLoggerConfig{
                    .flush_each_entry = true
                })) {}
    };
} // namespace demiplane::scroll
