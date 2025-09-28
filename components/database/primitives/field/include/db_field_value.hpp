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
                                    std::string_view,
                                    std::span<const uint8_t>  // Zero-copy binary data
                                    >;

    template <typename T>
    concept IsFieldValueType = std::is_constructible_v<FieldValue, T>;
        // std::is_same_v<T, std::monostate> || std::is_same_v<T, bool> || std::is_same_v<T, std::int32_t> ||
        // std::is_same_v<T, std::int64_t> || std::is_same_v<T, double> || std::is_same_v<T, std::string> ||
        // std::is_same_v<T, std::string_view> || std::is_same_v<T, std::span<const uint8_t>>;
}  // namespace demiplane::db
