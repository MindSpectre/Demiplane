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
    template <typename TablePtrTp>
        requires std::constructible_from<TablePtr, std::remove_cvref_t<TablePtrTp>> &&
                 (!gears::IsStringLike<TablePtrTp>)
    auto delete_from(TablePtrTp&& table) noexcept {
        return DeleteExpr<TablePtr>{std::forward<TablePtrTp>(table)};
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
}  // namespace demiplane::db
