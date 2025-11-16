#pragma once

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

    template <typename TablePtrTp>
        requires std::constructible_from<TablePtr, std::remove_cvref_t<TablePtrTp>>
    auto delete_from(TablePtrTp&& table) noexcept {
        return DeleteExpr<TablePtr>{std::forward<TablePtrTp>(table)};
    }

    template <typename StringTp>
        requires std::constructible_from<std::string, std::remove_cvref_t<StringTp>>
    auto delete_from(StringTp&& table_name) noexcept {
        return DeleteExpr<std::string>{std::forward<StringTp>(table_name)};
    }
    template <typename StringTp>
        requires std::is_same_v<std::remove_cvref_t<StringTp>, std::string_view> ||
                 std::is_same_v<std::remove_cvref_t<StringTp>, const char*>
    constexpr auto delete_from(StringTp&& table_name) noexcept {
        return DeleteExpr<std::string_view>{std::forward<StringTp>(table_name)};
    }
}  // namespace demiplane::db
