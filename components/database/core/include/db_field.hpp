#pragma once
#include <optional>

#include "db_field_schema.hpp"
#include "db_field_value.hpp"

namespace demiplane::db {
    class Field final {
    public:
        explicit Field(const FieldSchema* schema);

        // Copy constructor - safe copying
        Field(const Field& other) = default;

        // Move constructor
        Field(Field&& other) noexcept;

        // Assignment operators
        Field& operator=(const Field& other);

        Field& operator=(Field&& other) noexcept;

        // Type-safe setters
        template <typename T>
        Field& set(T&& value);

        // Zero-copy binary data setter
        Field& set_binary(std::span<const uint8_t> data);

        // Type-safe getters with proper error handling
        template <typename T>
        const T& get() const;

        template <typename T>
        std::optional<T> try_get() const;

        // Convenience getters
        [[nodiscard]] bool is_null() const;

        [[nodiscard]] const FieldValue& raw_value() const;

        [[nodiscard]] const FieldSchema& schema() const;

        [[nodiscard]] const std::string& name() const;

    private:
        FieldValue value_;
        const FieldSchema* schema_; // Non-owning, guaranteed to outlive this field
    };

}

#include "../source/db_field.inl"