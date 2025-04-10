#pragma once

#include <functional>

#include "../console_tracer.hpp"
#include "../file_tracer.hpp"


namespace demiplane::scroll {

    class TracerFactory final {
    public:
        template <class Service>
        static std::shared_ptr<TracerInterface<Service>> create_console_tracer(ConsoleTracerConfig cfg) {
            return std::make_shared<ConsoleTracer<Service>>(std::make_shared<ConsoleTracerConfig>(std::move(cfg)));
        }
        template <class Service = NoName>
        static std::shared_ptr<TracerInterface<Service>> create_default_console_tracer() {
            return std::make_shared<ConsoleTracer<Service>>(
                std::make_shared<ConsoleTracerConfig>(ScrollConfigFactory::create_default_console_tracer_config()));
        }
        template <class Service>
        static std::shared_ptr<TracerInterface<Service>> create_file_tracer(FileTracerConfig cfg) {
            return std::make_shared<FileTracer<Service>>(std::make_shared<FileTracerConfig>(std::move(cfg)));
        }
    };
    template <class Service>
    class TracerProvider {
    public:
        void set(const std::function<std::shared_ptr<TracerInterface<Service>>()>& tracer_setter) {
            i_tracer = tracer_setter();
        }
        explicit TracerProvider(std::shared_ptr<TracerInterface<Service>> tracer) : i_tracer(std::move(tracer)) {}

        TracerProvider() {
            i_tracer = TracerFactory::create_default_console_tracer<Service>();
        }

    protected:
        std::shared_ptr<TracerInterface<Service>> i_tracer;
    };
} // namespace demiplane::scroll
