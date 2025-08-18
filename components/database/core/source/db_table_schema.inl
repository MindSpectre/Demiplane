#pragma once
#include <ostream>
#include "db_field_schema.hpp"

namespace demiplane::db {
    template <typename T>
    TableSchema& TableSchema::add_field(std::string name, std::string db_type) {
        auto field      = std::make_unique<FieldSchema>();
        field->name     = std::move(name);
        field->db_type  = std::move(db_type);
        field->cpp_type = std::type_index(typeid(T));

        field_index_[field->name] = fields_.size();
        fields_.push_back(std::move(field));
        return *this;
    }

    // Implementation of TableSchema::column
    template <typename T>
    TableColumn<T> TableSchema::column(const std::string_view field_name) const {
        auto* field = get_field_schema(field_name);
        if (!field) {
            std::ostringstream ss;
            ss << "Unknown column: " << field_name << " in table " << table_name_;
            throw std::runtime_error(ss.str());
        }
        return field->as_column<T>(std::make_shared<std::string>(table_name_));
    }
}