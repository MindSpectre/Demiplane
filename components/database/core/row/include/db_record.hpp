#pragma once

#include <boost/unordered/unordered_map.hpp>
#include <gears_hash.hpp>

namespace demiplane::db {

    class Field;
    class TableSchema;

    class Record final {
    public:
        explicit Record(std::shared_ptr<const TableSchema> schema);

        // Copy and move constructors are automatically generated and safe

        // Field access by name (safe)
        Field& operator[](const std::string& field_name);

        const Field& operator[](const std::string& field_name) const;

        // Field access by index (fast, with bounds checking in debug)
        Field& at(std::size_t index);

        [[nodiscard]] const Field& at(std::size_t index) const;

        // Safe field access
        Field* get_field(std::string_view name);

        [[nodiscard]] const Field* get_field(std::string_view name) const;

        // Metadata access
        [[nodiscard]] const TableSchema& schema() const;
        // Metadata access
        [[nodiscard]] std::shared_ptr<const TableSchema> schema_ptr() const;
        [[nodiscard]] std::size_t field_count() const;

        // Iterator support
        std::vector<Field>::iterator begin();

        std::vector<Field>::iterator end();

        [[nodiscard]] std::vector<Field>::const_iterator begin() const;

        [[nodiscard]] std::vector<Field>::const_iterator end() const;

    private:
        std::shared_ptr<const TableSchema> schema_;
        std::vector<Field> fields_;
        boost::unordered_map<std::string, std::size_t, gears::StringHash, gears::StringEqual> field_index_;
    };
}  // namespace demiplane::db
