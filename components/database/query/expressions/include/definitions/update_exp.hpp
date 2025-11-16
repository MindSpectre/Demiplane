#pragma once

#include "../basic.hpp"

namespace demiplane::db {

    template <IsTable TableT>
    class UpdateExpr : public Expression<UpdateExpr<TableT>>, public TableHolder<TableT> {
    public:
        template <IsTable TableTp = std::remove_cvref_t<TableT>>
        constexpr explicit UpdateExpr(TableTp&& t) noexcept
            : TableHolder<TableT>{std::forward<TableTp>(t)} {
        }

        template <typename Self, typename StringTp, typename FieldValueTp>
            requires std::constructible_from<std::string, StringTp> && std::constructible_from<FieldValue, FieldValueTp>
        constexpr decltype(auto) set(this Self&& self, StringTp&& column, FieldValueTp&& value) {
            self.assignments_.emplace_back(std::forward<StringTp>(column), std::forward<FieldValueTp>(value));
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr decltype(auto) set(this Self&& self,
                                     const std::initializer_list<std::pair<std::string, FieldValue>> assigns) {
            for (auto& a : assigns) {
                self.assignments_.push_back(a);
            }
            return std::forward<Self>(self);
        }

        template <typename Self, IsCondition ConditionTp>
        constexpr auto where(this Self&& self, ConditionTp&& cond) {
            return UpdateWhereExpr<TableT, std::remove_cvref_t<ConditionTp>>{std::forward<Self>(self),
                                                                             std::forward<ConditionTp>(cond)};
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& assignments(this Self&& self) noexcept {
            return std::forward<Self>(self).assignments_;
        }

    private:
        std::vector<std::pair<std::string, FieldValue>> assignments_;
    };

    template <typename TablePtrTp>
        requires std::constructible_from<TablePtr, TablePtrTp> && (!std::constructible_from<std::string, TablePtrTp>)
    constexpr auto update(TablePtrTp&& table) {
        return UpdateExpr<TablePtr>{std::forward<TablePtrTp>(table)};
    }

    template <typename StringTp>
        requires std::constructible_from<std::string, StringTp>
    constexpr auto update(StringTp&& table_name) {
        return UpdateExpr<std::string>{std::forward<StringTp>(table_name)};
    }

    template <typename StringTp>
        requires std::is_same_v<std::remove_cvref_t<StringTp>, std::string_view> ||
                 std::is_same_v<std::remove_cvref_t<StringTp>, const char*>
    constexpr auto update(StringTp&& table_name) noexcept {
        return DeleteExpr<std::string_view>{std::forward<StringTp>(table_name)};
    }
}  // namespace demiplane::db
