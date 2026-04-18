#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "log_event.hpp"

namespace demiplane::scroll {
    /**
     * @brief Base interface for all log sinks
     *
     * Non-templated to allow heterogeneous storage in Logger.
     * Each sink implementation chooses its EntryType for formatting.
     *
     * Design:
     * - Logger stores vector<shared_ptr<Sink>>
     * - ConsoleSink<DetailedEntry>, FileSink<LightEntry> both inherit from Sink
     * - Consumer dispatches batches to sink strands via process_batch()
     * - Sink converts LogEvent → EntryType → formatted output
     */
    class Sink {
    public:
        virtual ~Sink() = default;

        /**
         * @brief Process a single log event
         * @param event Raw log event with all metadata
         */
        virtual void process(const LogEvent& event) = 0;

        /**
         * @brief Process a batch of log events
         * @param batch Shared ownership of the event batch
         *
         * Default: iterates and calls process() per event.
         * Override for batch-optimized I/O (e.g., batch network sends).
         */
        virtual void process_batch(const std::shared_ptr<std::vector<LogEvent>>& batch) {
            for (const auto& event : *batch) {
                process(event);
            }
        }

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
         * @param prefix Prefix of the message to check
         * @return true if sink will process this level
         *
         * Used for early filtering before calling process().
         */
        [[nodiscard]] virtual bool should_log(LogLevel lvl, std::string_view prefix) const noexcept = 0;
    };
}  // namespace demiplane::scroll
