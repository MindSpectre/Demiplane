#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    // Common base class for GroupBy expressions
    template <typename Derived, typename PreGroupQuery>
    class GroupByExprBase : public Expression<Derived>,
                            public QueryOperations<Derived, AllowHaving, AllowOrderBy, AllowLimit> {
    public:
        template <typename PreGroupQueryTp>
            requires std::constructible_from<PreGroupQuery, PreGroupQueryTp>
        constexpr explicit GroupByExprBase(PreGroupQueryTp&& q) noexcept
            : query_{std::forward<PreGroupQueryTp>(q)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward<Self>(self).query_;
        }

    protected:
        PreGroupQuery query_;
    };

    // Specialized for column-based grouping
    template <IsQuery PreGroupQuery, IsColumn... GroupColumns>
    class GroupByColumnExpr : public GroupByExprBase<GroupByColumnExpr<PreGroupQuery, GroupColumns...>, PreGroupQuery> {
        using Base = GroupByExprBase<GroupByColumnExpr, PreGroupQuery>;

    public:
        template <typename PreGroupQueryTp, typename... GroupColumnsTp>
            requires std::constructible_from<PreGroupQuery, PreGroupQueryTp> &&
                         (std::constructible_from<GroupColumns, GroupColumnsTp> && ...)
        constexpr explicit GroupByColumnExpr(PreGroupQueryTp&& q, GroupColumnsTp&&... cols) noexcept
            : Base{std::forward<PreGroupQueryTp>(q)},
              columns_{std::forward<GroupColumnsTp>(cols)...} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& columns(this Self&& self) noexcept {
            return std::forward<Self>(self).columns_;
        }

    private:
        std::tuple<GroupColumns...> columns_;
    };

    // Specialized for query-based grouping
    template <IsQuery PreGroupQuery, IsQuery GroupingCriteria>
    class GroupByQueryExpr : public GroupByExprBase<GroupByQueryExpr<PreGroupQuery, GroupingCriteria>, PreGroupQuery> {
        using Base = GroupByExprBase<GroupByQueryExpr, PreGroupQuery>;

    public:
        template <typename GroupByExprBaseTp, typename GroupingCriteriaTp>
            requires std::constructible_from<PreGroupQuery, GroupByExprBaseTp> &&
                         std::constructible_from<GroupingCriteria, GroupingCriteriaTp>
        constexpr GroupByQueryExpr(GroupByExprBaseTp&& q, GroupingCriteriaTp&& criteria) noexcept
            : Base{std::forward<GroupByExprBaseTp>(q)},
              grouping_criteria_{std::forward<GroupingCriteriaTp>(criteria)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& criteria(this Self&& self) noexcept {
            return std::forward<Self>(self).grouping_criteria_;
        }

    private:
        GroupingCriteria grouping_criteria_;
    };
}  // namespace demiplane::db
