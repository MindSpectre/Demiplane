#pragma once

#include <mutex>

#include <colors.hpp>

#include "console_sink_config.hpp"

namespace demiplane::scroll {


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
     */
    template <detail::EntryConcept EntryType>
    class ConsoleSink final : public Sink {
    public:
        template <typename ConsoleSinkConfigTp = ConsoleSinkConfig>
            requires std::constructible_from<ConsoleSinkConfig, ConsoleSinkConfigTp>
        explicit ConsoleSink(ConsoleSinkConfigTp&& cfg = {}) noexcept
            : config_{std::forward<ConsoleSinkConfigTp>(cfg)} {
        }

        void process(const LogEvent& event) override {
            if (!should_log(event.level)) {
                return;
            }

            // Convert LogEvent → EntryType using existing factory pattern
            EntryType entry             = make_entry_from_event<EntryType>(event);
            const std::string formatted = entry.to_string();

            std::lock_guard lock{mutex_};

            if (config_.is_enable_colors()) {
                *config_.get_output() << colorize_by_level(formatted, entry.level());
            } else {
                *config_.get_output() << formatted;
            }

            if (config_.is_flush_each_entry()) {
                config_.get_output()->flush();
            }
        }

        void flush() override {
            std::lock_guard lock{mutex_};
            config_.get_output()->flush();
        }

        [[nodiscard]] bool should_log(LogLevel lvl) const noexcept override {
            return static_cast<int8_t>(lvl) >= static_cast<int8_t>(config_.get_threshold());
        }

        // Allow runtime config changes
        constexpr ConsoleSinkConfig& config() noexcept {
            return config_;
        }

        [[nodiscard]] constexpr const ConsoleSinkConfig& config() const noexcept {
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
        [[nodiscard]] static std::string colorize_by_level(const std::string_view text, const LogLevel lvl) {
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
