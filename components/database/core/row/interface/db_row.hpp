#pragma once
#include <string_view>
#include <vector>

namespace demiplane::db {
    class Field;
    class Row {
    public:
        virtual ~Row() = default;
        // Copy and move constructors are automatically generated and safe

        // Field access by name (safe)
        virtual Field& operator[](std::string_view field_name) = 0;

        virtual const Field& operator[](std::string_view field_name) const = 0;

        // Field access by index (fast, with bounds checking in debug)
        virtual Field& at(std::size_t index) = 0;

        [[nodiscard]] virtual const Field& at(std::size_t index) const = 0;

        // Safe field access
        virtual Field* get_field(std::string_view name) = 0;

        [[nodiscard]] virtual const Field* get_field(std::string_view name) const = 0;

        // Iterator support
        virtual std::vector<Field>::iterator begin() = 0;

        virtual std::vector<Field>::iterator end() = 0;

        [[nodiscard]] virtual std::vector<Field>::const_iterator begin() const = 0;

        [[nodiscard]] virtual std::vector<Field>::const_iterator end() const = 0;
    };
}  // namespace demiplane::db
