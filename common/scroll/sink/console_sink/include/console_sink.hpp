#pragma once

#include <iostream>
#include <mutex>

#include <colors.hpp>
#include <sink_interface.hpp>

namespace demiplane::scroll {
    /**
     * @brief Configuration for console output
     */
    struct ConsoleSinkConfig {
        LogLevel threshold = LogLevel::Debug;
        bool enable_colors = true;
        bool flush_each_entry = false;
        std::ostream* output = &std::cout;  // Can redirect to &std::cerr
    };

    /**
     * @brief Console sink with ANSI color support
     *
     * @tparam EntryType Defines which metadata to include (e.g., DetailedEntry, LightEntry)
     *
     * Features:
     * - ANSI color-coded output based on log level
     * - Configurable threshold filtering
     * - Thread-safe writing
     * - Optional per-entry flushing
     *
     * Color scheme:
     * - TRC/DBG: Cyan
     * - INF: Green
     * - WRN: Yellow
     * - ERR: Red
     * - FAT: Bold red
     *
     * Usage:
     *   auto console = std::make_unique<ConsoleSink<DetailedEntry>>(
     *       ConsoleSinkConfig{
     *           .threshold = LogLevel::Info,
     *           .enable_colors = true
     *       }
     *   );
     *   logger.add_sink(std::move(console));
     */
    template <detail::EntryConcept EntryType>
    class ConsoleSink final : public Sink {
    public:
        explicit ConsoleSink(ConsoleSinkConfig cfg = {}) noexcept
            : config_{cfg} {}

        void process(const LogEvent& event) override {
            if (!should_log(event.level)) {
                return;
            }

            // Convert LogEvent → EntryType using existing factory pattern
            EntryType entry = make_entry_from_event<EntryType>(event);
            std::string formatted = entry.to_string();

            std::lock_guard lock{mutex_};

            if (config_.enable_colors) {
                *config_.output << colorize_by_level(formatted, entry.level());
            } else {
                *config_.output << formatted;
            }

            if (config_.flush_each_entry) {
                config_.output->flush();
            }
        }

        void flush() override {
            std::lock_guard lock{mutex_};
            config_.output->flush();
        }

        [[nodiscard]] bool should_log(LogLevel lvl) const noexcept override {
            return static_cast<int8_t>(lvl) >= static_cast<int8_t>(config_.threshold);
        }

        // Allow runtime config changes
        ConsoleSinkConfig& config() noexcept {
            return config_;
        }

        [[nodiscard]] const ConsoleSinkConfig& config() const noexcept {
            return config_;
        }

    private:
        ConsoleSinkConfig config_;
        mutable std::mutex mutex_;

        /**
         * @brief Colorize text based on log level
         * @param text Text to colorize
         * @param lvl Log level determining color
         * @return Colorized text with ANSI codes
         */
        [[nodiscard]] static std::string colorize_by_level(std::string_view text, LogLevel lvl) {
            using namespace colors;

            switch (lvl) {
                case LogLevel::Trace:
                case LogLevel::Debug:
                    return make_cyan(text);
                case LogLevel::Info:
                    return make_green(text);
                case LogLevel::Warning:
                    return make_yellow(text);
                case LogLevel::Error:
                    return make_red(text);
                case LogLevel::Fatal:
                    return make_bold_red(text);
                default:
                    return std::string{text};
            }
        }
    };
}  // namespace demiplane::scroll
