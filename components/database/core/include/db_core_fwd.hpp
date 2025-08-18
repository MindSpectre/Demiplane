#pragma once

#include <gears_templates.hpp>
#include <memory>


namespace demiplane::db {
    class QueryVisitor;

    template <typename T>
    class TableColumn;

    class DynamicColumn;

    class AllColumns;

    template <typename T>
    concept IsTableColumn = gears::is_specialization_of_v<std::remove_cvref_t<T>, TableColumn>;

    template <typename T>
    concept IsDynamicColumn = std::is_same_v<std::remove_cvref_t<T>, DynamicColumn>;

    template <typename T>
    concept IsAllColumns = std::is_same_v<std::remove_cvref_t<T>, AllColumns>;

    template <typename T>
    concept IsColumn = IsDynamicColumn<T> ||
                       IsTableColumn<T> ||
                       IsAllColumns<T>;

    struct FieldSchema;

    class Field;

    class TableSchema;

    using TableSchemaPtr = std::shared_ptr<const TableSchema>;

    template <typename TablePtr>
    concept IsTableSchema = std::is_same_v<std::remove_cvref_t<TableSchemaPtr>, TablePtr>;
}
