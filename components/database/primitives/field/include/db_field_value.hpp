#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <variant>

namespace demiplane::db {


    /**
     * @brief A variant for storing field data. Provided with owning and viewable options.
     * @warning Be careful using views(std::string_view, std::span and others non owning data).
     */
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
}  // namespace demiplane::db
