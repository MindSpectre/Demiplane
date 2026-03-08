#pragma once

#include <gears_concepts.hpp>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsTable TableT>
    class DeleteExpr : public Expression<DeleteExpr<TableT>>, public TableHolder<TableT> {
    public:
        template <IsTable TableTp = std::remove_cvref_t<TableT>>
        constexpr explicit DeleteExpr(TableTp&& t)
            : TableHolder<TableT>{std::forward<TableTp>(t)} {
        }

        template <typename Self, IsCondition ConditionTp>
        [[nodiscard]] constexpr auto where(this Self&& self, ConditionTp&& cond) noexcept {
            return DeleteWhereExpr<TableT, std::remove_cvref_t<ConditionTp>>{std::forward<Self>(self),
                                                                             std::forward<ConditionTp>(cond)};
        }
    };

    // 1. Pointer
    template <typename DynamicTablePtrTp>
        requires std::constructible_from<DynamicTablePtr, std::remove_cvref_t<DynamicTablePtrTp>> &&
                 (!gears::IsStringLike<DynamicTablePtrTp>)
    auto delete_from(DynamicTablePtrTp&& table) noexcept {
        return DeleteExpr<DynamicTablePtr>{std::forward<DynamicTablePtrTp>(table)};
    }

    // 2. std::string (general)
    template <typename StringTp>
        requires gears::IsStringLike<StringTp> && (!gears::IsStringViewLike<StringTp>)
    constexpr auto delete_from(StringTp&& table_name) noexcept {
        return DeleteExpr<std::string>{std::forward<StringTp>(table_name)};
    }

    // 3. string_view (more specific - wins due to subsumption)
    template <typename StringTp>
        requires gears::IsStringViewLike<StringTp>
    constexpr auto delete_from(StringTp&& table_name) noexcept {
        return DeleteExpr<std::string_view>{std::forward<StringTp>(table_name)};
    }

    // 4. Static table
    template <IsStaticTable TableTp>
    constexpr auto delete_from(const TableTp& table) noexcept {
        return DeleteExpr<TableTp>{table};
    }
}  // namespace demiplane::db
