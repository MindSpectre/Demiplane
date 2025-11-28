#pragma once

#include <source_location>
#include <string>

#include <entry_interface.hpp>

namespace demiplane::scroll {
    /**
     * @brief Raw log event data container
     *
     * Contains ALL possible metadata captured in producer thread.
     * Sinks extract what their EntryType needs via make_entry_from_event().
     *
     * Stored in RingBuffer for lock-free producer-consumer pattern.
     */
    struct LogEvent {
        // Core data
        LogLevel level = LogLevel::Debug;
        std::string message;  // Already formatted with std::format or stream

        // Metadata (captured in producer thread - correct TID/PID)
        detail::MetaSource location;
        detail::MetaTimePoint time_point;
        detail::MetaThread tid;
        detail::MetaProcess pid;

        // Cached string representations (avoid repeated formatting)

        // Shutdown signal for graceful consumer thread termination
        bool shutdown_signal = false;

        LogEvent() = default;

        /**
         * @brief Construct LogEvent with metadata captured from current thread
         * @param lvl Log level
         * @param msg Formatted message
         * @param loc Source location
         */
        LogEvent(const LogLevel lvl, std::string msg, const std::source_location& loc = std::source_location::current())
            : level{lvl},
              message{std::move(msg)},
              location{loc} {
        }
    };

    /**
     * @brief Create Entry from LogEvent using existing entry factory pattern
     *
     * Similar to make_entry() but sources metadata from LogEvent instead of capturing fresh.
     * This ensures TID/PID are from producer thread, not consumer thread.
     *
     * @tparam EntryT Entry type (DetailedEntry, LightEntry, etc.)
     * @param event LogEvent with pre-captured metadata
     * @return Constructed entry with appropriate metadata
     *
     * Usage:
     *   auto entry = make_entry_from_event<DetailedEntry>(event);
     *   std::cout << entry.to_string();
     */
    template <class EntryT>
    EntryT make_entry_from_event(const LogEvent& event) {
        // Build tuple with all available metadata from LogEvent
        auto available = std::tuple{event.time_point, event.location, event.tid, event.pid};

        // Use entry_traits to extract only what EntryT wants
        using want_types = detail::entry_traits<EntryT>::wants;
        auto args        = gears::make_arg_tuple<want_types, decltype(available)>::from(std::move(available));

        // Construct entry with level, message, and tailored metadata
        return std::apply(
            [&]<typename... Tailored>(Tailored&&... tail) {
                return EntryT{event.level, event.message, std::forward<Tailored>(tail)...};
            },
            args);
    }
}  // namespace demiplane::scroll
