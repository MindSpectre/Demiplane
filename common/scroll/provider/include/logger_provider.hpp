#pragma once

#include <memory>

#include "file_logger.hpp"
#include "console_logger.hpp"

// For test logger provider
#include <factory/entry_factory.hpp>

namespace demiplane::scroll {
    class LoggerProvider {
    public:
        virtual ~LoggerProvider() = default;

        LoggerProvider() = default;

        explicit LoggerProvider(const std::shared_ptr<Logger>& logger)
            : logger_(logger) {
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

    class ComponentLoggerManager {
    public:
        static constexpr Logger* get() {
            // Lazy initialization on first use
            if (!logger_) {
                initialize();
            }
            return logger_.get();
        }

        static void initialize();

        // Allow manual override for testing
        static void set_logger(const std::shared_ptr<Logger>& logger) {
            logger_ = logger;
        }

    private:
        static inline std::shared_ptr<Logger> logger_ = nullptr;
    };

    class TestLoggerProvider : public LoggerProvider {
    public:
        explicit TestLoggerProvider()
            : LoggerProvider(
                  std::make_shared<ConsoleLogger<DetailedEntry>>(ConsoleLoggerConfig{.flush_each_entry = true})) {
        }
    };
}  // namespace demiplane::scroll
