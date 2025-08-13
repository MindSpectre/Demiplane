#pragma once

#include <algorithm>
#include <utility>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query, IsCondition Condition>
    class JoinExpr : public Expression<JoinExpr<Query, Condition>>,
                     public QueryOperations<JoinExpr<Query, Condition>,
                                            AllowJoin, AllowOrderBy, AllowLimit,
                                            AllowWhere, AllowGroupBy> {
    public:
        constexpr JoinExpr(Query q,
                           std::string jt,
                           Condition c,
                           JoinType t,
                           std::optional<std::string> alias = std::nullopt)
            : query_(std::move(q)),
              joined_table_name_(std::move(jt)),
              on_condition_(std::move(c)),
              type_(t),
              joined_alias_(std::move(alias)) {}

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        template <typename Self>
        [[nodiscard]] auto&& joined_table(this Self&& self) {
            return std::forward<Self>(self).joined_table_name_;
        }

        template <typename Self>
        [[nodiscard]] auto&& on_condition(this Self&& self) {
            return std::forward<Self>(self).on_condition_;
        }

        [[nodiscard]] JoinType type() const {
            return type_;
        }

        template <typename Self>
        [[nodiscard]] auto&& joined_alias(this Self&& self) {
            return std::forward<Self>(self).joined_alias_;
        }

    private:
        Query query_;
        std::string joined_table_name_;
        Condition on_condition_;
        JoinType type_;
        std::optional<std::string> joined_alias_;
    };
}
