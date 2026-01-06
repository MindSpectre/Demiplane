#pragma once
#include <ostream>

#include <gears_templates.hpp>

namespace demiplane::db {

    // CONSTRUCTORS


    // Schema-aware constructor - auto-initializes fields from Schema::fields
    template <HasSchemaFields Schema>
    Table::Table(std::string table_name, [[maybe_unused]] Schema schema)
        : table_name_(std::move(table_name)) {
        // Use compile-time reflection to add all fields from Schema
        []<typename... FieldDefs>(gears::type_list<FieldDefs...>, Table* table) {
            // Fold expression to add each field
            (table->add_field<typename FieldDefs::value_type>(std::string(FieldDefs::name()), ""), ...);
        }(typename Schema::fields{}, this);
    }


    // FIELD MANAGEMENT
    template <typename T>
    Table& Table::add_field(std::string name, std::string db_type) {
        auto field      = std::make_unique<FieldSchema>();
        field->name     = std::move(name);
        field->db_type  = std::move(db_type);
        field->cpp_type = std::type_index(typeid(T));

        field_index_[field->name] = fields_.size();
        fields_.push_back(std::move(field));
        return *this;
    }

    // Runtime column accessor
    template <typename T>
    TableColumn<T> Table::column(const std::string_view field_name) const {
        auto* field = get_field_schema(field_name);
        if (!field) {
            std::ostringstream ss;
            ss << "Unknown column: " << field_name << " in table " << table_name_;
            throw std::runtime_error{ss.str()};
        }
        return field->as_column<T>(std::make_shared<std::string>(table_name_));
    }

    // ✨ Compile-time column accessor (new)
    template <IsFieldDef FieldDefT>
    constexpr TableColumn<typename FieldDefT::value_type> Table::column([[maybe_unused]] FieldDefT field_def) const {
        const auto* schema = get_field_schema(field_def.name());
        if (!schema) {
            throw std::runtime_error{std::string{"Field '"} + std::string{field_def.name()} + "' not found in table '" +
                                     table_name_ + "'"};
        }
        return TableColumn<typename FieldDefT::value_type>{schema, std::make_shared<std::string>(table_name_)};
    }


    // Builder methods
    template <IsFieldDef FieldDefT>
    Table& Table::primary_key(FieldDefT field_def) {
        return primary_key(field_def.name());
    }

    template <IsFieldDef FieldDefT>
    Table& Table::nullable(FieldDefT field_def, bool is_null) {
        return nullable(field_def.name(), is_null);
    }

    template <IsFieldDef FieldDefT>
    Table& Table::foreign_key(FieldDefT field_def, std::string_view ref_table, std::string_view ref_column) {
        return foreign_key(field_def.name(), ref_table, ref_column);
    }

    template <IsFieldDef FieldDefT>
    Table& Table::unique(FieldDefT field_def) {
        return unique(field_def.name());
    }

    template <IsFieldDef FieldDefT>
    Table& Table::indexed(FieldDefT field_def) {
        return indexed(field_def.name());
    }

    template <IsFieldDef FieldDefT>
    Table& Table::set_db_type(FieldDefT field_def, std::string db_type) {
        auto* schema = get_field_schema(field_def.name());
        if (!schema) {
            throw std::runtime_error{std::string{"Field '"} + std::string{field_def.name()} + "' not found in table '" +
                                     table_name_ + "'"};
        }
        schema->db_type = std::move(db_type);
        return *this;
    }

    template <IsFieldDef FieldDefT>
    Table& Table::add_db_attribute(FieldDefT field_def, std::string key, std::string value) {
        auto* schema = get_field_schema(field_def.name());
        if (!schema) {
            throw std::runtime_error{std::string{"Field '"} + std::string{field_def.name()} + "' not found in table '" +
                                     table_name_ + "'"};
        }
        schema->db_attributes[std::move(key)] = std::move(value);
        return *this;
    }

}  // namespace demiplane::db
