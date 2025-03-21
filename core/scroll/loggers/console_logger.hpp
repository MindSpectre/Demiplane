#pragma once
#include "../tracer_interface.hpp"

namespace demiplane::tracing {

    struct ConsoleTracerConfig : TracerConfig {};
    class ConsoleTracer final : public Tracer {
    public:
        void log(LogLevel level, const std::string_view message, const char* file, const int line,
            const char* function) override {
            if (static_cast<int>(level) < static_cast<int>(processor_.config().threshold)) {
                return;
            }
            if (thread_local bool header_written = false; processor_.config().enable_header && !header_written) {
                std::cout << processor_.make_header() << std::endl;
                header_written = true;
            }
            const std::string entry = processor_.create_entry(level, message, file, line, function);
            std::cout << entry << std::endl;
        }
        explicit ConsoleTracer(const ConsoleTracerConfig& config) : Tracer(config) {
            main_cfg = config;
        }
        ConsoleTracer() = default;
    private:
        ConsoleTracerConfig main_cfg;
        // Possible new params for console tracer config
    };

} // namespace demiplane::tracing
