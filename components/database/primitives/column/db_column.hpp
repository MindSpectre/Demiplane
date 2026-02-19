#pragma once
#include <memory>
#include <string>
#include <utility>

#include <gears_templates.hpp>

#include "db_field_schema.hpp"
namespace demiplane::db {

    class QueryVisitor;

    class DynamicColumn {
    public:
        constexpr explicit DynamicColumn(std::string name, std::string table)
            : name_{std::move(name)},
              context_{std::move(table)} {
        }

        constexpr explicit DynamicColumn(std::string name)
            : name_{std::move(name)} {
        }

        [[nodiscard]] constexpr const std::string& name() const {
            return name_;
        }

        [[nodiscard]] constexpr const std::string& context() const {
            return context_;
        }

        constexpr DynamicColumn& set_context(std::string table) {
            context_ = std::move(table);
            return *this;
        }

        constexpr DynamicColumn& set_name(std::string name) {
            name_ = std::move(name);
            return *this;
        }

        void accept(this auto&& self, QueryVisitor& visitor);

    private:
        std::string name_;
        std::string context_;
    };


    template <typename T>
    class TableColumn {
    public:
        using value_type = T;
        constexpr TableColumn(const FieldSchema* schema, std::shared_ptr<std::string> table, std::string alias)
            : schema_{schema},
              table_{std::move(table)},
              alias_{std::move(alias)} {
        }


        constexpr TableColumn(const FieldSchema* schema, std::shared_ptr<std::string> table)
            : schema_{schema},
              table_{std::move(table)} {
        }


        constexpr TableColumn(const FieldSchema* schema, std::string table, std::string alias)
            : schema_{schema},
              table_{std::make_shared<std::string>(std::move(table))},
              alias_{std::move(alias)} {
        }

        constexpr TableColumn(const FieldSchema* schema, std::string table)
            : schema_{schema},
              table_{std::make_shared<std::string>(std::move(table))} {
        }
        [[nodiscard]] constexpr const FieldSchema* schema() const {
            return schema_;
        }

        [[nodiscard]] constexpr const std::shared_ptr<std::string>& table() const {
            return table_;
        }

        [[nodiscard]] constexpr const std::string& table_name() const {
            return *table_;
        }

        [[nodiscard]] constexpr const std::string& alias() const {
            return alias_;
        }

        [[nodiscard]] constexpr const std::string& name() const {
            return schema_->name;
        }

        constexpr TableColumn as(std::string alias) const {
            return TableColumn{schema_, table_, std::move(alias)};
        }

        [[nodiscard]] constexpr DynamicColumn as_dynamic() const& {
            return DynamicColumn{schema_->name, *table_};
        }

        void accept(this auto&& self, QueryVisitor& visitor);

    private:
        const FieldSchema* schema_;
        std::shared_ptr<std::string> table_;
        std::string alias_;
    };


    // All columns selector
    class AllColumns {
    public:
        constexpr explicit AllColumns(std::shared_ptr<std::string> table)
            : table_{std::move(table)} {
        }

        explicit AllColumns(std::string table)
            : table_{std::make_shared<std::string>(std::move(table))} {
        }

        [[nodiscard]] constexpr const std::string& table_name() const {
            return *table_;
        }

        [[nodiscard]] constexpr const std::shared_ptr<std::string>& table() const {
            return table_;
        }

        [[nodiscard]] constexpr DynamicColumn as_dynamic() const {
            //? Does it work for all dialects?
            return DynamicColumn{"*", table_name()};
        }

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
