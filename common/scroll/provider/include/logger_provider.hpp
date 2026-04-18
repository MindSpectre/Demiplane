#pragma once

#include <memory>
#include <string_view>

#include "console_sink.hpp"
#include "detailed_entry.hpp"
#include "file_sink.hpp"
#include "logger.hpp"

namespace demiplane::scroll {
    /**
     * @brief Logger provider - wraps logger instance
     *
     * Allows dependency injection and testing.
     *
     * The @p prefix_ is a class-name / subsystem label rendered into log output
     * and usable by per-sink `PrefixFilter` for allow/deny routing.
     *
     * Prefix overflow: values longer than `PrefixNameStorage` capacity are
     * silently truncated at runtime (see `PrefixNameStorage` docs). Prefer
     * `SCROLL_COMPONENT_PREFIX` for literal names — it catches overflow at
     * compile time.
     *
     * Thread safety: @ref set_prefix is NOT thread-safe. Callers must set the
     * prefix during construction before the object is visible to other threads.
     * Concurrent `set_prefix` with active logging is UB (data race).
     */
    class LoggerProvider {
    public:
        virtual ~LoggerProvider() = default;

        LoggerProvider() = default;

        // Single 2-arg constructor with default prefix — the 1-arg
        // `LoggerProvider{logger}` form stays valid (prefix defaults empty).
        // `explicit` prevents implicit conversion from `shared_ptr<Logger>`
        // alone and avoids ambiguous overload resolution we'd hit if we
        // also declared a separate 1-arg constructor.
        constexpr explicit LoggerProvider(std::shared_ptr<Logger> logger, const std::string_view prefix = {})
            : logger_{std::move(logger)} {
            prefix_.assign(prefix);
        }

        [[nodiscard]] Logger* get_logger() noexcept {
            return logger_.get();
        }

        [[nodiscard]] const Logger* get_logger() const noexcept {
            return logger_.get();
        }

        [[nodiscard]] constexpr const PrefixNameStorage& prefix() const noexcept {
            return prefix_;
        }

        void set_logger(std::shared_ptr<Logger> logger) noexcept {
            logger_ = std::move(logger);
        }

        /// @note NOT thread-safe — see class docs.
        /// @note Oversized @p prefix is silently truncated at runtime.
        constexpr void set_prefix(const std::string_view prefix) noexcept {
            prefix_.assign(prefix);
        }

    private:
        std::shared_ptr<Logger> logger_;
        PrefixNameStorage prefix_;
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
     */
    class TestLoggerProvider : public LoggerProvider {
    public:
        constexpr explicit TestLoggerProvider(const std::string_view prefix = {}) {
            auto logger = std::make_shared<Logger>();
            logger->add_sink(std::make_shared<ConsoleSink<DetailedEntry>>(ConsoleSinkConfig::Builder{}
                                                                              .threshold(LogLevel::Debug)
                                                                              .enable_colors(true)
                                                                              .flush_each_entry(true)
                                                                              .finalize()));
            set_logger(std::move(logger));
            set_prefix(prefix);
        }
    };

    // todo: possibly make default file sink logger
}  // namespace demiplane::scroll
