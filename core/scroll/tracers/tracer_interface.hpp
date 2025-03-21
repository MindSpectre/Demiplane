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

#include "scroll_config_factory.hpp"
#include "entry_processor.hpp"
namespace demiplane::scroll {


    /**
     * @brief Main purpose is logging all in time
     */
    class TracerInterface : Immutable {
    public:
        virtual ~TracerInterface() = default;
        explicit TracerInterface(const std::shared_ptr<TracerConfigInterface>& config) : processor_(config->get_entry_cfg()){}

        // Log a message (with context information).
        virtual void log(
            LogLevel level, std::string_view message, const char* file, int line, const char* function) = 0;
    protected:
        EntryProcessor processor_;
    };
} // namespace demiplane::tracing


// -----------------------------------------------------------------------------
// Macros for easy tracing.
// If ENABLE_TRACING is defined at compile time, these macros expand to logging calls;
// otherwise, they are no-ops.
#ifdef ENABLE_TRACING

#define TRACE_LOG(trace, level, message) (trace)->log((level), (message), __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define TRACE_DEBUG(trace, message) TRACE_LOG(trace, demiplane::scroll::LogLevel::Debug, message)
#define TRACE_INFO(trace, message)  TRACE_LOG(trace, demiplane::scroll::LogLevel::Info, message)
#define TRACE_WARN(trace, message)  TRACE_LOG(trace, demiplane::scroll::LogLevel::Warning, message)
#define TRACE_ERROR(trace, message) TRACE_LOG(trace, demiplane::scroll::LogLevel::Error, message)
#define TRACE_FATAL(trace, message) TRACE_LOG(trace, demiplane::scroll::LogLevel::Fatal, message)
#define TRACE_LOG_STREAM(trace, level) \
    tracing::Tracer::LogStream(trace, (level), __FILE__, __LINE__, __PRETTY_FUNCTION__)

#else

// When tracing is disabled, all macros do nothing.
#define TRACE_LOG(tracer, level, message) ((void) 0)
#define TRACE_DEBUG(tracer, message)      ((void) 0)
#define TRACE_INFO(tracer, message)       ((void) 0)
#define TRACE_WARN(tracer, message)       ((void) 0)
#define TRACE_ERROR(tracer, message)      ((void) 0)
#define TRACE_FATAL(tracer, message)      ((void) 0)

#endif
