#include "db_field.hpp"

#include <stdexcept>

#include "db_field_schema.hpp"


namespace demiplane::db {
    Field::Field(const FieldSchema* schema)
        : schema_(schema) {
        if (!schema_) {
            throw std::invalid_argument("Schema cannot be null");
        }
    }

    Field::Field(Field&& other) noexcept
        : value_(std::move(other.value_)),
          schema_(other.schema_) {
    }

    Field& Field::operator=(const Field& other) {
        if (this != &other) {
            value_  = other.value_;
            schema_ = other.schema_;
        }
        return *this;
    }

    Field& Field::operator=(Field&& other) noexcept {
        if (this != &other) {
            value_  = std::move(other.value_);
            schema_ = other.schema_;
        }
        return *this;
    }

    bool Field::is_null() const {
        return std::holds_alternative<std::monostate>(value_);
    }

    const FieldValue& Field::raw_value() const& {
        return value_;
    }

    FieldValue Field::raw_value() && {
        return std::move(value_);
    }

    const FieldSchema& Field::schema() const {
        return *schema_;
    }

    const std::string& Field::name() const {
        return schema_->name;
    }
}  // namespace demiplane::db
