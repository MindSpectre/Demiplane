#pragma once

#include <gears_templates.hpp>
#include <memory>

namespace demiplane::db {
    class QueryVisitor;

    template <typename T>
    class Column;

    template <>
    class Column<void>;

    class AllColumns;

    template <typename T>
    concept IsTypedColumn = gears::is_specialization_of_v<std::remove_cvref_t<T>, Column> &&
                            !std::is_same_v<std::remove_cvref_t<T>, Column<void>>;

    template <typename T>
    concept IsUntypedColumn = std::is_same_v<std::remove_cvref_t<T>, Column<void>>;

    template <typename T>
    concept IsAllColumns = std::is_same_v<std::remove_cvref_t<T>, AllColumns>;

    template <typename T>
    concept IsColumn = IsUntypedColumn<T> ||
                       IsTypedColumn<T> ||
                       IsAllColumns<T>;

    struct FieldSchema;

    class Field;

    class TableSchema;

    using TableSchemaPtr = std::shared_ptr<const TableSchema>;

    template <typename TablePtr>
    concept IsTableSchema = std::is_same_v<std::remove_cvref_t<TableSchemaPtr>, TablePtr>;
}
