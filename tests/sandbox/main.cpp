#define ENABLE_TRACING

#include "scroll_tracer.hpp"

using namespace demiplane;
class ServiceX : public HasName<ServiceX> {
    public:
    static constexpr std::string_view name() { return "ServiceX"; }
};
int main() {
    auto cfg = demiplane::scroll::ScrollConfigFactory::create_file_tracer_config({});
    auto tracer = scroll::TracerFactory::create_console_tracer<ServiceX>(
        scroll::ScrollConfigFactory::create_default_console_tracer_config());
    TRACE_DEBUG(tracer, "1234");
    TRACE_INFO(tracer, "1234");
    TRACE_WARN(tracer, "1234");
    TRACE_ERROR(tracer, "1234");
    TRACE_FATAL(tracer, "1234");
    return 0;
}
