#include <demiplane/scroll>

#define ENABLE_TRACING
#include <demiplane/trace_macros>

using namespace demiplane;
class ServiceX  {
    public:
    static constexpr std::string_view name() { return "ServiceX"; }
};
int main() {
    auto cfg          = scroll::ScrollConfigFactory::create_file_tracer_config({});
    const auto tracer = scroll::TracerFactory::create_console_tracer<ServiceX>(
        scroll::ScrollConfigFactory::create_default_console_tracer_config());
    TRACE_DEBUG(tracer, "1234");
    TRACE_INFO(tracer, "1234");
    TRACE_WARN(tracer, "1234");
    TRACE_ERROR(tracer, "1234");
    TRACE_FATAL(tracer, "1234");
    return 0;
}
