#pragma once
#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace demiplane::db {


    /**
     * @brief A variant for storing field data. Provided with owning and viewable options.
     * @warning Be careful using views(std::string_view, std::span and others non owning data).
     */
    using FieldValue = std::variant<std::monostate,
                                    bool,
                                    char,
                                    std::int16_t,
                                    std::int32_t,
                                    std::int64_t,
                                    std::uint16_t,
                                    std::uint32_t,
                                    std::uint64_t,
                                    float,
                                    double,
                                    std::string,
                                    std::string_view,  // Zero-copy string data
                                    std::vector<std::uint8_t>,
                                    std::span<const std::uint8_t>  // Zero-copy binary data
                                    >;

    template <typename T>
    concept IsFieldValueType = std::constructible_from<FieldValue, T>;
}  // namespace demiplane::db
