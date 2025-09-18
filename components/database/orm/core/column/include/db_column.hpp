#pragma once
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <gears_templates.hpp>

#include "db_field_schema.hpp"
namespace demiplane::db {

    class QueryVisitor;

    class DynamicColumn {
    public:
        DynamicColumn(std::string name, std::string table);

        explicit DynamicColumn(std::string name);

        [[nodiscard]] const std::string& name() const;

        [[nodiscard]] const std::string& context() const;

        DynamicColumn& set_context(std::string table);

        DynamicColumn& set_name(std::string name);


        void accept(this auto&& self, QueryVisitor& visitor);

    private:
        std::string name_;
        std::string context_;
    };


    template <typename T>
    class TableColumn {
    public:
        using value_type = T;
        constexpr TableColumn(const FieldSchema* schema,
                              std::shared_ptr<std::string> table,
                              std::optional<std::string> alias = std::nullopt)
            : schema_(schema),
              table_(std::move(table)),
              alias_(std::move(alias)) {
        }

        constexpr TableColumn(const FieldSchema* schema,
                              std::string table,
                              std::optional<std::string> alias = std::nullopt)
            : schema_(schema),
              table_(std::make_shared<std::string>(std::move(table))),
              alias_(std::move(alias)) {
        }

        [[nodiscard]] const FieldSchema* schema() const {
            return schema_;
        }
        [[nodiscard]] const std::shared_ptr<std::string>& table() const {
            return table_;
        }
        [[nodiscard]] const std::string& table_name() const {
            return *table_;
        }
        [[nodiscard]] const std::optional<std::string>& alias() const {
            return alias_;
        }
        [[nodiscard]] const std::string& name() const {
            return schema_->name;
        }
        TableColumn as(std::string alias) const {
            return TableColumn{schema_, table_, std::move(alias)};
        }
        [[nodiscard]] DynamicColumn as_dynamic() const& {
            return DynamicColumn{schema_->name, *table_};
        }

        void accept(this auto&& self, QueryVisitor& visitor);

    private:
        const FieldSchema* schema_;
        std::shared_ptr<std::string> table_;
        std::optional<std::string> alias_;  // Optional table alias
    };


    // All columns selector
    class AllColumns {
    public:
        explicit AllColumns(std::shared_ptr<std::string> table);

        explicit AllColumns(std::string table);

        [[nodiscard]] const std::string& table_name() const;

        [[nodiscard]] const std::shared_ptr<std::string>& table() const;

        [[nodiscard]] DynamicColumn as_dynamic() const;

        void accept(this auto&& self, QueryVisitor& visitor);

    private:
        std::shared_ptr<std::string> table_;
    };


    // Column creation helpers
    template <typename T>
    constexpr TableColumn<T> col(const FieldSchema* schema, std::string table) {
        return TableColumn<T>{schema, std::move(table)};
    }

    constexpr AllColumns all(std::string table) {
        return AllColumns{std::move(table)};
    }

    constexpr AllColumns all(std::shared_ptr<std::string> table = nullptr) {
        return AllColumns{std::move(table)};
    }

    template <typename T>
    concept IsTableColumn = gears::is_specialization_of_v<std::remove_cvref_t<T>, TableColumn>;

    template <typename T>
    concept IsDynamicColumn = std::is_same_v<std::remove_cvref_t<T>, DynamicColumn>;

    template <typename T>
    concept IsAllColumns = std::is_same_v<std::remove_cvref_t<T>, AllColumns>;

    template <typename T>
    concept IsColumn = IsDynamicColumn<T> || IsTableColumn<T> || IsAllColumns<T>;

}  // namespace demiplane::db
