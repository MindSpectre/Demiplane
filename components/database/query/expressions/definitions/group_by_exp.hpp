#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    //TODO: maybe unite
    template <IsQuery PreGroupQuery, IsColumn... GroupColumns>
    class GroupByColumnExpr : public Expression<GroupByColumnExpr<PreGroupQuery, GroupColumns...>> {
    public:
        constexpr explicit GroupByColumnExpr(PreGroupQuery q, GroupColumns... cols)
            : query_(std::move(q)),
              columns_(cols...) {}

        template <IsCondition Condition>
        constexpr auto having(Condition cond) const {
            return HavingExpr<GroupByColumnExpr, Condition>{*this, std::move(cond)};
        }

        template <IsOrderBy... Orders>
        constexpr auto order_by(Orders... orders) const {
            return OrderByExpr<GroupByColumnExpr, Orders...>{*this, orders...};
        }

        constexpr auto limit(std::size_t count) const {
            return LimitExpr<GroupByColumnExpr>{*this, count, 0};
        }

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        template <typename Self>
        [[nodiscard]] auto&& columns(this Self&& self) {
            return std::forward<Self>(self).columns_;
        }

    private:
        PreGroupQuery query_;
        std::tuple<GroupColumns...> columns_;
    };

    template <IsQuery PreGroupQuery, IsQuery GroupingCriteria>
    class GroupByQueryExpr : public Expression<GroupByQueryExpr<PreGroupQuery, GroupingCriteria>> {
    public:
        constexpr GroupByQueryExpr(PreGroupQuery q, GroupingCriteria&& query)
            : query_(std::move(q)),
              grouping_criteria_(std::forward<GroupingCriteria>(query)) {}

        template <IsCondition Condition>
        constexpr auto having(Condition cond) const {
            return HavingExpr<GroupByQueryExpr, Condition>{*this, std::move(cond)};
        }

        template <IsOrderBy... Orders>
        constexpr auto order_by(Orders... orders) const {
            return OrderByExpr<GroupByQueryExpr, Orders...>{*this, orders...};
        }

        constexpr auto limit(std::size_t count) const {
            return LimitExpr<GroupByQueryExpr>{*this, count, 0};
        }

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        template <typename Self>
        [[nodiscard]] auto&& criteria(this Self&& self) {
            return std::forward<Self>(self).grouping_criteria_;
        }

    private:
        PreGroupQuery query_;
        GroupingCriteria grouping_criteria_;
    };
}
