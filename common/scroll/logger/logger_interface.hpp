#pragma once

#include <string_view>

#include "../core/entry_interface.hpp"


namespace demiplane::scroll {

    template <detail::EntryConcept EntryT>
    class Logger {
    public:
        virtual ~Logger() = default;

        explicit Logger(const LogLevel lvl = LogLevel::Debug) : threshold_{lvl} {}

        // ──────────────────────────────────────────────────────────────
        // New: captures file / line / function for you
        virtual void log(LogLevel lvl, std::string_view msg, std::source_location loc) = 0;
        // ──────────────────────────────────────────────────────────────

        virtual void log(const EntryT& entry) = 0;

        void set_threshold(const LogLevel lvl) {
            threshold_ = lvl;
        }
        [[nodiscard]] LogLevel get_threshold() const {
            return threshold_;
        }
        // TODO: add non blocking thread writer logger
    protected:
        LogLevel threshold_;
    };

} // namespace demiplane::scroll
