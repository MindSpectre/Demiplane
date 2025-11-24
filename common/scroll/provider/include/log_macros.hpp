#pragma once

#include <gears_macros.hpp>

#include "logger_provider.hpp"
namespace demiplane::scroll {
    // Dummy stream for disabled logging
    class DummyStream {
    public:
        template <typename T>
        DummyStream& operator<<(const T&) {
            return *this;
        }
    };
}  // namespace demiplane::scroll

// ============================================================================
// Macro overloading utilities (using C++20 __VA_OPT__)
// ============================================================================

// Override HAS_ARGS to properly detect empty arguments using __VA_OPT__


// ============================================================================
// Overloaded macros: LOG_DBG() uses stream, LOG_DBG("fmt", ...) uses format
// ============================================================================

#ifdef DMP_ENABLE_LOGGING
    /**
     * @brief Log with format string and arguments OR stream operators
     *
     * Usage:
     *   LOG_INF("User {} logged in from {}", username, ip_address);  // Format style
     *   LOG_INF() << "User " << username << " logged in";            // Stream style
     */

    // ========== TRACE ==========
    #define LOG_TRC_STREAM()                                                                                           \
        this->get_logger()->stream(::demiplane::scroll::LogLevel::Trace,               \
                                                                   std::source_location::current())

    #define LOG_TRC_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(                                                       \
            ::demiplane::scroll::LogLevel::Trace, fmt, std::source_location::current(), __VA_ARGS__)

    #define LOG_TRC_DISPATCH_() LOG_TRC_STREAM()
    #define LOG_TRC_DISPATCH_TRUE(...) LOG_TRC_FMT(__VA_ARGS__)
    #define LOG_TRC(...) CONCAT(LOG_TRC_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

    // ========== DEBUG ==========
    #define LOG_DBG_STREAM()                                                                                           \
        this->get_logger()->stream(::demiplane::scroll::LogLevel::Debug,               \
                                                                   std::source_location::current())

    #define LOG_DBG_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(                                                       \
            ::demiplane::scroll::LogLevel::Debug, fmt, std::source_location::current(), __VA_ARGS__)

    #define LOG_DBG_DISPATCH_() LOG_DBG_STREAM()
    #define LOG_DBG_DISPATCH_TRUE(...) LOG_DBG_FMT(__VA_ARGS__)
    #define LOG_DBG(...) CONCAT(LOG_DBG_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

    // ========== INFO ==========
    #define LOG_INF_STREAM()                                                                                           \
        this->get_logger()->stream(::demiplane::scroll::LogLevel::Info,                \
                                                                   std::source_location::current())

    #define LOG_INF_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(                                                       \
            ::demiplane::scroll::LogLevel::Info, fmt, std::source_location::current(), __VA_ARGS__)

    #define LOG_INF_DISPATCH_() LOG_INF_STREAM()
    #define LOG_INF_DISPATCH_TRUE(...) LOG_INF_FMT(__VA_ARGS__)
    #define LOG_INF(...) CONCAT(LOG_INF_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

    // ========== WARNING ==========
    #define LOG_WRN_STREAM()                                                                                           \
        this->get_logger()->stream(::demiplane::scroll::LogLevel::Warning,             \
                                                                   std::source_location::current())

    #define LOG_WRN_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(                                                       \
            ::demiplane::scroll::LogLevel::Warning, fmt, std::source_location::current(), __VA_ARGS__)

    #define LOG_WRN_DISPATCH_() LOG_WRN_STREAM()
    #define LOG_WRN_DISPATCH_TRUE(...) LOG_WRN_FMT(__VA_ARGS__)
    #define LOG_WRN(...) CONCAT(LOG_WRN_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

    // ========== ERROR ==========
    #define LOG_ERR_STREAM()                                                                                           \
        this->get_logger()->stream(::demiplane::scroll::LogLevel::Error,               \
                                                                   std::source_location::current())

    #define LOG_ERR_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(                                                       \
            ::demiplane::scroll::LogLevel::Error, fmt, std::source_location::current(), __VA_ARGS__)

    #define LOG_ERR_DISPATCH_() LOG_ERR_STREAM()
    #define LOG_ERR_DISPATCH_TRUE(...) LOG_ERR_FMT(__VA_ARGS__)
    #define LOG_ERR(...) CONCAT(LOG_ERR_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

    // ========== FATAL ==========
    #define LOG_FAT_STREAM()                                                                                           \
        this->get_logger()->stream(::demiplane::scroll::LogLevel::Fatal,               \
                                                                   std::source_location::current())

    #define LOG_FAT_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(                                                       \
            ::demiplane::scroll::LogLevel::Fatal, fmt, std::source_location::current(), __VA_ARGS__)

    #define LOG_FAT_DISPATCH_() LOG_FAT_STREAM()
    #define LOG_FAT_DISPATCH_TRUE(...) LOG_FAT_FMT(__VA_ARGS__)
    #define LOG_FAT(...) CONCAT(LOG_FAT_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

#else
   // Disabled logging - all macros become no-ops
    #define LOG_TRC(...) ::demiplane::scroll::DummyStream()
    #define LOG_DBG(...) ::demiplane::scroll::DummyStream()
    #define LOG_INF(...) ::demiplane::scroll::DummyStream()
    #define LOG_WRN(...) ::demiplane::scroll::DummyStream()
    #define LOG_ERR(...) ::demiplane::scroll::DummyStream()
    #define LOG_FAT(...) ::demiplane::scroll::DummyStream()
