#pragma once

#include <gears_macros.hpp>

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
// Declares a class-scope prefix for COMPONENT_LOG_*.
//
// Usage inside a class body:
//     class Server {
//     private:
//         SCROLL_COMPONENT_PREFIX("Server");
//         ...
//     };
//
// The IIFE initializer runs at compile time; oversized names trigger the
// consteval-throw path in InlineString::assign → compile error.
// ============================================================================
#define SCROLL_COMPONENT_PREFIX(name)                                                                                  \
    static constexpr ::demiplane::scroll::PrefixNameStorage _dmp_scroll_class_prefix = [] {                            \
        ::demiplane::scroll::PrefixNameStorage s;                                                                      \
        s.assign(::std::string_view{name});                                                                            \
        return s;                                                                                                      \
    }()

// ============================================================================
// Sets the prefix on a LoggerProvider-derived object from inside its
// constructor body. Runtime call — overflow silently truncates.
// ============================================================================
#define SCROLL_SET_LOGGER_PREFIX(name) this->set_prefix(::std::string_view{name})

#ifdef DMP_ENABLE_LOGGING
   // ========== LOG_* (LoggerProvider path) ==========
    #define LOG_TRC_STREAM()                                                                                           \
        this->get_logger()->stream(                                                                                    \
            ::demiplane::scroll::LogLevel::Trace, this->prefix().view(), std::source_location::current())
    #define LOG_TRC_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(::demiplane::scroll::LogLevel::Trace,                                                  \
                                this->prefix().view(),                                                                 \
                                std::source_location::current(),                                                       \
                                fmt,                                                                                   \
                                __VA_ARGS__)
    #define LOG_TRC_DISPATCH_() LOG_TRC_STREAM()
    #define LOG_TRC_DISPATCH_TRUE(...) LOG_TRC_FMT(__VA_ARGS__)
    #define LOG_TRC(...) CONCAT(LOG_TRC_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

    #define LOG_DBG_STREAM()                                                                                           \
        this->get_logger()->stream(                                                                                    \
            ::demiplane::scroll::LogLevel::Debug, this->prefix().view(), std::source_location::current())
    #define LOG_DBG_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(::demiplane::scroll::LogLevel::Debug,                                                  \
                                this->prefix().view(),                                                                 \
                                std::source_location::current(),                                                       \
                                fmt,                                                                                   \
                                __VA_ARGS__)
    #define LOG_DBG_DISPATCH_() LOG_DBG_STREAM()
    #define LOG_DBG_DISPATCH_TRUE(...) LOG_DBG_FMT(__VA_ARGS__)
    #define LOG_DBG(...) CONCAT(LOG_DBG_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

    #define LOG_INF_STREAM()                                                                                           \
        this->get_logger()->stream(                                                                                    \
            ::demiplane::scroll::LogLevel::Info, this->prefix().view(), std::source_location::current())
    #define LOG_INF_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(::demiplane::scroll::LogLevel::Info,                                                   \
                                this->prefix().view(),                                                                 \
                                std::source_location::current(),                                                       \
                                fmt,                                                                                   \
                                __VA_ARGS__)
    #define LOG_INF_DISPATCH_() LOG_INF_STREAM()
    #define LOG_INF_DISPATCH_TRUE(...) LOG_INF_FMT(__VA_ARGS__)
    #define LOG_INF(...) CONCAT(LOG_INF_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

    #define LOG_WRN_STREAM()                                                                                           \
        this->get_logger()->stream(                                                                                    \
            ::demiplane::scroll::LogLevel::Warning, this->prefix().view(), std::source_location::current())
    #define LOG_WRN_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(::demiplane::scroll::LogLevel::Warning,                                                \
                                this->prefix().view(),                                                                 \
                                std::source_location::current(),                                                       \
                                fmt,                                                                                   \
                                __VA_ARGS__)
    #define LOG_WRN_DISPATCH_() LOG_WRN_STREAM()
    #define LOG_WRN_DISPATCH_TRUE(...) LOG_WRN_FMT(__VA_ARGS__)
    #define LOG_WRN(...) CONCAT(LOG_WRN_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

    #define LOG_ERR_STREAM()                                                                                           \
        this->get_logger()->stream(                                                                                    \
            ::demiplane::scroll::LogLevel::Error, this->prefix().view(), std::source_location::current())
    #define LOG_ERR_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(::demiplane::scroll::LogLevel::Error,                                                  \
                                this->prefix().view(),                                                                 \
                                std::source_location::current(),                                                       \
                                fmt,                                                                                   \
                                __VA_ARGS__)
    #define LOG_ERR_DISPATCH_() LOG_ERR_STREAM()
    #define LOG_ERR_DISPATCH_TRUE(...) LOG_ERR_FMT(__VA_ARGS__)
    #define LOG_ERR(...) CONCAT(LOG_ERR_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

    #define LOG_FAT_STREAM()                                                                                           \
        this->get_logger()->stream(                                                                                    \
            ::demiplane::scroll::LogLevel::Fatal, this->prefix().view(), std::source_location::current())
    #define LOG_FAT_FMT(fmt, ...)                                                                                      \
        this->get_logger()->log(::demiplane::scroll::LogLevel::Fatal,                                                  \
                                this->prefix().view(),                                                                 \
                                std::source_location::current(),                                                       \
                                fmt,                                                                                   \
                                __VA_ARGS__)
    #define LOG_FAT_DISPATCH_() LOG_FAT_STREAM()
    #define LOG_FAT_DISPATCH_TRUE(...) LOG_FAT_FMT(__VA_ARGS__)
    #define LOG_FAT(...) CONCAT(LOG_FAT_DISPATCH_, HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

    // ========== ONCE GUARDS ==========
    #define SCROLL_ONCE_GUARD_                                                                                         \
        []() -> bool {                                                                                                 \
            static bool f_ = false;                                                                                    \
            return !std::exchange(f_, true);                                                                           \
        }()

    #define SCROLL_ATOMIC_ONCE_GUARD_                                                                                  \
        []() -> bool {                                                                                                 \
            static std::atomic<bool> f_{false};                                                                        \
            return !f_.exchange(true, std::memory_order_relaxed);                                                      \
        }()

    #define LOG_TRC_ONCE(...)                                                                                          \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        LOG_TRC(__VA_ARGS__)
    #define LOG_DBG_ONCE(...)                                                                                          \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        LOG_DBG(__VA_ARGS__)
    #define LOG_INF_ONCE(...)                                                                                          \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        LOG_INF(__VA_ARGS__)
    #define LOG_WRN_ONCE(...)                                                                                          \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        LOG_WRN(__VA_ARGS__)
    #define LOG_ERR_ONCE(...)                                                                                          \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        LOG_ERR(__VA_ARGS__)
    #define LOG_FAT_ONCE(...)                                                                                          \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        LOG_FAT(__VA_ARGS__)

    #define LOG_TRC_ATOMIC_ONCE(...)                                                                                   \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        LOG_TRC(__VA_ARGS__)
    #define LOG_DBG_ATOMIC_ONCE(...)                                                                                   \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        LOG_DBG(__VA_ARGS__)
    #define LOG_INF_ATOMIC_ONCE(...)                                                                                   \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        LOG_INF(__VA_ARGS__)
    #define LOG_WRN_ATOMIC_ONCE(...)                                                                                   \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        LOG_WRN(__VA_ARGS__)
    #define LOG_ERR_ATOMIC_ONCE(...)                                                                                   \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        LOG_ERR(__VA_ARGS__)
    #define LOG_FAT_ATOMIC_ONCE(...)                                                                                   \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        LOG_FAT(__VA_ARGS__)

