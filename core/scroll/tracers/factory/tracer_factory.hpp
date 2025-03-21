#pragma once

#include "../console_tracer.hpp"
#include "../file_tracer.hpp"


namespace demiplane::scroll {

    class TracerFactory final {
    public:
        template <class Service>
        static std::shared_ptr<TracerInterface> create_console_tracer(ConsoleTracerConfig cfg) {
            return std::make_shared<ConsoleTracer<Service>>(std::make_shared<ConsoleTracerConfig>(std::move(cfg)));
        }
        static std::shared_ptr<TracerInterface> create_default_console_tracer() {
            return std::make_shared<ConsoleTracer<NoName>>(
                std::make_shared<ConsoleTracerConfig>(ScrollConfigFactory::create_default_console_tracer_config()));
        }
        template <class Service>
        static std::shared_ptr<TracerInterface> create_file_tracer(FileTracerConfig cfg) {
            return std::make_shared<FileTracer<Service>>(std::make_shared<FileTracerConfig>(std::move(cfg)));
        }
    };
} // namespace demiplane::scroll
