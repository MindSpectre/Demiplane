#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    // Common base class for GroupBy expressions
    template <typename Derived, typename PreGroupQuery>
    class GroupByExprBase : public Expression<Derived>,
                            public QueryOperations<Derived, AllowHaving, AllowOrderBy, AllowLimit> {
    public:
        constexpr explicit GroupByExprBase(PreGroupQuery q)
            : query_(std::move(q)) {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) {
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
        constexpr explicit GroupByColumnExpr(PreGroupQuery q, GroupColumns... cols)
            : Base(std::move(q)),
              columns_(cols...) {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& columns(this Self&& self) {
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
        constexpr GroupByQueryExpr(PreGroupQuery q, GroupingCriteria&& criteria)
            : Base(std::move(q)),
              grouping_criteria_(std::forward<GroupingCriteria>(criteria)) {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& criteria(this Self&& self) {
            return std::forward<Self>(self).grouping_criteria_;
        }

    private:
        GroupingCriteria grouping_criteria_;
    };
}  // namespace demiplane::db