#else
    #define LOG_TRC(...) ::demiplane::scroll::DummyStream()
    #define LOG_DBG(...) ::demiplane::scroll::DummyStream()
    #define LOG_INF(...) ::demiplane::scroll::DummyStream()
    #define LOG_WRN(...) ::demiplane::scroll::DummyStream()
    #define LOG_ERR(...) ::demiplane::scroll::DummyStream()
    #define LOG_FAT(...) ::demiplane::scroll::DummyStream()

    #define LOG_TRC_ONCE(...) ::demiplane::scroll::DummyStream()
    #define LOG_DBG_ONCE(...) ::demiplane::scroll::DummyStream()
    #define LOG_INF_ONCE(...) ::demiplane::scroll::DummyStream()
    #define LOG_WRN_ONCE(...) ::demiplane::scroll::DummyStream()
    #define LOG_ERR_ONCE(...) ::demiplane::scroll::DummyStream()
    #define LOG_FAT_ONCE(...) ::demiplane::scroll::DummyStream()

    #define LOG_TRC_ATOMIC_ONCE(...) ::demiplane::scroll::DummyStream()
    #define LOG_DBG_ATOMIC_ONCE(...) ::demiplane::scroll::DummyStream()
    #define LOG_INF_ATOMIC_ONCE(...) ::demiplane::scroll::DummyStream()
    #define LOG_WRN_ATOMIC_ONCE(...) ::demiplane::scroll::DummyStream()
    #define LOG_ERR_ATOMIC_ONCE(...) ::demiplane::scroll::DummyStream()
    #define LOG_FAT_ATOMIC_ONCE(...) ::demiplane::scroll::DummyStream()
