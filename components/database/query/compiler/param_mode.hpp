#pragma once

#include <cstdint>

namespace demiplane::db {

    enum class ParamMode : std::uint8_t {
        Inline,  // Values inlined into SQL text, no params collected
        Tuple,   // Placeholders in SQL, params returned as std::tuple
        Sink     // Placeholders in SQL, params pushed to ParamSink*
    };

}  // namespace demiplane::db
