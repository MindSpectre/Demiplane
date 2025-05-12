#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include "entry_processor.hpp"
#include "scroll_config_factory.hpp"

namespace demiplane::scroll {


    /**
     * @brief Main purpose is logging all online
     */
    template <class>
    class Tracer : gears::Immutable {
    public:
        virtual ~Tracer() = default;
        explicit Tracer(const std::shared_ptr<TracerConfigInterface>& config)
            : processor_(config->get_entry_cfg()) {}

        // Log a message (with context information).
        virtual void log(
            LogLevel level, std::string_view message, const char* file, int line, const char* function) = 0;
        // Internal temporary logger to stream messages.
        struct InStream {
            Tracer& tracer;
            LogLevel level;
            const char* file;
            int line;
            const char* function;
            std::ostringstream stream;

            InStream(Tracer& tracer, const LogLevel lvl, const char* file, const int line, const char* func)
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


