#pragma once
#include <memory>
#include <string>
#include <typeindex>
#include <vector>

#include <boost/unordered_map.hpp>
#include <db_field_schema.hpp>
#include <gears_hash.hpp>

namespace demiplane::db {
    // Enhanced Table with type-safe column access
    template <typename T>
    class TableColumn;

    class Table {
    public:
        explicit Table(std::string table_name);
        // Enhanced builder pattern with type information
        template <typename T>
        Table& add_field(std::string name, std::string db_type);

        // Overload for runtime type specification
        Table& add_field(std::string name, std::string db_type, std::type_index cpp_type);

        // Type-safe column accessor
        template <typename T>
        [[nodiscard]] TableColumn<T> column(std::string_view field_name) const;


        // Existing methods
        Table& primary_key(std::string_view field_name);
        Table& nullable(std::string_view field_name, bool is_null = true);
        Table& foreign_key(std::string_view field_name, std::string_view ref_table, std::string_view ref_column);
        Table& unique(std::string_view field_name);
        Table& indexed(std::string_view field_name);

        [[nodiscard]] const FieldSchema* get_field_schema(std::string_view name) const;
        FieldSchema* get_field_schema(std::string_view name);

        [[nodiscard]] const std::string& table_name() const;

        [[nodiscard]] std::size_t field_count() const;

        [[nodiscard]] const std::vector<std::unique_ptr<FieldSchema>>& fields() const;

        // Get all column names
        [[nodiscard]] std::vector<std::string> field_names() const;

        [[nodiscard]] std::shared_ptr<Table> clone();

        [[nodiscard]] static std::shared_ptr<Table> make_ptr(std::string name);

    private:
        std::string table_name_;
        std::vector<std::unique_ptr<FieldSchema>> fields_;
        boost::unordered_map<std::string, std::size_t, gears::StringHash, gears::StringEqual> field_index_;
    };

    using TablePtr = std::shared_ptr<const Table>;

    template <typename TablePtr>
    concept IsTable = std::is_same_v<std::remove_cvref_t<TablePtr>, TablePtr>;

}  // namespace demiplane::db

#include "../source/db_table.inl"
