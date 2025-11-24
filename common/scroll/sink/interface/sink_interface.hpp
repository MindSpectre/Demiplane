#pragma once

#include "log_event.hpp"

namespace demiplane::scroll {
    /**
     * @brief Base interface for all log sinks
     *
     * Non-templated to allow heterogeneous storage in Logger.
     * Each sink implementation chooses its EntryType for formatting.
     *
     * Design:
     * - Logger stores vector<unique_ptr<Sink>>
     * - ConsoleSink<DetailedEntry>, FileSink<LightEntry> both inherit from Sink
     * - Consumer thread calls process() with LogEvent
     * - Sink converts LogEvent → EntryType → formatted output
     */
    class Sink {
    public:
        virtual ~Sink() = default;

        /**
         * @brief Process a log event
         * @param event Raw log event with all metadata
         *
         * Called by consumer thread. Sink should:
         * 1. Check should_log(event.level)
         * 2. Convert LogEvent → EntryType via make_entry_from_event
         * 3. Format and output entry.to_string()
         */
        virtual void process(const LogEvent& event) = 0;

        /**
         * @brief Flush any buffered data
         *
         * Called during:
         * - Logger shutdown
         * - Explicit flush requests
         * - Critical errors (ERR/FAT logs)
         */
        virtual void flush() = 0;

        /**
         * @brief Check if this sink should process this log level
         * @param lvl Log level to check
         * @return true if sink will process this level
         *
         * Used for early filtering before calling process().
         */
        [[nodiscard]] virtual bool should_log(LogLevel lvl) const noexcept = 0;
    };
}  // namespace demiplane::scroll
