#pragma once
#include <string>

#include <gears_templates.hpp>
#include <memory>
#include <optional>
#include <utility>

#include "db_field_schema.hpp"

namespace demiplane::db {
    class QueryVisitor;

    template <typename T>
    class Column {
    public:
        using value_type = T;

        constexpr Column(const FieldSchema* schema,
                         std::shared_ptr<std::string> table,
                         std::optional<std::string> alias = std::nullopt)
            : schema_(schema),
              table_(std::move(table)),
              alias_(std::move(alias)) {}

        constexpr Column(const FieldSchema* schema,
                         std::string table,
                         std::optional<std::string> alias = std::nullopt)
            : schema_(schema),
              table_(std::make_shared<std::string>(std::move(table))),
              alias_(std::move(alias)) {}

        [[nodiscard]] const FieldSchema* schema() const {
            return schema_;
        }

        [[nodiscard]] const std::shared_ptr<std::string>& table() const {
            return table_;
        }

        [[nodiscard]] std::string_view view_table() const {
            return *table_;
        }

        [[nodiscard]] const std::optional<std::string>& alias() const {
            return alias_;
        }

        [[nodiscard]] const std::string& name() const {
            return schema_->name;
        }

        // Create aliased column
        Column as(const std::string_view alias) const {
            return Column{schema_, table_, std::string{alias}};
        }

        void accept(QueryVisitor& visitor) const;

    private:
        const FieldSchema* schema_;
        std::shared_ptr<std::string> table_;
        std::optional<std::string> alias_; // Optional table alias
    };

    // Special column for untyped/dynamic queries
    template <>
    class Column<void> {
    public:
        Column(const FieldSchema* schema,
               std::shared_ptr<std::string> table,
               std::optional<std::string> alias = std::nullopt)
            : schema_(schema),
              table_(std::move(table)),
              alias_(std::move(alias)) {}

        Column(const FieldSchema* schema,
               std::string table,
               std::optional<std::string> alias = std::nullopt)
            : schema_(schema),
              table_(std::make_shared<std::string>(std::move(table))),
              alias_(std::move(alias)) {}

        explicit Column(const FieldSchema* schema)
            : schema_(schema),
              table_(nullptr),
              alias_(std::nullopt) {}

        [[nodiscard]] const FieldSchema* schema() const {
            return schema_;
        }

        [[nodiscard]] const std::shared_ptr<std::string>& table() const {
            return table_;
        }

        [[nodiscard]] std::string_view view_table() const {
            return *table_;
        }

        [[nodiscard]] const std::optional<std::string>& alias() const {
            return alias_;
        }

        [[nodiscard]] const std::string& name() const {
            return schema_->name;
        }

        void accept(QueryVisitor& visitor) const;

    private:
        const FieldSchema* schema_;
        std::shared_ptr<std::string> table_;
        std::optional<std::string> alias_;
    };

    // All columns selector
    class AllColumns {
    public:
        explicit AllColumns(std::shared_ptr<std::string> table)
            : table_(std::move(table)) {}

        explicit AllColumns(std::string table)
            : table_(std::make_shared<std::string>(std::move(table))) {}

        [[nodiscard]] std::string_view view_table() const {
            return *table_;
        }

        [[nodiscard]] const std::shared_ptr<std::string>& table() const {
            return table_;
        }

    private:
        std::shared_ptr<std::string> table_;
    };

    // Column creation helpers
    template <typename T>
    constexpr Column<T> col(const FieldSchema* schema, std::string table) {
        return Column<T>{schema, table};
    }

    constexpr AllColumns all(std::string table) {
        return AllColumns{std::move(table)};
    }

    constexpr AllColumns all(std::shared_ptr<std::string> table = nullptr) {
        return AllColumns{std::move(table)};
    }

    template <typename T>
    concept IsColumn = std::is_same_v<T, Column<void>> ||
                       std::is_same_v<T, AllColumns> ||
                       gears::is_specialization_of_v<Column, T>;

    template <typename T>
    concept IsTypedColumn = gears::is_specialization_of_v<Column, T> &&
                            !std::is_same_v<T, Column<void>>;

    template <typename T>
    concept IsUntypedColumn = std::is_same_v<T, Column<void>>;

    template <typename T>
    concept IsAllColumns = std::is_same_v<T, AllColumns>;
}