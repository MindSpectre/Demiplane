#pragma once

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
    Column<T> TableSchema::column(const std::string& name) const {
        auto* field = get_field_schema(name);
        if (!field) {
            throw std::runtime_error("Unknown column: " + name + " in table " + table_name_);
        }
        return field->as_column<T>(table_name_.c_str());
    }

}