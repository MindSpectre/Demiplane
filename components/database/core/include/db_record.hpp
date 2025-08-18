#pragma once

#include <gears_hash.hpp>
#include <boost/unordered/unordered_map.hpp>

#include "db_core_fwd.hpp"
#include "db_field.hpp"

namespace demiplane::db {
    class Record final {
    public:
        explicit Record(TableSchemaPtr schema);

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
        [[nodiscard]] TableSchemaPtr schema_ptr() const;
        [[nodiscard]] std::size_t field_count() const;

        // Iterator support
        std::vector<Field>::iterator begin();

        std::vector<Field>::iterator end();

        [[nodiscard]] std::vector<Field>::const_iterator begin() const;

        [[nodiscard]] std::vector<Field>::const_iterator end() const;

    private:
        TableSchemaPtr schema_;
        std::vector<Field> fields_;
        boost::unordered_map<std::string, std::size_t, gears::StringHash, gears::StringEqual> field_index_;
    };
}
