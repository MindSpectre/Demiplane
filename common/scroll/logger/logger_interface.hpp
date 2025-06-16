#pragma once

#include <string_view>

#include "entry/factory/entry_factory.hpp"


namespace demiplane::scroll {

    template <IsEntry EntryT>
    class Logger {
    public:
        virtual ~Logger() = default;

        explicit Logger(LogLevel lvl = LogLevel::Debug) : threshold_{lvl} {}

        // ──────────────────────────────────────────────────────────────
        // New: captures file / line / function for you
        virtual void log(
            LogLevel lvl, std::string_view msg, std::source_location loc = std::source_location::current()) = 0;
        // ──────────────────────────────────────────────────────────────

        virtual void log(const EntryT& entry) = 0;

        void set_threshold(LogLevel lvl) {
            threshold_ = lvl;
        }
        [[nodiscard]] LogLevel get_threshold() const {
            return threshold_;
        }

    protected:
        LogLevel threshold_;
    };

} // namespace demiplane::scroll
