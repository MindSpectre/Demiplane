#pragma once
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "entry_processor.hpp"
#include "scroll_config_factory.hpp"

#define ENABLE_TRACING
namespace demiplane::scroll {


    /**
     * @brief Main purpose is logging all online
     */
    template <class Service>
    class TracerInterface : Immutable {
    public:
        virtual ~TracerInterface() = default;
        explicit TracerInterface(const std::shared_ptr<TracerConfigInterface>& config)
            : processor_(config->get_entry_cfg()) {}

        // Log a message (with context information).
        virtual void log(
            LogLevel level, std::string_view message, const char* file, int line, const char* function) = 0;
        // Internal temporary logger to stream messages.
        struct InStream {
            TracerInterface& tracer;
            LogLevel level;
            const char* file;
            int line;
            const char* function;
            std::ostringstream stream;

            InStream(TracerInterface& tracer, const LogLevel lvl, const char* file, const int line, const char* func)
                : tracer(tracer), level(lvl), file(file), line(line), function(func) {}

            template <typename T>
            InStream& operator<<(const T& value) {
                stream << value;
                return *this;
            }

            InStream& operator<<(std::ostream& (*manip)(std::ostream&) ) {
                stream << manip;
                return *this;
            }

            ~InStream() {
                tracer.log(level, stream.str(), file, line, function);
            }
        };

        // Create temporary logger
        InStream force_stream(LogLevel level, const char* file, int line, const char* function) {
            return {*this, level, file, line, function};
        }

    protected:
        EntryProcessor processor_;
    };
} // namespace demiplane::scroll


// -----------------------------------------------------------------------------
// Macros for easy tracing.
// If ENABLE_TRACING is defined at compile time, these macros expand to logging calls;
// otherwise, they are no-ops.
#ifdef ENABLE_TRACING

#define TRACE_LOG(tracer, level, message) (tracer)->log((level), (message), __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define TRACE_DEBUG(tracer, message) TRACE_LOG(tracer, demiplane::scroll::LogLevel::Debug, message)
#define TRACE_INFO(tracer, message)  TRACE_LOG(tracer, demiplane::scroll::LogLevel::Info, message)
#define TRACE_WARN(tracer, message)  TRACE_LOG(tracer, demiplane::scroll::LogLevel::Warning, message)
#define TRACE_ERROR(tracer, message) TRACE_LOG(tracer, demiplane::scroll::LogLevel::Error, message)
#define TRACE_FATAL(tracer, message) TRACE_LOG(tracer, demiplane::scroll::LogLevel::Fatal, message)

#define TRACER_STREAM_LOG(tracer, level) (tracer)->force_stream(level, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define TRACER_STREAM_DEBUG() TRACER_STREAM_LOG(i_tracer, demiplane::scroll::LogLevel::Debug)
#define TRACER_STREAM_INFO()  TRACER_STREAM_LOG(i_tracer, demiplane::scroll::LogLevel::Info)
#define TRACER_STREAM_WARN()  TRACER_STREAM_LOG(i_tracer, demiplane::scroll::LogLevel::Warning)
#define TRACER_STREAM_ERROR() TRACER_STREAM_LOG(i_tracer, demiplane::scroll::LogLevel::Error)
#define TRACER_STREAM_FATAL() TRACER_STREAM_LOG(i_tracer, demiplane::scroll::LogLevel::Fatal)

#else

// When tracing is disabled, all macros do nothing.
#define TRACE_LOG(tracer, level, message) ((void) 0)
#define TRACE_DEBUG(tracer, message)      ((void) 0)
#define TRACE_INFO(tracer, message)       ((void) 0)
#define TRACE_WARN(tracer, message)       ((void) 0)
#define TRACE_ERROR(tracer, message)      ((void) 0)
#define TRACE_FATAL(tracer, message)      ((void) 0)
#define TRACER_S_DEBUG(tracer)            (void) 0
#define TRACER_S_INFO(tracer)             (void) 0
#define TRACER_S_WARN(tracer)             (void) 0
#define TRACER_S_ERROR(tracer)            (void) 0
#define TRACER_S_FATAL(tracer)            (void) 0
#endif
