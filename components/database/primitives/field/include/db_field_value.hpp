#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <variant>

namespace demiplane::db {
    using FieldValue = std::variant<std::monostate,
                                    bool,
                                    std::int32_t,
                                    std::int64_t,
                                    double,
                                    std::string,
                                    // std::pmr::string,
                                    // std::string_view, todo? askable
                                    std::span<const uint8_t>  // Zero-copy binary data
                                    >;
}
