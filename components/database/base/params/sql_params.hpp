#pragma once

#include <memory>

#include <db_field_value.hpp>

namespace demiplane::db {

    class ParamSink {
    public:
        virtual ~ParamSink() noexcept                         = default;
        virtual std::size_t push(const FieldValue& v)         = 0;  // returns 1-based index
        virtual std::size_t push(FieldValue&& v)              = 0;
        // optional: expose a backend-native “packet” handle
        [[nodiscard]] virtual std::shared_ptr<void> packet() const noexcept = 0;
    };

    struct DialectBindPacket {
        std::unique_ptr<ParamSink> sink;
        std::shared_ptr<void> packet;
    };
}  // namespace demiplane::db
