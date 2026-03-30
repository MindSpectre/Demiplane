#pragma once

#include <utility>

#include "../basic.hpp"

namespace demiplane::db {

    template <IsQuery Query, IsCondition Condition, IsTable TableT>
    class JoinExpr : public AliasableExpression<JoinExpr<Query, Condition, TableT>>,
                     public QueryOperations<JoinExpr<Query, Condition, TableT>,
                                            AllowJoin,
                                            AllowOrderBy,
                                            AllowLimit,
                                            AllowWhere,
                                            AllowGroupBy>,
                     public TableHolder<TableT> {
    public:
        template <typename QueryTp, typename TableTp, typename ConditionTp, typename AliasTp = std::string>
            requires std::constructible_from<Query, QueryTp> && std::constructible_from<TableT, TableTp> &&
                         std::constructible_from<Condition, ConditionTp> &&
                         std::convertible_to<AliasTp, std::string_view>
        constexpr JoinExpr(QueryTp&& parent_query,
                           TableTp&& joined_table,
                           ConditionTp&& condition,
                           const JoinType join_type,
                           AliasTp&& alias) noexcept
            : AliasableExpression<JoinExpr>{std::forward<AliasTp>(alias)},
              TableHolder<TableT>{std::forward<TableTp>(joined_table)},
              query_{std::forward<QueryTp>(parent_query)},
              on_condition_{std::forward<ConditionTp>(condition)},
              type_{join_type} {
        }

        template <typename QueryTp, typename TableTp, typename ConditionTp>
            requires std::constructible_from<Query, QueryTp> && std::constructible_from<TableT, TableTp> &&
                         std::constructible_from<Condition, ConditionTp>
        constexpr JoinExpr(QueryTp&& parent_query,
                           TableTp&& joined_table,
                           ConditionTp&& condition,
                           const JoinType join_type) noexcept
            : TableHolder<TableT>{std::forward<TableTp>(joined_table)},
              query_{std::forward<QueryTp>(parent_query)},
              on_condition_{std::forward<ConditionTp>(condition)},
              type_{join_type} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward_like<Self>(self.query_);
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& on_condition(this Self&& self) noexcept {
            return std::forward_like<Self>(self.on_condition_);
        }

        [[nodiscard]] constexpr JoinType type() const noexcept {
            return type_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto decompose(this Self&& self) noexcept {
            return std::forward_as_tuple(std::forward_like<Self>(self.query_),
                                         std::forward_like<Self>(self.on_condition_),
                                         std::forward_like<Self>(self.table),
                                         std::forward_like<Self>(self.type_),
                                         std::forward_like<Self>(self.alias));
        }

    private:
        Query query_;
        Condition on_condition_;
        JoinType type_;
    };
}  // namespace demiplane::db
