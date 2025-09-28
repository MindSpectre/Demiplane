#pragma once
#include <optional>

#include "db_field_value.hpp"

namespace demiplane::db {

    struct FieldSchema;

    /**
     * @brief Type-safe database field with schema validation
     *
     * Represents a single database field value with compile-time type safety
     * and runtime schema validation. Provides safe value access with proper
     * error handling for type mismatches and null values.
     */
    class Field final {
    public:
        /**
         * @brief Construct field with schema
         * @param schema Field schema metadata (non-owning pointer)
         */
        explicit Field(const FieldSchema* schema);

        /**
         * @brief Set field value with type validation
         * @tparam T Value type
         * @param value New value
         * @return Reference to this field for chaining
         */
        template <typename T>
        Field& set(T&& value);

        /**
         * @brief Get typed value with validation
         * @tparam T Expected type
         * @return Const reference to value
         * @throws std::bad_variant_access if type mismatch
         * @throws std::logic_error if null value
         */
        template <typename T>
        const T& get() const;

        /**
         * @brief Try get typed value safely
         * @tparam T Expected type
         * @return Optional value or nullopt if type mismatch/null
         */
        template <typename T>
        std::optional<T> try_get() const;

        /**
         * @brief Check if field contains null value
         * @return True if null, false otherwise
         */
        [[nodiscard]] bool is_null() const;

        /**
         * @brief Get raw underlying value (lvalue)
         * @return Const reference to FieldValue
         */
        [[nodiscard]] const FieldValue& raw_value() const&;

        /**
         * @brief Get raw underlying value (rvalue)
         * @return Moved FieldValue
         */
        [[nodiscard]] FieldValue raw_value() &&;

        /**
         * @brief Get field schema metadata
         * @return Const reference to schema
         */
        [[nodiscard]] const FieldSchema& schema() const;

        /**
         * @brief Get field name from schema
         * @return Const reference to field name
         */
        [[nodiscard]] const std::string& name() const;

    private:
        FieldValue value_;
        const FieldSchema* schema_;  // Non-owning, guaranteed to outlive this field
    };
}  // namespace demiplane::db

#include "../source/db_field.inl"
