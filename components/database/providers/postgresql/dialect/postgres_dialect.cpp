#include "postgres_dialect.hpp"

#include <postgres_params.hpp>

namespace demiplane::db::postgres {

    DialectBindPacket PostgresDialect::make_param_sink(std::pmr::memory_resource* memory_resource) {
        auto sink                          = std::make_unique<ParamSink>(memory_resource);
        const std::shared_ptr<void> packet = sink->packet();

        return DialectBindPacket{.sink = std::move(sink), .packet = packet};
    }

}  // namespace demiplane::db::postgres
