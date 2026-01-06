#pragma once

#include <utility>

#include "../basic.hpp"

namespace demiplane::db {

    template <IsQuery Query, IsCondition Condition>
    class JoinExpr : public AliasableExpression<JoinExpr<Query, Condition>>,
                     public QueryOperations<JoinExpr<Query, Condition>,
                                            AllowJoin,
                                            AllowOrderBy,
                                            AllowLimit,
                                            AllowWhere,
                                            AllowGroupBy> {
    public:
        template <typename QueryTp, typename TableTp, typename ConditionTp, typename AliasTp = std::string>
            requires std::constructible_from<Query, QueryTp> && std::constructible_from<TablePtr, TableTp> &&
                         std::constructible_from<Condition, ConditionTp> && gears::IsStringLike<AliasTp>
        constexpr JoinExpr(QueryTp&& parent_query,
                           TableTp&& joined_table,
                           ConditionTp&& condition,
                           const JoinType join_type,
                           AliasTp&& alias) noexcept
            : AliasableExpression<JoinExpr>{std::forward<AliasTp>(alias)},
              query_{std::forward<QueryTp>(parent_query)},
              joined_table_{std::forward<TableTp>(joined_table)},
              on_condition_{std::forward<ConditionTp>(condition)},
              type_{join_type} {
        }

        template <typename QueryTp, typename TableTp, typename ConditionTp, typename AliasTp = std::string>
            requires std::constructible_from<Query, QueryTp> && std::constructible_from<TablePtr, TableTp> &&
                         std::constructible_from<Condition, ConditionTp> && gears::IsStringLike<AliasTp>
        constexpr JoinExpr(QueryTp&& parent_query,
                           TableTp&& joined_table,
                           ConditionTp&& condition,
                           const JoinType join_type) noexcept
            : query_{std::forward<QueryTp>(parent_query)},
              joined_table_{std::forward<TableTp>(joined_table)},
              on_condition_{std::forward<ConditionTp>(condition)},
              type_{join_type} {
        }
        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward<Self>(self).query_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& joined_table(this Self&& self) noexcept {
            return std::forward<Self>(self).joined_table_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& on_condition(this Self&& self) noexcept {
            return std::forward<Self>(self).on_condition_;
        }

        [[nodiscard]] constexpr JoinType type() const noexcept {
            return type_;
        }

    private:
        Query query_;
        TablePtr joined_table_;
        Condition on_condition_;
        JoinType type_;
    };
}  // namespace demiplane::db
