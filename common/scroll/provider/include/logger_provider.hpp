#pragma once

#include <memory>

#include "console_sink.hpp"
#include "detailed_entry.hpp"
#include "file_sink.hpp"
#include "logger.hpp"

namespace demiplane::scroll {
    /**
     * @brief Logger provider - wraps logger instance
     *
     * Allows dependency injection and testing.
     */
    class LoggerProvider {
    public:
        virtual ~LoggerProvider() = default;

        LoggerProvider() = default;

        explicit LoggerProvider(std::shared_ptr<Logger> logger)
            : logger_{std::move(logger)} {
        }

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

    /**
     * @brief Global logger manager for component logging
     *
     * Provides static singleton access to logger instance.
     * Used by COMPONENT_LOG_* macros.
     */
    class ComponentLoggerManager {
    public:
        static Logger* get() {
            // Lazy initialization on first use
            if (!logger_) {
                initialize();
            }
            return logger_.get();
        }

        static void initialize();


        // Allow manual override for testing or custom configuration
        static void set_logger(std::shared_ptr<Logger> logger) {
            logger_ = std::move(logger);
        }

    private:
        static inline std::shared_ptr<Logger> logger_ = nullptr;
    };

    /**
     * @brief Test logger provider with console output
     *
     * Convenience for testing - creates logger with console sink that flushes each entry.
     */
    class TestLoggerProvider : public LoggerProvider {
    public:
        explicit TestLoggerProvider() {
            auto logger = std::make_shared<Logger>();
            logger->add_sink(std::make_shared<ConsoleSink<DetailedEntry>>(
                ConsoleSinkConfig{}.threshold(LogLevel::Debug).enable_colors(true).flush_each_entry(true)));
            set_logger(std::move(logger));
        }
    };
}  // namespace demiplane::scroll
