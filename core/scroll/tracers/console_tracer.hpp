#pragma once

#include <iostream>

#include "../tracer_interface.hpp"

namespace demiplane::scroll {
    template <class Service>
    class ConsoleTracer final : public TracerInterface<Service> {
    public:
        explicit ConsoleTracer(std::shared_ptr<ConsoleTracerConfig> config)
            : TracerInterface<Service>(config), config_(std::move(config)) {}
        void log(LogLevel level, const std::string_view message, const char* file, const int line,
            const char* function) override {
            if (static_cast<int>(level) < static_cast<int>(config_->get_threshold())) {
                return;
            }
            if (thread_local bool header_written = false; this->processor_.config().enable_header && !header_written) {
                std::cout << this->processor_.make_header() << std::endl;
                header_written = true;
            }
            const std::string entry = this->processor_.create_entry(level, message, file, line, function, Service::name());
            std::cout << entry << std::endl;
        }

    private:
        std::shared_ptr<ConsoleTracerConfig> config_;
    };
} // namespace demiplane::scroll
