#include "db_table.hpp"

namespace demiplane::db {
    Table& Table::primary_key(const std::string_view field_name) {
        if (auto* field = get_field_schema(field_name)) {
            field->is_primary_key = true;
            field->is_nullable    = false;
        }
        return *this;
    }

    Table& Table::nullable(const std::string_view field_name, const bool is_null) {
        if (auto* field = get_field_schema(field_name)) {
            field->is_nullable = is_null;
        }
        return *this;
    }

    Table& Table::foreign_key(const std::string_view field_name,
                              const std::string_view ref_table,
                              const std::string_view ref_column) {
        if (auto* field = get_field_schema(field_name)) {
            field->is_foreign_key = true;
            field->foreign_table  = ref_table;
            field->foreign_column = ref_column;
        }
        return *this;
    }

    Table& Table::unique(const std::string_view field_name) {
        if (auto* field = get_field_schema(field_name)) {
            field->is_unique = true;
        }
        return *this;
    }

    Table& Table::indexed(const std::string_view field_name) {
        if (auto* field = get_field_schema(field_name)) {
            field->is_indexed = true;
        }
        return *this;
    }

    const FieldSchema* Table::get_field_schema(const std::string_view name) const {
        const auto it = field_index_.find(name);
        return it != field_index_.end() ? fields_[it->second].get() : nullptr;
    }

    FieldSchema* Table::get_field_schema(const std::string_view name) {
        const auto it = field_index_.find(name);
        return it != field_index_.end() ? fields_[it->second].get() : nullptr;
    }

    std::vector<std::string> Table::field_names() const {
        std::vector<std::string> names;
        names.reserve(fields_.size());
        for (const auto& field : fields_) {
            names.push_back(field->name);
        }
        return names;
    }

    std::shared_ptr<Table> Table::clone() {
        auto cloned = std::make_shared<Table>(table_name_, provider_);

        // Copy fields
        cloned->fields_.reserve(fields_.size());
        for (const auto& field : fields_) {
            if (field) {
                cloned->fields_.push_back(std::make_unique<FieldSchema>(*field));
            }
        }

        // Copy field index
        cloned->field_index_ = field_index_;

        return cloned;
    }

    // Removed constexpr methods - they are now inline in the header
}  // namespace demiplane::db