#endif

// ============================================================================
// Direct logger macros (for custom logger instances) TODO: make it looks like LOG_DIRECT_TRC wihout FMT or STREAM
// ============================================================================

#ifdef DMP_ENABLE_LOGGING
    /**
     * @brief Log to a specific logger instance (not global)
     *
     * Usage:
     *   Logger* my_logger = get_logger();
     *   LOG_DIRECT_FMT_INF(my_logger, "User {} connected", user_id);
     *   LOG_DIRECT_STREAM_DBG(my_logger) << "Debug info: " << data;
     */
    #define LOG_DIRECT_FMT(logger_ptr, level, fmt, ...)                                                                    \
        (logger_ptr)->log(level, fmt, std::source_location::current(), __VA_ARGS__)

    #define SLOG_DIRECT_FMT(logger_ptr, level) (logger_ptr)->stream(level, std::source_location::current())

    // Convenience wrappers
    #define LOG_DIRECT_FMT_TRC(logger_ptr, fmt, ...)                                                                       \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Trace, fmt, __VA_ARGS__)
    #define LOG_DIRECT_FMT_DBG(logger_ptr, fmt, ...)                                                                       \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Debug, fmt, __VA_ARGS__)
    #define LOG_DIRECT_FMT_INF(logger_ptr, fmt, ...)                                                                       \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Info, fmt, __VA_ARGS__)
    #define LOG_DIRECT_FMT_WRN(logger_ptr, fmt, ...)                                                                       \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Warning, fmt, __VA_ARGS__)
    #define LOG_DIRECT_FMT_ERR(logger_ptr, fmt, ...)                                                                       \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Error, fmt, __VA_ARGS__)
    #define LOG_DIRECT_FMT_FAT(logger_ptr, fmt, ...)                                                                       \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Fatal, fmt, __VA_ARGS__)

    #define LOG_DIRECT_STREAM_TRC(logger_ptr) SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Trace)
    #define LOG_DIRECT_STREAM_DBG(logger_ptr) SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Debug)
    #define LOG_DIRECT_STREAM_INF(logger_ptr) SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Info)
    #define LOG_DIRECT_STREAM_WRN(logger_ptr) SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Warning)
    #define LOG_DIRECT_STREAM_ERR(logger_ptr) SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Error)
    #define LOG_DIRECT_STREAM_FAT(logger_ptr) SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Fatal)
#else
    #define LOG_DIRECT_FMT(logger_ptr, level, fmt, ...) ((void)0)
    #define SLOG_DIRECT_FMT(logger_ptr, level) ::demiplane::scroll::DummyStream()

    #define LOG_DIRECT_FMT_TRC(logger_ptr, fmt, ...) ((void)0)
    #define LOG_DIRECT_FMT_DBG(logger_ptr, fmt, ...) ((void)0)
    #define LOG_DIRECT_FMT_INF(logger_ptr, fmt, ...) ((void)0)
    #define LOG_DIRECT_FMT_WRN(logger_ptr, fmt, ...) ((void)0)
    #define LOG_DIRECT_FMT_ERR(logger_ptr, fmt, ...) ((void)0)
    #define LOG_DIRECT_FMT_FAT(logger_ptr, fmt, ...) ((void)0)

    #define LOG_DIRECT_STREAM_TRC(logger_ptr) ::demiplane::scroll::DummyStream()
    #define LOG_DIRECT_STREAM_DBG(logger_ptr) ::demiplane::scroll::DummyStream()
    #define LOG_DIRECT_STREAM_INF(logger_ptr) ::demiplane::scroll::DummyStream()
    #define LOG_DIRECT_STREAM_WRN(logger_ptr) ::demiplane::scroll::DummyStream()
    #define LOG_DIRECT_STREAM_ERR(logger_ptr) ::demiplane::scroll::DummyStream()
    #define LOG_DIRECT_STREAM_FAT(logger_ptr) ::demiplane::scroll::DummyStream()
#endif

#ifdef DMP_COMPONENT_LOGGING

    // Stream-based logging
    #define COMPONENT_LOG_TRC() LOG_DIRECT_STREAM_TRC(::demiplane::scroll::ComponentLoggerManager::get())
    #define COMPONENT_LOG_DBG() LOG_DIRECT_STREAM_DBG(::demiplane::scroll::ComponentLoggerManager::get())
    #define COMPONENT_LOG_INF() LOG_DIRECT_STREAM_INF(::demiplane::scroll::ComponentLoggerManager::get())
    #define COMPONENT_LOG_WRN() LOG_DIRECT_STREAM_WRN(::demiplane::scroll::ComponentLoggerManager::get())
    #define COMPONENT_LOG_ERR() LOG_DIRECT_STREAM_ERR(::demiplane::scroll::ComponentLoggerManager::get())
    #define COMPONENT_LOG_FAT() LOG_DIRECT_STREAM_FAT(::demiplane::scroll::ComponentLoggerManager::get())
    #define COMPONENT_LOG_ENTER_FUNCTION() COMPONENT_LOG_INF() << "Entering function " << __func__
#else
    // When component logging is disabled, use scroll's dummy implementations
    #define COMPONENT_LOG(level, message) ((void)0)
    #define COMPONENT_LOG_DBG() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_INF() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_WRN() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_ERR() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_FAT() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_ENTER_FUNCTION() ::demiplane::scroll::DummyStream()
#endif
