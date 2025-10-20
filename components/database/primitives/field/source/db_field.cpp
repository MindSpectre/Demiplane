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
}  // namespace demiplane::db