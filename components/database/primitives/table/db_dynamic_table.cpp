#include "db_dynamic_table.hpp"

#include <db_column.hpp>

namespace demiplane::db {

    Column DynamicTable::column(const std::string_view field_name) const {
        auto* field = get_field_schema(field_name);
        if (!field) {
            throw std::runtime_error{std::string{"Unknown column: "} + std::string{field_name} + " in table " +
                                     table_name_};
        }
        return Column{field->name, table_name_};
    }

    DynamicTable& DynamicTable::primary_key(const std::string_view field_name) {
        if (auto* field = get_field_schema(field_name)) {
            field->is_primary_key = true;
            field->is_nullable    = false;
        }
        return *this;
    }

    DynamicTable& DynamicTable::nullable(const std::string_view field_name, const bool is_null) {
        if (auto* field = get_field_schema(field_name)) {
            field->is_nullable = is_null;
        }
        return *this;
    }

    DynamicTable& DynamicTable::foreign_key(const std::string_view field_name,
                                            const std::string_view ref_table,
                                            const std::string_view ref_column) {
        if (auto* field = get_field_schema(field_name)) {
            field->is_foreign_key = true;
            field->foreign_table  = ref_table;
            field->foreign_column = ref_column;
        }
        return *this;
    }

    DynamicTable& DynamicTable::unique(const std::string_view field_name) {
        if (auto* field = get_field_schema(field_name)) {
            field->is_unique = true;
        }
        return *this;
    }

    DynamicTable& DynamicTable::indexed(const std::string_view field_name) {
        if (auto* field = get_field_schema(field_name)) {
            field->is_indexed = true;
        }
        return *this;
    }

    const DynamicFieldSchema* DynamicTable::get_field_schema(const std::string_view name) const {
        const auto it = field_index_.find(name);
        return it != field_index_.end() ? &fields_[it->second] : nullptr;
    }

    DynamicFieldSchema* DynamicTable::get_field_schema(const std::string_view name) {
        const auto it = field_index_.find(name);
        return it != field_index_.end() ? &fields_[it->second] : nullptr;
    }

    std::vector<std::string> DynamicTable::field_names() const {
        std::vector<std::string> names;
        names.reserve(fields_.size());
        for (const auto& field : fields_) {
            names.push_back(field.name);
        }
        return names;
    }
}  // namespace demiplane::db
