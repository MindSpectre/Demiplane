#pragma once

#include <string_view>

#include "../core/entry_interface.hpp"
#include "gears_utils.hpp"


namespace demiplane::scroll {


    template <detail::EntryConcept EntryT>
    class Logger {
    public:
        virtual ~Logger() = default;

        // ──────────────────────────────────────────────────────────────
        // New: captures file / line / function for you
        virtual void log(LogLevel lvl, std::string_view msg, std::source_location loc) = 0;
        // ──────────────────────────────────────────────────────────────

        virtual void log(const EntryT& entry) = 0;
        virtual void log(EntryT &&entry) = 0;
        // TODO: add non blocking thread writer logger
    };

    template <typename T>
    concept LoggerConcept = demiplane::gears::derived_from_specialization_of_v<T, Logger>;
    
} // namespace demiplane::scroll
