#pragma once
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <json/json.h>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "../../result/include/ires.hpp"
#include "../../utilities/include/chrono_utils.hpp"
#include "../colors/colors.hpp"
#include "../entry/entry_processor.hpp"
namespace demiplane::tracing {


    /**
     * @brief Main purpose is logging all in time
     */

    class Tracer {
    public:
        virtual ~Tracer() = default;
        Tracer()          = default;
        explicit Tracer(const TracerConfig& config) : processor_(config.entry_cfg) {}
        // Default constructor: uses default settings and adds a default log file.
        Tracer(const Tracer&)            = delete;
        Tracer& operator=(const Tracer&) = delete;

        // Log a message (with context information).
        virtual void log(
            LogLevel level, std::string_view message, const char* file, int line, const char* function) = 0;
        // Stream intercept helper.
        // This helper collects output via operator<< and logs the result when destroyed.
        [[nodiscard]] EntryProcessor& processor() {
            return processor_;
        }
        void set_processor(const EntryProcessor& processor) {
            processor_ = processor;
        }

    protected:
        EntryProcessor processor_;
    };
} // namespace demiplane::tracing


// -----------------------------------------------------------------------------
// Macros for easy tracing.
// If ENABLE_TRACING is defined at compile time, these macros expand to logging calls;
// otherwise, they are no-ops.
#ifdef ENABLE_TRACING

#define TRACE_LOG(scroll, level, message) (scroll).log((level), (message), __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define TRACE_DEBUG(scroll, message) TRACE_LOG(scroll, demiplane::tracing::LogLevel::Debug, message)
#define TRACE_INFO(scroll, message)  TRACE_LOG(scroll, demiplane::tracing::LogLevel::Info, message)
#define TRACE_WARN(scroll, message)  TRACE_LOG(scroll, demiplane::tracing::LogLevel::Warning, message)
#define TRACE_ERROR(scroll, message) TRACE_LOG(scroll, demiplane::tracing::LogLevel::Error, message)
#define TRACE_FATAL(scroll, message) TRACE_LOG(scroll, demiplane::tracing::LogLevel::Fatal, message)
#define TRACE_LOG_STREAM(scroll, level) \
    tracing::Tracer::LogStream(scroll, (level), __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define TRACE_DEBUG_STREAM(scroll) TRACE_LOG_STREAM(scroll, demiplane::tracing::LogLevel::Debug)
#define TRACE_INFO_STREAM(scroll)  TRACE_LOG_STREAM(scroll, demiplane::tracing::LogLevel::Info)
#define TRACE_WARN_STREAM(scroll)  TRACE_LOG_STREAM(scroll, demiplane::tracing::LogLevel::Warning)
#define TRACE_ERROR_STREAM(scroll) TRACE_LOG_STREAM(scroll, demiplane::tracing::LogLevel::Error)
#define TRACE_FATAL_STREAM(scroll) TRACE_LOG_STREAM(scroll, demiplane::tracing::LogLevel::Fatal)
#else

// When tracing is disabled, all macros do nothing.
#define TRACE_LOG(tracer, level, message) ((void) 0)
#define TRACE_DEBUG(tracer, message)      ((void) 0)
#define TRACE_INFO(tracer, message)       ((void) 0)
#define TRACE_WARN(tracer, message)       ((void) 0)
#define TRACE_ERROR(tracer, message)      ((void) 0)
#define TRACE_FATAL(tracer, message)      ((void) 0)
#define TRACE_LOG_STREAM(tracer, level) \
    ;                                   \
    if (false)                          \
    std::cerr
#define TRACE_DEBUG_STREAM(tracer) \
    ;                              \
    if (false)                     \
    std::cerr
#define TRACE_INFO_STREAM(tracer) \
    ;                             \
    if (false)                    \
    std::cerr
#define TRACE_WARN_STREAM(tracer) \
    ;                             \
    if (false)                    \
    std::cerr
#define TRACE_ERROR_STREAM(tracer) \
    ;                              \
    if (false)                     \
    std::cerr

#endif