#endif

// ============================================================================
// LOG_DIRECT_* — explicit prefix parameter. Used by test code and rare
// non-LoggerProvider call sites.
// ============================================================================
#ifdef DMP_ENABLE_LOGGING
    #define LOG_DIRECT_FMT(logger_ptr, level, prefix, fmt, ...)                                                        \
        (logger_ptr)->log(level, prefix, std::source_location::current(), fmt, __VA_ARGS__)

    #define SLOG_DIRECT_FMT(logger_ptr, level, prefix)                                                                 \
        (logger_ptr)->stream(level, prefix, std::source_location::current())

    #define LOG_DIRECT_FMT_TRC(logger_ptr, prefix, fmt, ...)                                                           \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Trace, prefix, fmt, __VA_ARGS__)
    #define LOG_DIRECT_FMT_DBG(logger_ptr, prefix, fmt, ...)                                                           \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Debug, prefix, fmt, __VA_ARGS__)
    #define LOG_DIRECT_FMT_INF(logger_ptr, prefix, fmt, ...)                                                           \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Info, prefix, fmt, __VA_ARGS__)
    #define LOG_DIRECT_FMT_WRN(logger_ptr, prefix, fmt, ...)                                                           \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Warning, prefix, fmt, __VA_ARGS__)
    #define LOG_DIRECT_FMT_ERR(logger_ptr, prefix, fmt, ...)                                                           \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Error, prefix, fmt, __VA_ARGS__)
    #define LOG_DIRECT_FMT_FAT(logger_ptr, prefix, fmt, ...)                                                           \
        LOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Fatal, prefix, fmt, __VA_ARGS__)

    #define LOG_DIRECT_STREAM_TRC(logger_ptr, prefix)                                                                  \
        SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Trace, prefix)
    #define LOG_DIRECT_STREAM_DBG(logger_ptr, prefix)                                                                  \
        SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Debug, prefix)
    #define LOG_DIRECT_STREAM_INF(logger_ptr, prefix)                                                                  \
        SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Info, prefix)
    #define LOG_DIRECT_STREAM_WRN(logger_ptr, prefix)                                                                  \
        SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Warning, prefix)
    #define LOG_DIRECT_STREAM_ERR(logger_ptr, prefix)                                                                  \
        SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Error, prefix)
    #define LOG_DIRECT_STREAM_FAT(logger_ptr, prefix)                                                                  \
        SLOG_DIRECT_FMT(logger_ptr, ::demiplane::scroll::LogLevel::Fatal, prefix)
#else
    #define LOG_DIRECT_FMT(logger_ptr, level, prefix, fmt, ...) ((void)0)
    #define SLOG_DIRECT_FMT(logger_ptr, level, prefix) ::demiplane::scroll::DummyStream()

    #define LOG_DIRECT_FMT_TRC(logger_ptr, prefix, fmt, ...) ((void)0)
    #define LOG_DIRECT_FMT_DBG(logger_ptr, prefix, fmt, ...) ((void)0)
    #define LOG_DIRECT_FMT_INF(logger_ptr, prefix, fmt, ...) ((void)0)
    #define LOG_DIRECT_FMT_WRN(logger_ptr, prefix, fmt, ...) ((void)0)
    #define LOG_DIRECT_FMT_ERR(logger_ptr, prefix, fmt, ...) ((void)0)
    #define LOG_DIRECT_FMT_FAT(logger_ptr, prefix, fmt, ...) ((void)0)

    #define LOG_DIRECT_STREAM_TRC(logger_ptr, prefix) ::demiplane::scroll::DummyStream()
    #define LOG_DIRECT_STREAM_DBG(logger_ptr, prefix) ::demiplane::scroll::DummyStream()
    #define LOG_DIRECT_STREAM_INF(logger_ptr, prefix) ::demiplane::scroll::DummyStream()
    #define LOG_DIRECT_STREAM_WRN(logger_ptr, prefix) ::demiplane::scroll::DummyStream()
    #define LOG_DIRECT_STREAM_ERR(logger_ptr, prefix) ::demiplane::scroll::DummyStream()
    #define LOG_DIRECT_STREAM_FAT(logger_ptr, prefix) ::demiplane::scroll::DummyStream()
#endif

