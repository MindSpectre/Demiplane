#pragma once

#include <cstdint>

namespace demiplane::scroll {
    /**
     * @enum LogLevel
     * @brief Represents different levels of logging severity.
     *
     * This enumeration defines the severity levels for logging. They can be
     * used to categorize and filter log messages based on their importance
     * or criticality. The levels increase in severity from Debug to Fatal.
     *
     * Enumerator values:
     * - Debug: Used for detailed debugging information.
     * - Info: Used for general informational messages.
     * - Warning: Used for warnings about potential issues.
     * - Error: Used for error messages indicating failures.
     * - Fatal: Used for critical errors requiring immediate attention.
     */
    enum class LogLevel : std::uint8_t {
        Trace = 0,
        Debug,
        Info,
        Warning,
        Error,
        Fatal
        // Extend with additional levels if needed.
    };

    inline constexpr auto TRC = LogLevel::Trace;
    inline constexpr auto DBG = LogLevel::Debug;
    inline constexpr auto INF = LogLevel::Info;
    inline constexpr auto WRN = LogLevel::Warning;
    inline constexpr auto ERR = LogLevel::Error;
    inline constexpr auto FAT = LogLevel::Fatal;

    constexpr const char* log_level_to_string(const LogLevel level) {
        switch (level) {
            case LogLevel::Trace:
                return "TRC";
            case LogLevel::Debug:
                return "DBG";
            case LogLevel::Info:
                return "INF";
            case LogLevel::Warning:
                return "WRN";
            case LogLevel::Error:
                return "ERR";
            case LogLevel::Fatal:
                return "FAT";
        }
        return "UNKNOWN";
    }
}  // namespace demiplane::scroll
