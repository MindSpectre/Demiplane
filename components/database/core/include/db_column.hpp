#pragma once
#include <string>

#include <gears_templates.hpp>
#include "db_field_schema.hpp"

namespace demiplane::db {
    class QueryVisitor;

    template <typename T>
    class Column {
    public:
        using value_type = T;

        constexpr Column(const FieldSchema* schema, const char* table, const char* alias = nullptr)
            : schema_(schema),
              table_(table),
              alias_(alias) {}

        [[nodiscard]] const FieldSchema* schema() const {
            return schema_;
        }

        [[nodiscard]] const char* table() const {
            return table_;
        }

        [[nodiscard]] const char* alias() const {
            return alias_ ? alias_ : table_;
        }

        [[nodiscard]] const std::string& name() const {
            return schema_->name;
        }

        // Create aliased column
        Column as(const char* alias) const {
            return Column{schema_, table_, alias};
        }

        void accept(QueryVisitor& visitor) const;

    private:
        const FieldSchema* schema_;
        const char* table_;
        const char* alias_; // Optional table alias
    };

    // Special column for untyped/dynamic queries
    template <>
    class Column<void> {
    public:
        Column(const FieldSchema* schema, const char* table, const char* alias = nullptr)
            : schema_(schema),
              table_(table),
              alias_(alias) {}

        [[nodiscard]] const FieldSchema* schema() const {
            return schema_;
        }

        [[nodiscard]] const char* table() const {
            return table_;
        }

        [[nodiscard]] const char* alias() const {
            return alias_ ? alias_ : table_;
        }

        [[nodiscard]] const std::string& name() const {
            return schema_->name;
        }

        void accept(QueryVisitor& visitor) const;

    private:
        const FieldSchema* schema_;
        const char* table_;
        const char* alias_;
    };

    // All columns selector
    struct AllColumns {
        const char* table = nullptr;

        explicit AllColumns(const char* t = nullptr)
            : table(t) {}
    };

    // Column creation helpers
    template <typename T>
    constexpr Column<T> col(const FieldSchema* schema, const char* table) {
        return Column<T>{schema, table};
    }

    constexpr AllColumns all(const char* table = nullptr) {
        return AllColumns{table};
    }

    template <typename T>
    concept IsColumn = std::is_same_v<T, Column<void>> ||
                       std::is_same_v<T, AllColumns> ||
                       gears::is_specialization_of_v<Column, T>;
}
