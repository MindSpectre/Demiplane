#pragma once

#include "db_dynamic_table.hpp"
#include "db_static_table.hpp"

namespace demiplane::db {

    using DynamicTablePtr = std::shared_ptr<const DynamicTable>;

    template <typename TablePtrT>
    concept IsDynamicTablePtr = std::constructible_from<DynamicTablePtr, std::remove_cvref_t<TablePtrT>>;

    template <typename T>
    concept IsStaticTable = requires(const T& t) {
        { t.table_name() } -> std::convertible_to<std::string_view>;
        { t.field_count() } -> std::convertible_to<std::size_t>;
        { t.provider() } -> std::convertible_to<Providers>;
    } && !IsDynamicTablePtr<T> && !std::constructible_from<std::string, std::remove_cvref_t<T>>;

    template <typename TableT>
    concept IsTable = IsDynamicTablePtr<std::remove_cvref_t<TableT>> || IsStaticTable<std::remove_cvref_t<TableT>> ||
                      std::constructible_from<std::string, std::remove_cvref_t<TableT>> ||
                      std::constructible_from<std::string_view, std::remove_cvref_t<TableT>>;
}  // namespace demiplane::db
