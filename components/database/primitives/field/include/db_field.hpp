#pragma once
#include <optional>

#include "db_field_value.hpp"

namespace demiplane::db {

    struct FieldSchema;

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

        // Type-safe getters with proper error handling
        template <typename T>
        const T& get() const;

        template <typename T>
        std::optional<T> try_get() const;

        // Convenience getters
        [[nodiscard]] bool is_null() const;

        [[nodiscard]] const FieldValue& raw_value() const&;
        [[nodiscard]] FieldValue raw_value() &&;

        [[nodiscard]] const FieldSchema& schema() const;

        [[nodiscard]] const std::string& name() const;

    private:
        FieldValue value_;
        const FieldSchema* schema_;  // Non-owning, guaranteed to outlive this field
    };
}  // namespace demiplane::db

#include "../source/db_field.inl"
