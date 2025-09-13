#pragma once

#include <boost/unordered/unordered_map.hpp>
#include <gears_hash.hpp>

#include "db_row.hpp"

namespace demiplane::db {

    class Table;

    class Record final : public Row {
    public:
        explicit Record(std::shared_ptr<const Table> schema);

        // Copy and move constructors are automatically generated and safe

        // Field access by name (safe)
        Field& operator[](std::string_view field_name) override;

        const Field& operator[](std::string_view field_name) const override;

        // Field access by index (fast, with bounds checking in debug)
        Field& at(std::size_t index) override;

        [[nodiscard]] const Field& at(std::size_t index) const override;

        // Safe field access
        Field* get_field(std::string_view name) override;

        [[nodiscard]] const Field* get_field(std::string_view name) const override;

        // Metadata access
        [[nodiscard]] const Table& schema() const;
        // Metadata access
        [[nodiscard]] std::shared_ptr<const Table> table_ptr() const;

        [[nodiscard]] std::size_t field_count() const;

        // Iterator support
        std::vector<Field>::iterator begin() override;

        std::vector<Field>::iterator end() override;

        [[nodiscard]] std::vector<Field>::const_iterator begin() const override;

        [[nodiscard]] std::vector<Field>::const_iterator end() const override;

    private:
        std::shared_ptr<const Table> schema_;
        std::vector<Field> fields_;
        boost::unordered_map<std::string, std::size_t, gears::StringHash, gears::StringEqual> field_index_;
    };
}  // namespace demiplane::db
