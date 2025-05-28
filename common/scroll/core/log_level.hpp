#pragma once

namespace demiplane::scroll {
    enum class LogLevel {
        Debug = 0,
        Info,
        Warning,
        Error,
        Fatal
        // Extend with additional levels if needed.
    };
    inline constexpr auto DBG = LogLevel::Debug;
    inline constexpr auto INF = LogLevel::Info;
    inline constexpr auto WRN = LogLevel::Warning;
    inline constexpr auto ERR = LogLevel::Error;
    inline constexpr auto FAT = LogLevel::Fatal;

    constexpr const char* to_string(const LogLevel level) {
        switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARNING";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Fatal:
            return "FATAL";
        }
        return "UNKNOWN";
    }
} // namespace demiplane::scroll