// ============================================================================
// COMPONENT_LOG_* — prefix sourced from a class-scope `_dmp_scroll_class_prefix`
// declared by SCROLL_COMPONENT_PREFIX. Usable only inside class member functions.
// ============================================================================
#ifdef DMP_COMPONENT_LOGGING
    #define COMPONENT_LOG_TRC()                                                                                        \
        LOG_DIRECT_STREAM_TRC(::demiplane::scroll::ComponentLoggerManager::get(), _dmp_scroll_class_prefix.view())
    #define COMPONENT_LOG_DBG()                                                                                        \
        LOG_DIRECT_STREAM_DBG(::demiplane::scroll::ComponentLoggerManager::get(), _dmp_scroll_class_prefix.view())
    #define COMPONENT_LOG_INF()                                                                                        \
        LOG_DIRECT_STREAM_INF(::demiplane::scroll::ComponentLoggerManager::get(), _dmp_scroll_class_prefix.view())
    #define COMPONENT_LOG_WRN()                                                                                        \
        LOG_DIRECT_STREAM_WRN(::demiplane::scroll::ComponentLoggerManager::get(), _dmp_scroll_class_prefix.view())
    #define COMPONENT_LOG_ERR()                                                                                        \
        LOG_DIRECT_STREAM_ERR(::demiplane::scroll::ComponentLoggerManager::get(), _dmp_scroll_class_prefix.view())
    #define COMPONENT_LOG_FAT()                                                                                        \
        LOG_DIRECT_STREAM_FAT(::demiplane::scroll::ComponentLoggerManager::get(), _dmp_scroll_class_prefix.view())

    #define COMPONENT_LOG_ENTER_FUNCTION() COMPONENT_LOG_INF() << "Entering function " << __func__
    #define COMPONENT_LOG_LEAVE_FUNCTION() COMPONENT_LOG_INF() << "Leaving function " << __func__

    #define COMPONENT_LOG_TRC_ONCE()                                                                                   \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        COMPONENT_LOG_TRC()
    #define COMPONENT_LOG_DBG_ONCE()                                                                                   \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        COMPONENT_LOG_DBG()
    #define COMPONENT_LOG_INF_ONCE()                                                                                   \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        COMPONENT_LOG_INF()
    #define COMPONENT_LOG_WRN_ONCE()                                                                                   \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        COMPONENT_LOG_WRN()
    #define COMPONENT_LOG_ERR_ONCE()                                                                                   \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        COMPONENT_LOG_ERR()
    #define COMPONENT_LOG_FAT_ONCE()                                                                                   \
        if (SCROLL_ONCE_GUARD_)                                                                                        \
        COMPONENT_LOG_FAT()

    #define COMPONENT_LOG_TRC_ATOMIC_ONCE()                                                                            \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        COMPONENT_LOG_TRC()
    #define COMPONENT_LOG_DBG_ATOMIC_ONCE()                                                                            \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        COMPONENT_LOG_DBG()
    #define COMPONENT_LOG_INF_ATOMIC_ONCE()                                                                            \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        COMPONENT_LOG_INF()
    #define COMPONENT_LOG_WRN_ATOMIC_ONCE()                                                                            \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        COMPONENT_LOG_WRN()
    #define COMPONENT_LOG_ERR_ATOMIC_ONCE()                                                                            \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        COMPONENT_LOG_ERR()
    #define COMPONENT_LOG_FAT_ATOMIC_ONCE()                                                                            \
        if (SCROLL_ATOMIC_ONCE_GUARD_)                                                                                 \
        COMPONENT_LOG_FAT()
#else
    #define COMPONENT_LOG(level, message) ((void)0)
    #define COMPONENT_LOG_TRC() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_DBG() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_INF() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_WRN() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_ERR() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_FAT() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_ENTER_FUNCTION() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_LEAVE_FUNCTION() ::demiplane::scroll::DummyStream()

    #define COMPONENT_LOG_TRC_ONCE() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_DBG_ONCE() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_INF_ONCE() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_WRN_ONCE() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_ERR_ONCE() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_FAT_ONCE() ::demiplane::scroll::DummyStream()

    #define COMPONENT_LOG_TRC_ATOMIC_ONCE() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_DBG_ATOMIC_ONCE() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_INF_ATOMIC_ONCE() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_WRN_ATOMIC_ONCE() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_ERR_ATOMIC_ONCE() ::demiplane::scroll::DummyStream()
    #define COMPONENT_LOG_FAT_ATOMIC_ONCE() ::demiplane::scroll::DummyStream()
#endif
