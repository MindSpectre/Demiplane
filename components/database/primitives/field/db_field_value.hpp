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
     *
     * @note When adding a new type to FieldValue, ensure you also update:
     *   - pg_sql_type_mapping.hpp: Add SqlTypeMapping<T, PostgreSQL> specialization
     *   - pg_type_registry.hpp: Add OID constant if needed
     *   - postgres_params.hpp/.cpp: Add bind_one(T) overload
     *   - postgres_result_views.hpp/.cpp: Add decode_*() and as<T>() support
     *   - postgres_dialect.inl: Add formatting in format_value_impl()
     *   - test_type_mapping.cpp: Update static assertions
     *
     * TODO: Types to consider adding:
     *   - std::chrono::system_clock::time_point (TIMESTAMP/TIMESTAMPTZ)
     *   - std::chrono::year_month_day (DATE)
     *   - std::chrono::hh_mm_ss (TIME)
     *   - UUID type (UUID) - consider std::array<uint8_t, 16> or custom type
     *   - Decimal/Numeric type for exact precision
     *   - JSON type (JSONB)
     *   - Array types (e.g., std::vector<int32_t> for INTEGER[])
     *   - Interval type for time intervals
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
