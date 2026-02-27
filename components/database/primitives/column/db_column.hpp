#pragma once
#include <memory>
#include <string>
#include <utility>

#include <gears_concepts.hpp>
#include <gears_templates.hpp>

#include "db_field_schema.hpp"
namespace demiplane::db {

    class DynamicColumn {
    public:
        template <gears::IsStringLike StringTp1, gears::IsStringLike StringTp2>
        constexpr explicit DynamicColumn(StringTp1&& name, StringTp2&& table)
            : name_{std::forward<StringTp1>(name)},
              context_{std::forward<StringTp2>(table)} {
        }

        template <gears::IsStringLike StringTp>
        constexpr explicit DynamicColumn(StringTp&& name)
            : name_{std::forward<StringTp>(name)} {
        }

        [[nodiscard]] constexpr const std::string& name() const noexcept {
            return name_;
        }

        [[nodiscard]] constexpr const std::string& context() const noexcept {
            return context_;
        }

        template <gears::IsStringLike StringTp>
        constexpr DynamicColumn& set_context(StringTp&& table) {
            context_ = std::forward<StringTp>(table);
            return *this;
        }

        template <gears::IsStringLike StringTp>
        constexpr DynamicColumn& set_name(StringTp&& name) {
            name_ = std::forward<StringTp>(name);
            return *this;
        }

        constexpr void accept(this auto&& self, auto& visitor);

    private:
        std::string name_;
        std::string context_{};
    };

    template <typename T>
    class TableColumn {
    public:
        using value_type = T;

        template <gears::IsStringLike StringTp>
        constexpr TableColumn(const FieldSchema* schema, std::shared_ptr<std::string> table, StringTp&& alias)
            : schema_{schema},
              table_{std::move(table)},
              alias_{std::forward<StringTp>(alias)} {
        }


        constexpr TableColumn(const FieldSchema* schema, std::shared_ptr<std::string> table)
            : schema_{schema},
              table_{std::move(table)} {
        }


        template <gears::IsStringLike StringTp>
        constexpr TableColumn(const FieldSchema* schema, std::string table, StringTp&& alias)
            : schema_{schema},
              table_{std::make_shared<std::string>(std::move(table))},
              alias_{std::forward<StringTp>(alias)} {
        }

        template <gears::IsStringLike StringTp>
        constexpr TableColumn(const FieldSchema* schema, StringTp&& table)
            : schema_{schema},
              table_{std::make_shared<std::string>(std::forward<StringTp>(table))} {
        }

        [[nodiscard]] constexpr const FieldSchema* schema() const noexcept {
            return schema_;
        }

        [[nodiscard]] constexpr const std::shared_ptr<std::string>& table() const noexcept {
            return table_;
        }

        [[nodiscard]] constexpr const std::string& table_name() const noexcept {
            return *table_;
        }

        [[nodiscard]] constexpr const std::string& alias() const noexcept {
            return alias_;
        }

        [[nodiscard]] constexpr const std::string& name() const noexcept {
            return schema_->name;
        }

        template <gears::IsStringLike StringTp>
        constexpr TableColumn as(StringTp&& alias) const {
            return TableColumn{schema_, table_, std::forward<StringTp>(alias)};
        }

        [[nodiscard]] constexpr DynamicColumn as_dynamic() const& {
            return DynamicColumn{schema_->name, *table_};
        }

        constexpr void accept(this auto&& self, auto& visitor);

    private:
        const FieldSchema* schema_;
        std::shared_ptr<std::string> table_;
        std::string alias_;
    };


    // All columns selector
    class AllColumns {
    public:
        template <gears::IsStringLike StringTp>
        constexpr explicit AllColumns(StringTp&& table)
            : table_{std::forward<StringTp>(table)} {
        }

        constexpr AllColumns() noexcept = default;

        [[nodiscard]] constexpr const std::string& table_name() const noexcept {
            return table_;
        }

        [[nodiscard]] constexpr DynamicColumn as_dynamic() const {
            //? Does it work for all dialects?
            return DynamicColumn{"*", table_name()};
        }

        constexpr void accept(this auto&& self, auto& visitor);

    private:
        std::string table_;
    };


    // Column creation helpers
    template <typename T>
    constexpr TableColumn<T> col(const FieldSchema* schema, std::string table) {
        return TableColumn<T>{schema, std::move(table)};
    }

    constexpr DynamicColumn col(const char* name) {
        return DynamicColumn{std::string{name}};
    }

    constexpr DynamicColumn col(const char* name, const char* table) {
        return DynamicColumn{std::string{name}, std::string{table}};
    }

    constexpr AllColumns all(std::string table) {
        return AllColumns{std::move(table)};
    }

    constexpr AllColumns all() {
        return AllColumns{};
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
