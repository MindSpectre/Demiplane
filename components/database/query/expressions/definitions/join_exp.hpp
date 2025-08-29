#pragma once

#include <algorithm>
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
        constexpr JoinExpr(
            Query q, TableSchemaPtr jt, Condition c, JoinType t, std::optional<std::string> alias = std::nullopt)
            : AliasableExpression<JoinExpr>{std::move(alias)},
              query_(std::move(q)),
              joined_table_(std::move(jt)),
              on_condition_(std::move(c)),
              type_(t) {
        }

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        [[nodiscard]] const TableSchemaPtr& joined_table() const {
            return joined_table_;
        }

        template <typename Self>
        [[nodiscard]] auto&& on_condition(this Self&& self) {
            return std::forward<Self>(self).on_condition_;
        }

        [[nodiscard]] JoinType type() const {
            return type_;
        }

        private:
        Query query_;
        TableSchemaPtr joined_table_;
        Condition on_condition_;
        JoinType type_;
    };
}  // namespace demiplane::db
