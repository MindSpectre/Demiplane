#pragma once
#include <memory>
#include <string>
#include <typeindex>
#include <vector>

#include <boost/unordered_map.hpp>
#include <gears_hash.hpp>

#include "db_core_fwd.hpp"

namespace demiplane::db {
    // Enhanced TableSchema with type-safe column access
    class TableSchema {
        public:
        explicit TableSchema(std::string table_name);
        // Enhanced builder pattern with type information
        template <typename T>
        TableSchema& add_field(std::string name, std::string db_type);

        // Overload for runtime type specification
        TableSchema& add_field(std::string name, std::string db_type, std::type_index cpp_type);

        // Type-safe column accessor
        template <typename T>
        TableColumn<T> column(std::string_view field_name) const;


        // Existing methods
        TableSchema& primary_key(std::string_view field_name);
        TableSchema& nullable(std::string_view field_name, bool is_null = true);
        TableSchema& foreign_key(std::string_view field_name, std::string_view ref_table, std::string_view ref_column);
        TableSchema& unique(std::string_view field_name);
        TableSchema& indexed(std::string_view field_name);

        [[nodiscard]] const FieldSchema* get_field_schema(std::string_view name) const;
        FieldSchema* get_field_schema(std::string_view name);

        [[nodiscard]] const std::string& table_name() const;

        [[nodiscard]] std::size_t field_count() const;

        [[nodiscard]] const std::vector<std::unique_ptr<FieldSchema>>& fields() const;

        // Get all column names
        [[nodiscard]] std::vector<std::string> field_names() const;

        [[nodiscard]] std::shared_ptr<TableSchema> clone();

        [[nodiscard]] static std::shared_ptr<TableSchema> make_ptr(std::string name);

        private:
        std::string table_name_;
        std::vector<std::unique_ptr<FieldSchema>> fields_;
        boost::unordered_map<std::string, std::size_t, gears::StringHash, gears::StringEqual> field_index_;
    };
}  // namespace demiplane::db

#include "../source/db_table_schema.inl"
