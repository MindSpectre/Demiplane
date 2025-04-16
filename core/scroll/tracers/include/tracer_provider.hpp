#pragma once

#include "../tracer_factory.hpp"



namespace demiplane::scroll {
    template <class Service = AnonymousClass>
    class TracerProvider {
    public:
        void set_tracer(const std::function<std::shared_ptr<Tracer<Service>>()>& tracer_setter) {
            tracer_ = tracer_setter();
        }

        [[nodiscard]] const std::shared_ptr<Tracer<Service>>& get_tracer() const {
            return tracer_;
        }
        explicit TracerProvider(std::shared_ptr<Tracer<Service>> tracer) : tracer_(std::move(tracer)) {}

        TracerProvider() {
            static_assert(HasStaticName<Service>, "Service must have static name()");
            tracer_ = TracerFactory::create_default_console_tracer<Service>();
        }
        void set_tracer(const std::shared_ptr<Tracer<Service>>& tracer) {
            this->tracer_ = tracer;
        }

    protected:
        std::shared_ptr<Tracer<Service>> tracer_;
    };
} // namespace demiplane::scroll
