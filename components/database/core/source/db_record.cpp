#include "db_record.hpp"

#include "db_field.hpp"
#include "db_table_schema.hpp"

namespace demiplane::db {
    Record::Record(TableSchemaPtr schema)
        : schema_(std::move(schema)) {
        if (!schema_)
            throw std::invalid_argument("Schema cannot be null");

        fields_.reserve(schema_->field_count());
        for (const auto& field_schema : schema_->fields()) {
            field_index_[field_schema->name] = fields_.size();
            fields_.emplace_back(field_schema.get());
        }
    }

    Field& Record::operator[](const std::string& field_name) {
        const auto it = field_index_.find(field_name);
        if (it == field_index_.end()) {
            throw std::out_of_range("Field not found: " + field_name);
        }
        return fields_[it->second];
    }

    const Field& Record::operator[](const std::string& field_name) const {
        const auto it = field_index_.find(field_name);
        if (it == field_index_.end()) {
            throw std::out_of_range("Field not found: " + field_name);
        }
        return fields_[it->second];
    }

    Field& Record::at(const std::size_t index) {
        if (index >= fields_.size()) {
            throw std::out_of_range("Field index out of range");
        }
        return fields_[index];
    }

    const Field& Record::at(const std::size_t index) const {
        if (index >= fields_.size()) {
            throw std::out_of_range("Field index out of range");
        }
        return fields_[index];
    }

    Field* Record::get_field(const std::string_view name) {
        const auto it = field_index_.find(name);
        return it != field_index_.end() ? &fields_[it->second] : nullptr;
    }

    const Field* Record::get_field(const std::string_view name) const {
        const auto it = field_index_.find(name);
        return it != field_index_.end() ? &fields_[it->second] : nullptr;
    }

    const TableSchema& Record::schema() const {
        return *schema_;
    }

    TableSchemaPtr Record::schema_ptr() const {
        return schema_;
    }

    std::size_t Record::field_count() const {
        return fields_.size();
    }

    std::vector<Field>::iterator Record::begin() {
        return fields_.begin();
    }

    std::vector<Field>::iterator Record::end() {
        return fields_.end();
    }

    std::vector<Field>::const_iterator Record::begin() const {
        return fields_.begin();
    }

    std::vector<Field>::const_iterator Record::end() const {
        return fields_.end();
    }
}  // namespace demiplane::db
